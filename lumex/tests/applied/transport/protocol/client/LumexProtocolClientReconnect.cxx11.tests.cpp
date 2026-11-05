// The generated client over a session: what a new connection repeats (the
// identification, the layer, the streams), what happens to the calls under way
// and to the ones that come meanwhile, the epoch that ends a partial frame, a
// device that comes back as another or as none, the second loss during the
// restoration, the watchdog, the wait bound and on_reconnect. On a virtual
// clock; the device table is invented.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/protocol/client/LumexClientReconnectSupport.hpp"

namespace
{
using namespace lumex_client_test;

/// The handler of a reconnect: counts and keeps the last call.
struct reconnects_seen_t
{
  int calls = 0;
  std::error_code error;
  pr::identity_t identity;
};

std::shared_ptr<reconnects_seen_t>
watch_reconnects (pr::client<gauge> &client)
{
  auto const seen = std::make_shared<reconnects_seen_t> ();
  client.on_reconnect (
      [seen] (std::error_code const &error, pr::identity_t const &identity)
        {
          ++seen->calls;
          seen->error = error;
          seen->identity = identity;
        });
  return seen;
}

/// A bench of a device of layer a, identified.
struct identified_bench_t : stream_bench_t<gauge_a, gauge>
{
  identified_bench_t ()
  {
    device->set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1));
    auto const out = identify ();
    EXPECT_FALSE (out->error) << out->error.message ();
    seen = watch_reconnects (*client);
  }

  /// Hangs up, and runs until the device was checked again.
  void
  reconnect ()
  {
    int const before = seen->calls;
    lose ();
    EXPECT_TRUE (run_until ([&] () { return seen->calls != before; }));
    settle ();
  }

  std::shared_ptr<reconnects_seen_t> seen;
};

std::uint64_t
requests_of (tr::testing::simulator_statistics_t const &statistics,
             char const *row)
{
  for (std::size_t i = 0; i < statistics.by_row.size (); ++i)
    if (std::string (statistics.by_row[i].row) == row)
      return statistics.by_row[i].requests;
  return 0;
}

TEST (LumexProtocolClientReconnect, Reconnect_ReidentifiesAndReselectsLayer)
{
  // Falsified by a client that keeps what it knew: no WHO on the new link.
  identified_bench_t bench;
  auto const before = bench.requests ();
  bench.reconnect ();
  EXPECT_GT (bench.requests (), before);
  EXPECT_FALSE (bench.seen->error) << bench.seen->error.message ();
  EXPECT_EQ (bench.seen->identity.model, "G100");
  EXPECT_EQ (bench.seen->identity.layer, "gauge_a_t");
  EXPECT_EQ (bench.client->session ().model (), "G100");
  EXPECT_EQ (bench.client->session ().layer (), "gauge_a_t");
  EXPECT_EQ (bench.client->session ().firmware (), 3u);
  ASSERT_TRUE (bench.client->identity ().has_value ());
  EXPECT_EQ (bench.client->identity ()->model, "G100");
  // The layer works again.
  bench.device->set_reply<gauge_a::ONLY_A_t> (std::uint8_t (9));
  auto const only = new_outcome<gauge_a::ONLY_A_t> ();
  bench.client->as<gauge_a> ().ONLY_A (into<gauge_a::ONLY_A_t> (only));
  ASSERT_TRUE (bench.run_until ([&] () { return only->calls != 0; }));
  EXPECT_FALSE (only->error) << only->error.message ();
  EXPECT_EQ (only->value, 9);
}

TEST (LumexProtocolClientReconnect,
      Reconnect_BetweenLossAndCheckNothingIsAccepted)
{
  // Falsified by a client whose layer stays through the loss: as<L> goes out.
  identified_bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  bench.lose ();
  // The link is open again and the identification under way.
  EXPECT_EQ (bench.session->state (), ss::channel_state::open);
  EXPECT_EQ (bench.client->identity ().error (), protocol_errc::link_lost);
  EXPECT_TRUE (bench.client->session ().model ().empty ());
  EXPECT_TRUE (bench.client->session ().layer ().empty ());
  EXPECT_EQ (bench.client->session ().firmware (), 0u);
  std::uint64_t const requests = bench.device->statistics ().requests;
  auto const as = new_outcome<gauge_a::ONLY_A_t> ();
  bench.client->as<gauge_a> ().ONLY_A (into<gauge_a::ONLY_A_t> (as));
  auto const own = new_outcome<gauge::SET_t> ();
  bench.client->SET (std::uint8_t (1), into<gauge::SET_t> (own));
  bench.settle ();
  EXPECT_EQ (as->calls, 1);
  EXPECT_EQ (as->error, protocol_errc::link_lost);
  EXPECT_EQ (own->calls, 1);
  EXPECT_EQ (own->error, protocol_errc::link_lost);
  EXPECT_EQ (bench.device->statistics ().requests, requests);
}

TEST (LumexProtocolClientReconnect, Reconnect_DuringTheOutageCallsEndAtOnce)
{
  // Falsified by a queue that takes them: they would wait for the device.
  identified_bench_t bench;
  bench.mock->control.fail_opens_for (
      ms (500), make_error_code (tr::error::transport_errc::disconnected));
  bench.lose ();
  EXPECT_EQ (bench.session->state (), ss::channel_state::reconnecting);
  auto const read = new_outcome<gauge::READ_t> ();
  bench.client->READ (into<gauge::READ_t> (read));
  bench.settle ();
  EXPECT_EQ (read->calls, 1);
  EXPECT_EQ (read->error, protocol_errc::link_lost);
  // Nothing inline: the identification of a new call waits too.
  auto const who = bench.identify ();
  EXPECT_EQ (who->error, protocol_errc::link_lost);
  EXPECT_TRUE (bench.run_until ([&] () { return bench.seen->calls != 0; }));
  EXPECT_FALSE (bench.seen->error) << bench.seen->error.message ();
}

