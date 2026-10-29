// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// The test table of device G: a device of the same family as devices E and F
// whose state byte holds two interleaved bit masks (one bit per channel for
// "running" and one for "fault"), an identity reply that is only a serial
// number when the body is four bytes, a statistics message chosen by its first
// byte, and a second, older protocol of the same device (g_old) with
// single-character command keys and a length byte in front.

#ifndef LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_G_TABLE_HPP
#define LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_G_TABLE_HPP

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <ratio>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp"

namespace lumex_tests
{
namespace tables_g
{
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace registry = lumex::core::protocol::registry;
namespace index = lumex::core::protocol::index;

/// The two working modes of the device.
LUMEX_DEFINE_REFLECTED_ENUM (power_mode, std::uint8_t, (m_15, 0x15 /*@r04*/),
                             (m_3f, 0x3F /*@r04*/))

struct codec
{
};

/// The message format of the family: [address][message][data...].
struct family_header
{
  using key_reader = index::key_at<1, 1>;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
};

/// The identity reply: the full record or, from a network device, only the
/// serial number (four bytes).
LUMEX_PROTOCOL_LAYOUT (serial_t, (i32_le_), ("serial"));
LUMEX_PROTOCOL_LAYOUT (record_t, (u8_, u8_, u8_, u8_, u8_, u8_, u8_, u8_),
                       ("type", "sw", "sub", "hw", "year", "month", "day",
                        "mask"));
LUMEX_PROTOCOL_LAYOUT (identity_t, (by_length_<5, serial_t, record_t>),
                       ("identity"));

// clang-format off
#define FAMILY_G_ROWS(X, X_AS) \
  X (query_ad, 0xAD /*@r04*/, "Query the identity record", (), (), (identity_t), \
     (idempotent))
// clang-format on
LUMEX_PROTOCOL_DEFINE (family_g, codec, family_header, FAMILY_G_ROWS);

/// The two interleaved masks of the state byte: the even bits say "running",
/// the odd bits say "fault", one bit of each per channel.
LUMEX_PROTOCOL_LAYOUT (channels_t, (mask_<u8_, 0x55>, mask_<u8_, 0xAA>),
                       ("running", "fault"));

LUMEX_PROTOCOL_LAYOUT (state_t, (u8_, u8_, split_<u8_, channels_t>),
                       ("node", "mode", "channels"));

LUMEX_PROTOCOL_LAYOUT (cpu_t,
                       (u16_le_<scale<std::ratio<1, 10>>, unit::percent>,
                        u32_le_),
                       ("load", "switches"));
LUMEX_PROTOCOL_LAYOUT (stack_t, (u8_, u8_, u16_le_),
                       ("task", "priority", "used"));

LUMEX_PROTOCOL_LAYOUT (counters_t, (i16_le_, i16_le_, i16_le_, i16_le_),
                       ("c0", "c1", "c2", "c3"));

// clang-format off
LUMEX_PROTOCOL_LAYOUT (statistics_t,
  (union_by_<u8_, case_<0xAD /*@r04*/, cpu_t>, case_<0x65 /*@r04*/, stack_t> >),
  ("statistics"));

#define DEVICE_G_ROWS(X, X_AS) \
  X (set_65, 0x65 /*@r04*/, "Set the working mode", (enum_<u8_, power_mode>), \
     ("mode"), (), (dangerous)) \
  X (report_d9, 0xD9 /*@r04*/, "Report the state", (), (), (state_t), \
     (stream (1000))) \
  X (error_44, 0x44 /*@r04*/, "Report an error", (), (), (u8_), ()) \
  X (query_b8, 0xB8 /*@r04*/, "Query a node", (u8_), ("node"), (), ()) \
  X (report_ab, 0xAB /*@r04*/, "Report statistics", (), (), (statistics_t), \
     (stream (6000))) \
  X (debug_8e, 0x8E /*@r04*/, "Report the debug counters", (), (), \
     (counters_t), (stream (600)))
// clang-format on
LUMEX_PROTOCOL_EXTEND (device_g, family_g, DEVICE_G_ROWS);

/// The older protocol of the device: [length][command][arguments], the
/// length counts the command and the arguments, the commands are letters.
struct old_header
{
  // The length byte is the length-prefix codec's: the message starts with
  // the command.
  using key_reader = index::key_at<0, 1>;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
};

/// The state of the two groups: bit 7 is the first group, bit 6 the second.
LUMEX_PROTOCOL_LAYOUT (groups_t,
                       (bits_<u8_, 7, 1, bool>, bits_<u8_, 6, 1, bool>),
                       ("first", "second"));

// clang-format off
#define OLD_G_ROWS(X, X_AS) \
  X (letter_a, 'Q' /*@r11*/, "Letter command A", (), (), \
     (split_<u8_, groups_t>), (idempotent)) \
  X (letter_b, 'H' /*@r11*/, "Letter command B", (split_<u8_, groups_t>), \
     ("groups"), (), (dangerous))
// clang-format on
LUMEX_PROTOCOL_DEFINE (old_g, codec, old_header, OLD_G_ROWS);

/// The bytes of a list as a buffer.
inline layout::bytes_t
bytes (std::initializer_list<int> values)
{
  layout::bytes_t out;
  for (int value : values)
    out.push_back (static_cast<std::uint8_t> (value));
  return out;
}

/// The bytes of a buffer as a view.
inline layout::bytes_view_t
view_of (layout::bytes_t const &data)
{
  return layout::bytes_view_t (data.data (), data.size ());
}

/// The payload of a frame: the bytes after the address (or the length) and
/// the key.
inline layout::bytes_view_t
payload_of (layout::bytes_t const &frame)
{
  return layout::bytes_view_t (frame.data () + 2, frame.size () - 2);
}
} // namespace tables_g
} // namespace lumex_tests

#endif // !LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_G_TABLE_HPP
