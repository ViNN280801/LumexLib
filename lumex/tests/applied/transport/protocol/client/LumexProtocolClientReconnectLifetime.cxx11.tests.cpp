// The generated client over a session, the part of the lifetime: on_reconnect
// (never inline, a client or a handler destroyed inside it, a handler that
// owns its client), cancel, close, destruction during a reconnect, the front
// of the session gone, many clients on one runner and threads. On a virtual
// clock, except the threads; the device table is invented.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <system_error>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/protocol/client/LumexClientReconnectBench.hpp"

namespace
{
using namespace lumex_client_test;

TEST (LumexProtocolClientReconnect, OnReconnect_NeverInlineAndRunsOnTheStrand)
{
  identified_bench_t bench;
  bool on_strand = false;
  int calls = 0;
  bench.client->on_reconnect (
      [&] (std::error_code const &, pr::identity_t const &)
        {
          ++calls;
          on_strand = bench.client->get_strand ().running_in_this_thread ();
        });
  EXPECT_EQ (calls, 0);
  bench.lose ();
  ASSERT_TRUE (bench.run_until ([&] () { return calls != 0; }));
  EXPECT_EQ (calls, 1);
  EXPECT_TRUE (on_strand);
}

TEST (LumexProtocolClientReconnect, OnReconnect_MayDestroyTheClient)
{
  identified_bench_t bench;
  int calls = 0;
  bench.client->on_reconnect (
      [&] (std::error_code const &, pr::identity_t const &)
        {
          ++calls;
          bench.client.reset ();
        });
  bench.lose ();
  ASSERT_TRUE (bench.run_until ([&] () { return calls != 0; }));
  EXPECT_EQ (bench.client, nullptr);
  bench.settle ();
  // The session went with it.
  EXPECT_EQ (bench.session->state (), ss::channel_state::closed);
}

// Owns the client, so a handler holding it is a cycle until the end.
struct owner_t
{
  std::unique_ptr<pr::client<gauge>> client;
};

TEST (LumexProtocolClientReconnect,
      OnReconnect_AHandlerThatOwnsItsClientIsNoLeak)
{
  // LeakSanitizer: the shutdown drops the handler.
  stream_bench_t<gauge_a, gauge> bench;
  auto owner = std::make_shared<owner_t> ();
  owner->client = std::move (bench.client);
  std::weak_ptr<owner_t> const watch = owner;
  owner->client->on_reconnect (
      [owner] (std::error_code const &, pr::identity_t const &) {});
  owner.reset ();
  EXPECT_FALSE (watch.expired ());
  {
    std::shared_ptr<owner_t> const held = watch.lock ();
    held->client->close ();
  }
  bench.settle ();
  EXPECT_TRUE (watch.expired ());
}

// A handler whose destructor calls the client back: under a lock it would wait
// for itself.
struct calls_back_t
{
  pr::client<gauge> *client;
  bool armed;

  ~calls_back_t ()
  {
    if (armed)
      client->on_reconnect (nullptr);
  }

  calls_back_t (pr::client<gauge> *made, bool arm) : client (made), armed (arm)
  {
  }

  calls_back_t (calls_back_t const &other)
      : client (other.client), armed (other.armed)
  {
  }

