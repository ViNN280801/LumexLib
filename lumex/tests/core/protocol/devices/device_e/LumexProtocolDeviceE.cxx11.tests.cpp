// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Device E (lumex/tests/core/protocol/devices/device_e): every command of the
// table on reference frames (request and reply, encode and decode), the
// describe lines of the log, the layers (family and device), the parameter
// tables addressed by a pair and by a mnemonic, and the index.

#include <cstdint>
#include <string>
#include <system_error>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/devices/device_e/LumexProtocolDeviceETable.hpp"

namespace
{
namespace tables = lumex_tests::tables_e;
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace registry = lumex::core::protocol::registry;
namespace index = lumex::core::protocol::index;
using lumex::core::protocol::error::protocol_errc;
using tables::bytes;
using tables::payload_of;
using tables::view_of;

using device = tables::device_e;

index::protocol_index_t const &
device_index ()
{
  index::index_result_t const &result
      = index::protocol_index<device, tables::drive, tables::service> ();
  EXPECT_TRUE (result.has_value ());
  return *result;
}

std::string
tx (layout::bytes_t const &frame)
{
  return device_index ().describe (view_of (frame),
                                   index::frame_direction::tx);
}

std::string
rx (layout::bytes_t const &frame)
{
  return device_index ().describe (view_of (frame),
                                   index::frame_direction::rx);
}

// The payload of a frame (without the address and the key) as a buffer.
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
reply_error (layout::bytes_t const &frame)
{
  layout::decode_result_t<typename layout::reply_value_of<Row>::type> const
      decoded
      = layout::decode_reply<device, Row> (payload_of (frame));
  return decoded ? std::error_code () : decoded.error ();
}

// A request frame decodes and encodes back to the same bytes.
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

// A reply frame decodes and encodes back to the same bytes.
template <typename Row>
void
expect_reply_round_trip (layout::bytes_t const &frame)
{
  layout::decode_result_t<typename layout::reply_value_of<Row>::type> const
      decoded
      = layout::decode_reply<device, Row> (payload_of (frame));
  ASSERT_TRUE (decoded.has_value ());
  layout::bytes_t again;
  ASSERT_TRUE (
      (layout::encode_reply<device, Row> (again, *decoded).has_value ()));
  EXPECT_EQ (again, payload_bytes (frame));
}

// A number as decimal text (the log shows keys and codes in decimal).
std::string
dec (unsigned value)
{
  return std::to_string (value);
}

// The bytes of a frame as the log dumps them: two upper-case digits each.
std::string
hexdump (layout::bytes_t const &frame)
{
  static char const digits[] = "0123456789ABCDEF";
  std::string out;
  for (std::size_t i = 0; i < frame.size (); ++i)
    {
      if (i != 0)
        out += ' ';
      out += digits[(frame[i] >> 4) & 0x0F];
      out += digits[frame[i] & 0x0F];
    }
  return out;
}

// The frames of the reference vectors.
layout::bytes_t
query_request ()
{
  return bytes ({ 0x80 /*@r31*/, 0xAD /*@r04*/ });
}

layout::bytes_t
query_reply ()
{
  return bytes ({ 0x80 /*@r31*/, 0xAD /*@r04*/, 0x2D, 0x07, 0x03, 0x0B, 0x1D,
                  0x09, 0x11, 0x06, 0x87, 0xD6, 0x12, 0x00 });
}

layout::bytes_t
mode_frame (std::initializer_list<int> body)
{
  layout::bytes_t out = bytes ({ 0x80 /*@r31*/, 0x65 /*@r04*/ });
  layout::bytes_t const tail = bytes (body);
  out.insert (out.end (), tail.begin (), tail.end ());
  return out;
}
} // namespace

