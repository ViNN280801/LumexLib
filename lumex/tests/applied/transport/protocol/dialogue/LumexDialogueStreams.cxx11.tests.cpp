// The streams of the dialogue engine: the watch of a stream row and its gaps,
// the enable rows, the link state, the restart of the reading and the
// watchdog. On a manual link and a virtual clock.

#include <cstddef>
#include <future>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/protocol/dialogue/LumexDialogueFeatureSupport.hpp"

namespace
{
namespace ft = lumex_dialogue_feature_test;
namespace pr = ft::pr;
namespace tr = ft::tr;
using ft::bytes;
using ft::bytes_t;
using ft::log_capture_t;
using ft::log_level;
using ft::manual_bench_t;
using ft::ms;
using ft::outcome_ptr;
using ft::protocol_errc;
using ft::quiet_t;
using ft::sequence_outcome_ptr;
using ft::sequence_outcome_t;
using ft::sequence_record_t;
using ft::status_frame;
using ft::tank_t;
using ft::transport_errc;
using ft::watched_t;

sequence_outcome_ptr
start_enable (pr::dialogue_engine &engine)
{
  sequence_outcome_ptr const out = std::make_shared<sequence_outcome_t> ();
  sequence_record_t handler = { out };
  engine.async_enable_streams (handler);
  return out;
}

// --- gaps
// --------------------------------------------------------------------

TEST (LumexDialogueStreamsTest, Stream_GapDetectedAfterTwoIntervals)
{
  log_capture_t const log;
  manual_bench_t<quiet_t> bench;
  bench.start ();
  bench.deliver (status_frame (0));
  bench.advance (ms (199));
  EXPECT_EQ (bench.engine->statistics ().stream_gaps, 0u);
  bench.advance (ms (1));
  EXPECT_EQ (bench.engine->statistics ().stream_gaps, 1u);
  EXPECT_EQ (log_capture_t::count (log_level::warning, "STATUS"), 1u);
  // One per silence: the silence goes on and nothing more is counted.
  bench.advance (ms (5000));
  EXPECT_EQ (bench.engine->statistics ().stream_gaps, 1u);
  EXPECT_EQ (log_capture_t::count (log_level::warning, "STATUS"), 1u);
}

TEST (LumexDialogueStreamsTest, Stream_ALateFrameWithinTwoIntervalsIsNoGap)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  for (int i = 0; i < 20; ++i)
    {
      bench.deliver (status_frame (0));
      // Later than the interval, within two of them.
      bench.advance (ms (150));
    }
  EXPECT_EQ (bench.engine->statistics ().stream_gaps, 0u);
}

TEST (LumexDialogueStreamsTest, Stream_AFrameAfterAGapStartsANewWatch)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  bench.deliver (status_frame (0));
  bench.advance (ms (250));
  EXPECT_EQ (bench.engine->statistics ().stream_gaps, 1u);
  bench.deliver (status_frame (0));
  bench.advance (ms (199));
  EXPECT_EQ (bench.engine->statistics ().stream_gaps, 1u);
  bench.advance (ms (1));
  EXPECT_EQ (bench.engine->statistics ().stream_gaps, 2u);
}

TEST (LumexDialogueStreamsTest, Stream_NotWatchedBeforeItsFirstFrame)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  bench.advance (ms (5000));
  EXPECT_EQ (bench.engine->statistics ().stream_gaps, 0u);
}

TEST (LumexDialogueStreamsTest, Stream_EachRowHasItsOwnInterval)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  bench.deliver (status_frame (0));
  bench.deliver (bytes ({ 0x13, 0x01, 0x00 }));
  bench.advance (ms (200));
  EXPECT_EQ (bench.engine->statistics ().stream_gaps, 1u);
  bench.advance (ms (799));
  EXPECT_EQ (bench.engine->statistics ().stream_gaps, 1u);
  bench.advance (ms (1));
  EXPECT_EQ (bench.engine->statistics ().stream_gaps, 2u);
}

