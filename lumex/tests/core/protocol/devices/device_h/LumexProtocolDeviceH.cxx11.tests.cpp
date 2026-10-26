// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Device H (LumexProtocolDeviceH.hpp): every request and reply on
// reference vectors, the two addresses of one table, the models told apart
// by the device information and their parameters, the scaled measurement
// and scan, the mode flags of a state byte, and the status message nested
// in the data of a bus packet.

#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/devices/LumexProtocolDevicesSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_h/LumexProtocolDeviceH.hpp"

namespace
{
namespace dev = lumex_tests::devices;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace index = lumex::core::protocol::index;
namespace registry = lumex::core::protocol::registry;
namespace error = lumex::core::protocol::error;
namespace h = lumex_tests::device_h;
using dev::bytes;
using dev::bytes_t;
using dev::join;
using dev::tail_of;
using h::device_h;
using h::h_can;
using h::h_work;

template <typename Row>
layout::decode_result_t<typename layout::reply_value_of<Row>::type>
reply_of (bytes_t const &message)
{
  return layout::decode_reply<device_h, Row> (dev::view_of (message));
}

std::string
describe_rx (bytes_t const &frame)
{
  return dev::index_of<device_h> ().describe (dev::view_of (frame),
                                              index::frame_direction::rx);
}
} // namespace

TEST (LumexProtocolDeviceHTest,
      GivenTheTwoAddresses_WhenFramesAreMatched_ThenOneTableServesBoth)
{
  index::protocol_index_t const &protocol = dev::index_of<device_h> ();
  bytes_t const scan
      = bytes ({ 0x00, 0x00, 0x00, 0x00, 0x38, 0x63, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });
  EXPECT_TRUE (dev::finds<device_h::stream_fa_t> (
      protocol, join (bytes ({ h::kAddressH1, 0xFA /*@r04*/ }), scan), 2));
  EXPECT_TRUE (dev::finds<device_h::stream_fa_t> (
      protocol, join (bytes ({ h::kAddressH2, 0xFA /*@r04*/ }), scan), 2));
  EXPECT_EQ (
      describe_rx (join (bytes ({ h::kAddressH2, 0xFA /*@r04*/ }), scan)),
      "RX device_h.stream_fa (Fast scan result) time=0 ms "
      "wavelength=254 nm photo=0 reference=0");
  EXPECT_EQ (protocol.rows ().size (), 12u);
  EXPECT_TRUE ((index::build_index<device_h, h_work> ()).has_value ());
}

TEST (LumexProtocolDeviceHTest,
      GivenTheInformation_WhenTheModelIsKnown_ThenItsParametersAreAvailable)
{
  bytes_t const network = bytes ({ 0x39, 0x30, 0x00, 0x00 });
  bytes_t const full = bytes ({ 0x0A, 0x01, 0x02, 0x00, 0x19, 0x03, 0x0E, 0x00,
                                0x39, 0x30, 0x00, 0x00 });
  EXPECT_TRUE (
      (dev::reply_round_trip<device_h, device_h::query_ad_t> (network)));
  EXPECT_TRUE ((dev::reply_round_trip<device_h, device_h::query_ad_t> (full)));
  layout::decode_result_t<
      layout::reply_value_of<device_h::query_ad_t>::type> const information
      = reply_of<device_h::query_ad_t> (full);
  ASSERT_TRUE (information.has_value ());
  ASSERT_EQ (field::get<0> (*information).index (), 1u);
  EXPECT_EQ (field::get<8> (field::get<0> (*information).get<1> ()), 12345u);

  registry::registry_entry_t const &speed
      = h_work::entries ()
            .data ()[static_cast<std::size_t> (h_work::motor_speed)];
  index::session_context_t session;
  EXPECT_TRUE (session.available (speed));
  session.set_model ("h2");
  EXPECT_FALSE (session.available (speed));
  session.set_model ("h4");
  EXPECT_TRUE (session.available (speed));
}

