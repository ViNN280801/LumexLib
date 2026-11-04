// The device simulator playing a scenario written for the client
// (`device.play<Scenario> ()`) against the dialogue engine that runs the same
// scenario (`async_run_scenario`), on the lab table and the virtual clock:
// every kind of step and both ways of a branch, the position and the end,
// what other requests and the behaviours do to the script, and the edges (a
// predicate that throws, a loop that never waits, a second play, a scenario
// that fails its check, the life of the simulator).

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/testing/simulator/LumexSimulatorRawBench.hpp"
#include "lumex/tests/applied/transport/testing/simulator/LumexSimulatorTestSupport.hpp"
#include "lumex/tests/applied/transport/testing/simulator/LumexSimulatorTestTables.hpp"

namespace
{
namespace st = lumex_simulator_test;
namespace tables = lumex_tests::simulator_tables;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace tr = st::tr;
using lab = tables::lab_t;
using bench_t = st::bench_t<lab>;
using st::bytes_t;
using st::ms;
using st::protocol_errc;
using tables::bytes;
using tr::testing::playback_state;
using device_t = tr::testing::device_simulator<lab>;

bool
level_is_high (std::uint8_t const &level)
{
  return level == 7;
}

bool
throws_always (std::uint8_t const &)
{
  throw std::runtime_error ("predicate");
}

std::error_code
code (protocol_errc value)
{
  return lumex::core::protocol::error::make_error_code (value);
}

// Every kind of step: a send, an expect, a wait for a state, labels, a
// branch on a reply both ways and a jump.
LUMEX_PROTOCOL_SCENARIO (full_script, lab, (send<lab::INFO_t> ()), // 0
                         (expect<lab::STATE_t> ()),                // 1
                         (wait_state (tables::lab_state::busy)),   // 2
                         (label ("poll")),                         // 3
                         (send<lab::LEVEL_t> ()),                  // 4
                         (branch_on_reply<lab::LEVEL_t> (&level_is_high,
                                                         "high")), // 5
                         (send<lab::STOP_t> ()),                   // 6
                         (jump ("end")),                           // 7
                         (label ("high")),                         // 8
                         (send<lab::RESET_t> ()),                  // 9
                         (label ("end")));                         // 10
std::size_t const kFullSteps = 11;

LUMEX_PROTOCOL_SCENARIO (info_then_state_script, lab, (send<lab::INFO_t> ()),
                         (expect<lab::STATE_t> ()));
LUMEX_PROTOCOL_SCENARIO (move_then_level_script, lab, (send<lab::MOVE_t> (1)),
                         (expect<lab::LEVEL_t> ()));
LUMEX_PROTOCOL_SCENARIO (info_stop_script, lab, (send<lab::INFO_t> ()),
                         (send<lab::STOP_t> ()));
LUMEX_PROTOCOL_SCENARIO (info_state_stop_level_script, lab,
                         (send<lab::INFO_t> ()), (expect<lab::STATE_t> ()),
                         (send<lab::STOP_t> ()), (expect<lab::LEVEL_t> ()));
LUMEX_PROTOCOL_SCENARIO (two_expects_script, lab, (send<lab::INFO_t> ()),
                         (expect<lab::STATE_t> ()), (expect<lab::LEVEL_t> ()),
                         (send<lab::STOP_t> ()));
LUMEX_PROTOCOL_SCENARIO (
    write_script, lab, (send<lab::WRITE_t> (1, 2)),
    (branch_on_reply<lab::CHANGE_t> (
        [] (layout::reply_message_of<lab::CHANGE_t>::type const &change)
          { return field::get<1> (change) == 5; }, "end")),
    (send<lab::INFO_t> ()), (label ("end")));
LUMEX_PROTOCOL_SCENARIO (
    param_then_level_script, lab,
    (send<lab::PARAM_GET_t> (tables::lab_registry::FREQUENCY)),
    (expect<lab::LEVEL_t> ()));
LUMEX_PROTOCOL_SCENARIO (info_script, lab, (send<lab::INFO_t> ()));
LUMEX_PROTOCOL_SCENARIO (
    level_nine_script, lab, (expect<lab::LEVEL_t> ()),
    (branch_on_reply<lab::LEVEL_t> ([] (std::uint8_t const &level)
                                      { return level == 9; }, "end")),
    (send<lab::INFO_t> ()), (label ("end")));
LUMEX_PROTOCOL_SCENARIO (level_expect_script, lab, (expect<lab::LEVEL_t> ()));
LUMEX_PROTOCOL_SCENARIO (throwing_script, lab, (send<lab::LEVEL_t> ()),
                         (branch_on_reply<lab::LEVEL_t> (&throws_always,
                                                         "end")),
                         (send<lab::STOP_t> ()), (label ("end")));
LUMEX_PROTOCOL_SCENARIO (loop_script, lab, (label ("again")),
                         (jump ("again")));
LUMEX_PROTOCOL_SCENARIO (flood_script, lab, (label ("again")),
                         (expect<lab::LEVEL_t> ()), (jump ("again")));
LUMEX_PROTOCOL_SCENARIO (lost_label_script, lab, (jump ("nowhere")),
                         (send<lab::INFO_t> ()));
LUMEX_PROTOCOL_SCENARIO (branch_first_script, lab,
                         (branch_on_reply<lab::LEVEL_t> (&level_is_high,
                                                         "end")),
                         (label ("end")));
LUMEX_PROTOCOL_SCENARIO (outside_script, lab, (expect<lab::NEWER_t> ()),
                         (send<lab::INFO_t> ()));
LUMEX_PROTOCOL_SCENARIO (
    counting_loop_script, lab, (label ("poll")), (send<lab::LEVEL_t> ()),
    (branch_on_reply<lab::LEVEL_t> ([] (std::uint8_t const &level)
                                      { return level != 3; }, "poll")),
    (send<lab::INFO_t> ()));

// A second protocol: its scenario is not for a device of the lab table.
struct other_codec_t
{
};

struct other_header_t
{
  using lumex_vocabulary = st::dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
};

#define OTHER_ROWS(X, X_AS) X (PING, 0x01, "Ping", (), (), (u8_), ())
LUMEX_PROTOCOL_DEFINE (other_t, other_codec_t, other_header_t, OTHER_ROWS);
LUMEX_PROTOCOL_SCENARIO (other_script, other_t, (send<other_t::PING_t> ()));

// Whether `device.play<Scenario> ()` compiles.
template <typename Device, typename Scenario>
auto can_play (int)
    -> decltype (std::declval<Device &> ().template play<Scenario> (),
                 std::true_type ());

template <typename Device, typename Scenario> std::false_type can_play (long);

static_assert (decltype (can_play<device_t, full_script> (0))::value,
               "a scenario of the layer of the device plays");
static_assert (!decltype (can_play<device_t, other_script> (0))::value,
               "a scenario of another protocol does not compile");

// The engine reads the table's features (the state row) with these options.
tr::protocol::dialogue_options_t
engine_options ()
{
  return tr::protocol::dialogue::options_for<lab> ();
}

// What the client's run ended with.
struct run_outcome_t
{
  int calls = 0;
  std::error_code error;
  std::size_t step = 0;
};

struct run_record_t
{
  std::shared_ptr<run_outcome_t> out;