TEST (
    LumexProtocolDeviceETest,
    GivenTheTable_WhenTheIndexIsBuilt_ThenThereIsNoDuplicateKeyAndEveryRowIsFound)
{
  index::protocol_index_t const &built = device_index ();
  EXPECT_STREQ (built.protocol_name (), "device_e");
  EXPECT_STREQ (built.base_name (), "family_e");
  EXPECT_EQ (built.key_width (), 1u);
  EXPECT_EQ (built.rows ().size (), 9u);
  EXPECT_EQ (built.registries ().size (), 2u);
  EXPECT_TRUE (index::keys_unique<device>::value);
  EXPECT_NE (built.find_name ("query_ad"), nullptr);
  EXPECT_NE (built.find_name ("param_set_cc"), nullptr);
  EXPECT_EQ (built.find_name ("query_ae"), nullptr);
  EXPECT_EQ (built.find_integer (0x65 /*@r04*/)->entry->identifier,
             std::string ("set_65"));
  EXPECT_EQ (built.find_integer (0x27 /*@r04*/)->is_event, true);
  EXPECT_EQ (built.find_integer (0x65 /*@r04*/)->is_event, false);
  EXPECT_EQ (built.find_integer (0x66), nullptr);
}

TEST (
    LumexProtocolDeviceETest,
    GivenTheFamilyLayer_WhenTheDeviceExtendsIt_ThenTheRowsAreListedInOrderAndTheStateRowIsTheDevices)
{
  using family = tables::family_e;
  EXPECT_EQ (dsl::command_count<family>::value, 2u);
  EXPECT_EQ (dsl::command_count<device>::value, 9u);
  EXPECT_TRUE ((std::is_same<device::query_ad_t, family::query_ad_t>::value));
  EXPECT_FALSE (
      (std::is_same<device::report_d9_t, family::report_d9_t>::value));
  EXPECT_TRUE (
      (std::is_same<dsl::list_at<0, dsl::commands_of<device>::type>::type,
                    family::query_ad_t>::value));
  EXPECT_TRUE (
      (std::is_same<dsl::list_at<1, dsl::commands_of<device>::type>::type,
                    device::report_d9_t>::value));
}

TEST (
    LumexProtocolDeviceETest,
    GivenTheIdentityQuery_WhenEncodedAndDecoded_ThenTheRequestIsEmptyAndTheReplyHasEveryField)
{
  layout::bytes_t request;
  ASSERT_TRUE ((layout::encode_request<device, device::query_ad_t> (request)
                    .has_value ()));
  EXPECT_TRUE (request.empty ());
  expect_request_round_trip<device::query_ad_t> (query_request ());
  expect_reply_round_trip<device::query_ad_t> (query_reply ());
  EXPECT_EQ (tx (query_request ()),
             "TX device_e.query_ad (Query the identity record)");
  EXPECT_EQ (rx (query_reply ()),
             "RX device_e.query_ad (Query the identity record) type=45 sw=7 "
             "sub=3 hw=11 year=29 month=9 day=17 config={two_units=true} "
             "serial=1234567");
  layout::bytes_t shorter = query_reply ();
  shorter.pop_back ();
  EXPECT_TRUE (reply_error<device::query_ad_t> (shorter)
               == protocol_errc::truncated);
}

TEST (LumexProtocolDeviceETest,
      GivenTheIdentityReply_WhenTheHardwareBitIsRead_ThenTheOtherBitsAreKept)
{
  layout::decode_result_t<
      layout::reply_value_of<device::query_ad_t>::type> const decoded
      = layout::decode_reply<device, device::query_ad_t> (
          payload_of (query_reply ()));
  ASSERT_TRUE (decoded.has_value ());
  EXPECT_EQ (field::get<7> (*decoded).raw, 0x06u);
  EXPECT_EQ (field::get<8> (*decoded), 0x0012D687);
}

