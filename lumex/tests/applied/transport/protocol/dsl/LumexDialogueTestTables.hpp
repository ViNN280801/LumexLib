// Synthetic tables of the tests of the dialogue vocabulary
// (lumex/applied/transport/protocol/dsl): an invented "pump" with every
// reply kind, every row option and every protocol option of the dialogue, a
// layer of it, a tokenized twin and headers with and without protocol
// options. No real device, its names, codes or values are in them.
#ifndef LUMEX_TESTS_APPLIED_TRANSPORT_PROTOCOL_DSL_DIALOGUE_TEST_TABLES_HPP
#define LUMEX_TESTS_APPLIED_TRANSPORT_PROTOCOL_DSL_DIALOGUE_TEST_TABLES_HPP

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <ratio>
#include <vector>

#include "lumex/applied/transport/protocol/dsl/LumexDialogueVocabulary.hpp"
#include "lumex/core/protocol/LumexProtocol"

namespace lumex_tests
{
namespace dialogue_tables
{
namespace dialogue = lumex::applied::transport::protocol::dialogue;
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace index = lumex::core::protocol::index;
namespace layout = lumex::core::protocol::layout;

/// The states the pump reports.
enum class pump_state : std::uint8_t
{
  idle,
  running,
  fault
};

struct codec_t
{
};

/// Answers every reply (the matcher of a row with a custom correlation).
inline bool
match_any (index::row_t const *, layout::bytes_view_t, layout::bytes_view_t)
{
  return true;
}

/// A one-byte key first; the protocol options name rows of the layer they
/// are read for.
struct pump_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;

  template <typename P>
  using dialogue_options = dialogue::options<
      dialogue::keepalive<typename P::CLOCK_t, 1000>,
      dialogue::watchdog_ms<3000>, dialogue::min_interval_ms<20>,
      dialogue::state_from<typename P::STATUS_t, 1>,
      dialogue::identify_with<typename P::INFO_t>, dialogue::max_in_flight<1>,
      dialogue::default_timeout_ms<400>, dialogue::default_retries<1>,
      dialogue::late_reply_window_ms<150>>;
};

LUMEX_PROTOCOL_LAYOUT (refusal_t, (u8_, u16_le_), ("code", "detail"));
LUMEX_PROTOCOL_LAYOUT (status_t, (u8_, enum_<u8_, pump_state>, u16_le_),
                       ("valve", "state", "flow"));
LUMEX_PROTOCOL_LAYOUT (status_v2_t,
                       (u8_, enum_<u8_, pump_state>, u16_le_, u8_),
                       ("valve", "state", "flow", "pressure"));
LUMEX_PROTOCOL_LAYOUT (info_t, (text_<8>, u16_le_), ("model", "firmware"));
LUMEX_PROTOCOL_LAYOUT (change_t, (u8_, u16_le_), ("parameter", "value"));
LUMEX_PROTOCOL_LAYOUT (nothing_t, (), ());

// The acknowledgements and the change report are rows of their own,
// written before the rows that name them.
// clang-format off
#define PUMP_ROWS(X, X_AS) \
  X (ACK, 0x06, "Accepted", (), (), (), ()) \
  X (NAK, 0x15, "Refused", (), (), (u8_), ()) \
  X (CHANGE, 0x30, "A parameter changed", (), (), (change_t), ()) \
  X (INFO, 0x10, "Identity", (), (), (info_t), (idempotent)) \
  X (CLOCK, 0x11, "Set the clock", (u32_le_), ("seconds"), (), \
     (idempotent)) \
  X (STATUS, 0x12, "State", (), (), (status_t), \
     (stream (100), edge_event<0, 0, 1> (), edge_event<0, 1, 0> (), \
      edge_event<1, -1, 2> ())) \
  X (START, 0x01, "Start", (u8_), ("channel"), (ack_nak<u8_, refusal_t>), \
     (min_interval (50))) \
  X (STOP, 0x02, "Stop", (), (), (ack_nak<ACK_t, NAK_t>), ()) \
  X (PRIME, 0x03, "Prime", (u16_le_), ("volume"), \
     (two_phase<ACK_t, u16_le_>), (timeout (5000))) \
  X (SET_FLOW, 0x04, "Set the flow", (u16_le_), ("flow"), \
     (echo_set<u16_le_>), ()) \
  X (SET_LIMIT, 0x05, "Set the limit", (u8_), ("limit"), \
     (echo_zero_on_error<u8_>), ()) \
  X (SET_MODE, 0x07, "Set the mode", (enum_<u8_, pump_state>), ("mode"), \
     (state_instead_of_refusal<enum_<u8_, pump_state> >), ()) \
  X (WRITE, 0x08, "Write a parameter", (u8_, u16_le_), \
     ("parameter", "value"), (), (reply_row<CHANGE_t> ())) \
  X (READ, 0x09, "Read a parameter", (u8_), ("parameter"), (u16_le_), \
     (correlate (correlation::by_order))) \
  X (QUERY, 0x0A, "Query by a matcher", (), (), (u8_), \
     (match_with (&match_any))) \
  X (TOUCH, 0x0B, "Touch", (), (), (ack_nak<nothing_t, NAK_t>), ()) \
  X (POLL, 0x0C, "Poll", (), (), (u8_), ()) \
  X (BOTH, 0x0D, "Two parts of one shape", (), (), (ack_nak<u8_, i8_>), ()) \
  X (QUERY_NAMED, 0x0E, "Query, the correlation named", (), (), (u8_), \
     (match_with (&match_any), correlate (correlation::custom))) \
  X (WRITE_NAMED, 0x0F, "Write, the correlation named", (u8_), \
     ("parameter"), (), \
     (reply_row<CHANGE_t> (), correlate (correlation::by_reply_key))) \
  X (SET_STATE, 0x16, "Set the state", (enum_<u8_, pump_state>), \
     ("state"), (echo_zero_on_error<enum_<u8_, pump_state> >), ()) \
  X (SET_GAIN, 0x17, "Set the gain", (u16_le_<scale<std::ratio<1, 100> > >), \
     ("gain"), (echo_zero_on_error<u16_le_<scale<std::ratio<1, 100> > > >), \
     ()) \
  X (SET_RATE, 0x18, "Set the rate", (f32_le_), ("rate"), \
     (echo_zero_on_error<f32_le_>), ())
// The layer replaces the change report that WRITE of the base names, so it
// lists WRITE again: there WRITE names the change report of the layer.
#define PUMP_V2_ROWS(X, X_AS) \
  X (STATUS, 0x12, "State", (), (), (status_v2_t), (stream (50))) \
  X (CHANGE, 0x30, "A parameter changed", (), (), (status_v2_t), ()) \
  X (WRITE, 0x08, "Write a parameter", (u8_, u16_le_), \
     ("parameter", "value"), (), (reply_row<CHANGE_t> ())) \
  X (WRITE_NAMED, 0x0F, "Write, the correlation named", (u8_), \
     ("parameter"), (), \
     (reply_row<CHANGE_t> (), correlate (correlation::by_reply_key)))
// clang-format on

LUMEX_PROTOCOL_DEFINE (pump_t, codec_t, pump_header_t, PUMP_ROWS);
LUMEX_PROTOCOL_EXTEND (pump_v2_t, pump_t, PUMP_V2_ROWS);

/// A reply kind of another layer: the dialogue reports it as other.
struct foreign_reply_t
{
  using lumex_protocol_reply = dsl::reply_tag;
};

struct foreign_vocabulary_t : dialogue::vocabulary_t
{
  using foreign = foreign_reply_t;
};

/// Final, with its protocol options: they are read all the same.
struct final_header_t final
{
  using lumex_vocabulary = foreign_vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
  using dialogue_options = dialogue::options<dialogue::watchdog_ms<700>>;
};

// clang-format off
#define FOREIGN_ROWS(X, X_AS) \
  X (ODD, 0x01, "A reply of another layer", (), (), (foreign), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (foreign_t, codec_t, final_header_t, FOREIGN_ROWS);

/// Protocol options that name no row: the plain member form.
struct fixed_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
  using dialogue_options = dialogue::options<dialogue::watchdog_ms<500>,
                                             dialogue::max_in_flight<4>>;
};

/// No protocol options.
struct plain_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
};