TEST (LumexProtocolClientReconnect, Reconnect_IdempotentResentOthersLinkLost)
{
  // Falsified by a loss that sends everything again, and by one that ends
  // everything.
  identified_bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  bench.device->set_reply<gauge_a::READ_t> (std::uint16_t (77));
  auto const read = new_outcome<gauge::READ_t> ();
  bench.client->READ (into<gauge::READ_t> (read));
  bench.settle ();
  bench.mock->control.disconnect ();
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  ASSERT_TRUE (bench.run_until ([&] () { return read->calls != 0; }));
  EXPECT_FALSE (read->error) << read->error.message ();
  EXPECT_EQ (read->value, 77);
  // The attempt lost counts too: the device saw it.
  EXPECT_EQ (requests_of (bench.device->statistics (), "READ"), 2u);

  // A row that is not idempotent: never sent again.
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const set = new_outcome<gauge::SET_t> ();
  bench.client->SET (std::uint8_t (1), into<gauge::SET_t> (set));
  bench.settle ();
  std::uint64_t const sets_before
      = requests_of (bench.device->statistics (), "SET");
  bench.mock->control.disconnect ();
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  ASSERT_TRUE (bench.run_until ([&] () { return set->calls != 0; }));
  EXPECT_EQ (set->error, protocol_errc::link_lost);
  bench.advance (ms (500));
  EXPECT_EQ (requests_of (bench.device->statistics (), "SET"), sets_before);
}

TEST (LumexProtocolClientReconnect, Reconnect_ADangerousRowIsNeverSentAgain)
{
  // Falsified by a check of idempotent alone.
  identified_bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const wipe = new_outcome<gauge::WIPE_t> ();
  bench.client->WIPE (pr::confirm, into<gauge::WIPE_t> (wipe));
  bench.settle ();
  bench.mock->control.disconnect ();
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  ASSERT_TRUE (bench.run_until ([&] () { return wipe->calls != 0; }));
  EXPECT_EQ (wipe->error, protocol_errc::link_lost);
  bench.advance (ms (500));
  EXPECT_LE (requests_of (bench.device->statistics (), "WIPE"), 1u);
}

TEST (LumexProtocolClientReconnect,
      Reconnect_AttemptsCountFromTheStartOnTheNewLink)
{
  // Falsified by a record that keeps its attempts: it would end at its first
  // timeout on the new link although its retries are unused.
  identified_bench_t bench;
  bench.device->refuse<gauge_a::READ_t> ();
  bench.device->set_reply<gauge_a::READ_t> (std::uint16_t (8));
  auto const read = new_outcome<gauge::READ_t> ();
  bench.client->READ (into<gauge::READ_t> (read));
  // Two of the three attempts time out on the old link, on the third...
  bench.advance (ms (250));
  EXPECT_EQ (read->calls, 0);
  EXPECT_EQ (bench.client->statistics ().retries, 2u);
  bench.reconnect ();
  EXPECT_FALSE (bench.seen->error) << bench.seen->error.message ();
  EXPECT_EQ (read->calls, 0);
  // ...and on the new link the first attempt times out as well.
  bench.advance (ms (150));
  EXPECT_EQ (read->calls, 0);
  bench.device->refuse<gauge_a::READ_t> (false);
  ASSERT_TRUE (bench.run_until ([&] () { return read->calls != 0; }));
  EXPECT_FALSE (read->error) << read->error.message ();
  EXPECT_EQ (read->value, 8);
}

TEST (LumexProtocolClientReconnect, Reconnect_NoGapsOfAStreamDuringTheOutage)
{
  // Falsified by a watch that goes on: a lost link would count as silence.
  identified_bench_t bench;
  auto const go = new_outcome<gauge::GO_t> ();
  bench.client->GO (into<gauge::GO_t> (go));
  ASSERT_TRUE (bench.run_until ([&] () { return go->calls != 0; }));
  bench.mock->control.fail_opens_for (
      ms (2000), make_error_code (tr::error::transport_errc::disconnected));
  bench.lose ();
  std::uint64_t const gaps = bench.client->statistics ().stream_gaps;
  bench.advance (ms (1500));
  EXPECT_EQ (bench.client->statistics ().stream_gaps, gaps);
}

TEST (LumexProtocolClientReconnect, Reconnect_AQueuedExchangeGoesAfterTheCheck)
{
  // Falsified by a loss that ends the queue, and by a queue that moves before
  // the identification ended.
  identified_bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const first = new_outcome<gauge::SLOW_t> ();
  bench.client->SLOW (into<gauge::SLOW_t> (first));
  auto const queued = new_outcome<gauge::SET_t> ();
  bench.client->SET (std::uint8_t (5), into<gauge::SET_t> (queued));
  bench.settle ();
  EXPECT_EQ (queued->calls, 0);
  std::uint64_t const sets = requests_of (bench.device->statistics (), "SET");
  EXPECT_EQ (sets, 0u);
  bench.mock->control.disconnect ();
  bench.settle ();
  // Still silent: nothing but the identification goes out.
  EXPECT_EQ (requests_of (bench.device->statistics (), "SET"), 0u);
  EXPECT_EQ (queued->calls, 0);
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  // The identification timed out: the link goes round once more, then all
  // ends.
  ASSERT_TRUE (bench.run_until (
      [&] () { return queued->calls != 0 && first->calls != 0; }));
  EXPECT_FALSE (first->error) << first->error.message ();
  EXPECT_FALSE (queued->error) << queued->error.message ();
  EXPECT_EQ (requests_of (bench.device->statistics (), "SET"), 1u);
}