TEST (LumexProtocolDeviceHTest,
      GivenTheControlMode_WhenSet_ThenTheReplyIsTheModeNowInForce)
{
  using lumex_tests::device_a::new_header;
  using lumex_tests::device_a::request_frame;
  bytes_t const frame = request_frame<device_h, device_h::set_65_t> (
      new_header (h::kAddressH2, 0x65 /*@r04*/), h::control::host);
  EXPECT_EQ (frame, bytes ({ h::kAddressH2, 0x65 /*@r04*/, 0x65 /*@r04*/ }));
  layout::decode_result_t<
      layout::reply_value_of<device_h::set_65_t>::type> const mode
      = reply_of<device_h::set_65_t> (bytes ({ 0xAD /*@r04*/ }));
  ASSERT_TRUE (mode.has_value ());
  EXPECT_TRUE (*mode == h::control::keyboard);
  EXPECT_FALSE (
      (reply_of<device_h::set_65_t> (bytes ({ 0x00 }))).has_value ());
}

TEST (LumexProtocolDeviceHTest,
      GivenTheWorkParameters_WhenWritten_ThenTheRegistryChecksThem)
{
  bytes_t out;
  ASSERT_TRUE ((layout::encode_request<device_h, device_h::param_set_3f_t> (
                    out, h_work::target_wavelength, 254))
                   .has_value ());
  EXPECT_EQ (out, bytes ({ 0x3F /*@r04*/, 0xAD /*@r04*/, 0xFE, 0x00 }));
  bytes_t refused;
  EXPECT_TRUE ((layout::encode_request<device_h, device_h::param_set_3f_t> (
                    refused, h_work::frequency, 100))
                   .error ()
               == error::protocol_errc::out_of_limits);
  EXPECT_TRUE (
      (dev::reply_round_trip<device_h, device_h::param_set_3f_t> (out)));
  EXPECT_TRUE ((dev::request_round_trip<device_h, device_h::param_get_d1_t> (
      bytes ({ 0xD1 /*@r04*/, 0x65 /*@r04*/ }))));
  EXPECT_EQ (describe_rx (
                 join (bytes ({ h::kAddressH2, 0xD1 /*@r04*/ }),
                       bytes ({ 0x65 /*@r04*/, 0x65 /*@r04*/, 0x0A, 0x00 }))),
             "RX device_h.param_get_d1 (Read a work parameter of a unit) "
             "parameter=frequency value=10 Hz");
}

TEST (LumexProtocolDeviceHTest,
      GivenTheMeasurement_WhenDescribed_ThenTheScaleOfTheTableIsTheOnlyOne)
{
  bytes_t const frame = bytes ({ h::kAddressH2, 0x27 /*@r04*/, 0x64, 0x00,
                                 0x00, 0x00, 0x87, 0xD6, 0x12, 0x00, 0x10,
                                 0x27, 0x00, 0x00, 0x20, 0x4E, 0x00, 0x00 });
  EXPECT_EQ (describe_rx (frame),
             "RX device_h.stream_27 (Measurement result) time=100 ms "
             "absorbance=1234.567 mAU photo=10000 reference=20000");
  EXPECT_TRUE ((dev::reply_round_trip<device_h, device_h::stream_27_t> (
      tail_of (frame, 2))));
  EXPECT_EQ (lumex::core::protocol::dsl::get_option<
                 lumex::core::protocol::dsl::kind::stream> (
                 device_h::stream_27_t::options ())
                 .interval_ms,
             100u);
}

TEST (LumexProtocolDeviceHTest,
      GivenTheStateOfTheMainUnit_WhenDecoded_ThenEveryFlagIsItsOwnBit)
{
  layout::decode_result_t<
      layout::reply_value_of<device_h::report_d9_t>::type> const state
      = reply_of<device_h::report_d9_t> (bytes ({ 0xAD /*@r04*/, 0x83 }));
  ASSERT_TRUE (state.has_value ());
  EXPECT_EQ (describe_rx (bytes (
                 { h::kAddressH1, 0xD9 /*@r04*/, 0xAD /*@r04*/, 0x83 })),
             "RX device_h.report_d9 (State of a unit) unit=case "
                 + dev::dec (0xAD /*@r04*/)
                 + ": {mode={external=true analysis=true self_test=false "
                   "calibration=false valve=true}}");
  EXPECT_TRUE ((dev::reply_round_trip<device_h, device_h::report_d9_t> (
      bytes ({ 0xAD /*@r04*/, 0x8C }))));
}

