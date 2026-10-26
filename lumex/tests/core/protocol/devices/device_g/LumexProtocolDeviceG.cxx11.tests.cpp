// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Device G (lumex/tests/core/protocol/devices/device_g): every command of the
// table on reference frames, the interleaved bit masks of the state byte, the
// identity reply chosen by its length, the statistics message chosen by its
// first byte, the older protocol g_old with letters as keys, the describe
// lines of the log and the index.

#include <cstdint>
#include <initializer_list>
#include <string>
#include <system_error>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/devices/device_g/LumexProtocolDeviceGTable.hpp"

namespace
{
namespace tables = lumex_tests::tables_g;
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace index = lumex::core::protocol::index;
using lumex::core::protocol::error::protocol_errc;
using tables::bytes;
using tables::payload_of;
using tables::view_of;

typedef tables::device_g device;
typedef tables::family_g family;
typedef tables::old_g old_protocol;

template <typename P>
index::protocol_index_t const &
index_of ()
{
  index::index_result_t const &result = index::protocol_index<P> ();
  EXPECT_TRUE (result.has_value ());
  return *result;
}

template <typename P>
std::string
tx (layout::bytes_t const &frame)
{
  return index_of<P> ().describe (view_of (frame), index::frame_direction::tx);
}

template <typename P>
std::string
rx (layout::bytes_t const &frame)
{
  return index_of<P> ().describe (view_of (frame), index::frame_direction::rx);
}

layout::bytes_t
payload_bytes (layout::bytes_t const &frame)
{
  return layout::bytes_t (frame.begin () + 2, frame.end ());
}

template <typename P, typename Row>
std::error_code
request_error (layout::bytes_t const &frame)
{
  layout::decode_result_t<typename layout::request_of<Row>::type> const decoded
      = layout::decode_request<P, Row> (payload_of (frame));
  return decoded ? std::error_code () : decoded.error ();
}

template <typename P, typename Row>
std::error_code
reply_error (layout::bytes_t const &frame)
{
  layout::decode_result_t<typename layout::reply_value_of<Row>::type> const
      decoded
      = layout::decode_reply<P, Row> (payload_of (frame));
  return decoded ? std::error_code () : decoded.error ();
}

template <typename P, typename Row>
void
expect_request_round_trip (layout::bytes_t const &frame)
{
  layout::decode_result_t<typename layout::request_of<Row>::type> const decoded
      = layout::decode_request<P, Row> (payload_of (frame));
  ASSERT_TRUE (decoded.has_value ());
  layout::bytes_t again;
  ASSERT_TRUE (
      (layout::encode_request_record<P, Row> (again, *decoded).has_value ()));
  EXPECT_EQ (again, payload_bytes (frame));
}

template <typename P, typename Row>
void
expect_reply_round_trip (layout::bytes_t const &frame)
{
  layout::decode_result_t<typename layout::reply_value_of<Row>::type> const
      decoded
      = layout::decode_reply<P, Row> (payload_of (frame));
  ASSERT_TRUE (decoded.has_value ());
  layout::bytes_t again;
  ASSERT_TRUE ((layout::encode_reply<P, Row> (again, *decoded).has_value ()));
  EXPECT_EQ (again, payload_bytes (frame));
}

// A number as decimal text (the log shows keys and codes in decimal).
std::string
dec (unsigned value)
{
  return std::to_string (value);
}

// A frame of the family: the address, the key, the body.
layout::bytes_t
frame_of (std::uint8_t key, std::initializer_list<int> body)
{
  layout::bytes_t out = bytes ({ 0x7F /*@r31*/, key });
  layout::bytes_t const tail = bytes (body);
  out.insert (out.end (), tail.begin (), tail.end ());
  return out;
}

// A frame of the older protocol: the length, the command letter, the
// arguments.
layout::bytes_t
old_frame (char letter, std::initializer_list<int> arguments)
{
  layout::bytes_t out = bytes (
      { static_cast<int> (arguments.size ()) + 1, static_cast<int> (letter) });
  layout::bytes_t const tail = bytes (arguments);
  out.insert (out.end (), tail.begin (), tail.end ());
  return out;
}
} // namespace

