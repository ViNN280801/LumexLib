// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// The test table of device F: a device of the same family as device E whose
// state message has three layouts. One layout is chosen by a value the
// session learned earlier (the type of the device from the identity reply),
// another by a field of the message itself (the node), temperatures are
// fixed-point numbers (hundredths), and the identity reply has its two
// version bytes in the other order than the family default.

#ifndef LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_F_TABLE_HPP
#define LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_F_TABLE_HPP

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <ratio>

#include "lumex/core/protocol/LumexProtocol"

namespace lumex_tests
{
namespace tables_f
{
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace registry = lumex::core::protocol::registry;
namespace index = lumex::core::protocol::index;

struct codec
{
};

/// The message format of the family: [address][message][data...].
struct family_header
{
  typedef index::key_at<1, 1> key_reader;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
};

/// The session value that chooses the layout of the state message: 0 for the
/// older firmware, 1 for the newer (the session sets it from the type of the
/// identity reply).
LUMEX_PROTOCOL_CONTEXT_KEY (thermo_layout);

/// The identity reply of the family: model, software version, sub-version,
/// hardware version, build date, hardware mask.
LUMEX_PROTOCOL_LAYOUT (info_t, (u8_, u8_, u8_, u8_, u8_, u8_, u8_, u8_),
                       ("type", "sw", "sub", "hw", "year", "month", "day",
                        "mask"));

/// The identity reply of the device: the sub-version comes before the
/// software version.
LUMEX_PROTOCOL_LAYOUT (info_swapped_t,
                       (u8_, u8_, u8_, u8_, u8_, u8_, u8_, u8_),
                       ("type", "sub", "sw", "hw", "year", "month", "day",
                        "mask"));

LUMEX_PROTOCOL_LAYOUT (counters_t, (i16_le_, i16_le_, i16_le_, i16_le_),
                       ("c0", "c1", "c2", "c3"));

// clang-format off
#define FAMILY_F_ROWS(X, X_AS) \
  X (query_ad, 0xAD /*@r04*/, "Query the identity record", (), (), (info_t), \
     (idempotent)) \
  X (debug_8e, 0x8E /*@r04*/, "Report the debug counters", (), (), (counters_t), \
     (stream (600)))
// clang-format on
LUMEX_PROTOCOL_DEFINE (family_f, codec, family_header, FAMILY_F_ROWS);

/// The temperatures of every layout: hundredths of a degree.
LUMEX_PROTOCOL_LAYOUT (
    temperatures_t,
    (i16_le_<scale<std::ratio<1, 100>>, unit::degree_celsius>,
     i16_le_<scale<std::ratio<1, 100>>, unit::degree_celsius>,
     i16_le_<scale<std::ratio<1, 100>>, unit::degree_celsius>),
    ("target", "work", "room"));

/// The state of the older firmware: the extra words are present when the
/// message continues after the temperatures.
LUMEX_PROTOCOL_LAYOUT (
    old_state_t,
    (u8_, u8_, temperatures_t,
     optional_tail_<u16_le_<unit::second>, i16_le_, i16_le_, i16_le_>),
    ("mode", "state", "temperatures", "tail"));

/// The state of a node near the main one (the first two nodes).
LUMEX_PROTOCOL_LAYOUT (near_node_t,
                       (u16_le_<unit::second>, u16_le_, u32_le_<unit::second>),
                       ("mode_time", "valve", "device_time"));

/// The state of an additional node (the other nodes).
LUMEX_PROTOCOL_LAYOUT (far_node_t, (i16_le_, u8_, u8_),
                       ("state", "mode", "error"));

/// The state of the newer firmware: the node is both a field and the
/// discriminant of the rest.
// clang-format off
LUMEX_PROTOCOL_LAYOUT (new_state_t,
  (u8_, u8_, temperatures_t,
   union_by_<field_<0>,
             case_<0x15 /*@r04*/, near_node_t>,
             case_<0xAD /*@r04*/, near_node_t>,
             case_<0x65 /*@r04*/, far_node_t>,
             case_<0x3F /*@r04*/, far_node_t>,
             case_<0xD1 /*@r04*/, far_node_t> >),
  ("node", "node_mode", "temperatures", "rest"));

LUMEX_PROTOCOL_LAYOUT (state_t,
  (union_by_context_<thermo_layout,
                     case_<0, old_state_t>,
                     case_<1, new_state_t> >),
  ("state"));

#define DEVICE_F_ROWS(X, X_AS) \
  X (query_ad, 0xAD /*@r04*/, "Query the identity record", (), (), (info_swapped_t), \
     (idempotent)) \
  X (set_65, 0x65 /*@r04*/, "Set the working mode", \
     (i16_le_, i16_le_<scale<std::ratio<1, 100> >, unit::degree_celsius>), \
     ("on", "temperature"), (), (timeout (800), retries (3))) \
  X (report_d9, 0xD9 /*@r04*/, "Report the state", (), (), (state_t), \
     (stream (1000))) \
  X (error_44, 0x44 /*@r04*/, "Report an error", (), (), (u8_), ()) \
  X (query_b8, 0xB8 /*@r04*/, "Query a node", (u8_), ("node"), (), ())
// clang-format on
LUMEX_PROTOCOL_EXTEND (device_f, family_f, DEVICE_F_ROWS);

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

/// The payload of a frame: the bytes after the address and the key.
inline layout::bytes_view_t
payload_of (layout::bytes_t const &frame)
{
  return layout::bytes_view_t (frame.data () + 2, frame.size () - 2);
}
} // namespace tables_f
} // namespace lumex_tests

#endif // !LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_F_TABLE_HPP
