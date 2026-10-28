// Synthetic layouts and protocols for the tests of the field types and the
// layouts of lumex/core/protocol: an invented "gauge" device, no real
// protocol. The sink_t layout has 40 fields and every field kind of the
// module (the compile-time measurement and the property tests use it).

#ifndef LUMEX_TESTS_CORE_PROTOCOL_FIELD_TABLES_HPP
#define LUMEX_TESTS_CORE_PROTOCOL_FIELD_TABLES_HPP

#include <cstdint>
#include <cstring>
#include <ratio>
#include <string>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp"

namespace lumex_tests
{
namespace field_tables
{
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;

LUMEX_DEFINE_REFLECTED_ENUM (gauge_mode, std::uint8_t, (idle, 0), (run, 1),
                             (hold, 5))

enum class gauge_letter : char
{
  start = 'S',
  rest = 'R'
};

LUMEX_PROTOCOL_CONTEXT_KEY (gauge_kind);

struct codec
{
};

// The keys of the gauge are ints: four bytes on the wire.
struct binary_header
{
  static LUMEX_CONSTEXPR std::size_t key_width = 4;
};

struct hex_header
{
  using layout_type = layout::fixed_width_text_layout<16>;
  static LUMEX_CONSTEXPR std::size_t key_width = 4;
};

struct decimal_header
{
  using layout_type = layout::fixed_width_text_layout<10>;
};

struct text_header
{
  using layout_type = layout::tokenized_text_layout<','>;
};

LUMEX_PROTOCOL_LAYOUT (point_t, (i16_le_, u8_), ("x", "flags"));

// The inner protocol of nested_: messages with a character key.
// clang-format off
#define INNER_ROWS(X, X_AS) \
  X (LAMP, 'L', "Lamp level", (u8_, u16_be_), ("lamp", "level"), (), ()) \
  X (VALVE, 'V', "Valve state", (u8_), ("valve"), (), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (inner_protocol, codec, binary_header, INNER_ROWS);

LUMEX_PROTOCOL_LAYOUT (
    short_tail_t,
    (u8_, union_by_<at_end_<u8_>, case_<1, u16_le_>, case_<2, u8_>>),
    ("tag", "item"));

LUMEX_PROTOCOL_LAYOUT (long_tail_t, (u16_be_, nested_<inner_protocol, char_>),
                       ("code", "inner"));

// clang-format off
LUMEX_PROTOCOL_LAYOUT (sink_t,
  (u8_,
   i8_,
   u16_le_<scale<std::ratio<1, 100> >, unit::degree_celsius>,
   u16_be_,
   i16_le_,
   i16_be_<scale<std::ratio<1, 10> > >,
   u24_le_,
   i24_be_,
   u32_le_,
   i32_be_<scale<std::ratio<1, 1000> >, unit::millivolt>,
   u64_be_,
   i64_le_,
   f32_le_<unit::nanometer>,
   f64_be_,
   sign_magnitude_<i32_be_>,
   sign_magnitude_<i16_le_<scale<std::ratio<1, 100> > > >,
   ascii_hex_<4>,
   ascii_hex_<2, scale<std::ratio<1, 10> > >,
   dec_text_<6, 3>,
   char_,
   text_<8>,
   bits_<u8_, 0, 4>,
   mask_<u8_, 0x55>,
   split_<u16_le_, bits_<u16_le_, 0, 12>, bits_<u16_le_, 12, 4> >,
   enum_<u8_, gauge_mode>,
   enum_<char_, gauge_letter>,
   array_<u16_le_, 3>,
   array_<point_t, 2>,
   counted_<u8_, u8_, 8>,
   u8_,
   computed_count_<i16_be_, count_from_<29>, 6>,
   u8_,
   union_by_<field_<31>, case_<1, point_t>, case_<2, u32_le_> >,
   union_by_<u8_, case_<7, point_t>, case_<9, text_<4> > >,
   union_by_context_<gauge_kind, case_<1, u8_>, case_<2, u16_be_> >,
   bits_<u16_be_, 3, 5, std::int8_t>,
   mask_<u8_, 0xAA>,
   point_t,
   u8_<unit::percent>,
   optional_tail_<u16_le_, by_length_<6, short_tail_t, long_tail_t> >),
  ("u8", "i8", "temperature", "u16be", "i16le", "i16be", "u24le", "i24be",
   "u32le", "voltage", "u64be", "i64le", "wavelength", "f64be", "sm32",
   "sm16", "hex4", "hex2", "dec", "letter", "name", "low_nibble", "even",
   "split", "mode", "code", "triple", "points", "counted", "count",
   "computed", "selector", "by_field", "by_lead", "by_context", "signed_bits",
   "odd", "point", "percent", "tail"));
// clang-format on

// The same logical commands in four layers: binary, hexadecimal and decimal
// fixed-width text, tokenized text.
// clang-format off
#define GAUGE_ROWS(X, X_AS) \
  X (PING, 0x01, "Liveness", (), (), (), ()) \
  X (SET_LEVEL, 0x02, "Set the level", \
     (u16_be_<scale<std::ratio<1, 100> >, unit::microliter>, u8_, i32_le_), \
     ("volume", "channel", "offset"), (point_t), (timeout (100))) \
  X (READ_SINK, 0x03, "Read everything", (), (), (sink_t), ()) \
  X (READ_WORD, 0x04, "Read one word", (u8_), ("index"), (u16_le_), ()) \
  X_AS (SET_NAME, "NM", "NM", "Set the name", (text_<6>, dec_text_<4, 1>), \
        ("name", "factor"), (), ())
#define NO_ROWS(X, X_AS)
// clang-format on

LUMEX_PROTOCOL_DEFINE (gauge, codec, binary_header, GAUGE_ROWS);
LUMEX_PROTOCOL_EXTEND_FRAMED (gauge_hex, gauge, codec, hex_header, NO_ROWS);
LUMEX_PROTOCOL_EXTEND_FRAMED (gauge_decimal, gauge, codec, decimal_header,
                              NO_ROWS);
LUMEX_PROTOCOL_EXTEND_FRAMED (gauge_text, gauge, codec, text_header, NO_ROWS);

/// A session that knows gauge_kind = `kind` (or nothing when `known` is
/// false).
struct test_session
{
  std::int64_t kind;
  bool known;

  static bool
  lookup (void const *state, char const *key, std::int64_t &value)
  {
    test_session const *const self = static_cast<test_session const *> (state);
    if (!self->known || std::strcmp (key, "gauge_kind") != 0)
      return false;
    value = self->kind;
    return true;
  }

  field::decode_context_t
  context () const
  {
    field::decode_context_t const result = { this, &test_session::lookup };
    return result;
  }
};

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

/// A buffer as a string (for the text layouts).
inline std::string
text_of (layout::bytes_t const &bytes)
{
  return std::string (bytes.begin (), bytes.end ());
}
} // namespace field_tables
} // namespace lumex_tests

#endif // !LUMEX_TESTS_CORE_PROTOCOL_FIELD_TABLES_HPP
