// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Device B (LumexProtocolDeviceB.hpp): the two device information replies
// of reference, every request and reply of b2 and b1 on reference vectors,
// the registries (limits, access, the clash of two numbers, models and
// firmware, the comparison with the list the device reports), the alignment
// of b2, the model layer b2_m2, the system message of the link and the
// request header of b1 that the replies do not have.

#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/devices/LumexProtocolDevicesSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_b/LumexProtocolDeviceB.hpp"

namespace
{
namespace dev = lumex_tests::devices;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace index = lumex::core::protocol::index;
namespace registry = lumex::core::protocol::registry;
namespace error = lumex::core::protocol::error;
namespace b = lumex_tests::device_b;
using b::b1_params;
using b::b1_params_unique;
using b::b2_service;
using b::b2_work;
using b::device_b1;
using b::device_b1_rx;
using b::device_b2;
using b::device_b2_m2;
using b::kAddress;
using dev::bytes;
using dev::bytes_t;
using dev::join;
using dev::tail_of;
using dev::text_bytes;

// The device information of two units, as the device sent them in a report
// of the transport: the report number 1 and the length of the message come
// first.
bytes_t const kInformationOne = bytes (
    { 0x01, 0x0E, 0x1A /*@r31*/, 0xAD /*@r04*/, 0xD2 /*@r04*/, 0xAD /*@r04*/,
      0x90 /*@r04*/, 0x15 /*@r04*/, 0x9B /*@r15*/, 0x07 /*@r28*/,
      0x06 /*@r06*/, 0x02, 0xFC, 0x12, 0x00, 0x00 /*@r58*/ });
bytes_t const kInformationTwo = bytes (
    { 0x01, 0x0E, 0x1A /*@r31*/, 0xAD /*@r04*/, 0xD2 /*@r04*/, 0xAD /*@r04*/,
      0x44 /*@r04*/, 0x15 /*@r04*/, 0x2E /*@r15*/, 0x09 /*@r28*/,
      0x09 /*@r06*/, 0x0A, 0xE2, 0x10, 0x00, 0x00 /*@r58*/ });

template <typename P, typename Row>
layout::decode_result_t<typename layout::reply_value_of<Row>::type>
reply_of (bytes_t const &message)
{
  return layout::decode_reply<P, Row> (dev::view_of (message));
}

template <typename P, typename Row>
layout::decode_result_t<typename layout::request_of<Row>::type>
request_of (bytes_t const &message)
{
  return layout::decode_request<P, Row> (dev::view_of (message));
}

// A registry key of a mnemonic from its two letters.
registry::registry_key_t
mnemonic_key (char first, char second)
{
  registry::registry_key_t key
      = { registry::registry_key_kind::mnemonic, 0, 0, { 0, 0, 0, 0 } };
  key.mnemonic[0] = first;
  key.mnemonic[1] = second;
  return key;
}

// The device side of the comparison: the replies to the request of every
// index (high byte 0) in the order of the device; each reply carries the
// two letters the device gave the parameter at that index.
struct listing_device
{
  std::vector<bytes_t> replies;

  bool
  operator() (std::size_t position, registry::device_entry_t &out) const
  {
    if (position >= replies.size ())
      return false;
    out = registry::device_entry_t ();
    bytes_t const &reply = replies[position];
    // Requested index, then the two letters (the value is not compared).
    out.key = mnemonic_key (static_cast<char> (reply[2]),
                            static_cast<char> (reply[3]));
    return true;
  }
};

bytes_t
listing_reply (std::uint8_t number, char const *letters)
{
  return join (bytes ({ number, 0x00 }),
               join (text_bytes (letters), bytes ({ 0, 0, 0, 0 })));
}
} // namespace

// ---------------------------------------------------------------------------
// b2
// ---------------------------------------------------------------------------

