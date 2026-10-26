// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Device M (LumexProtocolDeviceM.hpp): the reference exchanges of m1 (a
// request with its terminator and the reply after the framing) and of m2
// (whole frames through the header codec of core/framing, the preambles of
// both directions and the word-reversed CRC-32), every row of both tables,
// the values of the replies, the alias letters of one reply and the model
// layers that scale the collected data.

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
typedef m::device_m1_m3::query_qa_t device_m1_m3_qa;

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
  { "BV" /*@text*/ "\n", "\x06" },
  { "EM" /*@text*/ "\n", "EM" /*@text*/ ","
                         "660952" /*@text*/ ","
                         "6.0844" /*@text*/ ",0" },
  { "KI" /*@text*/ "\n", "KI" /*@text*/ ",0" },
  { "QA" /*@text*/ ",01\n", "QA" /*@text*/ ",2,1.5,2.5" },
  { "QA" /*@text*/ ",02\n",
    "QA" /*@text*/ ",0000000200,000500.000,001600.000" },
  { "QA" /*@text*/ ",01\n", "QA" /*@text*/ ",0" },
  { "QA" /*@text*/ ",01\n", "QA" /*@text*/ ",0000000000" },
  { "EF" /*@text*/ "\n", "EF" /*@text*/ ",0,1,1,0,4" },
  { "EW" /*@text*/ "\n", "EW" /*@text*/ ",40,42" },
  { "BV" /*@text*/ "\n", "\x15" },
  { "BV" /*@text*/ "\n", "BV" /*@text*/ ",0080" },
  { "EX" /*@text*/ "\n", "EX" /*@text*/ ",C2" },
  { "EM" /*@text*/ "\n", "EN" /*@text*/ ","
                         "660952" /*@text*/ ","
                         "6.0844" /*@text*/ ",0" },
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
  bytes_t const to_device = bytes ({ 0xA0 /*@m8*/, 0xE0 /*@m8*/ });
  bytes_t const from_device = bytes ({ 0x5E /*@m8*/, 0x74 /*@m8*/ });
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
  { "A0E00000000059D1A127C942" /*@m2frame*/,
    "5E740000400059D151582D35333130420000000000000000"
    "52374B0051582D353331304200000000000000003934345730333100"
    "5100E307091100000000000000000000000000009351CDAE" /*@m2frame*/ },
  { "A0E00000000065914039E2BC" /*@m2frame*/,
    "5E740000140065913C424C414E4B3E20202020202020202020202000DA1B7490"
    /*@m2frame*/ },
  { "A0E0000000003BF2103BD886" /*@m2frame*/,
    "5E74000014003BF23C424C414E4B3E0000000000000000000000000044E047FA"
    /*@m2frame*/ },
  { "A0E000001400968E53544F524544204E4541522042454E434820340079171E56"
    /*@m2frame*/,
    "5E7400000000968E0139D117" /*@m2frame*/ },
  { "A0E000000000B99A306F2FAB" /*@m2frame*/,
    "5E7400000400B99A0300000074078AB8" /*@m2frame*/ },
  { "A0E000000400FA4400000000C1762DEB" /*@m2frame*/,
    "5E7400003C00FA4456455354412D3037000000000000000037363730373937393734"
    "3733323636383434335834343038000000005100E30709111C0000000000000000"
    "0036A08B64" /*@m2frame*/ },
  { "A0E000000000109AEF747173" /*@m2frame*/,
    "5E7400000400109A01000000A7339268" /*@m2frame*/ },
  { "A0E00000040097560200000032F66AA1" /*@m2frame*/,
    "5E74000000009756678D026F" /*@m2frame*/ },
  { "A0E0000000007BE11D859067" /*@m2frame*/,
    "5E74000004007BE12D000000F83FF74F" /*@m2frame*/ },
  { "A0E0000000002788906EEB1B" /*@m2frame*/,
    "5E7400000400278801000000DB2D3AB4" /*@m2frame*/ },
  { "A0E0000004002A0402000000AD096503" /*@m2frame*/,
    "5E74000000002A04FB79A937" /*@m2frame*/ },
  { "A0E0000000008CE8E5FAFEFB" /*@m2frame*/,
    "5E74000000008CE83F7E3AFC" /*@m2frame*/ },
  { "A0E0000000000E87AE508BCA" /*@m2frame*/,
    "5E74000000000E8774D44FCD" /*@m2frame*/ },
  { "A0E0000000006DDE4CAB5C86" /*@m2frame*/,
    "5E74000004006DDE02010000714C42DC" /*@m2frame*/ },
  { "A0E00000040064F5010100019043382E" /*@m2frame*/,
    "5E740000000064F5BC8BF1FD" /*@m2frame*/ },
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
  EXPECT_EQ (request_text<device_m1::query_bv_t> (), kExchanges[0].request);
  EXPECT_EQ (request_text<device_m1::query_em_t> (), kExchanges[1].request);
  EXPECT_EQ (request_text<device_m1::start_ki_t> (), kExchanges[2].request);
  EXPECT_EQ (request_text<device_m1::query_qa_t> (1), kExchanges[3].request);
  EXPECT_EQ (request_text<device_m1::query_qa_t> (2), kExchanges[4].request);
  EXPECT_EQ (request_text<device_m1::query_ef_t> (), kExchanges[7].request);
  EXPECT_EQ (request_text<device_m1::query_ew_t> (), kExchanges[8].request);
  EXPECT_EQ (request_text<device_m1::query_ex_t> (), kExchanges[11].request);
  // The setting commands.
  EXPECT_EQ (request_text<device_m1::set_ls_t> (1),
             std::string ("LS" /*@text*/ ",1\n"));
  EXPECT_EQ (request_text<device_m1::set_vs_t> ('c'),
             std::string ("VS" /*@text*/ ",c\n"));
  EXPECT_EQ (request_text<device_m1::set_fj_t> ('-'),
             std::string ("FJ" /*@text*/ ",-\n"));
  EXPECT_EQ (request_text<device_m1::set_xj_t> (150),
             std::string ("XJ" /*@text*/ ",150\n"));
  EXPECT_EQ (request_text<device_m1::set_wc_t> (35),
             std::string ("WC" /*@text*/ ",35\n"));
  EXPECT_EQ (request_text<device_m1::set_cv_t> (0.5),
             std::string ("CV" /*@text*/ ",5\n"));
  EXPECT_EQ (request_text<device_m1::event_ng_t> (),
             std::string ("NG" /*@text*/ "\n"));
  for (char const *other :
       { "CZ" /*@text*/, "VF" /*@text*/, "LV" /*@text*/, "CE" /*@text*/ })
    EXPECT_TRUE (dev::index_of<device_m1> ().find_text (other, 2) != nullptr)
        << other;
}

