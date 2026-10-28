// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Device F (lumex/tests/core/protocol/devices/device_f): every command of the
// table on reference frames, the state message in its three layouts (chosen
// by the session and by a field), the override of the identity reply over the
// family, the describe lines of the log and the index.

#include <cstdint>
#include <initializer_list>
#include <string>
#include <system_error>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/devices/device_f/LumexProtocolDeviceFTable.hpp"

namespace
{
namespace tables = lumex_tests::tables_f;
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace index = lumex::core::protocol::index;
using lumex::core::protocol::error::protocol_errc;
using tables::bytes;
using tables::payload_of;
using tables::view_of;

using device = tables::device_f;
using family = tables::family_f;

index::protocol_index_t const &
device_index ()
{
  index::index_result_t const &result = index::protocol_index<device> ();
  EXPECT_TRUE (result.has_value ());
  return *result;
}

std::string
tx (layout::bytes_t const &frame,
    index::session_context_t const *session = nullptr)
{
  return device_index ().describe (view_of (frame), index::frame_direction::tx,
                                   session);
}

std::string
rx (layout::bytes_t const &frame,
    index::session_context_t const *session = nullptr)
{
  return device_index ().describe (view_of (frame), index::frame_direction::rx,
                                   session);
}

layout::bytes_t
payload_bytes (layout::bytes_t const &frame)
{
  return layout::bytes_t (frame.begin () + 2, frame.end ());
}

// The error of decoding a frame as the request or the reply of a row (no
// error when it decodes).
template <typename Row>
std::error_code
request_error (layout::bytes_t const &frame)
{
  layout::decode_result_t<typename layout::request_of<Row>::type> const decoded
      = layout::decode_request<device, Row> (payload_of (frame));
  return decoded ? std::error_code () : decoded.error ();
}

template <typename Row>
std::error_code
reply_error (layout::bytes_t const &frame,
             index::session_context_t const *session = nullptr)
{
  field::decode_context_t const context = session != nullptr
                                              ? session->decode_context ()
                                              : field::decode_context_t ();
  layout::decode_result_t<typename layout::reply_value_of<Row>::type> const
      decoded
      = layout::decode_reply<device, Row> (
          payload_of (frame), session != nullptr ? &context : nullptr);
  return decoded ? std::error_code () : decoded.error ();
}

template <typename Row>
void
expect_request_round_trip (layout::bytes_t const &frame)
{
  layout::decode_result_t<typename layout::request_of<Row>::type> const decoded
      = layout::decode_request<device, Row> (payload_of (frame));
  ASSERT_TRUE (decoded.has_value ());
  layout::bytes_t again;
  ASSERT_TRUE ((layout::encode_request_record<device, Row> (again, *decoded)
                    .has_value ()));
  EXPECT_EQ (again, payload_bytes (frame));
}

template <typename Row>
void
expect_reply_round_trip (layout::bytes_t const &frame,
                         index::session_context_t const *session = nullptr)
{
  field::decode_context_t const context = session != nullptr
                                              ? session->decode_context ()
                                              : field::decode_context_t ();
  field::decode_context_t const *const used
      = session != nullptr ? &context : nullptr;
  layout::decode_result_t<typename layout::reply_value_of<Row>::type> const
      decoded
      = layout::decode_reply<device, Row> (payload_of (frame), used);
  ASSERT_TRUE (decoded.has_value ());
  layout::bytes_t again;
  ASSERT_TRUE ((
      layout::encode_reply<device, Row> (again, *decoded, used).has_value ()));
  EXPECT_EQ (again, payload_bytes (frame));
}

index::session_context_t
session_with (std::int64_t layout_value)
{
  index::session_context_t session;
  session.set ("thermo_layout", layout_value);
  return session;
}

// A number as decimal text (the log shows keys and codes in decimal).
std::string
dec (unsigned value)
{
  return std::to_string (value);
}

// The frames of the reference vectors (address, key, body).
layout::bytes_t
frame_of (std::uint8_t key, std::initializer_list<int> body)
{
  layout::bytes_t out = bytes ({ 0xC3 /*@r31*/, key });
  layout::bytes_t const tail = bytes (body);
  out.insert (out.end (), tail.begin (), tail.end ());
  return out;
}

layout::bytes_t
old_state ()
{
  return frame_of (0xD9 /*@r04*/,
                   { 0x02, 0x01, 0xAC, 0x0D, 0x9F, 0x0D, 0xA7, 0x08 });
}

layout::bytes_t
old_state_with_tail ()
{
  return frame_of (0xD9 /*@r04*/,
                   { 0x02, 0x01, 0xAC, 0x0D, 0x9F, 0x0D, 0xA7, 0x08, 0x58,
                     0x02, 0x06, 0xFF, 0x7A, 0x00, 0x8A, 0xFF });
}

layout::bytes_t
near_state (int valve)
{
  return frame_of (0xD9 /*@r04*/,
                   { 0xAD /*@r04*/, 0x02, 0xAC, 0x0D, 0x9F, 0x0D, 0xA7, 0x08,
                     0x08, 0x07, valve, 0x00, 0x80, 0x51, 0x01, 0x00 });
}

layout::bytes_t
far_state ()
{
  return frame_of (0xD9 /*@r04*/,
                   { 0x65 /*@r04*/, 0x01, 0xAC, 0x0D, 0x9F, 0x0D, 0xA7, 0x08,
                     0x02, 0x01, 0x03, 0x00 });
}
} // namespace