TEST (LumexProtocolDeviceBTest,
      GivenTheReferenceInformation_WhenDecoded_ThenModelVersionDateAndSerial)
{
  index::protocol_index_t const &b2 = dev::index_of<device_b2> ();
  for (bytes_t const *report : { &kInformationOne, &kInformationTwo })
    {
      // The transport strips the report number and the length.
      ASSERT_EQ ((*report)[1], report->size () - 2u);
      bytes_t const frame = tail_of (*report, 2);
      EXPECT_TRUE (dev::finds<device_b2::query_ad_t> (b2, frame, 2));
      EXPECT_TRUE ((dev::reply_round_trip<device_b2, device_b2::query_ad_t> (
          tail_of (frame, 2))));
    }
  layout::decode_result_t<
      layout::reply_value_of<device_b2::query_ad_t>::type> const one
      = reply_of<device_b2, device_b2::query_ad_t> (
          tail_of (kInformationOne, 4));
  ASSERT_TRUE (one.has_value ());
  EXPECT_EQ (field::get<0> (*one), 0xD2u /*@r04*/);
  EXPECT_EQ (field::get<2> (*one), 0x90u /*@r04*/);
  EXPECT_EQ (field::get<4> (*one), 0x9Bu /*@r15*/);
  EXPECT_EQ (field::get<5> (*one), 0x07u /*@r28*/);
  EXPECT_EQ (field::get<6> (*one), 0x06u /*@r06*/);
  EXPECT_EQ (field::get<7> (*one), 0x02u);
  EXPECT_EQ (field::get<8> (*one), 4860u /*@r11*/);
  layout::decode_result_t<
      layout::reply_value_of<device_b2::query_ad_t>::type> const two
      = reply_of<device_b2, device_b2::query_ad_t> (
          tail_of (kInformationTwo, 4));
  ASSERT_TRUE (two.has_value ());
  EXPECT_EQ (field::get<0> (*two), field::get<0> (*one));
  EXPECT_EQ (field::get<2> (*two), 0x44u /*@r04*/);
  EXPECT_EQ (field::get<8> (*two), 4322u /*@r11*/);
}

TEST (LumexProtocolDeviceBTest,
      GivenTheWorkMode_WhenRoundTripped_ThenThePositionsArePackedInTheirWords)
{
  bytes_t const frame
      = bytes ({ kAddress, 0x65 /*@r04*/, 0x3F /*@r04*/, 0x00, 0x0A, 0x00,
                 0x32, 0x00, 0x00, 0x00, 0x64, 0x00, 0x03, 0x01, 0x07, 0x00 });
  EXPECT_TRUE (
      dev::finds<device_b2::set_65_t> (dev::index_of<device_b2> (), frame, 2));
  layout::decode_result_t<layout::request_of<device_b2::set_65_t>::type> const
      mode
      = request_of<device_b2, device_b2::set_65_t> (tail_of (frame, 2));
  ASSERT_TRUE (mode.has_value ());
  EXPECT_TRUE (field::get<0> (*mode) == b::b2_mode::inject_by_pressure);
  EXPECT_EQ (field::get<1> (*mode), 10);
  EXPECT_DOUBLE_EQ (field::get<2> (*mode).value (), 5.0);
  EXPECT_DOUBLE_EQ (field::get<4> (*mode).value (), 0.1);
  // The current limit is signed: -0.1 mA.
  bytes_t below = tail_of (frame, 2);
  below[8] = 0x9C;
  below[9] = 0xFF;
  layout::decode_result_t<layout::request_of<device_b2::set_65_t>::type> const
      negative
      = request_of<device_b2, device_b2::set_65_t> (below);
  ASSERT_TRUE (negative.has_value ());
  EXPECT_DOUBLE_EQ (field::get<4> (*negative).value (), -0.1);
  EXPECT_EQ (field::get<0> (field::get<5> (*mode).parts), 3u);
  EXPECT_TRUE (field::get<1> (field::get<5> (*mode).parts) == b::lift::upper);
  EXPECT_EQ (field::get<5> (*mode).raw, 0x0103u);
  EXPECT_TRUE (field::get<1> (field::get<6> (*mode).parts) == b::lift::lower);
  EXPECT_TRUE ((dev::request_round_trip<device_b2, device_b2::set_65_t> (
      tail_of (frame, 2))));
  // The device refuses and reports the mode it is in.
  layout::decode_result_t<
      layout::reply_value_of<device_b2::set_65_t>::type> const current
      = reply_of<device_b2, device_b2::set_65_t> (
          bytes ({ 0x15 /*@r04*/, 0x00 }));
  ASSERT_TRUE (current.has_value ());
  EXPECT_TRUE (*current == b::b2_mode::idle);
  EXPECT_FALSE (
      (reply_of<device_b2, device_b2::set_65_t> (bytes ({ 0x7E, 0x00 })))
          .has_value ());
}