TEST (
    LumexProtocolDeviceETest,
    GivenEveryMode_WhenTheModeRequestIsRoundTripped_ThenTheLayoutOfTheModeIsKept)
{
  layout::bytes_t const frames[] = {
    mode_frame ({ 0xAD /*@r04*/, 0x00, 0xE2, 0x04, 0x2C, 0x01, 0x00, 0x00 }),
    mode_frame ({ 0x65 /*@r04*/, 0x00, 0xE2, 0x04, 0x2C, 0x01, 0x00, 0x00 }),
    mode_frame ({ 0x3F /*@r04*/, 0x00, 0xE2, 0x04, 0x2C, 0x01, 0x90, 0x01 }),
    mode_frame ({ 0xD1 /*@r04*/, 0x0A, 0x64, 0x00, 0xF4, 0x01, 0x00, 0x00 }),
    mode_frame ({ 0x2C /*@r04*/, 0x00, 0x64, 0x00, 0xF4, 0x01, 0x58, 0x02 }),
    mode_frame ({ 0x72 /*@r04*/, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }),
    mode_frame ({ 0xF3 /*@r04*/, 0x00, 0x90, 0x01, 0x00, 0x00, 0x00, 0x00 }),
    mode_frame ({ 0x15 /*@r04*/, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }),
  };
  for (std::size_t i = 0; i < sizeof frames / sizeof frames[0]; ++i)
    {
      SCOPED_TRACE (i);
      ASSERT_EQ (frames[i].size (), 10u);
      expect_request_round_trip<device::set_65_t> (frames[i]);
    }
}

TEST (LumexProtocolDeviceETest,
      GivenTheModes_WhenDescribed_ThenTheCaseAndTheScaledValuesAreShown)
{
  EXPECT_EQ (tx (mode_frame (
                 { 0xAD /*@r04*/, 0x00, 0xE2, 0x04, 0x2C, 0x01, 0x00, 0x00 })),
             std::string (
                 "TX device_e.set_65 (Set the working mode) mode={mode=case ")
                 + dec (0xAD /*@r04*/) + ": {arg=0 a=12.5 b=30 c=0}}");
  EXPECT_EQ (tx (mode_frame (
                 { 0xD1 /*@r04*/, 0x0A, 0x64, 0x00, 0xF4, 0x01, 0x00, 0x00 })),
             std::string (
                 "TX device_e.set_65 (Set the working mode) mode={mode=case ")
                 + dec (0xD1 /*@r04*/) + ": {arg=10 min a=1 b=5 c=0}}");
  EXPECT_EQ (tx (mode_frame (
                 { 0x2C /*@r04*/, 0x00, 0x64, 0x00, 0xF4, 0x01, 0x58, 0x02 })),
             std::string (
                 "TX device_e.set_65 (Set the working mode) mode={mode=case ")
                 + dec (0x2C /*@r04*/) + ": {arg=0 a=1 b=5 c=600 s}}");
}

TEST (LumexProtocolDeviceETest,
      GivenAModeTheTableDoesNotKnow_WhenDecoded_ThenTheCaseIsUnknown)
{
  layout::bytes_t const frame
      = mode_frame ({ 0x99, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });
  EXPECT_TRUE (request_error<device::set_65_t> (frame)
               == protocol_errc::unknown_case);
  EXPECT_NE (tx (frame).find ("[decode error:"), std::string::npos);
  layout::bytes_t const shorter
      = mode_frame ({ 0xAD /*@r04*/, 0x00, 0xE2, 0x04, 0x2C, 0x01, 0x00 });
  EXPECT_TRUE (request_error<device::set_65_t> (shorter)
               == protocol_errc::truncated);
}

