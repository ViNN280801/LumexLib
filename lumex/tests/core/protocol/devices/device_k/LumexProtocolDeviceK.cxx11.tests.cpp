// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Device K (lumex/tests/core/protocol/devices/device_k): the text layout with
// fields of fixed width, one header in front of every message (error, state,
// length), the length that counts characters up to the checksum, state codes
// that are texts, the nibbles of the error of the zone, every command in both
// directions on reference frames with recomputed checks, the describe lines
// and the index.

#include <cstddef>
#include <cstdint>
#include <string>
#include <system_error>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/devices/device_k/LumexProtocolDeviceKTable.hpp"

namespace
{
namespace tables = lumex_tests::tables_k;
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace index = lumex::core::protocol::index;
using lumex::core::protocol::error::protocol_errc;
using tables::bytes_of;
using tables::view_of;
using tables::xor_text;

typedef tables::device_k device;

index::protocol_index_t const &
device_index ()
{
  index::index_result_t const &result = index::protocol_index<device> ();
  EXPECT_TRUE (result.has_value ());
  return *result;
}

// The digits of a wire frame: without the start character, the checksum and
// the carriage return.
std::string
body_of (std::string const &wire)
{
  return wire.substr (1, wire.size () - 4);
}

// The wire frame: the checks that make a frame (start character, check over
// the characters, carriage return, the length field of the header).
void
expect_wire_frame (std::string const &wire)
{
  ASSERT_GE (wire.size (), 18u);
  EXPECT_EQ (wire[0], '!');
  EXPECT_EQ (wire[wire.size () - 1], '\r');
  std::string const body = body_of (wire);
  EXPECT_EQ (wire.substr (wire.size () - 3, 2), xor_text (body));
  // The length counts the characters of the body and of the check.
  std::string const length = body.substr (10, 4);
  EXPECT_EQ (static_cast<std::size_t> (std::stoul (length, nullptr, 16)),
             body.size () + 2);
}

layout::bytes_t
frame_of (std::string const &wire)
{
  return bytes_of (body_of (wire));
}

layout::bytes_view_t
payload_of (layout::bytes_t const &frame)
{
  return layout::bytes_view_t (frame.data () + 2, frame.size () - 2);
}

std::string
tx (std::string const &wire)
{
  return device_index ().describe (view_of (frame_of (wire)),
                                   index::frame_direction::tx);
}

std::string
rx (std::string const &wire)
{
  return device_index ().describe (view_of (frame_of (wire)),
                                   index::frame_direction::rx);
}

template <typename Row>
void
expect_request_round_trip (std::string const &wire)
{
  expect_wire_frame (wire);
  layout::bytes_t const frame = frame_of (wire);
  layout::decode_result_t<typename layout::request_of<Row>::type> const decoded
      = layout::decode_request<device, Row> (payload_of (frame));
  ASSERT_TRUE (decoded.has_value ());
  layout::bytes_t again;
  ASSERT_TRUE ((layout::encode_request_record<device, Row> (again, *decoded)
                    .has_value ()));
  EXPECT_EQ (std::string (again.begin (), again.end ()),
             body_of (wire).substr (2));
}

template <typename Row>
void
expect_reply_round_trip (std::string const &wire)
{
  expect_wire_frame (wire);
  layout::bytes_t const frame = frame_of (wire);
  layout::decode_result_t<typename layout::reply_value_of<Row>::type> const
      decoded
      = layout::decode_reply<device, Row> (payload_of (frame));
  ASSERT_TRUE (decoded.has_value ());
  layout::bytes_t again;
  ASSERT_TRUE (
      (layout::encode_reply<device, Row> (again, *decoded).has_value ()));
  EXPECT_EQ (std::string (again.begin (), again.end ()),
             body_of (wire).substr (2));
}

template <typename Row>
std::error_code
reply_error (std::string const &body)
{
  layout::bytes_t const frame = bytes_of (body);
  layout::decode_result_t<typename layout::reply_value_of<Row>::type> const
      decoded
      = layout::decode_reply<device, Row> (payload_of (frame));
  return decoded ? std::error_code () : decoded.error ();
}

// The body of a frame made from its parts, the length computed: what the
// dialogue does when it sends.
std::string
made_body (std::string const &code, std::string const &error,
           std::string const &state, std::string const &fields)
{
  std::size_t const length = 2 + 12 + fields.size () + 2;
  static char const digits[] = "0123456789ABCDEF";
  std::string text;
  text += digits[(length >> 12) & 0x0F];
  text += digits[(length >> 8) & 0x0F];
  text += digits[(length >> 4) & 0x0F];
  text += digits[length & 0x0F];
  return code + error + state + text + fields;
}

// Two upper-case hexadecimal digits of a byte.
std::string
hx (unsigned value)
{
  static char const digits[] = "0123456789ABCDEF";
  std::string out;
  out += digits[(value >> 4) & 0x0F];
  out += digits[value & 0x0F];
  return out;
}

// A number as decimal text (the log shows keys and codes in decimal).
std::string
dec (unsigned value)
{
  return std::to_string (value);
}

// The frame of the wire: the start character, the body with its length
// computed, the XOR of the characters of the body, a carriage return.
std::string
wire_of (std::string const &code, std::string const &error,
         std::string const &state, std::string const &fields)
{
  std::string const body = made_body (code, error, state, fields);
  return "!" + body + xor_text (body) + "\r";
}

// The fields of the state reply: the counters, the two step texts, the valve
// character, the zone state and the temperatures.
std::string
state_fields ()
{
  return std::string ("00012D0000") + "7A" /*@r11*/ + "56" /*@r11*/ + "010506"
         + std::string (1, '2' /*@r11*/)
         + "0203FF03"
           "01020304"
           "0A14"
           "0064"
         + "01"
           "0078"
           "003C"
         + hx (0xAD /*@r04*/) + "1200DC00E6";
}

std::string
stop_request ()
{
  return wire_of (hx (0x35 /*@r04*/), "0000", "0000", "");
}

std::string
stop_reply ()
{
  return wire_of (hx (0x35 /*@r04*/), "0000", "0003", "");
}

std::string
state_request ()
{
  return wire_of (hx (0xAC /*@r04*/), "0000", "0000", "");
}

std::string
state_reply ()
{
  return wire_of (hx (0xAC /*@r04*/), "0000", "0003", state_fields ());
}

std::string
clean_request ()
{
  return wire_of (hx (0xF4 /*@r04*/), "0000", "0000", "020064");
}

std::string
clean_reply ()
{
  return wire_of (hx (0xF4 /*@r04*/), "0000", "0002", "");
}

std::string
inject_request ()
{
  return wire_of (hx (0x64 /*@r04*/), "0000", "0000",
                  "0105FFFFFF0101" + hx (0x2C /*@r04*/) + "00");
}

std::string
inject_reply ()
{
  return wire_of (hx (0x64 /*@r04*/), "0000", "0003", "");
}

std::string
zone_request ()
{
  return wire_of (hx (0xC9 /*@r04*/), "0000", "0000",
                  "0B07050904010E03080D02060C0400DC0000");
}

std::string
zone_reply ()
{
  return wire_of (hx (0xC9 /*@r04*/), "0000", "0000", "");
}

std::string
serial_request ()
{
  return wire_of (hx (0xEA /*@r04*/), "0000", "0000", "");
}

std::string
serial_reply ()
{
  return wire_of (hx (0xEA /*@r04*/), "0000", "0000", "3C1F");
}
} // namespace