TEST (LumexProtocolDeviceBTest,
      GivenAWorkParameter_WhenWritten_ThenTheRegistryChecksLimitsAndAccess)
{
  bytes_t out;
  ASSERT_TRUE ((layout::encode_request<device_b2, device_b2::param_set_3f_t> (
                    out, b2_work::frequency, 20.0))
                   .has_value ());
  EXPECT_EQ (out, bytes ({ 0xD1 /*@r04*/, 0x65 /*@r04*/, 0x14, 0x00 }));
  bytes_t frame = bytes ({ kAddress, 0x3F /*@r04*/ });
  frame.insert (frame.end (), out.begin (), out.end ());
  EXPECT_TRUE (dev::finds<device_b2::param_set_3f_t> (
      dev::index_of<device_b2> (), frame, 2));
  bytes_t refused;
  layout::encode_result_t const high
      = layout::encode_request<device_b2, device_b2::param_set_3f_t> (
          refused, b2_work::frequency, 200.0);
  ASSERT_FALSE (high.has_value ());
  EXPECT_TRUE (high.error () == error::protocol_errc::out_of_limits);
  bytes_t read_only;
  ASSERT_TRUE ((layout::encode_request<device_b2, device_b2::param_set_3f_t> (
                    read_only, b2_work::reset_density, 1))
                   .has_value ());
  EXPECT_EQ (read_only, bytes ({ 0xD1 /*@r04*/, 0xAD /*@r04*/, 0x01, 0x00 }));
  EXPECT_TRUE (refused.empty ());

  // The echo carries the value really set.
  layout::decode_result_t<
      layout::reply_value_of<device_b2::param_set_3f_t>::type> const echo
      = reply_of<device_b2, device_b2::param_set_3f_t> (out);
  ASSERT_TRUE (echo.has_value ());
  EXPECT_TRUE (field::get<0> (*echo) == b2_work::frequency);
  EXPECT_DOUBLE_EQ (field::get<1> (*echo).engineering, 20.0);
  EXPECT_TRUE (
      (dev::reply_round_trip<device_b2, device_b2::param_get_d1_t> (out)));
  EXPECT_TRUE ((dev::request_round_trip<device_b2, device_b2::param_get_d1_t> (
      bytes ({ 0xF3 /*@r04*/, 0xAD /*@r04*/ }))));
  // A unit and parameter the registry does not have.
  EXPECT_FALSE ((request_of<device_b2, device_b2::param_get_d1_t> (
                     bytes ({ 0xF3 /*@r04*/, 0x65 /*@r04*/ })))
                    .has_value ());
}

TEST (LumexProtocolDeviceBTest,
      GivenTheMeasurement_WhenDescribed_ThenTheModelLayerDoublesTheScale)
{
  bytes_t const frame
      = bytes ({ kAddress, 0x27 /*@r04*/, 0xE8, 0x03, 0x00, 0x00, 0x39, 0x30,
                 0x00, 0x00, 0x10, 0x27, 0x00, 0x00, 0x20, 0x4E, 0x00, 0x00 });
  EXPECT_EQ (dev::index_of<device_b2> ().describe (dev::view_of (frame),
                                                   index::frame_direction::rx),
             "RX device_b2.stream_27 (Measurement result) time=1000 ms "
             "absorbance=0.012345 AU photo=10000 reference=20000");
  EXPECT_EQ (dev::index_of<device_b2_m2> ().describe (
                 dev::view_of (frame), index::frame_direction::rx),
             "RX device_b2_m2.stream_27 (Measurement result) time=1000 ms "
             "absorbance=0.02469 AU photo=10000 reference=20000");
  bytes_t second = frame;
  second[1] = 0xFA /*@r04*/;
  EXPECT_TRUE (dev::finds<device_b2::stream_fa_t> (dev::index_of<device_b2> (),
                                                   second, 2));
  EXPECT_TRUE ((dev::reply_round_trip<device_b2, device_b2::stream_fa_t> (
      tail_of (second, 2))));
  EXPECT_TRUE (
      (dev::reply_round_trip<device_b2_m2, device_b2_m2::stream_27_t> (
          tail_of (frame, 2))));
  // The model layer row is there only for its model.
  index::session_context_t session;
  session.set_model ("b2_m1");
  EXPECT_FALSE (session.available (
      *dev::index_of<device_b2_m2> ().find_name ("stream_27")->entry));
  session.set_model ("b2_m2");
  EXPECT_TRUE (session.available (
      *dev::index_of<device_b2_m2> ().find_name ("stream_27")->entry));
}