TEST (
    LumexProtocolDeviceETest,
    GivenADriveParameter_WhenWritten_ThenTheKeyPairAndTheRawValueAreOnTheWire)
{
  layout::bytes_t out;
  ASSERT_TRUE ((layout::encode_request<device, device::set_3f_t> (
                    out, tables::drive::p_65_ad, 12.5)
                    .has_value ()));
  EXPECT_EQ (out, bytes ({ 0x65 /*@r04*/, 0xAD /*@r04*/, 0xE2, 0x04 }));
  layout::bytes_t const request
      = bytes ({ 0x80 /*@r31*/, 0x3F /*@r04*/, 0x65 /*@r04*/, 0xAD /*@r04*/,
                 0xE2, 0x04 });
  expect_request_round_trip<device::set_3f_t> (request);
  expect_reply_round_trip<device::set_3f_t> (request);
  EXPECT_EQ (tx (request), "TX device_e.set_3f (Write a drive parameter) "
                           "parameter=p_65_ad value=12.5");
  EXPECT_EQ (rx (request), "RX device_e.set_3f (Write a drive parameter) "
                           "parameter=p_65_ad value=12.5");
}

TEST (LumexProtocolDeviceETest,
      GivenADriveValueOutsideItsLimits_WhenWritten_ThenNothingIsEncoded)
{
  layout::bytes_t out;
  layout::encode_result_t const high
      = layout::encode_request<device, device::set_3f_t> (
          out, tables::drive::p_65_ad, 300.0);
  ASSERT_FALSE (high.has_value ());
  EXPECT_TRUE (high.error () == protocol_errc::out_of_limits);
  EXPECT_TRUE (out.empty ());
  layout::encode_result_t const read_only
      = layout::encode_request<device, device::set_3f_t> (
          out, tables::drive::p_65_3f, 1);
  EXPECT_TRUE (read_only.has_value ());
  EXPECT_EQ (out, bytes ({ 0x65 /*@r04*/, 0x3F /*@r04*/, 0x01, 0x00 }));
}

TEST (
    LumexProtocolDeviceETest,
    GivenADriveParameterRead_WhenTheReplyComes_ThenTheScaleOfTheEntryIsApplied)
{
  layout::bytes_t out;
  ASSERT_TRUE ((layout::encode_request<device, device::param_get_d1_t> (
                    out, tables::drive::p_65_27)
                    .has_value ()));
  EXPECT_EQ (out, bytes ({ 0x65 /*@r04*/, 0x27 /*@r04*/ }));
  layout::bytes_t const request
      = bytes ({ 0x80 /*@r31*/, 0xD1 /*@r04*/, 0x65 /*@r04*/, 0x27 /*@r04*/ });
  layout::bytes_t const reply
      = bytes ({ 0x80 /*@r31*/, 0xD1 /*@r04*/, 0x65 /*@r04*/, 0x27 /*@r04*/,
                 0x90, 0x01 });
  expect_request_round_trip<device::param_get_d1_t> (request);
  expect_reply_round_trip<device::param_get_d1_t> (reply);
  EXPECT_EQ (tx (request), "TX device_e.param_get_d1 (Read a drive parameter) "
                           "parameter=p_65_27");
  EXPECT_EQ (rx (reply), "RX device_e.param_get_d1 (Read a drive parameter) "
                         "parameter=p_65_27 value=4");
  layout::bytes_t const unknown = bytes (
      { 0x80 /*@r31*/, 0xD1 /*@r04*/, 0x65 /*@r04*/, 0x28, 0x90, 0x01 });
  EXPECT_TRUE (reply_error<device::param_get_d1_t> (unknown)
               == protocol_errc::unknown_case);
}

