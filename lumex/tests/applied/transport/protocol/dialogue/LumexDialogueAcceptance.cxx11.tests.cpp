// Acceptance by state and dangerous rows in the dialogue engine: a request the
// last known state does not accept is refused before anything is written; an
// unknown state sends; a dangerous row logs at info.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <system_error>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/protocol/dialogue/LumexDialogueFeatureSupport.hpp"

namespace
{
namespace ft = lumex_dialogue_feature_test;
namespace pr = ft::pr;
using ft::bytes;
using ft::log_capture_t;
using ft::log_level;
using ft::manual_bench_t;
using ft::ms;
using ft::outcome_ptr;
using ft::protocol_errc;
using ft::quiet_t;
using ft::status_frame;
using ft::tank_state;
using ft::wide_t;

TEST (LumexDialogueAcceptanceTest, AcceptedIn_RefusedBeforeSending)
{
  log_capture_t const log;
  manual_bench_t<quiet_t> bench;
  bench.start ();
  bench.deliver (status_frame (0, tank_state::filling));
  outcome_ptr const out
      = bench.start_exchange (ft::make_exchange<quiet_t, quiet_t::FILL_t> (1));
  EXPECT_EQ (out->calls, 0);
  bench.settle ();
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, protocol_errc::not_accepted_in_state);
  EXPECT_TRUE (bench.link->written ().empty ());
  EXPECT_EQ (bench.engine->statistics ().requests, 0u);
  EXPECT_EQ (bench.engine->statistics ().state_refusals, 1u);
  EXPECT_EQ (log_capture_t::count (log_level::warning, "FILL"), 1u);
}

TEST (LumexDialogueAcceptanceTest, AcceptedIn_UnknownStateSends)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  EXPECT_FALSE (bench.engine->state_known ());
  // Neither 0 (the first state, FILL) nor another value is assumed.
  outcome_ptr const out = bench.start_exchange (
      ft::make_exchange<quiet_t, quiet_t::DRAIN_t> (1));
  bench.settle ();
  EXPECT_EQ (bench.count (0x32), 1u);
  bench.deliver (bytes ({ 0x32, 0x01 }));
  EXPECT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error);
}

TEST (LumexDialogueAcceptanceTest, AcceptedIn_AStateOfTheListSends)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  bench.deliver (status_frame (0, tank_state::draining));
  outcome_ptr const out = bench.start_exchange (
      ft::make_exchange<quiet_t, quiet_t::DRAIN_t> (1));
  bench.settle ();
  EXPECT_EQ (bench.count (0x32), 1u);
  // The first state of the list too.
  bench.deliver (bytes ({ 0x32, 0x01 }));
  bench.deliver (status_frame (0, tank_state::filling));
  bench.start_exchange (ft::make_exchange<quiet_t, quiet_t::DRAIN_t> (1));
  bench.settle ();
  EXPECT_EQ (bench.count (0x32), 2u);
  EXPECT_EQ (out->calls, 1);
}

TEST (LumexDialogueAcceptanceTest,
      AcceptedIn_ARowWithoutTheOptionSendsInAnyState)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  bench.deliver (status_frame (0, tank_state::fault));
  bench.start_exchange (ft::make_exchange<quiet_t, quiet_t::QUERY_t> ());
  bench.settle ();
  EXPECT_EQ (bench.count (0x33), 1u);
}

TEST (LumexDialogueAcceptanceTest, AcceptedIn_SetStateActsOnTheNextRequest)
{
  manual_bench_t<wide_t> bench;
  bench.start ();
  bench.engine->set_state (static_cast<int> (tank_state::fault));
  outcome_ptr const refused
      = bench.start_exchange (ft::make_exchange<wide_t, wide_t::FILL_t> (1));
  bench.settle ();
  EXPECT_EQ (refused->error, protocol_errc::not_accepted_in_state);
  bench.engine->clear_state ();
  bench.start_exchange (ft::make_exchange<wide_t, wide_t::FILL_t> (1));
  bench.settle ();
  EXPECT_EQ (bench.count (0x31), 1u);
}

