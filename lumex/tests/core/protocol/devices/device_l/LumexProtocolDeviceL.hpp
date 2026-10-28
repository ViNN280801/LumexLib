// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Test table of device L: a register table read and written over Modbus
// RTU (big-endian registers, the check of the frame high byte first, see
// core/framing modbus_rtu). The requests are keyed by the function after
// the address of the device; the replies by the function and the byte
// count together (l_replies: the state reply of 23 registers is the pair
// 03 2E). One register takes commands (an enumeration); five consecutive
// registers hold the injection parameters, and each wash selector is
// followed by its volume register.
#ifndef LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_L_HPP
#define LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_L_HPP

#include <cstddef>
#include <cstdint>
#include <ratio>

#include "lumex/core/framing/modbus_rtu/LumexModbusRtuCodec.hpp"
#include "lumex/core/protocol/LumexProtocol"
#include "lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp"

namespace lumex_tests
{
namespace device_l
{
namespace index = lumex::core::protocol::index;
namespace layout = lumex::core::protocol::layout;
namespace registry = lumex::core::protocol::registry;
namespace field = lumex::core::protocol::field;

/// The framing of device L.
using codec = lumex::core::framing::modbus_rtu::modbus_rtu_codec;

/// The Modbus address of device L (the default slave address).
std::uint8_t const kSlave = 0x01;

/// The values of the command register.
LUMEX_DEFINE_REFLECTED_ENUM (l_command, std::uint16_t,
                             (reset, 0x4159 /*@r09*/), (stop, 0x5590 /*@r09*/),
                             (clear_alarm, 0x6D00 /*@r09*/),
                             (inject, 0x2D18 /*@r09*/),
                             (wash, 0xF23B /*@r09*/),
                             (load_temperature, 0x1B43 /*@r09*/),
                             (tray_start, 0x3F48 /*@r09*/),
                             (tray_stop, 0x9555 /*@r09*/))

// The registers of device L.
// clang-format off
#define DEVICE_L_REGISTERS(P, P_AS) \
  P (command, (modbus (0x2CC8 /*@r09*/)), "Command", \
     (enum_<u16_be_, l_command>), W, ()) \
  P (tray_control, (modbus (0xF4A6 /*@r09*/)), "Tray temperature control", \
     (u16_be_), RW, (limits (0, 1))) \
  P (tray_temperature, (modbus (0x233B /*@r09*/)), "Tray temperature", \
     (u16_be_<scale<std::ratio<1, 10> >, unit::degree_celsius>), RW, \
     (limits (4, 40))) \
  P (inject_mode, (modbus (0xF67E /*@r09*/)), "Injection mode", (u16_be_), \
     RW, (limits (0, 3))) \
  P (sample, (modbus (0xF67F /*@r63*/)), "Sample number", (u16_be_), RW, \
     (limits (1, 120))) \
  P (extraction, (modbus (0xF680 /*@r66*/)), "Extraction mode", \
     (u16_be_), RW, ()) \
  P (overflow_volume, (modbus (0xF681 /*@r69*/)), "Overflow volume", \
     (u16_be_<scale<std::ratio<1, 10> >, unit::microliter>), RW, ()) \
  P (inject_volume, (modbus (0xF682 /*@r72*/)), "Injection volume", \
     (u16_be_<scale<std::ratio<1, 10> >, unit::microliter>), RW, \
     (limits (0, 100))) \
  P (wash_a, (modbus (0x8C52 /*@r09*/)), "Wash A selected", (u16_be_), RW, \
     (limits (0, 1))) \
  P (wash_a_volume, (modbus (0x8C53 /*@r63*/)), "Wash A volume", \
     (u16_be_<scale<std::ratio<1, 10> >, unit::microliter>), RW, ()) \
  P (wash_b, (modbus (0x357C /*@r09*/)), "Wash B selected", (u16_be_), RW, \
     (limits (0, 1))) \
  P (wash_b_volume, (modbus (0x357D /*@r63*/)), "Wash B volume", \
     (u16_be_<scale<std::ratio<1, 10> >, unit::microliter>), RW, ()) \
  P (wash_ab, (modbus (0x4ED3 /*@r09*/)), "Wash A and B selected", \
     (u16_be_), RW, (limits (0, 1))) \
  P (wash_ab_volume, (modbus (0x4ED4 /*@r63*/)), "Wash A and B volume", \
     (u16_be_<scale<std::ratio<1, 10> >, unit::microliter>), RW, ()) \
  P (state, (modbus (0x1000)), "First register of the state block", \
     (u16_be_), R, ())
// clang-format on
LUMEX_PROTOCOL_REGISTRY (l_registers, DEVICE_L_REGISTERS);

/// The number of registers of a byte count (the field before).
struct half_of_previous
{
  template <typename Siblings>
  static std::size_t
  count (Siblings const &siblings) LUMEX_NOEXCEPT
  {
    return static_cast<std::size_t> (field::get<2> (siblings)) / 2u;
  }
};

/// Requests: the address of the device, then the function.
struct request_header
{
  using lumex_vocabulary = registry::table_vocabulary;
  using key_reader = index::key_at<1, 1>;
};

/// Replies: the function and the byte count are the key.
struct reply_header
{
  using key_reader = index::key_at<1, 2>;
};

// clang-format off
#define DEVICE_L_REQUEST_ROWS(X, X_AS) \
  X (query_03, 0x03, "Read registers", \
     (registry_key_<l_registers, u16_be_>, u16_be_), ("first", "count"), \
     (), (idempotent, timeout (1000))) \
  X (set_06, 0x06, "Write a register", \
     (registry_key_<l_registers, u16_be_>, \
      registry_value_<l_registers, 0, u16_be_>), ("register", "value"), (), \
     (timeout (1000))) \
  X (set_10, 0x10, "Write registers", \
     (registry_key_<l_registers, u16_be_>, u16_be_, u8_, \
      computed_count_<u16_be_, half_of_previous, 123>), \
     ("first", "count", "bytes", "values"), (), (timeout (1000)))
// clang-format on
LUMEX_PROTOCOL_DEFINE (l_requests, codec, request_header,
                       DEVICE_L_REQUEST_ROWS);

// The state block: work state, process, error, four unused registers,
// external inputs and outputs, two valves, one unused register, the tray
// temperature control (state, target, current, ready), four solvent
// counters and two 32-bit switch counters (high register first).
LUMEX_PROTOCOL_LAYOUT (state_t,
                       (u16_be_, u16_be_, u16_be_, array_<u16_be_, 4>, u16_be_,
                        u16_be_, u16_be_, u16_be_, u16_be_, u16_be_, u16_be_,
                        u16_be_, array_<u16_be_, 4>, u32_be_, u32_be_),
                       ("work", "process", "error", "unused", "io", "valve_1",
                        "valve_2", "unused_2", "tray_state", "tray_target",
                        "tray_current", "tray_ready", "solvents", "switches_1",
                        "switches_2"));

// clang-format off
#define DEVICE_L_REPLY_ROWS(X, X_AS) \
  X (report_032e, 0x032E, "State of the device", (), (), (state_t), \
     (idempotent))
// clang-format on
LUMEX_PROTOCOL_DEFINE (l_replies, codec, reply_header, DEVICE_L_REPLY_ROWS);

/// The codec of device L: the check high byte first; `requests` for the
/// device side (it reads requests), otherwise the host side.
inline lumex::core::framing::modbus_rtu::modbus_rtu_options_t
codec_options (bool requests)
{
  lumex::core::framing::modbus_rtu::modbus_rtu_options_t options;
  options.crc_order = lumex::core::framing::codec::byte_order::big;
  if (requests)
    options.functions
        = lumex::core::framing::modbus_rtu::modbus_rtu_requests ();
  return options;
}
} // namespace device_l
} // namespace lumex_tests

#endif // !LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_L_HPP
