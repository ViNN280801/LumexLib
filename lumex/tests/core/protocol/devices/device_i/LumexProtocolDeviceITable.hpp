// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// The test table of device I: a device whose messages have no address,
// [command][data...], whose codes mean something else than the same codes of
// devices E, F and G (the key has a meaning only inside one protocol), whose
// measurements are scaled counts and floating-point numbers, and which speaks
// another protocol (i_alt) after power-on until an out-of-band byte sequence
// switches it to this one (and another sequence switches it back).

#ifndef LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_I_TABLE_HPP
#define LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_I_TABLE_HPP

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <ratio>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp"

namespace lumex_tests
{
namespace tables_i
{
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace registry = lumex::core::protocol::registry;
namespace index = lumex::core::protocol::index;

/// What a measurement command asks the device to do.
LUMEX_DEFINE_REFLECTED_ENUM (run_state, std::uint8_t, (c_15, 0x15 /*@r04*/),
                             (c_ad, 0xAD /*@r04*/), (c_65, 0x65 /*@r04*/),
                             (c_3f, 0x3F /*@r04*/))

struct codec
{
};

/// The message format: the key is the first byte (there is no address).
struct plain_header
{
  using lumex_vocabulary = registry::table_vocabulary;
  using key_reader = index::key_at<0, 1>;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;
};

/// The working parameters of the device: the key is an index, the value 16
/// bits.
// clang-format off
#define WORK_PARAMS(P, P_AS) \
  P (w_15, (0x15 /*@r04*/), "Working parameter A", (u8_), RW, (limits (0, 3))) \
  P (w_ad, (0xAD /*@r04*/), "Working parameter B", (u16_le_), RW, (limits (0, 1000)))
// clang-format on
LUMEX_PROTOCOL_REGISTRY (work, WORK_PARAMS);

LUMEX_PROTOCOL_LAYOUT (info_t,
                       (u8_, i32_le_, i32_le_<unit::millisecond>, u8_, u8_),
                       ("type", "serial", "resource", "sw", "sub"));

LUMEX_PROTOCOL_LAYOUT (counters_t, (array_<u16_le_, 10>), ("counters"));

LUMEX_PROTOCOL_LAYOUT (pulse_t,
                       (f32_le_<unit::microvolt>, f32_le_<unit::microvolt>,
                        f32_le_<unit::microvolt>),
                       ("ch_a", "ch_b", "ch_c"));

/// The counts of the converter times the step of the converter are volts.
LUMEX_PROTOCOL_LAYOUT (
    counts_t,
    (u32_le_<unit::millisecond>,
     u32_le_<scale<std::ratio<2551021, 10000000000000>>, unit::volt>,
     u32_le_<scale<std::ratio<2551021, 10000000000000>>, unit::volt>,
     u32_le_<scale<std::ratio<2551021, 10000000000000>>, unit::volt>),
    ("time", "ch_a", "ch_b", "ch_c"));

LUMEX_PROTOCOL_LAYOUT (fault_t, (u8_, optional_tail_<u16_le_>),
                       ("code", "value"));

LUMEX_PROTOCOL_LAYOUT (
    work_value_t,
    (registry::registry_key_<work, u8_>, u8_,
     registry::registry_value_<work, 0, u16_le_, registry::access_t::read>),
    ("parameter", "zero", "value"));

// clang-format off
#define DEVICE_I_ROWS(X, X_AS) \
  X (query_ad, 0xAD /*@r04*/, "Query the identity record", (), (), (info_t), \
     (idempotent, timeout (1200), retries (2))) \
  X (report_65, 0x65 /*@r04*/, "Report the link counters", (), (), (counters_t), \
     (stream (1000))) \
  X (start_3f, 0x3F /*@r04*/, "Start or stop a run", (enum_<u8_, run_state>), \
     ("state"), (), ()) \
  X (event_72, 0x72 /*@r04*/, "Make one pulse", (), (), (pulse_t), ()) \
  X (event_27, 0x27 /*@r04*/, "Report a measurement", (), (), (counts_t), \
     (stream (250))) \
  X (set_38, 0x38 /*@r04*/, "Enable the status messages", (u8_), ("enable"), (), ()) \
  X (query_4b, 0x4B /*@r04*/, "Query the status", (), (), (u16_le_), (idempotent)) \
  X (error_f8, 0xF8 /*@r04*/, "Report an error", (), (), (fault_t), ()) \
  X (set_b2, 0xB2 /*@r04*/, "Lock the keyboard", (u8_), ("lock"), (), ()) \
  X (query_c7, 0xC7 /*@r04*/, "Query the keyboard", (), (), (u8_), (idempotent)) \
  X (param_get_bb, 0xBB /*@r04*/, "Read a working parameter", \
     (registry_key_<work, u8_>), ("parameter"), (work_value_t), \
     (idempotent)) \
  X (param_set_00, 0x00 /*@r04*/, "Write a working parameter", \
     (registry_key_<work, u8_>, u8_, registry_value_<work, 0, u16_le_>), \
     ("parameter", "zero", "value"), (), ()) \
  X (query_8b, 0x8B /*@r04*/, "Query the state of the device", (), (), (u8_), \
     (idempotent)) \
  X (debug_99, 0x99 /*@r04*/, "Report the debug word", (), (), (u32_le_), \
     (stream (600))) \
  X (switch_back_7b, 0x7B /*@r37*/, "Switch back to the other protocol", \
     (u8_, u8_, u8_, u8_), ("m1", "m2", "m3", "m4"), (), (dangerous))
// clang-format on
LUMEX_PROTOCOL_DEFINE (device_i, codec, plain_header, DEVICE_I_ROWS);

/// The protocol of the device after power-on (i_alt): this table only knows
/// the frame that switches the device to the protocol above.
// clang-format off
#define ALT_I_ROWS(X, X_AS) \
  X (switch_f7, 0xF7 /*@r37*/, "Switch to the main protocol", \
     (u8_, u8_, u8_, u8_, u8_), ("m1", "m2", "m3", "m4", "end"), (), \
     (dangerous))
// clang-format on
LUMEX_PROTOCOL_DEFINE (alt_i, codec, plain_header, ALT_I_ROWS);

/// The out-of-band sequence that switches the device to device_i (the last
/// byte is the frame end of the byte-stuffing link, sent in case the device
/// is switched already).
inline layout::bytes_t
switch_in ()
{
  layout::bytes_t out;
  out.push_back (0xF7 /*@r37*/);
  out.push_back (0xF8 /*@r37*/);
  out.push_back (0xC9 /*@r37*/);
  out.push_back (0xAB /*@r37*/);
  out.push_back (0x3E /*@r37*/);
  out.push_back (0xC0);
  return out;
}

/// The sequence that switches it back (the first byte differs only).
inline layout::bytes_t
switch_back ()
{
  layout::bytes_t out;
  out.push_back (0x7B /*@r37*/);
  out.push_back (0xF8 /*@r37*/);
  out.push_back (0xC9 /*@r37*/);
  out.push_back (0xAB /*@r37*/);
  out.push_back (0x3E /*@r37*/);
  return out;
}

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

/// The payload of a frame: the bytes after the key.
inline layout::bytes_view_t
payload_of (layout::bytes_t const &frame)
{
  return layout::bytes_view_t (frame.data () + 1, frame.size () - 1);
}
} // namespace tables_i
} // namespace lumex_tests

#endif // !LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_I_TABLE_HPP
