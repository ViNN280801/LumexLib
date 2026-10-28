// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Test table of device B, two generations of one device:
// - b2 (models b2_m1, b2_m2): a layer of the family of device A whose fields
//   are aligned to their size; the work mode with two positions packed into
//   words, work parameters of units through a registry with (unit,
//   parameter) keys, service parameters with two-letter keys that the device
//   lists itself (the high byte of an index 0), the state of a unit chosen by
//   the unit, a second measurement channel, the system message of the link
//   with the time of the host; model b2_m2 doubles the scale of the
//   measurement;
// - b1 (models b1_m1 ... b1_m4): the process header of device A (a task of
//   the firmware is addressed), the state command with its discriminant at
//   the end, parameters by number with the value before the number (one
//   number is used by two parameters), a device information with an
//   optional date.
#ifndef LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_B_HPP
#define LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_B_HPP

#include <cstddef>
#include <cstdint>
#include <ratio>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp"
#include "lumex/tests/core/protocol/devices/device_a/LumexProtocolDeviceA.hpp"

namespace lumex_tests
{
namespace device_b
{
namespace index = lumex::core::protocol::index;
namespace layout = lumex::core::protocol::layout;
namespace registry = lumex::core::protocol::registry;
using device_a::codec;

/// The address of device B in the device network.
std::uint8_t const kAddress = 0x1A /*@r31*/;

// ---------------------------------------------------------------------------
// b2
// ---------------------------------------------------------------------------

/// The work modes of b2.
LUMEX_DEFINE_REFLECTED_ENUM (b2_mode, std::int16_t, (idle, 0x15 /*@r04*/),
                             (rinse, 0x65 /*@r04*/),
                             (inject_by_pressure, 0x3F /*@r04*/),
                             (analysis, 0x2C /*@r04*/),
                             (self_test, 0x27 /*@r04*/))

/// The position of the lift of a carousel.
LUMEX_DEFINE_REFLECTED_ENUM (lift, std::uint8_t, (lower, 0), (upper, 1),
                             (pressed, 2))

/// The fields of b2 are aligned to their size from the first field on.
struct b2_header
{
  using lumex_vocabulary = registry::table_vocabulary;
  using layout_type = layout::binary_layout<layout::natural_alignment<0>>;
  using key_reader = index::key_at<1, 1>;
};

/// The family table with the aligned policy (the family itself is not
/// aligned: one of its rows puts a 4-byte field at offset 2).
struct aligned_family_header
{
  using lumex_vocabulary = registry::table_vocabulary;
  using layout_type = layout::binary_layout<layout::natural_alignment<0>>;
  using key_reader = index::key_at<1, 1>;
};

// The work parameters of the units of b2: (unit, parameter) keys.
// clang-format off
#define DEVICE_B2_WORK(P, P_AS) \
  P (mode_duration, (unit_param (0xAD /*@r04*/, 0xAD /*@r04*/)), \
     "Duration of the mode", (i16_le_<unit::second>), RW, \
     (limits (0, 3600))) \
  P (reset_density, (unit_param (0xD1 /*@r04*/, 0xAD /*@r04*/)), \
     "Zero the optical density", (i16_le_), W, ()) \
  P (frequency, (unit_param (0xD1 /*@r04*/, 0x65 /*@r04*/)), \
     "Measurement frequency", (i16_le_<unit::hertz>), RW, \
     (limits (5, 100))) \
  P (flow_switch, (unit_param (0xF3 /*@r04*/, 0xAD /*@r04*/)), \
     "Flow potential on or off", (i16_le_), RW, (limits (0, 1)))
// clang-format on
LUMEX_PROTOCOL_REGISTRY (b2_work, DEVICE_B2_WORK);

// The service parameters of b2: two letters, written on the wire as the
// little-endian word of the first letter in the high byte (the key text is
// the wire order).
// clang-format off
#define DEVICE_B2_SERVICE(P, P_AS) \
  P (p_01, ("YM" /*@r11*/), "Service parameter 1", (u32_le_), R, ()) \
  P (p_02, ("YE" /*@r11*/), "Service parameter 2", (i32_le_), RW, ()) \
  P (p_03, ("LE" /*@r11*/), "Service parameter 3", (i32_le_), RW, ()) \
  P (p_04, ("KN" /*@r11*/), "Service parameter 4", \
     (i32_le_<scale<std::ratio<1, 10> > >), RW, ()) \
  P (p_05, ("EF" /*@r11*/), "Service parameter 5", (i32_le_), RW, ()) \
  P (p_06, ("TQ" /*@r11*/), "Service parameter 6", (i32_le_), RW, ()) \
  P (p_07, ("QT" /*@r11*/), "Service parameter 7", (i32_le_), RW, ()) \
  P (p_08, ("SR" /*@r11*/), "Service parameter 8", (i32_le_), RW, ()) \
  P (p_09, ("OQ" /*@r11*/), "Service parameter 9", (i32_le_), RW, ()) \
  P (p_10, ("WQ" /*@r11*/), "Service parameter 10", (i32_le_), RW, ()) \
  P (p_11, ("XQ" /*@r11*/), "Service parameter 11", (i32_le_), RW, ()) \
  P (p_12, ("4O" /*@r11*/), "Service parameter 12", (i32_le_), RW, ()) \
  P (p_13, ("1O" /*@r11*/), "Service parameter 13", (i32_le_), RW, ())
// A floating entry has no raw value: it lives in a registry of its own,
// for the exports and verify_registry.
#define DEVICE_B2_FLOATING(P, P_AS) \
  P (p_14, ("QR" /*@r11*/), "Service parameter 14", (f32_le_), RW, ())
// clang-format on
LUMEX_PROTOCOL_REGISTRY (b2_service, DEVICE_B2_SERVICE);
LUMEX_PROTOCOL_REGISTRY (b2_floating, DEVICE_B2_FLOATING);

LUMEX_PROTOCOL_LAYOUT (vial_t,
                       (bits_<u16_le_, 0, 8>, bits_<u16_le_, 8, 8, lift>),
                       ("vial", "lift"));

LUMEX_PROTOCOL_LAYOUT (
    b2_measurement_t,
    (u32_le_<unit::millisecond>,
     i32_le_<scale<std::ratio<1, 1000000>>, unit::absorbance>, i32_le_,
     i32_le_),
    ("time", "absorbance", "photo", "reference"));

LUMEX_PROTOCOL_LAYOUT (
    work_value_t,
    (registry::registry_key_<b2_work, u8_, u8_>,
     registry::registry_value_<b2_work, 0, i16_le_, registry::access_t::read>),
    ("parameter", "value"));

LUMEX_PROTOCOL_LAYOUT (service_value_t,
                       (u16_le_, registry::registry_key_<b2_service, text_<2>>,
                        registry::registry_value_<b2_service, 1, i32_le_,
                                                  registry::access_t::read>),
                       ("requested", "parameter", "value"));

// The state of the main controller: mode, cassette and cover, the time
// counter, the duration, voltage, pressure, current; newer firmware adds
// the targets of both carousels.
LUMEX_PROTOCOL_LAYOUT (main_state_t,
                       (u8_, u8_, u8_, u32_le_<unit::millisecond>,
                        u32_le_<unit::second>, i16_le_<unit::volt>,
                        i16_le_<scale<std::ratio<1, 10>>, unit::kilopascal>,
                        u16_le_<scale<std::ratio<1, 1000>>, unit::milliampere>,
                        optional_tail_<u8_, u8_, u8_, u8_, u8_, u8_>),
                       ("mode", "cassette", "cover", "time", "duration",
                        "voltage", "pressure", "max_current", "targets"));

// The state of the compressor; two bytes keep the time on a 4-byte boundary.
LUMEX_PROTOCOL_LAYOUT (compressor_state_t,
                       (u8_, i16_le_,
                        i32_le_<scale<std::ratio<1, 100>>, unit::kilopascal>,
                        i16_le_, u8_, u8_, u8_, u8_, u16_le_,
                        u32_le_<unit::second>),
                       ("state", "target", "pressure", "power", "mode",
                        "state_1", "state_2", "error", "reserve", "time"));

LUMEX_PROTOCOL_LAYOUT (unit_state_t,
                       (union_by_<u8_, case_<0xAD /*@r04*/, main_state_t>,
                                  case_<0xFA /*@r04*/, compressor_state_t>>),
                       ("unit"));

// The system message of the link: from the device its address and three
// error counters; from the host the time (seconds since 1970).
LUMEX_PROTOCOL_LAYOUT (link_t, (u8_, u16_le_, u16_le_, u16_le_),
                       ("instrument", "check_errors", "length_errors",
                        "overflows"));

// clang-format off
#define DEVICE_B2_ROWS(X, X_AS) \
  X (set_65, 0x65 /*@r04*/, "Work mode", \
     (enum_<i16_le_, b2_mode>, i16_le_<unit::second>, \
      i16_le_<scale<std::ratio<1, 10> >, unit::kilopascal>, \
      i16_le_<unit::volt>, \
      i16_le_<scale<std::ratio<1, 1000> >, unit::milliampere>, \
      split_<u16_le_, vial_t>, split_<u16_le_, vial_t>), \
     ("mode", "duration", "pressure", "voltage", "max_current", "input", \
      "output"), \
     (enum_<i16_le_, b2_mode>), (timeout (1000))) \
  X (param_set_3f, 0x3F /*@r04*/, "Work parameter of a unit", \
     (registry_key_<b2_work, u8_, u8_>, registry_value_<b2_work, 0, i16_le_>), \
     ("parameter", "value"), (work_value_t), (timeout (1000))) \
  X (param_get_d1, 0xD1 /*@r04*/, "Read a work parameter of a unit", \
     (registry_key_<b2_work, u8_, u8_>), ("parameter"), (work_value_t), \
     (idempotent, timeout (1000))) \
  X (stream_27, 0x27 /*@r04*/, "Measurement result", (), (), \
     (b2_measurement_t), (stream (100))) \
  X (stream_fa, 0xFA /*@r04*/, "Measurement result of the second channel", \
     (), (), (b2_measurement_t), (stream (100))) \
  X (report_d9, 0xD9 /*@r04*/, "State of a unit", (), (), (unit_state_t), \
     (stream (1000))) \
  X (param_get_83, 0x83 /*@r04*/, "Read a service parameter", \
     (registry_key_<b2_service, text_<2> >), ("parameter"), \
     (service_value_t), (idempotent)) \
  X (param_set_cc, 0xCC /*@r04*/, "Service parameter", \
     (registry_key_<b2_service, text_<4> >, \
      registry_value_<b2_service, 0, i32_le_>), \
     ("parameter", "value"), (service_value_t), (dangerous)) \
  X (query_b8, 0xB8 /*@r04*/, "Information of an internal node", \
     (u8_, char_), ("node", "kind"), (), ())
// Model b2_m2 doubles the scale of the measurement.
#define DEVICE_B2_M2_ROWS(X, X_AS) \
  X (stream_27, 0x27 /*@r04*/, "Measurement result", (), (), \
     (b2_m2_measurement_t), (stream (100), model ("b2_m2")))
#define DEVICE_B2_SYSTEM_ROWS(X, X_AS) \
  X (keepalive_00, 0x00, "System message of the link", \
     (u8_, u32_le_<unit::second>), ("message", "time"), (link_t), \
     (stream (1000)))
#define DEVICE_B_NO_ROWS(X, X_AS)
// clang-format on

LUMEX_PROTOCOL_LAYOUT (
    b2_m2_measurement_t,
    (u32_le_<unit::millisecond>,
     i32_le_<scale<std::ratio<2, 1000000>>, unit::absorbance>, i32_le_,
     i32_le_),
    ("time", "absorbance", "photo", "reference"));

LUMEX_PROTOCOL_EXTEND_FRAMED (device_b2, device_a::family_a, codec, b2_header,
                              DEVICE_B2_ROWS);
LUMEX_PROTOCOL_EXTEND (device_b2_m2, device_b2, DEVICE_B2_M2_ROWS);
LUMEX_PROTOCOL_EXTEND (b2_system, device_a::a_system, DEVICE_B2_SYSTEM_ROWS);
LUMEX_PROTOCOL_EXTEND_FRAMED (aligned_family, device_a::family_a, codec,
                              aligned_family_header, DEVICE_B_NO_ROWS);

// ---------------------------------------------------------------------------
// b1
// ---------------------------------------------------------------------------

/// The tasks of the firmware of b1 (the process byte of the header).
namespace task
{
std::uint8_t const manager = 0x3F /*@r04*/;
std::uint8_t const sampler = 0xD1 /*@r04*/;
std::uint8_t const lamp = 0x27 /*@r04*/;
std::uint8_t const monochromator = 0xFA /*@r04*/;
std::uint8_t const high_voltage = 0xF3 /*@r04*/;
} // namespace task

// The parameters of b1 by number; the value is a 4-byte integer of every
// type. The numbers of rinse_pressure and starting_voltage are the same (a
// clash the index reports).
// clang-format off
#define DEVICE_B1_PARAMS(P, P_AS) \
  P (link_protocol, (0x15 /*@r04*/), "Protocol of the link", (i32_le_), RW, \
     ()) \
  P (rinse_voltage, (0xAD /*@r04*/), "Rinse voltage", \
     (i32_le_<unit::volt>), RW, (limits (0, 30000))) \
  P (inject_voltage, (0x65 /*@r04*/), "Injection voltage", \
     (i32_le_<unit::volt>), RW, (limits (0, 30000))) \
  P (inject_time, (0x3F /*@r04*/), "Injection time", \
     (i32_le_<unit::second>), RW, (limits (0, 999))) \
  P (inject_pressure, (0xD1 /*@r04*/), "Injection pressure", \
     (i32_le_<scale<std::ratio<1, 10> >, unit::kilopascal>), RW, \
     (limits (0, 100))) \
  P (analysis_voltage, (0x72 /*@r04*/), "Analysis voltage", \
     (i32_le_<unit::volt>), RW, (limits (-30000, 30000))) \
  P (target_temperature, (0xDD /*@r04*/), "Target temperature", \
     (i32_le_<scale<std::ratio<1, 10> >, unit::degree_celsius>), RW, \
     (limits (10, 70))) \
  P (analysis_wavelength, (0xF3 /*@r04*/), "Analysis wavelength", \
     (i32_le_<unit::nanometer>), RW, (limits (200, 400))) \
  P (zero_shift, (0x47 /*@r04*/), "Zero shift of the monochromator", \
     (i32_le_<unit::step>), RW, (model ("b1_m1"))) \
  P (steps_per_nm, (0xE0 /*@r04*/), "Steps per nanometer", \
     (i32_le_<unit::step>), RW, (model ("b1_m1"))) \
  P (serial, (0xE8 /*@r04*/), "Serial number", (i32_le_), R, ()) \
  P (rinse_pressure, (0x50 /*@r04*/), "Rinse pressure", \
     (i32_le_<scale<std::ratio<1, 10> >, unit::kilopascal>), RW, ()) \
  P (starting_voltage, (0x50 /*@r04*/), "Lamp starting voltage", \
     (i32_le_<unit::millivolt>), RW, ()) \
  P (start_to_work_level, (0xD3 /*@r04*/), "Lamp level to work", \
     (i32_le_), RW, (model ("b1_m3", "b1_m4"))) \
  P (work_to_start_level, (0xBC /*@r04*/), "Lamp level to start", \
     (i32_le_), RW, (model ("b1_m3", "b1_m4"))) \
  P (inlet_target, (0xAC /*@r04*/), "Inlet target position", (i32_le_), RW, \
     (model ("b1_m1", "b1_m2", "b1_m3"))) \
  P (outlet_target, (0x55 /*@r04*/), "Outlet target position", (i32_le_), \
     RW, (model ("b1_m1", "b1_m2", "b1_m3"))) \
  P (calibration_passed, (0x9A /*@r04*/), "Calibration passed", (i32_le_), \
     R, (model ("b1_m1"), firmware (20, 99)))
// The same table without the clash (the second of the two entries left out).
#define DEVICE_B1_PARAMS_UNIQUE(P, P_AS) \
  P (rinse_pressure, (0x50 /*@r04*/), "Rinse pressure", \
     (i32_le_<scale<std::ratio<1, 10> >, unit::kilopascal>), RW, ()) \
  P (serial, (0xE8 /*@r04*/), "Serial number", (i32_le_), R, ()) \
  P (inject_time, (0x3F /*@r04*/), "Injection time", \
     (i32_le_<unit::second>), RW, (limits (0, 999)))
// clang-format on
LUMEX_PROTOCOL_REGISTRY (b1_params, DEVICE_B1_PARAMS);
LUMEX_PROTOCOL_REGISTRY (b1_params_unique, DEVICE_B1_PARAMS_UNIQUE);

/// The states of the manager of b1 (the last byte of the state command).
namespace state
{
std::uint8_t const ready = 0x15 /*@r04*/;
std::uint8_t const rinse = 0x72 /*@r04*/;
std::uint8_t const inject_by_voltage = 0x2C /*@r04*/;
std::uint8_t const inject_by_pressure = 0xDD /*@r04*/;
std::uint8_t const analysis = 0x27 /*@r04*/;
} // namespace state

LUMEX_PROTOCOL_LAYOUT (pressure_args_t,
                       (i16_le_<scale<std::ratio<1, 10>>, unit::kilopascal>,
                        i16_le_<unit::second>, i16_le_),
                       ("pressure", "time", "unused"));
LUMEX_PROTOCOL_LAYOUT (voltage_args_t,
                       (i16_le_<unit::volt>, i16_le_<unit::second>, i16_le_),
                       ("voltage", "time", "unused"));
LUMEX_PROTOCOL_LAYOUT (plain_args_t, (i16_le_, i16_le_, i16_le_),
                       ("first", "second", "third"));

// The state: three numbers whose meaning the state at the end chooses.
LUMEX_PROTOCOL_LAYOUT (
    manager_t,
    (union_by_<at_end_<u8_>, case_<0xDD /*@r04*/, pressure_args_t>,
               case_<0x2C /*@r04*/, voltage_args_t>,
               case_<0x72 /*@r04*/, plain_args_t>,
               case_<0x27 /*@r04*/, plain_args_t>,
               case_<0x15 /*@r04*/, plain_args_t>>),
    ("state"));

LUMEX_PROTOCOL_LAYOUT (device_info_t,
                       (u8_, u8_, u8_, u8_, i32_le_,
                        optional_tail_<u8_, u8_, u8_>),
                       ("type", "software", "subversion", "hardware", "serial",
                        "date"));

LUMEX_PROTOCOL_LAYOUT (parameter_value_t,
                       (i32_le_,
                        registry::registry_key_<b1_params_unique, u8_>),
                       ("value", "parameter"));

// clang-format off
#define DEVICE_B1_ROWS(X, X_AS) \
  X (query_b8, 0xB8 /*@r04*/, "Ask for the device information", (), (), (), \
     (idempotent)) \
  X (report_15, 0x15 /*@r04*/, "Device information", (), (), \
     (device_info_t), ()) \
  X (set_ac, 0xAC /*@r04*/, "Manager state", (manager_t), ("state"), (), \
     (timeout (1000))) \
  X (report_9a, 0x9A /*@r04*/, "State of the manager", (), (), (manager_t), \
     ()) \
  X (param_set_64, 0x64 /*@r04*/, "Parameter", \
     (i32_le_, registry_key_<b1_params_unique, u8_>), ("value", "parameter"), \
     (), (dangerous)) \
  X (param_get_83, 0x83 /*@r04*/, "Ask for a parameter", \
     (registry_key_<b1_params_unique, u8_>), ("parameter"), (), \
     (idempotent)) \
  X (report_cc, 0xCC /*@r04*/, "Parameter value", (), (), \
     (parameter_value_t), ()) \
  X (stream_e1, 0xE1 /*@r04*/, "A message on or off", (u8_, u8_), \
     ("message", "enable"), (), ()) \
  X (send_94, 0x94 /*@r04*/, "Frame to an internal node", \
     (u8_, array_<u8_, 8>), ("node", "data"), (), ()) \
  X (report_84, 0x84 /*@r04*/, "Frame of an internal node", (), (), \
     (device_a::can_packet_t), (log_level (level::trace)))
// clang-format on

// The host writes the process header; the device answers without the
// process and the attribute: the same rows, the key after the address.
LUMEX_PROTOCOL_DEFINE (device_b1, codec, device_a::process_header,
                       DEVICE_B1_ROWS);
LUMEX_PROTOCOL_EXTEND_FRAMED (device_b1_rx, device_b1, codec,
                              device_a::family_header, DEVICE_B_NO_ROWS);
} // namespace device_b
} // namespace lumex_tests

#endif // !LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_B_HPP
