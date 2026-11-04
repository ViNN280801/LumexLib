// A scenario on the dialogue engine (LUMEX_PROTOCOL_SCENARIO, its executor):
// the order of the steps, a branch, the failure of a step with its index, an
// expect and a wait for a state with their times, the races with their
// timers, a cancellation, a close, a predicate that throws, the tokens and
// what a scenario may own.

#include <chrono>
#include <cstddef>
#include <cstdint>
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
namespace field = lumex::core::protocol::field;
using ft::bytes;
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
using ft::tank_state;
using ft::transport_errc;
using status_value_t = field::value_of<ft::tank_status_t>::type;

bool
valve_open (status_value_t const &status)
{
  return field::get<0> (status) == 1;
}

bool
throws_always (status_value_t const &)
{
  throw std::runtime_error ("predicate");
}

LUMEX_PROTOCOL_SCENARIO (
    wash_scenario, quiet_t, (send<quiet_t::FILL_t> (5)),
    (expect<quiet_t::LEVEL_t> ()), (wait_state (tank_state::draining, 1000)),
    (send<quiet_t::STATUS_t> ()),
    (branch_on_reply<quiet_t::STATUS_t> (&valve_open, "open")),
    (send<quiet_t::QUERY_t> ()), (jump ("end")), (label ("open")),
    (send<quiet_t::DRAIN_t> (9)), (label ("end")));

LUMEX_PROTOCOL_SCENARIO (two_sends_scenario, quiet_t,
                         (send<quiet_t::QUERY_t> ()),
                         (send<quiet_t::TIMED_t> ()));
LUMEX_PROTOCOL_SCENARIO (expect_scenario, quiet_t,
                         (expect<quiet_t::LEVEL_t> (200)),
                         (send<quiet_t::QUERY_t> ()));
LUMEX_PROTOCOL_SCENARIO (expect_default_scenario, quiet_t,
                         (expect<quiet_t::CLOCK_t> ()));
LUMEX_PROTOCOL_SCENARIO (send_then_expect_scenario, quiet_t,
                         (send<quiet_t::QUERY_t> ()),
                         (expect<quiet_t::LEVEL_t> (200)));
LUMEX_PROTOCOL_SCENARIO (wait_scenario, quiet_t,
                         (wait_state (tank_state::draining, 300)),
                         (send<quiet_t::QUERY_t> ()));
LUMEX_PROTOCOL_SCENARIO (wait_for_ever_scenario, quiet_t,
                         (wait_state (tank_state::fault, 0)));
LUMEX_PROTOCOL_SCENARIO (loop_scenario, quiet_t, (label ("again")),
                         (jump ("again")));
LUMEX_PROTOCOL_SCENARIO (poll_loop_scenario, quiet_t, (label ("again")),
                         (send<quiet_t::STATUS_t> ()),
                         (branch_on_reply<quiet_t::STATUS_t> (
                             [] (status_value_t const &status)
                               { return field::get<0> (status) == 0; },
                             "again")));
LUMEX_PROTOCOL_SCENARIO (lost_label_scenario, quiet_t, (jump ("nowhere")),
                         (send<quiet_t::QUERY_t> ()));
LUMEX_PROTOCOL_SCENARIO (no_reply_yet_scenario, quiet_t,
                         (branch_on_reply<quiet_t::STATUS_t> (&valve_open,
                                                              "end")),
                         (label ("end")));
LUMEX_PROTOCOL_SCENARIO (throwing_scenario, quiet_t,
                         (send<quiet_t::STATUS_t> ()),
                         (branch_on_reply<quiet_t::STATUS_t> (&throws_always,
                                                              "end")),
                         (send<quiet_t::QUERY_t> ()), (label ("end")));
LUMEX_PROTOCOL_SCENARIO (too_big_scenario, quiet_t,
                         (send<quiet_t::QUERY_t> ()),
                         (send<quiet_t::FILL_t> (300)));
LUMEX_PROTOCOL_SCENARIO (no_reply_row_scenario, quiet_t,
                         (send<quiet_t::NOTHING_t> ()),
                         (send<quiet_t::QUERY_t> ()));
LUMEX_PROTOCOL_SCENARIO (timed_row_scenario, quiet_t,
                         (send<quiet_t::TIMED_t> ()));
LUMEX_PROTOCOL_SCENARIO (dangerous_scenario, quiet_t,
                         (send<quiet_t::ERASE_t> (confirm, 3)));