TEST (LumexProtocolClientReconnect, Reconnect_ReenablesStreams)
{
  // Falsified by a restoration that forgets the row, and by one that sends
  // what the application never turned on.
  identified_bench_t bench;
  bench.reconnect ();
  EXPECT_EQ (requests_of (bench.device->statistics (), "GO"), 0u);
  auto const go = new_outcome<gauge::GO_t> ();
  bench.client->GO (into<gauge::GO_t> (go));
  ASSERT_TRUE (bench.run_until ([&] () { return go->calls != 0; }));
  EXPECT_FALSE (go->error) << go->error.message ();
  EXPECT_EQ (requests_of (bench.device->statistics (), "GO"), 1u);
  bench.reconnect ();
  EXPECT_FALSE (bench.seen->error);
  EXPECT_EQ (requests_of (bench.device->statistics (), "GO"), 2u);
  // A stream that does not come back is a gap again: it is watched.
  std::uint64_t const gaps = bench.client->statistics ().stream_gaps;
  bench.advance (ms (500));
  EXPECT_GT (bench.client->statistics ().stream_gaps, gaps);
}

TEST (LumexProtocolClientReconnect, Reconnect_ALostStreamRowIsReported)
{
  // Falsified by a restoration that stops on it: the dialogue stays closed.
  identified_bench_t bench;
  auto const go = new_outcome<gauge::GO_t> ();
  bench.client->GO (into<gauge::GO_t> (go));
  ASSERT_TRUE (bench.run_until ([&] () { return go->calls != 0; }));
  auto const go_too = new_outcome<gauge::GO_TOO_t> ();
  bench.client->GO_TOO (into<gauge::GO_TOO_t> (go_too));
  ASSERT_TRUE (bench.run_until ([&] () { return go_too->calls != 0; }));
  bench.device->refuse<gauge_a::GO_t> ();
  bench.reconnect ();
  EXPECT_EQ (bench.seen->error, protocol_errc::timeout);
  // The row after it still went.
  EXPECT_EQ (requests_of (bench.device->statistics (), "GO_TOO"), 2u);
  bench.device->set_reply<gauge_a::READ_t> (std::uint16_t (5));
  auto const read = new_outcome<gauge::READ_t> ();
  bench.client->READ (into<gauge::READ_t> (read));
  ASSERT_TRUE (bench.run_until ([&] () { return read->calls != 0; }));
  EXPECT_FALSE (read->error) << read->error.message ();
}

TEST (LumexProtocolClientReconnect, Reconnect_PartialFrameNotCarriedOver)
{
  // Falsified by a view without the epoch: half a reply glues to the next.
  identified_bench_t bench;
  bench.device->set_reply<gauge_a::READ_t> (std::uint16_t (0x1234));
  std::vector<std::uint8_t> half = wire_of ({ 0x10, 0x34 });
  half.pop_back ();
  bench.mock->control.feed (half);
  bench.settle ();
  bench.reconnect ();
  EXPECT_FALSE (bench.seen->error) << bench.seen->error.message ();
  auto const read = new_outcome<gauge::READ_t> ();
  bench.client->READ (into<gauge::READ_t> (read));
  ASSERT_TRUE (bench.run_until ([&] () { return read->calls != 0; }));
  EXPECT_FALSE (read->error) << read->error.message ();
  EXPECT_EQ (read->value, 0x1234);
  EXPECT_EQ (bench.client->statistics ().retries, 0u);
  // The half of the old connection was never a frame.
  EXPECT_EQ (bench.client->statistics ().unexpected_replies, 0u);
}

TEST (LumexProtocolClientReconnect,
      Reconnect_PartialRequestAtTheDeviceDoesNotGlue)
{
  // Falsified by a simulator that keeps a partial frame across an open.
  identified_bench_t bench;
  bench.device->set_reply<gauge_a::READ_t> (std::uint16_t (6));
  // Half a request reaches the device, written by hand on the mock.
  std::vector<std::uint8_t> half = wire_of ({ 0x10 });
  half.pop_back ();
  bool written = false;
  bench.mock->stream.async_write_all (
      tr::const_bytes_t (half.data (), half.size ()),
      [&written] (std::error_code, std::size_t) { written = true; });
  EXPECT_TRUE (bench.run_until ([&] () { return written; }));
  bench.reconnect ();
  std::uint64_t const errors = bench.device->statistics ().framing_errors;
  EXPECT_FALSE (bench.seen->error) << bench.seen->error.message ();
  auto const read = new_outcome<gauge::READ_t> ();
  bench.client->READ (into<gauge::READ_t> (read));
  ASSERT_TRUE (bench.run_until ([&] () { return read->calls != 0; }));
  EXPECT_FALSE (read->error) << read->error.message ();
  EXPECT_EQ (bench.client->statistics ().retries, 0u);
  EXPECT_EQ (bench.device->statistics ().framing_errors, errors);
  // The half of the old connection was never a request.
  EXPECT_EQ (requests_of (bench.device->statistics (), "READ"), 1u);
}

TEST (LumexProtocolClientReconnect, Reconnect_DatagramPartialAndStaleMessages)
{
  // Falsified by a view that hands on a message of the old connection.
  datagram_bench_t<gauge_a, gauge> bench;
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1));
  auto const out = std::make_shared<outcome_t<pr::identity_t>> ();
  record_t<pr::identity_t> done = { out };
  bench.client->async_identify (done);
  ASSERT_TRUE (bench.run_until ([&] () { return out->calls != 0; }));
  ASSERT_FALSE (out->error) << out->error.message ();
  auto const seen = watch_reconnects (*bench.client);
  bench.lose ();
  ASSERT_TRUE (bench.run_until ([&] () { return seen->calls != 0; }));
  EXPECT_FALSE (seen->error) << seen->error.message ();
  EXPECT_EQ (seen->identity.layer, "gauge_a_t");
  EXPECT_EQ (bench.client->statistics ().reconnects, 1u);
  bench.device->set_reply<gauge_a::READ_t> (std::uint16_t (11));
  auto const read = new_outcome<gauge::READ_t> ();
  bench.client->READ (into<gauge::READ_t> (read));
  ASSERT_TRUE (bench.run_until ([&] () { return read->calls != 0; }));
  EXPECT_FALSE (read->error) << read->error.message ();
  EXPECT_EQ (read->value, 11);
}