TEST (LumexProtocolDeviceMTest,
      GivenTheReferenceRepliesOfM1_WhenMatched_ThenEveryOneFindsItsRow)
{
  char const *const rows[]
      = { "ack",      "query_em", "start_ki", "query_qa", "query_qa",
          "query_qa", "query_qa", "query_ef", "query_ew", "nak",
          "query_bv", "query_ex", "report_en" };
  for (std::size_t i = 0; i < sizeof (rows) / sizeof (rows[0]); ++i)
    EXPECT_EQ (row_of (kExchanges[i].reply), rows[i]) << i;

  EXPECT_TRUE ((dev::reply_round_trip<device_m1, device_m1::start_ki_t> (
      text_bytes (kExchanges[2].reply))));
  EXPECT_TRUE ((dev::reply_round_trip<device_m1, device_m1::query_ef_t> (
      text_bytes (kExchanges[7].reply))));
  EXPECT_TRUE ((dev::reply_round_trip<device_m1, device_m1::query_ew_t> (
      text_bytes (kExchanges[8].reply))));
  EXPECT_TRUE ((dev::reply_round_trip<device_m1, device_m1::query_bv_t> (
      text_bytes (kExchanges[10].reply))));
  EXPECT_TRUE ((dev::reply_round_trip<device_m1, device_m1::query_ex_t> (
      text_bytes (kExchanges[11].reply))));
  EXPECT_TRUE ((dev::reply_round_trip<device_m1, device_m1::query_em_t> (
      text_bytes (kExchanges[1].reply))));
  EXPECT_TRUE ((dev::reply_round_trip<device_m1, device_m1::report_en_t> (
      text_bytes (kExchanges[12].reply))));

  layout::decode_result_t<
      layout::reply_value_of<device_m1::query_em_t>::type> const identity
      = reply_of<device_m1, device_m1::query_em_t> (kExchanges[1].reply);
  ASSERT_TRUE (identity.has_value ());
  EXPECT_EQ (std::string (field::get<0> (*identity).data (),
                          field::get<0> (*identity).size ()),
             "660952" /*@text*/);
  EXPECT_EQ (field::get<2> (*identity), 0u);
  layout::decode_result_t<
      layout::reply_value_of<device_m1::query_bv_t>::type> const status
      = reply_of<device_m1, device_m1::query_bv_t> (kExchanges[10].reply);
  ASSERT_TRUE (status.has_value ());
  EXPECT_EQ (*status, 0x0080u);
  layout::decode_result_t<
      layout::reply_value_of<device_m1::query_ex_t>::type> const shift
      = reply_of<device_m1, device_m1::query_ex_t> (kExchanges[11].reply);
  ASSERT_TRUE (shift.has_value ());
  EXPECT_EQ (*shift, 0xC2u);
  // The identity under the letters of the request is not the alias row.
  EXPECT_FALSE (
      (reply_of<device_m1, device_m1::report_en_t> (kExchanges[1].reply))
          .has_value ());
}