TEST (LumexDialogueStreamsTest,
      Stream_ANearerDeadlineRearmsTheWaitForALongerOne)
{
  // The level (a deadline of 1000 ms) first arms the wait; the state must not
  // wait for it.
  manual_bench_t<quiet_t> bench;
  bench.start ();
  bench.deliver (bytes ({ 0x13, 0x01, 0x00 }));
  bench.advance (ms (10));
  bench.deliver (status_frame (0));
  bench.advance (ms (199));
  EXPECT_EQ (bench.engine->statistics ().stream_gaps, 0u);
  bench.advance (ms (1));
  EXPECT_EQ (bench.engine->statistics ().stream_gaps, 1u);
}

TEST (LumexDialogueStreamsTest, Stream_ADueWaitRacingAFrameCountsTheGapOnce)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  bench.deliver (status_frame (0));
  // The frame's completion is posted first; the clock then ends the wait,
  // whose completion runs after it: the frame came two intervals later.
  ASSERT_TRUE (bench.link->deliver (status_frame (0)));
  bench.clock.advance (ms (200));
  bench.settle ();
  EXPECT_EQ (bench.engine->statistics ().stream_gaps, 1u);
  // The watch goes on from the frame.
  bench.advance (ms (199));
  EXPECT_EQ (bench.engine->statistics ().stream_gaps, 1u);
  bench.advance (ms (1));
  EXPECT_EQ (bench.engine->statistics ().stream_gaps, 2u);
}

TEST (LumexDialogueStreamsTest,
      Stream_ADueWaitRacingAnEarlierFrameCountsNothing)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  bench.deliver (status_frame (0));
  bench.advance (ms (199));
  ASSERT_TRUE (bench.link->deliver (status_frame (0)));
  bench.settle ();
  // The wait of the first frame ends now; the frame moved its deadline.
  bench.advance (ms (1));
  EXPECT_EQ (bench.engine->statistics ().stream_gaps, 0u);
}

TEST (LumexDialogueStreamsTest,
      Stream_AnEventOfTheStreamStillCountsAndReachesItsSubscriber)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  int seen = 0;
  pr::subscription_t const made
      = bench.engine->subscribe (ft::row_of<quiet_t, quiet_t::STATUS_t> (),
                                 [&seen] (bytes_t const &) { ++seen; });
  EXPECT_NE (made.id, 0u);
  bench.deliver (status_frame (0));
  EXPECT_EQ (seen, 1);
  EXPECT_EQ (bench.engine->statistics ().events, 1u);
}

TEST (LumexDialogueStreamsTest, Stream_TheWatchEndsWithTheLink)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  bench.deliver (status_frame (0));
  ASSERT_TRUE (
      bench.link->fail_read (std::make_error_code (std::errc::io_error)));
  bench.settle ();
  bench.advance (ms (5000));
  EXPECT_EQ (bench.engine->statistics ().stream_gaps, 0u);
}

TEST (LumexDialogueStreamsTest,
      Stream_AWithoutFeaturesEngineWatchesByTheIndexAlone)
{
  manual_bench_t<quiet_t> bench;
  bench.options.features = nullptr;
  bench.start ();
  bench.deliver (status_frame (0));
  bench.advance (ms (200));
  EXPECT_EQ (bench.engine->statistics ().stream_gaps, 1u);
}

TEST (LumexDialogueStreamsTest,
      Stream_ATimerThatCannotBeArmedLeavesTheEngineWorking)
{
  manual_bench_t<quiet_t> bench;
  std::shared_ptr<ft::flaky_timers_t> const timers
      = std::make_shared<ft::flaky_timers_t> (bench.clock.source ());
  bench.options.timers = tr::timing::timer_source (timers);
  bench.start ();
  timers->set_throwing (true);
  bench.deliver (status_frame (0));
  EXPECT_EQ (bench.engine->statistics ().events, 1u);
  timers->set_throwing (false);
  // The next frame arms it.
  bench.deliver (status_frame (0));
  bench.advance (ms (200));
  EXPECT_EQ (bench.engine->statistics ().stream_gaps, 1u);
}

// --- enable rows
// -------------------------------------------------------------