  void
  operator() (std::error_code error, std::size_t step)
  {
    ++out->calls;
    out->error = error;
    out->step = step;
  }
};

// The client's run of `Scenario` on the engine of the bench.
template <typename Scenario>
std::shared_ptr<run_outcome_t>
start_client (bench_t &bench)
{
  std::shared_ptr<run_outcome_t> const out
      = std::make_shared<run_outcome_t> ();
  run_record_t handler = { out };
  tr::protocol::async_run_scenario<Scenario> (*bench.engine, handler);
  return out;
}

tables::lab_state
state_of (bytes_t const &frame)
{
  auto const value
      = layout::decode_reply_message<lab, lab::STATE_t> (st::view_of (frame));
  EXPECT_TRUE (value.has_value ());
  return value ? field::get<0> (*value) : tables::lab_state::fault;
}

// Runs `Scenario` on the client and on the device until the client ended.
template <typename Scenario>
std::shared_ptr<run_outcome_t>
run_both (bench_t &bench)
{
  std::shared_ptr<run_outcome_t> const client = start_client<Scenario> (bench);
  EXPECT_TRUE (bench.device->template play<Scenario> ());
  EXPECT_TRUE (bench.run_until ([&] () { return client->calls != 0; }));
  bench.settle ();
  return client;
}

void
expect_playback (bench_t &bench, playback_state state, std::size_t step,
                 std::size_t steps, std::error_code error = std::error_code ())
{
  tr::testing::playback_status_t const status = bench.device->playback ();
  EXPECT_EQ (status.state, state);
  EXPECT_EQ (status.step, step);
  EXPECT_EQ (status.steps, steps);
  EXPECT_EQ (status.error, error);
}

namespace dsl = lumex::core::protocol::dsl;

// Scenarios written by hand, as a table-less description: the device reads
// only the kinds, rows and labels, and calls `decide`.
dsl::scenario_step_t
step_of (dsl::scenario_step_kind kind, char const *row, char const *label)
{
  dsl::scenario_step_t step;
  step.kind = kind;
  step.row = row;
  step.label = label;
  return step;
}

std::shared_ptr<device_t> holding_device;

// Waits at a send; its decision holds a device (it never runs).
struct holding_script_t
{
  using protocol_type = lab;

  static dsl::scenario_t
  make ()
  {
    dsl::scenario_t out;
    out.name = "holding";
    out.steps.push_back (
        step_of (dsl::scenario_step_kind::send, "LEVEL", nullptr));
    dsl::scenario_step_t branch
        = step_of (dsl::scenario_step_kind::branch, "LEVEL", "end");
    std::shared_ptr<device_t> const held = holding_device;
    branch.decide = [held] (layout::bytes_view_t,
                            field::decode_context_t const &, bool &taken)
      {
        taken = false;
        return std::error_code ();
      };
    out.steps.push_back (branch);
    out.steps.push_back (
        step_of (dsl::scenario_step_kind::label, nullptr, "end"));
    return out;
  }
};

// A decision that fails; `Taken` is what it leaves in `taken`.
template <bool Taken> struct failing_decision_script_t
{
  using protocol_type = lab;

  static dsl::scenario_t
  make ()
  {
    dsl::scenario_t out;
    out.name = "failing";
    out.steps.push_back (
        step_of (dsl::scenario_step_kind::send, "LEVEL", nullptr));
    dsl::scenario_step_t branch
        = step_of (dsl::scenario_step_kind::branch, "LEVEL", "end");
    branch.decide =
        [] (layout::bytes_view_t, field::decode_context_t const &, bool &taken)
      {
        taken = Taken;
        return std::make_error_code (std::errc::io_error);
      };
    out.steps.push_back (branch);
    out.steps.push_back (
        step_of (dsl::scenario_step_kind::label, nullptr, "end"));
    return out;
  }
};

// A predicate that is true for the first `turns_wanted` calls.
int turns_made = 0;
int turns_wanted = 0;

bool
turns_left (std::uint8_t const &)
{
  return ++turns_made <= turns_wanted;
}

LUMEX_PROTOCOL_SCENARIO (cap_script, lab, (send<lab::LEVEL_t> ()),
                         (label ("top")),
                         (branch_on_reply<lab::LEVEL_t> (&turns_left, "top")));
// A forward and a back jump in each turn.
LUMEX_PROTOCOL_SCENARIO (forward_back_script, lab, (send<lab::LEVEL_t> ()),
                         (label ("top")), (jump ("fwd")), (label ("fwd")),
                         (branch_on_reply<lab::LEVEL_t> (&turns_left, "top")));
LUMEX_PROTOCOL_SCENARIO (two_labels_loop_script, lab, (label ("a")),
                         (jump ("b")), (label ("b")), (jump ("a")));

// The state row after a wait for a state: the client never keeps the frame.
bool
is_busy (layout::reply_message_of<lab::STATE_t>::type const &status)
{
  return field::get<0> (status) == tables::lab_state::busy;
}

LUMEX_PROTOCOL_SCENARIO (state_branch_script, lab, (expect<lab::STATE_t> ()),
                         (wait_state (tables::lab_state::busy)),
                         (branch_on_reply<lab::STATE_t> (&is_busy, "high")),
                         (send<lab::STOP_t> ()), (jump ("end")),
                         (label ("high")), (send<lab::RESET_t> ()),
                         (label ("end")));
LUMEX_PROTOCOL_SCENARIO (state_branch_no_expect_script, lab,
                         (send<lab::INFO_t> ()),
                         (wait_state (tables::lab_state::busy)),
                         (branch_on_reply<lab::STATE_t> (&is_busy, "end")),
                         (label ("end")));
LUMEX_PROTOCOL_SCENARIO (twice_waiting_script, lab,
                         (wait_state (tables::lab_state::busy)),
                         (wait_state (tables::lab_state::busy)),
                         (expect<lab::STATE_t> ()),
                         (branch_on_reply<lab::STATE_t> (&is_busy, "high")),
                         (send<lab::STOP_t> ()), (jump ("end")),
                         (label ("high")), (send<lab::RESET_t> ()),
                         (label ("end")));
LUMEX_PROTOCOL_SCENARIO (wait_busy_script, lab,
                         (wait_state (tables::lab_state::busy)));
LUMEX_PROTOCOL_SCENARIO (expect_reset_script, lab, (expect<lab::RESET_t> ()));
LUMEX_PROTOCOL_SCENARIO (expect_ack_script, lab, (expect<lab::ACK_t> ()));
LUMEX_PROTOCOL_SCENARIO (expect_write_script, lab, (expect<lab::WRITE_t> ()));
LUMEX_PROTOCOL_SCENARIO (expect_prime_script, lab, (expect<lab::PRIME_t> ()));
LUMEX_PROTOCOL_SCENARIO (expect_report_script, lab,
                         (expect<lab::PARAM_REPORT_t> ()));
LUMEX_PROTOCOL_SCENARIO (send_stop_script, lab, (send<lab::STOP_t> ()));

std::unique_ptr<bench_t>
make_bench (bool datagram)
{
  if (datagram)
    return std::unique_ptr<bench_t> (
        new bench_t (std::uint8_t (5), tr::testing::simulator_options_t (),
                     engine_options ()));
  return std::unique_ptr<bench_t> (
      new bench_t ((tr::testing::simulator_options_t ()), engine_options ()));
}

} // namespace