TEST (LumexProtocolClientReconnect, Reconnect_ReenumerationUnderVirtualClock)
{
  // The node goes away and comes back 14 s later: attempts at the times of the
  // policy, a call that survives, and a client that comes back.
  identified_bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const read = new_outcome<gauge::SLOW_t> ();
  bench.client->SLOW (into<gauge::SLOW_t> (read));
  bench.settle ();
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  bench.mock->control.fail_opens_for (
      ms (14000), make_error_code (tr::error::transport_errc::disconnected));
  std::size_t const opens = bench.mock->control.open_count ();
  bench.mock->control.disconnect ();
  bench.settle ();
  bench.advance (ms (13900));
  EXPECT_EQ (bench.mock->control.open_count (), opens);
  EXPECT_EQ (read->calls, 0);
  ASSERT_GE (bench.session->session_statistics ().failed_attempts, 3u);
  ASSERT_TRUE (bench.run_until ([&] () { return bench.seen->calls != 0; }));
  EXPECT_FALSE (bench.seen->error) << bench.seen->error.message ();
  EXPECT_EQ (bench.mock->control.open_count (), opens + 1);
  ASSERT_TRUE (bench.run_until ([&] () { return read->calls != 0; }));
  EXPECT_FALSE (read->error) << read->error.message ();
}

TEST (LumexProtocolClientReconnect, Reconnect_WaitIsBounded)
{
  // Falsified by a queue without a time: the call would wait for ever.
  pr::client_options_t options = pr::client_options_for<gauge> ();
  options.dialogue.reconnect_wait_ms = 1000;
  stream_bench_t<gauge_a, gauge> bench (options);
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1));
  ASSERT_FALSE (bench.identify ()->error);
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const read = new_outcome<gauge::SLOW_t> ();
  bench.client->SLOW (into<gauge::SLOW_t> (read));
  bench.settle ();
  bench.mock->control.fail_opens_for (
      ms (100000), make_error_code (tr::error::transport_errc::disconnected));
  bench.lose ();
  bench.advance (ms (990));
  EXPECT_EQ (read->calls, 0);
  bench.advance (ms (20));
  EXPECT_EQ (read->calls, 1);
  EXPECT_EQ (read->error, protocol_errc::link_lost);
}

TEST (LumexProtocolClientReconnect, Reconnect_NoWaitEndsThemAtOnce)
{
  pr::client_options_t options = pr::client_options_for<gauge> ();
  options.dialogue.reconnect_wait_ms = 0;
  stream_bench_t<gauge_a, gauge> bench (options);
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const read = new_outcome<gauge::SLOW_t> ();
  bench.client->SLOW (into<gauge::SLOW_t> (read));
  bench.settle ();
  bench.lose ();
  EXPECT_EQ (read->calls, 1);
  EXPECT_EQ (read->error, protocol_errc::link_lost);
}

TEST (LumexProtocolClientReconnect,
      Reconnect_NoWaitEvenWhenTheLinkIsBackAtOnce)
{
  // Falsified by a wait of 0 ms: a table with nothing to check is back before
  // its timer, and the call would go out again.
  pr::client_options_t options = pr::client_options_for<rt::plain_gauge_t> ();
  options.dialogue.reconnect_wait_ms = 0;
  stream_bench_t<rt::plain_gauge_t> bench (options);
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const read = new_outcome<rt::plain_gauge_t::READ_t> ();
  bench.client->READ (into<rt::plain_gauge_t::READ_t> (read));
  bench.settle ();
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  bench.lose ();
  EXPECT_EQ (read->calls, 1);
  EXPECT_EQ (read->error, protocol_errc::link_lost);
}

TEST (LumexProtocolClientReconnect, Reconnect_ASessionThatGivesUpEndsWhatWaits)
{
  // Falsified by nothing said of the end of a cycle: the wait would run out.
  identified_bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const read = new_outcome<gauge::SLOW_t> ();
  bench.client->SLOW (into<gauge::SLOW_t> (read));
  bench.settle ();
  bench.session->close ();
  bench.settle ();
  EXPECT_EQ (read->calls, 1);
  EXPECT_EQ (read->error, protocol_errc::link_lost);
  // A new start of the session comes back as a reconnect.
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  bench.session->start ();
  ASSERT_TRUE (bench.run_until ([&] () { return bench.seen->calls != 0; }));
  EXPECT_FALSE (bench.seen->error) << bench.seen->error.message ();
  EXPECT_EQ (bench.client->statistics ().reconnects, 1u);
}

