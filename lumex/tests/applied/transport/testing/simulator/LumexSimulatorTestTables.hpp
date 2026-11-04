// The synthetic table of the tests of the device simulator
// (lumex/applied/transport/testing/simulator): an invented "lab" instrument
// with one row of every reply kind, a stream whose state field is the state
// of the device, rows accepted in one state only, a registry with generic
// get and set rows, and rows limited to a firmware. No real device, its
// names, codes or values are in it.
#ifndef LUMEX_TESTS_APPLIED_TRANSPORT_TESTING_SIMULATOR_TEST_TABLES_HPP
#define LUMEX_TESTS_APPLIED_TRANSPORT_TESTING_SIMULATOR_TEST_TABLES_HPP

#include <cstddef>
#include <cstdint>
#include <initializer_list>

#include "lumex/applied/transport/protocol/dsl/LumexDialogueVocabulary.hpp"
#include "lumex/core/protocol/LumexProtocol"

namespace lumex_tests
{
namespace simulator_tables
{
namespace dialogue = lumex::applied::transport::protocol::dialogue;
namespace registry = lumex::core::protocol::registry;
namespace layout = lumex::core::protocol::layout;

/// The states of the lab instrument.
enum class lab_state : std::uint8_t
{
  idle = 0,
  busy = 1,
  fault = 2
};

struct codec_t
{
};

/// A one-byte key first; the state of the device is field 0 of STATE.
struct lab_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;