TEST (
    LumexProtocolDeviceGTest,
    GivenTheTables_WhenTheIndicesAreBuilt_ThenThereIsNoDuplicateKeyInEitherProtocol)
{
  index::protocol_index_t const &built = index_of<device> ();
  EXPECT_STREQ (built.protocol_name (), "device_g");
  EXPECT_STREQ (built.base_name (), "family_g");
  EXPECT_EQ (built.rows ().size (), 7u);
  EXPECT_TRUE (index::keys_unique<device>::value);
  EXPECT_EQ (dsl::command_count<family>::value, 1u);
  index::protocol_index_t const &old_built = index_of<old_protocol> ();
  EXPECT_STREQ (old_built.protocol_name (), "old_g");
  EXPECT_STREQ (old_built.base_name (), "");
  EXPECT_EQ (old_built.rows ().size (), 2u);
  EXPECT_TRUE (index::keys_unique<old_protocol>::value);
  EXPECT_NE (old_built.find_integer ('Q' /*@r11*/), nullptr);
  EXPECT_NE (old_built.find_integer ('H' /*@r11*/), nullptr);
  EXPECT_EQ (old_built.find_integer ('X'), nullptr);
}

TEST (
    LumexProtocolDeviceGTest,
    GivenTheIdentityReply_WhenItIsFourBytesOrMore_ThenItIsTheSerialOrTheWholeRecord)
{
  layout::bytes_t const serial
      = frame_of (0xAD /*@r04*/, { 0x87, 0xD6, 0x12, 0x00 });
  layout::bytes_t const record = frame_of (
      0xAD /*@r04*/, { 0x2B, 0x05, 0x02, 0x09, 0x1C, 0x0A, 0x0F, 0x03 });
  expect_reply_round_trip<device, device::query_ad_t> (serial);
  expect_reply_round_trip<device, device::query_ad_t> (record);
  EXPECT_EQ (rx<device> (serial),
             "RX device_g.query_ad (Query the identity record) "
             "identity={serial=1234567}");
  EXPECT_EQ (rx<device> (record),
             "RX device_g.query_ad (Query the identity record) "
             "identity={type=43 sw=5 sub=2 hw=9 year=28 month=10 day=15 "
             "mask=3}");
  EXPECT_EQ (tx<device> (frame_of (0xAD /*@r04*/, {})),
             "TX device_g.query_ad (Query the identity record)");
  // Three bytes cannot be a serial; five or seven bytes cannot be a record.
  EXPECT_TRUE ((reply_error<device, device::query_ad_t> (
                   frame_of (0xAD /*@r04*/, { 0x87, 0xD6, 0x12 })))
               == protocol_errc::truncated);
  EXPECT_TRUE ((reply_error<device, device::query_ad_t> (
                   frame_of (0xAD /*@r04*/, { 1, 2, 3, 4, 5 })))
               == protocol_errc::truncated);
  EXPECT_TRUE ((reply_error<device, device::query_ad_t> (
                   frame_of (0xAD /*@r04*/, { 1, 2, 3, 4, 5, 6, 7, 8, 9 })))
               == protocol_errc::trailing_bytes);
}

TEST (LumexProtocolDeviceGTest,
      GivenTheWorkingMode_WhenEncodedAndDecoded_ThenOnlyTheTwoModesAreAccepted)
{
  layout::bytes_t out;
  ASSERT_TRUE ((layout::encode_request<device, device::set_65_t> (
                    out, tables::power_mode::m_3f)
                    .has_value ()));
  EXPECT_EQ (out, bytes ({ 0x3F /*@r04*/ }));
  layout::bytes_t const on = frame_of (0x65 /*@r04*/, { 0x3F /*@r04*/ });
  layout::bytes_t const off = frame_of (0x65 /*@r04*/, { 0x15 /*@r04*/ });
  expect_request_round_trip<device, device::set_65_t> (on);
  expect_request_round_trip<device, device::set_65_t> (off);
  EXPECT_EQ (tx<device> (on), "TX device_g.set_65 (Set the working mode) "
                              "mode=m_3f");
  EXPECT_EQ (tx<device> (off), "TX device_g.set_65 (Set the working mode) "
                               "mode=m_15");
  EXPECT_TRUE ((request_error<device, device::set_65_t> (
                   frame_of (0x65 /*@r04*/, { 0x16 })))
               == protocol_errc::invalid_value);
  EXPECT_TRUE (dsl::flags_of (device::set_65_t::options ())
               & dsl::row_flags::dangerous);
}