TEST (LumexProtocolClientReconnect, Reconnect_ToAnotherLayer)
{
  // Falsified by a client that keeps its old layer, and by calls that follow
  // to the new device.
  identified_bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const read = new_outcome<gauge::SLOW_t> ();
  bench.client->SLOW (into<gauge::SLOW_t> (read));
  auto const only = new_outcome<gauge_a::ONLY_A_t> ();
  bench.client->as<gauge_a> ().ONLY_A (into<gauge_a::ONLY_A_t> (only));
  bench.settle ();
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G200", 4, 2));
  int const before = bench.seen->calls;
  bench.mock->control.disconnect ();
  ASSERT_TRUE (
      bench.run_until ([&] () { return bench.seen->calls != before; }));
  bench.settle ();
  // It works, and says who it is.
  EXPECT_FALSE (bench.seen->error) << bench.seen->error.message ();
  EXPECT_EQ (bench.seen->identity.layer, "gauge_b_t");
  EXPECT_EQ (bench.seen->identity.model, "G200");
  EXPECT_STREQ (bench.client->engine ().index ().protocol_name (),
                "gauge_b_t");
  EXPECT_EQ (bench.client->session ().layer (), "gauge_b_t");
  EXPECT_EQ (read->calls, 1);
  EXPECT_EQ (read->error, protocol_errc::link_lost);
  // A row the new layer has not ends the same way.
  EXPECT_EQ (only->calls, 1);
  EXPECT_EQ (only->error, protocol_errc::link_lost);
  // The old layer is closed, the new one open.
  auto const old = new_outcome<gauge_a::ONLY_A_t> ();
  bench.client->as<gauge_a> ().ONLY_A (into<gauge_a::ONLY_A_t> (old));
  bench.settle ();
  EXPECT_EQ (old->error, protocol_errc::not_supported);
}

TEST (LumexProtocolClientReconnect, Reconnect_ToAnotherDeviceOfTheSameLayer)
{
  // Falsified by a comparison of the layer alone: the waiting call goes to a
  // device it was not made for.
  identified_bench_t bench;
  auto const go = new_outcome<gauge::GO_t> ();
  bench.client->GO (into<gauge::GO_t> (go));
  ASSERT_TRUE (bench.run_until ([&] () { return go->calls != 0; }));
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const read = new_outcome<gauge::SLOW_t> ();
  bench.client->SLOW (into<gauge::SLOW_t> (read));
  bench.settle ();
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1, 2));
  int const before = bench.seen->calls;
  bench.mock->control.disconnect ();
  ASSERT_TRUE (
      bench.run_until ([&] () { return bench.seen->calls != before; }));
  bench.settle ();
  EXPECT_EQ (bench.seen->identity.serial_number, "2");
  EXPECT_EQ (read->error, protocol_errc::link_lost);
  EXPECT_EQ (requests_of (bench.device->statistics (), "SLOW"), 1u);
  // No stream row goes to it.
  EXPECT_EQ (requests_of (bench.device->statistics (), "GO"), 1u);
}

TEST (LumexProtocolClientReconnect,
      Reconnect_ADeviceThatNamesNoLayerClosesTheClient)
{
  // Falsified by a gate that opens on any identification.
  identified_bench_t bench;
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G900", 9, 9));
  bench.reconnect ();
  EXPECT_EQ (bench.seen->error, protocol_errc::not_supported);
  EXPECT_EQ (bench.seen->identity.model, "G900");
  EXPECT_EQ (bench.client->identity ().error (), protocol_errc::not_supported);
  EXPECT_TRUE (bench.client->session ().layer ().empty ());
  auto const as = new_outcome<gauge_a::ONLY_A_t> ();
  bench.client->as<gauge_a> ().ONLY_A (into<gauge_a::ONLY_A_t> (as));
  auto const own = new_outcome<gauge::READ_t> ();
  bench.client->READ (into<gauge::READ_t> (own));
  bench.settle ();
  EXPECT_EQ (as->error, protocol_errc::not_supported);
  EXPECT_EQ (own->error, protocol_errc::not_supported);
  EXPECT_EQ (requests_of (bench.device->statistics (), "READ"), 0u);
  // An identify of the application that finds the device opens it.
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1));
  ASSERT_FALSE (bench.identify ()->error);
  bench.device->set_reply<gauge_a::READ_t> (std::uint16_t (8));
  auto const read = new_outcome<gauge::READ_t> ();
  bench.client->READ (into<gauge::READ_t> (read));
  ASSERT_TRUE (bench.run_until ([&] () { return read->calls != 0; }));
  EXPECT_FALSE (read->error) << read->error.message ();
  EXPECT_EQ (read->value, 8);
}

TEST (LumexProtocolClientReconnect,
      Reconnect_ACheckThatFailsEndsWhatWaitsAtOnce)
{
  // Falsified by what waited staying for the time of the outage.
  identified_bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const read = new_outcome<gauge::SLOW_t> ();
  bench.client->SLOW (into<gauge::SLOW_t> (read));
  bench.settle ();
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G900", 9, 9));
  bench.reconnect ();
  EXPECT_EQ (bench.seen->error, protocol_errc::not_supported);
  EXPECT_EQ (read->calls, 1);
  EXPECT_EQ (read->error, protocol_errc::link_lost);
}

TEST (LumexProtocolClientReconnect,
      Reconnect_ANewConnectionChecksAfterAFailedCheck)
{
  identified_bench_t bench;
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G900", 9, 9));
  bench.reconnect ();
  EXPECT_EQ (bench.seen->error, protocol_errc::not_supported);
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1));
  bench.reconnect ();
  EXPECT_FALSE (bench.seen->error) << bench.seen->error.message ();
  ASSERT_TRUE (bench.client->identity ().has_value ());
}

TEST (LumexProtocolClientReconnect,
      Reconnect_AnIdentifyThatTimesOutConnectsAgain)
{
  // Falsified by a dialogue that stays with the silent device, and by one that
  // opens on the timeout.
  identified_bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  std::size_t const opens = bench.mock->control.open_count ();
  bench.lose ();
  auto const read = new_outcome<gauge::READ_t> ();
  ASSERT_TRUE (bench.run_until ([&] () { return bench.seen->calls != 0; }));
  EXPECT_EQ (bench.seen->error, protocol_errc::timeout);
  bench.client->READ (into<gauge::READ_t> (read));
  bench.settle ();
  EXPECT_EQ (read->error, protocol_errc::link_lost);
  // Round again, and then the device answers.
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  int const before = bench.seen->calls;
  ASSERT_TRUE (
      bench.run_until ([&] () { return bench.seen->calls != before; }));
  EXPECT_FALSE (bench.seen->error) << bench.seen->error.message ();
  EXPECT_GE (bench.mock->control.open_count (), opens + 2);
}