TEST (LumexDialogueStreamsTest, Stream_EnableRowsSentInTableOrder)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  sequence_outcome_ptr const out = start_enable (*bench.engine);
  bench.settle ();
  ASSERT_EQ (bench.keys (), (std::vector<int>{ 0x21 }));
  EXPECT_EQ (out->calls, 0);
  bench.deliver (bytes ({ 0x21, 0x00 }));
  ASSERT_EQ (bench.keys (), (std::vector<int>{ 0x21, 0x22 }));
  EXPECT_EQ (out->calls, 0);
  bench.deliver (bytes ({ 0x22, 0x00 }));
  ASSERT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error);
  EXPECT_EQ (out->step, 2u);
}

TEST (LumexDialogueStreamsTest, Stream_EnableWatchesFromTheReply)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  sequence_outcome_ptr const out = start_enable (*bench.engine);
  bench.settle ();
  bench.deliver (bytes ({ 0x21, 0x00 }));
  bench.deliver (bytes ({ 0x22, 0x00 }));
  ASSERT_EQ (out->calls, 1);
  // No frame of the state came: the watch runs from the reply on.
  bench.advance (ms (199));
  EXPECT_EQ (bench.engine->statistics ().stream_gaps, 0u);
  bench.advance (ms (1));
  EXPECT_EQ (bench.engine->statistics ().stream_gaps, 1u);
}

TEST (LumexDialogueStreamsTest,
      Stream_EnableStopsAtTheFirstFailureWithItsIndex)
{
  manual_bench_t<quiet_t> bench;
  bench.options.defaults.timeout_ms = 100;
  bench.start ();
  sequence_outcome_ptr const out = start_enable (*bench.engine);
  bench.settle ();
  bench.deliver (bytes ({ 0x21, 0x00 }));
  ASSERT_TRUE (bench.run_until ([&out] () { return out->calls != 0; }));
  EXPECT_EQ (out->error, protocol_errc::timeout);
  EXPECT_EQ (out->step, 1u);
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x21, 0x22 }));
}

TEST (LumexDialogueStreamsTest,
      Stream_EnableWithoutFeaturesEndsAtOnceAndNotInsideTheCall)
{
  manual_bench_t<quiet_t> bench;
  bench.options.features = nullptr;
  bench.start ();
  sequence_outcome_ptr const out = start_enable (*bench.engine);
  EXPECT_EQ (out->calls, 0);
  bench.settle ();
  ASSERT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error);
  EXPECT_EQ (out->step, 0u);
  EXPECT_TRUE (bench.keys ().empty ());
}

TEST (LumexDialogueStreamsTest,
      Stream_EnableEndsWithCancelledByItsSlotAndTheLinkStays)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  asio::cancellation_signal signal;
  sequence_outcome_ptr const out = std::make_shared<sequence_outcome_t> ();
  sequence_record_t handler = { out };
  bench.engine->async_enable_streams (
      asio::bind_cancellation_slot (signal.slot (), handler));
  bench.settle ();
  ASSERT_EQ (bench.keys ().size (), 1u);
  signal.emit (asio::cancellation_type::terminal);
  bench.settle ();
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, transport_errc::cancelled);
  EXPECT_EQ (out->step, 0u);
  // The exchange of the step ended with it: the flight is free, the link goes
  // on.
  outcome_ptr const next
      = bench.start_exchange (ft::make_exchange<quiet_t, quiet_t::QUERY_t> ());
  bench.settle ();
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x21, 0x33 }));
  bench.deliver (bytes ({ 0x33, 0x01 }));
  EXPECT_EQ (next->calls, 1);
  EXPECT_FALSE (next->error);
  // The reply that comes later is a late one and starts nothing.
  bench.deliver (bytes ({ 0x21, 0x00 }));
  EXPECT_EQ (bench.engine->statistics ().late_replies, 1u);
  EXPECT_EQ (bench.keys ().size (), 2u);
  EXPECT_EQ (out->calls, 1);
}

TEST (LumexDialogueStreamsTest, Stream_EnableOnAMovedFromEngineAnswersNotOpen)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  pr::dialogue_engine moved (std::move (*bench.engine));
  std::future<std::size_t> refused
      = bench.engine->async_enable_streams (asio::use_future);
  EXPECT_THROW (refused.get (), std::system_error);
  EXPECT_FALSE (bench.engine->state_known ());
  EXPECT_EQ (bench.engine->last_state (), 0);
  EXPECT_FALSE (bench.engine->link_is_up ());
  EXPECT_FALSE (bench.engine->set_keepalive (ft::tank_keepalive_none ()));
  bench.engine->set_state (1);
  bench.engine->clear_state ();
  bench.engine->on_link_state (pr::link_state_handler_t ());
  EXPECT_EQ (bench.engine->on_edge (nullptr, pr::edge_handler_t ()).id, 0u);
}