TEST (LumexProtocolDeviceETest,
      GivenTwoOrThreeValues_WhenWritten_ThenTheThirdIsATailOfTheNewerFirmware)
{
  layout::bytes_t const two
      = bytes ({ 0x80 /*@r31*/, 0x72 /*@r04*/, 0x65 /*@r04*/, 0xDD /*@r04*/,
                 0x90, 0x01, 0x58, 0x02 });
  layout::bytes_t const three
      = bytes ({ 0x80 /*@r31*/, 0x72 /*@r04*/, 0x65 /*@r04*/, 0xDD /*@r04*/,
                 0x90, 0x01, 0x58, 0x02, 0xC8, 0x00 });
  expect_request_round_trip<device::set_72_t> (two);
  expect_request_round_trip<device::set_72_t> (three);
  std::string const head = "TX device_e.set_72 (Write several values of a "
                           "parameter) node="
                           + dec (0x65 /*@r04*/)
                           + " parameter=" + dec (0xDD /*@r04*/)
                           + " first=400 second=600 third=";
  EXPECT_EQ (tx (two), head + "absent");
  EXPECT_EQ (tx (three), head + "{200}");
  layout::bytes_t const half
      = bytes ({ 0x80 /*@r31*/, 0x72 /*@r04*/, 0x65 /*@r04*/, 0xDD /*@r04*/,
                 0x90, 0x01, 0x58, 0x02, 0xC8 });
  EXPECT_TRUE (request_error<device::set_72_t> (half)
               == protocol_errc::truncated);
}

TEST (LumexProtocolDeviceETest,
      GivenAMeasurement_WhenItHasNoTime_ThenTheOlderFirmwareIsDecoded)
{
  layout::bytes_t const older
      = bytes ({ 0x80 /*@r31*/, 0x27 /*@r04*/, 0x32, 0x01, 0x58, 0x02, 0xE2,
                 0x04, 0x10, 0x03 });
  layout::bytes_t const newer
      = bytes ({ 0x80 /*@r31*/, 0x27 /*@r04*/, 0x32, 0x01, 0x58, 0x02, 0xE2,
                 0x04, 0x10, 0x03, 0x45, 0x23, 0x01, 0x00 });
  expect_reply_round_trip<device::event_27_t> (older);
  expect_reply_round_trip<device::event_27_t> (newer);
  EXPECT_EQ (rx (older), "RX device_e.event_27 (Report a measurement) "
                         "flags={state=2 aux=3} valve=1 volume=60 rate=12.5 "
                         "level=78.4 time=absent");
  EXPECT_EQ (rx (newer), "RX device_e.event_27 (Report a measurement) "
                         "flags={state=2 aux=3} valve=1 volume=60 rate=12.5 "
                         "level=78.4 time={74565 ms}");
}

TEST (LumexProtocolDeviceETest,
      GivenTheStateByte_WhenSplit_ThenTheNibblesAreTheStateAndTheAux)
{
  layout::bytes_t const frame
      = bytes ({ 0x80 /*@r31*/, 0x27 /*@r04*/, 0x32, 0x01, 0x58, 0x02, 0xE2,
                 0x04, 0x10, 0x03 });
  layout::decode_result_t<
      layout::reply_value_of<device::event_27_t>::type> const decoded
      = layout::decode_reply<device, device::event_27_t> (payload_of (frame));
  ASSERT_TRUE (decoded.has_value ());
  EXPECT_EQ (field::get<0> (*decoded).raw, 0x32u);
  EXPECT_EQ (field::get<0> (field::get<0> (*decoded).parts), 2u);
  EXPECT_EQ (field::get<1> (field::get<0> (*decoded).parts), 3u);
}

TEST (LumexProtocolDeviceETest,
      GivenTheManagerState_WhenReported_ThenTheEnumerationIsStrict)
{
  layout::bytes_t const known
      = bytes ({ 0x80 /*@r31*/, 0xD9 /*@r04*/, 0xF4 /*@r04*/ });
  expect_reply_round_trip<device::report_d9_t> (known);
  EXPECT_EQ (rx (known), "RX device_e.report_d9 (Report the manager state) "
                         "reply=st_f4");
  layout::bytes_t const unknown
      = bytes ({ 0x80 /*@r31*/, 0xD9 /*@r04*/, 0x01 });
  EXPECT_TRUE (reply_error<device::report_d9_t> (unknown)
               == protocol_errc::invalid_value);
  EXPECT_NE (rx (unknown).find ("[decode error:"), std::string::npos);
}

