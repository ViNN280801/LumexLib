// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Device A (LumexProtocolDeviceA.hpp): every request and reply of the
// family table on reference vectors, the three message formats (the index
// finds the fields after their own header), the same message code with
// another meaning in the stream layer, the old system message, describe and
// the export.

#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/devices/LumexProtocolDevicesSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_a/LumexProtocolDeviceA.hpp"

namespace
{
namespace dev = lumex_tests::devices;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace index = lumex::core::protocol::index;
namespace dsl = lumex::core::protocol::dsl;
using dev::bytes;
using dev::bytes_t;
using lumex_tests::device_a::a_old;
using lumex_tests::device_a::a_stream;
using lumex_tests::device_a::a_system;
using lumex_tests::device_a::family_a;

// The address of the device under test (any address of the family).
std::uint8_t const kAddress = 0x5A;

std::string
describe_tx (bytes_t const &frame)
{
  return dev::index_of<family_a> ().describe (dev::view_of (frame),
                                              index::frame_direction::tx);
}

std::string
describe_rx (bytes_t const &frame)
{
  return dev::index_of<family_a> ().describe (dev::view_of (frame),
                                              index::frame_direction::rx);
}
} // namespace

TEST (LumexProtocolDeviceATest,
      GivenTheFamilyTable_WhenIndexed_ThenEveryKeyIsUniqueAndEveryRowIsThere)
{
  static_assert (index::keys_unique<family_a>::value,
                 "the family keys are unique");
  static_assert (dsl::command_count<family_a>::value == 11,
                 "eleven family rows");
  index::index_result_t const built = index::build_index<family_a> ();
  ASSERT_TRUE (built.has_value ());
  EXPECT_EQ (built->rows ().size (), 11u);
  index::row_t const *const information = built->find_integer (0xAD /*@r04*/);
  ASSERT_TRUE (information != nullptr);
  EXPECT_EQ (information->entry->identifier, std::string ("query_ad"));
  EXPECT_TRUE (built->find_name ("report_41") != nullptr);
  EXPECT_TRUE (index::build_index<a_stream> ().has_value ());
  EXPECT_TRUE (index::build_index<a_old> ().has_value ());
  EXPECT_TRUE (index::build_index<a_system> ().has_value ());
}

TEST (LumexProtocolDeviceATest,
      GivenEveryFamilyRequest_WhenEncoded_ThenTheBytesAreTheReference)
{
  using set_parameter = family_a::param_set_3f_t;
  using get_parameter = family_a::param_get_d1_t;
  using set_mode = family_a::set_65_t;
  using get_service = family_a::param_get_83_t;
  using set_service = family_a::param_set_cc_t;
  using lumex_tests::device_a::new_header;
  using lumex_tests::device_a::request_frame;

  bytes_t const set_parameter_frame
      = bytes ({ kAddress, 0x3F /*@r04*/, 0x02, 0x01, 0xD4, 0xFE });
  EXPECT_EQ ((request_frame<family_a, set_parameter> (
                 new_header (kAddress, 0x3F /*@r04*/), 2, 1, -300)),
             set_parameter_frame);
  bytes_t const get_parameter_frame
      = bytes ({ kAddress, 0xD1 /*@r04*/, 0x02, 0x01 });
  EXPECT_EQ ((request_frame<family_a, get_parameter> (
                 new_header (kAddress, 0xD1 /*@r04*/), 2, 1)),
             get_parameter_frame);
  bytes_t const set_mode_frame = bytes ({ kAddress, 0x65 /*@r04*/, 0x03 });
  EXPECT_EQ ((request_frame<family_a, set_mode> (
                 new_header (kAddress, 0x65 /*@r04*/), 3)),
             set_mode_frame);
  bytes_t const get_service_frame
      = bytes ({ kAddress, 0x83 /*@r04*/, 0x34, 0x12 });
  EXPECT_EQ ((request_frame<family_a, get_service> (
                 new_header (kAddress, 0x83 /*@r04*/), 0x1234)),
             get_service_frame);
  bytes_t const set_service_frame = bytes (
      { kAddress, 0xCC /*@r04*/, 0x34, 0x12, 0xF0, 0xFF, 0xFF, 0xFF });
  EXPECT_EQ ((request_frame<family_a, set_service> (
                 new_header (kAddress, 0xCC /*@r04*/), 0x1234, -16)),
             set_service_frame);

  index::protocol_index_t const &family = dev::index_of<family_a> ();
  EXPECT_TRUE (dev::finds<set_parameter> (family, set_parameter_frame, 2));
  EXPECT_TRUE (dev::finds<get_parameter> (family, get_parameter_frame, 2));
  EXPECT_TRUE (dev::finds<set_mode> (family, set_mode_frame, 2));
  EXPECT_TRUE (dev::finds<get_service> (family, get_service_frame, 2));
  EXPECT_TRUE (dev::finds<set_service> (family, set_service_frame, 2));
  EXPECT_TRUE ((dev::request_round_trip<family_a, set_parameter> (
      dev::tail_of (set_parameter_frame, 2))));
  EXPECT_TRUE ((dev::request_round_trip<family_a, set_service> (
      dev::tail_of (set_service_frame, 2))));
  EXPECT_TRUE (
      (dev::request_round_trip<family_a, family_a::query_ad_t> (bytes_t ())));
}