TEST (LumexProtocolClientReconnect, Reconnect_ALossDuringTheCheckStartsItAgain)
{
  // Falsified by a check of the past that ends the check of the new link.
  identified_bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  bench.lose ();
  EXPECT_EQ (bench.session->state (), ss::channel_state::open);
  std::uint64_t const reconnects = bench.client->statistics ().reconnects;
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  bench.mock->control.disconnect ();
  ASSERT_TRUE (bench.run_until ([&] () { return bench.seen->calls != 0; }));
  EXPECT_FALSE (bench.seen->error) << bench.seen->error.message ();
  EXPECT_EQ (bench.seen->calls, 1);
  EXPECT_EQ (bench.client->statistics ().reconnects, reconnects + 1);
  ASSERT_TRUE (bench.client->identity ().has_value ());
}

TEST (LumexProtocolClientReconnect, Reconnect_ALossBeforeTheStreamsAreOn)
{
  identified_bench_t bench;
  auto const go = new_outcome<gauge::GO_t> ();
  bench.client->GO (into<gauge::GO_t> (go));
  ASSERT_TRUE (bench.run_until ([&] () { return go->calls != 0; }));
  bool lost_once = false;
  bench.device->on<gauge_a::GO_t> (
      [&bench, &lost_once] (field::record<> const &)
        {
          if (lost_once)
            return;
          lost_once = true;
          bench.mock->control.disconnect ();
        });
  bench.lose ();
  ASSERT_TRUE (bench.run_until ([&] () { return bench.seen->calls != 0; }));
  EXPECT_TRUE (lost_once);
  EXPECT_FALSE (bench.seen->error) << bench.seen->error.message ();
  EXPECT_EQ (requests_of (bench.device->statistics (), "GO"), 3u);
}

TEST (LumexProtocolClientReconnect, Reconnect_ATableWithoutLayerSet)
{
  // The identity says the own layer; another model is another device.
  stream_bench_t<rt::single_t> bench;
  bench.device->set_reply<rt::single_t::WHO_t> (
      gauge_who<pr::reply_of<rt::single_t::WHO_t>> ("S1", 1, 0));
  ASSERT_FALSE (bench.identify ()->error);
  auto const seen = std::make_shared<reconnects_seen_t> ();
  bench.client->on_reconnect (
      [seen] (std::error_code const &error, pr::identity_t const &identity)
        {
          ++seen->calls;
          seen->error = error;
          seen->identity = identity;
        });
  bench.lose ();
  ASSERT_TRUE (bench.run_until ([&] () { return seen->calls != 0; }));
  EXPECT_FALSE (seen->error) << seen->error.message ();
  EXPECT_EQ (seen->identity.layer, "single_t");
  EXPECT_EQ (bench.client->identity ()->model, "S1");
}

TEST (LumexProtocolClientReconnect, Reconnect_ATableWithoutIdentifyWith)
{
  // Nothing to check: the same device, the calls go on, the state is
  // forgotten.
  stream_bench_t<rt::plain_gauge_t> bench;
  auto const seen = std::make_shared<reconnects_seen_t> ();
  bench.client->on_reconnect (
      [seen] (std::error_code const &error, pr::identity_t const &identity)
        {
          ++seen->calls;
          seen->error = error;
          seen->identity = identity;
        });
  bench.client->engine ().set_state (7);
  bench.settle ();
  EXPECT_TRUE (bench.client->engine ().state_known ());
  auto const refused = new_outcome<rt::plain_gauge_t::BUSY_t> ();
  bench.client->BUSY (into<rt::plain_gauge_t::BUSY_t> (refused));
  bench.settle ();
  EXPECT_EQ (refused->error, protocol_errc::not_accepted_in_state);
  bench.lose ();
  EXPECT_TRUE (bench.run_until ([&] () { return seen->calls != 0; }));
  EXPECT_FALSE (seen->error) << seen->error.message ();
  EXPECT_TRUE (seen->identity.model.empty ());
  EXPECT_EQ (bench.client->identity ().error (), protocol_errc::not_supported);
  EXPECT_FALSE (bench.client->engine ().state_known ());
  bench.device->set_reply<rt::plain_gauge_t::READ_t> (std::uint16_t (3));
  auto const read = new_outcome<rt::plain_gauge_t::READ_t> ();
  bench.client->READ (into<rt::plain_gauge_t::READ_t> (read));
  ASSERT_TRUE (bench.run_until ([&] () { return read->calls != 0; }));
  EXPECT_FALSE (read->error) << read->error.message ();
  // The state is known by no frame yet: accepted_in lets it go.
  bench.device->set_state (1);
  bench.device->set_reply<rt::plain_gauge_t::BUSY_t> (std::uint8_t (1));
  auto const busy = new_outcome<rt::plain_gauge_t::BUSY_t> ();
  bench.client->BUSY (into<rt::plain_gauge_t::BUSY_t> (busy));
  ASSERT_TRUE (bench.run_until ([&] () { return busy->calls != 0; }));
  EXPECT_FALSE (busy->error) << busy->error.message ();
}

TEST (LumexProtocolClientReconnect,
      Reconnect_ATableWithoutAReaderOfTheIdentity)
{
  // Compiles and goes on: it cannot identify, so it checks nothing.
  stream_bench_t<rt::unread_t> bench;
  auto const seen = std::make_shared<reconnects_seen_t> ();
  bench.client->on_reconnect (
      [seen] (std::error_code const &error, pr::identity_t const &)
        {
          ++seen->calls;
          seen->error = error;
        });
  bench.lose ();
  EXPECT_TRUE (bench.run_until ([&] () { return seen->calls != 0; }));
  EXPECT_FALSE (seen->error) << seen->error.message ();
}

