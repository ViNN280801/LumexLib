// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Test table of device A: the common application layer of a device family
// (family_a) and its message formats. Every message starts with the address
// of the device in the device network; three formats follow it:
// - a_new: [address][message] + fields (the family table, family_a);
// - a_old: [address][process][attribute][message] + fields (a process of the
//   firmware is addressed, a_old);
// - a_stream: [address][message][enable] (the device sends a message on its
//   own or stops, a_stream).
// A message from the device with address 0 is the system message of the
// link, once a second (a_system: its old content; the devices of the family
// override it). The devices B, C, D and H extend these tables.
#ifndef LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_A_HPP
#define LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_A_HPP

#include <cstddef>
#include <cstdint>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp"

namespace lumex_tests
{
namespace device_a
{
namespace index = lumex::core::protocol::index;
namespace layout = lumex::core::protocol::layout;
namespace registry = lumex::core::protocol::registry;

/// The framing of the family is the transport's (not named here).
struct codec
{
};

/// The severity of an error report.
LUMEX_DEFINE_REFLECTED_ENUM (severity, std::uint8_t, (warning, 0),
                             (recoverable, 1), (fatal, 2))

/// a_new: the address, then the message code; the fields follow it.
struct family_header
{
  typedef registry::table_vocabulary lumex_vocabulary;
  typedef index::key_at<1, 1> key_reader;
};

/// a_old: the address, the process, the attribute, then the message code.
struct process_header
{
  typedef registry::table_vocabulary lumex_vocabulary;
  typedef index::key_at<3, 1> key_reader;
};

/// a_system: the address 0 is the key; the message byte is a field.
struct system_header
{
  typedef index::key_at<0, 1> key_reader;
};

LUMEX_PROTOCOL_LAYOUT (info_t,
                       (u8_, u8_, u8_, u8_, u8_, u8_, u8_, u8_, u32_le_),
                       ("model", "firmware", "revision", "hardware", "year",
                        "month", "day", "configuration", "serial"));

LUMEX_PROTOCOL_LAYOUT (work_parameter_t, (u8_, u8_, i16_le_),
                       ("unit", "parameter", "value"));

LUMEX_PROTOCOL_LAYOUT (measurement_t,
                       (u32_le_<unit::millisecond>, i32_le_, i32_le_, i32_le_),
                       ("time", "absorbance", "photo", "reference"));

LUMEX_PROTOCOL_LAYOUT (unit_state_t, (u8_, u8_), ("unit", "state"));

LUMEX_PROTOCOL_LAYOUT (error_t, (u16_le_, enum_<u8_, severity>, u8_, i32_le_),
                       ("code", "severity", "has_value", "value"));

LUMEX_PROTOCOL_LAYOUT (event_t, (u8_, u8_), ("unit", "event"));

LUMEX_PROTOCOL_LAYOUT (parameter_t, (u16_le_, u16_le_, i32_le_),
                       ("requested", "parameter", "value"));

LUMEX_PROTOCOL_LAYOUT (can_packet_t, (u8_, array_<u8_, 8>), ("node", "data"));

// The old system message: the address of the master, the free space of
// the write buffer, the format errors (twice) and the check errors.
LUMEX_PROTOCOL_LAYOUT (old_system_t, (u8_, u8_, u16_le_, u8_, u8_, u8_),
                       ("message", "master", "free_space", "format_errors",
                        "format_errors_again", "check_errors"));

// clang-format off
#define DEVICE_A_FAMILY_ROWS(X, X_AS) \
  X (query_ad, 0xAD /*@r04*/, "Instrument information", (), (), (info_t), \
     (idempotent, timeout (1000))) \
  X (set_65, 0x65 /*@r04*/, "Work mode", (u8_), ("mode"), (u8_), \
     (timeout (1000))) \
  X (param_set_3f, 0x3F /*@r04*/, "Work parameter of a unit", \
     (u8_, u8_, i16_le_), ("unit", "parameter", "value"), \
     (work_parameter_t), (timeout (1000))) \
  X (param_get_d1, 0xD1 /*@r04*/, "Read a work parameter of a unit", \
     (u8_, u8_), ("unit", "parameter"), (work_parameter_t), \
     (idempotent, timeout (1000))) \
  X (stream_27, 0x27 /*@r04*/, "Measurement result", (), (), \
     (measurement_t), (stream (100))) \
  X (report_d9, 0xD9 /*@r04*/, "State of a unit", (), (), (unit_state_t), \
     (stream (1000))) \
  X (error_44, 0x44 /*@r04*/, "Error report", (), (), (error_t), \
     (log_level (level::warning))) \
  X (event_d4, 0xD4 /*@r04*/, "Event of a unit", (), (), (event_t), ()) \
  X (param_get_83, 0x83 /*@r04*/, "Read a service parameter", (u16_le_), \
     ("parameter"), (parameter_t), (idempotent)) \
  X (param_set_cc, 0xCC /*@r04*/, "Service parameter", (u16_le_, i32_le_), \
     ("parameter", "value"), (parameter_t), (dangerous)) \
  X (report_41, 0x41 /*@r04*/, "Packet of an internal node", (), (), \
     (can_packet_t), (log_level (level::trace)))
#define DEVICE_A_STREAM_ROWS(X, X_AS) \
  X (stream_on_27, 0x27 /*@r04*/, "Measurement result on or off", (u8_), \
     ("enable"), (), ()) \
  X (stream_on_d9, 0xD9 /*@r04*/, "State of a unit on or off", (u8_), \
     ("enable"), (), ())
#define DEVICE_A_OLD_ROWS(X, X_AS) \
  X (stream_af, 0xAF /*@r04*/, "A message on or off", (u8_, u8_), \
     ("message", "enable"), (), ())
#define DEVICE_A_SYSTEM_ROWS(X, X_AS) \
  X (keepalive_00, 0x00, "System message of the link", (), (), \
     (old_system_t), (stream (1000)))
// clang-format on

LUMEX_PROTOCOL_DEFINE (family_a, codec, family_header, DEVICE_A_FAMILY_ROWS);
LUMEX_PROTOCOL_DEFINE (a_stream, codec, family_header, DEVICE_A_STREAM_ROWS);
LUMEX_PROTOCOL_DEFINE (a_old, codec, process_header, DEVICE_A_OLD_ROWS);
LUMEX_PROTOCOL_DEFINE (a_system, codec, system_header, DEVICE_A_SYSTEM_ROWS);

/// The header of a_new: the address and the message code.
inline layout::bytes_t
new_header (std::uint8_t address, std::uint8_t message)
{
  return layout::bytes_t{ address, message };
}

/// The header of a_old: the address, the process, the attribute and the
/// message code.
inline layout::bytes_t
old_header (std::uint8_t address, std::uint8_t process, std::uint8_t attribute,
            std::uint8_t message)
{
  return layout::bytes_t{ address, process, attribute, message };
}

/// The frame of a request of `Row` of protocol `P` after `header`.
template <typename P, typename Row, typename... Args>
layout::bytes_t
request_frame (layout::bytes_t header, Args const &...args)
{
  layout::encode_request<P, Row> (header, args...);
  return header;
}
} // namespace device_a
} // namespace lumex_tests

#endif // !LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_A_HPP