LUMEX_PROTOCOL_SCENARIO (two_expects_scenario, quiet_t,
                         (expect<quiet_t::LEVEL_t> (200)),
                         (expect<quiet_t::LEVEL_t> (1000)));
LUMEX_PROTOCOL_SCENARIO (expect_then_for_ever_scenario, quiet_t,
                         (expect<quiet_t::LEVEL_t> (200)),
                         (expect<quiet_t::LEVEL_t> (0)));
LUMEX_PROTOCOL_SCENARIO (reply_kind_scenario, quiet_t,
                         (send<quiet_t::RETRIED_t> ()));
LUMEX_PROTOCOL_SCENARIO (wait_then_expect_scenario, quiet_t,
                         (wait_state (tank_state::draining, 200)),
                         (expect<quiet_t::LEVEL_t> (1000)));
LUMEX_PROTOCOL_SCENARIO (
    reply_row_scenario, quiet_t, (send<quiet_t::SET_VALUE_t> (4)),
    (branch_on_reply<quiet_t::CHANGED_t> ([] (std::uint8_t const &value)
                                            { return value == 4; }, "end")),
    (send<quiet_t::QUERY_t> ()), (label ("end")));

LUMEX_PROTOCOL_SCENARIO (null_jump_scenario, quiet_t, (jump (nullptr)));
LUMEX_PROTOCOL_SCENARIO (null_label_scenario, quiet_t, (label (nullptr)),
                         (jump ("somewhere")), (label ("somewhere")));

template <typename Scenario>
sequence_outcome_ptr
start (manual_bench_t<quiet_t> &bench)
{
  sequence_outcome_ptr const out = std::make_shared<sequence_outcome_t> ();
  sequence_record_t handler = { out };
  pr::async_run_scenario<Scenario> (*bench.engine, handler);
  return out;
}

TEST (LumexDialogueScenarioTest, Scenario_StepsInOrderAndBranch)
{
  log_capture_t const log;
  manual_bench_t<quiet_t> bench;
  bench.start ();
  sequence_outcome_ptr const out = start<wash_scenario> (bench);
  EXPECT_EQ (out->calls, 0);
  bench.settle ();
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x31 }));
  bench.deliver (bytes ({ 0x31, 0x01 }));
  // The expect: nothing is written until the frame of the row comes.
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x31 }));
  bench.deliver (bytes ({ 0x13, 0x01, 0x00 }));
  // The wait for the state.
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x31 }));
  bench.deliver (status_frame (0, tank_state::filling));
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x31 }));
  bench.deliver (status_frame (0, tank_state::draining));
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x31, 0x12 }));
  // The reply: the valve is open, so the branch skips the query.
  bench.deliver (status_frame (1, tank_state::draining));
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x31, 0x12, 0x32 }));
  EXPECT_EQ (out->calls, 0);
  bench.deliver (bytes ({ 0x32, 0x01 }));
  ASSERT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error);
  EXPECT_EQ (out->step, wash_scenario::make ().steps.size ());
  EXPECT_EQ (log_capture_t::count (log_level::info, "wash_scenario"), 2u);
}

TEST (LumexDialogueScenarioTest,
      Scenario_ASendOfAReplyRowWaitsForItAndABranchReadsIt)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  sequence_outcome_ptr const out = start<reply_row_scenario> (bench);
  bench.settle ();
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x3D }));
  // A frame of the row itself is no reply of a by_reply_key row.
  bench.deliver (bytes ({ 0x3C, 0x04 }));
  ASSERT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error);
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x3D }));
}

TEST (LumexDialogueScenarioTest, Scenario_ASendReadsTheReplyKindOfItsRow)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  sequence_outcome_ptr const acknowledged = start<reply_kind_scenario> (bench);
  bench.settle ();
  bench.deliver (bytes ({ 0x36, 0x05 }));
  ASSERT_EQ (acknowledged->calls, 1);
  EXPECT_FALSE (acknowledged->error);
  bench.advance (ms (50));
  // The row says retries (1): a NAK, the request again, a NAK.
  sequence_outcome_ptr const refused = start<reply_kind_scenario> (bench);
  bench.settle ();
  bench.deliver (bytes ({ 0x36, 0xFB, 0x00 }));
  bench.advance (ms (50));
  EXPECT_EQ (refused->calls, 0);
  bench.deliver (bytes ({ 0x36, 0xFB, 0x00 }));
  ASSERT_EQ (refused->calls, 1);
  EXPECT_EQ (refused->error, protocol_errc::nak);
  EXPECT_EQ (refused->step, 0u);
}