// --- the link state and the restart of the reading
// ---------------------------

TEST (LumexDialogueLinkTest, Read_RestartsAfterALostLinkWithoutARequest)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  EXPECT_EQ (bench.link->reads_started (), 1u);
  ASSERT_TRUE (
      bench.link->fail_read (std::make_error_code (std::errc::io_error)));
  bench.settle ();
  EXPECT_EQ (bench.link->pending_reads (), 0u);
  bench.advance (ms (99));
  EXPECT_EQ (bench.link->pending_reads (), 0u);
  bench.advance (ms (1));
  EXPECT_EQ (bench.link->pending_reads (), 1u);
  EXPECT_EQ (bench.link->reads_started (), 2u);
  // A stream comes back with it.
  int seen = 0;
  bench.engine->subscribe (ft::row_of<quiet_t, quiet_t::STATUS_t> (),
                           [&seen] (bytes_t const &) { ++seen; });
  bench.deliver (status_frame (0));
  EXPECT_EQ (seen, 1);
}

TEST (LumexDialogueLinkTest,
      Read_FailingAgainWaitsTwiceAsLongUpToTheLongestWait)
{
  manual_bench_t<quiet_t> bench;
  bench.options.read_restart_ms = 100;
  bench.options.read_restart_max_ms = 350;
  bench.start ();
  std::error_code const failed = std::make_error_code (std::errc::io_error);
  ms const expected[] = { ms (100), ms (200), ms (350), ms (350) };
  for (ms const wait : expected)
    {
      std::size_t const before = bench.link->reads_started ();
      ASSERT_TRUE (bench.link->fail_read (failed));
      bench.settle ();
      bench.advance (wait - ms (1));
      EXPECT_EQ (bench.link->reads_started (), before) << wait.count ();
      bench.advance (ms (1));
      EXPECT_EQ (bench.link->reads_started (), before + 1) << wait.count ();
    }
  // A frame starts the count over.
  bench.deliver (status_frame (0));
  std::size_t const before = bench.link->reads_started ();
  ASSERT_TRUE (bench.link->fail_read (failed));
  bench.settle ();
  bench.advance (ms (100));
  EXPECT_EQ (bench.link->reads_started (), before + 1);
}

TEST (LumexDialogueLinkTest,
      Read_ACancelledReadOfAnOpenLinkStartsAgainOnceAtOnceThenWaits)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  EXPECT_EQ (bench.link->reads_started (), 1u);
  bench.link->cancel_reads ();
  bench.settle ();
  EXPECT_EQ (bench.link->reads_started (), 2u);
  // A link whose reads keep ending cancelled does not spin.
  bench.link->cancel_reads ();
  bench.settle ();
  EXPECT_EQ (bench.link->reads_started (), 2u);
  bench.advance (ms (99));
  EXPECT_EQ (bench.link->reads_started (), 2u);
  bench.advance (ms (1));
  EXPECT_EQ (bench.link->reads_started (), 3u);
  // The state of the link stays up: nothing was lost.
  EXPECT_TRUE (bench.engine->link_is_up ());
}

TEST (LumexDialogueLinkTest,
      Read_WithoutARestartTimeReadingStartsAgainWithTheNextRequest)
{
  manual_bench_t<quiet_t> bench;
  bench.options.read_restart_ms = 0;
  bench.start ();
  ASSERT_TRUE (
      bench.link->fail_read (std::make_error_code (std::errc::io_error)));
  bench.settle ();
  bench.advance (ms (10000));
  EXPECT_EQ (bench.link->reads_started (), 1u);
  outcome_ptr const out
      = bench.start_exchange (ft::make_exchange<quiet_t, quiet_t::QUERY_t> ());
  bench.settle ();
  EXPECT_EQ (bench.link->reads_started (), 2u);
  bench.deliver (bytes ({ 0x33, 0x01 }));
  EXPECT_EQ (out->calls, 1);
}

