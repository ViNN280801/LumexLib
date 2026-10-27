// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Test table of device D: a layer of the family of device A. Work
// parameters are addressed by a logical unit and a parameter of that unit
// (the same parameter number means another thing in another unit); one
// command writes two or three values of a parameter; service parameters
// have two-letter keys; the device information is a 4-byte serial number
// when the device is reached over the network and the full information
// otherwise (the length decides); the system message of the link carries
// ten statistics counters in this generation (d_system) instead of the old
// content of the family.
#ifndef LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_D_HPP
#define LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_D_HPP

#include <cstddef>
#include <cstdint>
#include <ratio>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/devices/device_a/LumexProtocolDeviceA.hpp"

namespace lumex_tests
{
namespace device_d
{
namespace index = lumex::core::protocol::index;
namespace layout = lumex::core::protocol::layout;
namespace registry = lumex::core::protocol::registry;
using device_a::codec;

/// The address of device D in the device network.
std::uint8_t const kAddress = 0x7D /*@r31*/;

/// The logical units of device D.
namespace unit_of
{
std::uint8_t const main_controller = 0xAD /*@r04*/;
std::uint8_t const measurement = 0x65 /*@r04*/;
std::uint8_t const excitation = 0x3F /*@r04*/;
std::uint8_t const emission = 0xD1 /*@r04*/;
std::uint8_t const detector = 0x72 /*@r04*/;
std::uint8_t const filter = 0x2C /*@r04*/;
std::uint8_t const accessory = 0xDD /*@r04*/;
} // namespace unit_of

// Parameter 1 of four units: four parameters.
// clang-format off
#define DEVICE_D_WORK(P, P_AS) \
  P (measurement_mode, (unit_param (0x65 /*@r04*/, 0xAD /*@r04*/)), \
     "Measurement mode", (i16_le_), RW, (limits (0, 7))) \
  P (excitation_target, (unit_param (0x3F /*@r04*/, 0xAD /*@r04*/)), \
     "Target excitation wavelength", \
     (i16_le_<scale<std::ratio<1, 10> >, unit::nanometer>), RW, \
     (limits (215, 875))) \
  P (emission_target, (unit_param (0xD1 /*@r04*/, 0xAD /*@r04*/)), \
     "Target emission wavelength", \
     (i16_le_<scale<std::ratio<1, 10> >, unit::nanometer>), RW, \
     (limits (215, 875))) \
  P (detector_sensitivity, (unit_param (0x72 /*@r04*/, 0xAD /*@r04*/)), \
     "Detector sensitivity", (i16_le_), RW, (limits (0, 1000)))
// Service parameters by two letters (examples of the format).
#define DEVICE_D_SERVICE(P, P_AS) \
  P (s_a, ("PW"), "Service parameter A", (i32_le_), R, ()) \
  P (s_b, ("DL"), "Service parameter B", (i32_le_), RW, ())
// clang-format on
LUMEX_PROTOCOL_REGISTRY (d_work, DEVICE_D_WORK);
LUMEX_PROTOCOL_REGISTRY (d_service, DEVICE_D_SERVICE);

LUMEX_PROTOCOL_LAYOUT (
    work_value_t,
    (registry::registry_key_<d_work, u8_, u8_>,
     registry::registry_value_<d_work, 0, i16_le_, registry::access_t::read>),
    ("parameter", "value"));

LUMEX_PROTOCOL_LAYOUT (serial_t, (i32_le_), ("serial"));
LUMEX_PROTOCOL_LAYOUT (full_info_t, (u8_, u8_, u8_, u8_, u8_, u8_, u8_),
                       ("model", "software", "subversion", "hardware", "year",
                        "month", "day"));
LUMEX_PROTOCOL_LAYOUT (info_t, (by_length_<5, serial_t, full_info_t>),
                       ("information"));

LUMEX_PROTOCOL_LAYOUT (service_value_t,
                       (registry::registry_key_<d_service, text_<2>>, i32_le_),
                       ("parameter", "value"));

LUMEX_PROTOCOL_LAYOUT (statistics_t, (u8_, array_<u16_le_, 10>),
                       ("message", "counters"));

// clang-format off
#define DEVICE_D_ROWS(X, X_AS) \
  X (query_ad, 0xAD /*@r04*/, "Instrument information", (), (), (info_t), \
     (idempotent, timeout (1000))) \
  X (param_set_3f, 0x3F /*@r04*/, "Work parameter of a unit", \
     (registry_key_<d_work, u8_, u8_>, registry_value_<d_work, 0, i16_le_>), \
     ("parameter", "value"), (work_value_t), (timeout (1000))) \
  X (param_get_d1, 0xD1 /*@r04*/, "Read a work parameter of a unit", \
     (registry_key_<d_work, u8_, u8_>), ("parameter"), (work_value_t), \
     (idempotent, timeout (1000))) \
  X (param_set_72, 0x72 /*@r04*/, "Several values of a work parameter", \
     (registry_key_<d_work, u8_, u8_>, i16_le_, i16_le_, \
      optional_tail_<i16_le_>), \
     ("parameter", "first", "second", "third"), (), (timeout (1000))) \
  X (param_get_83, 0x83 /*@r04*/, "Read a service parameter", \
     (registry_key_<d_service, text_<2> >), ("parameter"), \
     (service_value_t), (idempotent)) \
  X (param_set_cc, 0xCC /*@r04*/, "Service parameter", \
     (registry_key_<d_service, text_<2> >, i32_le_), ("parameter", "value"), \
     (service_value_t), (dangerous)) \
  X (query_b8, 0xB8 /*@r04*/, "Information of an internal node", (u8_), \
     ("node"), (), ()) \
  X (set_64, 0x64 /*@r04*/, "Trace of the internal bus", (u8_), \
     ("enable"), (), ())
#define DEVICE_D_SYSTEM_ROWS(X, X_AS) \
  X (keepalive_00, 0x00, "System message of the link", (), (), \
     (statistics_t), (stream (1000)))
// clang-format on

LUMEX_PROTOCOL_EXTEND (device_d, device_a::family_a, DEVICE_D_ROWS);
LUMEX_PROTOCOL_EXTEND (d_system, device_a::a_system, DEVICE_D_SYSTEM_ROWS);
} // namespace device_d
} // namespace lumex_tests

#endif // !LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_D_HPP