TEST (LumexProtocolDeviceBTest,
      GivenTheStateOfAUnit_WhenDecoded_ThenTheUnitChoosesTheLayout)
{
  bytes_t const old_main
      = bytes ({ 0xAD /*@r04*/, 0x2C, 0x01, 0x00, 0x94, 0x11, 0x00, 0x00, 0x3C,
                 0x00, 0x00, 0x00, 0x98, 0x3A, 0x32, 0x00, 0x2C, 0x01 });
  bytes_t const new_main
      = join (old_main, bytes ({ 0x03, 0x01, 0x00, 0x07, 0x00, 0x01 }));
  bytes_t const compressor = bytes (
      { 0xFA /*@r04*/, 0x01, 0xF4, 0x01, 0xE8, 0x03, 0x00, 0x00, 0x40, 0x00,
        0x02,          0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00, 0x00 });
  EXPECT_EQ (new_main.size (), 24u);
  for (bytes_t const *message : { &old_main, &new_main, &compressor })
    EXPECT_TRUE (
        (dev::reply_round_trip<device_b2, device_b2::report_d9_t> (*message)));
  index::protocol_index_t const &b2 = dev::index_of<device_b2> ();
  EXPECT_EQ (b2.describe (dev::view_of (join (
                              bytes ({ kAddress, 0xD9 /*@r04*/ }), old_main)),
                          index::frame_direction::rx),
             "RX device_b2.report_d9 (State of a unit) unit=case "
                 + dev::dec (0xAD /*@r04*/)
                 + ": {mode=44 cassette=1 cover=0 time=4500 ms duration=60 s "
                   "voltage=15000 V pressure=5 kPa max_current=0.3 mA "
                   "targets=absent}");
  layout::decode_result_t<
      layout::reply_value_of<device_b2::report_d9_t>::type> const pump
      = reply_of<device_b2, device_b2::report_d9_t> (compressor);
  ASSERT_TRUE (pump.has_value ());
  EXPECT_EQ (field::get<0> (*pump).discriminant, 0xFAu /*@r04*/);
  // A unit without a layout.
  EXPECT_FALSE ((reply_of<device_b2, device_b2::report_d9_t> (
                     bytes ({ 0xF3 /*@r04*/, 0x01 })))
                    .has_value ());
}

TEST (LumexProtocolDeviceBTest,
      GivenAnErrorAndAnEvent_WhenDecoded_ThenTheFamilyLayoutsServeTheLayer)
{
  bytes_t const error_frame
      = bytes ({ kAddress, 0x44 /*@r04*/, 0xA7, 0x43 /*@r47*/, 0x00, 0x01,
                 0x77, 0x00, 0x00, 0x00 });
  EXPECT_TRUE (dev::finds<device_b2::error_44_t> (dev::index_of<device_b2> (),
                                                  error_frame, 2));
  layout::decode_result_t<
      layout::reply_value_of<device_b2::error_44_t>::type> const report
      = reply_of<device_b2, device_b2::error_44_t> (tail_of (error_frame, 2));
  ASSERT_TRUE (report.has_value ());
  EXPECT_EQ (field::get<0> (*report), 0x43A7u /*@r09*/);
  EXPECT_TRUE (field::get<1> (*report)
               == lumex_tests::device_a::severity::warning);
  EXPECT_EQ (field::get<3> (*report), 0x77);
  EXPECT_TRUE ((dev::reply_round_trip<device_b2, device_b2::event_d4_t> (
      bytes ({ 0x0B, 0x02 }))));
  EXPECT_TRUE ((dev::reply_round_trip<device_b2, device_b2::report_41_t> (
      bytes ({ 0x05, 1, 2, 3, 4, 5, 6, 7, 8 }))));
}

