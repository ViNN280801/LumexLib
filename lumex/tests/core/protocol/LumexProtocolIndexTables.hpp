// Synthetic tables of the tests of the registries, the run-time index,
// describe, the session context and the exports of lumex/core/protocol: an
// invented "widget" with a "lamp" parameter table. No real device, its
// names, codes or values are in them.
#ifndef LUMEX_TESTS_CORE_PROTOCOL_INDEX_TABLES_HPP
#define LUMEX_TESTS_CORE_PROTOCOL_INDEX_TABLES_HPP

#include <cstdint>
#include <cstring>
#include <ratio>
#include <string>
#include <system_error>
#include <type_traits>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp"

namespace lumex_tests
{
namespace index_tables
{
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace registry = lumex::core::protocol::registry;
namespace index = lumex::core::protocol::index;

LUMEX_DEFINE_REFLECTED_ENUM (widget_mode, std::uint8_t, (idle, 0), (run, 1),
                             (hold, 2))

// The states of the widget (the type of accepted_in).
enum class widget_state
{
  idle,
  running
};

struct codec
{
};

// The message format of the widget: a one-byte key in the first byte of the
// frame, parameter rows through the registry words.
struct widget_header
{
  using lumex_vocabulary = registry::table_vocabulary;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
};

// The same rows as tokenized text: the key is the first token.
struct text_header
{
  using lumex_vocabulary = registry::table_vocabulary;
  using layout_type = layout::tokenized_text_layout<','>;
};

// A header that names neither a key width nor a reader (the layer cannot
// match frames, but key_width_of works).
struct plain_header
{
};

// A binary header that names the reader of the key and no width: the width
// is the widest key type of the table.
struct wide_reader_header
{
  using key_reader = index::key_at<0, 4>;
};

struct pair_reader_header
{
  using key_reader = index::key_at<0, 2>;
};

// The parameter table of the lamp.
// clang-format off
#define LAMP_PARAMS(P, P_AS) \
  P (FREQUENCY, (0x10), "Flash frequency", \
     (u16_be_<scale<std::ratio<1, 10> >, unit::hertz>), RW, \
     (limits (1, 100), firmware (2, 7))) \
  P (LEVEL, (0x11), "Lamp level", (u8_<unit::percent>), R, ()) \
  P (OFFSET, (0x12), "Level offset", \
     (i16_le_<scale<std::ratio<1, 100> > >), RW, \
     (limits (-5, 5), model ("A", "B"))) \
  P_AS (SERIAL, "SN", (0x13), "Serial number", (u32_le_), R, ()) \
  P_AS (REMOVE_ALL, "DELETE", (0x14), "Clear all parameters", (u8_), W, ())
// clang-format on
LUMEX_PROTOCOL_REGISTRY (lamp, LAMP_PARAMS);

// One entry of every kind of key.
// clang-format off
#define MIXED_PARAMS(P, P_AS) \
  P (BY_INDEX, (5), "By index", (u8_), RW, ()) \
  P (BY_PAIR, (typed (3, 5)), "By type and index", (u16_le_), R, ()) \
  P (BY_UNIT, (unit_param (2, 9)), "By unit and parameter", (i8_), W, ()) \
  P (BY_MNEMONIC, ("AZ"), "By mnemonic", (u8_), RW, ()) \
  P (BY_ADDRESS, (modbus (0x0100)), "By register", (u16_be_), RW, ())
// clang-format on
LUMEX_PROTOCOL_REGISTRY (mixed, MIXED_PARAMS);

// clang-format off
#define PAIR_PARAMS(P, P_AS) \
  P (WARM_UP, (typed (3, 5)), "Warm-up time", \
     (u16_le_<scale<std::ratio<1, 10> >, unit::second>), RW, (limits (0, 60))) \
  P (COOL_DOWN, (typed (3, 6)), "Cool-down time", (u16_le_), RW, ())
// clang-format on
LUMEX_PROTOCOL_REGISTRY (timers, PAIR_PARAMS);

// clang-format off
#define MNEMONIC_PARAMS(P, P_AS) \
  P (GAIN, ("GN"), "Gain", (i16_le_<scale<std::ratio<1, 10> > >), RW, \
     (limits (-10, 10))) \
  P (BIAS, ("BS"), "Bias", (u8_), RW, ())
// clang-format on
LUMEX_PROTOCOL_REGISTRY (tuning, MNEMONIC_PARAMS);

// clang-format off
#define MODBUS_PARAMS(P, P_AS) \
  P (HOLD_TIME, (modbus (0x0100)), "Hold time", \
     (u16_be_<unit::second>), RW, (limits (0, 3600))) \
  P (RAMP, (modbus (0x0101)), "Ramp", (i16_be_), RW, ())
// clang-format on
LUMEX_PROTOCOL_REGISTRY (holding, MODBUS_PARAMS);

// A registry with a floating entry (it has no raw value).
// clang-format off
#define FLOAT_PARAMS(P, P_AS) \
  P (RATIO, (1), "Ratio", (f32_le_), RW, ()) \
  P (SIGNED, (2), "Signed magnitude", (sign_magnitude_<i16_le_>), RW, ()) \
  P (BIG, (3), "Wide", (u64_le_), RW, ())
// clang-format on
LUMEX_PROTOCOL_REGISTRY (odd, FLOAT_PARAMS);

// Two entries with the same key: the index refuses the registry.
// clang-format off
#define CLASH_PARAMS(P, P_AS) \
  P (FIRST, (7), "First", (u8_), RW, ()) \
  P (SECOND, (7), "Second", (u8_), RW, ())
// clang-format on
LUMEX_PROTOCOL_REGISTRY (clashing, CLASH_PARAMS);

LUMEX_PROTOCOL_LAYOUT (state_t, (enum_<u8_, widget_mode>, u8_),
                       ("mode", "level"));

LUMEX_PROTOCOL_LAYOUT (
    parameter_t,
    (registry::registry_key_<lamp, u8_>,
     registry::registry_value_<lamp, 0, i32_le_, registry::access_t::read>),
    ("parameter", "value"));

// clang-format off
#define WIDGET_ROWS(X, X_AS) \
  X (PING, 0x01, "Liveness check", (), (), (), (idempotent)) \
  X (SET_SPEED, 0x02, "Set the speed", \
     (u16_le_<scale<std::ratio<1, 10> >, unit::rpm>), ("speed"), (), \
     (timeout (500), retries (2))) \
  X (STATUS, 0x03, "Read the state", (), (), (state_t), \
     (accepted_in (widget_state::idle, widget_state::running))) \
  X (READ_TEMP, 0x04, "Read the temperature", (u8_), ("channel"), \
     (i16_be_<scale<std::ratio<1, 100> >, unit::degree_celsius>), \
     (log_level (level::info))) \
  X_AS (ERASE, "DELETE", 0x05, "Erase the log", (), (), (), (dangerous)) \
  X (TICK, 0x06, "Periodic state", (), (), (state_t), (stream (100))) \
  X (SET_PARAMETER, 0x20, "Write a parameter", \
     (registry_key_<lamp, u8_>, registry_value_<lamp, 0, i32_le_>), \
     ("parameter", "value"), (), ()) \
  X (GET_PARAMETER, 0x21, "Read a parameter", \
     (registry_key_<lamp, u8_>), ("parameter"), (parameter_t), ())
// A newer firmware: the speed has a finer scale, one new row.
#define WIDGET_V2_ROWS(X, X_AS) \
  X (SET_SPEED, 0x02, "Set the speed", \
     (u16_le_<scale<std::ratio<1, 100> >, unit::rpm>), ("speed"), (), \
     (timeout (500), retries (2))) \
  X (FLASH, 0x07, "Flash the lamp", (u8_<unit::count>), ("times"), (), \
     (firmware (2, 9), model ("A")))
#define NO_ROWS(X, X_AS)
// clang-format on

LUMEX_PROTOCOL_DEFINE (widget, codec, widget_header, WIDGET_ROWS);
LUMEX_PROTOCOL_EXTEND (widget_v2, widget, WIDGET_V2_ROWS);
LUMEX_PROTOCOL_EXTEND_FRAMED (widget_text, widget, codec, text_header,
                              NO_ROWS);

// Rows with a text key (a mnemonic), tokenized.
// clang-format off
#define MNEMONIC_ROWS(X, X_AS) \
  X (AZ, "AZ", "Start the wash", (u8_), ("cycles"), (), ()) \
  X (BY, "BY", "Stop", (), (), (u16_le_), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (washer, codec, text_header, MNEMONIC_ROWS);

// A key that is two numbers: type and number, two bytes on the wire.
struct pair_key
{
  using lumex_composite_key = dsl::composite_key_tag;
  std::uint8_t type;
  std::uint8_t number;

  LUMEX_CONSTEXPR_CTOR
  pair_key (std::uint8_t t, std::uint8_t n) : type (t), number (n) {}

  static LUMEX_CONSTEXPR std::size_t
  lumex_key_width ()
  {
    return 2;
  }

  LUMEX_CONSTEXPR std::uint64_t
  lumex_key_bits () const
  {
    return static_cast<std::uint64_t> ((type << 8) | number);
  }
};

struct pair_header
{
  static LUMEX_CONSTEXPR std::size_t key_width = 2;
};

// clang-format off
#define PAIR_ROWS(X, X_AS) \
  X (WARM, pair_key (3, 5), "Warm", (u8_), ("seconds"), (), ()) \
  X (COOL, pair_key (3, 6), "Cool", (), (), (u8_), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (pairs, codec, pair_header, PAIR_ROWS);

// Composite keys on a wire whose width only the key type gives.
// clang-format off
#define PAIR_WIDE_ROWS(X, X_AS) \
  X (WARM, pair_key (3, 5), "Warm", (), (), (), ()) \
  X (HEAT, pair_key (4, 5), "Heat", (), (), (), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (pairs_wide, codec, pair_reader_header, PAIR_WIDE_ROWS);

// A tokenized layer: the wire carries "A" and "65", two keys.
// clang-format off
#define LETTER_ROWS(X, X_AS) \
  X (LETTER, 'A', "Letter", (), (), (), ()) \
  X (NUMBER, 65, "Number", (), (), (), ())
#define SAME_TEXT_ROWS(X, X_AS) \
  X (SIGNED, 7, "Signed", (), (), (), ()) \
  X (UNSIGNED, 7u, "Unsigned", (), (), (), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (letters_text, codec, text_header, LETTER_ROWS);
LUMEX_PROTOCOL_DEFINE (same_text, codec, text_header, SAME_TEXT_ROWS);

// Texts, a scale that is not decimal, a description with several lines.
// clang-format off
#define NOTE_ROWS(X, X_AS) \
  X (NOTE, 0x50, "Note", (token_), ("text"), (), ()) \
  X (LABEL, 0x51, "Label", (text_<8>), ("text"), (), ()) \
  X (THIRD, 0x52, "Third", (u16_le_<scale<std::ratio<1, 3> > >), ("third"), \
     (), ()) \
  X (MULTI, 0x53, "Title\n# Heading\n- item", (), (), (), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (notes, codec, widget_header, NOTE_ROWS);

// Mnemonics of three letters, one differing in the last; a description with
// a line break.
// clang-format off
#define TRIPLE_PARAMS(P, P_AS) \
  P (FIRST_ONE, ("ABC"), "Line one\nLine two", (u8_), RW, ()) \
  P (SECOND_ONE, ("ABD"), "Second", (u8_), RW, ())
// clang-format on
LUMEX_PROTOCOL_REGISTRY (triple, TRIPLE_PARAMS);

// Tables the index refuses (build_index reports the clash).
// clang-format off
#define CLASH_INT_ROWS(X, X_AS) \
  X (ONE, 0x07, "One", (), (), (), ()) \
  X (TWO, 0x07, "Two", (), (), (), ())
#define CLASH_WIDTH_ROWS(X, X_AS) \
  X (LETTER, '\xFF', "A character", (), (), (), ()) \
  X (NUMBER, -1, "An integer", (), (), (), ())
#define CLASH_TEXT_ROWS(X, X_AS) \
  X (FIRST, "AZ", "First", (), (), (), ()) \
  X (SECOND, "AZ", "Second", (), (), (), ())
#define CLASH_DECIMAL_ROWS(X, X_AS) \
  X (NUMERIC, 5, "A number", (), (), (), ()) \
  X (TEXTUAL, "5", "A text", (), (), (), ())
#define CLASH_NAME_ROWS(X, X_AS) \
  X (ALPHA, 1, "One", (), (), (), ()) \
  X_AS (BETA, "ALPHA", 2, "Two", (), (), (), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (clash_int, codec, widget_header, CLASH_INT_ROWS);
LUMEX_PROTOCOL_DEFINE (clash_narrow, codec, widget_header, CLASH_WIDTH_ROWS);
LUMEX_PROTOCOL_DEFINE (clash_wide, codec, wide_reader_header,
                       CLASH_WIDTH_ROWS);
LUMEX_PROTOCOL_DEFINE (clash_text, codec, text_header, CLASH_TEXT_ROWS);
LUMEX_PROTOCOL_DEFINE (clash_decimal, codec, text_header, CLASH_DECIMAL_ROWS);
LUMEX_PROTOCOL_DEFINE (clash_name, codec, widget_header, CLASH_NAME_ROWS);

// A small layer for the exports: every shape of field list in one place.
LUMEX_PROTOCOL_LAYOUT (points_t,
                       (array_<u16_le_, 2>,
                        optional_tail_<u8_, u8_<unit::percent>>),
                       ("pair", "tail"));

// clang-format off
#define GADGET_ROWS(X, X_AS) \
  X (PING, 0x01, "Liveness check", (), (), (), (idempotent)) \
  X (SET_SPEED, 0x02, "Set the speed", \
     (u16_le_<scale<std::ratio<1, 10> >, unit::rpm>), ("speed"), (), \
     (timeout (500), retries (2))) \
  X (STATUS, 0x03, "Read the state | now", (), (), (state_t), \
     (accepted_in (widget_state::idle), \
      log_level (level::debug))) \
  X (POINTS, 0x04, "Read the points", (), (), (points_t), (stream (250))) \
  X_AS (ERASE, "DELETE", 0x05, "Erase the log", (), (), (), \
        (dangerous, firmware (2, 9), model ("A", "B")))
// clang-format on
LUMEX_PROTOCOL_DEFINE (gadget, codec, widget_header, GADGET_ROWS);

// A layer above the core, as the dialogue of applied/transport/protocol adds
// it: a reply kind, an option of its own and a vocabulary that names them.
namespace dialogue
{
template <typename Ack, typename Nak> struct ack_nak
{
  using lumex_protocol_reply = dsl::reply_tag;
};

struct rate_limit_kind
{
};

struct rate_limit_t
{
  using option_kind = rate_limit_kind;
  std::uint32_t milliseconds;

  static char const *
  option_name ()
  {
    return "rate_limit";
  }

  std::int64_t
  view_value () const
  {
    return milliseconds;
  }
};

struct vocabulary : registry::table_vocabulary
{
  template <typename Ack, typename Nak>
  using ack_nak = dialogue::ack_nak<Ack, Nak>;

  static constexpr rate_limit_t
  rate_limit (std::uint32_t milliseconds)
  {
    return rate_limit_t{ milliseconds };
  }
};

struct header
{
  using lumex_vocabulary = vocabulary;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
};

// clang-format off
#define DIALOGUE_ROWS(X, X_AS) \
  X (START, 0x01, "Start", (u8_), ("channel"), (ack_nak<u8_, u16_le_>), \
     (rate_limit (50), timeout (200))) \
  X (STOP, 0x02, "Stop", (), (), (u8_), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (dialogue_protocol, codec, header, DIALOGUE_ROWS);
} // namespace dialogue

// A layer above the core that also fills the hooks of the index: a reply
// kind that describes its message and names itself for the export, options
// with a text, and a vocabulary whose check the index asks for.
namespace hooked
{
// The reply is one field; a value 1 means yes, any other value no.
template <typename Answer> struct yes_no_t
{
  using lumex_protocol_reply = dsl::reply_tag;

  template <typename P, typename Row>
  static std::error_code
  lumex_describe_reply (layout::bytes_view_t payload,
                        field::decode_context_t const *context,
                        index::describe_options_t const &options,
                        std::string &out)
  {
    layout::decode_result_t<typename field::value_of<Answer>::type> const
        decoded
        = layout::decode<typename layout::layout_of<P>::type, Answer> (
            payload, context);
    if (!decoded)
      return decoded.error ();
    out += *decoded == 1 ? "YES " : "NO ";
    index::append_value<Answer> (out, *decoded, options, "answer");
    return std::error_code ();
  }

  static char const *
  lumex_reply_text ()
  {
    return "yes_no (one field)";
  }
};

struct mode_option_t
{
  using option_kind = mode_option_t;
  bool fast;

  static char const *
  option_name ()
  {
    return "mode";
  }

  std::int64_t
  view_value () const
  {
    return fast ? 1 : 0;
  }

  char const *
  view_text () const
  {
    return fast ? "fast" : "slow";
  }
};

// Several steps of one row: one kind per number, one name for all.
template <int N> struct step_option_t
{
  using option_kind = step_option_t;

  static char const *
  option_name ()
  {
    return "step";
  }

  std::int64_t
  view_value () const
  {
    return N;
  }
};

// The check the index asks the vocabulary for: every protocol passes.
template <typename P> struct accepts_t : std::true_type
{
};

struct vocabulary_t : registry::table_vocabulary
{
  template <typename Answer> using yes_no = yes_no_t<Answer>;

  template <int N> using step = step_option_t<N>;

  static constexpr mode_option_t
  mode (bool fast)
  {
    return mode_option_t{ fast };
  }

  template <typename P> using lumex_check_protocol = accepts_t<P>;
};

struct header_t
{
  using lumex_vocabulary = vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
};

// clang-format off
#define HOOKED_ROWS(X, X_AS) \
  X (ASK, 0x01, "Ask", (u8_), ("question"), (yes_no<u8_>), \
     (mode (true), step<1> (), step<2> ())) \
  X (TELL, 0x02, "Tell", (), (), (u8_), (mode (false)))
// clang-format on
LUMEX_PROTOCOL_DEFINE (hooked_protocol, codec, header_t, HOOKED_ROWS);
} // namespace hooked

/// The bytes of a buffer as a view.
inline layout::bytes_view_t
view_of (layout::bytes_t const &bytes)
{
  return layout::bytes_view_t (bytes.data (), bytes.size ());
}

/// The bytes of a text (no terminating NUL) as a buffer.
inline layout::bytes_t
bytes_of (char const *text)
{
  return layout::bytes_t (text, text + std::strlen (text));
}

/// The key byte, then the encoded request of `Row` (the frame of widget).
template <typename P, typename Row, typename... Args>
layout::bytes_t
request_frame (std::uint8_t key, Args const &...args)
{
  layout::bytes_t out;
  out.push_back (key);
  layout::encode_request<P, Row> (out, args...);
  return out;
}
} // namespace index_tables
} // namespace lumex_tests

#endif // !LUMEX_TESTS_CORE_PROTOCOL_INDEX_TABLES_HPP