TEST (LumexDialogueScenarioTest, Scenario_ABranchNotTakenGoesToTheNextStep)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  sequence_outcome_ptr const out = start<wash_scenario> (bench);
  bench.settle ();
  bench.deliver (bytes ({ 0x31, 0x01 }));
  bench.deliver (bytes ({ 0x13, 0x01, 0x00 }));
  bench.deliver (status_frame (0, tank_state::draining));
  bench.deliver (status_frame (0, tank_state::draining));
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x31, 0x12, 0x33 }));
  bench.deliver (bytes ({ 0x33, 0x01 }));
  ASSERT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error);
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x31, 0x12, 0x33 }));
}

TEST (LumexDialogueScenarioTest, Scenario_FailsAtStepWithItsIndex)
{
  log_capture_t const log;
  manual_bench_t<quiet_t> bench;
  bench.options.defaults.timeout_ms = 100;
  bench.start ();
  sequence_outcome_ptr const out = start<two_sends_scenario> (bench);
  bench.settle ();
  bench.deliver (bytes ({ 0x33, 0x01 }));
  ASSERT_TRUE (bench.run_until ([&out] () { return out->calls != 0; }));
  // TIMED has retries (2) and a timeout of its own: 3 attempts of 77 ms.
  EXPECT_EQ (out->error, protocol_errc::retries_exhausted);
  EXPECT_EQ (out->step, 1u);
  EXPECT_EQ (bench.count (0x38), 3u);
  EXPECT_EQ (log_capture_t::count (log_level::warning, "two_sends_scenario"),
             1u);
}

TEST (LumexDialogueScenarioTest, Scenario_SendTakesTheTimeOfItsRowFirst)
{
  manual_bench_t<quiet_t> bench;
  bench.options.defaults.timeout_ms = 10000;
  bench.start ();
  sequence_outcome_ptr const out = start<timed_row_scenario> (bench);
  bench.settle ();
  bench.advance (ms (76));
  EXPECT_EQ (bench.count (0x38), 1u);
  bench.advance (ms (1));
  EXPECT_EQ (bench.count (0x38), 2u);
  EXPECT_EQ (out->calls, 0);
}

TEST (LumexDialogueScenarioTest, Scenario_ASendWithoutAReplyEndsWithItsWrite)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  sequence_outcome_ptr const out = start<no_reply_row_scenario> (bench);
  bench.settle ();
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x39, 0x33 }));
  bench.deliver (bytes ({ 0x33, 0x01 }));
  EXPECT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error);
}

TEST (LumexDialogueScenarioTest, Scenario_ADangerousRowIsLogged)
{
  log_capture_t const log;
  manual_bench_t<quiet_t> bench;
  bench.start ();
  start<dangerous_scenario> (bench);
  bench.settle ();
  EXPECT_EQ (log_capture_t::count (log_level::info, "a dangerous request"),
             1u);
}

TEST (LumexDialogueScenarioTest, Scenario_AnArgumentThatDoesNotFitFailsItsStep)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  sequence_outcome_ptr const out = start<too_big_scenario> (bench);
  bench.settle ();
  bench.deliver (bytes ({ 0x33, 0x01 }));
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, protocol_errc::invalid_value);
  EXPECT_EQ (out->step, 1u);
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x33 }));
}

TEST (LumexDialogueScenarioTest,
      Scenario_ALabelThatDoesNotExistRefusesTheScenarioAtTheStart)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  sequence_outcome_ptr const out = start<lost_label_scenario> (bench);
  bench.settle ();
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, std::make_error_code (std::errc::invalid_argument));
  EXPECT_EQ (out->step, 0u);
  EXPECT_TRUE (bench.link->written ().empty ());
}

TEST (LumexDialogueScenarioTest,
      Scenario_ABranchWithoutAReplyOfItsRowFailsItsStep)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  sequence_outcome_ptr const out = start<no_reply_yet_scenario> (bench);
  bench.settle ();
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, protocol_errc::unexpected_reply);
  EXPECT_EQ (out->step, 0u);
}