TEST (
    LumexProtocolDeviceGTest,
    GivenTheStateByte_WhenDecoded_ThenTheEvenBitsAreRunningAndTheOddBitsAreFaults)
{
  layout::bytes_t const all_running
      = frame_of (0xD9 /*@r04*/, { 0xAD /*@r04*/, 0x3F /*@r04*/, 0x55 });
  layout::bytes_t const all_fault
      = frame_of (0xD9 /*@r04*/, { 0xAD /*@r04*/, 0x3F /*@r04*/, 0xAA });
  layout::bytes_t const mixed
      = frame_of (0xD9 /*@r04*/, { 0xAD /*@r04*/, 0x3F /*@r04*/, 0x69 });
  expect_reply_round_trip<device, device::report_d9_t> (all_running);
  expect_reply_round_trip<device, device::report_d9_t> (all_fault);
  expect_reply_round_trip<device, device::report_d9_t> (mixed);
  EXPECT_EQ (rx<device> (all_running),
             "RX device_g.report_d9 (Report the state) node="
                 + dec (0xAD /*@r04*/) + " mode=" + dec (0x3F /*@r04*/)
                 + " channels={running=15 fault=0}");
  EXPECT_EQ (rx<device> (all_fault),
             "RX device_g.report_d9 (Report the state) node="
                 + dec (0xAD /*@r04*/) + " mode=" + dec (0x3F /*@r04*/)
                 + " channels={running=0 fault=15}");
  EXPECT_EQ (rx<device> (mixed),
             "RX device_g.report_d9 (Report the state) node="
                 + dec (0xAD /*@r04*/) + " mode=" + dec (0x3F /*@r04*/)
                 + " channels={running=9 fault=6}");
  layout::decode_result_t<
      layout::reply_value_of<device::report_d9_t>::type> const decoded
      = layout::decode_reply<device, device::report_d9_t> (payload_of (mixed));
  ASSERT_TRUE (decoded.has_value ());
  EXPECT_EQ (field::get<2> (*decoded).raw, 0x69u);
}

TEST (LumexProtocolDeviceGTest,
      GivenAnErrorAndANodeQuery_WhenEncodedAndDecoded_ThenTheyAreOneByte)
{
  layout::bytes_t const failure = frame_of (0x44 /*@r04*/, { 0x07 });
  expect_reply_round_trip<device, device::error_44_t> (failure);
  EXPECT_EQ (rx<device> (failure), "RX device_g.error_44 (Report an error) "
                                   "reply=7");
  layout::bytes_t out;
  ASSERT_TRUE (
      (layout::encode_request<device, device::query_b8_t> (out, 0xAD /*@r04*/)
           .has_value ()));
  EXPECT_EQ (out, bytes ({ 0xAD /*@r04*/ }));
  layout::bytes_t const request = frame_of (0xB8 /*@r04*/, { 0xAD /*@r04*/ });
  expect_request_round_trip<device, device::query_b8_t> (request);
  EXPECT_EQ (tx<device> (request), "TX device_g.query_b8 (Query a node) "
                                   "node="
                                       + dec (0xAD /*@r04*/));
}