TEST (LumexDialogueLinkTest, Read_TheRestartEndsWithClose)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  ASSERT_TRUE (
      bench.link->fail_read (std::make_error_code (std::errc::io_error)));
  bench.settle ();
  bench.engine->close ();
  bench.settle ();
  bench.advance (ms (10000));
  EXPECT_EQ (bench.link->reads_started (), 1u);
}

TEST (LumexDialogueLinkTest, Read_ADueRestartRacingARequestStartsOneRead)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  ASSERT_TRUE (
      bench.link->fail_read (std::make_error_code (std::errc::io_error)));
  bench.settle ();
  // The restart is due and posted; the request starts a read before it runs.
  bench.clock.advance (ms (100));
  outcome_ptr const out
      = bench.start_exchange (ft::make_exchange<quiet_t, quiet_t::QUERY_t> ());
  bench.settle ();
  EXPECT_EQ (bench.link->pending_reads (), 1u);
  EXPECT_EQ (bench.link->reads_started (), 2u);
}

TEST (LumexDialogueLinkTest, LinkState_SignalsLostOnceAndUpWithTheNextFrame)
{
  log_capture_t const log;
  manual_bench_t<quiet_t> bench;
  bench.start ();
  std::vector<pr::link_state> seen;
  std::vector<std::error_code> reasons;
  bench.engine->on_link_state (
      [&] (pr::link_state state, std::error_code const &reason)
        {
          seen.push_back (state);
          reasons.push_back (reason);
        });
  EXPECT_TRUE (bench.engine->link_is_up ());
  std::error_code const failed = std::make_error_code (std::errc::io_error);
  ASSERT_TRUE (bench.link->fail_read (failed));
  bench.settle ();
  ASSERT_EQ (seen.size (), 1u);
  EXPECT_EQ (seen[0], pr::link_state::lost);
  EXPECT_EQ (reasons[0], failed);
  EXPECT_FALSE (bench.engine->link_is_up ());
  // Failing again changes nothing.
  bench.advance (ms (100));
  ASSERT_TRUE (bench.link->fail_read (failed));
  bench.settle ();
  EXPECT_EQ (seen.size (), 1u);
  bench.advance (ms (200));
  bench.deliver (status_frame (0));
  ASSERT_EQ (seen.size (), 2u);
  EXPECT_EQ (seen[1], pr::link_state::up);
  EXPECT_FALSE (reasons[1]);
  EXPECT_TRUE (bench.engine->link_is_up ());
}

TEST (LumexDialogueLinkTest,
      LinkState_ReadFailuresWithoutAnExchangeAreLoggedOnce)
{
  log_capture_t const log;
  manual_bench_t<quiet_t> bench;
  bench.start ();
  std::error_code const failed = std::make_error_code (std::errc::io_error);
  ASSERT_TRUE (bench.link->fail_read (failed));
  bench.settle ();
  bench.advance (ms (100));
  ASSERT_TRUE (bench.link->fail_read (failed));
  bench.settle ();
  EXPECT_EQ (log_capture_t::count (log_level::info, "reading stopped"), 1u);
}

TEST (LumexDialogueLinkTest,
      LinkState_AHandlerThatThrowsLeavesTheRunAndTheReadingGoesOn)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  bench.engine->on_link_state ([] (pr::link_state, std::error_code const &)
                                 { throw std::runtime_error ("handler"); });
  ASSERT_TRUE (
      bench.link->fail_read (std::make_error_code (std::errc::io_error)));
  EXPECT_THROW (bench.context.poll (), std::runtime_error);
  bench.settle ();
  bench.advance (ms (100));
  EXPECT_EQ (bench.link->pending_reads (), 1u);
  // The frame is read although the handler throws again.
  int seen = 0;
  bench.engine->subscribe (ft::row_of<quiet_t, quiet_t::STATUS_t> (),
                           [&seen] (bytes_t const &) { ++seen; });
  ASSERT_TRUE (bench.link->deliver (status_frame (0)));
  bench.context.restart ();
  EXPECT_THROW (bench.context.poll (), std::runtime_error);
  EXPECT_EQ (seen, 1);
  EXPECT_EQ (bench.link->pending_reads (), 1u);
}

