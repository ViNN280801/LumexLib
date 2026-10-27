// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Device D (LumexProtocolDeviceD.hpp): every request and reply on
// reference vectors, the parameter number that means another thing in
// another unit, two or three values in one command, the device information
// chosen by its length, the service parameters by two letters, and the
// system message of the link of this generation against the old one.

#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/devices/LumexProtocolDevicesSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_d/LumexProtocolDeviceD.hpp"

namespace
{
namespace dev = lumex_tests::devices;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace index = lumex::core::protocol::index;
namespace error = lumex::core::protocol::error;
namespace d = lumex_tests::device_d;
using d::d_service;
using d::d_work;
using d::device_d;
using d::kAddress;
using dev::bytes;
using dev::bytes_t;
using dev::join;
using dev::tail_of;
using dev::text_bytes;

template <typename Row>
layout::decode_result_t<typename layout::reply_value_of<Row>::type>
reply_of (bytes_t const &message)
{
  return layout::decode_reply<device_d, Row> (dev::view_of (message));
}

std::string
describe_rx (bytes_t const &frame)
{
  return dev::index_of<device_d> ().describe (dev::view_of (frame),
                                              index::frame_direction::rx);
}
} // namespace

TEST (LumexProtocolDeviceDTest,
      GivenTheInformation_WhenItsLengthDiffers_ThenTheLengthChoosesTheLayout)
{
  bytes_t const network
      = bytes ({ kAddress, 0xAD /*@r04*/, 0x39, 0x30, 0x00, 0x00 });
  bytes_t const usb = bytes (
      { kAddress, 0xAD /*@r04*/, 0x21, 0x02, 0x05, 0x01, 0x19, 0x0B, 0x07 });
  EXPECT_TRUE (dev::finds<device_d::query_ad_t> (dev::index_of<device_d> (),
                                                 network, 2));
  EXPECT_EQ (describe_rx (network),
             "RX device_d.query_ad (Instrument information) "
             "information={serial=12345}");
  EXPECT_EQ (describe_rx (usb),
             "RX device_d.query_ad (Instrument information) "
             "information={model=33 software=2 subversion=5 hardware=1 "
             "year=25 month=11 day=7}");
  EXPECT_TRUE ((dev::reply_round_trip<device_d, device_d::query_ad_t> (
      tail_of (network, 2))));
  EXPECT_TRUE ((dev::reply_round_trip<device_d, device_d::query_ad_t> (
      tail_of (usb, 2))));
  // Five or six bytes are neither form.
  EXPECT_FALSE ((reply_of<device_d::query_ad_t> (
                     bytes ({ 0x21, 0x02, 0x05, 0x01, 0x19 })))
                    .has_value ());
}