TEST (LumexDialogueScenarioTest, Scenario_ARowTheIndexHasNotFailsItsStep)
{
  // The scenario of another layer: the engine works on a table without it.
  manual_bench_t<quiet_t> bench;
  bench.start ();
  lumex::core::protocol::dsl::scenario_t made = two_sends_scenario::make ();
  made.steps[0].row = "NO_SUCH_ROW";
  sequence_outcome_ptr const out = std::make_shared<sequence_outcome_t> ();
  sequence_record_t handler = { out };
  bench.engine->async_run_scenario (std::move (made), bench.options.features,
                                    handler);
  bench.settle ();
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, protocol_errc::not_supported);
  EXPECT_EQ (out->step, 0u);
}

TEST (LumexDialogueScenarioTest,
      Scenario_ALoopOfBranchesDoesNotHoldTheStrandAndCanBeCancelled)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  asio::cancellation_signal signal;
  sequence_outcome_ptr const out = std::make_shared<sequence_outcome_t> ();
  sequence_record_t handler = { out };
  pr::async_run_scenario<loop_scenario> (
      *bench.engine, asio::bind_cancellation_slot (signal.slot (), handler));
  // Each poll_one runs one posted step: the others get their turn.
  int passes = 0;
  bench.context.restart ();
  for (; passes < 1000 && bench.context.poll_one () != 0; ++passes)
    {
    }
  EXPECT_EQ (passes, 1000);
  EXPECT_EQ (out->calls, 0);
  signal.emit (asio::cancellation_type::terminal);
  for (passes = 0; passes < 1000 && out->calls == 0; ++passes)
    bench.context.poll_one ();
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, transport_errc::cancelled);
  bench.settle ();
}

TEST (LumexDialogueScenarioTest,
      Scenario_ALoopOverAReplyEndsWithTheReplyThatEndsIt)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  sequence_outcome_ptr const out = start<poll_loop_scenario> (bench);
  bench.settle ();
  for (int i = 1; i <= 3; ++i)
    {
      EXPECT_EQ (bench.count (0x12), static_cast<std::size_t> (i));
      bench.deliver (status_frame (0));
    }
  EXPECT_EQ (out->calls, 0);
  bench.deliver (status_frame (1));
  ASSERT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error);
  EXPECT_EQ (bench.count (0x12), 4u);
}

// --- expect
// ------------------------------------------------------------------

TEST (LumexDialogueScenarioTest, Expect_TimesOutWithItsIndex)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  sequence_outcome_ptr const out = start<expect_scenario> (bench);
  bench.settle ();
  bench.advance (ms (199));
  EXPECT_EQ (out->calls, 0);
  bench.advance (ms (1));
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, protocol_errc::timeout);
  EXPECT_EQ (out->step, 0u);
}

TEST (LumexDialogueScenarioTest, Expect_ByDefaultTakesTheDefaultTime)
{
  manual_bench_t<quiet_t> bench;
  bench.options.defaults.timeout_ms = 321;
  bench.start ();
  sequence_outcome_ptr const out = start<expect_default_scenario> (bench);
  bench.settle ();
  bench.advance (ms (320));
  EXPECT_EQ (out->calls, 0);
  bench.advance (ms (1));
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, protocol_errc::timeout);
}

TEST (LumexDialogueScenarioTest, Expect_AFrameOfAnotherRowDoesNotEndIt)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  sequence_outcome_ptr const out = start<expect_scenario> (bench);
  bench.settle ();
  bench.deliver (status_frame (0));
  EXPECT_TRUE (bench.link->written ().empty ());
  EXPECT_EQ (out->calls, 0);
  bench.deliver (bytes ({ 0x13, 0x01, 0x00 }));
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x33 }));
}

TEST (LumexDialogueScenarioTest,
      Expect_TheFrameBehindOrBeforeTheReplyOfTheSendBeforeIt)
{
  {
    // Before the reply.
    manual_bench_t<quiet_t> bench;
    bench.start ();
    sequence_outcome_ptr const out = start<send_then_expect_scenario> (bench);
    bench.settle ();
    bench.deliver (bytes ({ 0x13, 0x01, 0x00 }));
    EXPECT_EQ (out->calls, 0);
    bench.deliver (bytes ({ 0x33, 0x01 }));
    ASSERT_EQ (out->calls, 1);
    EXPECT_FALSE (out->error);
  }
  {
    // In one run with the reply: both completions are on their way.
    manual_bench_t<quiet_t> bench;
    bench.start ();
    sequence_outcome_ptr const out = start<send_then_expect_scenario> (bench);
    bench.settle ();
    ASSERT_TRUE (bench.link->deliver (bytes ({ 0x33, 0x01 })));
    bench.context.poll_one ();
    ASSERT_TRUE (bench.link->deliver (bytes ({ 0x13, 0x01, 0x00 })));
    bench.settle ();
    ASSERT_EQ (out->calls, 1);
    EXPECT_FALSE (out->error);
  }
}