TEST (LumexDeviceSimulatorScenario, Simulator_ScenarioPlaybackAgainstTheEngine)
{
  bench_t bench ((tr::testing::simulator_options_t ()), engine_options ());
  std::vector<bytes_t> states;
  bench.subscribe<lab::STATE_t> (states);
  expect_playback (bench, playback_state::idle, 0, 0);
  // Two runs: the reply of LEVEL decides the branch.
  for (int pass = 0; pass < 2; ++pass)
    {
      bool const high = pass == 1;
      std::size_t const before = states.size ();
      bench.device->set_state (0);
      bench.device->reset_statistics ();
      bench.device->set_reply<lab::LEVEL_t> (high ? 7 : 1);
      std::shared_ptr<run_outcome_t> const client
          = run_both<full_script> (bench);
      ASSERT_EQ (client->calls, 1);
      EXPECT_FALSE (client->error) << client->error.message ();
      EXPECT_EQ (client->step, kFullSteps);
      expect_playback (bench, playback_state::finished, kFullSteps,
                       kFullSteps);
      tr::testing::simulator_statistics_t const counted
          = bench.device->statistics ();
      EXPECT_EQ (counted.requests_of ("INFO"), 1u);
      EXPECT_EQ (counted.requests_of ("LEVEL"), 1u);
      EXPECT_EQ (counted.requests_of ("STOP"), high ? 0u : 1u);
      EXPECT_EQ (counted.requests_of ("RESET"), high ? 1u : 0u);
      // The expect and the wait for a state: two messages of the state row,
      // the device's state set by the second.
      EXPECT_EQ (counted.scripted_messages, 2u);
      EXPECT_EQ (bench.device->state (), 1);
      ASSERT_EQ (states.size (), before + 2);
      EXPECT_EQ (state_of (states[before]), tables::lab_state::idle);
      EXPECT_EQ (state_of (states[before + 1]), tables::lab_state::busy);
    }
}

TEST (LumexDeviceSimulatorScenario, Simulator_ScenarioPlaybackOverDatagrams)
{
  bench_t bench (std::uint8_t (5), tr::testing::simulator_options_t (),
                 engine_options ());
  bench.device->set_reply<lab::LEVEL_t> (7);
  std::shared_ptr<run_outcome_t> const client = run_both<full_script> (bench);
  ASSERT_EQ (client->calls, 1);
  EXPECT_FALSE (client->error) << client->error.message ();
  EXPECT_EQ (client->step, kFullSteps);
  expect_playback (bench, playback_state::finished, kFullSteps, kFullSteps);
  EXPECT_EQ (bench.device->statistics ().requests_of ("RESET"), 1u);
}

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioExpectSendsTheConfiguredReply)
{
  bench_t bench;
  bench.device->set_reply<lab::LEVEL_t> (9);
  // The client branches on the value of the message: 9 skips the request.
  std::shared_ptr<run_outcome_t> const client
      = run_both<level_nine_script> (bench);
  EXPECT_FALSE (client->error) << client->error.message ();
  expect_playback (bench, playback_state::finished, 4, 4);
  EXPECT_EQ (bench.device->statistics ().scripted_messages, 1u);
  EXPECT_EQ (bench.device->statistics ().requests_of ("INFO"), 0u);
  // A handler replaces the value, and a handler is for requests: it does not
  // run for a message by itself, and the sample is sent, so the branch is
  // not taken.
  int handled = 0;
  bench.device->on<lab::LEVEL_t> (
      [&handled] (layout::request_of<lab::LEVEL_t>::type const &)
        {
          ++handled;
          return std::uint8_t (9);
        });
  std::shared_ptr<run_outcome_t> const again
      = run_both<level_nine_script> (bench);
  EXPECT_FALSE (again->error) << again->error.message ();
  EXPECT_EQ (handled, 0);
  expect_playback (bench, playback_state::finished, 4, 4);
  EXPECT_EQ (bench.device->statistics ().requests_of ("INFO"), 1u);
  // So does clearing the value.
  bench.device->set_reply<lab::LEVEL_t> (9);
  bench.device->clear_reply<lab::LEVEL_t> ();
  std::shared_ptr<run_outcome_t> const cleared
      = run_both<level_nine_script> (bench);
  EXPECT_FALSE (cleared->error) << cleared->error.message ();
  expect_playback (bench, playback_state::finished, 4, 4);
  EXPECT_EQ (bench.device->statistics ().requests_of ("INFO"), 2u);
}

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioBranchOnTheRowThatAnswered)
{
  bench_t bench ((tr::testing::simulator_options_t ()), engine_options ());
  // WRITE is answered by a message of CHANGE: the branch reads that row.
  layout::reply_message_of<lab::CHANGE_t>::type changed
      = layout::reply_message_of<lab::CHANGE_t>::type ();
  field::get<1> (changed) = 5;
  bench.device->set_reply<lab::WRITE_t> (changed);
  std::shared_ptr<run_outcome_t> const client = run_both<write_script> (bench);
  EXPECT_FALSE (client->error) << client->error.message ();
  expect_playback (bench, playback_state::finished, 4, 4);
  EXPECT_EQ (bench.device->statistics ().requests_of ("INFO"), 0u);
  // The sample has 0 in it: the branch is not taken.
  bench.device->clear_reply<lab::WRITE_t> ();
  std::shared_ptr<run_outcome_t> const again = run_both<write_script> (bench);
  EXPECT_FALSE (again->error) << again->error.message ();
  expect_playback (bench, playback_state::finished, 4, 4);
  EXPECT_EQ (bench.device->statistics ().requests_of ("INFO"), 1u);
}

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioMalformedRequestDoesNotAdvance)
{
  bench_t bench;
  ASSERT_TRUE (bench.device->play<move_then_level_script> ());
  // MOVE with no target: the row is known, the request does not decode.
  bench.send_payload<lab::RESET_t> (bytes ({ 0x21 }));
  EXPECT_EQ (bench.device->statistics ().malformed_requests, 1u);
  expect_playback (bench, playback_state::playing, 0, 2);
  EXPECT_FALSE (bench.call<lab::MOVE_t> (std::uint8_t (1))->error);
  bench.settle ();
  expect_playback (bench, playback_state::finished, 2, 2);
}

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioTwoExpectsInARowReachTheClient)
{
  bench_t bench ((tr::testing::simulator_options_t ()), engine_options ());
  std::shared_ptr<run_outcome_t> const client
      = run_both<two_expects_script> (bench);
  ASSERT_EQ (client->calls, 1);
  EXPECT_FALSE (client->error) << client->error.message ();
  EXPECT_EQ (client->step, 4u);
  expect_playback (bench, playback_state::finished, 4, 4);
  EXPECT_EQ (bench.device->statistics ().scripted_messages, 2u);
}