TEST (LumexProtocolDeviceATest,
      GivenEveryFamilyReply_WhenDecoded_ThenTheValuesAreTheReference)
{
  index::protocol_index_t const &family = dev::index_of<family_a> ();

  bytes_t const info
      = bytes ({ kAddress, 0xAD /*@r04*/, 0x21, 0x02, 0x05, 0x01, 0x19, 0x0B,
                 0x07, 0x10, 0x39, 0x30, 0x00, 0x00 });
  EXPECT_TRUE (dev::finds<family_a::query_ad_t> (family, info, 2));
  layout::decode_result_t<
      layout::reply_value_of<family_a::query_ad_t>::type> const decoded_info
      = layout::decode_reply<family_a, family_a::query_ad_t> (
          dev::view_of (dev::tail_of (info, 2)));
  ASSERT_TRUE (decoded_info.has_value ());
  EXPECT_EQ (field::get<0> (*decoded_info), 0x21u);
  EXPECT_EQ (field::get<7> (*decoded_info), 0x10u);
  EXPECT_EQ (field::get<8> (*decoded_info), 12345u);

  bytes_t const parameter
      = bytes ({ kAddress, 0x3F /*@r04*/, 0x02, 0x01, 0xD4, 0xFE });
  layout::decode_result_t<
      layout::reply_value_of<family_a::param_set_3f_t>::type> const echo
      = layout::decode_reply<family_a, family_a::param_set_3f_t> (
          dev::view_of (dev::tail_of (parameter, 2)));
  ASSERT_TRUE (echo.has_value ());
  EXPECT_EQ (field::get<2> (*echo), -300);

  bytes_t const measurement
      = bytes ({ kAddress, 0x27 /*@r04*/, 0xDC, 0x05, 0x00, 0x00, 0xD4, 0x04,
                 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0xFF, 0xFF, 0xFF, 0x7F });
  EXPECT_TRUE (dev::finds<family_a::stream_27_t> (family, measurement, 2));
  layout::decode_result_t<
      layout::reply_value_of<family_a::stream_27_t>::type> const values
      = layout::decode_reply<family_a, family_a::stream_27_t> (
          dev::view_of (dev::tail_of (measurement, 2)));
  ASSERT_TRUE (values.has_value ());
  EXPECT_EQ (field::get<0> (*values), 1500u);
  EXPECT_EQ (field::get<1> (*values), 1236);
  EXPECT_EQ (field::get<2> (*values), 65536);
  EXPECT_EQ (field::get<3> (*values), 0x7FFFFFFF);
  // A channel below zero (the channels are signed).
  layout::decode_result_t<
      layout::reply_value_of<family_a::stream_27_t>::type> const negative
      = layout::decode_reply<family_a, family_a::stream_27_t> (dev::view_of (
          bytes ({ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0xFF,
                   0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00 })));
  ASSERT_TRUE (negative.has_value ());
  // Widened first: an unsigned channel would compare equal to -2 otherwise.
  EXPECT_EQ (static_cast<std::int64_t> (field::get<2> (*negative)), -2);

  bytes_t const error = bytes ({ kAddress, 0x44 /*@r04*/, 0x34, 0x12, 0x02,
                                 0x01, 0x07, 0x00, 0x00, 0x00 });
  layout::decode_result_t<
      layout::reply_value_of<family_a::error_44_t>::type> const report
      = layout::decode_reply<family_a, family_a::error_44_t> (
          dev::view_of (dev::tail_of (error, 2)));
  ASSERT_TRUE (report.has_value ());
  EXPECT_EQ (field::get<0> (*report), 0x1234u);
  EXPECT_TRUE (field::get<1> (*report)
               == lumex_tests::device_a::severity::fatal);
  EXPECT_EQ (field::get<3> (*report), 7);

  bytes_t const can = bytes ({ kAddress, 0x41 /*@r04*/, 0x08, 0x01, 0x02, 0x03,
                               0x04, 0x05, 0x06, 0x07, 0x08 });
  layout::decode_result_t<
      layout::reply_value_of<family_a::report_41_t>::type> const packet
      = layout::decode_reply<family_a, family_a::report_41_t> (
          dev::view_of (dev::tail_of (can, 2)));
  ASSERT_TRUE (packet.has_value ());
  EXPECT_EQ (field::get<0> (*packet), 8u);
  EXPECT_EQ (field::get<1> (*packet)[7], 8u);

  EXPECT_TRUE ((dev::reply_round_trip<family_a, family_a::query_ad_t> (
      dev::tail_of (info, 2))));
  EXPECT_TRUE ((
      dev::reply_round_trip<family_a, family_a::set_65_t> (bytes ({ 0x03 }))));
  EXPECT_TRUE ((dev::reply_round_trip<family_a, family_a::param_get_d1_t> (
      dev::tail_of (parameter, 2))));
  EXPECT_TRUE ((dev::reply_round_trip<family_a, family_a::stream_27_t> (
      dev::tail_of (measurement, 2))));
  EXPECT_TRUE ((dev::reply_round_trip<family_a, family_a::report_d9_t> (
      bytes ({ 0x02, 0x05 }))));
  EXPECT_TRUE ((dev::reply_round_trip<family_a, family_a::error_44_t> (
      dev::tail_of (error, 2))));
  EXPECT_TRUE ((dev::reply_round_trip<family_a, family_a::event_d4_t> (
      bytes ({ 0x02, 0x01 }))));
  EXPECT_TRUE ((dev::reply_round_trip<family_a, family_a::param_get_83_t> (
      bytes ({ 0x34, 0x12, 0x34, 0x12, 0x10, 0x00, 0x00, 0x00 }))));
  EXPECT_TRUE ((dev::reply_round_trip<family_a, family_a::param_set_cc_t> (
      bytes ({ 0x34, 0x12, 0x34, 0x12, 0xF0, 0xFF, 0xFF, 0xFF }))));
  EXPECT_TRUE ((dev::reply_round_trip<family_a, family_a::report_41_t> (
      dev::tail_of (can, 2))));
}