TEST (LumexDialogueScenarioTest, Expect_AFrameBeforeTheSendDoesNotCount)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  bench.deliver (bytes ({ 0x13, 0x01, 0x00 }));
  sequence_outcome_ptr const out = start<send_then_expect_scenario> (bench);
  bench.settle ();
  bench.deliver (bytes ({ 0x33, 0x01 }));
  EXPECT_EQ (out->calls, 0);
  bench.advance (ms (200));
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, protocol_errc::timeout);
  EXPECT_EQ (out->step, 1u);
}

TEST (LumexDialogueScenarioTest,
      Expect_ADueTimeoutRacingTheFrameLosesToTheFrame)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  sequence_outcome_ptr const out = start<expect_scenario> (bench);
  bench.settle ();
  // The frame's completion runs first; the expired wait right behind it must
  // not fail the step that follows.
  ASSERT_TRUE (bench.link->deliver (bytes ({ 0x13, 0x01, 0x00 })));
  bench.clock.advance (ms (200));
  bench.settle ();
  EXPECT_EQ (out->calls, 0);
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x33 }));
  bench.deliver (bytes ({ 0x33, 0x01 }));
  EXPECT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error);
}

TEST (LumexDialogueScenarioTest,
      Expect_ADueTimeoutOfTheStepBeforeEndsNothingOfTheNextWait)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  sequence_outcome_ptr const out = start<two_expects_scenario> (bench);
  bench.settle ();
  ASSERT_TRUE (bench.link->deliver (bytes ({ 0x13, 0x01, 0x00 })));
  bench.clock.advance (ms (200));
  bench.settle ();
  EXPECT_EQ (out->calls, 0);
  bench.advance (ms (999));
  EXPECT_EQ (out->calls, 0);
  bench.advance (ms (1));
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, protocol_errc::timeout);
  EXPECT_EQ (out->step, 1u);
}

TEST (LumexDialogueScenarioTest,
      Expect_ADueTimeoutOfTheStepBeforeEndsNothingOfAWaitForEver)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  sequence_outcome_ptr const out
      = start<expect_then_for_ever_scenario> (bench);
  bench.settle ();
  ASSERT_TRUE (bench.link->deliver (bytes ({ 0x13, 0x01, 0x00 })));
  bench.clock.advance (ms (200));
  bench.settle ();
  bench.advance (ms (100000));
  EXPECT_EQ (out->calls, 0);
}

TEST (LumexDialogueScenarioTest,
      Expect_AFrameThatIsTakenIsNoUnexpectedReplyOfANonStreamRow)
{
  // CLOCK is no stream row: the frame answers the step.
  manual_bench_t<quiet_t> bench;
  bench.start ();
  int seen = 0;
  bench.engine->subscribe (ft::row_of<quiet_t, quiet_t::CLOCK_t> (),
                           [&seen] (ft::bytes_t const &) { ++seen; });
  sequence_outcome_ptr const out = start<expect_default_scenario> (bench);
  bench.settle ();
  bench.deliver (bytes ({ 0x11 }));
  ASSERT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error);
  EXPECT_EQ (bench.engine->statistics ().unexpected_replies, 0u);
  EXPECT_EQ (bench.engine->statistics ().replies, 1u);
  EXPECT_EQ (seen, 0);
}

TEST (LumexDialogueScenarioTest, Expect_AnEventStillReachesItsSubscribers)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  int seen = 0;
  bench.engine->subscribe (ft::row_of<quiet_t, quiet_t::LEVEL_t> (),
                           [&seen] (ft::bytes_t const &) { ++seen; });
  start<expect_scenario> (bench);
  bench.settle ();
  bench.deliver (bytes ({ 0x13, 0x01, 0x00 }));
  EXPECT_EQ (seen, 1);
  EXPECT_EQ (bench.engine->statistics ().events, 1u);
}

// --- wait_state
// --------------------------------------------------------------

TEST (LumexDialogueScenarioTest, WaitState_AlreadyThereGoesOnAtOnce)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  bench.engine->set_state (static_cast<int> (tank_state::draining));
  start<wait_scenario> (bench);
  bench.settle ();
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x33 }));
}

