// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Device M (LumexProtocolDeviceM.hpp): the reference exchanges of m1 (a
// request with its terminator and the reply after the framing) and of m2
// (whole frames through the header codec of core/framing, the preambles of
// both directions and the word-reversed CRC-32), every row of both tables,
// the values of the replies, the alias letters of one reply and the model
// layers that scale the values of a counted reply.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <system_error>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/framing/header/LumexHeaderCodec.hpp"
#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/devices/LumexProtocolDevicesSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_m/LumexProtocolDeviceM.hpp"

namespace
{
namespace dev = lumex_tests::devices;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace index = lumex::core::protocol::index;
namespace dsl = lumex::core::protocol::dsl;
namespace m = lumex_tests::device_m;
namespace framing = lumex::core::framing;
using dev::bytes;
using dev::bytes_t;
using dev::hex_bytes;
using dev::tail_of;
using dev::text_bytes;
using m::device_m1;
using m::device_m2;
typedef m::device_m1_m3::text_13_t device_m1_m3_counted;

// ---------------------------------------------------------------------------
// m1
// ---------------------------------------------------------------------------

struct exchange_t
{
  char const *request;
  char const *reply;
};

// The request as written (with its line feed) and the reply after the
// framing (without its terminator).
exchange_t const kExchanges[] = {
  { "MA" /*@r11*/ "\n", "\x06" },
  { "BT" /*@r11*/ "\n", "BT" /*@r11*/ ","
                        "009257" /*@r11*/ ","
                        "0.9811" /*@r11*/ ",0" },
  { "QJ" /*@r11*/ "\n", "QJ" /*@r11*/ ",0" },
  { "HS" /*@r11*/ ",01\n", "HS" /*@r11*/ ",2,3.5,4.5" },
  { "HS" /*@r11*/ ",02\n", "HS" /*@r11*/ ",0000000300,000700.000,002100.000" },
  { "HS" /*@r11*/ ",01\n", "HS" /*@r11*/ ",0" },
  { "HS" /*@r11*/ ",01\n", "HS" /*@r11*/ ",0000000000" },
  { "BI" /*@r11*/ "\n", "BI" /*@r11*/ ",1,3,0,1,6" },
  { "BC" /*@r11*/ "\n", "BC" /*@r11*/ ",37,39" },
  { "MA" /*@r11*/ "\n", "\x15" },
  { "MA" /*@r11*/ "\n", "MA" /*@r11*/ ",0104" },
  { "BF" /*@r11*/ "\n", "BF" /*@r11*/ ",A5" },
  { "BT" /*@r11*/ "\n", "BE" /*@r11*/ ","
                        "009257" /*@r11*/ ","
                        "0.9811" /*@r11*/ ",0" },
};

// The identifier of the row the index of m1 finds for a reply.
std::string
row_of (char const *reply)
{
  index::frame_match_t const found
      = dev::index_of<device_m1> ().match (dev::view_of (text_bytes (reply)));
  return found.row != nullptr ? found.row->entry->identifier : "";
}

template <typename P, typename Row>
layout::decode_result_t<typename layout::reply_value_of<Row>::type>
reply_of (char const *reply)
{
  // A view of the static text itself: a token of the reply points into it.
  return layout::decode_reply<P, Row> (layout::bytes_view_t (
      reinterpret_cast<std::uint8_t const *> (reply), std::strlen (reply)));
}

template <typename Row, typename... Args>
std::string
request_text (Args const &...args)
{
  bytes_t out;
  EXPECT_TRUE (
      (layout::encode_request<device_m1, Row> (out, args...)).has_value ());
  out.push_back ('\n');
  return std::string (out.begin (), out.end ());
}

// ---------------------------------------------------------------------------
// m2
// ---------------------------------------------------------------------------

typedef framing::stage::word_reversed<
    framing::stage::crc<lumex::core::crc::parametric::crc32_mpeg2_spec_t,
                        framing::codec::byte_order::little>,
    4>
    word_crc;
typedef framing::header::header_codec<word_crc> m2_codec;

// One side of the link: its own preamble out, the other one in.
framing::header::header_options_t
m2_options (bool host)
{
  bytes_t const to_device = bytes ({ 0xA0 /*@r37*/, 0xE0 /*@r37*/ });
  bytes_t const from_device = bytes ({ 0x5E /*@r37*/, 0x74 /*@r37*/ });
  framing::header::header_options_t options;
  options.tx_preamble = host ? to_device : from_device;
  options.rx_preamble = host ? from_device : to_device;
  options.header_size = 8;
  options.length_offset = 4;
  options.length_width = 2;
  options.length_order = framing::codec::byte_order::little;
  options.counts = framing::header::length_counts::data;
  options.check_from = 0;
  options.max_frame_size = 4096;
  return options;
}

// The payload of a frame: the variant, the code and the data.
bytes_t
payload_of (bytes_t const &frame)
{
  bytes_t payload (frame.begin () + 2, frame.begin () + 4);
  payload.insert (payload.end (), frame.begin () + 6, frame.end () - 4);
  return payload;
}

bytes_t
framed (bytes_t const &payload, bool host)
{
  m2_codec const codec (m2_options (host));
  bytes_t out;
  EXPECT_TRUE (codec
                   .encode (framing::codec::bytes_view_t (payload.data (),
                                                          payload.size ()),
                            out)
                   .has_value ());
  return out;
}

struct pair_t
{
  char const *command;
  char const *reply;
};

// The reference pairs of m2, one per row in the order of the table.
pair_t const kPairs[] = {
  { "A0E00000000059D1A127C942" /*@r02*/,
    "5E740000400059D151582D3533313042000000000000000052374B0051582D35333"
    "13042000000000000000039343457303331005100E3070911000000000000000000"
    "00000000009351CDAE" /*@r02*/ },
  { "A0E00000000065914039E2BC" /*@r02*/, "5E740000140065913C424C414E4B3E"
                                         "20202020202020202020202000DA1B"
                                         "7490" /*@r02*/ },
  { "A0E0000000003BF2103BD886" /*@r02*/, "5E74000014003BF23C424C414E4B3E"
                                         "0000000000000000000000000044E0"
                                         "47FA" /*@r02*/ },
  { "A0E000001400968E53544F524544204E4541522042454E434820340079171E56" /*@r02*/
    ,
    "5E7400000000968E0139D117" /*@r02*/ },
  { "A0E000000000B99A306F2FAB" /*@r02*/,
    "5E7400000400B99A0300000074078AB8" /*@r02*/ },
  { "A0E000000400FA4400000000C1762DEB" /*@r02*/,
    "5E7400003C00FA4456455354412D303700000000000000003736373037393739373"
    "43733323636383434335834343038000000005100E30709111C0000000000000000"
    "0036A08B64" /*@r02*/ },
  { "A0E000000000109AEF747173" /*@r02*/,
    "5E7400000400109A01000000A7339268" /*@r02*/ },
  { "A0E00000040097560200000032F66AA1" /*@r02*/,
    "5E74000000009756678D026F" /*@r02*/ },
  { "A0E0000000007BE11D859067" /*@r02*/,
    "5E74000004007BE12D000000F83FF74F" /*@r02*/ },
  { "A0E0000000002788906EEB1B" /*@r02*/,
    "5E7400000400278801000000DB2D3AB4" /*@r02*/ },
  { "A0E0000004002A0402000000AD096503" /*@r02*/,
    "5E74000000002A04FB79A937" /*@r02*/ },
  { "A0E0000000008CE8E5FAFEFB" /*@r02*/, "5E74000000008CE83F7E3AFC" /*@r02*/ },
  { "A0E0000000000E87AE508BCA" /*@r02*/, "5E74000000000E8774D44FCD" /*@r02*/ },
  { "A0E0000000006DDE4CAB5C86" /*@r02*/,
    "5E74000004006DDE02010000714C42DC" /*@r02*/ },
  { "A0E00000040064F5010100019043382E" /*@r02*/,
    "5E740000000064F5BC8BF1FD" /*@r02*/ },
};

// The request of a row with fields decodes and encodes again; a row without
// fields has none.
template <typename Row>
void
check_request (bytes_t const &payload, std::true_type)
{
  EXPECT_TRUE (
      (dev::request_round_trip<device_m2, Row> (tail_of (payload, 4))));
}

template <typename Row>
void
check_request (bytes_t const &payload, std::false_type)
{
  EXPECT_EQ (payload.size (), 4u) << Row::identifier ();
}

template <typename Row>
void
check_reply (bytes_t const &payload, std::true_type)
{
  EXPECT_TRUE ((dev::reply_round_trip<device_m2, Row> (tail_of (payload, 4))));
}

template <typename Row>
void
check_reply (bytes_t const &payload, std::false_type)
{
  EXPECT_EQ (payload.size (), 4u) << Row::identifier ();
}

// The pair of row `Row`: both frames are what the codec makes of their
// payloads, the index finds the row for both, the request and the reply
// decode and encode again.
template <typename Row>
void
check_pair (pair_t const &pair)
{
  bytes_t const command = hex_bytes (pair.command);
  bytes_t const reply = hex_bytes (pair.reply);
  EXPECT_EQ (framed (payload_of (command), true), command) << pair.command;
  EXPECT_EQ (framed (payload_of (reply), false), reply) << pair.reply;
  index::protocol_index_t const &protocol = dev::index_of<device_m2> ();
  EXPECT_TRUE (dev::finds<Row> (protocol, payload_of (command), 4));
  EXPECT_TRUE (dev::finds<Row> (protocol, payload_of (reply), 4));
  check_request<Row> (
      payload_of (command),
      std::integral_constant<bool, (Row::fields_type::count::value > 0)> ());
  check_reply<Row> (
      payload_of (reply),
      std::integral_constant<bool, !std::is_same<typename Row::reply_type,
                                                 dsl::no_reply>::value> ());
}

template <typename Row>
layout::decode_result_t<typename layout::reply_value_of<Row>::type>
m2_reply (pair_t const &pair)
{
  return layout::decode_reply<device_m2, Row> (
      dev::view_of (tail_of (payload_of (hex_bytes (pair.reply)), 4)));
}

std::string
text_of (field::fixed_string<16> const &value)
{
  return std::string (value.view ().data (), value.view ().size ());
}
} // namespace