  template <typename P>
  using dialogue_options
      = dialogue::options<dialogue::state_from<typename P::STATE_t, 0>>;
};

// clang-format off
#define LAB_PARAMS(P, P_AS) \
  P (FREQUENCY, (0x01), "Frequency", (u16_le_), RW, (limits (10, 500))) \
  P (LEVEL, (0x02), "Level", (u8_), RW, (limits (0, 100))) \
  P (SERIAL, (0x03), "Serial number", (u16_le_), R, ()) \
  P (ARM, (0x04), "Arm", (u8_), W, ()) \
  P (ODD, (0x05), "Odd", (u8_<scale<std::ratio<1, 10> > >), RW, \
     (limits (0.04, 0.5499)))
// clang-format on
LUMEX_PROTOCOL_REGISTRY (lab_registry, LAB_PARAMS);

// Different from the acknowledgement of MOVE, so that the two are told apart.
/// The session value that chooses the layout of CTX_SET.
LUMEX_PROTOCOL_CONTEXT_KEY (lab_mode);

LUMEX_PROTOCOL_LAYOUT (small_t, (u8_), ("a"));
LUMEX_PROTOCOL_LAYOUT (wide_t, (u16_le_, u8_), ("a", "b"));
LUMEX_PROTOCOL_LAYOUT (
    mode_t, (union_by_context_<lab_mode, case_<0, small_t>, case_<1, wide_t>>),
    ("form"));
LUMEX_PROTOCOL_LAYOUT (pair_t, (u8_, u16_le_), ("a", "b"));
LUMEX_PROTOCOL_LAYOUT (code_t, (u8_), ("code"));
LUMEX_PROTOCOL_LAYOUT (reject_t, (u8_, u16_le_), ("code", "detail"));
LUMEX_PROTOCOL_LAYOUT (info_t, (text_<8>, u16_le_), ("model", "firmware"));
LUMEX_PROTOCOL_LAYOUT (status_t, (enum_<u8_, lab_state>, u8_),
                       ("state", "level"));
LUMEX_PROTOCOL_LAYOUT (change_t, (u8_, u16_le_), ("parameter", "value"));
LUMEX_PROTOCOL_LAYOUT (param_t,
                       (registry::registry_key_<lab_registry, u8_>,
                        registry::registry_value_<lab_registry, 0, u16_le_,
                                                  registry::access_t::read>),
                       ("parameter", "value"));

// The acknowledgement and the change report are rows of their own, written
// before the rows that name them.
// clang-format off
#define LAB_ROWS(X, X_AS) \
  X (ACK, 0x06, "Accepted", (), (), (), ()) \
  X (REJECT, 0x15, "Refused", (), (), (reject_t), ()) \
  X (CHANGE, 0x30, "A parameter changed", (), (), (change_t), ()) \
  X (INFO, 0x10, "Identity", (), (), (info_t), (idempotent)) \
  X (STATE, 0x12, "State", (), (), (status_t), \
     (stream (100), edge_event<0, 0, 1> ())) \
  X (LEVEL, 0x13, "Level", (), (), (u8_), (stream (250))) \
  X (RESET, 0x20, "Reset", (), (), (), ()) \
  X (MOVE, 0x21, "Move", (u8_), ("target"), (ack_nak<u8_, reject_t>), \
     (accepted_in (lab_state::idle))) \
  X (STOP, 0x22, "Stop", (), (), (ack_nak<ACK_t, REJECT_t>), ()) \
  X (SET_LIMIT, 0x23, "Set the limit", (u8_), ("limit"), \
     (echo_zero_on_error<u8_>), (accepted_in (lab_state::idle))) \
  X (SET_MODE, 0x24, "Set the mode", (enum_<u8_, lab_state>), ("mode"), \
     (state_instead_of_refusal<enum_<u8_, lab_state> >), \
     (accepted_in (lab_state::idle))) \
  X (SET_FLOW, 0x25, "Set the flow", (u16_le_), ("flow"), \
     (echo_set<u16_le_>), (accepted_in (lab_state::idle))) \
  X (PRIME, 0x26, "Prime", (u16_le_), ("volume"), \
     (two_phase<ACK_t, u16_le_>), (timeout (5000))) \
  X (WRITE, 0x27, "Write a parameter", (u8_, u16_le_), \
     ("parameter", "value"), (), (reply_row<CHANGE_t> ())) \
  X (PARAM_GET, 0x40, "Read a parameter", (registry_key_<lab_registry, u8_>), \
     ("parameter"), (param_t), (idempotent)) \
  X (PARAM_SET, 0x41, "Write a parameter", \
     (registry_key_<lab_registry, u8_>, \
      registry_value_<lab_registry, 0, u16_le_>), \
     ("parameter", "value"), (param_t), ()) \
  X (PARAM_ASK, 0x42, "Ask for a parameter", \
     (registry_key_<lab_registry, u8_>), ("parameter"), \
     (ack_nak<param_t, code_t>), ()) \
  X (PARAM_REPORT, 0x45, "A parameter report", (), (), (param_t), ()) \
  X (PARAM_PHASE, 0x43, "Ask for a parameter in two phases", \
     (registry_key_<lab_registry, u8_>), ("parameter"), \
     (two_phase<ACK_t, param_t>), ()) \
  X (PARAM_NOTE, 0x44, "Ask for a parameter, a report answers", \
     (registry_key_<lab_registry, u8_>), ("parameter"), (), \
     (reply_row<PARAM_REPORT_t> ())) \
  X (BOTH, 0x52, "Acknowledgement and refusal of one shape", (u8_), ("t"), \
     (ack_nak<u8_, i8_>), ()) \
  X (ASK_STATE, 0x54, "Ask for the state, the state row answers", (), (), (), \
     (reply_row<STATE_t> ())) \
  X (STATE_NOTE, 0x5a, "A state report that does not stream", (), (), \
     (status_t), ()) \
  X (PHASE_STATE, 0x55, "Two phases, a state report completes", (u8_), ("t"), \
     (two_phase<ACK_t, STATE_NOTE_t>), ()) \
  X (ASK_STATE_ACK, 0x56, "Acknowledged by a state report", (), (), \
     (ack_nak<STATE_NOTE_t, code_t>), ()) \
  X (ECHO_WHOLE, 0x57, "Echo of the whole record", (u8_, u16_le_), \
     ("a", "b"), (echo_set<pair_t>), ()) \
  X (OLD_STREAM, 0x58, "A stream of an old firmware", (), (), (u8_), \
     (stream (100), firmware (1, 9))) \
  X (CTX_SET, 0x59, "A request that depends on the session", (mode_t), \
     ("form"), (), ()) \
  X (LEGACY, 0x50, "An old row", (), (), (u8_), (firmware (1, 9))) \
  X (NEWER, 0x51, "A new row", (), (), (u8_), (firmware (20, 99)))
// clang-format on

LUMEX_PROTOCOL_DEFINE (lab_t, codec_t, lab_header_t, LAB_ROWS);

/// The bytes of a list of values.
inline layout::bytes_t
bytes (std::initializer_list<int> values)
{
  layout::bytes_t out;
  for (int value : values)
    out.push_back (static_cast<std::uint8_t> (value));
  return out;
}
} // namespace simulator_tables
} // namespace lumex_tests

#endif // !LUMEX_TESTS_APPLIED_TRANSPORT_TESTING_SIMULATOR_TEST_TABLES_HPP