TEST (LumexProtocolDeviceATest,
      GivenTheSeverityOfAnError_WhenItIsNoKnownValue_ThenTheReplyIsRefused)
{
  layout::decode_result_t<
      layout::reply_value_of<family_a::error_44_t>::type> const report
      = layout::decode_reply<family_a, family_a::error_44_t> (dev::view_of (
          bytes ({ 0x34, 0x12, 0x03, 0x01, 0x07, 0x00, 0x00, 0x00 })));
  ASSERT_FALSE (report.has_value ());
}

TEST (
    LumexProtocolDeviceATest,
    GivenTheProcessHeader_WhenAFrameIsMatched_ThenTheFieldsStartAfterFourBytes)
{
  using lumex_tests::device_a::old_header;
  using lumex_tests::device_a::request_frame;
  // The display process of the firmware, attribute 0 (one process).
  bytes_t const frame = request_frame<a_old, a_old::stream_af_t> (
      old_header (kAddress, 0x72 /*@r04*/, 0x00, 0xAF /*@r04*/), 0xD9 /*@r04*/,
      1);
  EXPECT_EQ (frame, bytes ({ kAddress, 0x72 /*@r04*/, 0x00, 0xAF /*@r04*/,
                             0xD9 /*@r04*/, 0x01 }));
  EXPECT_TRUE (
      dev::finds<a_old::stream_af_t> (dev::index_of<a_old> (), frame, 4));
  EXPECT_TRUE ((dev::request_round_trip<a_old, a_old::stream_af_t> (
      dev::tail_of (frame, 4))));
  // The same bytes in the family format: the process is taken for the
  // message code, which the family does not have.
  EXPECT_TRUE (dev::index_of<family_a> ().match (dev::view_of (frame)).row
               == nullptr);
}