TEST (LumexProtocolDeviceDTest,
      GivenParameterOneOfFourUnits_WhenWritten_ThenEachUnitHasItsOwnParameter)
{
  bytes_t excitation;
  ASSERT_TRUE ((layout::encode_request<device_d, device_d::param_set_3f_t> (
                    excitation, d_work::excitation_target, 350.0))
                   .has_value ());
  EXPECT_EQ (excitation,
             bytes ({ d::unit_of::excitation, 0xAD /*@r04*/, 0xAC, 0x0D }));
  bytes_t emission;
  ASSERT_TRUE ((layout::encode_request<device_d, device_d::param_set_3f_t> (
                    emission, d_work::emission_target, 450.0))
                   .has_value ());
  EXPECT_EQ (emission, bytes ({ 0xD1 /*@r04*/, 0xAD /*@r04*/, 0x94, 0x11 }));
  bytes_t refused;
  EXPECT_TRUE ((layout::encode_request<device_d, device_d::param_set_3f_t> (
                    refused, d_work::detector_sensitivity, 1001))
                   .error ()
               == error::protocol_errc::out_of_limits);
  // The targets keep their range: both ends written, a step outside refused.
  for (d_work::id target :
       { d_work::excitation_target, d_work::emission_target })
    {
      for (double inside : { 215.0, 875.0 })
        EXPECT_TRUE (
            (layout::encode_request<device_d, device_d::param_set_3f_t> (
                 refused, target, inside))
                .has_value ())
            << inside;
      for (double outside : { 214.9, 875.1 })
        {
          layout::encode_result_t const result
              = layout::encode_request<device_d, device_d::param_set_3f_t> (
                  refused, target, outside);
          ASSERT_FALSE (result.has_value ()) << outside;
          EXPECT_TRUE (result.error () == error::protocol_errc::out_of_limits)
              << outside;
        }
    }
  EXPECT_TRUE (dev::finds<device_d::param_set_3f_t> (
      dev::index_of<device_d> (),
      join (bytes ({ kAddress, 0x3F /*@r04*/ }), excitation), 2));
  EXPECT_EQ (
      describe_rx (join (bytes ({ kAddress, 0x3F /*@r04*/ }), emission)),
      "RX device_d.param_set_3f (Work parameter of a unit) "
      "parameter=emission_target value=450 nm");
  EXPECT_TRUE ((dev::request_round_trip<device_d, device_d::param_get_d1_t> (
      bytes ({ 0x65 /*@r04*/, 0xAD /*@r04*/ }))));
  EXPECT_TRUE ((dev::reply_round_trip<device_d, device_d::param_get_d1_t> (
      bytes ({ 0x72 /*@r04*/, 0xAD /*@r04*/, 0xF4, 0x01 }))));
  // Parameter 2 of the excitation unit is no entry.
  EXPECT_FALSE ((reply_of<device_d::param_get_d1_t> (
                     bytes ({ 0x3F /*@r04*/, 0x65 /*@r04*/, 0x00, 0x00 })))
                    .has_value ());
}

TEST (LumexProtocolDeviceDTest,
      GivenSeveralValues_WhenTwoOrThreeAreSent_ThenTheThirdIsAnOptionalTail)
{
  bytes_t const two
      = bytes ({ 0x3F /*@r04*/, 0xAD /*@r04*/, 0xAC, 0x0D, 0x94, 0x11 });
  bytes_t const three = join (two, bytes ({ 0x0A, 0x00 }));
  layout::decode_result_t<
      layout::request_of<device_d::param_set_72_t>::type> const pair
      = layout::decode_request<device_d, device_d::param_set_72_t> (
          dev::view_of (two));
  ASSERT_TRUE (pair.has_value ());
  EXPECT_FALSE (field::get<3> (*pair).present);
  layout::decode_result_t<
      layout::request_of<device_d::param_set_72_t>::type> const triple
      = layout::decode_request<device_d, device_d::param_set_72_t> (
          dev::view_of (three));
  ASSERT_TRUE (triple.has_value ());
  EXPECT_TRUE (field::get<3> (*triple).present);
  EXPECT_EQ (field::get<0> (field::get<3> (*triple).fields), 10);
  EXPECT_TRUE (
      (dev::request_round_trip<device_d, device_d::param_set_72_t> (two)));
  EXPECT_TRUE (
      (dev::request_round_trip<device_d, device_d::param_set_72_t> (three)));
  EXPECT_TRUE ((dev::request_round_trip<device_d, device_d::param_set_72_t> (
                   join (three, bytes ({ 0x01 }))))
               == false);
}