  void
  operator() (std::error_code const &, pr::identity_t const &)
  {
  }
};

TEST (LumexProtocolClientReconnect, OnReconnect_AReplacedHandlerMayCallBack)
{
  // Falsified by a replacement under the lock: a deadlock, in this test a
  // hang.
  stream_bench_t<gauge_a, gauge> bench;
  bench.client->on_reconnect (calls_back_t (bench.client.get (), false));
  std::function<void (std::error_code const &, pr::identity_t const &)>
      replaced = calls_back_t (bench.client.get (), true);
  bench.client->on_reconnect (std::move (replaced));
  replaced = nullptr;
  bench.client->on_reconnect (nullptr);
  SUCCEED ();
}

TEST (LumexProtocolClientReconnect,
      Reconnect_CancelDuringTheCheckStartsItAgain)
{
  // Falsified by a check that ends with the cancel: the client stays
  // unchecked.
  identified_bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  bench.lose ();
  bench.client->cancel ();
  bench.settle ();
  EXPECT_EQ (bench.seen->calls, 0);
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  // A check that failed is reported too: the end that comes has no error.
  ASSERT_TRUE (bench.run_until (
      [&] () { return bench.seen->calls != 0 && !bench.seen->error; }));
  EXPECT_EQ (bench.session->state (), ss::channel_state::open);
}

TEST (LumexProtocolClientReconnect,
      Reconnect_CancelDuringTheOutageKeepsConnecting)
{
  // Falsified by a view whose cancel is the session's: the session closes.
  identified_bench_t bench;
  auto const read = new_outcome<gauge::READ_t> ();
  bench.mock->control.fail_opens_for (
      ms (500), make_error_code (tr::error::transport_errc::disconnected));
  bench.lose ();
  EXPECT_EQ (bench.session->state (), ss::channel_state::reconnecting);
  bench.client->cancel ();
  bench.settle ();
  EXPECT_EQ (bench.session->state (), ss::channel_state::reconnecting);
  ASSERT_TRUE (bench.run_until ([&] () { return bench.seen->calls != 0; }));
  EXPECT_EQ (bench.session->state (), ss::channel_state::open);
}

TEST (LumexProtocolClientReconnect, Close_DuringAReconnectEndsItAll)
{
  // Falsified by a close that leaves the session running or a call pending.
  identified_bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const read = new_outcome<gauge::SLOW_t> ();
  bench.client->SLOW (into<gauge::SLOW_t> (read));
  bench.settle ();
  bench.mock->control.fail_opens_for (
      ms (10000), make_error_code (tr::error::transport_errc::disconnected));
  bench.lose ();
  EXPECT_EQ (read->calls, 0);
  bench.client->close ();
  bench.settle ();
  EXPECT_EQ (read->calls, 1);
  EXPECT_EQ (read->error, tr::error::transport_errc::cancelled);
  EXPECT_EQ (bench.session->state (), ss::channel_state::closed);
  auto const after = new_outcome<gauge::READ_t> ();
  bench.client->READ (into<gauge::READ_t> (after));
  bench.settle ();
  EXPECT_EQ (after->error, tr::error::transport_errc::closed);
}

TEST (LumexProtocolClientReconnect, Destruction_DuringAReconnectIsNoHang)
{
  identified_bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const read = new_outcome<gauge::SLOW_t> ();
  bench.client->SLOW (into<gauge::SLOW_t> (read));
  bench.settle ();
  bench.lose ();
  bench.client.reset ();
  bench.settle ();
  EXPECT_EQ (read->calls, 1);
  EXPECT_EQ (read->error, tr::error::transport_errc::cancelled);
}

TEST (LumexProtocolClientReconnect, Destruction_OfTheSessionFrontEndsTheWaits)
{
  identified_bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const read = new_outcome<gauge::SLOW_t> ();
  bench.client->SLOW (into<gauge::SLOW_t> (read));
  bench.settle ();
  bench.session.reset ();
  bench.settle ();
  EXPECT_EQ (read->calls, 1);
  EXPECT_EQ (read->error, protocol_errc::link_lost);
}

TEST (LumexProtocolClientReconnect, ManyClients_OnOneRunner)
{
  // Falsified by state shared between dialogues; TSan, two threads, real time.
  asio::io_context context;
  auto guard = asio::make_work_guard (context);
  std::thread first ([&] () { context.run (); });
  std::thread second ([&] () { context.run (); });
  tr::testing::virtual_clock device_clock;
  struct one_t
  {
    std::unique_ptr<tr::testing::mock_stream> mock;
    std::unique_ptr<ss::stream_session> session;
    std::unique_ptr<tr::testing::device_simulator<gauge_a>> device;
    std::unique_ptr<pr::client<gauge>> client;
    std::atomic<int> reconnects{ 0 };
  };
  std::vector<std::unique_ptr<one_t>> all;
  for (int i = 0; i < 4; ++i)
    {
      std::unique_ptr<one_t> one (new one_t ());
      one->mock.reset (new tr::testing::mock_stream (
          tr::testing::make_mock_stream (context.get_executor ())));
      std::shared_ptr<tr::stream::stream_impl> const impl
          = one->mock->stream.impl ();
      ss::session_options_t options;
      options.policy.initial_delay = ms (1);
      options.policy.jitter = 0;
      one->session.reset (new ss::stream_session (
          context.get_executor (), [impl] (tr::executor_type const &)
            { return tr::stream::byte_stream (impl); }, options));
      one->device.reset (new tr::testing::device_simulator<gauge_a> (
          one->mock->control, fr::slip::slip_codec (), device_clock));
      one->device->set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1));
      one->device->set_reply<gauge_a::READ_t> (std::uint16_t (i + 1));
      EXPECT_TRUE (one->session->open (tr::deadline_t (5000)).has_value ());
      one->client.reset (new pr::client<gauge> (
          pr::frame_link (tr::framed::framed_stream<fr::slip::slip_codec> (
              one->session->as_byte_stream (), fr::slip::slip_codec ()))));
      EXPECT_TRUE (one->client->identify (tr::deadline_t (5000)).has_value ());
      one_t *const raw = one.get ();
      one->client->on_reconnect (
          [raw] (std::error_code const &error, pr::identity_t const &)
            {
              if (!error)
                ++raw->reconnects;
            });
      all.push_back (std::move (one));
    }
  for (int round = 1; round <= 5; ++round)
    {
      // Let each link count as established.
      std::this_thread::sleep_for (ms (5));
      for (std::size_t i = 0; i < all.size (); ++i)
        all[i]->mock->control.disconnect ();
      auto const end
          = std::chrono::steady_clock::now () + std::chrono::seconds (20);
      for (std::size_t i = 0; i < all.size (); ++i)
        while (all[i]->reconnects.load () < round
               && std::chrono::steady_clock::now () < end)
          std::this_thread::sleep_for (ms (1));
      for (std::size_t i = 0; i < all.size (); ++i)
        {
          EXPECT_EQ (all[i]->reconnects.load (), round);
          // Each still talks to its own device, in the blocking form too.
          pr::call_result_t<gauge::READ_t> const read
              = all[i]->client->READ (tr::deadline_t (5000));
          ASSERT_TRUE (read.has_value ()) << read.error ().message ();
          EXPECT_EQ (*read, static_cast<int> (i) + 1);
        }
    }
  for (std::size_t i = 0; i < all.size (); ++i)
    {
      all[i]->client.reset ();
      all[i]->session.reset ();
      all[i]->device.reset ();
    }
  guard.reset ();
  context.stop ();
  first.join ();
  second.join ();
  all.clear ();
}