TEST (LumexProtocolDeviceBTest,
      GivenAServiceParameter_WhenReadAndWritten_ThenTheTwoLettersAreTheKey)
{
  bytes_t out;
  ASSERT_TRUE ((layout::encode_request<device_b2, device_b2::param_get_83_t> (
                    out, b2_service::p_01))
                   .has_value ());
  EXPECT_EQ (out, text_bytes ("YM" /*@r11*/));
  bytes_t const reply
      = join (join (text_bytes ("YM" /*@r11*/), text_bytes ("YM" /*@r11*/)),
              bytes ({ 0xFC, 0x12, 0x00, 0x00 /*@r58*/ }));
  layout::decode_result_t<
      layout::reply_value_of<device_b2::param_get_83_t>::type> const first
      = reply_of<device_b2, device_b2::param_get_83_t> (reply);
  ASSERT_TRUE (first.has_value ());
  EXPECT_TRUE (field::get<1> (*first) == b2_service::p_01);
  EXPECT_EQ (field::get<2> (*first).raw, 4860 /*@r11*/);
  EXPECT_TRUE (
      (dev::reply_round_trip<device_b2, device_b2::param_get_83_t> (reply)));

  bytes_t set;
  ASSERT_TRUE ((layout::encode_request<device_b2, device_b2::param_set_cc_t> (
                    set, b2_service::p_04, 12.5))
                   .has_value ());
  EXPECT_EQ (set, join (text_bytes ("KN" /*@r11*/),
                        bytes ({ 0x00, 0x00, 0x7D, 0x00, 0x00, 0x00 })));
  EXPECT_TRUE (
      (dev::request_round_trip<device_b2, device_b2::param_set_cc_t> (set)));
  bytes_t refused;
  layout::encode_result_t const read_only
      = layout::encode_request<device_b2, device_b2::param_set_cc_t> (
          refused, b2_service::p_01, 1);
  ASSERT_FALSE (read_only.has_value ());
  EXPECT_TRUE (read_only.error () == error::protocol_errc::access_denied);
}

TEST (LumexProtocolDeviceBTest,
      GivenTheListTheDeviceReports_WhenVerified_ThenTheFloatingEntryIsMissing)
{
  char const *const letters[]
      = { "YM" /*@r11*/, "YE" /*@r11*/, "LE" /*@r11*/, "KN" /*@r11*/,
          "EF" /*@r11*/, "TQ" /*@r11*/, "QT" /*@r11*/, "SR" /*@r11*/,
          "QR" /*@r11*/, "OQ" /*@r11*/, "WQ" /*@r11*/, "XQ" /*@r11*/,
          "4O" /*@r11*/, "1O" /*@r11*/ };
  listing_device device;
  std::uint8_t number = 1;
  for (char const *pair : letters)
    device.replies.push_back (listing_reply (number++, pair));
  // Every reply of the list decodes: the index is the requested key.
  EXPECT_TRUE ((dev::reply_round_trip<device_b2, device_b2::param_get_83_t> (
      device.replies[0])));
  registry::verify_report_t const service
      = registry::verify_registry<b2_service> (device);
  EXPECT_EQ (service.device_entries, 14u);
  EXPECT_EQ (service.compared, 13u);
  ASSERT_EQ (service.mismatches.size (), 1u);
  EXPECT_TRUE (service.mismatches[0].kind
               == registry::mismatch_kind::missing_in_table);
  registry::verify_report_t const floating
      = registry::verify_registry<b::b2_floating> (device);
  EXPECT_EQ (floating.compared, 1u);
  EXPECT_EQ (floating.mismatches.size (), 13u);
  // The service registry and the work registry index without a clash.
  index::index_result_t const built
      = index::build_index<device_b2, b2_work, b2_service, b::b2_floating> ();
  ASSERT_TRUE (built.has_value ());
  EXPECT_EQ (built->registries ().size (), 3u);
  EXPECT_TRUE (built->find_registry ("b2_service") != nullptr);
}