TEST (
    LumexProtocolDeviceFTest,
    GivenTheTable_WhenTheIndexIsBuilt_ThenThereIsNoDuplicateKeyAndTheLayersAreNamed)
{
  index::protocol_index_t const &built = device_index ();
  EXPECT_STREQ (built.protocol_name (), "device_f");
  EXPECT_STREQ (built.base_name (), "family_f");
  EXPECT_EQ (built.rows ().size (), 6u);
  EXPECT_TRUE (index::keys_unique<device>::value);
  EXPECT_EQ (dsl::command_count<family>::value, 2u);
  EXPECT_NE (built.find_name ("query_b8"), nullptr);
  EXPECT_EQ (built.find_integer (0x8E /*@r04*/)->is_event, true);
  EXPECT_EQ (built.find_integer (0x65 /*@r04*/)->is_event, false);
  EXPECT_EQ (built.find_integer (0x09), nullptr);
}

TEST (
    LumexProtocolDeviceFTest,
    GivenTheIdentityReply_WhenTheDeviceOverridesTheFamily_ThenTheVersionBytesSwap)
{
  EXPECT_FALSE ((std::is_same<device::query_ad_t, family::query_ad_t>::value));
  EXPECT_TRUE ((std::is_same<device::debug_8e_t, family::debug_8e_t>::value));
  layout::bytes_t const reply = frame_of (
      0xAD /*@r04*/, { 0x2B, 0x03, 0x07, 0x0B, 0x1D, 0x08, 0x14, 0x05 });
  expect_reply_round_trip<device::query_ad_t> (reply);
  EXPECT_EQ (rx (reply), "RX device_f.query_ad (Query the identity record) "
                         "type=43 sub=3 sw=7 hw=11 year=29 month=8 day=20 "
                         "mask=5");
  // The same bytes read with the row of the family name the bytes the other
  // way round.
  layout::decode_result_t<
      layout::reply_value_of<family::query_ad_t>::type> const base
      = layout::decode_reply<family, family::query_ad_t> (payload_of (reply));
  ASSERT_TRUE (base.has_value ());
  EXPECT_EQ (field::get<1> (*base), 3);
  EXPECT_EQ (field::get<2> (*base), 7);
  EXPECT_EQ (tx (frame_of (0xAD /*@r04*/, {})),
             "TX device_f.query_ad (Query the identity record)");
}