TEST (LumexProtocolClientReconnect, Threads_ABlockingCallSurvivesTheLoss)
{
  // A blocking call of an idempotent row, with the link going under it: it
  // ends with the reply of the new connection.
  asio::io_context context;
  auto guard = asio::make_work_guard (context);
  std::thread runner ([&] () { context.run (); });
  tr::testing::virtual_clock device_clock;
  std::unique_ptr<tr::testing::mock_stream> mock (
      new tr::testing::mock_stream (
          tr::testing::make_mock_stream (context.get_executor ())));
  std::shared_ptr<tr::stream::stream_impl> const impl = mock->stream.impl ();
  ss::session_options_t options;
  options.policy.initial_delay = ms (1);
  options.policy.jitter = 0;
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      context.get_executor (), [impl] (tr::executor_type const &)
        { return tr::stream::byte_stream (impl); }, options));
  std::unique_ptr<tr::testing::device_simulator<gauge_a>> device (
      new tr::testing::device_simulator<gauge_a> (
          mock->control, fr::slip::slip_codec (), device_clock));
  device->set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1));
  device->set_reply<gauge_a::SLOW_t> (std::uint8_t (6));
  ASSERT_TRUE (session->open (tr::deadline_t (5000)).has_value ());
  std::unique_ptr<pr::client<gauge>> client (new pr::client<gauge> (
      pr::frame_link (tr::framed::framed_stream<fr::slip::slip_codec> (
          session->as_byte_stream (), fr::slip::slip_codec ()))));
  ASSERT_TRUE (client->identify (tr::deadline_t (5000)).has_value ());
  std::this_thread::sleep_for (ms (5));
  device->set_behavior (tr::testing::behavior::silent ());
  std::future<pr::call_result_t<gauge::SLOW_t>> call
      = std::async (std::launch::async, [&client] ()
                      { return client->SLOW (tr::deadline_t (9000)); });
  auto const end
      = std::chrono::steady_clock::now () + std::chrono::seconds (5);
  while (device->statistics ().requests < 2u
         && std::chrono::steady_clock::now () < end)
    std::this_thread::sleep_for (ms (1));
  device->set_behavior (tr::testing::behavior::perfect ());
  mock->control.disconnect ();
  pr::call_result_t<gauge::SLOW_t> const result = call.get ();
  ASSERT_TRUE (result.has_value ()) << result.error ().message ();
  EXPECT_EQ (*result, 6);
  client.reset ();
  session.reset ();
  device.reset ();
  guard.reset ();
  context.stop ();
  runner.join ();
  mock.reset ();
}