TEST (LumexProtocolClientReconnect,
      Reconnect_ByOrderWindowEndsWithTheConnection)
{
  // Falsified by a window that stays open: the frame of the new link is
  // dropped as a late reply of the old one.
  pr::client_options_t options = pr::client_options_for<gauge> ();
  options.dialogue.late_reply_window_ms = 5000;
  stream_bench_t<gauge_a, gauge> bench (options);
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1));
  ASSERT_FALSE (bench.identify ()->error);
  auto const seen = watch_reconnects (*bench.client);
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const ordered = new_outcome<gauge::ORDERED_t> ();
  bench.client->ORDERED (into<gauge::ORDERED_t> (ordered));
  // It timed out, and the window opened.
  ASSERT_TRUE (bench.run_until ([&] () { return ordered->calls != 0; }));
  EXPECT_EQ (ordered->error, protocol_errc::timeout);
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  bench.lose ();
  // Well inside the window.
  ASSERT_TRUE (
      bench.run_until ([&] () { return seen->calls != 0; }, ms (1000)));
  EXPECT_FALSE (seen->error) << seen->error.message ();
  EXPECT_EQ (bench.client->statistics ().late_replies, 0u);
}

TEST (LumexProtocolClientReconnect, Reconnect_LateRowsEndWithTheConnection)
{
  // Falsified by a late note that stays: the frame is counted as late, not as
  // what it is.
  identified_bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const set = new_outcome<gauge::SET_t> ();
  bench.client->SET (std::uint8_t (1), into<gauge::SET_t> (set));
  ASSERT_TRUE (bench.run_until ([&] () { return set->calls != 0; }));
  EXPECT_EQ (set->error, protocol_errc::timeout);
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  bench.reconnect ();
  std::uint64_t const unexpected
      = bench.client->statistics ().unexpected_replies;
  bench.mock->control.feed (wire_of ({ 0x11, 0x01 }));
  bench.settle ();
  EXPECT_EQ (bench.client->statistics ().late_replies, 0u);
  EXPECT_EQ (bench.client->statistics ().unexpected_replies, unexpected + 1);
}

TEST (LumexProtocolClientReconnect, Reconnect_AScenarioEndsWithTheConnection)
{
  // Falsified by a run that goes on with a device that forgot its steps.
  identified_bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const run = std::make_shared<outcome_t<std::size_t>> ();
  record_t<std::size_t> done = { run };
  pr::async_run_scenario<rt::read_then_set_scenario> (bench.client->engine (),
                                                      done);
  bench.settle ();
  bench.mock->control.disconnect ();
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  bench.settle ();
  EXPECT_EQ (run->calls, 1);
  EXPECT_EQ (run->error, protocol_errc::link_lost);
  EXPECT_EQ (run->value, 0u);
  bench.advance (ms (200));
  EXPECT_EQ (requests_of (bench.device->statistics (), "READ"), 1u);
  // One that waits for ever too.
  auto const waiting = std::make_shared<outcome_t<std::size_t>> ();
  record_t<std::size_t> done_too = { waiting };
  pr::async_run_scenario<rt::wait_forever_scenario> (bench.client->engine (),
                                                     done_too);
  bench.settle ();
  EXPECT_EQ (waiting->calls, 0);
  bench.lose ();
  EXPECT_EQ (waiting->calls, 1);
  EXPECT_EQ (waiting->error, protocol_errc::link_lost);
}

TEST (LumexProtocolClientReconnect, Reconnect_AQueuedStepOfAScenarioGoesWithIt)
{
  // Falsified by a queue that keeps the step of a run that ended: it would go
  // out on the new connection.
  identified_bench_t bench;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const first = new_outcome<gauge::SLOW_t> ();
  bench.client->SLOW (into<gauge::SLOW_t> (first));
  auto const run = std::make_shared<outcome_t<std::size_t>> ();
  record_t<std::size_t> done = { run };
  pr::async_run_scenario<rt::read_then_set_scenario> (bench.client->engine (),
                                                      done);
  bench.settle ();
  EXPECT_EQ (requests_of (bench.device->statistics (), "READ"), 0u);
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  bench.reconnect ();
  EXPECT_EQ (run->error, protocol_errc::link_lost);
  ASSERT_TRUE (bench.run_until ([&] () { return first->calls != 0; }));
  bench.advance (ms (200));
  EXPECT_EQ (requests_of (bench.device->statistics (), "READ"), 0u);
}

TEST (LumexProtocolClientReconnect,
      Reconnect_AQueuedIdentifyEndsWithTheConnection)
{
  // Falsified by what finds out a device staying for the next one: it could
  // wait for ever, after the wait of the outage too.
  pr::client_options_t options = pr::client_options_for<gauge> ();
  options.dialogue.reconnect_wait_ms = 100;
  stream_bench_t<gauge_a, gauge> bench (options);
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1));
  ASSERT_FALSE (bench.identify ()->error);
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const first = new_outcome<gauge::SLOW_t> ();
  bench.client->SLOW (into<gauge::SLOW_t> (first));
  auto const who = std::make_shared<outcome_t<pr::identity_t>> ();
  record_t<pr::identity_t> done = { who };
  bench.client->async_identify (done);
  bench.settle ();
  EXPECT_EQ (who->calls, 0);
  bench.mock->control.fail_opens_for (
      ms (100000), make_error_code (tr::error::transport_errc::disconnected));
  bench.lose ();
  EXPECT_EQ (who->calls, 1);
  EXPECT_EQ (who->error, protocol_errc::link_lost);
}