TEST (LumexProtocolDeviceMTest,
      GivenTheReferenceRequestsOfM1_WhenEncoded_ThenTheTextsAreTheReference)
{
  EXPECT_EQ (request_text<device_m1::text_17_t> (), kExchanges[0].request);
  EXPECT_EQ (request_text<device_m1::text_18_t> (), kExchanges[1].request);
  EXPECT_EQ (request_text<device_m1::text_01_t> (), kExchanges[2].request);
  EXPECT_EQ (request_text<device_m1::text_13_t> (1), kExchanges[3].request);
  EXPECT_EQ (request_text<device_m1::text_13_t> (2), kExchanges[4].request);
  EXPECT_EQ (request_text<device_m1::text_14_t> (), kExchanges[7].request);
  EXPECT_EQ (request_text<device_m1::text_16_t> (), kExchanges[8].request);
  EXPECT_EQ (request_text<device_m1::text_15_t> (), kExchanges[11].request);
  // The commands with a value.
  EXPECT_EQ (request_text<device_m1::text_03_t> (1),
             std::string ("DX" /*@r11*/ ",1\n"));
  EXPECT_EQ (request_text<device_m1::text_04_t> ('c'),
             std::string ("AX" /*@r11*/ ",c\n"));
  EXPECT_EQ (request_text<device_m1::text_05_t> ('-'),
             std::string ("IU" /*@r11*/ ",-\n"));
  EXPECT_EQ (request_text<device_m1::text_07_t> (150),
             std::string ("FU" /*@r11*/ ",150\n"));
  EXPECT_EQ (request_text<device_m1::text_09_t> (35),
             std::string ("CZ" /*@r11*/ ",35\n"));
  EXPECT_EQ (request_text<device_m1::text_11_t> (0.5),
             std::string ("ZA" /*@r11*/ ",5\n"));
  EXPECT_EQ (request_text<device_m1::text_02_t> (),
             std::string ("EL" /*@r11*/ "\n"));
  for (char const *other :
       { "ZK" /*@r11*/, "AI" /*@r11*/, "DA" /*@r11*/, "ZB" /*@r11*/ })
    EXPECT_TRUE (dev::index_of<device_m1> ().find_text (other, 2) != nullptr)
        << other;
}

