// The lifetime of the generated client: destroyed inside its own handler, with
// calls in flight, during the end of the io_context, a handler that owns the
// client (a capture cycle: run under LeakSanitizer), and a handler whose
// destructor calls the client back. The synthetic mixer table.

#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/protocol/client/LumexClientTestSupport.hpp"

namespace
{
using namespace lumex_client_test;
using client_type = pr::client<mixer>;
using client_ptr = std::shared_ptr<client_type>;

/// A client owned by a shared pointer, over a mock on a real context with no
/// device: nothing answers unless the test feeds it.
struct owned_bench_t
{
  owned_bench_t () : context (new asio::io_context ())
  {
    tr::testing::mock_options_t mock_options;
    mock_options.timers = clock.source ();
    tr::testing::mock_stream mock = tr::testing::make_mock_stream (
        context->get_executor (), mock_options);
    control.reset (new tr::testing::mock_stream_control (mock.control));
    mock.stream.async_open ([] (std::error_code) {});
    context->poll ();
    context->restart ();
    pr::client_options_t options = pr::client_options_for<mixer> ();
    options.dialogue.timers = clock.source ();
    owner = std::make_shared<client_type> (
        pr::frame_link (tr::framed::framed_stream<fr::slip::slip_codec> (
            std::move (mock.stream), fr::slip::slip_codec ())),
        options);
    settle ();
  }

  void
  settle ()
  {
    tr::testing::run_until (*context, clock, [] () { return false; }, ms (0));
  }

  void
  advance (ms by)
  {
    tr::testing::run_until (*context, clock, [] () { return false; }, by);
  }

  void
  end_context ()
  {
    control.reset ();
    context.reset ();
  }

  tr::testing::virtual_clock clock;
  std::unique_ptr<asio::io_context> context;
  std::unique_ptr<tr::testing::mock_stream_control> control;
  client_ptr owner;
};

/// Runs `on_end` when the last copy is destroyed.
struct on_destroy_t
{
  explicit on_destroy_t (std::function<void ()> on_end)
      : run (std::make_shared<runner_t> ())
  {
    run->on_end = std::move (on_end);
  }

  struct runner_t
  {
    std::function<void ()> on_end;
    ~runner_t ()
    {
      if (on_end)
        on_end ();
    }
  };

  std::shared_ptr<runner_t> run;
};
} // namespace

TEST (LumexProtocolClientLifetime, Client_DestroyedInsideItsOwnHandler)
{
  owned_bench_t bench;
  std::weak_ptr<client_type> const watch = bench.owner;
  int calls = 0;
  std::error_code ended;
  bench.owner->RATE (300,
                     [&] (std::error_code error, std::uint8_t)
                       {
                         ++calls;
                         ended = error;
                         bench.owner.reset ();
                       });
  bench.settle ();
  EXPECT_EQ (calls, 1);
  EXPECT_EQ (ended, protocol_errc::invalid_value);
  EXPECT_TRUE (watch.expired ());
  bench.settle ();
}

TEST (LumexProtocolClientLifetime, Client_DestroyedWithCallsInFlight)
{
  owned_bench_t bench;
  auto const first = new_outcome<mixer::RATE_t> ();
  auto const queued = new_outcome<mixer::PING_t> ();
  bench.owner->RATE (std::uint8_t (1), into<mixer::RATE_t> (first));
  bench.owner->PING (into<mixer::PING_t> (queued));
  bench.settle ();
  EXPECT_EQ (first->calls, 0);
  bench.owner.reset ();
  bench.settle ();
  EXPECT_EQ (first->calls, 1);
  EXPECT_EQ (first->error, transport_errc::cancelled);
  EXPECT_EQ (queued->calls, 1);
  EXPECT_EQ (queued->error, transport_errc::cancelled);
  bench.advance (ms (2000));
  EXPECT_EQ (first->calls, 1);
  EXPECT_EQ (queued->calls, 1);
}

TEST (LumexProtocolClientLifetime,
      Client_DestroyedInsideAHandlerWithOthersInFlight)
{
  owned_bench_t bench;
  auto const other = new_outcome<mixer::PING_t> ();
  int calls = 0;
  bench.owner->RATE (300,
                     [&] (std::error_code, std::uint8_t)
                       {
                         ++calls;
                         bench.owner.reset ();
                       });
  bench.owner->PING (into<mixer::PING_t> (other));
  bench.settle ();
  EXPECT_EQ (calls, 1);
  EXPECT_EQ (other->calls, 1);
  EXPECT_EQ (other->error, transport_errc::cancelled);
}

TEST (LumexProtocolClientLifetime, Client_DestroyedDuringIdentify)
{
  owned_bench_t bench;
  auto const out = std::make_shared<outcome_t<pr::identity_t>> ();
  record_t<pr::identity_t> done = { out };
  bench.owner->async_identify (done);
  bench.settle ();
  bench.owner.reset ();
  bench.settle ();
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, transport_errc::cancelled);
}