TEST (LumexProtocolDeviceMTest,
      GivenTheCollectedData_WhenDecoded_ThenTheCountAndTheScaledPointsCome)
{
  layout::decode_result_t<
      layout::reply_value_of<device_m1::query_qa_t>::type> const two
      = reply_of<device_m1, device_m1::query_qa_t> (kExchanges[3].reply);
  ASSERT_TRUE (two.has_value ());
  EXPECT_EQ (field::get<0> (*two), 2u);
  ASSERT_TRUE (field::get<1> (*two).present);
  EXPECT_DOUBLE_EQ (field::get<0> (field::get<1> (*two).fields).value (), 1.5);
  ASSERT_TRUE (field::get<1> (field::get<1> (*two).fields).present);
  EXPECT_FALSE (
      field::get<1> (field::get<1> (field::get<1> (*two).fields).fields)
          .present);

  layout::decode_result_t<
      layout::reply_value_of<device_m1::query_qa_t>::type> const document
      = reply_of<device_m1, device_m1::query_qa_t> (kExchanges[4].reply);
  ASSERT_TRUE (document.has_value ());
  EXPECT_EQ (field::get<0> (*document), 200u);
  EXPECT_DOUBLE_EQ (field::get<0> (field::get<1> (*document).fields).value (),
                    500.0);
  layout::decode_result_t<layout::reply_value_of<device_m1_m3_qa>::type> const
      scaled
      = reply_of<m::device_m1_m3, device_m1_m3_qa> (kExchanges[4].reply);
  ASSERT_TRUE (scaled.has_value ());
  EXPECT_DOUBLE_EQ (field::get<0> (field::get<1> (*scaled).fields).value (),
                    10000.0);
  EXPECT_EQ (dev::index_of<m::device_m1_m2> ().describe (
                 dev::view_of (text_bytes (kExchanges[4].reply)),
                 index::frame_direction::rx),
             "RX device_m1_m2.query_qa (Collected data) count=200 "
             "points={1000 {3200 absent}}");

  for (std::size_t empty : { 5u, 6u })
    {
      layout::decode_result_t<
          layout::reply_value_of<device_m1::query_qa_t>::type> const none
          = reply_of<device_m1, device_m1::query_qa_t> (
              kExchanges[empty].reply);
      ASSERT_TRUE (none.has_value ()) << empty;
      EXPECT_EQ (field::get<0> (*none), 0u);
      EXPECT_FALSE (field::get<1> (*none).present);
    }
  // A point with more fraction digits than the field.
  EXPECT_FALSE (
      (reply_of<device_m1, device_m1::query_qa_t> ("QA" /*@text*/ ",1,1.0005"))
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
  EXPECT_EQ (text_of (field::get<0> (*info)), "QX-5310B" /*@data*/);
  EXPECT_EQ (std::string (field::get<1> (*info).view ().data (),
                          field::get<1> (*info).view ().size ()),
             "R7K" /*@data*/);
  EXPECT_EQ (std::string (field::get<3> (*info).view ().data (),
                          field::get<3> (*info).view ().size ()),
             "944W031" /*@data*/);
  EXPECT_EQ (field::get<4> (*info), 0x51u /*@m2*/);
  EXPECT_EQ (field::get<5> (*info), 2019u /*@year3*/);
  EXPECT_EQ (field::get<6> (*info), 0x09u /*@m2*/);
  EXPECT_EQ (field::get<7> (*info), 0x11u /*@m2*/);

  layout::decode_result_t<
      layout::reply_value_of<device_m2::query_9ab9_t>::type> const boards
      = m2_reply<device_m2::query_9ab9_t> (kPairs[4]);
  ASSERT_TRUE (boards.has_value ());
  EXPECT_EQ (field::get<0> (*boards), 0x03u /*@m2*/);

  layout::decode_result_t<
      layout::reply_value_of<device_m2::query_44fa_t>::type> const board
      = m2_reply<device_m2::query_44fa_t> (kPairs[5]);
  ASSERT_TRUE (board.has_value ());
  EXPECT_EQ (text_of (field::get<0> (*board)), "VESTA-07" /*@data*/);
  EXPECT_EQ (std::string (field::get<1> (*board).view ().data (),
                          field::get<1> (*board).view ().size ()),
             "7670797974732668443X4408" /*@data*/);
  EXPECT_EQ (field::get<6> (*board), 0x1Cu /*@m2*/);

  bytes_t out;
  ASSERT_TRUE ((layout::encode_request<device_m2, device_m2::set_8e96_t> (
                    out, "STORED NEAR BENCH 4" /*@data*/))
                   .has_value ());
  EXPECT_EQ (tail_of (payload_of (hex_bytes (kPairs[3].command)), 4), out);
  EXPECT_EQ (dev::index_of<device_m2> ().describe (
                 dev::view_of (payload_of (hex_bytes (kPairs[4].reply))),
                 index::frame_direction::rx),
             "RX device_m2.query_9ab9 (Number of boards) count="
                 + dev::dec (0x03 /*@m2*/) + " reserve=0");
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