TEST (LumexProtocolDeviceMTest,
      GivenTheReferenceRepliesOfM1_WhenMatched_ThenEveryOneFindsItsRow)
{
  char const *const rows[]
      = { "ack",     "text_18", "text_01", "text_13", "text_13",
          "text_13", "text_13", "text_14", "text_16", "nak",
          "text_17", "text_15", "text_19" };
  for (std::size_t i = 0; i < sizeof (rows) / sizeof (rows[0]); ++i)
    EXPECT_EQ (row_of (kExchanges[i].reply), rows[i]) << i;

  EXPECT_TRUE ((dev::reply_round_trip<device_m1, device_m1::text_01_t> (
      text_bytes (kExchanges[2].reply))));
  EXPECT_TRUE ((dev::reply_round_trip<device_m1, device_m1::text_14_t> (
      text_bytes (kExchanges[7].reply))));
  EXPECT_TRUE ((dev::reply_round_trip<device_m1, device_m1::text_16_t> (
      text_bytes (kExchanges[8].reply))));
  EXPECT_TRUE ((dev::reply_round_trip<device_m1, device_m1::text_17_t> (
      text_bytes (kExchanges[10].reply))));
  EXPECT_TRUE ((dev::reply_round_trip<device_m1, device_m1::text_15_t> (
      text_bytes (kExchanges[11].reply))));
  EXPECT_TRUE ((dev::reply_round_trip<device_m1, device_m1::text_18_t> (
      text_bytes (kExchanges[1].reply))));
  EXPECT_TRUE ((dev::reply_round_trip<device_m1, device_m1::text_19_t> (
      text_bytes (kExchanges[12].reply))));

  layout::decode_result_t<
      layout::reply_value_of<device_m1::text_18_t>::type> const triple
      = reply_of<device_m1, device_m1::text_18_t> (kExchanges[1].reply);
  ASSERT_TRUE (triple.has_value ());
  EXPECT_EQ (std::string (field::get<0> (*triple).data (),
                          field::get<0> (*triple).size ()),
             "009257" /*@r11*/);
  EXPECT_EQ (field::get<2> (*triple), 0u);
  layout::decode_result_t<
      layout::reply_value_of<device_m1::text_17_t>::type> const hex_word
      = reply_of<device_m1, device_m1::text_17_t> (kExchanges[10].reply);
  ASSERT_TRUE (hex_word.has_value ());
  EXPECT_EQ (*hex_word, 0x0104u);
  layout::decode_result_t<
      layout::reply_value_of<device_m1::text_15_t>::type> const hex_byte
      = reply_of<device_m1, device_m1::text_15_t> (kExchanges[11].reply);
  ASSERT_TRUE (hex_byte.has_value ());
  EXPECT_EQ (*hex_byte, 0xA5u);
  // The triple under the letters of the request is not the alias row.
  EXPECT_FALSE (
      (reply_of<device_m1, device_m1::text_19_t> (kExchanges[1].reply))
          .has_value ());
}