TEST (LumexProtocolClientReconnect, Reconnect_SetSessionDuringTheOutageStays)
{
  // Falsified by a check that puts the session of before back, and by a
  // session that goes when set_session takes the layer of nothing.
  identified_bench_t bench;
  bench.mock->control.fail_opens_for (
      ms (500), make_error_code (tr::error::transport_errc::disconnected));
  bench.lose ();
  index::session_context_t made;
  made.set ("mine", 5);
  bench.client->set_session (made);
  bench.settle ();
  ASSERT_TRUE (bench.run_until ([&] () { return bench.seen->calls != 0; }));
  EXPECT_FALSE (bench.seen->error) << bench.seen->error.message ();
  index::session_context_t const session = bench.client->session ();
  EXPECT_EQ (session.layer (), "gauge_a_t");
  EXPECT_EQ (session.model (), "G100");
  EXPECT_EQ (bench.client->engine ().session ().model (), "G100");
  std::int64_t value = 0;
  EXPECT_TRUE (session.find ("mine", value));
  EXPECT_EQ (value, 5);
}

TEST (LumexProtocolClientReconnect,
      Reconnect_TheFrontOfTheSessionDestroyedBeforeTheClientUsedItAnswers)
{
  // Falsified by a client that hangs or crashes on a session that is gone.
  stream_bench_t<gauge_a, gauge> bench;
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1));
  ASSERT_FALSE (bench.identify ()->error);
  bench.session.reset ();
  bench.settle ();
  for (int i = 0; i < 3; ++i)
    {
      auto const read = new_outcome<gauge::READ_t> ();
      bench.client->READ (into<gauge::READ_t> (read));
      ASSERT_TRUE (bench.run_until ([&] () { return read->calls != 0; }));
      EXPECT_TRUE (read->error);
    }
  auto const again = bench.identify ();
  EXPECT_EQ (again->calls, 1);
  EXPECT_TRUE (again->error);
}

TEST (LumexProtocolClientReconnect,
      Lifetime_TheContextGoesFirstMidRestorationWithAnOwningHandler)
{
  // Falsified by a handler that keeps its client alive after the context is
  // gone: the client would never be freed.
  std::weak_ptr<pr::client<gauge>> watch;
  {
    asio::io_context context;
    tr::testing::virtual_clock clock;
    tr::testing::mock_options_t mock_options;
    mock_options.timers = clock.source ();
    tr::testing::mock_stream mock = tr::testing::make_mock_stream (
        context.get_executor (), mock_options);
    std::shared_ptr<tr::stream::stream_impl> const impl = mock.stream.impl ();
    ss::stream_session session (
        context.get_executor (), [impl] (tr::executor_type const &)
          { return tr::stream::byte_stream (impl); },
        bench_session_options (clock));
    tr::testing::device_simulator<gauge_a> device (
        mock.control, fr::slip::slip_codec (), clock);
    device.set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1));
    session.start ();
    auto const settle = [&] ()
      {
        tr::testing::run_until (
            context, clock, [] () { return false; }, ms (0));
      };
    auto const advance = [&] (ms by)
      {
        tr::testing::run_until (context, clock, [] () { return false; }, by);
      };
    settle ();
    pr::client_options_t options = pr::client_options_for<gauge> ();
    options.dialogue.timers = clock.source ();
    auto client = std::make_shared<pr::client<gauge>> (
        pr::frame_link (tr::framed::framed_stream<fr::slip::slip_codec> (
            session.as_byte_stream (), fr::slip::slip_codec ())),
        options);
    client->on_reconnect (
        [client] (std::error_code const &, pr::identity_t const &) {});
    settle ();
    advance (ms (200));
    int identified = 0;
    client->async_identify (
        [&identified, client] (std::error_code, pr::identity_t)
          { ++identified; });
    tr::testing::run_until (
        context, clock, [&] () { return identified != 0; }, ms (5000));
    device.set_behavior (tr::testing::behavior::silent ());
    client->SLOW ([client] (std::error_code, pr::reply_of<gauge::SLOW_t>) {});
    settle ();
    mock.control.disconnect ();
    advance (ms (150));
    watch = client;
    client.reset ();
  }
  EXPECT_TRUE (watch.expired ());
}
} // namespace