TEST (LumexProtocolDeviceBTest,
      GivenTheNodeQueryAndTheLink_WhenEncoded_ThenTheBytesAreTheReference)
{
  bytes_t node;
  ASSERT_TRUE ((layout::encode_request<device_b2, device_b2::query_b8_t> (
                    node, 0x07, 'K' /*@r11*/))
                   .has_value ());
  EXPECT_EQ (node, bytes ({ 0x07, 'K' /*@r11*/ }));
  EXPECT_TRUE (dev::finds<device_b2::query_b8_t> (
      dev::index_of<device_b2> (),
      join (bytes ({ kAddress, 0xB8 /*@r04*/ }), node), 2));

  // The host sends its time once a second; the device its error counters.
  bytes_t const host = bytes ({ 0x00, 0x00, 0x00, 0xF1, 0x53, 0x65 });
  bytes_t const device
      = bytes ({ 0x00, kAddress, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00 });
  index::protocol_index_t const &system = dev::index_of<b::b2_system> ();
  EXPECT_TRUE (dev::finds<b::b2_system::keepalive_00_t> (system, host, 1));
  layout::decode_result_t<
      layout::request_of<b::b2_system::keepalive_00_t>::type> const time
      = request_of<b::b2_system, b::b2_system::keepalive_00_t> (
          tail_of (host, 1));
  ASSERT_TRUE (time.has_value ());
  EXPECT_EQ (field::get<1> (*time), 1700000000u);
  layout::decode_result_t<
      layout::reply_value_of<b::b2_system::keepalive_00_t>::type> const link
      = reply_of<b::b2_system, b::b2_system::keepalive_00_t> (
          tail_of (device, 1));
  ASSERT_TRUE (link.has_value ());
  EXPECT_EQ (field::get<0> (*link), kAddress);
  EXPECT_EQ (field::get<1> (*link), 2u);
  EXPECT_EQ (field::get<3> (*link), 1u);
}

TEST (LumexProtocolDeviceBTest,
      GivenTheAlignedPolicy_WhenTheTablesAreChecked_ThenOnlyTheFamilyFails)
{
  static_assert (layout::is_aligned<device_b2>::value,
                 "every field of b2 sits on its own size");
  static_assert (!layout::is_aligned<b::aligned_family>::value,
                 "a family row puts a 4-byte value at offset 2");
  static_assert (index::keys_unique<device_b2>::value, "unique keys of b2");
  EXPECT_EQ (dev::index_of<device_b2> ().rows ().size (), 13u);
  EXPECT_STREQ (dev::index_of<device_b2> ().base_name (), "family_a");
}

// ---------------------------------------------------------------------------
// b1
// ---------------------------------------------------------------------------

TEST (LumexProtocolDeviceBTest,
      GivenTheRequestsOfB1_WhenEncoded_ThenTheProcessHeaderPrecedesTheFields)
{
  using lumex_tests::device_a::old_header;
  using lumex_tests::device_a::request_frame;
  index::protocol_index_t const &tx = dev::index_of<device_b1> ();

  bytes_t const ask = request_frame<device_b1, device_b1::query_b8_t> (
      old_header (kAddress, b::task::manager, 0x00, 0xB8 /*@r04*/));
  EXPECT_EQ (ask, bytes ({ kAddress, 0x3F /*@r04*/, 0x00, 0xB8 /*@r04*/ }));
  EXPECT_TRUE (dev::finds<device_b1::query_b8_t> (tx, ask, 4));

  bytes_t const set = request_frame<device_b1, device_b1::param_set_64_t> (
      old_header (kAddress, b::task::manager, 0x00, 0x64 /*@r04*/), 1000,
      b1_params_unique::inject_time);
  EXPECT_EQ (set, bytes ({ kAddress, 0x3F /*@r04*/, 0x00, 0x64 /*@r04*/, 0xE8,
                           0x03, 0x00, 0x00, 0x3F /*@r04*/ }));
  EXPECT_TRUE (dev::finds<device_b1::param_set_64_t> (tx, set, 4));

  bytes_t const get = request_frame<device_b1, device_b1::param_get_83_t> (
      old_header (kAddress, b::task::manager, 0x00, 0x83 /*@r04*/),
      b1_params_unique::serial);
  EXPECT_EQ (get, bytes ({ kAddress, 0x3F /*@r04*/, 0x00, 0x83 /*@r04*/,
                           0xE8 /*@r04*/ }));

  bytes_t const stream = request_frame<device_b1, device_b1::stream_e1_t> (
      old_header (kAddress, b::task::manager, 0x00, 0xE1 /*@r04*/),
      0x9A /*@r04*/, 1);
  EXPECT_EQ (tail_of (stream, 4), bytes ({ 0x9A /*@r04*/, 0x01 }));

  bytes_t const can = bytes ({ kAddress, 0x3F /*@r04*/, 0x00, 0x94 /*@r04*/,
                               0x05, 1, 2, 3, 4, 5, 6, 7, 8 });
  EXPECT_TRUE (dev::finds<device_b1::send_94_t> (tx, can, 4));
  EXPECT_TRUE ((dev::request_round_trip<device_b1, device_b1::send_94_t> (
      tail_of (can, 4))));
  // Other tasks of the firmware are addressed by the same header.
  EXPECT_TRUE (dev::finds<device_b1::query_b8_t> (
      tx, bytes ({ kAddress, b::task::lamp, 0x01, 0xB8 /*@r04*/ }), 4));
}