TEST (LumexDialogueScenarioTest, WaitState_SetStateWakesIt)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  sequence_outcome_ptr const out = start<wait_scenario> (bench);
  bench.settle ();
  bench.engine->set_state (static_cast<int> (tank_state::filling));
  bench.settle ();
  EXPECT_TRUE (bench.link->written ().empty ());
  bench.engine->set_state (static_cast<int> (tank_state::draining));
  bench.settle ();
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x33 }));
  EXPECT_EQ (out->calls, 0);
}

TEST (LumexDialogueScenarioTest, WaitState_ATransientStateSetByHandIsNotMissed)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  start<wait_scenario> (bench);
  bench.settle ();
  bench.engine->set_state (static_cast<int> (tank_state::draining));
  bench.engine->set_state (static_cast<int> (tank_state::idle));
  bench.settle ();
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x33 }));
}

TEST (LumexDialogueScenarioTest,
      WaitState_ATransientStateOfTheFramesIsNotMissed)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  start<wait_scenario> (bench);
  bench.settle ();
  bench.deliver (status_frame (0, tank_state::draining));
  bench.deliver (status_frame (0, tank_state::idle));
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x33 }));
}

TEST (LumexDialogueScenarioTest, WaitState_TimesOutWithItsIndex)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  sequence_outcome_ptr const out = start<wait_scenario> (bench);
  bench.settle ();
  bench.advance (ms (299));
  EXPECT_EQ (out->calls, 0);
  bench.advance (ms (1));
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, protocol_errc::timeout);
  EXPECT_EQ (out->step, 0u);
}

TEST (LumexDialogueScenarioTest,
      WaitState_ADueTimeoutRacingTheStateLosesToTheState)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  sequence_outcome_ptr const out = start<wait_scenario> (bench);
  bench.settle ();
  ASSERT_TRUE (bench.link->deliver (status_frame (0, tank_state::draining)));
  bench.clock.advance (ms (300));
  bench.settle ();
  EXPECT_EQ (out->calls, 0);
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x33 }));
}

TEST (LumexDialogueScenarioTest, WaitState_ADueTimeoutEndsNothingOfTheNextWait)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  sequence_outcome_ptr const out = start<wait_then_expect_scenario> (bench);
  bench.settle ();
  ASSERT_TRUE (bench.link->deliver (status_frame (0, tank_state::draining)));
  bench.clock.advance (ms (200));
  bench.settle ();
  EXPECT_EQ (out->calls, 0);
  bench.advance (ms (999));
  EXPECT_EQ (out->calls, 0);
  bench.advance (ms (1));
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->step, 1u);
}

TEST (LumexDialogueScenarioTest, WaitState_ZeroWaitsForEver)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  sequence_outcome_ptr const out = start<wait_for_ever_scenario> (bench);
  bench.settle ();
  bench.advance (ms (100000));
  EXPECT_EQ (out->calls, 0);
  bench.engine->set_state (static_cast<int> (tank_state::fault));
  bench.settle ();
  ASSERT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error);
}

// --- the end of a run
// --------------------------------------------------------

TEST (LumexDialogueScenarioTest,
      Lifetime_CancelEndsARunWithItsStepAndTheEngineGoesOn)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  sequence_outcome_ptr const sending = start<two_sends_scenario> (bench);
  sequence_outcome_ptr const waiting = start<wait_for_ever_scenario> (bench);
  bench.settle ();
  bench.engine->cancel ();
  bench.settle ();
  ASSERT_EQ (sending->calls, 1);
  EXPECT_EQ (sending->error, transport_errc::cancelled);
  EXPECT_EQ (sending->step, 0u);
  ASSERT_EQ (waiting->calls, 1);
  EXPECT_EQ (waiting->error, transport_errc::cancelled);
  sequence_outcome_ptr const next = start<two_sends_scenario> (bench);
  bench.settle ();
  bench.deliver (bytes ({ 0x33, 0x01 }));
  EXPECT_EQ (next->calls, 0);
  EXPECT_EQ (bench.count (0x38), 1u);
}

