// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Test table of device M, two devices of one maker:
// - m1 (models m1_m1, m1_m2, m1_m3): tokenized text, a two-letter command,
//   a comma and the values; the device answers with an acknowledgement
//   character, a refusal character or the command letters and values (one
//   query is answered under other letters); the collected data of the last
//   points come as a counter and up to 20 decimal values whose scale the
//   model sets (m1_m2 doubles it, m1_m3 multiplies it by 20);
// - m2: binary, [variant:2][code:2] and the data after the framing of
//   core/framing (header codec: preamble, size, word-reversed CRC-32), the
//   reply carries the code of its request; texts are NUL-padded fields.
#ifndef LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_M_HPP
#define LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_M_HPP

#include <cstddef>
#include <cstdint>
#include <ratio>

#include "lumex/core/protocol/LumexProtocol"

namespace lumex_tests
{
namespace device_m
{
namespace index = lumex::core::protocol::index;
namespace layout = lumex::core::protocol::layout;
namespace field = lumex::core::protocol::field;

/// The framing is the transport's (a terminator on each side for m1, the
/// header codec for m2).
struct codec
{
};

// ---------------------------------------------------------------------------
// m1
// ---------------------------------------------------------------------------

struct m1_header
{
  typedef layout::tokenized_text_layout<','> layout_type;
};

/// Up to `N` decimal values with the scale `Value`: each one more is an
/// optional tail of the one before.
template <typename Value, std::size_t N> struct values_upto
{
  typedef field::optional_tail_<Value,
                                typename values_upto<Value, N - 1>::type>
      type;
};

template <typename Value> struct values_upto<Value, 1>
{
  typedef field::optional_tail_<Value> type;
};

typedef values_upto<field::dec_text_<6, 3>, 20>::type points_1;
typedef values_upto<field::dec_text_<6, 3, field::scale<std::ratio<2>>>,
                    20>::type points_2;
typedef values_upto<field::dec_text_<6, 3, field::scale<std::ratio<20>>>,
                    20>::type points_20;

LUMEX_PROTOCOL_LAYOUT (collected_t, (u32_le_, points_1), ("count", "points"));
LUMEX_PROTOCOL_LAYOUT (collected_2_t, (u32_le_, points_2),
                       ("count", "points"));
LUMEX_PROTOCOL_LAYOUT (collected_20_t, (u32_le_, points_20),
                       ("count", "points"));

LUMEX_PROTOCOL_LAYOUT (settings_t, (u8_, u8_, u8_, u8_, u8_),
                       ("gain", "range", "polarity", "purge", "response"));
LUMEX_PROTOCOL_LAYOUT (temperatures_t,
                       (u8_<unit::degree_celsius>, u8_<unit::degree_celsius>),
                       ("target", "current"));
LUMEX_PROTOCOL_LAYOUT (identity_t, (token_, token_, u8_),
                       ("serial", "version", "kind"));

// clang-format off
#define DEVICE_M1_ROWS(X, X_AS) \
  X (ack, "\x06", "Accepted", (), (), (), ()) \
  X (nak, "\x15", "Refused", (), (), (), ()) \
  X (start_ki, "KI" /*@text*/, "Zero the signal", (), (), (u8_), \
     (timeout (10000))) \
  X (event_ng, "NG" /*@text*/, "Mark an event", (), (), (), ()) \
  X (set_ls, "LS" /*@text*/, "Integrator gain", (u8_), ("gain"), (), ()) \
  X (set_vs, "VS" /*@text*/, "Recorder range", (char_), ("range"), (), ()) \
  X (set_fj, "FJ" /*@text*/, "Polarity", (char_), ("polarity"), (), ()) \
  X (set_cz, "CZ" /*@text*/, "Purge", (u8_), ("purge"), (), ()) \
  X (set_xj, "XJ" /*@text*/, "Baseline balance", (u16_le_), ("level"), (), \
     ()) \
  X (set_vf, "VF" /*@text*/, "Response time", (u8_), ("response"), (), ()) \
  X (set_wc, "WC" /*@text*/, "Cell temperature", \
     (u8_<unit::degree_celsius>), ("temperature"), (), ()) \
  X (set_lv, "LV" /*@text*/, "Key lock", (u8_), ("lock"), (), ()) \
  X (set_cv, "CV" /*@text*/, "Collection interval", \
     (u8_<scale<std::ratio<1, 10> >, unit::second>), ("interval"), (), ()) \
  X (set_ce, "CE" /*@text*/, "Collection on or off", (u8_), ("collect"), \
     (), ()) \
  X (query_qa, "QA" /*@text*/, "Collected data", (dec_text_<2, 0>), \
     ("number"), (collected_t), (idempotent)) \
  X (query_ef, "EF" /*@text*/, "Settings", (), (), (settings_t), \
     (idempotent)) \
  X (query_ex, "EX" /*@text*/, "Baseline shift", (), (), \
     (ascii_hex_<2>), (idempotent)) \
  X (query_ew, "EW" /*@text*/, "Temperatures", (), (), (temperatures_t), \
     (idempotent)) \
  X (query_bv, "BV" /*@text*/, "Status word", (), (), (ascii_hex_<4>), \
     (idempotent)) \
  X (query_em, "EM" /*@text*/, "Identity", (), (), (identity_t), \
     (idempotent)) \
  X (report_en, "EN" /*@text*/, "Identity under the other letters", (), (), \
     (identity_t), ())
#define DEVICE_M1_M2_ROWS(X, X_AS) \
  X (query_qa, "QA" /*@text*/, "Collected data", (dec_text_<2, 0>), \
     ("number"), (collected_2_t), (idempotent, model ("m1_m2")))
#define DEVICE_M1_M3_ROWS(X, X_AS) \
  X (query_qa, "QA" /*@text*/, "Collected data", (dec_text_<2, 0>), \
     ("number"), (collected_20_t), (idempotent, model ("m1_m3")))
// clang-format on

LUMEX_PROTOCOL_DEFINE (device_m1, codec, m1_header, DEVICE_M1_ROWS);
LUMEX_PROTOCOL_EXTEND (device_m1_m2, device_m1, DEVICE_M1_M2_ROWS);
LUMEX_PROTOCOL_EXTEND (device_m1_m3, device_m1, DEVICE_M1_M3_ROWS);

// ---------------------------------------------------------------------------
// m2
// ---------------------------------------------------------------------------

/// The payload after the framing: the variant (2 bytes), the code (2 bytes,
/// little endian), then the data.
struct m2_header
{
  typedef index::key_at<2, 2, index::key_byte_order::little> key_reader;
};

LUMEX_PROTOCOL_LAYOUT (m2_info_t,
                       (text_<16>, text_<4>, text_<16>, text_<8>, u16_le_,
                        u16_le_, u8_, u8_, u16_le_, text_<12>),
                       ("name", "model", "maker", "serial", "firmware", "year",
                        "month", "day", "reserve", "special"));
LUMEX_PROTOCOL_LAYOUT (board_query_t, (u16_le_, u16_le_),
                       ("board", "reserve"));
LUMEX_PROTOCOL_LAYOUT (board_t,
                       (text_<16>, text_<28>, u16_le_, u16_le_, u8_, u8_,
                        u16_le_, text_<8>),
                       ("name", "identifier", "firmware", "year", "month",
                        "day", "revision", "special"));
LUMEX_PROTOCOL_LAYOUT (count_t, (u16_le_, u16_le_), ("count", "reserve"));
LUMEX_PROTOCOL_LAYOUT (serial_port_t, (u8_, array_<u8_, 3>),
                       ("speed", "reserve"));
LUMEX_PROTOCOL_LAYOUT (lamps_t, (u8_, u8_, u8_, u8_),
                       ("first", "second", "reserve", "reserve_2"));
LUMEX_PROTOCOL_LAYOUT (lamps_request_t, (u8_, u8_, u8_, u8_),
                       ("first", "second", "reserve", "zero"));

// clang-format off
#define DEVICE_M2_ROWS(X, X_AS) \
  X (query_d159, 0xD159 /*@q16*/, "Information", (), (), (m2_info_t), \
     (idempotent)) \
  X (query_9165, 0x9165 /*@q16*/, "Extended specification", (), (), \
     (text_<20>), (idempotent)) \
  X (query_f23b, 0xF23B /*@q16*/, "User text", (), (), (text_<20>), \
     (idempotent)) \
  X (set_8e96, 0x8E96 /*@q16*/, "User text", (text_<20>), ("text"), (), \
     (dangerous)) \
  X (query_9ab9, 0x9AB9 /*@q16*/, "Number of boards", (), (), (count_t), \
     (idempotent)) \
  X (query_44fa, 0x44FA /*@q16*/, "Board information", (board_query_t), \
     ("board"), (board_t), (idempotent)) \
  X (query_9a10, 0x9A10 /*@q16*/, "Serial port", (), (), (serial_port_t), \
     (idempotent)) \
  X (set_5697, 0x5697 /*@q16*/, "Serial port", (serial_port_t), ("port"), \
     (), ()) \
  X (query_e17b, 0xE17B /*@q16*/, "Number of error records", (), (), \
     (count_t), (idempotent)) \
  X (query_8827, 0x8827 /*@q16*/, "Number of calibration records", (), (), \
     (count_t), (idempotent)) \
  X (start_042a, 0x042A /*@q16*/, "Sound", (u16_le_, u16_le_), \
     ("sound", "reserve"), (), ()) \
  X (stop_e88c, 0xE88C /*@q16*/, "Stop the sound", (), (), (), ()) \
  X (keepalive_870e, 0x870E /*@q16*/, "Keep the remote control", (), (), \
     (), (timeout (2500))) \
  X (query_de6d, 0xDE6D /*@q16*/, "Lamps", (), (), (lamps_t), \
     (idempotent)) \
  X (set_f564, 0xF564 /*@q16*/, "Lamps", (lamps_request_t), ("lamps"), (), \
     ())
// clang-format on

LUMEX_PROTOCOL_DEFINE (device_m2, codec, m2_header, DEVICE_M2_ROWS);
} // namespace device_m
} // namespace lumex_tests

#endif // !LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_M_HPP