TEST (LumexDialogueLinkTest, LinkState_AHandlerMayDestroyTheEngine)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  bench.engine->on_link_state (
      [&bench] (pr::link_state, std::error_code const &)
        { bench.engine.reset (); });
  ASSERT_TRUE (
      bench.link->fail_read (std::make_error_code (std::errc::io_error)));
  bench.settle ();
  EXPECT_EQ (bench.engine, nullptr);
  bench.advance (ms (1000));
  EXPECT_EQ (bench.link->reads_started (), 1u);
}

// --- the watchdog
// ------------------------------------------------------------

TEST (LumexDialogueWatchdogTest, Watchdog_SilenceEndsLinkLost)
{
  log_capture_t const log;
  manual_bench_t<watched_t> bench;
  bench.start ();
  outcome_ptr const out = bench.start_exchange (
      ft::make_exchange<watched_t, watched_t::QUERY_t> ());
  bench.advance (ms (299));
  EXPECT_EQ (bench.engine->statistics ().watchdog_expiries, 0u);
  EXPECT_TRUE (bench.engine->link_is_up ());
  bench.advance (ms (1));
  EXPECT_EQ (bench.engine->statistics ().watchdog_expiries, 1u);
  EXPECT_FALSE (bench.engine->link_is_up ());
  EXPECT_EQ (log_capture_t::count (log_level::error, "watchdog"), 1u);
  EXPECT_EQ (bench.link->closes (), 1u);
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, protocol_errc::link_lost);
}

TEST (LumexDialogueWatchdogTest, Watchdog_OnlyWhenTheProtocolNamesOne)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  bench.advance (ms (100000));
  EXPECT_EQ (bench.engine->statistics ().watchdog_expiries, 0u);
  EXPECT_TRUE (bench.engine->link_is_up ());
  EXPECT_EQ (bench.link->closes (), 0u);
}

TEST (LumexDialogueWatchdogTest, Watchdog_EveryFrameMovesTheDeadline)
{
  manual_bench_t<watched_t> bench;
  bench.start ();
  for (int i = 0; i < 10; ++i)
    {
      bench.advance (ms (250));
      // A frame of no row counts: the link talks.
      bench.deliver (bytes ({ 0x7E }));
    }
  EXPECT_EQ (bench.engine->statistics ().watchdog_expiries, 0u);
  bench.advance (ms (299));
  EXPECT_EQ (bench.engine->statistics ().watchdog_expiries, 0u);
  bench.advance (ms (1));
  EXPECT_EQ (bench.engine->statistics ().watchdog_expiries, 1u);
}

TEST (LumexDialogueWatchdogTest, Watchdog_ADueWaitRacingAFrameEndsNothing)
{
  manual_bench_t<watched_t> bench;
  bench.start ();
  // The frame's completion runs first, then the wait that the clock ended.
  ASSERT_TRUE (bench.link->deliver (status_frame (0)));
  bench.clock.advance (ms (300));
  bench.settle ();
  EXPECT_EQ (bench.engine->statistics ().watchdog_expiries, 0u);
  EXPECT_TRUE (bench.engine->link_is_up ());
  // The watch goes on from the frame.
  bench.advance (ms (299));
  EXPECT_EQ (bench.engine->statistics ().watchdog_expiries, 0u);
  bench.advance (ms (1));
  EXPECT_EQ (bench.engine->statistics ().watchdog_expiries, 1u);
}

TEST (LumexDialogueWatchdogTest, Watchdog_WatchesAgainOnceALostLinkTalks)
{
  manual_bench_t<watched_t> bench;
  bench.start ();
  bench.advance (ms (300));
  EXPECT_EQ (bench.engine->statistics ().watchdog_expiries, 1u);
  // While lost nothing more is counted.
  bench.advance (ms (5000));
  EXPECT_EQ (bench.engine->statistics ().watchdog_expiries, 1u);
  // A link of one's own may come back.
  ASSERT_TRUE (bench.run_until (
      [&bench] () { return bench.link->pending_reads () == 1; }));
  bench.deliver (status_frame (0));
  EXPECT_TRUE (bench.engine->link_is_up ());
  bench.advance (ms (299));
  EXPECT_EQ (bench.engine->statistics ().watchdog_expiries, 1u);
  bench.advance (ms (1));
  EXPECT_EQ (bench.engine->statistics ().watchdog_expiries, 2u);
}