TEST (LumexProtocolDeviceHTest,
      GivenABusPacket_WhenItsDataIsAStatus_ThenTheCharacterChoosesTheMessage)
{
  bytes_t const packet
      = bytes ({ 0x23, 0x01, 0x02, 0x08, 0x00, 'Q' /*@r11*/, 0x41, 0x05, 0x2C,
                 0x01, 0x0A, 0x01, 0x10, 0x27, 0x00, 0x00 });
  layout::decode_result_t<
      layout::reply_value_of<device_h::report_41_t>::type> const bus
      = reply_of<device_h::report_41_t> (packet);
  ASSERT_TRUE (bus.has_value ());
  EXPECT_EQ (field::get<0> (*bus), 0x0123u);
  EXPECT_EQ (field::get<4> (*bus), 10000u);
  bytes_t const data (field::get<3> (*bus).begin (),
                      field::get<3> (*bus).end ());
  layout::decode_result_t<field::value_of<h::status_data_t>::type> const status
      = layout::decode<layout::binary_layout<>, h::status_data_t> (
          dev::view_of (data));
  ASSERT_TRUE (status.has_value ());
  EXPECT_EQ (field::get<1> (*status).key, 'Q' /*@r11*/);
  layout::decode_result_t<layout::request_of<h_can::status_a_t>::type> const
      first_status
      = layout::decode_nested<h_can, h_can::status_a_t> (
          field::get<1> (*status));
  ASSERT_TRUE (first_status.has_value ());
  EXPECT_TRUE (field::get<0> (field::get<0> (*first_status).parts)
               == h::status_a_state::state_1);
  EXPECT_TRUE (field::get<1> (field::get<0> (*first_status).parts));
  EXPECT_FALSE (field::get<2> (field::get<0> (*first_status).parts));
  EXPECT_EQ (field::get<2> (*first_status), 300);
  EXPECT_EQ (field::get<3> (*first_status), 266);
  // The state takes all six low bits: with bit 5 set it is no state of the
  // table (refused, or the number 33), never the state of the low five bits.
  bytes_t const wide
      = bytes ({ 0x00, 'Q' /*@r11*/, 0x21, 0x05, 0x2C, 0x01, 0x0A, 0x01 });
  layout::decode_result_t<field::value_of<h::status_data_t>::type> const
      wide_status
      = layout::decode<layout::binary_layout<>, h::status_data_t> (
          dev::view_of (wide));
  ASSERT_TRUE (wide_status.has_value ());
  layout::decode_result_t<layout::request_of<h_can::status_a_t>::type> const
      wide_state
      = layout::decode_nested<h_can, h_can::status_a_t> (
          field::get<1> (*wide_status));
  EXPECT_TRUE (
      !wide_state.has_value ()
      || static_cast<int> (field::get<0> (field::get<0> (*wide_state).parts))
             == 33);
  EXPECT_TRUE ((layout::decode_nested<h_can, h_can::status_b_t> (
                    field::get<1> (*status)))
                   .error ()
               == error::protocol_errc::unknown_frame);
  EXPECT_TRUE (
      (dev::reply_round_trip<device_h, device_h::report_41_t> (packet)));

  bytes_t const other = bytes ({ 0x00, 'N' /*@r11*/, 1, 2, 3, 4, 5, 6 });
  layout::decode_result_t<field::value_of<h::status_data_t>::type> const
      other_status
      = layout::decode<layout::binary_layout<>, h::status_data_t> (
          dev::view_of (other));
  ASSERT_TRUE (other_status.has_value ());
  EXPECT_TRUE ((layout::decode_nested<h_can, h_can::status_b_t> (
                    field::get<1> (*other_status)))
                   .has_value ());
  // Status message B is no status message A.
  EXPECT_FALSE ((layout::decode_nested<h_can, h_can::status_a_t> (
                     field::get<1> (*other_status)))
                    .has_value ());
}