TEST (LumexProtocolDeviceKTest,
      GivenTheTable_WhenTheIndexIsBuilt_ThenThereIsNoDuplicateKey)
{
  index::protocol_index_t const &built = device_index ();
  EXPECT_STREQ (built.protocol_name (), "device_k");
  EXPECT_STREQ (built.base_name (), "");
  EXPECT_EQ (built.rows ().size (), 6u);
  EXPECT_TRUE (index::keys_unique<device>::value);
  EXPECT_EQ (built.key_width (), 1u);
  EXPECT_NE (built.find_integer (0xAC /*@r04*/), nullptr);
  EXPECT_EQ (built.find_integer (0x09), nullptr);
  EXPECT_NE (built.find_name ("query_ea"), nullptr);
}

TEST (
    LumexProtocolDeviceKTest,
    GivenTheReferenceFrames_WhenTheirChecksAreRecomputed_ThenTheLengthAndTheXorAgree)
{
  std::string const wires[] = {
    stop_request (),  stop_reply (),  state_request (),  state_reply (),
    clean_request (), clean_reply (), inject_request (), inject_reply (),
    zone_request (),  zone_reply (),  serial_request (), serial_reply ()
  };
  for (std::size_t i = 0; i < sizeof wires / sizeof wires[0]; ++i)
    {
      SCOPED_TRACE (i);
      expect_wire_frame (wires[i]);
    }
  // A frame made from its parts has the same length field and check.
  EXPECT_EQ (made_body (hx (0x35 /*@r04*/), "0000", "0000", ""),
             body_of (stop_request ()));
  EXPECT_EQ (made_body (hx (0xF4 /*@r04*/), "0000", "0000", "020064"),
             body_of (clean_request ()));
  EXPECT_EQ (xor_text ("AB"), "03");
  EXPECT_EQ (xor_text ("A"), "41");
  EXPECT_NE (xor_text ("AB"), xor_text ("AC"));
}

