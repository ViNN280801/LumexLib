// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// The test table of device K: text with fields of fixed width in hexadecimal
// digits, the frame [code][error][state][length][fields...] (the start
// character, the XOR of the characters and the carriage return around it are
// the framing stages' work). The codes are those of device J (one family of
// protocols), the header is longer, the state codes are texts, the length is a
// field of the frame, and the error byte of the zone has two nibbles.

#ifndef LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_K_TABLE_HPP
#define LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_K_TABLE_HPP

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <ratio>
#include <string>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp"

namespace lumex_tests
{
namespace tables_k
{
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace registry = lumex::core::protocol::registry;
namespace index = lumex::core::protocol::index;

/// The state of the valve, one character.
LUMEX_DEFINE_REFLECTED_ENUM (valve_state, char, (v_a, '2' /*@r11*/),
                             (v_b, '3' /*@r11*/), (v_c, '1' /*@r11*/))

/// The state of the zone conditioning.
LUMEX_DEFINE_REFLECTED_ENUM (zone_state, std::uint8_t, (t_15, 0x15 /*@r04*/),
                             (t_ad, 0xAD /*@r04*/), (t_65, 0x65 /*@r04*/),
                             (t_3f, 0x3F /*@r04*/))

struct codec
{
};

/// The message format: digits of fixed width in hexadecimal, the key is the
/// first two digits.
struct text_header
{
  typedef layout::fixed_width_text_layout<16> layout_type;
  typedef index::key_at_digits<0, 2, 16> key_reader;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
};

/// What follows the code in every frame: the error, the state and the length
/// (the number of characters from the code up to and including the checksum).
LUMEX_PROTOCOL_LAYOUT (head_t, (u16_be_, u16_be_, u16_be_),
                       ("error", "state", "length"));

/// The two nibbles of the error byte of the zone.
LUMEX_PROTOCOL_LAYOUT (zone_error_t, (bits_<u8_, 4, 4>, bits_<u8_, 0, 4>),
                       ("high", "low"));

// clang-format off
LUMEX_PROTOCOL_LAYOUT (state_t,
  (head_t,
   u24_be_,
   u16_be_,
   text_<2>,
   text_<2>,
   u8_,
   array_<u8_, 2>,
   enum_<char_, valve_state>,
   array_<u8_, 3>,
   u8_,
   array_<u8_, 4>,
   array_<u8_, 2>,
   u16_be_<scale<std::ratio<100, 1> >, unit::microliter>,
   u8_,
   array_<u16_be_<unit::second>, 2>,
   enum_<u8_, zone_state>,
   split_<u8_, zone_error_t>,
   array_<u16_be_<scale<std::ratio<1, 10> >, unit::degree_celsius>, 2>),
  ("head", "total", "fault", "step", "last_step", "method", "positions",
   "valve", "slots", "cycles_left", "lines", "flags", "volume",
   "mode", "times", "zone", "zone_error", "temperatures"));

LUMEX_PROTOCOL_LAYOUT (serial_reply_t, (head_t, u16_be_),
                       ("head", "serial"));

#define DEVICE_K_ROWS(X, X_AS) \
  X (report_ac, 0xAC /*@r04*/, "Report the state", (head_t), ("head"), (state_t), \
     (idempotent, timeout (7000))) \
  X (start_f4, 0xF4 /*@r04*/, "Run the cleaning cycle", (head_t, u8_, u16_be_), \
     ("head", "liquid", "volume"), (head_t), ()) \
  X (start_64, 0x64 /*@r04*/, "Run the load cycle", \
     (head_t, u8_, u8_, array_<u8_, 3>, u8_, u8_, u8_, u8_), \
     ("head", "method", "position", "slots", "before", "after", "sync", \
      "zone"), \
     (head_t), ()) \
  X (stop_35, 0x35 /*@r04*/, "Stop", (head_t), ("head"), (head_t), ()) \
  X (param_set_c9, 0xC9 /*@r04*/, "Write the zone settings", \
     (head_t, array_<u8_, 14>, \
      u16_be_<scale<std::ratio<1, 10> >, unit::degree_celsius>, u16_be_), \
     ("head", "block", "temperature", "reserved"), (head_t), (dangerous)) \
  X (query_ea, 0xEA /*@r04*/, "Query the serial number", (head_t), ("head"), \
     (serial_reply_t), (idempotent))
// clang-format on
LUMEX_PROTOCOL_DEFINE (device_k, codec, text_header, DEVICE_K_ROWS);

/// The bytes of a text as a buffer.
inline layout::bytes_t
bytes_of (std::string const &text)
{
  return layout::bytes_t (text.begin (), text.end ());
}

/// The bytes of a buffer as a view.
inline layout::bytes_view_t
view_of (layout::bytes_t const &data)
{
  return layout::bytes_view_t (data.data (), data.size ());
}

/// The XOR of the characters of a text as two upper-case hexadecimal digits.
inline std::string
xor_text (std::string const &text)
{
  unsigned value = 0;
  for (std::size_t i = 0; i < text.size (); ++i)
    value ^= static_cast<unsigned char> (text[i]);
  static char const digits[] = "0123456789ABCDEF";
  std::string out;
  out += digits[(value >> 4) & 0x0F];
  out += digits[value & 0x0F];
  return out;
}
} // namespace tables_k
} // namespace lumex_tests

#endif // !LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_K_TABLE_HPP