TEST (LumexDeviceSimulatorScenario, Simulator_ScenarioFirstStepsRunOnTheStrand)
{
  bench_t bench;
  ASSERT_TRUE (bench.device->play<level_expect_script> ());
  // Not inside play (): the strand runs them.
  expect_playback (bench, playback_state::playing, 0, 1);
  EXPECT_EQ (bench.device->statistics ().scripted_messages, 0u);
  bench.settle ();
  expect_playback (bench, playback_state::finished, 1, 1);
  EXPECT_EQ (bench.device->statistics ().scripted_messages, 1u);
}

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioReplyThatCannotBeEncodedDoesNotAdvance)
{
  bench_t bench;
  // A value over the limits of FREQUENCY (10 to 500): the encoder refuses it.
  layout::reply_message_of<lab::PARAM_GET_t>::type bad
      = layout::reply_message_of<lab::PARAM_GET_t>::type ();
  field::get<0> (bad) = tables::lab_registry::FREQUENCY;
  field::get<1> (bad)
      = lumex::core::protocol::registry::parameter_value_t::of_raw (9999);
  bench.device->set_reply<lab::PARAM_GET_t> (bad);
  ASSERT_TRUE (bench.device->play<param_then_level_script> ());
  bench.call<lab::PARAM_GET_t> (tables::lab_registry::FREQUENCY);
  EXPECT_EQ (bench.device->statistics ().encode_errors, 1u);
  expect_playback (bench, playback_state::playing, 0, 2);
  EXPECT_EQ (bench.device->statistics ().scripted_messages, 0u);
}

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioOnlyTheRowOfTheStepAdvances)
{
  bench_t bench;
  std::vector<bytes_t> states;
  bench.subscribe<lab::STATE_t> (states);
  ASSERT_TRUE (bench.device->play<info_then_state_script> ());
  expect_playback (bench, playback_state::playing, 0, 2);
  // A request of another row is served as usual and does not advance.
  EXPECT_FALSE (bench.call<lab::STOP_t> ()->error);
  EXPECT_FALSE (bench.call<lab::LEVEL_t> ()->error);
  EXPECT_EQ (bench.device->statistics ().replies, 2u);
  expect_playback (bench, playback_state::playing, 0, 2);
  EXPECT_TRUE (states.empty ());
  EXPECT_EQ (bench.device->statistics ().scripted_messages, 0u);
  // The row of the step does, and the expect that follows is sent by itself.
  EXPECT_FALSE (bench.call<lab::INFO_t> ()->error);
  bench.settle ();
  expect_playback (bench, playback_state::finished, 2, 2);
  EXPECT_EQ (states.size (), 1u);
  EXPECT_EQ (bench.device->statistics ().scripted_messages, 1u);
}

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioRefusedRequestDoesNotAdvance)
{
  bench_t bench;
  std::vector<bytes_t> levels;
  bench.subscribe<lab::LEVEL_t> (levels);
  bench.device->set_state (1);
  ASSERT_TRUE (bench.device->play<move_then_level_script> ());
  // MOVE is accepted in the idle state only: the device refuses it.
  bench.call<lab::MOVE_t> (std::uint8_t (1));
  EXPECT_EQ (bench.device->statistics ().refused, 1u);
  expect_playback (bench, playback_state::playing, 0, 2);
  EXPECT_TRUE (levels.empty ());
  bench.device->set_state (0);
  EXPECT_FALSE (bench.call<lab::MOVE_t> (std::uint8_t (1))->error);
  bench.settle ();
  expect_playback (bench, playback_state::finished, 2, 2);
  EXPECT_EQ (levels.size (), 1u);
}

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioWaitsForARequestThatNeverComes)
{
  bench_t bench;
  ASSERT_TRUE (bench.device->play<info_then_state_script> ());
  bench.advance (ms (3600000));
  // No timeout of its own: it is still at the step, with no error.
  expect_playback (bench, playback_state::playing, 0, 2);
  EXPECT_EQ (bench.device->statistics ().scripted_messages, 0u);
}

TEST (LumexDeviceSimulatorScenario, Simulator_ScenarioUnderASilentDevice)
{
  bench_t bench;
  std::vector<bytes_t> states;
  bench.subscribe<lab::STATE_t> (states);
  ASSERT_TRUE (bench.device->play<info_then_state_script> ());
  bench.device->set_behavior (tr::testing::behavior::silent ());
  EXPECT_EQ (bench.call<lab::INFO_t> ()->error, code (protocol_errc::timeout));
  // The request was not processed: the script stays.
  EXPECT_EQ (bench.device->statistics ().not_processed, 1u);
  expect_playback (bench, playback_state::playing, 0, 2);
  bench.device->set_behavior (tr::testing::behavior::perfect ());
  EXPECT_FALSE (bench.call<lab::INFO_t> ()->error);
  bench.settle ();
  expect_playback (bench, playback_state::finished, 2, 2);
  EXPECT_EQ (states.size (), 1u);
}