TEST (LumexProtocolDeviceFTest,
      GivenTheWorkingMode_WhenEncodedAndDecoded_ThenTheTemperatureIsHundredths)
{
  layout::bytes_t out;
  ASSERT_TRUE ((layout::encode_request<device, device::set_65_t> (out, 1, 35.0)
                    .has_value ()));
  EXPECT_EQ (out, bytes ({ 0x01, 0x00, 0xAC, 0x0D }));
  layout::bytes_t const request
      = frame_of (0x65 /*@r04*/, { 0x01, 0x00, 0xAC, 0x0D });
  expect_request_round_trip<device::set_65_t> (request);
  EXPECT_EQ (tx (request), "TX device_f.set_65 (Set the working mode) on=1 "
                           "temperature=35 degC");
  layout::decode_result_t<layout::request_of<device::set_65_t>::type> const
      decoded
      = layout::decode_request<device, device::set_65_t> (
          payload_of (request));
  ASSERT_TRUE (decoded.has_value ());
  EXPECT_EQ (field::get<1> (*decoded).raw, 3500);
  layout::bytes_t again;
  layout::encode_result_t const too_hot
      = layout::encode_request<device, device::set_65_t> (again, 1, 400.0);
  ASSERT_FALSE (too_hot.has_value ());
  EXPECT_TRUE (too_hot.error () == protocol_errc::invalid_value);
  EXPECT_TRUE (again.empty ());
}

TEST (
    LumexProtocolDeviceFTest,
    GivenTheOlderFirmware_WhenTheStateIsDecoded_ThenTheWordsAfterTheTemperaturesAreATail)
{
  index::session_context_t const session = session_with (0);
  expect_reply_round_trip<device::report_d9_t> (old_state (), &session);
  expect_reply_round_trip<device::report_d9_t> (old_state_with_tail (),
                                                &session);
  EXPECT_EQ (rx (old_state (), &session),
             "RX device_f.report_d9 (Report the state) state=case 0: "
             "{mode=2 state=1 temperatures={target=35 degC work=34.87 degC "
             "room=22.15 degC} tail=absent}");
  EXPECT_EQ (rx (old_state_with_tail (), &session),
             "RX device_f.report_d9 (Report the state) state=case 0: "
             "{mode=2 state=1 temperatures={target=35 degC work=34.87 degC "
             "room=22.15 degC} tail={600 s -250 122 -118}}");
  layout::bytes_t half = old_state_with_tail ();
  half.pop_back ();
  EXPECT_TRUE (reply_error<device::report_d9_t> (half, &session)
               == protocol_errc::truncated);
}

TEST (LumexProtocolDeviceFTest,
      GivenTheNewerFirmware_WhenTheStateIsDecoded_ThenTheNodeChoosesTheRest)
{
  index::session_context_t const session = session_with (1);
  expect_reply_round_trip<device::report_d9_t> (near_state (0), &session);
  expect_reply_round_trip<device::report_d9_t> (near_state (1), &session);
  expect_reply_round_trip<device::report_d9_t> (far_state (), &session);
  EXPECT_EQ (rx (near_state (1), &session),
             "RX device_f.report_d9 (Report the state) state=case 1: {node="
                 + dec (0xAD /*@r04*/)
                 + " node_mode=2 temperatures={target=35 degC work=34.87 "
                   "degC room=22.15 degC} rest=case "
                 + dec (0xAD /*@r04*/)
                 + ": {mode_time=1800 s valve=1 device_time=86400 s}}");
  EXPECT_EQ (rx (far_state (), &session),
             "RX device_f.report_d9 (Report the state) state=case 1: {node="
                 + dec (0x65 /*@r04*/)
                 + " node_mode=1 temperatures={target=35 degC work=34.87 "
                   "degC room=22.15 degC} rest=case "
                 + dec (0x65 /*@r04*/) + ": {state=258 mode=3 error=0}}");
  layout::bytes_t unknown = far_state ();
  unknown[2] = 0x99;
  EXPECT_TRUE (reply_error<device::report_d9_t> (unknown, &session)
               == protocol_errc::unknown_case);
}