TEST (LumexProtocolDeviceGTest,
      GivenTheStatistics_WhenTheTypeIsRead_ThenItChoosesTheLayoutOfTheRest)
{
  layout::bytes_t const cpu = frame_of (
      0xAB /*@r04*/, { 0xAD /*@r04*/, 0x59, 0x01, 0x40, 0xE2, 0x01, 0x00 });
  layout::bytes_t const stack
      = frame_of (0xAB /*@r04*/, { 0x65 /*@r04*/, 0x03, 0x04, 0x00, 0x02 });
  expect_reply_round_trip<device, device::report_ab_t> (cpu);
  expect_reply_round_trip<device, device::report_ab_t> (stack);
  EXPECT_EQ (rx<device> (cpu),
             "RX device_g.report_ab (Report statistics) statistics=case "
                 + dec (0xAD /*@r04*/) + ": {load=34.5 % switches=123456}");
  EXPECT_EQ (rx<device> (stack),
             "RX device_g.report_ab (Report statistics) statistics=case "
                 + dec (0x65 /*@r04*/) + ": {task=3 priority=4 used=512}");
  EXPECT_TRUE ((reply_error<device, device::report_ab_t> (
                   frame_of (0xAB /*@r04*/, { 0x99, 0x03, 0x04, 0x00, 0x02 })))
               == protocol_errc::unknown_case);
}

TEST (LumexProtocolDeviceGTest,
      GivenTheDebugCounters_WhenDecoded_ThenTheSignedWordsKeepTheirSign)
{
  layout::bytes_t const frame = frame_of (
      0x8E /*@r04*/, { 0xFF, 0xFF, 0x2C, 0x01, 0xFF, 0x7F, 0x00, 0x80 });
  expect_reply_round_trip<device, device::debug_8e_t> (frame);
  EXPECT_EQ (rx<device> (frame), "RX device_g.debug_8e (Report the debug "
                                 "counters) c0=-1 c1=300 c2=32767 c3=-32768");
}

TEST (
    LumexProtocolDeviceGTest,
    GivenTheOlderProtocol_WhenBothLetterCommandsAreEncoded_ThenTheLengthByteLeadsAndTheMaskIsTheArgument)
{
  layout::bytes_t const a_request = old_frame ('Q' /*@r11*/, {});
  layout::bytes_t const a_reply = old_frame ('Q' /*@r11*/, { 0x80 });
  layout::bytes_t const b_both = old_frame ('H' /*@r11*/, { 0xC0 });
  layout::bytes_t const b_none = old_frame ('H' /*@r11*/, { 0x00 });
  EXPECT_EQ (a_request, bytes ({ 0x01, 'Q' /*@r11*/ }));
  EXPECT_EQ (b_both, bytes ({ 0x02, 'H' /*@r11*/, 0xC0 }));
  expect_reply_round_trip<old_protocol, old_protocol::letter_a_t> (a_reply);
  expect_request_round_trip<old_protocol, old_protocol::letter_b_t> (b_both);
  expect_request_round_trip<old_protocol, old_protocol::letter_b_t> (b_none);
  EXPECT_EQ (tx<old_protocol> (a_request),
             "TX old_g.letter_a (Letter command A)");
  EXPECT_EQ (rx<old_protocol> (a_reply),
             "RX old_g.letter_a (Letter command A) reply={first=true "
             "second=false}");
  EXPECT_EQ (tx<old_protocol> (b_both),
             "TX old_g.letter_b (Letter command B) groups={first=true "
             "second=true}");
  EXPECT_EQ (tx<old_protocol> (b_none),
             "TX old_g.letter_b (Letter command B) groups={first=false "
             "second=false}");
  layout::bytes_t out;
  ASSERT_TRUE (
      (layout::encode_request<old_protocol, old_protocol::letter_b_t> (
           out, field::value_of<
                    field::split_<field::u8_<>, tables::groups_t>>::type ())
           .has_value ()));
  EXPECT_EQ (out, bytes ({ 0x00 }));
}

TEST (
    LumexProtocolDeviceGTest,
    GivenAFrameWithAKeyThatIsNoLetterOfTheTable_WhenDescribed_ThenItIsUnknown)
{
  EXPECT_EQ (tx<old_protocol> (old_frame ('X', {})),
             "TX unknown key=0x58 len=2: 01 58");
}