TEST (LumexDialogueWatchdogTest, Watchdog_ALinkLostByARead_IsNotEndedAgain)
{
  manual_bench_t<watched_t> bench;
  bench.start ();
  ASSERT_TRUE (
      bench.link->fail_read (std::make_error_code (std::errc::io_error)));
  bench.settle ();
  bench.advance (ms (1000));
  EXPECT_EQ (bench.engine->statistics ().watchdog_expiries, 0u);
  EXPECT_EQ (bench.link->closes (), 0u);
}

TEST (LumexDialogueWatchdogTest, Watchdog_LogsErrorAndAHandlerSeesTheLinkLost)
{
  log_capture_t const log;
  manual_bench_t<watched_t> bench;
  bench.start ();
  std::vector<std::error_code> reasons;
  bench.engine->on_link_state (
      [&reasons] (pr::link_state, std::error_code const &reason)
        { reasons.push_back (reason); });
  bench.advance (ms (300));
  ASSERT_EQ (reasons.size (), 1u);
  EXPECT_EQ (reasons[0], protocol_errc::link_lost);
}

TEST (LumexDialogueWatchdogTest, Watchdog_DoesNotFireAfterClose)
{
  manual_bench_t<watched_t> bench;
  bench.start ();
  bench.engine->close ();
  bench.settle ();
  bench.advance (ms (1000));
  EXPECT_EQ (bench.engine->statistics ().watchdog_expiries, 0u);
}

TEST (LumexDialogueWatchdogTest, Options_ForTakeWhatTheHeaderSays)
{
  pr::dialogue_options_t const options = ft::dialogue::options_for<tank_t> ();
  EXPECT_EQ (options.max_in_flight, 1u);
  EXPECT_EQ (options.defaults.timeout_ms, 400u);
  EXPECT_EQ (options.defaults.retries, 1u);
  EXPECT_EQ (options.late_reply_window_ms, 30u);
  ASSERT_NE (options.features, nullptr);
  EXPECT_EQ (options.features->watchdog_ms, 2500u);
  EXPECT_EQ (options.features->keepalive_period_ms, 1000u);
  EXPECT_STREQ (options.features->keepalive_row, "CLOCK");
  EXPECT_STREQ (options.features->state_row, "STATUS");
  EXPECT_EQ (options.features->state_field, 1u);
  EXPECT_EQ (options.features->min_interval_ms, 0u);
  pr::dialogue_options_t const plain
      = ft::dialogue::options_for<ft::wide_t> ();
  EXPECT_EQ (plain.defaults.timeout_ms, pr::retry_policy_t ().timeout_ms);
  EXPECT_EQ (plain.features->watchdog_ms, 0u);
  EXPECT_EQ (plain.features->state_row, nullptr);
}

TEST (LumexDialogueWatchdogTest, Features_ListEveryRowInTableOrder)
{
  pr::protocol_features_t const *const features
      = ft::dialogue::features_of<tank_t> ();
  ASSERT_EQ (features->row_count, 26u);
  EXPECT_STREQ (features->rows[18].reply_row, "CHANGED");
  EXPECT_EQ (features->rows[17].reply_row, nullptr);
  EXPECT_STREQ (features->rows[0].identifier, "ACK");
  EXPECT_STREQ (features->rows[2].identifier, "STATUS");
  EXPECT_EQ (features->rows[2].edge_count, 2u);
  EXPECT_EQ (features->rows[2].edges[1].from, 1);
  EXPECT_EQ (features->rows[2].edges[1].to, 0);
  EXPECT_NE (features->rows[2].read_levels, nullptr);
  EXPECT_EQ (features->rows[3].read_levels, nullptr);
  EXPECT_STREQ (features->rows[4].enables, "STATUS");
  EXPECT_EQ (features->rows[9].min_interval_ms, 50u);
  EXPECT_EQ (features->rows[1].empty_request, nullptr);
  EXPECT_NE (features->rows[8].empty_request, nullptr);
  EXPECT_NE (features->rows[11].reader, nullptr);
  EXPECT_EQ (features->rows[12].how, ft::dialogue::correlation::by_order);
}
} // namespace