TEST (LumexProtocolDeviceKTest,
      GivenTheStopCommand_WhenSent_ThenTheHeaderIsTheOnlyField)
{
  expect_request_round_trip<device::stop_35_t> (stop_request ());
  expect_reply_round_trip<device::stop_35_t> (stop_reply ());
  EXPECT_EQ (tx (stop_request ()),
             "TX device_k.stop_35 (Stop) head={error=0 state=0 length=16}");
  EXPECT_EQ (rx (stop_reply ()),
             "RX device_k.stop_35 (Stop) error=0 state=3 length=16");
  layout::bytes_t out;
  ASSERT_TRUE ((layout::encode_request<device, device::stop_35_t> (
                    out, field::value_of<tables::head_t>::type ())
                    .has_value ()));
  EXPECT_EQ (std::string (out.begin (), out.end ()), "000000000000");
}

TEST (
    LumexProtocolDeviceKTest,
    GivenTheStateQuery_WhenTheReplyComes_ThenEveryFieldHasItsWidthAndItsMeaning)
{
  expect_request_round_trip<device::report_ac_t> (state_request ());
  expect_reply_round_trip<device::report_ac_t> (state_reply ());
  EXPECT_EQ (tx (state_request ()), "TX device_k.report_ac (Report the state) "
                                    "head={error=0 state=0 length=16}");
  EXPECT_EQ (rx (state_reply ()),
             "RX device_k.report_ac (Report the state) "
             "head={error=0 state=3 length=83} total=301 fault=0 step=\""
             "7A" /*@r11*/ "\" last_step=\""
             "56" /*@r11*/
             "\" method=1 positions=[5, 6] valve=v_a slots=[2, 3, "
             "255] cycles_left=3 lines=[1, 2, 3, 4] flags=[10, 20] "
             "volume=10000 uL mode=1 times=[120 s, 60 s] zone=t_ad "
             "zone_error={high=1 low=2} temperatures=[22 degC, 23 degC]");
}

TEST (
    LumexProtocolDeviceKTest,
    GivenTheTextStates_WhenDecoded_ThenTheyAreTextsAndTheEnumerationsAreStrict)
{
  std::string const body = body_of (state_reply ());
  layout::bytes_t const frame = bytes_of (body);
  layout::decode_result_t<
      layout::reply_value_of<device::report_ac_t>::type> const decoded
      = layout::decode_reply<device, device::report_ac_t> (payload_of (frame));
  ASSERT_TRUE (decoded.has_value ());
  EXPECT_EQ (field::get<3> (*decoded).view (),
             field::text_view ("7A" /*@r11*/));
  EXPECT_EQ (field::get<4> (*decoded).view (),
             field::text_view ("56" /*@r11*/));
  EXPECT_EQ (field::get<7> (*decoded), tables::valve_state::v_a);
  // The valve character that is no state of the table, the zone state that is
  // none.
  std::string other_valve = body;
  other_valve[34] = '5';
  EXPECT_TRUE ((reply_error<device::report_ac_t> (other_valve)
                == protocol_errc::invalid_value));
  std::string other_zone = body;
  other_zone.replace (body.find (hx (0xAD /*@r04*/) + "1200DC"), 2, "AE");
  EXPECT_TRUE ((reply_error<device::report_ac_t> (other_zone)
                == protocol_errc::invalid_value));
  // The nibbles of the error byte of the zone.
  EXPECT_EQ (field::get<0> (field::get<16> (*decoded).parts), 1u);
  EXPECT_EQ (field::get<1> (field::get<16> (*decoded).parts), 2u);
  EXPECT_EQ (field::get<16> (*decoded).raw, 0x12u);
}