// The words of the registries stay in the vocabulary of the dialogue.
// clang-format off
#define VALVE_PARAMS(P, P_AS) \
  P (OPENING, (0x01), "Opening", (u8_), RW, (limits (0, 100)))
#define SMALL_ROWS(X, X_AS) \
  X (SET_VALVE, 0x20, "Set a valve parameter", \
     (registry_key_<valve_registry, u8_>, \
      registry_value_<valve_registry, 0, u8_>), \
     ("parameter", "value"), (echo_set<u8_>), ())
// clang-format on
LUMEX_PROTOCOL_REGISTRY (valve_registry, VALVE_PARAMS);
LUMEX_PROTOCOL_DEFINE (fixed_t, codec_t, fixed_header_t, SMALL_ROWS);
LUMEX_PROTOCOL_DEFINE (plain_t, codec_t, plain_header_t, SMALL_ROWS);

/// The same acknowledgements in a tokenized text: the key is the first
/// token.
struct text_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  using layout_type = layout::tokenized_text_layout<','>;
};

// clang-format off
#define TEXT_ROWS(X, X_AS) \
  X (GO, "GO", "Go", (u8_), ("speed"), (ack_nak<nothing_t, u8_>), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (text_pump_t, codec_t, text_header_t, TEXT_ROWS);

/// The bytes of a list of values.
inline layout::bytes_t
bytes (std::initializer_list<int> values)
{
  layout::bytes_t out;
  for (int value : values)
    out.push_back (static_cast<std::uint8_t> (value));
  return out;
}

/// A buffer as a view.
inline layout::bytes_view_t
view_of (layout::bytes_t const &bytes)
{
  return layout::bytes_view_t (bytes.data (), bytes.size ());
}
} // namespace dialogue_tables
} // namespace lumex_tests

#endif // !LUMEX_TESTS_APPLIED_TRANSPORT_PROTOCOL_DSL_DIALOGUE_TEST_TABLES_HPP