TEST (LumexProtocolClientLifetime, Client_ALayerOutlivesTheClient)
{
  owned_bench_t bench;
  pr::client_layer<mixer, mixer_v1> layer = bench.owner->as<mixer_v1> ();
  bench.owner.reset ();
  bench.settle ();
  EXPECT_EQ (layer
                 .on<mixer_v1::ONLY_V1_t> (
                     [] (pr::reply_of<mixer_v1::ONLY_V1_t> const &) {})
                 .id,
             0u);
  // No strand left: the system executor runs the handler.
  std::promise<std::error_code> ended;
  std::future<std::error_code> result = ended.get_future ();
  layer.ONLY_V1 ([&ended] (std::error_code error, std::uint8_t)
                   { ended.set_value (error); });
  ASSERT_EQ (result.wait_for (std::chrono::seconds (20)),
             std::future_status::ready);
  EXPECT_EQ (result.get (), transport_errc::closed);
}

TEST (LumexProtocolClientLifetime,
      Client_AHandlerThatOwnsTheClientIsNoCycleAfterClose)
{
  owned_bench_t bench;
  std::weak_ptr<client_type> const watch = bench.owner;
  client_ptr const held = bench.owner;
  bench.owner->on<mixer::LEVEL_t> (
      [held] (pr::reply_of<mixer::LEVEL_t> const &) {});
  bench.owner->on_unknown ([held] (pr::reply_frame_t const &) {});
  bench.owner->close ();
  bench.owner.reset ();
  EXPECT_FALSE (watch.expired ()) << "held by the copy of this test";
  // Nothing of the client in the dialogue keeps it: only `held` is left.
  bench.settle ();
  EXPECT_EQ (watch.use_count (), 1);
}

TEST (LumexProtocolClientLifetime,
      Client_AHandlerThatOwnsTheClientDiesWithTheContext)
{
  std::weak_ptr<client_type> watch;
  {
    owned_bench_t bench;
    watch = bench.owner;
    client_ptr const held = bench.owner;
    bench.owner->on<mixer::LEVEL_t> (
        [held] (pr::reply_of<mixer::LEVEL_t> const &) {});
    client_ptr const by_call = bench.owner;
    bench.owner->RATE (std::uint8_t (1),
                       [by_call] (std::error_code, std::uint8_t) {});
    bench.settle ();
    bench.owner.reset ();
    EXPECT_FALSE (watch.expired ());
    bench.control.reset ();
  }
  // The end of the context broke both cycles; LeakSanitizer checks the rest.
  EXPECT_TRUE (watch.expired ());
}

TEST (LumexProtocolClientLifetime,
      Client_DestroyedByTheContextWhileItsCallIsPending)
{
  // The one owner is a pending handler; nothing else names the client.
  std::weak_ptr<client_type> watch;
  auto calls = std::make_shared<int> (0);
  {
    owned_bench_t bench;
    watch = bench.owner;
    client_ptr const by_call = bench.owner;
    bench.owner->async_identify (
        [by_call, calls] (std::error_code, pr::identity_t) { ++*calls; });
    bench.settle ();
    bench.owner.reset ();
    bench.control.reset ();
  }
  EXPECT_TRUE (watch.expired ());
  EXPECT_EQ (*calls, 0);
}

TEST (LumexProtocolClientLifetime,
      Client_ADestructorInAHandlerMayCallTheClientBack)
{
  owned_bench_t bench;
  int destroyed = 0;
  client_type *const client = bench.owner.get ();
  // Every place that drops a handler while holding what the engine locks.
  on_destroy_t const unsubscribes (
      [&destroyed, client] ()
        {
          ++destroyed;
          client->unsubscribe (pr::subscription_t ());
        });
  pr::subscription_t const made = bench.owner->on<mixer::LEVEL_t> (
      [unsubscribes] (pr::reply_of<mixer::LEVEL_t> const &) {});
  on_destroy_t const subscribes (
      [&destroyed, client] ()
        {
          ++destroyed;
          client->on<mixer::LEVEL_t> (
              [] (pr::reply_of<mixer::LEVEL_t> const &) {});
        });
  bench.owner->on_unknown ([subscribes] (pr::reply_frame_t const &) {});
  on_destroy_t const unknowns (
      [&destroyed, client] ()
        {
          ++destroyed;
          client->on_unknown (nullptr);
        });
  std::shared_ptr<on_destroy_t> const pending
      = std::make_shared<on_destroy_t> (unknowns);
  (void)pending;
  EXPECT_TRUE (bench.owner->unsubscribe (made));
  bench.owner->on_unknown (nullptr);
  bench.settle ();
  bench.owner->close ();
  bench.settle ();
}

TEST (LumexProtocolClientLifetime,
      Client_TheLastOwnerDroppedByUnsubscribeIsSafe)
{
  owned_bench_t bench;
  std::weak_ptr<client_type> const watch = bench.owner;
  client_ptr const held = bench.owner;
  client_type *const raw = bench.owner.get ();
  pr::subscription_t const made = raw->on<mixer::LEVEL_t> (
      [held] (pr::reply_of<mixer::LEVEL_t> const &) {});
  // Not the last one yet: `held` lives in this scope. The handler itself goes.
  EXPECT_TRUE (raw->unsubscribe (made));
  EXPECT_EQ (watch.use_count (), 2);
  bench.settle ();
}

TEST (LumexProtocolClientLifetime, Client_ACloseInsideAHandler)
{
  owned_bench_t bench;
  auto const second = new_outcome<mixer::PING_t> ();
  bench.owner->RATE (300,
                     [&] (std::error_code, std::uint8_t)
                       {
                         bench.owner->close ();
                         bench.owner->PING (into<mixer::PING_t> (second));
                       });
  bench.settle ();
  EXPECT_EQ (second->calls, 1);
  EXPECT_EQ (second->error, transport_errc::closed);
}