TEST (LumexProtocolDeviceDTest,
      GivenAServiceParameter_WhenReadAndWritten_ThenTheTwoLettersAreTheKey)
{
  bytes_t get;
  ASSERT_TRUE ((layout::encode_request<device_d, device_d::param_get_83_t> (
                    get, d_service::s_b))
                   .has_value ());
  EXPECT_EQ (get, text_bytes ("DL"));
  bytes_t set;
  ASSERT_TRUE ((layout::encode_request<device_d, device_d::param_set_cc_t> (
                    set, d_service::s_b, -20))
                   .has_value ());
  EXPECT_EQ (set,
             join (text_bytes ("DL"), bytes ({ 0xEC, 0xFF, 0xFF, 0xFF })));
  EXPECT_TRUE (
      (dev::reply_round_trip<device_d, device_d::param_get_83_t> (set)));
  EXPECT_EQ (describe_rx (join (bytes ({ kAddress, 0x83 /*@r04*/ }), set)),
             "RX device_d.param_get_83 (Read a service parameter) "
             "parameter=s_b value=-20");
  // Two letters the device table does not have.
  EXPECT_FALSE ((reply_of<device_d::param_get_83_t> (
                     join (text_bytes ("ZZ"), bytes ({ 0, 0, 0, 0 }))))
                    .has_value ());
}

TEST (LumexProtocolDeviceDTest,
      GivenTheDiagnosticRequests_WhenEncoded_ThenTheBytesAreTheReference)
{
  using lumex_tests::device_a::new_header;
  using lumex_tests::device_a::request_frame;
  bytes_t const node = request_frame<device_d, device_d::query_b8_t> (
      new_header (kAddress, 0xB8 /*@r04*/), 0x65 /*@r04*/);
  EXPECT_EQ (node, bytes ({ kAddress, 0xB8 /*@r04*/, 0x65 /*@r04*/ }));
  bytes_t const trace = request_frame<device_d, device_d::set_64_t> (
      new_header (kAddress, 0x64 /*@r04*/), 1);
  EXPECT_EQ (trace, bytes ({ kAddress, 0x64 /*@r04*/, 0x01 }));
  index::protocol_index_t const &protocol = dev::index_of<device_d> ();
  EXPECT_TRUE (dev::finds<device_d::query_b8_t> (protocol, node, 2));
  EXPECT_TRUE (dev::finds<device_d::set_64_t> (protocol, trace, 2));
  // The internal bus packet of the family: a node and eight bytes.
  EXPECT_TRUE ((dev::reply_round_trip<device_d, device_d::report_41_t> (
      bytes ({ 0x3F /*@r04*/, 1, 2, 3, 4, 5, 6, 7, 8 }))));
  EXPECT_EQ (protocol.rows ().size (), 14u);
  index::index_result_t const built
      = index::build_index<device_d, d_work, d_service> ();
  ASSERT_TRUE (built.has_value ());
  EXPECT_EQ (built->registries ().size (), 2u);
}

TEST (LumexProtocolDeviceDTest,
      GivenTheSystemMessage_WhenEachGenerationReadsIt_ThenOnlyItsOwnLayoutFits)
{
  bytes_t modern = bytes ({ 0x00, 0x00 });
  for (int counter = 1; counter <= 10; ++counter)
    {
      modern.push_back (static_cast<std::uint8_t> (counter));
      modern.push_back (0x00);
    }
  EXPECT_TRUE (dev::finds<d::d_system::keepalive_00_t> (
      dev::index_of<d::d_system> (), modern, 1));
  layout::decode_result_t<
      layout::reply_value_of<d::d_system::keepalive_00_t>::type> const counters
      = layout::decode_reply<d::d_system, d::d_system::keepalive_00_t> (
          dev::view_of (tail_of (modern, 1)));
  ASSERT_TRUE (counters.has_value ());
  EXPECT_EQ (field::get<1> (*counters)[9], 10u);
  EXPECT_TRUE (
      (dev::reply_round_trip<d::d_system, d::d_system::keepalive_00_t> (
          tail_of (modern, 1))));
  // The old layout of the family is shorter: the counters are trailing
  // bytes there.
  layout::decode_result_t<layout::reply_value_of<
      lumex_tests::device_a::a_system::keepalive_00_t>::type> const old
      = layout::decode_reply<lumex_tests::device_a::a_system,
                             lumex_tests::device_a::a_system::keepalive_00_t> (
          dev::view_of (tail_of (modern, 1)));
  ASSERT_FALSE (old.has_value ());
  EXPECT_TRUE (old.error () == error::protocol_errc::trailing_bytes);
}