TEST (LumexDialogueAcceptanceTest,
      AcceptedIn_ANewStateIsReadBeforeTheQueuedRequestStarts)
{
  // The reply of the exchange in flight also tells the new state: the queued
  // request is checked against that one.
  manual_bench_t<quiet_t> bench;
  bench.start ();
  bench.deliver (status_frame (0, tank_state::idle));
  bench.start_exchange (ft::make_exchange<quiet_t, quiet_t::STATUS_t> ());
  outcome_ptr const queued
      = bench.start_exchange (ft::make_exchange<quiet_t, quiet_t::FILL_t> (1));
  bench.settle ();
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x12 }));
  bench.deliver (status_frame (0, tank_state::fault));
  EXPECT_EQ (queued->error, protocol_errc::not_accepted_in_state);
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x12 }));
}

TEST (LumexDialogueAcceptanceTest,
      AcceptedIn_ARefusalInTheQueueLetsTheNextOneGo)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  bench.deliver (status_frame (0, tank_state::fault));
  bench.start_exchange (ft::make_exchange<quiet_t, quiet_t::QUERY_t> ());
  outcome_ptr const refused
      = bench.start_exchange (ft::make_exchange<quiet_t, quiet_t::FILL_t> (1));
  bench.start_exchange (ft::make_exchange<quiet_t, quiet_t::TIMED_t> ());
  bench.settle ();
  bench.deliver (bytes ({ 0x33, 0x00 }));
  EXPECT_EQ (refused->error, protocol_errc::not_accepted_in_state);
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x33, 0x38 }));
}

TEST (LumexDialogueAcceptanceTest,
      Statistics_ResetClearsTheCountersOfTheFeatures)
{
  manual_bench_t<ft::tank_t> bench;
  bench.options.defaults.timeout_ms = 100;
  bench.start ();
  bench.engine->set_keepalive (ft::dialogue::keepalive_from<ft::tank_t> (
      [] () { return std::make_tuple (std::uint32_t (1)); }));
  bench.deliver (status_frame (0));
  bench.deliver (status_frame (1));
  bench.deliver (status_frame (1, tank_state::fault));
  bench.start_exchange (ft::make_exchange<ft::tank_t, ft::tank_t::FILL_t> (1));
  bench.settle ();
  bench.advance (ms (3000));
  pr::dialogue_statistics_t moved = bench.engine->statistics ();
  EXPECT_EQ (moved.state_refusals, 1u);
  EXPECT_EQ (moved.edge_events, 1u);
  EXPECT_GE (moved.keepalives, 2u);
  EXPECT_EQ (moved.watchdog_expiries, 1u);
  bench.engine->reset_statistics ();
  moved = bench.engine->statistics ();
  EXPECT_EQ (moved.state_refusals, 0u);
  EXPECT_EQ (moved.edge_events, 0u);
  EXPECT_EQ (moved.keepalives, 0u);
  EXPECT_EQ (moved.watchdog_expiries, 0u);
}

TEST (LumexDialogueAcceptanceTest, Dangerous_LogsInfo)
{
  log_capture_t const log;
  manual_bench_t<quiet_t> bench;
  bench.start ();
  bench.start_exchange (ft::make_exchange<quiet_t, quiet_t::ERASE_t> (3));
  bench.settle ();
  EXPECT_EQ (bench.count (0x35), 1u);
  EXPECT_EQ (log_capture_t::count (log_level::info,
                                   "a dangerous request: TX quiet_t.ERASE"),
             1u);
  // An ordinary row logs nothing at info.
  bench.deliver (bytes ({ 0x35, 0x00 }));
  bench.start_exchange (ft::make_exchange<quiet_t, quiet_t::QUERY_t> ());
  bench.settle ();
  EXPECT_EQ (log_capture_t::count (log_level::info, "a dangerous request"),
             1u);
}

TEST (LumexDialogueAcceptanceTest, Dangerous_EachAttemptIsLogged)
{
  log_capture_t const log;
  manual_bench_t<quiet_t> bench;
  bench.start ();
  pr::exchange_t request = ft::make_exchange<quiet_t, quiet_t::ERASE_t> (3);
  request.policy = lumex_dialogue_test::policy (100, 1);
  bench.start_exchange (std::move (request));
  bench.settle ();
  bench.advance (ms (100));
  EXPECT_EQ (bench.count (0x35), 2u);
  EXPECT_EQ (log_capture_t::count (log_level::info, "a dangerous request"),
             2u);
}
} // namespace
