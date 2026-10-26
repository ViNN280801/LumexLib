// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Test table of device H, a layer of the family of device A shared by four
// models: h1 and h2 answer at two addresses, h3 and h4 are told apart by
// the device information (a 4-byte serial number over the network, the full
// information otherwise) and have two parameters more. The measurement and
// the fast scan carry scaled values; the state of the main unit is a byte
// of mode flags; the packets of the internal bus carry, in their data, a
// status message with a character key (h_can: two messages, nested),
// one of them with a state byte that also holds two flags.
#ifndef LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_H_HPP
#define LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_H_HPP

#include <cstddef>
#include <cstdint>
#include <ratio>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp"
#include "lumex/tests/core/protocol/devices/device_a/LumexProtocolDeviceA.hpp"

namespace lumex_tests
{
namespace device_h
{
namespace index = lumex::core::protocol::index;
namespace layout = lumex::core::protocol::layout;
namespace registry = lumex::core::protocol::registry;
using device_a::codec;

/// The addresses of models h1 and h2 (h3 and h4 share the one of h2).
std::uint8_t const kAddressH1 = 0x9F /*@r31*/;
std::uint8_t const kAddressH2 = 0xB1 /*@r31*/;

/// Who controls device H.
LUMEX_DEFINE_REFLECTED_ENUM (control, std::uint8_t, (keyboard, 0xAD /*@r04*/),
                             (host, 0x65 /*@r04*/))

/// The state of status message A (the low six bits of its state byte).
LUMEX_DEFINE_REFLECTED_ENUM (status_a_state, std::uint8_t, (state_0, 0),
                             (state_1, 1), (state_2, 2), (state_3, 3),
                             (state_4, 4))

// The work parameters of the units of device H.
// clang-format off
#define DEVICE_H_WORK(P, P_AS) \
  P (reset_density, (unit_param (0x65 /*@r04*/, 0xAD /*@r04*/)), \
     "Zero the optical density", (i16_le_), W, ()) \
  P (frequency, (unit_param (0x65 /*@r04*/, 0x65 /*@r04*/)), \
     "Measurement frequency", (i16_le_<unit::hertz>), RW, \
     (limits (1, 50))) \
  P (target_wavelength, (unit_param (0x3F /*@r04*/, 0xAD /*@r04*/)), \
     "Target wavelength", (i16_le_<unit::nanometer>), RW, \
     (limits (200, 800))) \
  P (target_step, (unit_param (0x3F /*@r04*/, 0x65 /*@r04*/)), \
     "Target step", (i16_le_<unit::step>), RW, ()) \
  P (motor_speed, (unit_param (0x3F /*@r04*/, 0x3F /*@r04*/)), \
     "Motor speed", (i16_le_), RW, (model ("h3", "h4"))) \
  P (fast_scan, (unit_param (0x3F /*@r04*/, 0x72 /*@r04*/)), \
     "Fast scan data", (i16_le_), RW, (model ("h3", "h4"))) \
  P (lamp_mode, (unit_param (0xD1 /*@r04*/, 0xAD /*@r04*/)), "Lamp mode", \
     (i16_le_), RW, (limits (0, 1))) \
  P (lamp_test, (unit_param (0xD1 /*@r04*/, 0x65 /*@r04*/)), "Lamp test", \
     (i16_le_), RW, (limits (0, 1)))
// clang-format on
LUMEX_PROTOCOL_REGISTRY (h_work, DEVICE_H_WORK);

LUMEX_PROTOCOL_LAYOUT (
    work_value_t,
    (registry::registry_key_<h_work, u8_, u8_>,
     registry::registry_value_<h_work, 0, i16_le_, registry::access_t::read>),
    ("parameter", "value"));

LUMEX_PROTOCOL_LAYOUT (serial_t, (i32_le_), ("serial"));
LUMEX_PROTOCOL_LAYOUT (full_info_t,
                       (u8_, u8_, u8_, u8_, u8_, u8_, u8_, u8_, u32_le_),
                       ("model", "firmware", "revision", "hardware", "year",
                        "month", "day", "configuration", "serial"));
LUMEX_PROTOCOL_LAYOUT (info_t, (by_length_<5, serial_t, full_info_t>),
                       ("information"));

LUMEX_PROTOCOL_LAYOUT (
    h_measurement_t,
    (u32_le_<unit::millisecond>,
     i32_le_<scale<std::ratio<1, 1000>>, unit::milli_absorbance>, i32_le_,
     i32_le_),
    ("time", "absorbance", "photo", "reference"));

LUMEX_PROTOCOL_LAYOUT (scan_t,
                       (u32_le_<unit::millisecond>,
                        i32_le_<scale<std::ratio<1, 100>>, unit::nanometer>,
                        i32_le_, i32_le_),
                       ("time", "wavelength", "photo", "reference"));

// The mode flags of the main unit (bits 4 to 6 are not used).
LUMEX_PROTOCOL_LAYOUT (mode_flags_t,
                       (mask_<u8_, 0x01, bool>, mask_<u8_, 0x02, bool>,
                        mask_<u8_, 0x04, bool>, mask_<u8_, 0x08, bool>,
                        mask_<u8_, 0x80, bool>),
                       ("external", "analysis", "self_test", "calibration",
                        "valve"));
LUMEX_PROTOCOL_LAYOUT (main_state_t, (split_<u8_, mode_flags_t>), ("mode"));
LUMEX_PROTOCOL_LAYOUT (unit_state_t,
                       (union_by_<u8_, case_<0xAD /*@r04*/, main_state_t>>),
                       ("unit"));

// A packet of the internal bus: identifier, type, length, eight data bytes
// and the time of the packet.
LUMEX_PROTOCOL_LAYOUT (bus_packet_t,
                       (u16_le_, u8_, u8_, array_<u8_, 8>,
                        u32_le_<unit::millisecond>),
                       ("identifier", "type", "length", "data", "time"));

// h_can: the status messages inside the data of a bus packet.
LUMEX_PROTOCOL_LAYOUT (status_a_flags_t,
                       (bits_<u8_, 0, 6, status_a_state>,
                        bits_<u8_, 6, 1, bool>, bits_<u8_, 7, 1, bool>),
                       ("state", "flag_1", "flag_2"));

struct status_header
{
  typedef index::key_at<0, 1> key_reader;
};

// clang-format off
#define DEVICE_H_CAN_ROWS(X, X_AS) \
  X (status_a, 'Q' /*@r11*/, "Status message A", \
     (split_<u8_, status_a_flags_t>, u8_, i16_le_, i16_le_), \
     ("state", "value_1", "value_2", "value_3"), (), ()) \
  X (status_b, 'N' /*@r11*/, "Status message B", (array_<u8_, 6>), \
     ("state"), (), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (h_can, codec, status_header, DEVICE_H_CAN_ROWS);

/// The data of a status packet: a flag byte, then a message of h_can.
LUMEX_PROTOCOL_LAYOUT (status_data_t, (u8_, nested_<h_can, char_>),
                       ("flag", "status"));

// clang-format off
#define DEVICE_H_ROWS(X, X_AS) \
  X (query_ad, 0xAD /*@r04*/, "Instrument information", (), (), (info_t), \
     (idempotent, timeout (1000))) \
  X (set_65, 0x65 /*@r04*/, "Control mode", (enum_<u8_, control>), \
     ("control"), (enum_<u8_, control>), (timeout (1000))) \
  X (param_set_3f, 0x3F /*@r04*/, "Work parameter of a unit", \
     (registry_key_<h_work, u8_, u8_>, registry_value_<h_work, 0, i16_le_>), \
     ("parameter", "value"), (work_value_t), (timeout (1000))) \
  X (param_get_d1, 0xD1 /*@r04*/, "Read a work parameter of a unit", \
     (registry_key_<h_work, u8_, u8_>), ("parameter"), (work_value_t), \
     (idempotent, timeout (1000))) \
  X (stream_27, 0x27 /*@r04*/, "Measurement result", (), (), \
     (h_measurement_t), (stream (100))) \
  X (stream_fa, 0xFA /*@r04*/, "Fast scan result", (), (), (scan_t), \
     (stream (100))) \
  X (report_d9, 0xD9 /*@r04*/, "State of a unit", (), (), (unit_state_t), \
     (stream (1000))) \
  X (report_41, 0x41 /*@r04*/, "Packet of the internal bus", (), (), \
     (bus_packet_t), (log_level (level::trace)))
// clang-format on

LUMEX_PROTOCOL_EXTEND (device_h, device_a::family_a, DEVICE_H_ROWS);
} // namespace device_h
} // namespace lumex_tests

#endif // !LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_H_HPP