TEST (LumexProtocolDeviceMTest,
      GivenTheCountedReply_WhenDecoded_ThenTheCountAndTheScaledValuesCome)
{
  layout::decode_result_t<
      layout::reply_value_of<device_m1::text_13_t>::type> const two
      = reply_of<device_m1, device_m1::text_13_t> (kExchanges[3].reply);
  ASSERT_TRUE (two.has_value ());
  EXPECT_EQ (field::get<0> (*two), 2u);
  ASSERT_TRUE (field::get<1> (*two).present);
  EXPECT_DOUBLE_EQ (field::get<0> (field::get<1> (*two).fields).value (), 3.5);
  ASSERT_TRUE (field::get<1> (field::get<1> (*two).fields).present);
  EXPECT_FALSE (
      field::get<1> (field::get<1> (field::get<1> (*two).fields).fields)
          .present);

  layout::decode_result_t<
      layout::reply_value_of<device_m1::text_13_t>::type> const long_reply
      = reply_of<device_m1, device_m1::text_13_t> (kExchanges[4].reply);
  ASSERT_TRUE (long_reply.has_value ());
  EXPECT_EQ (field::get<0> (*long_reply), 300u);
  EXPECT_DOUBLE_EQ (
      field::get<0> (field::get<1> (*long_reply).fields).value (), 700.0);
  layout::decode_result_t<
      layout::reply_value_of<device_m1_m3_counted>::type> const scaled
      = reply_of<m::device_m1_m3, device_m1_m3_counted> (kExchanges[4].reply);
  ASSERT_TRUE (scaled.has_value ());
  EXPECT_DOUBLE_EQ (field::get<0> (field::get<1> (*scaled).fields).value (),
                    14000.0);
  EXPECT_EQ (dev::index_of<m::device_m1_m2> ().describe (
                 dev::view_of (text_bytes (kExchanges[4].reply)),
                 index::frame_direction::rx),
             "RX device_m1_m2.text_13 (Text command 13) count=300 "
             "values={1400 {4200 absent}}");

  for (std::size_t empty : { 5u, 6u })
    {
      layout::decode_result_t<
          layout::reply_value_of<device_m1::text_13_t>::type> const none
          = reply_of<device_m1, device_m1::text_13_t> (
              kExchanges[empty].reply);
      ASSERT_TRUE (none.has_value ()) << empty;
      EXPECT_EQ (field::get<0> (*none), 0u);
      EXPECT_FALSE (field::get<1> (*none).present);
    }
  // A value with more fraction digits than the field.
  EXPECT_FALSE (
      (reply_of<device_m1, device_m1::text_13_t> ("HS" /*@r11*/ ",1,1.0005"))
          .has_value ());
}