TEST (LumexProtocolDeviceBTest,
      GivenTheManagerState_WhenRoundTripped_ThenTheLastByteChoosesTheMeaning)
{
  bytes_t const pressure
      = bytes ({ 0x32, 0x00, 0x0A, 0x00, 0x00, 0x00, 0xDD /*@r04*/ });
  bytes_t const voltage
      = bytes ({ 0x98, 0x3A, 0x0A, 0x00, 0x00, 0x00, 0x2C /*@r04*/ });
  layout::decode_result_t<layout::request_of<device_b1::set_ac_t>::type> const
      by_pressure
      = request_of<device_b1, device_b1::set_ac_t> (pressure);
  ASSERT_TRUE (by_pressure.has_value ());
  EXPECT_EQ (field::get<0> (field::get<0> (*by_pressure)).discriminant,
             b::state::inject_by_pressure);
  EXPECT_TRUE (
      (dev::request_round_trip<device_b1, device_b1::set_ac_t> (pressure)));
  EXPECT_TRUE (
      (dev::request_round_trip<device_b1, device_b1::set_ac_t> (voltage)));
  EXPECT_EQ (dev::index_of<device_b1> ().describe (
                 dev::view_of (join (
                     bytes ({ kAddress, 0x3F /*@r04*/, 0x00, 0xAC /*@r04*/ }),
                     pressure)),
                 index::frame_direction::tx),
             "TX device_b1.set_ac (Manager state) state={state=case "
                 + dev::dec (0xDD /*@r04*/)
                 + ": {pressure=5 kPa time=10 s unused=0}}");
  // The device reports the state without the process header.
  bytes_t const report = join (bytes ({ kAddress, 0x9A /*@r04*/ }), voltage);
  EXPECT_TRUE (dev::finds<device_b1::report_9a_t> (
      dev::index_of<device_b1_rx> (), report, 2));
  EXPECT_TRUE (
      (dev::reply_round_trip<device_b1_rx, device_b1_rx::report_9a_t> (
          voltage)));
  // A state without a meaning.
  EXPECT_FALSE ((request_of<device_b1, device_b1::set_ac_t> (
                     bytes ({ 0, 0, 0, 0, 0, 0, 0xF3 /*@r04*/ })))
                    .has_value ());
}