TEST (LumexDeviceSimulatorScenario, Simulator_ScenarioUnderAFrozenDevice)
{
  bench_t bench;
  std::vector<bytes_t> states;
  bench.subscribe<lab::STATE_t> (states);
  ASSERT_TRUE (bench.device->play<info_state_stop_level_script> ());
  bench.device->set_behavior (tr::testing::behavior::frozen (1));
  // The reply is the one the device says: the request is processed and the
  // script goes on; what the script sends by itself after it is frozen too.
  EXPECT_FALSE (bench.call<lab::INFO_t> ()->error);
  bench.settle ();
  expect_playback (bench, playback_state::playing, 2, 4);
  EXPECT_TRUE (states.empty ());
  EXPECT_EQ (bench.device->statistics ().scripted_messages, 0u);
  // The next request is not processed at all.
  EXPECT_EQ (bench.call<lab::STOP_t> ()->error, code (protocol_errc::timeout));
  expect_playback (bench, playback_state::playing, 2, 4);
}

TEST (LumexDeviceSimulatorScenario, Simulator_ScenarioPredicateThatThrows)
{
  bench_t bench;
  ASSERT_TRUE (bench.device->play<throwing_script> ());
  bench.start_exchange (bench.make_exchange<lab::LEVEL_t> ());
  // The exception leaves the run () of the strand, as that of a handler.
  EXPECT_THROW (bench.advance (ms (250)), std::runtime_error);
  expect_playback (bench, playback_state::failed, 1, 4,
                   std::make_error_code (std::errc::interrupted));
  EXPECT_EQ (bench.device->statistics ().handler_errors, 1u);
  // The device goes on serving.
  EXPECT_FALSE (bench.call<lab::INFO_t> ()->error);
  expect_playback (bench, playback_state::failed, 1, 4,
                   std::make_error_code (std::errc::interrupted));
}

TEST (LumexDeviceSimulatorScenario, Simulator_ScenarioLoopThatNeverWaitsEnds)
{
  bench_t bench;
  ASSERT_TRUE (bench.device->play<loop_script> ());
  bench.settle ();
  // It does not spin: it ends with an error, and the strand serves on.
  expect_playback (
      bench, playback_state::failed, 1, 2,
      std::make_error_code (std::errc::resource_deadlock_would_occur));
  EXPECT_FALSE (bench.call<lab::INFO_t> ()->error);
  // A loop that sends a message each time ends too.
  bench.device->reset_statistics ();
  ASSERT_TRUE (bench.device->play<flood_script> ());
  bench.settle ();
  expect_playback (
      bench, playback_state::failed, 2, 3,
      std::make_error_code (std::errc::resource_deadlock_would_occur));
  EXPECT_GT (bench.device->statistics ().scripted_messages, 0u);
  EXPECT_LT (bench.device->statistics ().scripted_messages, 100000u);
}

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioLoopThatWaitsForARequestGoesOn)
{
  bench_t bench;
  // The loop polls LEVEL until it reads 3: the device answers 5, 5, 3.
  std::shared_ptr<int> turns = std::make_shared<int> (0);
  bench.device->on<lab::LEVEL_t> (
      [turns] (layout::request_of<lab::LEVEL_t>::type const &)
        { return std::uint8_t (++*turns < 3 ? 5 : 3); });
  std::shared_ptr<run_outcome_t> const client
      = run_both<counting_loop_script> (bench);
  EXPECT_FALSE (client->error) << client->error.message ();
  EXPECT_EQ (*turns, 3);
  expect_playback (bench, playback_state::finished, 4, 4);
}

TEST (LumexDeviceSimulatorScenario, Simulator_ScenarioSecondPlayWhileOneRuns)
{
  bench_t bench;
  ASSERT_TRUE (bench.device->play<info_stop_script> ());
  EXPECT_FALSE (bench.device->play<info_then_state_script> ());
  EXPECT_FALSE (bench.device->play<info_stop_script> ());
  // The running one is as it was.
  expect_playback (bench, playback_state::playing, 0, 2);
  EXPECT_FALSE (bench.call<lab::INFO_t> ()->error);
  EXPECT_FALSE (bench.call<lab::STOP_t> ()->error);
  expect_playback (bench, playback_state::finished, 2, 2);
  // Once it ended another one plays, from its first step.
  ASSERT_TRUE (bench.device->play<info_then_state_script> ());
  bench.settle ();
  expect_playback (bench, playback_state::playing, 0, 2);
  EXPECT_FALSE (bench.call<lab::INFO_t> ()->error);
  bench.settle ();
  expect_playback (bench, playback_state::finished, 2, 2);
}

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioThatFailsItsCheckIsNotPlayed)
{
  bench_t bench;
  EXPECT_FALSE (bench.device->play<lost_label_script> ());
  expect_playback (bench, playback_state::failed, 0, 2,
                   std::make_error_code (std::errc::invalid_argument));
  bench.settle ();
  EXPECT_FALSE (bench.call<lab::INFO_t> ()->error);
  EXPECT_EQ (bench.device->statistics ().scripted_messages, 0u);
  // A failed one does not stop the next.
  EXPECT_TRUE (bench.device->play<info_script> ());
  EXPECT_FALSE (bench.call<lab::INFO_t> ()->error);
  expect_playback (bench, playback_state::finished, 1, 1);
}

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioBranchWithNoReplyYetFails)
{
  bench_t bench;
  ASSERT_TRUE (bench.device->play<branch_first_script> ());
  bench.settle ();
  // As for the client: no reply of the row was seen in this run.
  expect_playback (bench, playback_state::failed, 0, 2,
                   code (protocol_errc::unexpected_reply));
}

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioExpectOfARowOutsideTheSessionSendsNothing)
{
  bench_t bench;
  bench.device->session ().set_firmware (5);
  ASSERT_TRUE (bench.device->play<outside_script> ());
  bench.settle ();
  expect_playback (bench, playback_state::playing, 1, 2);
  EXPECT_EQ (bench.device->statistics ().scripted_messages, 0u);
}

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioSurvivesTheDeviceBeingDestroyed)
{
  bench_t bench ((tr::testing::simulator_options_t ()), engine_options ());
  std::shared_ptr<run_outcome_t> const client
      = start_client<info_state_stop_level_script> (bench);
  ASSERT_TRUE (bench.device->play<info_state_stop_level_script> ());
  bench.device.reset ();
  // The device is gone: nothing answers, the client's run times out.
  EXPECT_TRUE (bench.run_until ([&] () { return client->calls != 0; }));
  EXPECT_EQ (client->error, code (protocol_errc::timeout));
  EXPECT_EQ (client->step, 0u);
}