TEST (LumexProtocolClientReconnect,
      Reconnect_TheWatchdogSendsTheIdempotentAgain)
{
  // Falsified by the watchdog of a link that does not connect: it ends them.
  stream_bench_t<rt::watched_t> bench;
  bench.device->set_reply<rt::watched_t::WHO_t> (
      gauge_who<pr::reply_of<rt::watched_t::WHO_t>> ("W1", 1, 0));
  ASSERT_FALSE (bench.identify ()->error);
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const read = new_outcome<rt::watched_t::READ_t> ();
  bench.client->READ (into<rt::watched_t::READ_t> (read));
  bench.advance (ms (400));
  EXPECT_GE (bench.client->statistics ().watchdog_expiries, 1u);
  EXPECT_EQ (read->calls, 0);
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  bench.device->set_reply<rt::watched_t::READ_t> (std::uint16_t (5));
  ASSERT_TRUE (bench.run_until ([&] () { return read->calls != 0; }));
  EXPECT_FALSE (read->error) << read->error.message ();
  EXPECT_EQ (read->value, 5);
}

TEST (LumexProtocolClientReconnect, Reconnect_TheWatchdogConnectsAgain)
{
  // Falsified by a drop that closes for good: the session would stay closed.
  stream_bench_t<rt::watched_t> bench;
  bench.device->set_reply<rt::watched_t::WHO_t> (
      gauge_who<pr::reply_of<rt::watched_t::WHO_t>> ("W1", 1, 0));
  ASSERT_FALSE (bench.identify ()->error);
  auto const seen = std::make_shared<reconnects_seen_t> ();
  bench.client->on_reconnect (
      [seen] (std::error_code const &error, pr::identity_t const &identity)
        {
          ++seen->calls;
          seen->error = error;
          seen->identity = identity;
        });
  std::size_t const opens = bench.mock->control.open_count ();
  bench.device->set_behavior (tr::testing::behavior::silent ());
  bench.advance (ms (400));
  EXPECT_GE (bench.client->statistics ().watchdog_expiries, 1u);
  EXPECT_NE (bench.session->state (), ss::channel_state::closed);
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  ASSERT_TRUE (
      bench.run_until ([&] () { return seen->calls != 0 && !seen->error; }));
  EXPECT_GE (bench.mock->control.open_count (), opens + 1);
  EXPECT_EQ (bench.session->state (), ss::channel_state::open);
  // It watches again.
  std::uint64_t const expiries = bench.client->statistics ().watchdog_expiries;
  bench.device->set_behavior (tr::testing::behavior::silent ());
  bench.advance (ms (400));
  EXPECT_GT (bench.client->statistics ().watchdog_expiries, expiries);
}

TEST (LumexProtocolClientReconnect, Reconnect_NoKeepaliveBeforeTheCheck)
{
  // Falsified by a keepalive that goes on an unchecked link.
  stream_bench_t<rt::watched_t> bench;
  bench.device->set_reply<rt::watched_t::WHO_t> (
      gauge_who<pr::reply_of<rt::watched_t::WHO_t>> ("W1", 1, 0));
  ASSERT_FALSE (bench.identify ()->error);
  bench.device->set_behavior (tr::testing::behavior::silent ());
  bench.lose ();
  EXPECT_EQ (bench.session->state (), ss::channel_state::open);
  std::uint64_t const beats = bench.client->statistics ().keepalives;
  // Two periods, inside the time of the watchdog and of the identification.
  bench.advance (ms (250));
  EXPECT_EQ (bench.session->state (), ss::channel_state::open);
  EXPECT_EQ (bench.client->statistics ().keepalives, beats);
}

TEST (LumexProtocolClientReconnect, Statistics_ReconnectsCounted)
{
  // Falsified by a counter that stays 0, and by one that counts the first
  // open.
  stream_bench_t<gauge_a, gauge> bench (false);
  bench.session->start ();
  bench.settle ();
  EXPECT_EQ (bench.client->statistics ().reconnects, 0u);
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1));
  ASSERT_FALSE (bench.identify ()->error);
  auto const seen = watch_reconnects (*bench.client);
  bench.advance (ms (200));
  bench.lose ();
  ASSERT_TRUE (bench.run_until ([&] () { return seen->calls == 1; }));
  EXPECT_EQ (bench.client->statistics ().reconnects, 1u);
  bench.advance (ms (200));
  bench.lose ();
  ASSERT_TRUE (bench.run_until ([&] () { return seen->calls == 2; }));
  EXPECT_EQ (bench.client->statistics ().reconnects, 2u);
  EXPECT_EQ (bench.session->session_statistics ().reconnects, 2u);
}

TEST (LumexProtocolClientReconnect,
      Reconnect_AClientBeforeTheFirstOpenAnswersLinkLost)
{
  stream_bench_t<gauge_a, gauge> bench (false);
  auto const read = new_outcome<gauge::READ_t> ();
  bench.client->READ (into<gauge::READ_t> (read));
  bench.settle ();
  EXPECT_EQ (read->error, protocol_errc::link_lost);
  bench.session->start ();
  bench.settle ();
  bench.device->set_reply<gauge_a::READ_t> (std::uint16_t (4));
  auto const later = new_outcome<gauge::READ_t> ();
  bench.client->READ (into<gauge::READ_t> (later));
  ASSERT_TRUE (bench.run_until ([&] () { return later->calls != 0; }));
  EXPECT_FALSE (later->error) << later->error.message ();
}

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
  ASSERT_TRUE (bench.run_until ([&] () { return bench.seen->calls != 0; }));
  EXPECT_FALSE (bench.seen->error) << bench.seen->error.message ();
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
} // namespace
