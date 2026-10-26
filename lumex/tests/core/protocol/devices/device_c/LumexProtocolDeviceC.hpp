// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Test table of device C: the process header of device A on requests (a
// process of the firmware is addressed), replies with the address and the
// message only. Three parameter tables, one per value type, whose numbers
// overlap (a parameter is a type and a number); a parameter is written
// with its value first; the device answers a write and a read with a
// change report, another message. A measurement of 17 packed bytes with a
// 24-bit time; blocks of a memory; an accessory on an internal two-wire bus
// reached through a message of the device (c_i2c: its own command
// characters, nested in the message).
#ifndef LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_C_HPP
#define LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_C_HPP

#include <cstddef>
#include <cstdint>
#include <ratio>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp"
#include "lumex/tests/core/protocol/devices/device_a/LumexProtocolDeviceA.hpp"

namespace lumex_tests
{
namespace device_c
{
namespace index = lumex::core::protocol::index;
namespace layout = lumex::core::protocol::layout;
namespace registry = lumex::core::protocol::registry;
using device_a::codec;

/// The address of device C in the device network.
std::uint8_t const kAddress = 0x7D /*@p8z*/;

/// The processes of the firmware (the process byte of the header).
namespace process
{
std::uint8_t const display = 0x72 /*@p8*/;
std::uint8_t const measure = 0x2C /*@p8*/;
std::uint8_t const stepper = 0xDD /*@p8*/;
std::uint8_t const bus = 0x27 /*@p8*/;
std::uint8_t const memory = 0xFA /*@p8*/;
} // namespace process

/// The kind of a measurement.
LUMEX_DEFINE_REFLECTED_ENUM (measurement_kind, std::uint8_t,
                             (flash_no_background, 0x87 /*@p8*/),
                             (flash, 0x8F /*@p8*/), (dark, 0x0B /*@p8*/))

// The parameters by type: 16-bit, 32-bit and floating values.
// clang-format off
#define DEVICE_C_INT_PARAMS(P, P_AS) \
  P (mode_number, (0x15 /*@p8*/), "Number of the mode", (i16_le_), RW, ()) \
  P (sensitivity, (0xF4 /*@p8*/), "Detector sensitivity", (i16_le_), RW, \
     (limits (0, 1000))) \
  P (filter, (0xF3 /*@p8*/), "Filter", (i16_le_), RW, ())
#define DEVICE_C_LONG_PARAMS(P, P_AS) \
  P (lamp_flashes, (0x65 /*@p8*/), "Flashes of the lamp", (i32_le_), R, ()) \
  P (lamp_hours, (0x3F /*@p8*/), "Operating time of the lamp", \
     (i32_le_<unit::minute>), R, ()) \
  P (motor_steps, (0xD1 /*@p8*/), "Steps of the motor", (i32_le_), R, ()) \
  P (serial, (0x72 /*@p8*/), "Serial number", (i32_le_), R, ())
#define DEVICE_C_REAL_PARAMS(P, P_AS) \
  P (excitation, (0xF3 /*@p8*/), "Current excitation wavelength", \
     (f32_le_<unit::nanometer>), R, ()) \
  P (target, (0xD9 /*@p8*/), "Target excitation wavelength", \
     (f32_le_<unit::nanometer>), RW, ())
// The same parameters in one table: a key is the type and the number.
#define DEVICE_C_TYPED_PARAMS(P, P_AS) \
  P (filter, (typed (1, 0xF3 /*@p8*/)), "Filter", (i16_le_), RW, ()) \
  P (serial, (typed (2, 0x72 /*@p8*/)), "Serial number", (i32_le_), R, ()) \
  P (excitation, (typed (3, 0xF3 /*@p8*/)), "Excitation wavelength", \
     (f32_le_<unit::nanometer>), R, ())
// clang-format on
LUMEX_PROTOCOL_REGISTRY (c_int, DEVICE_C_INT_PARAMS);
LUMEX_PROTOCOL_REGISTRY (c_long, DEVICE_C_LONG_PARAMS);
LUMEX_PROTOCOL_REGISTRY (c_real, DEVICE_C_REAL_PARAMS);
LUMEX_PROTOCOL_REGISTRY (c_typed, DEVICE_C_TYPED_PARAMS);

/// c_i2c: the key of an accessory command is a character.
struct accessory_header
{
  typedef index::key_at<0, 1> key_reader;
};

// clang-format off
#define DEVICE_C_I2C_ROWS(X, X_AS) \
  X (query_v, 'V' /*@text*/, "Read the state of the accessory", (u8_), \
     ("read_address"), (), ()) \
  X (set_j, 'J' /*@text*/, "Wavelength of the accessory", \
     (i16_le_<scale<std::ratio<1, 10> >, unit::nanometer>, u8_), \
     ("wavelength", "read_address"), (), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (c_i2c, codec, accessory_header, DEVICE_C_I2C_ROWS);

LUMEX_PROTOCOL_LAYOUT (key_parts_t, (bits_<u8_, 0, 7>, bits_<u8_, 7, 1, bool>),
                       ("code", "released"));
LUMEX_PROTOCOL_LAYOUT (busy_parts_t,
                       (bits_<u8_, 0, 7>, bits_<u8_, 7, 1, bool>),
                       ("count", "flag"));

// The measurement: three channels, two bytes that are always 1 around the
// delay, the kind, two steps and a 24-bit time.
LUMEX_PROTOCOL_LAYOUT (measure_t,
                       (i16_le_, i16_le_, i16_le_, u8_, u8_, u8_,
                        enum_<u8_, measurement_kind>, i16_le_<unit::step>,
                        i16_le_<unit::step>, u24_le_<unit::millisecond>),
                       ("fluorescence", "photometry", "reference", "one",
                        "delay", "one_again", "kind", "excitation_step",
                        "emission_step", "time"));

LUMEX_PROTOCOL_LAYOUT (int_change_t,
                       (i16_le_, registry::registry_key_<c_int, u8_>),
                       ("value", "parameter"));
LUMEX_PROTOCOL_LAYOUT (long_change_t,
                       (i32_le_, registry::registry_key_<c_long, u8_>),
                       ("value", "parameter"));
LUMEX_PROTOCOL_LAYOUT (real_change_t,
                       (f32_le_<unit::nanometer>,
                        registry::registry_key_<c_real, u8_>),
                       ("value", "parameter"));

LUMEX_PROTOCOL_LAYOUT (memory_block_t,
                       (u8_, u8_, i16_le_,
                        computed_count_<u8_, count_from_<0>, 64>),
                       ("size", "zero", "address", "data"));

LUMEX_PROTOCOL_LAYOUT (bus_result_t,
                       (split_<u8_, busy_parts_t>, split_<u8_, busy_parts_t>,
                        u8_, u8_, u8_, char_, token_),
                       ("busy", "no_ack", "delay", "address", "length",
                        "command", "data"));

// clang-format off
#define DEVICE_C_ROWS(X, X_AS) \
  X (reset_ad, 0xAD /*@p8*/, "Reset", (), (), (), (dangerous)) \
  X (set_65, 0x65 /*@p8*/, "Key press", (split_<u8_, key_parts_t>), \
     ("key"), (), ()) \
  X (param_set_fa, 0xFA /*@p8*/, "16-bit parameter", \
     (i16_le_, registry_key_<c_int, u8_>), ("value", "parameter"), (), \
     (timeout (500), retries (3))) \
  X (param_get_f4, 0xF4 /*@p8*/, "Read a 16-bit parameter", \
     (registry_key_<c_int, u8_>), ("parameter"), (), \
     (idempotent, timeout (500), retries (3))) \
  X (report_35, 0x35 /*@p8*/, "16-bit parameter changed", (), (), \
     (int_change_t), ()) \
  X (param_set_67, 0x67 /*@p8*/, "32-bit parameter", \
     (i32_le_, registry_key_<c_long, u8_>), ("value", "parameter"), (), \
     (timeout (500), retries (3))) \
  X (param_get_4b, 0x4B /*@p8*/, "Read a 32-bit parameter", \
     (registry_key_<c_long, u8_>), ("parameter"), (), \
     (idempotent, timeout (500), retries (3))) \
  X (report_38, 0x38 /*@p8*/, "32-bit parameter changed", (), (), \
     (long_change_t), ()) \
  X (param_set_47, 0x47 /*@p8*/, "Floating parameter", \
     (f32_le_<unit::nanometer>, registry_key_<c_real, u8_>), \
     ("value", "parameter"), (), (timeout (500), retries (3))) \
  X (param_get_44, 0x44 /*@p8*/, "Read a floating parameter", \
     (registry_key_<c_real, u8_>), ("parameter"), (), \
     (idempotent, timeout (500), retries (3))) \
  X (report_e0, 0xE0 /*@p8*/, "Floating parameter changed", (), (), \
     (real_change_t), ()) \
  X (stream_dd, 0xDD /*@p8*/, "Measurement", (), (), (measure_t), \
     (stream (100))) \
  X (start_83, 0x83 /*@p8*/, "Start the measurement", (u8_, u8_), \
     ("source", "kind"), (), ()) \
  X (stop_cc, 0xCC /*@p8*/, "Stop the measurement", (u8_), ("source"), (), \
     ()) \
  X (stop_0f, 0x0F /*@p8*/, "Abort the measurement", (), (), (), ()) \
  X (report_87, 0x87 /*@p8*/, "Monochromator stopped", (), (), \
     (i16_le_<unit::step>), ()) \
  X (write_8d, 0x8D /*@p8*/, "Write a memory block", (memory_block_t), \
     ("block"), (), (dangerous)) \
  X (query_c9, 0xC9 /*@p8*/, "Read a memory block", (u8_, u8_, i16_le_), \
     ("size", "zero", "address"), (), (idempotent)) \
  X (report_bc, 0xBC /*@p8*/, "Memory block", (), (), (memory_block_t), ()) \
  X (write_8e, 0x8E /*@p8*/, "Transaction on the accessory bus", \
     (u8_, u8_, u8_, u8_, u8_, nested_<c_i2c, char_>), \
     ("write_count", "read_count", "delay", "address", "length", \
      "command"), (), (timeout (500))) \
  X (report_7b, 0x7B /*@p8*/, "Result of a bus transaction", (), (), \
     (bus_result_t), ()) \
  X (stream_af, 0xAF /*@p8*/, "A message on or off", (u8_, u8_), \
     ("message", "enable"), (), ())
#define DEVICE_C_NO_ROWS(X, X_AS)
// clang-format on

// Requests carry the process header; replies only the address and the
// message: the same rows, the key at another place.
LUMEX_PROTOCOL_DEFINE (device_c, codec, device_a::process_header,
                       DEVICE_C_ROWS);
LUMEX_PROTOCOL_EXTEND_FRAMED (device_c_rx, device_c, codec,
                              device_a::family_header, DEVICE_C_NO_ROWS);
} // namespace device_c
} // namespace lumex_tests

#endif // !LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_C_HPP