namespace
{
// A predicate that destroys the device that calls it.
std::unique_ptr<device_t> *device_to_destroy = nullptr;

bool
destroys_the_device (std::uint8_t const &)
{
  device_to_destroy->reset ();
  return false;
}

LUMEX_PROTOCOL_SCENARIO (destroying_script, lab, (send<lab::LEVEL_t> ()),
                         (branch_on_reply<lab::LEVEL_t> (&destroys_the_device,
                                                         "end")),
                         (expect<lab::STATE_t> ()), (label ("end")));
} // namespace

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioPredicateDestroysTheDevice)
{
  st::raw_bench_t bench;
  device_to_destroy = &bench.device;
  ASSERT_TRUE (bench.device->play<destroying_script> ());
  bench.advance (ms (0));
  bench.write (st::framed (bytes ({ 0x13 })));
  // The device went inside its own predicate, which would let the expect
  // run: nothing more is sent.
  EXPECT_EQ (bench.device.get (), nullptr);
  EXPECT_EQ (bench.replies ().size (), 1u);
  device_to_destroy = nullptr;
}

namespace
{
// A device on a stream mock whose io_context the test may destroy.
struct context_device_t
{
  std::unique_ptr<asio::io_context> context;
  tr::testing::virtual_clock clock;
  std::shared_ptr<device_t> device;

  context_device_t () : context (new asio::io_context ())
  {
    tr::testing::mock_options_t options;
    options.timers = clock.source ();
    tr::testing::mock_stream mock
        = tr::testing::make_mock_stream (context->get_executor (), options);
    mock.stream.async_open ([] (std::error_code) {});
    st::drain (*context);
    device.reset (
        new device_t (mock.control, st::fr::slip::slip_codec (), clock));
  }
};
} // namespace

TEST (LumexDeviceSimulatorScenario, Simulator_ScenarioAfterTheContextIsGone)
{
  context_device_t bench;
  ASSERT_TRUE (bench.device->play<info_then_state_script> ());
  bench.context.reset ();
  // As start_stream: refused, and the playback it had is dropped.
  EXPECT_FALSE (bench.device->play<info_script> ());
  EXPECT_EQ (bench.device->playback ().state, playback_state::failed);
  EXPECT_EQ (bench.device->playback ().error,
             std::make_error_code (std::errc::operation_canceled));
}

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioPredicateHoldingTheDeviceEndsWithTheContext)
{
  context_device_t bench;
  std::weak_ptr<device_t> weak = bench.device;
  // A script whose predicate holds the device it plays on, waiting at a send.
  holding_device = bench.device;
  ASSERT_TRUE (bench.device->play<holding_script_t> ());
  holding_device.reset ();
  bench.device.reset ();
  EXPECT_FALSE (weak.expired ());
  bench.context.reset ();
  // The shutdown of the context dropped the script, and the device with it.
  EXPECT_TRUE (weak.expired ());
}

// --- the device and the client take the same branch

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioStateMessageOfAWaitIsNoReplyForABranch)
{
  for (int datagram = 0; datagram < 2; ++datagram)
    {
      std::unique_ptr<bench_t> const bench = make_bench (datagram != 0);
      // The client keeps the frame its expect took (idle): not busy, so STOP.
      std::shared_ptr<run_outcome_t> const client
          = run_both<state_branch_script> (*bench);
      ASSERT_EQ (client->calls, 1);
      EXPECT_FALSE (client->error) << client->error.message ();
      EXPECT_EQ (bench->device->statistics ().requests_of ("STOP"), 1u);
      EXPECT_EQ (bench->device->statistics ().requests_of ("RESET"), 0u);
      // The device took the same branch: it waited at STOP, not at RESET.
      expect_playback (*bench, playback_state::finished, 8, 8);
    }
}

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioBranchOnAStateTheWaitShowedHasNoReply)
{
  for (int datagram = 0; datagram < 2; ++datagram)
    {
      std::unique_ptr<bench_t> const bench = make_bench (datagram != 0);
      // A wait for a state keeps no frame of the state row for the client:
      // the branch fails there, and the device must fail in the same way.
      std::shared_ptr<run_outcome_t> const client
          = run_both<state_branch_no_expect_script> (*bench);
      ASSERT_EQ (client->calls, 1);
      EXPECT_EQ (client->error, code (protocol_errc::unexpected_reply));
      expect_playback (*bench, playback_state::failed, 2, 4,
                       code (protocol_errc::unexpected_reply));
    }
}

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioWaitForAStateTheRunShowedSendsNoSecondMessage)
{
  for (int datagram = 0; datagram < 2; ++datagram)
    {
      std::unique_ptr<bench_t> const bench = make_bench (datagram != 0);
      // The expect takes the configured message (idle); a second message of
      // the wait for a state already shown would be taken by it instead.
      layout::reply_message_of<lab::STATE_t>::type idle
          = layout::reply_message_of<lab::STATE_t>::type ();
      bench->device->set_reply<lab::STATE_t> (idle);
      std::shared_ptr<run_outcome_t> const client
          = run_both<twice_waiting_script> (*bench);
      ASSERT_EQ (client->calls, 1);
      EXPECT_FALSE (client->error) << client->error.message ();
      EXPECT_EQ (bench->device->statistics ().requests_of ("STOP"), 1u);
      EXPECT_EQ (bench->device->statistics ().requests_of ("RESET"), 0u);
      expect_playback (*bench, playback_state::finished, 9, 9);
      // One for the first wait, one for the expect.
      EXPECT_EQ (bench->device->statistics ().scripted_messages, 2u);
    }
}

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioWaitForAStateShowsItAgainstAConfiguredMessage)
{
  std::unique_ptr<bench_t> const bench = make_bench (false);
  layout::reply_message_of<lab::STATE_t>::type idle
      = layout::reply_message_of<lab::STATE_t>::type ();
  bench->device->set_reply<lab::STATE_t> (idle);
  std::shared_ptr<run_outcome_t> const client
      = run_both<wait_busy_script> (*bench);
  EXPECT_FALSE (client->error) << client->error.message ();
  expect_playback (*bench, playback_state::finished, 1, 1);
}