TEST (LumexProtocolDeviceATest,
      GivenTheStreamFormat_WhenTheSameCodeIsMatched_ThenEachLayerHasItsOwnRow)
{
  bytes_t const frame = bytes ({ kAddress, 0x27 /*@r04*/, 0x01 });
  EXPECT_TRUE (dev::finds<a_stream::stream_on_27_t> (
      dev::index_of<a_stream> (), frame, 2));
  EXPECT_TRUE (dev::finds<family_a::stream_27_t> (dev::index_of<family_a> (),
                                                  frame, 2));
  EXPECT_EQ (dev::index_of<a_stream> ().describe (dev::view_of (frame),
                                                  index::frame_direction::tx),
             "TX a_stream.stream_on_27 (Measurement result on or off) "
             "enable=1");
  EXPECT_TRUE ((dev::request_round_trip<a_stream, a_stream::stream_on_d9_t> (
      bytes ({ 0x00 }))));
}

TEST (LumexProtocolDeviceATest,
      GivenTheOldSystemMessage_WhenDecoded_ThenTheFormatErrorsComeTwice)
{
  bytes_t const frame
      = bytes ({ 0x00, 0x00, kAddress, 0x40, 0x00, 0x03, 0x03, 0x01 });
  index::protocol_index_t const &system = dev::index_of<a_system> ();
  EXPECT_TRUE (dev::finds<a_system::keepalive_00_t> (system, frame, 1));
  layout::decode_result_t<
      layout::reply_value_of<a_system::keepalive_00_t>::type> const message
      = layout::decode_reply<a_system, a_system::keepalive_00_t> (
          dev::view_of (dev::tail_of (frame, 1)));
  ASSERT_TRUE (message.has_value ());
  EXPECT_EQ (field::get<1> (*message), kAddress);
  EXPECT_EQ (field::get<2> (*message), 0x40u);
  EXPECT_EQ (field::get<3> (*message), field::get<4> (*message));
  EXPECT_EQ (field::get<5> (*message), 1u);
  EXPECT_TRUE ((dev::reply_round_trip<a_system, a_system::keepalive_00_t> (
      dev::tail_of (frame, 1))));
  // Five bytes after the address are too few.
  EXPECT_FALSE (
      (layout::decode_reply<a_system, a_system::keepalive_00_t> (
           dev::view_of (bytes ({ 0x00, kAddress, 0x40, 0x00, 0x03, 0x03 }))))
          .has_value ());
}

TEST (LumexProtocolDeviceATest,
      GivenFamilyFrames_WhenDescribed_ThenTheLogLinesNameRowsFieldsAndUnits)
{
  EXPECT_EQ (describe_tx (
                 bytes ({ kAddress, 0x3F /*@r04*/, 0x02, 0x01, 0xD4, 0xFE })),
             "TX family_a.param_set_3f (Work parameter of a unit) unit=2 "
             "parameter=1 value=-300");
  EXPECT_EQ (describe_rx (bytes ({ kAddress, 0x27 /*@r04*/, 0xDC, 0x05, 0x00,
                                   0x00, 0xD4, 0x04, 0x00, 0x00, 0x00, 0x00,
                                   0x01, 0x00, 0x10, 0x00, 0x00, 0x00 })),
             "RX family_a.stream_27 (Measurement result) time=1500 ms "
             "absorbance=1236 photo=65536 reference=16");
  EXPECT_EQ (describe_rx (bytes ({ kAddress, 0x44 /*@r04*/, 0x34, 0x12, 0x02,
                                   0x01, 0x07, 0x00, 0x00, 0x00 })),
             "RX family_a.error_44 (Error report) code=4660 severity=fatal "
             "has_value=1 value=7");
  EXPECT_EQ (describe_rx (bytes ({ kAddress, 0x00 })),
             "RX unknown key=0x00 len=2: 5A 00");
  EXPECT_EQ (describe_rx (bytes ({ kAddress, 0x27 /*@r04*/, 0xDC })),
             "RX family_a.stream_27 (Measurement result) [decode error: "
             "message shorter than its fields] len=3: 5A "
                 + dev::dump (bytes ({ 0x27 /*@r04*/ })) + " DC");
}

TEST (LumexProtocolDeviceATest,
      GivenTheFamilyTable_WhenExported_ThenEveryRowHasASection)
{
  std::string const markdown
      = index::export_markdown (dev::index_of<family_a> ());
  char const *const rows[]
      = { "query_ad",     "set_65",       "param_set_3f", "param_get_d1",
          "stream_27",    "report_d9",    "error_44",     "event_d4",
          "param_get_83", "param_set_cc", "report_41" };
  for (char const *row : rows)
    EXPECT_NE (markdown.find (row), std::string::npos) << row;
  EXPECT_NE (markdown.find ("Measurement result"), std::string::npos);
}