TEST (LumexDialogueScenarioTest, Lifetime_CloseEndsARunAndALaterOneIsRefused)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  sequence_outcome_ptr const running = start<expect_scenario> (bench);
  bench.settle ();
  bench.engine->close ();
  bench.settle ();
  ASSERT_EQ (running->calls, 1);
  EXPECT_EQ (running->error, transport_errc::cancelled);
  sequence_outcome_ptr const later = start<two_sends_scenario> (bench);
  // Nor does a run that would only wait start.
  sequence_outcome_ptr const waits = start<expect_scenario> (bench);
  bench.settle ();
  ASSERT_EQ (later->calls, 1);
  EXPECT_EQ (later->error, transport_errc::closed);
  ASSERT_EQ (waits->calls, 1);
  EXPECT_EQ (waits->error, transport_errc::closed);
}

TEST (LumexDialogueScenarioTest,
      Lifetime_DestroyingTheEngineEndsARunExactlyOnce)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  sequence_outcome_ptr const out = start<wait_scenario> (bench);
  bench.settle ();
  bench.stop ();
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, transport_errc::cancelled);
  bench.clock.advance (ms (1000));
  bench.settle ();
  EXPECT_EQ (out->calls, 1);
}

TEST (LumexDialogueScenarioTest, Lifetime_ACompletionMayDestroyTheEngine)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  bool called = false;
  pr::async_run_scenario<no_reply_yet_scenario> (
      *bench.engine,
      [&] (std::error_code, std::size_t)
        {
          called = true;
          bench.engine.reset ();
        });
  bench.settle ();
  EXPECT_TRUE (called);
  EXPECT_EQ (bench.engine, nullptr);
  bench.settle ();
}

TEST (LumexDialogueScenarioTest,
      Lifetime_APredicateThatThrowsLeavesTheRunAndEndsItsScenario)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  sequence_outcome_ptr const out = start<throwing_scenario> (bench);
  bench.settle ();
  ASSERT_TRUE (bench.link->deliver (status_frame (0)));
  bool thrown = false;
  for (int i = 0; i < 20 && !thrown; ++i)
    try
      {
        bench.context.poll ();
        bench.context.restart ();
      }
    catch (std::runtime_error const &)
      {
        thrown = true;
      }
  EXPECT_TRUE (thrown);
  bench.settle ();
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, std::make_error_code (std::errc::interrupted));
  EXPECT_EQ (out->step, 1u);
  EXPECT_EQ (bench.keys (), (std::vector<int>{ 0x12 }));
}

TEST (LumexDialogueScenarioTest,
      Lifetime_ACancelSignalAfterTheEndReachesNothing)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  asio::cancellation_signal signal;
  sequence_outcome_ptr const out = std::make_shared<sequence_outcome_t> ();
  sequence_record_t handler = { out };
  pr::async_run_scenario<two_sends_scenario> (
      *bench.engine, asio::bind_cancellation_slot (signal.slot (), handler));
  bench.settle ();
  bench.deliver (bytes ({ 0x33, 0x01 }));
  bench.deliver (bytes ({ 0x38, 0x01 }));
  ASSERT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error);
  signal.emit (asio::cancellation_type::terminal);
  bench.settle ();
  EXPECT_EQ (out->calls, 1);
  EXPECT_FALSE (out->error);
}

TEST (LumexDialogueScenarioTest, Lifetime_ATotalCancellationIsNotAnEndOfARun)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  asio::cancellation_signal signal;
  sequence_outcome_ptr const out = std::make_shared<sequence_outcome_t> ();
  sequence_record_t handler = { out };
  pr::async_run_scenario<wait_for_ever_scenario> (
      *bench.engine, asio::bind_cancellation_slot (signal.slot (), handler));
  bench.settle ();
  signal.emit (asio::cancellation_type::total);
  bench.settle ();
  EXPECT_EQ (out->calls, 0);
  signal.emit (asio::cancellation_type::partial);
  bench.settle ();
  EXPECT_EQ (out->calls, 1);
}

