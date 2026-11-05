// The generated client over a session, the part of the queue: what a loss does
// to the exchanges in flight and queued (sent again, ended, the order), the
// bound of the wait from the first loss (also when the link flaps and when the
// check keeps failing), the late-reply window, a scenario, the watchdog and
// the statistics. On a virtual clock; the device table is invented.

#include <cstdint>
#include <memory>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/protocol/client/LumexClientReconnectBench.hpp"

namespace
{
using namespace lumex_client_test;

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

TEST (LumexProtocolClientReconnect, Reconnect_ResentExchangesKeepTheirOrder)
{
  // Falsified by a loss that puts the exchanges back in reverse.
  pr::client_options_t options = pr::client_options_for<gauge> ();
  options.dialogue.max_in_flight = 3;
  stream_bench_t<gauge_a, gauge> bench (options);
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1));
  ASSERT_FALSE (bench.identify ()->error);
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const read = new_outcome<gauge::READ_t> ();
  auto const slow = new_outcome<gauge::SLOW_t> ();
  bench.client->READ (into<gauge::READ_t> (read));
  bench.client->SLOW (into<gauge::SLOW_t> (slow));
  bench.settle ();
  std::size_t const mark = bench.mock->control.written ().size ();
  bench.mock->control.disconnect ();
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  ASSERT_TRUE (bench.run_until (
      [&] () { return read->calls != 0 && slow->calls != 0; }));
  // WHO of the check, then READ and SLOW as they went.
  EXPECT_EQ (keys_after (bench.mock->control.written (), mark),
             (std::vector<int>{ 0x01, 0x10, 0x17 }));
}

TEST (LumexProtocolClientReconnect, Reconnect_ResentGoesBeforeTheQueued)
{
  // Falsified by a loss that puts the exchanges back behind the queue.
  stream_bench_t<gauge_a, gauge> bench;
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1));
  ASSERT_FALSE (bench.identify ()->error);
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const slow = new_outcome<gauge::SLOW_t> ();
  auto const read = new_outcome<gauge::READ_t> ();
  bench.client->SLOW (into<gauge::SLOW_t> (slow));
  bench.client->READ (into<gauge::READ_t> (read));
  bench.settle ();
  std::size_t const mark = bench.mock->control.written ().size ();
  bench.mock->control.disconnect ();
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  ASSERT_TRUE (bench.run_until (
      [&] () { return read->calls != 0 && slow->calls != 0; }));
  EXPECT_EQ (keys_after (bench.mock->control.written (), mark),
             (std::vector<int>{ 0x01, 0x17, 0x10 }));
}

TEST (LumexProtocolClientReconnect,
      Reconnect_WaitIsBoundedFromTheFirstLossWhenTheCheckKeepsFailing)
{
  // Falsified by a wait that starts again at each loss during the check, and
  // by a session that counts each such link as a good one: a device that
  // opens and says nothing would be asked 291 times in 29 s.
  stream_bench_t<gauge_a, gauge> bench;
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1));
  ASSERT_FALSE (bench.identify ()->error);
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const slow = new_outcome<gauge::SLOW_t> ();
  bench.client->SLOW (into<gauge::SLOW_t> (slow));
  bench.settle ();
  bench.lose ();
  bench.advance (ms (29000));
  EXPECT_EQ (slow->calls, 0);
  // The backoff of the session grows while no check succeeds.
  EXPECT_LT (bench.session->session_statistics ().reconnects, 40u);
  EXPECT_LT (requests_of (bench.device->statistics (), "WHO"), 45u);
  bench.advance (ms (2500));
  EXPECT_EQ (slow->calls, 1);
  EXPECT_EQ (slow->error, protocol_errc::link_lost);
}

TEST (LumexProtocolClientReconnect, Reconnect_ASecondOutageGetsItsOwnWait)
{
  // Falsified by a wait that stays armed from the first outage.
  stream_bench_t<gauge_a, gauge> bench;
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1));
  ASSERT_FALSE (bench.identify ()->error);
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const first = new_outcome<gauge::SLOW_t> ();
  bench.client->SLOW (into<gauge::SLOW_t> (first));
  bench.settle ();
  bench.mock->control.disconnect ();
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  ASSERT_TRUE (bench.run_until ([&] () { return first->calls != 0; }));
  EXPECT_FALSE (first->error);
  bench.advance (ms (20000));
  bench.device->set_behavior (tr::testing::behavior::silent ());
  auto const second = new_outcome<gauge::SLOW_t> ();
  bench.client->SLOW (into<gauge::SLOW_t> (second));
  bench.settle ();
  bench.lose ();
  bench.advance (ms (20000));
  EXPECT_EQ (second->calls, 0) << "ended by the wait of the first outage";
  bench.advance (ms (11000));
  EXPECT_EQ (second->calls, 1);
  EXPECT_EQ (second->error, protocol_errc::link_lost);
}

