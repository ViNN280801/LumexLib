// LUMEX_PROTOCOL_SCENARIO (lumex/core/protocol/dsl): the steps a scenario
// describes, in order, with no I/O: the message of a send, the row of an
// expect, the state of a wait, the decision of a branch, the labels, and the
// check of the labels. An invented table: no real device, its names, codes or
// values are in it.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"

namespace
{
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace perror = lumex::core::protocol::error;
namespace index = lumex::core::protocol::index;

enum class mixer_state : std::uint8_t
{
  idle,
  busy
};

struct mixer_codec_t
{
};

struct mixer_header_t
{
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
};

LUMEX_PROTOCOL_LAYOUT (mixer_status_t, (u8_, enum_<u8_, mixer_state>),
                       ("speed", "state"));

// clang-format off
#define MIXER_ROWS(X, X_AS) \
  X (WASH, 0x01, "Wash", (u8_, u16_le_), ("cycles", "volume"), (), ()) \
  X (STATUS, 0x02, "Status", (), (), (mixer_status_t), ()) \
  X (INJECT, 0x03, "Inject", (), (), (u8_), ()) \
  X (ERASE, 0x04, "Erase", (u8_), ("block"), (), (dangerous)) \
  X (SILENT, 0x05, "No reply", (), (), (), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (mixer_t, mixer_codec_t, mixer_header_t, MIXER_ROWS);

bool
is_idle (field::value_of<mixer_status_t>::type const &status)
{
  return field::get<1> (status) == mixer_state::idle;
}

LUMEX_PROTOCOL_SCENARIO (
    inject_scenario, mixer_t, (send<mixer_t::WASH_t> (2, 500)),
    (expect<mixer_t::STATUS_t> ()), (expect<mixer_t::STATUS_t> (750)),
    (wait_state (mixer_state::busy, 10000)), (label ("again")),
    (send<mixer_t::STATUS_t> ()),
    (branch_on_reply<mixer_t::STATUS_t> (&is_idle, "done")), (jump ("again")),
    (label ("done")), (send<mixer_t::INJECT_t> ()),
    (send<mixer_t::ERASE_t> (confirm, 7)));

LUMEX_PROTOCOL_SCENARIO (lost_label_scenario, mixer_t, (jump ("nowhere")));
LUMEX_PROTOCOL_SCENARIO (twice_label_scenario, mixer_t, (label ("here")),
                         (label ("here")));
LUMEX_PROTOCOL_SCENARIO (too_big_scenario, mixer_t,
                         (send<mixer_t::WASH_t> (300, 1)));
LUMEX_PROTOCOL_SCENARIO (
    lambda_scenario, mixer_t,
    (branch_on_reply<mixer_t::INJECT_t> ([] (std::uint8_t const &value)
                                           { return value == 9; }, "end")),
    (label ("end")));

layout::bytes_t
encoded (dsl::scenario_step_t const &step)
{
  layout::bytes_t out;
  index::session_context_t const session;
  field::decode_context_t const context = session.decode_context ();
  EXPECT_FALSE (step.encode (out, context));
  return out;
}
} // namespace

TEST (LumexProtocolScenarioTest,
      GivenAScenario_WhenMade_ThenItsStepsAreInOrderWithTheirKindsAndRows)
{
  dsl::scenario_t const made = inject_scenario::make ();
  EXPECT_STREQ (made.name, "inject_scenario");
  ASSERT_EQ (made.steps.size (), 11u);
  dsl::scenario_step_kind const kinds[]
      = { dsl::scenario_step_kind::send,   dsl::scenario_step_kind::expect,
          dsl::scenario_step_kind::expect, dsl::scenario_step_kind::wait_state,
          dsl::scenario_step_kind::label,  dsl::scenario_step_kind::send,
          dsl::scenario_step_kind::branch, dsl::scenario_step_kind::branch,
          dsl::scenario_step_kind::label,  dsl::scenario_step_kind::send,
          dsl::scenario_step_kind::send };
  for (std::size_t i = 0; i < made.steps.size (); ++i)
    EXPECT_EQ (made.steps[i].kind, kinds[i]) << "step " << i;
  EXPECT_STREQ (made.steps[0].row, "WASH");
  EXPECT_STREQ (made.steps[1].row, "STATUS");
  EXPECT_STREQ (made.steps[6].row, "STATUS");
  EXPECT_EQ (made.steps[7].row, nullptr);
  EXPECT_STREQ (made.steps[7].label, "again");
  EXPECT_FALSE (made.check ());
  std::is_same<decltype (inject_scenario::make ()), dsl::scenario_t> const
      same{};
  EXPECT_TRUE (same.value);
  std::is_same<inject_scenario::protocol_type, mixer_t> const protocol{};
  EXPECT_TRUE (protocol.value);
}

TEST (LumexProtocolScenarioTest,
      GivenASend_WhenEncoded_ThenItIsTheMessageOfTheRowWithItsArguments)
{
  dsl::scenario_t const made = inject_scenario::make ();
  layout::bytes_t expected;
  ASSERT_TRUE (
      (layout::encode_message<mixer_t, mixer_t::WASH_t> (expected, 2, 500)
           .has_value ()));
  EXPECT_EQ (encoded (made.steps[0]), expected);
  EXPECT_EQ (encoded (made.steps[5]), layout::bytes_t (1, 0x02));
  // The second send of the row encodes the same bytes again.
  EXPECT_EQ (encoded (made.steps[0]), expected);
}

TEST (
    LumexProtocolScenarioTest,
    GivenADangerousRowWithItsConfirmation_WhenEncoded_ThenTheArgumentIsNoField)
{
  dsl::scenario_t const made = inject_scenario::make ();
  layout::bytes_t expected;
  ASSERT_TRUE ((layout::encode_message<mixer_t, mixer_t::ERASE_t> (expected, 7)
                    .has_value ()));
  EXPECT_EQ (encoded (made.steps[10]), expected);
}

TEST (LumexProtocolScenarioTest,
      GivenAnArgumentThatDoesNotFit_WhenEncoded_ThenItIsItsErrorAndNoBytes)
{
  dsl::scenario_t const made = too_big_scenario::make ();
  layout::bytes_t out;
  index::session_context_t const session;
  field::decode_context_t const context = session.decode_context ();
  EXPECT_EQ (made.steps[0].encode (out, context),
             perror::make_error_code (perror::protocol_errc::invalid_value));
  EXPECT_TRUE (out.empty ());
}

TEST (LumexProtocolScenarioTest,
      GivenAnExpectAndAWait_WhenMade_ThenTheirTimesAndTheStateAreKept)
{
  dsl::scenario_t const made = inject_scenario::make ();
  EXPECT_TRUE (made.steps[1].default_timeout);
  EXPECT_FALSE (made.steps[2].default_timeout);
  EXPECT_EQ (made.steps[2].timeout_ms, 750u);
  EXPECT_FALSE (made.steps[3].default_timeout);
  EXPECT_EQ (made.steps[3].timeout_ms, 10000u);
  EXPECT_EQ (made.steps[3].state,
             static_cast<std::int64_t> (mixer_state::busy));
}

TEST (LumexProtocolScenarioTest,
      GivenABranch_WhenAskedWithAReply_ThenItDecidesByThePredicateOfItsValue)
{
  dsl::scenario_t const made = inject_scenario::make ();
  dsl::scenario_step_t const &step = made.steps[6];
  index::session_context_t const session;
  field::decode_context_t const context = session.decode_context ();
  layout::bytes_t const idle = { 0x02, 0x05, 0x00 };
  layout::bytes_t const busy = { 0x02, 0x05, 0x01 };
  bool taken = false;
  EXPECT_FALSE (step.decide (layout::bytes_view_t (idle.data (), idle.size ()),
                             context, taken));
  EXPECT_TRUE (taken);
  EXPECT_FALSE (step.decide (layout::bytes_view_t (busy.data (), busy.size ()),
                             context, taken));
  EXPECT_FALSE (taken);
}

TEST (LumexProtocolScenarioTest,
      GivenABranch_WhenItsFrameIsAnotherRow_ThenItAnswersTheErrorOfTheDecoder)
{
  dsl::scenario_t const made = inject_scenario::make ();
  index::session_context_t const session;
  field::decode_context_t const context = session.decode_context ();
  layout::bytes_t const other = { 0x03, 0x05 };
  bool taken = true;
  EXPECT_EQ (
      made.steps[6].decide (
          layout::bytes_view_t (other.data (), other.size ()), context, taken),
      perror::make_error_code (perror::protocol_errc::unexpected_reply));
  EXPECT_TRUE (taken);
}

TEST (LumexProtocolScenarioTest,
      GivenABranchWithALambda_WhenAsked_ThenItTakesTheValueOfOneFieldReply)
{
  dsl::scenario_t const made = lambda_scenario::make ();
  index::session_context_t const session;
  field::decode_context_t const context = session.decode_context ();
  layout::bytes_t const nine = { 0x03, 0x09 };
  bool taken = false;
  EXPECT_FALSE (made.steps[0].decide (
      layout::bytes_view_t (nine.data (), nine.size ()), context, taken));
  EXPECT_TRUE (taken);
}

TEST (LumexProtocolScenarioTest,
      GivenLabels_WhenFound_ThenTheStepIsNamedAndAMissingOneIsTheEnd)
{
  dsl::scenario_t const made = inject_scenario::make ();
  EXPECT_EQ (made.find_label ("again"), 4u);
  EXPECT_EQ (made.find_label ("done"), 8u);
  EXPECT_EQ (made.find_label ("nowhere"), made.steps.size ());
}

TEST (LumexProtocolScenarioTest,
      GivenALabelThatIsMissingOrGivenTwice_WhenChecked_ThenTheCheckNamesIt)
{
  EXPECT_EQ (lost_label_scenario::make ().check (),
             std::make_error_code (std::errc::invalid_argument));
  EXPECT_EQ (twice_label_scenario::make ().check (),
             std::make_error_code (std::errc::invalid_argument));
  EXPECT_FALSE (lambda_scenario::make ().check ());
}