TEST (LumexProtocolDeviceMTest,
      GivenEveryReferencePairOfM2_WhenFramedAndMatched_ThenTheRowsAgree)
{
  check_pair<device_m2::query_d159_t> (kPairs[0]);
  check_pair<device_m2::query_9165_t> (kPairs[1]);
  check_pair<device_m2::query_f23b_t> (kPairs[2]);
  check_pair<device_m2::set_8e96_t> (kPairs[3]);
  check_pair<device_m2::query_9ab9_t> (kPairs[4]);
  check_pair<device_m2::query_44fa_t> (kPairs[5]);
  check_pair<device_m2::query_9a10_t> (kPairs[6]);
  check_pair<device_m2::set_5697_t> (kPairs[7]);
  check_pair<device_m2::query_e17b_t> (kPairs[8]);
  check_pair<device_m2::query_8827_t> (kPairs[9]);
  check_pair<device_m2::start_042a_t> (kPairs[10]);
  check_pair<device_m2::stop_e88c_t> (kPairs[11]);
  check_pair<device_m2::keepalive_870e_t> (kPairs[12]);
  check_pair<device_m2::query_de6d_t> (kPairs[13]);
  check_pair<device_m2::set_f564_t> (kPairs[14]);
  static_assert (dsl::command_count<device_m2>::value == 15,
                 "one reference pair per row");
}