TEST (LumexProtocolDeviceKTest,
      GivenTheCleanAndThePosition_WhenSent_ThenTheFieldsFollowTheHeader)
{
  expect_request_round_trip<device::start_f4_t> (clean_request ());
  expect_reply_round_trip<device::start_f4_t> (clean_reply ());
  EXPECT_EQ (tx (clean_request ()),
             "TX device_k.start_f4 (Run the cleaning cycle) "
             "head={error=0 state=0 length=22} liquid=2 volume=100");
  expect_request_round_trip<device::start_64_t> (inject_request ());
  expect_reply_round_trip<device::start_64_t> (inject_reply ());
  EXPECT_EQ (tx (inject_request ()),
             "TX device_k.start_64 (Run the load cycle) "
             "head={error=0 state=0 length=34} method=1 position=5 "
             "slots=[255, 255, 255] before=1 after=1 sync="
                 + dec (0x2C /*@r04*/) + " zone=0");
  EXPECT_EQ (rx (inject_reply ()), "RX device_k.start_64 (Run the load cycle) "
                                   "error=0 state=3 length=16");
}

TEST (LumexProtocolDeviceKTest,
      GivenTheZoneSettings_WhenWritten_ThenTheTemperatureIsTenthsOfADegree)
{
  expect_request_round_trip<device::param_set_c9_t> (zone_request ());
  expect_reply_round_trip<device::param_set_c9_t> (zone_reply ());
  EXPECT_EQ (tx (zone_request ()),
             "TX device_k.param_set_c9 (Write the zone settings) "
             "head={error=0 state=0 length=52} block=[11, 7, 5, 9, 4, 1, 14, "
             "3, 8, 13, 2, 6, 12, 4] temperature=22 degC reserved=0");
  layout::bytes_t const frame = frame_of (zone_request ());
  layout::decode_result_t<
      layout::request_of<device::param_set_c9_t>::type> const decoded
      = layout::decode_request<device, device::param_set_c9_t> (
          payload_of (frame));
  ASSERT_TRUE (decoded.has_value ());
  EXPECT_EQ (field::get<2> (*decoded).raw, 220u);
}

TEST (LumexProtocolDeviceKTest,
      GivenTheSerialQuery_WhenTheReplyComes_ThenTheNumberFollowsTheHeader)
{
  expect_request_round_trip<device::query_ea_t> (serial_request ());
  expect_reply_round_trip<device::query_ea_t> (serial_reply ());
  EXPECT_EQ (rx (serial_reply ()),
             "RX device_k.query_ea (Query the serial number) "
             "head={error=0 state=0 length=20} serial=15391");
}

TEST (LumexProtocolDeviceKTest,
      GivenAReplyWithAnError_WhenDescribed_ThenTheHeaderCarriesTheErrorWord)
{
  std::string const wire = wire_of (hx (0xAC /*@r04*/), "0012", "0003", "");
  EXPECT_EQ (tx (wire), "TX device_k.report_ac (Report the state) "
                        "head={error=18 state=3 length=16}");
}

TEST (
    LumexProtocolDeviceKTest,
    GivenDamagedBodies_WhenDecoded_ThenTheErrorsAreTruncatedTrailingOrInvalid)
{
  std::string const stop = body_of (stop_reply ());
  EXPECT_TRUE (
      (reply_error<device::stop_35_t> (stop.substr (0, stop.size () - 2))
       == protocol_errc::truncated));
  EXPECT_TRUE ((reply_error<device::stop_35_t> (stop + "00")
                == protocol_errc::trailing_bytes));
  std::string not_hex = stop;
  not_hex[5] = 'G';
  EXPECT_TRUE ((reply_error<device::stop_35_t> (not_hex)
                == protocol_errc::invalid_value));
  EXPECT_EQ (device_index ().describe (view_of (bytes_of ("77")),
                                       index::frame_direction::tx),
             "TX unknown key=0x77 len=2: 37 37");
}