TEST (LumexProtocolDeviceETest,
      GivenAServiceParameter_WhenReadAndWritten_ThenTheMnemonicIsTwoCharacters)
{
  layout::bytes_t out;
  ASSERT_TRUE ((layout::encode_request<device, device::param_get_83_t> (
                    out, tables::service::s_a)
                    .has_value ()));
  EXPECT_EQ (out, bytes ({ 'I' /*@r11*/, 'P' /*@r11*/ }));
  out.clear ();
  ASSERT_TRUE ((layout::encode_request<device, device::param_set_cc_t> (
                    out, tables::service::s_a, 6699.0)
                    .has_value ()));
  EXPECT_EQ (out,
             bytes ({ 'I' /*@r11*/, 'P' /*@r11*/, 0x2B, 0x1A, 0x00, 0x00 }));
  layout::bytes_t const write
      = bytes ({ 0x80 /*@r31*/, 0xCC /*@r04*/, 'I' /*@r11*/, 'P' /*@r11*/,
                 0x2B, 0x1A, 0x00, 0x00 });
  expect_request_round_trip<device::param_set_cc_t> (write);
  EXPECT_EQ (tx (write), "TX device_e.param_set_cc (Write a service "
                         "parameter) parameter=s_a value=6699");
  layout::bytes_t const reply
      = bytes ({ 0x80 /*@r31*/, 0x83 /*@r04*/, 'I' /*@r11*/, 'P' /*@r11*/,
                 0x2B, 0x1A, 0x00, 0x00 });
  expect_reply_round_trip<device::param_get_83_t> (reply);
  EXPECT_EQ (rx (reply), "RX device_e.param_get_83 (Read a service "
                         "parameter) parameter=s_a value=6699");
  layout::bytes_t const other
      = bytes ({ 0x80 /*@r31*/, 0x83 /*@r04*/, 'I' /*@r11*/, 'W', 0x2B, 0x1A,
                 0x00, 0x00 });
  EXPECT_TRUE (reply_error<device::param_get_83_t> (other)
               == protocol_errc::unknown_case);
}

TEST (LumexProtocolDeviceETest,
      GivenTheParameterTables_WhenChecked_ThenTheKeysAreUniqueAndFound)
{
  registry::entries_t const drive = tables::drive::entries ();
  EXPECT_EQ (drive.size (), 6u);
  std::size_t first = 0;
  std::size_t second = 0;
  EXPECT_FALSE (registry::find_duplicate_key (drive, first, second));
  EXPECT_NE (registry::find_entry (drive, registry::make_unit_param_key (
                                              0x65 /*@r04*/, 0x27 /*@r04*/)),
             nullptr);
  EXPECT_EQ (registry::find_entry (
                 drive, registry::make_unit_param_key (0x65 /*@r04*/, 0x28)),
             nullptr);
  EXPECT_EQ (registry::find_entry (
                 drive, registry::make_unit_param_key (0x3F, 0x27 /*@r04*/)),
             nullptr);
  registry::entries_t const service = tables::service::entries ();
  EXPECT_EQ (service.size (), 1u);
  EXPECT_NE (registry::find_entry (
                 service, registry::make_mnemonic_key ("IP" /*@r11*/)),
             nullptr);
}

TEST (LumexProtocolDeviceETest,
      GivenAFrameOfAnotherAddressOrKey_WhenDescribed_ThenItIsUnknown)
{
  layout::bytes_t const frame = bytes ({ 0x80 /*@r31*/, 0x09, 0x02 });
  EXPECT_EQ (rx (frame), "RX unknown key=0x09 len=3: " + hexdump (frame));
  layout::bytes_t const lone = bytes ({ 0x80 /*@r31*/ });
  EXPECT_EQ (rx (lone), "RX unknown len=1: " + hexdump (lone));
}