// --- behaviours that move bytes in time

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioMessagesKeepTheirOrderUnderBehavioursThatMoveBytes)
{
  for (int datagram = 0; datagram < 2; ++datagram)
    for (int kind = 0; kind < 2; ++kind)
      {
        // fragmented () splits a message across datagrams, which no client
        // takes for a frame: there is nothing to order.
        if (datagram != 0 && kind == 1)
          continue;
        std::unique_ptr<bench_t> const bench = make_bench (datagram != 0);
        bench->device->set_behavior (
            kind == 0 ? tr::testing::behavior::slow_trickle (ms (2))
                      : tr::testing::behavior::fragmented (7));
        std::shared_ptr<run_outcome_t> const client
            = run_both<info_then_state_script> (*bench);
        ASSERT_EQ (client->calls, 1);
        EXPECT_FALSE (client->error)
            << client->error.message () << " datagram " << datagram << " kind "
            << kind;
        expect_playback (*bench, playback_state::finished, 2, 2);
      }
}

TEST (LumexDeviceSimulatorScenario,
      Simulator_TwoPhaseReplyKeepsItsOrderUnderBehavioursThatMoveBytes)
{
  for (int datagram = 0; datagram < 2; ++datagram)
    for (int kind = 0; kind < 2; ++kind)
      {
        // fragmented () splits a message across datagrams, which no client
        // takes for a frame: there is nothing to order.
        if (datagram != 0 && kind == 1)
          continue;
        std::unique_ptr<bench_t> const bench = make_bench (datagram != 0);
        bench->device->set_behavior (
            kind == 0 ? tr::testing::behavior::slow_trickle (ms (2))
                      : tr::testing::behavior::fragmented (7));
        st::outcome_ptr const out
            = bench->call<lab::PRIME_t> (std::uint16_t (5));
        EXPECT_FALSE (out->error) << out->error.message () << " datagram "
                                  << datagram << " kind " << kind;
      }
}

// --- an expect the device cannot satisfy

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioExpectOfARowThatHasNoMessageFails)
{
  bench_t bench;
  // A row without a reply, a key-only acknowledgement row.
  ASSERT_TRUE (bench.device->play<expect_reset_script> ());
  bench.settle ();
  expect_playback (bench, playback_state::failed, 0, 1,
                   code (protocol_errc::not_supported));
  ASSERT_TRUE (bench.device->play<expect_ack_script> ());
  bench.settle ();
  expect_playback (bench, playback_state::failed, 0, 1,
                   code (protocol_errc::not_supported));
}

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioExpectOfARowAnotherRowAnswersFails)
{
  bench_t bench;
  // WRITE is answered by a message of CHANGE, which no expect<WRITE> takes.
  ASSERT_TRUE (bench.device->play<expect_write_script> ());
  bench.settle ();
  expect_playback (bench, playback_state::failed, 0, 1,
                   code (protocol_errc::not_supported));
}

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioExpectOfATwoPhaseRowSendsTheCompletion)
{
  std::unique_ptr<bench_t> const bench = make_bench (false);
  std::shared_ptr<run_outcome_t> const client
      = run_both<expect_prime_script> (*bench);
  EXPECT_FALSE (client->error) << client->error.message ();
  expect_playback (*bench, playback_state::finished, 1, 1);
}

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioMessageThatCannotBeEncodedFails)
{
  bench_t bench;
  layout::reply_message_of<lab::PARAM_REPORT_t>::type bad
      = layout::reply_message_of<lab::PARAM_REPORT_t>::type ();
  field::get<0> (bad) = tables::lab_registry::FREQUENCY;
  field::get<1> (bad)
      = lumex::core::protocol::registry::parameter_value_t::of_raw (9999);
  bench.device->set_reply<lab::PARAM_REPORT_t> (bad);
  ASSERT_TRUE (bench.device->play<expect_report_script> ());
  bench.settle ();
  EXPECT_EQ (bench.device->statistics ().encode_errors, 1u);
  expect_playback (bench, playback_state::failed, 0, 1,
                   code (protocol_errc::invalid_value));
}

// --- when the script starts

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioPlayedAfterTheClientSentItsFirstRequestMissesIt)
{
  std::unique_ptr<bench_t> const bench = make_bench (false);
  std::shared_ptr<run_outcome_t> const client
      = start_client<info_then_state_script> (*bench);
  // The request is served as usual before the script starts, and the client
  // waits for the message at its expect while the device waits for INFO.
  bench->settle ();
  ASSERT_TRUE (bench->device->play<info_then_state_script> ());
  EXPECT_TRUE (bench->run_until ([&] () { return client->calls != 0; }));
  EXPECT_EQ (client->error, code (protocol_errc::timeout));
  expect_playback (*bench, playback_state::playing, 0, 2);
}

TEST (
    LumexDeviceSimulatorScenario,
    Simulator_ScenarioStartedByAHandlerIsNotAdvancedByTheRequestThatStartedIt)
{
  bench_t bench;
  device_t *const device = bench.device.get ();
  bool played = false;
  bench.device->on<lab::INFO_t> (
      [device, &played] (layout::request_of<lab::INFO_t>::type const &)
        { played = device->play<info_stop_script> (); });
  EXPECT_FALSE (bench.call<lab::INFO_t> ()->error);
  bench.settle ();
  EXPECT_TRUE (played);
  // The request that started it is not its first send.
  expect_playback (bench, playback_state::playing, 0, 2);
  bench.device->clear_reply<lab::INFO_t> ();
  EXPECT_FALSE (bench.call<lab::INFO_t> ()->error);
  EXPECT_FALSE (bench.call<lab::STOP_t> ()->error);
  expect_playback (bench, playback_state::finished, 2, 2);
}

// --- the edges of the loop cap and of the decisions

TEST (LumexDeviceSimulatorScenario, Simulator_ScenarioLoopCapIsAThousandJumps)
{
  for (int wanted = 1000; wanted <= 1001; ++wanted)
    {
      bench_t bench;
      turns_made = 0;
      turns_wanted = wanted;
      ASSERT_TRUE (bench.device->play<cap_script> ());
      EXPECT_FALSE (bench.call<lab::LEVEL_t> ()->error);
      bench.settle ();
      if (wanted == 1000)
        expect_playback (bench, playback_state::finished, 3, 3);
      else
        expect_playback (
            bench, playback_state::failed, 2, 3,
            std::make_error_code (std::errc::resource_deadlock_would_occur));
    }
}

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioOnlyJumpsBackCountAgainstTheCap)
{
  bench_t bench;
  turns_made = 0;
  turns_wanted = 600;
  ASSERT_TRUE (bench.device->play<forward_back_script> ());
  EXPECT_FALSE (bench.call<lab::LEVEL_t> ()->error);
  bench.settle ();
  expect_playback (bench, playback_state::finished, 5, 5);
}