TEST (
    LumexProtocolDeviceMTest,
    GivenTheInformationAndTheBoard_WhenDecoded_ThenTextsAndDatesAreTheReference)
{
  layout::decode_result_t<
      layout::reply_value_of<device_m2::query_d159_t>::type> const info
      = m2_reply<device_m2::query_d159_t> (kPairs[0]);
  ASSERT_TRUE (info.has_value ());
  EXPECT_EQ (text_of (field::get<0> (*info)), "QX-5310B" /*@r20*/);
  EXPECT_EQ (std::string (field::get<1> (*info).view ().data (),
                          field::get<1> (*info).view ().size ()),
             "R7K" /*@r20*/);
  EXPECT_EQ (std::string (field::get<3> (*info).view ().data (),
                          field::get<3> (*info).view ().size ()),
             "944W031" /*@r20*/);
  EXPECT_EQ (field::get<4> (*info), 0x51u /*@r33*/);
  EXPECT_EQ (field::get<5> (*info), 2019u /*@r33*/);
  EXPECT_EQ (field::get<6> (*info), 0x09u /*@r33*/);
  EXPECT_EQ (field::get<7> (*info), 0x11u /*@r33*/);

  layout::decode_result_t<
      layout::reply_value_of<device_m2::query_9ab9_t>::type> const boards
      = m2_reply<device_m2::query_9ab9_t> (kPairs[4]);
  ASSERT_TRUE (boards.has_value ());
  EXPECT_EQ (field::get<0> (*boards), 0x03u /*@r33*/);

  layout::decode_result_t<
      layout::reply_value_of<device_m2::query_44fa_t>::type> const board
      = m2_reply<device_m2::query_44fa_t> (kPairs[5]);
  ASSERT_TRUE (board.has_value ());
  EXPECT_EQ (text_of (field::get<0> (*board)), "VESTA-07" /*@r20*/);
  EXPECT_EQ (std::string (field::get<1> (*board).view ().data (),
                          field::get<1> (*board).view ().size ()),
             "7670797974732668443X4408" /*@r20*/);
  EXPECT_EQ (field::get<6> (*board), 0x1Cu /*@r33*/);

  bytes_t out;
  ASSERT_TRUE ((layout::encode_request<device_m2, device_m2::set_8e96_t> (
                    out, "STORED NEAR BENCH 4" /*@r20*/))
                   .has_value ());
  EXPECT_EQ (tail_of (payload_of (hex_bytes (kPairs[3].command)), 4), out);
  EXPECT_EQ (dev::index_of<device_m2> ().describe (
                 dev::view_of (payload_of (hex_bytes (kPairs[4].reply))),
                 index::frame_direction::rx),
             "RX device_m2.query_9ab9 (Number of boards) count="
                 + dev::dec (0x03 /*@r33*/) + " reserve=0");
}

TEST (LumexProtocolDeviceMTest,
      GivenAReplyFrame_WhenTheHostReadsItsOwnPreamble_ThenNothingArrives)
{
  m2_codec decoder (m2_options (true));
  bytes_t const command = hex_bytes (kPairs[4].command);
  std::size_t frames = 0;
  decoder.decode (
      framing::codec::bytes_view_t (command.data (), command.size ()),
      [&frames] (framing::codec::bytes_view_t, std::error_code failure)
        {
          if (!failure)
            ++frames;
        });
  EXPECT_EQ (frames, 0u);
  bytes_t const reply = hex_bytes (kPairs[4].reply);
  decoder.decode (
      framing::codec::bytes_view_t (reply.data (), reply.size ()),
      [&frames] (framing::codec::bytes_view_t, std::error_code failure)
        {
          if (!failure)
            ++frames;
        });
  EXPECT_EQ (frames, 1u);
}
