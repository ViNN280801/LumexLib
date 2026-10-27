// bench_protocol_fields.hpp
// The layouts of compile_time.cpp for BENCH_PROTOCOL_TABLE 4 and 5: a layout
// of 40 fields with every field kind of lumex/core/protocol (the sink layout
// of the module tests,
// lumex/tests/core/protocol/LumexProtocolFieldTables.hpp), for an invented
// device. Table 5 also encodes and decodes it in the binary, hexadecimal and
// tokenized layouts.
#ifndef LUMEX_BENCHMARKS_PROTOCOL_FIELDS_HPP
#define LUMEX_BENCHMARKS_PROTOCOL_FIELDS_HPP

#include <cstdint>
#include <ratio>

#include "lumex/core/protocol/LumexProtocol"

namespace bench_fields
{
enum class mode : std::uint8_t
{
  idle,
  run
};

enum class letter : char
{
  start = 'S',
  rest = 'R'
};

LUMEX_PROTOCOL_CONTEXT_KEY (kind);

struct codec
{
};

struct header
{
};

LUMEX_PROTOCOL_LAYOUT (point_t, (i16_le_, u8_), ("x", "flags"));

// clang-format off
#define INNER_ROWS(X, X_AS) \
  X (LAMP, 'L', "Lamp level", (u8_, u16_be_), ("lamp", "level"), (), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (inner_protocol, codec, header, INNER_ROWS);

LUMEX_PROTOCOL_LAYOUT (
    short_tail_t,
    (u8_, union_by_<at_end_<u8_>, case_<1, u16_le_>, case_<2, u8_>>),
    ("tag", "item"));

LUMEX_PROTOCOL_LAYOUT (long_tail_t, (u16_be_, nested_<inner_protocol, char_>),
                       ("code", "inner"));

// clang-format off
LUMEX_PROTOCOL_LAYOUT (sink_t,
  (u8_, i8_, u16_le_<scale<std::ratio<1, 100> >, unit::degree_celsius>,
   u16_be_, i16_le_, i16_be_<scale<std::ratio<1, 10> > >, u24_le_, i24_be_,
   u32_le_, i32_be_<scale<std::ratio<1, 1000> >, unit::millivolt>, u64_be_,
   i64_le_, f32_le_<unit::nanometer>, f64_be_, sign_magnitude_<i32_be_>,
   sign_magnitude_<i16_le_<scale<std::ratio<1, 100> > > >, ascii_hex_<4>,
   ascii_hex_<2, scale<std::ratio<1, 10> > >, dec_text_<6, 3>, char_,
   text_<8>, bits_<u8_, 0, 4>, mask_<u8_, 0x55>,
   split_<u16_le_, bits_<u16_le_, 0, 12>, bits_<u16_le_, 12, 4> >,
   enum_<u8_, mode>, enum_<char_, letter>, array_<u16_le_, 3>,
   array_<point_t, 2>, counted_<u8_, u8_, 8>, u8_,
   computed_count_<i16_be_, count_from_<29>, 6>, u8_,
   union_by_<field_<31>, case_<1, point_t>, case_<2, u32_le_> >,
   union_by_<u8_, case_<7, point_t>, case_<9, text_<4> > >,
   union_by_context_<kind, case_<1, u8_>, case_<2, u16_be_> >,
   bits_<u16_be_, 3, 5, std::int8_t>, mask_<u8_, 0xAA>, point_t,
   u8_<unit::percent>,
   optional_tail_<u16_le_, by_length_<6, short_tail_t, long_tail_t> >),
  ("u8", "i8", "temperature", "u16be", "i16le", "i16be", "u24le", "i24be",
   "u32le", "voltage", "u64be", "i64le", "wavelength", "f64be", "sm32",
   "sm16", "hex4", "hex2", "dec", "letter", "name", "low_nibble", "even",
   "split", "mode", "code", "triple", "points", "counted", "count",
   "computed", "selector", "by_field", "by_lead", "by_context", "signed_bits",
   "odd", "point", "percent", "tail"));
// clang-format on
} // namespace bench_fields

#endif // !LUMEX_BENCHMARKS_PROTOCOL_FIELDS_HPP