TEST (LumexProtocolClientReconnect,
      Reconnect_AFlappingLinkResendsOnlyForTheWaitFromTheFirstLoss)
{
  // Falsified by a resend that counts its time again at each loss: a link
  // that goes faster than the timeout of the exchange would send it for ever.
  stream_bench_t<gauge_a, gauge> bench;
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1));
  ASSERT_FALSE (bench.identify ()->error);
  bench.device->refuse<gauge_a::READ_t> ();
  auto const read = new_outcome<gauge::READ_t> ();
  bench.client->READ (into<gauge::READ_t> (read));
  bench.settle ();
  int turn = 0;
  for (; turn < 400 && read->calls == 0; ++turn)
    {
      bench.lose ();
      bench.advance (ms (160));
    }
  // 30000 ms by default, counted from the first loss.
  EXPECT_GE (turn * 160, 29000);
  EXPECT_LE (turn * 160, 31000);
  ASSERT_EQ (read->calls, 1);
  EXPECT_EQ (read->error, protocol_errc::link_lost);
  EXPECT_GT (requests_of (bench.device->statistics (), "READ"), 100u);
  // And it is not sent again.
  std::uint64_t const sent = requests_of (bench.device->statistics (), "READ");
  bench.lose ();
  bench.advance (ms (500));
  EXPECT_EQ (requests_of (bench.device->statistics (), "READ"), sent);
}

TEST (LumexProtocolClientReconnect,
      Reconnect_AQueuedExchangeIsBoundedAcrossOutagesToo)
{
  // Falsified by a queue that waits for a place across each outage with a
  // wait of its own: behind an exchange that is sent again it would wait for
  // ever, and go out when that one ends.
  stream_bench_t<gauge_a, gauge> bench;
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1));
  ASSERT_FALSE (bench.identify ()->error);
  bench.device->refuse<gauge_a::READ_t> ();
  auto const read = new_outcome<gauge::READ_t> ();
  auto const set = new_outcome<gauge::SET_t> ();
  bench.client->READ (into<gauge::READ_t> (read));
  bench.client->SET (std::uint8_t (1), into<gauge::SET_t> (set));
  bench.settle ();
  int turn = 0;
  for (; turn < 400 && read->calls == 0; ++turn)
    {
      bench.lose ();
      bench.advance (ms (160));
    }
  ASSERT_EQ (read->calls, 1);
  EXPECT_EQ (read->error, protocol_errc::link_lost);
  EXPECT_EQ (set->calls, 1);
  EXPECT_EQ (set->error, protocol_errc::link_lost);
  bench.advance (ms (500));
  EXPECT_EQ (requests_of (bench.device->statistics (), "SET"), 0u);
}

TEST (LumexProtocolClientReconnect,
      Reconnect_EachQueuedExchangeEndsAtItsOwnTimeFromItsFirstLoss)
{
  // Falsified by one timer that ends the whole queue when the first of them
  // runs out, and by a timer set for the latest end: the one that was lost
  // first ends at 30 s, the one that came later at 38 s, though the link is
  // away since second 24.
  stream_bench_t<gauge_a, gauge> bench;
  bench.device->set_reply<gauge_a::WHO_t> (gauge_who ("G100", 3, 1));
  ASSERT_FALSE (bench.identify ()->error);
  bench.device->refuse<gauge_a::SLOW_t> ();
  auto const slow = new_outcome<gauge::SLOW_t> ();
  auto const read = new_outcome<gauge::READ_t> ();
  auto const set = new_outcome<gauge::SET_t> ();
  bench.client->SLOW (into<gauge::SLOW_t> (slow));
  bench.client->READ (into<gauge::READ_t> (read));
  bench.settle ();
  // The first loss is second 0; the link comes back at once each time.
  bench.lose ();
  bench.advance (ms (4000));
  bench.client->SET (std::uint8_t (1), into<gauge::SET_t> (set));
  bench.settle ();
  bench.advance (ms (4000));
  bench.lose ();
  bench.advance (ms (8000));
  bench.lose ();
  bench.advance (ms (8000));
  EXPECT_EQ (slow->calls, 0);
  EXPECT_EQ (read->calls, 0);
  EXPECT_EQ (set->calls, 0);
  // Second 24: the link goes and does not come back.
  bench.mock->control.fail_opens_for (
      ms (100000), make_error_code (tr::error::transport_errc::disconnected));
  bench.lose ();
  bench.advance (ms (5500));
  EXPECT_EQ (slow->calls, 0);
  EXPECT_EQ (read->calls, 0);
  EXPECT_EQ (set->calls, 0);
  bench.advance (ms (1000));
  EXPECT_EQ (slow->calls, 1);
  EXPECT_EQ (slow->error, protocol_errc::link_lost);
  EXPECT_EQ (read->calls, 1);
  EXPECT_EQ (read->error, protocol_errc::link_lost);
  EXPECT_EQ (set->calls, 0);
  bench.advance (ms (7000));
  EXPECT_EQ (set->calls, 0);
  bench.advance (ms (1000));
  EXPECT_EQ (set->calls, 1);
  EXPECT_EQ (set->error, protocol_errc::link_lost);
}
} // namespace