TEST (
    LumexProtocolDeviceFTest,
    GivenTwoStatesOfTheNearNode_WhenTheValveWordIsRead_ThenItRisesFromZeroToOne)
{
  index::session_context_t const session = session_with (1);
  field::decode_context_t const context = session.decode_context ();
  int valves[2] = { -1, -1 };
  for (int i = 0; i < 2; ++i)
    {
      layout::bytes_t const frame = near_state (i);
      layout::decode_result_t<
          layout::reply_value_of<device::report_d9_t>::type> const decoded
          = layout::decode_reply<device, device::report_d9_t> (
              payload_of (frame), &context);
      ASSERT_TRUE (decoded.has_value ());
      ASSERT_EQ (field::get<0> (*decoded).cases.index (), 1u);
      field::value_of<tables::new_state_t>::type const &fresh
          = field::get<0> (*decoded).cases.get<1> ();
      ASSERT_EQ (field::get<3> (fresh).cases.index (), 1u);
      valves[i] = static_cast<int> (
          field::get<1> (field::get<3> (fresh).cases.get<1> ()));
    }
  // The external start of a measurement is this edge; the dialogue derives
  // it, the table only names the field.
  EXPECT_EQ (valves[0], 0);
  EXPECT_EQ (valves[1], 1);
}

TEST (LumexProtocolDeviceFTest,
      GivenNoSessionValue_WhenTheStateIsDecoded_ThenTheLayoutCannotBeChosen)
{
  EXPECT_TRUE (reply_error<device::report_d9_t> (old_state ())
               != std::error_code ());
  index::session_context_t const other = session_with (7);
  EXPECT_TRUE (reply_error<device::report_d9_t> (old_state (), &other)
               == protocol_errc::unknown_case);
  EXPECT_NE (rx (old_state ()).find ("[decode error:"), std::string::npos);
}

TEST (LumexProtocolDeviceFTest,
      GivenTheDebugCounters_WhenDecoded_ThenTheSignedWordsKeepTheirExtremes)
{
  layout::bytes_t const frame = frame_of (
      0x8E /*@r04*/, { 0xFF, 0xFF, 0x2C, 0x01, 0xFF, 0x7F, 0x00, 0x80 });
  expect_reply_round_trip<device::debug_8e_t> (frame);
  EXPECT_EQ (rx (frame), "RX device_f.debug_8e (Report the debug counters) "
                         "c0=-1 c1=300 c2=32767 c3=-32768");
}

TEST (LumexProtocolDeviceFTest,
      GivenAnErrorAndANodeQuery_WhenEncodedAndDecoded_ThenTheyAreOneByte)
{
  layout::bytes_t const failure = frame_of (0x44 /*@r04*/, { 0x07 });
  expect_reply_round_trip<device::error_44_t> (failure);
  EXPECT_EQ (rx (failure), "RX device_f.error_44 (Report an error) reply=7");
  layout::bytes_t out;
  ASSERT_TRUE ((layout::encode_request<device, device::query_b8_t> (out, 2)
                    .has_value ()));
  EXPECT_EQ (out, bytes ({ 0x02 }));
  layout::bytes_t const request = frame_of (0xB8 /*@r04*/, { 0x02 });
  expect_request_round_trip<device::query_b8_t> (request);
  EXPECT_EQ (tx (request), "TX device_f.query_b8 (Query a node) node=2");
  EXPECT_TRUE (request_error<device::query_b8_t> (frame_of (0xB8 /*@r04*/, {}))
               == protocol_errc::truncated);
  EXPECT_TRUE (request_error<device::query_b8_t> (
                   frame_of (0xB8 /*@r04*/, { 0x02, 0x03 }))
               == protocol_errc::trailing_bytes);
}
