// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Test table of device M, two devices of one maker:
// - m1 (models m1_m1, m1_m2, m1_m3): tokenized text, a two-letter command,
//   a comma and the values; the device answers with an acknowledgement
//   character, a refusal character or the command letters and values (one
//   query is answered under other letters); one reply is a counter and
//   up to 20 decimal values whose scale the model sets (m1_m2 doubles
//   it, m1_m3 multiplies it by 20);
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

typedef values_upto<field::dec_text_<6, 3>, 20>::type values_1;
typedef values_upto<field::dec_text_<6, 3, field::scale<std::ratio<2>>>,
                    20>::type values_2;
typedef values_upto<field::dec_text_<6, 3, field::scale<std::ratio<20>>>,
                    20>::type values_20;

LUMEX_PROTOCOL_LAYOUT (counted_t, (u32_le_, values_1), ("count", "values"));
LUMEX_PROTOCOL_LAYOUT (counted_2_t, (u32_le_, values_2), ("count", "values"));
LUMEX_PROTOCOL_LAYOUT (counted_20_t, (u32_le_, values_20),
                       ("count", "values"));

LUMEX_PROTOCOL_LAYOUT (quintet_t, (u8_, u8_, u8_, u8_, u8_),
                       ("value_1", "value_2", "value_3", "value_4",
                        "value_5"));
LUMEX_PROTOCOL_LAYOUT (pair_t, (u8_, u8_), ("value_1", "value_2"));
LUMEX_PROTOCOL_LAYOUT (triple_t, (token_, token_, u8_),
                       ("first", "second", "third"));

// clang-format off
#define DEVICE_M1_ROWS(X, X_AS) \
  X (ack, "\x06", "Accepted", (), (), (), ()) \
  X (nak, "\x15", "Refused", (), (), (), ()) \
  X (text_01, "QJ" /*@r11*/, "Text command 1", (), (), (u8_), \
     (timeout (12000))) \
  X (text_02, "EL" /*@r11*/, "Text command 2", (), (), (), ()) \
  X (text_03, "DX" /*@r11*/, "Text command 3", (u8_), ("value"), (), ()) \
  X (text_04, "AX" /*@r11*/, "Text command 4", (char_), ("value"), (), ()) \
  X (text_05, "IU" /*@r11*/, "Text command 5", (char_), ("value"), (), ()) \
  X (text_06, "ZK" /*@r11*/, "Text command 6", (u8_), ("value"), (), ()) \
  X (text_07, "FU" /*@r11*/, "Text command 7", (u16_le_), ("value"), (), \
     ()) \
  X (text_08, "AI" /*@r11*/, "Text command 8", (u8_), ("value"), (), ()) \
  X (text_09, "CZ" /*@r11*/, "Text command 9", (u8_), \
     ("value"), (), ()) \
  X (text_10, "DA" /*@r11*/, "Text command 10", (u8_), ("value"), (), ()) \
  X (text_11, "ZA" /*@r11*/, "Text command 11", \
     (u8_<scale<std::ratio<1, 10> > >), ("value"), (), ()) \
  X (text_12, "ZB" /*@r11*/, "Text command 12", (u8_), ("value"), \
     (), ()) \
  X (text_13, "HS" /*@r11*/, "Text command 13", (dec_text_<2, 0>), \
     ("value"), (counted_t), (idempotent)) \
  X (text_14, "BI" /*@r11*/, "Text command 14", (), (), (quintet_t), \
     (idempotent)) \
  X (text_15, "BF" /*@r11*/, "Text command 15", (), (), \
     (ascii_hex_<2>), (idempotent)) \
  X (text_16, "BC" /*@r11*/, "Text command 16", (), (), (pair_t), \
     (idempotent)) \
  X (text_17, "MA" /*@r11*/, "Text command 17", (), (), (ascii_hex_<4>), \
     (idempotent)) \
  X (text_18, "BT" /*@r11*/, "Text command 18", (), (), (triple_t), \
     (idempotent)) \
  X (text_19, "BE" /*@r11*/, "Text command 18 under other letters", (), (), \
     (triple_t), ())
#define DEVICE_M1_M2_ROWS(X, X_AS) \
  X (text_13, "HS" /*@r11*/, "Text command 13", (dec_text_<2, 0>), \
     ("value"), (counted_2_t), (idempotent, model ("m1_m2")))
#define DEVICE_M1_M3_ROWS(X, X_AS) \
  X (text_13, "HS" /*@r11*/, "Text command 13", (dec_text_<2, 0>), \
     ("value"), (counted_20_t), (idempotent, model ("m1_m3")))
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
  X (query_d159, 0xD159 /*@r09*/, "Information", (), (), (m2_info_t), \
     (idempotent)) \
  X (query_9165, 0x9165 /*@r09*/, "Extended specification", (), (), \
     (text_<20>), (idempotent)) \
  X (query_f23b, 0xF23B /*@r09*/, "User text", (), (), (text_<20>), \
     (idempotent)) \
  X (set_8e96, 0x8E96 /*@r09*/, "User text", (text_<20>), ("text"), (), \
     (dangerous)) \
  X (query_9ab9, 0x9AB9 /*@r09*/, "Number of boards", (), (), (count_t), \
     (idempotent)) \
  X (query_44fa, 0x44FA /*@r09*/, "Board information", (board_query_t), \
     ("board"), (board_t), (idempotent)) \
  X (query_9a10, 0x9A10 /*@r09*/, "Serial port", (), (), (serial_port_t), \
     (idempotent)) \
  X (set_5697, 0x5697 /*@r09*/, "Serial port", (serial_port_t), ("port"), \
     (), ()) \
  X (query_e17b, 0xE17B /*@r09*/, "Number of error records", (), (), \
     (count_t), (idempotent)) \
  X (query_8827, 0x8827 /*@r09*/, "Number of calibration records", (), (), \
     (count_t), (idempotent)) \
  X (start_042a, 0x042A /*@r09*/, "Sound", (u16_le_, u16_le_), \
     ("sound", "reserve"), (), ()) \
  X (stop_e88c, 0xE88C /*@r09*/, "Stop the sound", (), (), (), ()) \
  X (keepalive_870e, 0x870E /*@r09*/, "Keep the remote control", (), (), \
     (), (timeout (2500))) \
  X (query_de6d, 0xDE6D /*@r09*/, "Lamps", (), (), (lamps_t), \
     (idempotent)) \
  X (set_f564, 0xF564 /*@r09*/, "Lamps", (lamps_request_t), ("lamps"), (), \
     ())
// clang-format on

LUMEX_PROTOCOL_DEFINE (device_m2, codec, m2_header, DEVICE_M2_ROWS);
} // namespace device_m
} // namespace lumex_tests

#endif // !LUMEX_TESTS_CORE_PROTOCOL_DEVICES_DEVICE_M_HPP