TEST (LumexProtocolDeviceBTest,
      GivenTheRepliesOfB1_WhenDecoded_ThenTheDateIsAnOptionalTail)
{
  index::protocol_index_t const &rx = dev::index_of<device_b1_rx> ();
  bytes_t const short_info
      = bytes ({ kAddress, 0x15 /*@r04*/, 0x99 /*@r04*/, 0x01, 0x05, 0x02,
                 0x2A, 0x00, 0x00, 0x00 });
  bytes_t const long_info = join (short_info, bytes ({ 0x13, 0x04, 0x16 }));
  EXPECT_TRUE (dev::finds<device_b1::report_15_t> (rx, short_info, 2));
  layout::decode_result_t<
      layout::reply_value_of<device_b1::report_15_t>::type> const older
      = reply_of<device_b1_rx, device_b1_rx::report_15_t> (
          tail_of (short_info, 2));
  ASSERT_TRUE (older.has_value ());
  EXPECT_EQ (field::get<0> (*older), 0x99u /*@r04*/);
  EXPECT_EQ (field::get<4> (*older), 42);
  EXPECT_FALSE (field::get<5> (*older).present);
  layout::decode_result_t<
      layout::reply_value_of<device_b1::report_15_t>::type> const newer
      = reply_of<device_b1_rx, device_b1_rx::report_15_t> (
          tail_of (long_info, 2));
  ASSERT_TRUE (newer.has_value ());
  EXPECT_TRUE (field::get<5> (*newer).present);
  EXPECT_TRUE (
      (dev::reply_round_trip<device_b1_rx, device_b1_rx::report_15_t> (
          tail_of (long_info, 2))));

  bytes_t const value = bytes (
      { kAddress, 0xCC /*@r04*/, 0xE8, 0x03, 0x00, 0x00, 0x3F /*@r04*/ });
  EXPECT_TRUE (dev::finds<device_b1::report_cc_t> (rx, value, 2));
  layout::decode_result_t<
      layout::reply_value_of<device_b1::report_cc_t>::type> const parameter
      = reply_of<device_b1_rx, device_b1_rx::report_cc_t> (tail_of (value, 2));
  ASSERT_TRUE (parameter.has_value ());
  EXPECT_EQ (field::get<0> (*parameter), 1000);
  EXPECT_TRUE (field::get<1> (*parameter) == b1_params_unique::inject_time);
  EXPECT_TRUE (
      (dev::reply_round_trip<device_b1_rx, device_b1_rx::report_84_t> (
          bytes ({ 0x05, 1, 2, 3, 4, 5, 6, 7, 8 }))));
}

TEST (LumexProtocolDeviceBTest,
      GivenTheParametersOfB1_WhenIndexed_ThenTheSharedNumberIsRefused)
{
  std::size_t first = 0;
  std::size_t second = 0;
  ASSERT_TRUE (
      registry::find_duplicate_key (b1_params::entries (), first, second));
  EXPECT_STREQ (b1_params::entries ().data ()[first].identifier,
                "rinse_pressure");
  EXPECT_STREQ (b1_params::entries ().data ()[second].identifier,
                "starting_voltage");
  index::index_result_t const refused
      = index::build_index<device_b1, b1_params> ();
  ASSERT_FALSE (refused.has_value ());
  EXPECT_TRUE (refused.error ().code == error::protocol_errc::duplicate_key);
  EXPECT_NE (refused.error ().message.find ("rinse_pressure"),
             std::string::npos);
  EXPECT_NE (refused.error ().message.find ("starting_voltage"),
             std::string::npos);
  EXPECT_TRUE (
      (index::build_index<device_b1, b1_params_unique> ()).has_value ());
}

TEST (LumexProtocolDeviceBTest,
      GivenTheModelsAndTheFirmware_WhenAsked_ThenOnlyTheirParametersAreThere)
{
  registry::entries_t const entries = b1_params::entries ();
  registry::registry_entry_t const &zero_shift
      = entries.data ()[static_cast<std::size_t> (b1_params::zero_shift)];
  registry::registry_entry_t const &level
      = entries.data ()[static_cast<std::size_t> (
          b1_params::start_to_work_level)];
  registry::registry_entry_t const &passed
      = entries
            .data ()[static_cast<std::size_t> (b1_params::calibration_passed)];
  EXPECT_TRUE (registry::entry_available (zero_shift, 0, "b1_m1"));
  EXPECT_FALSE (registry::entry_available (zero_shift, 0, "b1_m3"));
  EXPECT_FALSE (registry::entry_available (level, 0, "b1_m1"));
  EXPECT_TRUE (registry::entry_available (level, 0, "b1_m4"));
  EXPECT_FALSE (registry::entry_available (passed, 10, "b1_m1"));
  EXPECT_TRUE (registry::entry_available (passed, 20, "b1_m1"));
  index::session_context_t session;
  session.set_model ("b1_m2");
  EXPECT_FALSE (session.available (zero_shift));
  EXPECT_TRUE (session.available (
      entries.data ()[static_cast<std::size_t> (b1_params::inlet_target)]));
}