TEST (LumexDeviceSimulatorScenario, Simulator_ScenarioLoopThroughTwoLabelsEnds)
{
  bench_t bench;
  ASSERT_TRUE (bench.device->play<two_labels_loop_script> ());
  bench.settle ();
  expect_playback (
      bench, playback_state::failed, 3, 4,
      std::make_error_code (std::errc::resource_deadlock_would_occur));
}

TEST (LumexDeviceSimulatorScenario, Simulator_ScenarioDecisionThatFailsEnds)
{
  {
    bench_t bench;
    ASSERT_TRUE (bench.device->play<failing_decision_script_t<true>> ());
    EXPECT_FALSE (bench.call<lab::LEVEL_t> ()->error);
    bench.settle ();
    expect_playback (bench, playback_state::failed, 1, 3,
                     std::make_error_code (std::errc::io_error));
  }
  {
    bench_t bench;
    ASSERT_TRUE (bench.device->play<failing_decision_script_t<false>> ());
    EXPECT_FALSE (bench.call<lab::LEVEL_t> ()->error);
    bench.settle ();
    expect_playback (bench, playback_state::failed, 1, 3,
                     std::make_error_code (std::errc::io_error));
  }
}

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioPlayAfterAFailedOneStartsAtOnce)
{
  bench_t bench;
  ASSERT_TRUE (bench.device->play<two_labels_loop_script> ());
  bench.settle ();
  expect_playback (
      bench, playback_state::failed, 3, 4,
      std::make_error_code (std::errc::resource_deadlock_would_occur));
  // Before the strand runs anything of the new one.
  ASSERT_TRUE (bench.device->play<send_stop_script> ());
  expect_playback (bench, playback_state::playing, 0, 1);
  bench.settle ();
  expect_playback (bench, playback_state::playing, 0, 1);
}

// --- another thread reads and configures while the strand plays

TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioReadAndConfiguredFromAnotherThreadWhilePlaying)
{
  for (int pass = 0; pass < 5; ++pass)
    {
      asio::io_context context;
      tr::testing::virtual_clock clock;
      tr::testing::mock_options_t options;
      options.timers = clock.source ();
      tr::testing::mock_stream mock
          = tr::testing::make_mock_stream (context.get_executor (), options);
      mock.stream.async_open ([] (std::error_code) {});
      context.poll ();
      context.restart ();
      device_t device (mock.control, st::fr::slip::slip_codec (), clock);
      asio::executor_work_guard<asio::io_context::executor_type> guard
          = asio::make_work_guard (context);
      std::thread runner ([&context] () { context.run (); });
      std::atomic<bool> stop (false);
      std::thread other (
          [&device, &stop] ()
            {
              std::uint8_t value = 0;
              while (!stop.load ())
                {
                  device.playback ();
                  device.statistics ();
                  device.set_reply<lab::LEVEL_t> (++value);
                  device.clear_reply<lab::LEVEL_t> ();
                  device.set_state (value & 1);
                  device.play<state_branch_script> ();
                  std::this_thread::yield ();
                }
            });
      for (int i = 0; i < 200; ++i)
        {
          device.play<info_then_state_script> ();
          device.playback ();
        }
      std::this_thread::sleep_for (std::chrono::milliseconds (20));
      stop.store (true);
      other.join ();
      guard.reset ();
      context.stop ();
      runner.join ();
    }
}

// --- a completion or an acknowledgement that comes by a row that streams

// The rule of the engine as it stands (a frame of a row that streams is an
// event and never reaches the reader of a by_order exchange; see
// ByOrder_AnEventOrALateRowDoesNotReachTheReader): a table whose completion
// or acknowledgement comes by the state row is answered by the device and
// timed out by the client.
TEST (LumexDeviceSimulatorScenario,
      Simulator_ReplyKindCompletedByARowThatStreamsIsAnEventForTheEngine)
{
  for (int datagram = 0; datagram < 2; ++datagram)
    {
      std::unique_ptr<bench_t> const bench = make_bench (datagram != 0);
      st::outcome_ptr const phase = bench->call<lab::PHASE_STATE_t> (1);
      EXPECT_EQ (phase->error, code (protocol_errc::timeout));
      EXPECT_EQ (bench->device->statistics ().replies, 2u);
      st::outcome_ptr const ack = bench->call<lab::ASK_STATE_ACK_t> ();
      EXPECT_EQ (ack->error, code (protocol_errc::timeout));
    }
}

LUMEX_PROTOCOL_SCENARIO (phase_state_branch_script, lab,
                         (send<lab::PHASE_STATE_t> (1)),
                         (branch_on_reply<lab::STATE_t> (&is_busy, "high")),
                         (send<lab::STOP_t> ()), (jump ("end")),
                         (label ("high")), (send<lab::RESET_t> ()),
                         (label ("end")));

// The device keeps the last message of a two-phase reply, the completion, as
// the client does (the client of such a table needs reply_row, see above).
TEST (LumexDeviceSimulatorScenario,
      Simulator_ScenarioKeepsTheLastMessageOfATwoPhaseReply)
{
  bench_t bench;
  layout::reply_message_of<lab::STATE_t>::type busy
      = layout::reply_message_of<lab::STATE_t>::type ();
  field::get<0> (busy) = tables::lab_state::busy;
  bench.device->set_reply<lab::PHASE_STATE_t> (busy);
  ASSERT_TRUE (bench.device->play<phase_state_branch_script> ());
  // The completion says busy; the acknowledgement before it is no STATE
  // message: the device takes the high branch and waits at RESET.
  bench.call<lab::PHASE_STATE_t> (1);
  bench.settle ();
  expect_playback (bench, playback_state::playing, 5, 7);
  EXPECT_FALSE (bench.call<lab::RESET_t> ()->error);
  expect_playback (bench, playback_state::finished, 7, 7);
}

// The one behaviour the ordering cannot help: fragments of a message are
// datagrams of their own, so no reply is a frame for the client.
TEST (LumexDeviceSimulatorScenario,
      Simulator_FragmentedDatagramsAreNoFramesForAnyReply)
{
  std::unique_ptr<bench_t> const bench = make_bench (true);
  bench->device->set_behavior (tr::testing::behavior::fragmented (7));
  EXPECT_EQ (bench->call<lab::INFO_t> ()->error,
             code (protocol_errc::timeout));
}
