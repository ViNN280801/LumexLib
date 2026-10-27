// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// The test table of device J: one logical protocol in two encodings. j_text
// is the text encoding (every byte is two hexadecimal digits, the frame
// around the digits has a start character, a checksum over the characters
// and a carriage return, which are the framing stages' work, not the table's);
// j_hid is the binary encoding of the same commands in a report (a length
// byte, then the command and the bytes). The rows are written once, in j_text,
// and j_hid re-binds them to the binary layout.
//
// The two nibbles of the status byte of a reply have a meaning of their own
// (the result of receiving the frame and the state of the device), values
// are signed-magnitude 32-bit numbers, some bytes are counted in units of 100
// microliters, 16-bit numbers are big-endian.

#ifndef LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_J_TABLE_HPP
#define LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_J_TABLE_HPP

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <ratio>
#include <string>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp"

namespace lumex_tests
{
namespace tables_j
{
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace registry = lumex::core::protocol::registry;
namespace index = lumex::core::protocol::index;

/// The result of receiving a frame (the high nibble of the status byte).
LUMEX_DEFINE_REFLECTED_ENUM (rx_result, std::uint8_t, (r_0, 0), (r_1, 1),
                             (r_2, 2), (r_3, 3), (r_4, 4))

/// The state of the device (the low nibble of the status byte).
LUMEX_DEFINE_REFLECTED_ENUM (device_state, std::uint8_t, (s_0, 0), (s_1, 1),
                             (s_2, 2), (s_3, 3), (s_4, 4), (s_5, 5), (s_6, 6),
                             (s_7, 7), (s_8, 8))

struct codec
{
};

/// The text encoding: the key is the first two digits.
struct text_header
{
  typedef layout::fixed_width_text_layout<16> layout_type;
  typedef index::key_at_digits<0, 2, 16> key_reader;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
};

/// The binary encoding: [length][command][data...], the key is the second
/// byte.
struct hid_header
{
  typedef index::key_at<1, 1> key_reader;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
};

/// The status byte: the two nibbles.
LUMEX_PROTOCOL_LAYOUT (status_parts_t,
                       (bits_<u8_, 4, 4, rx_result>,
                        bits_<u8_, 0, 4, device_state>),
                       ("result", "state"));

/// A reply that carries nothing but the status byte.
LUMEX_PROTOCOL_LAYOUT (status_t, (split_<u8_, status_parts_t>), ("status"));

/// The version of the firmware.
LUMEX_PROTOCOL_LAYOUT (version_t, (u8_, u8_), ("major", "minor"));

LUMEX_PROTOCOL_LAYOUT (version_reply_t,
                       (split_<u8_, status_parts_t>, version_t),
                       ("status", "version"));

/// The settings: seven small numbers, four corrections (signed magnitude),
/// the serial number and ten volumes counted in units of 100 microliters.
// clang-format off
LUMEX_PROTOCOL_LAYOUT (settings_t,
  (array_<u8_, 7>,
   array_<sign_magnitude_<i32_be_>, 4>,
   u16_be_,
   array_<u8_<scale<std::ratio<100, 1> >, unit::microliter>, 10>),
  ("small", "corrections", "serial", "volumes"));
// clang-format on

LUMEX_PROTOCOL_LAYOUT (settings_reply_t,
                       (split_<u8_, status_parts_t>, settings_t),
                       ("status", "settings"));

// clang-format off
#define DEVICE_J_ROWS(X, X_AS) \
  X (report_ac, 0xAC /*@r04*/, "Report the state", (), (), (status_t), \
     (idempotent)) \
  X (param_get_8d, 0x8D /*@r04*/, "Read the settings", (), (), (settings_reply_t), \
     (idempotent, timeout (6000))) \
  X (param_set_c9, 0xC9 /*@r04*/, "Write the settings", (settings_t), ("settings"), \
     (status_t), (dangerous)) \
  X (start_f4, 0xF4 /*@r04*/, "Run the cleaning cycle", (u16_be_, u16_be_), \
     ("first", "second"), (status_t), ()) \
  X (start_64, 0x64 /*@r04*/, "Run the load cycle", (u8_), ("position"), (status_t), ()) \
  X (stop_35, 0x35 /*@r04*/, "Stop", (), (), (status_t), ()) \
  X (query_3f, 0x3F /*@r04*/, "Query the version", (), (), (version_reply_t), \
     (idempotent))
#define NO_ROWS(X, X_AS)
// clang-format on
LUMEX_PROTOCOL_DEFINE (j_text, codec, text_header, DEVICE_J_ROWS);
LUMEX_PROTOCOL_EXTEND_FRAMED (j_hid, j_text, codec, hid_header, NO_ROWS);

/// The bytes of a list as a buffer.
inline layout::bytes_t
bytes (std::initializer_list<int> values)
{
  layout::bytes_t out;
  for (int value : values)
    out.push_back (static_cast<std::uint8_t> (value));
  return out;
}

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

/// The XOR of the characters of a text as two upper-case hexadecimal digits:
/// the checksum of the text encoding counts the characters, not the bytes
/// they stand for.
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

/// A byte buffer as upper-case hexadecimal digits.
inline std::string
hex_of (layout::bytes_t const &data)
{
  static char const digits[] = "0123456789ABCDEF";
  std::string out;
  for (std::size_t i = 0; i < data.size (); ++i)
    {
      out += digits[(data[i] >> 4) & 0x0F];
      out += digits[data[i] & 0x0F];
    }
  return out;
}
} // namespace tables_j
} // namespace lumex_tests

#endif // !LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_J_TABLE_HPP