TEST (
    LumexDialogueScenarioTest,
    Lifetime_AStepOwnedPredicateAndHandlerThatOwnTheEngineDieWithTheIoContext)
{
  struct owner_t
  {
    std::unique_ptr<pr::dialogue_engine> engine;
  };
  std::weak_ptr<owner_t> watch;
  std::shared_ptr<int> calls = std::make_shared<int> (0);
  {
    asio::io_context context;
    tr::testing::virtual_clock clock;
    std::shared_ptr<ft::manual_link_t> const link
        = std::make_shared<ft::manual_link_t> (context);
    pr::dialogue_options_t options = ft::dialogue::options_for<quiet_t> ();
    options.timers = clock.source ();
    std::shared_ptr<owner_t> owner = std::make_shared<owner_t> ();
    owner->engine.reset (new pr::dialogue_engine (
        pr::frame_link (link), *ft::index::protocol_index<quiet_t> (),
        options));
    // The steps keep the owner as well as the handler.
    lumex::core::protocol::dsl::scenario_t made
        = wait_for_ever_scenario::make ();
    lumex::core::protocol::dsl::scenario_step_t kept;
    kept.kind = lumex::core::protocol::dsl::scenario_step_kind::send;
    kept.row = "QUERY";
    kept.encode
        = [owner] (ft::layout::bytes_t &, field::decode_context_t const &)
      { return std::error_code (); };
    made.steps.push_back (kept);
    kept.encode = nullptr;
    owner->engine->async_run_scenario (
        std::move (made), options.features,
        [owner, calls] (std::error_code, std::size_t) { ++*calls; });
    watch = owner;
    context.poll ();
    owner.reset ();
    EXPECT_FALSE (watch.expired ());
  }
  EXPECT_TRUE (watch.expired ());
  EXPECT_EQ (*calls, 0);
}

TEST (LumexDialogueScenarioTest, Lifetime_ATimerThatCannotBeArmedFailsTheStep)
{
  manual_bench_t<quiet_t> bench;
  std::shared_ptr<ft::flaky_timers_t> const timers
      = std::make_shared<ft::flaky_timers_t> (bench.clock.source ());
  bench.options.timers = tr::timing::timer_source (timers);
  bench.start ();
  timers->set_throwing (true);
  sequence_outcome_ptr const out = start<expect_scenario> (bench);
  bench.settle ();
  timers->set_throwing (false);
  ASSERT_EQ (out->calls, 1);
  EXPECT_EQ (out->error, std::make_error_code (std::errc::not_enough_memory));
  EXPECT_EQ (out->step, 0u);
}

// --- the tokens
// --------------------------------------------------------------

TEST (LumexDialogueScenarioTest, Token_UseFutureAndDeferred)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  std::future<std::size_t> finished
      = pr::async_run_scenario<no_reply_row_scenario> (*bench.engine,
                                                       asio::use_future);
  bench.settle ();
  bench.deliver (bytes ({ 0x33, 0x01 }));
  ASSERT_EQ (finished.wait_for (std::chrono::seconds (0)),
             std::future_status::ready);
  EXPECT_EQ (finished.get (), 2u);

  auto later = pr::async_run_scenario<no_reply_row_scenario> (*bench.engine,
                                                              asio::deferred);
  bench.settle ();
  EXPECT_EQ (bench.count (0x39), 1u);
  sequence_outcome_ptr const out = std::make_shared<sequence_outcome_t> ();
  sequence_record_t handler = { out };
  std::move (later) (handler);
  bench.settle ();
  EXPECT_EQ (bench.count (0x39), 2u);
  bench.deliver (bytes ({ 0x33, 0x01 }));
  EXPECT_EQ (out->calls, 1);
}

TEST (LumexDialogueScenarioTest, Token_AMovedFromEngineAnswersNotOpen)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  pr::dialogue_engine moved (std::move (*bench.engine));
  std::future<std::size_t> refused
      = pr::async_run_scenario<no_reply_row_scenario> (*bench.engine,
                                                       asio::use_future);
  try
    {
      refused.get ();
      ADD_FAILURE () << "a moved-from engine ran a scenario";
    }
  catch (std::system_error const &error)
    {
      EXPECT_EQ (error.code (), transport_errc::not_open);
    }
}
TEST (LumexDialogueScenarioTest,
      Scenario_ANullLabelRefusesTheScenarioAtTheStart)
{
  manual_bench_t<quiet_t> bench;
  bench.start ();
  sequence_outcome_ptr const jump_out = start<null_jump_scenario> (bench);
  sequence_outcome_ptr const label_out = start<null_label_scenario> (bench);
  bench.settle ();
  ASSERT_EQ (jump_out->calls, 1);
  EXPECT_EQ (jump_out->error,
             std::make_error_code (std::errc::invalid_argument));
  EXPECT_EQ (jump_out->step, 0u);
  ASSERT_EQ (label_out->calls, 1);
  EXPECT_EQ (label_out->error,
             std::make_error_code (std::errc::invalid_argument));
}

} // namespace
