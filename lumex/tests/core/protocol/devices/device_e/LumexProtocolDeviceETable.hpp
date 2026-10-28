// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// The test table of device E: a device of a family whose messages are
// [address][message][data...], with a layer of family rows below the rows of
// the device, a mode message whose layout is chosen by its first byte, a
// parameter table addressed by a pair (node, parameter), a state byte with
// two nibbles, and a tail the older firmware does not send.

#ifndef LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_E_TABLE_HPP
#define LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_E_TABLE_HPP

#include <cstddef>
#include <cstdint>
#include <ratio>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp"

namespace lumex_tests
{
namespace tables_e
{
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace registry = lumex::core::protocol::registry;
namespace index = lumex::core::protocol::index;

/// The states the device manager reports.
LUMEX_DEFINE_REFLECTED_ENUM (manager_state, std::uint8_t,
                             (st_35, 0x35 /*@r04*/), (st_f4, 0xF4 /*@r04*/),
                             (st_90, 0x90 /*@r04*/), (st_f3, 0xF3 /*@r04*/),
                             (st_47, 0x47 /*@r04*/), (st_e0, 0xE0 /*@r04*/),
                             (st_d9, 0xD9 /*@r04*/), (st_44, 0x44 /*@r04*/))

struct codec
{
};

/// The message format of the family: the key is the second byte, after the
/// address; the fields follow it.
struct family_header
{
  using lumex_vocabulary = registry::table_vocabulary;
  using key_reader = index::key_at<1, 1>;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
};

/// The identity record: model, versions, build date, the hardware bits (one
/// bit says there are two units) and the serial number.
LUMEX_PROTOCOL_LAYOUT (config_bits_t, (bits_<u8_, 1, 1, bool>), ("two_units"));
LUMEX_PROTOCOL_LAYOUT (info_t,
                       (u8_, u8_, u8_, u8_, u8_, u8_, u8_,
                        split_<u8_, config_bits_t>, i32_le_),
                       ("type", "sw", "sub", "hw", "year", "month", "day",
                        "config", "serial"));

/// A state of a node: the node and its state byte.
LUMEX_PROTOCOL_LAYOUT (node_state_t, (u8_, u8_), ("node", "state"));

// clang-format off
#define FAMILY_E_ROWS(X, X_AS) \
  X (query_ad, 0xAD /*@r04*/, "Query the identity record", (), (), (info_t), \
     (idempotent)) \
  X (report_d9, 0xD9 /*@r04*/, "Report the state of a node", (), (), (node_state_t), \
     (stream (1000)))
// clang-format on
LUMEX_PROTOCOL_DEFINE (family_e, codec, family_header, FAMILY_E_ROWS);

// The drive parameters: a pair (node, parameter) is the key, the value is a
// 16-bit number with a scale of its own.
// clang-format off
#define DRIVE_PARAMS(P, P_AS) \
  P (p_65_ad, (unit_param (0x65 /*@r04*/, 0xAD /*@r04*/)), "Drive parameter A", \
     (i16_le_<scale<std::ratio<1, 100> > >), RW, (limits (0, 250))) \
  P (p_65_65, (unit_param (0x65 /*@r04*/, 0x65 /*@r04*/)), "Drive parameter B", \
     (i16_le_), RW, ()) \
  P (p_65_3f, (unit_param (0x65 /*@r04*/, 0x3F /*@r04*/)), "Drive parameter C", \
     (i16_le_), W, ()) \
  P (p_65_d1, (unit_param (0x65 /*@r04*/, 0xD1 /*@r04*/)), "Drive parameter D", \
     (i16_le_<scale<std::ratio<1, 10> > >), RW, (limits (0, 400))) \
  P (p_65_dd, (unit_param (0x65 /*@r04*/, 0xDD /*@r04*/)), "Drive parameter E", \
     (i16_le_<scale<std::ratio<1, 10> > >), RW, (limits (0, 400))) \
  P (p_65_27, (unit_param (0x65 /*@r04*/, 0x27 /*@r04*/)), "Drive parameter F", \
     (i16_le_<scale<std::ratio<1, 100> > >), RW, (limits (0, 250)))
// clang-format on
LUMEX_PROTOCOL_REGISTRY (drive, DRIVE_PARAMS);

// The service parameters: a mnemonic of two characters in a 16-bit word.
// clang-format off
#define SERVICE_PARAMS(P, P_AS) \
  P (s_a, ("IP" /*@r11*/), "Service parameter A", (i32_le_), RW, ())
// clang-format on
LUMEX_PROTOCOL_REGISTRY (service, SERVICE_PARAMS);

/// A parameter of the drive on the wire: the key (node, parameter) and the
/// value. The reply of a write is the same pair with the value in force.
LUMEX_PROTOCOL_LAYOUT (
    drive_value_t,
    (registry::registry_key_<drive, u8_, u8_>,
     registry::registry_value_<drive, 0, i16_le_, registry::access_t::read>),
    ("parameter", "value"));

LUMEX_PROTOCOL_LAYOUT (
    service_value_t,
    (registry::registry_key_<service, text_<2>>,
     registry::registry_value_<service, 0, i32_le_, registry::access_t::read>),
    ("parameter", "value"));

// The working modes: after the mode byte the layout depends on the mode, and
// the unused fields are zero.
LUMEX_PROTOCOL_LAYOUT (fill_t,
                       (u8_, i16_le_<scale<std::ratio<1, 100>>>,
                        i16_le_<scale<std::ratio<1, 10>>>, i16_le_),
                       ("arg", "a", "b", "c"));
LUMEX_PROTOCOL_LAYOUT (hold_t,
                       (u8_, i16_le_<scale<std::ratio<1, 100>>>,
                        i16_le_<scale<std::ratio<1, 10>>>,
                        i16_le_<scale<std::ratio<1, 100>>>),
                       ("arg", "a", "b", "c"));
LUMEX_PROTOCOL_LAYOUT (ramp_t,
                       (u8_<unit::minute>, i16_le_<scale<std::ratio<1, 100>>>,
                        i16_le_<scale<std::ratio<1, 100>>>, i16_le_),
                       ("arg", "a", "b", "c"));
LUMEX_PROTOCOL_LAYOUT (ramp_seconds_t,
                       (u8_, i16_le_<scale<std::ratio<1, 100>>>,
                        i16_le_<scale<std::ratio<1, 100>>>,
                        u16_le_<unit::second>),
                       ("arg", "a", "b", "c"));
LUMEX_PROTOCOL_LAYOUT (valve_t, (u8_, i16_le_, i16_le_, i16_le_),
                       ("arg", "a", "b", "c"));
LUMEX_PROTOCOL_LAYOUT (release_t,
                       (u8_, i16_le_<scale<std::ratio<1, 100>>>, i16_le_,
                        i16_le_),
                       ("arg", "a", "b", "c"));
LUMEX_PROTOCOL_LAYOUT (idle_t, (u8_, i16_le_, i16_le_, i16_le_),
                       ("arg", "a", "b", "c"));

// clang-format off
LUMEX_PROTOCOL_LAYOUT (mode_t,
  (union_by_<u8_,
             case_<0x15 /*@r04*/, idle_t>,
             case_<0xAD /*@r04*/, fill_t>,
             case_<0x65 /*@r04*/, fill_t>,
             case_<0x3F /*@r04*/, hold_t>,
             case_<0xD1 /*@r04*/, ramp_t>,
             case_<0x72 /*@r04*/, valve_t>,
             case_<0x2C /*@r04*/, ramp_seconds_t>,
             case_<0xF3 /*@r04*/, release_t> >),
  ("mode"));
// clang-format on

/// The two nibbles of the state byte of a measurement.
LUMEX_PROTOCOL_LAYOUT (nibbles_t, (bits_<u8_, 0, 4>, bits_<u8_, 4, 4>),
                       ("state", "aux"));

/// A measurement: the older firmware ends the message after the pressure
/// word, the newer one adds a time stamp.
LUMEX_PROTOCOL_LAYOUT (measurement_t,
                       (split_<u8_, nibbles_t>, u8_,
                        u16_le_<scale<std::ratio<1, 10>>>,
                        i16_le_<scale<std::ratio<1, 100>>>,
                        u16_le_<scale<std::ratio<1, 10>>>,
                        optional_tail_<u32_le_<unit::millisecond>>),
                       ("flags", "valve", "volume", "rate", "level", "time"));

// The rows of the device. report_d9 is overridden: the device reports its
// manager state, not the generic node state.
// clang-format off
#define DEVICE_E_ROWS(X, X_AS) \
  X (set_65, 0x65 /*@r04*/, "Set the working mode", (mode_t), ("mode"), (), \
     (accepted_in (manager_state::st_35))) \
  X (set_3f, 0x3F /*@r04*/, "Write a drive parameter", \
     (registry_key_<drive, u8_, u8_>, registry_value_<drive, 0, i16_le_>), \
     ("parameter", "value"), (drive_value_t), (timeout (400), retries (2))) \
  X (param_get_d1, 0xD1 /*@r04*/, "Read a drive parameter", \
     (registry_key_<drive, u8_, u8_>), ("parameter"), (drive_value_t), \
     (idempotent)) \
  X (set_72, 0x72 /*@r04*/, "Write several values of a parameter", \
     (u8_, u8_, i16_le_, i16_le_, optional_tail_<i16_le_>), \
     ("node", "parameter", "first", "second", "third"), (), ()) \
  X (event_27, 0x27 /*@r04*/, "Report a measurement", (), (), (measurement_t), \
     (stream (100))) \
  X (report_d9, 0xD9 /*@r04*/, "Report the manager state", (), (), \
     (enum_<u8_, manager_state>), (stream (1000))) \
  X (param_get_83, 0x83 /*@r04*/, "Read a service parameter", \
     (registry_key_<service, text_<2> >), ("parameter"), (service_value_t), \
     (idempotent)) \
  X (param_set_cc, 0xCC /*@r04*/, "Write a service parameter", \
     (registry_key_<service, text_<2> >, registry_value_<service, 0, i32_le_>), \
     ("parameter", "value"), (), ())
// clang-format on
LUMEX_PROTOCOL_EXTEND (device_e, family_e, DEVICE_E_ROWS);

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
} // namespace tables_e
} // namespace lumex_tests

#endif // !LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_E_TABLE_HPP
