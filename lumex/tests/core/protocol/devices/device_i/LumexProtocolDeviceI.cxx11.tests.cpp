// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Device I (lumex/tests/core/protocol/devices/device_i): every command of the
// table on reference frames (no address, the key is the first byte), the
// scaled counts and the floating-point values, the parameter table, the
// protocol before the switch (i_alt) and the two out-of-band sequences, the
// describe lines of the log and the index.

#include <cstdint>
#include <initializer_list>
#include <string>
#include <system_error>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/devices/device_i/LumexProtocolDeviceITable.hpp"

namespace
{
namespace tables = lumex_tests::tables_i;
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace registry = lumex::core::protocol::registry;
namespace index = lumex::core::protocol::index;
using lumex::core::protocol::error::protocol_errc;
using tables::bytes;
using tables::payload_of;
using tables::view_of;

using device = tables::device_i;
using alt = tables::alt_i;

index::protocol_index_t const &
device_index ()
{
  index::index_result_t const &result
      = index::protocol_index<device, tables::work> ();
  EXPECT_TRUE (result.has_value ());
  return *result;
}

index::protocol_index_t const &
alt_index ()
{
  index::index_result_t const &result = index::protocol_index<alt> ();
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

layout::bytes_t
payload_bytes (layout::bytes_t const &frame)
{
  return layout::bytes_t (frame.begin () + 1, frame.end ());
}

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

// A frame: the key, the body.
layout::bytes_t
frame_of (std::uint8_t key, std::initializer_list<int> body)
{
  layout::bytes_t out = bytes ({ key });
  layout::bytes_t const tail = bytes (body);
  out.insert (out.end (), tail.begin (), tail.end ());
  return out;
}
} // namespace

TEST (
    LumexProtocolDeviceITest,
    GivenTheTables_WhenTheIndicesAreBuilt_ThenThereIsNoDuplicateKeyInEitherProtocol)
{
  index::protocol_index_t const &built = device_index ();
  EXPECT_STREQ (built.protocol_name (), "device_i");
  EXPECT_STREQ (built.base_name (), "");
  EXPECT_EQ (built.rows ().size (), 15u);
  EXPECT_EQ (built.registries ().size (), 1u);
  EXPECT_TRUE (index::keys_unique<device>::value);
  EXPECT_EQ (built.find_integer (0x00 /*@r04*/)->entry->identifier,
             std::string ("param_set_00"));
  EXPECT_NE (built.find_name ("event_27"), nullptr);
  EXPECT_EQ (built.find_integer (0x09), nullptr);
  EXPECT_STREQ (alt_index ().protocol_name (), "alt_i");
  EXPECT_EQ (alt_index ().rows ().size (), 1u);
  // The same key is two commands in two protocols.
  EXPECT_NE (alt_index ().find_integer (0xF7 /*@r37*/), nullptr);
  EXPECT_EQ (built.find_integer (0xF7 /*@r37*/), nullptr);
}

TEST (LumexProtocolDeviceITest,
      GivenTheIdentityQuery_WhenEncodedAndDecoded_ThenTheKeyIsTheFirstByte)
{
  layout::bytes_t out;
  ASSERT_TRUE (
      (layout::encode_request<device, device::query_ad_t> (out).has_value ()));
  EXPECT_TRUE (out.empty ());
  layout::bytes_t const reply
      = frame_of (0xAD /*@r04*/, { 0x2D, 0xA1, 0xF7, 0xB3, 0x00, 0x40, 0x1F,
                                   0x00, 0x00, 0x04, 0x0B });
  expect_reply_round_trip<device::query_ad_t> (reply);
  EXPECT_EQ (tx (frame_of (0xAD /*@r04*/, {})),
             "TX device_i.query_ad (Query the identity record)");
  EXPECT_EQ (rx (reply), "RX device_i.query_ad (Query the identity record) "
                         "type=45 serial=11794337 resource=8000 ms sw=4 "
                         "sub=11");
  EXPECT_EQ (
      dsl::get_option<dsl::kind::timeout> (device::query_ad_t::options ())
          .milliseconds,
      1200u);
  EXPECT_EQ (
      dsl::get_option<dsl::kind::retries> (device::query_ad_t::options ())
          .count,
      2u);
}

TEST (
    LumexProtocolDeviceITest,
    GivenTheSameKeyInTwoDevices_WhenDescribed_ThenItNamesTheRowOfThisProtocol)
{
  // The key 0x65 is a mode setting in devices E, F and G.
  layout::bytes_t const counters
      = frame_of (0x65 /*@r04*/, { 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x00,
                                   0x01, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00,
                                   0x08, 0x00, 0x09, 0x00, 0xFF, 0xFF });
  expect_reply_round_trip<device::report_65_t> (counters);
  EXPECT_EQ (rx (counters), "RX device_i.report_65 (Report the link counters) "
                            "counters=[1, 2, 3, 256, 5, 6, 7, 8, 9, 65535]");
  EXPECT_TRUE (reply_error<device::report_65_t> (
                   frame_of (0x65 /*@r04*/, { 0x01, 0x00 }))
               == protocol_errc::truncated);
}

TEST (LumexProtocolDeviceITest,
      GivenARunCommand_WhenEncodedAndDecoded_ThenTheStateIsAStrictEnumeration)
{
  layout::bytes_t out;
  ASSERT_TRUE ((layout::encode_request<device, device::start_3f_t> (
                    out, tables::run_state::c_ad)
                    .has_value ()));
  EXPECT_EQ (out, bytes ({ 0xAD /*@r04*/ }));
  layout::bytes_t const states[]
      = { frame_of (0x3F /*@r04*/, { 0x15 /*@r04*/ }),
          frame_of (0x3F /*@r04*/, { 0xAD /*@r04*/ }),
          frame_of (0x3F /*@r04*/, { 0x65 /*@r04*/ }),
          frame_of (0x3F /*@r04*/, { 0x3F /*@r04*/ }) };
  for (layout::bytes_t const &frame : states)
    expect_request_round_trip<device, device::start_3f_t> (frame);
  EXPECT_EQ (tx (states[2]), "TX device_i.start_3f (Start or stop a run) "
                             "state=c_65");
  EXPECT_TRUE (
      request_error<device::start_3f_t> (frame_of (0x3F /*@r04*/, { 0x77 }))
      == protocol_errc::invalid_value);
}

TEST (LumexProtocolDeviceITest,
      GivenOnePulse_WhenTheReplyComes_ThenTheThreeFloatsAreMicrovolts)
{
  layout::bytes_t const reply
      = frame_of (0x72 /*@r04*/, { 0x00, 0x00, 0xC0, 0x3F, 0x00, 0x40, 0x7A,
                                   0x43, 0x00, 0x00, 0x70, 0xC0 });
  expect_reply_round_trip<device::event_72_t> (reply);
  EXPECT_EQ (rx (reply), "RX device_i.event_72 (Make one pulse) ch_a=1.5 uV "
                         "ch_b=250.25 uV ch_c=-3.75 uV");
  EXPECT_EQ (tx (frame_of (0x72 /*@r04*/, {})),
             "TX device_i.event_72 (Make one pulse)");
}

TEST (
    LumexProtocolDeviceITest,
    GivenAMeasurement_WhenTheCountsAreRead_ThenTheStepOfTheConverterMakesVolts)
{
  layout::bytes_t const reply = frame_of (
      0x27 /*@r04*/, { 0x40, 0xE2, 0x01, 0x00, 0x80, 0x96, 0x98, 0x00, 0x00,
                       0x09, 0x3D, 0x00, 0x20, 0x4E, 0x00, 0x00 });
  expect_reply_round_trip<device::event_27_t> (reply);
  EXPECT_EQ (rx (reply),
             "RX device_i.event_27 (Report a measurement) time=123456 ms "
             "ch_a=2.551021 V ch_b=1.020408 V ch_c=0.005102 V");
  layout::decode_result_t<
      layout::reply_value_of<device::event_27_t>::type> const decoded
      = layout::decode_reply<device, device::event_27_t> (payload_of (reply));
  ASSERT_TRUE (decoded.has_value ());
  EXPECT_EQ (field::get<1> (*decoded).raw, 10000000u);
  EXPECT_NEAR (field::get<1> (*decoded).value (), 2.551021, 1e-12);
}

TEST (LumexProtocolDeviceITest,
      GivenTheStatusCommands_WhenEncodedAndDecoded_ThenEachHasItsOwnKey)
{
  layout::bytes_t out;
  ASSERT_TRUE ((
      layout::encode_request<device, device::set_38_t> (out, 1).has_value ()));
  ASSERT_TRUE ((
      layout::encode_request<device, device::set_b2_t> (out, 0).has_value ()));
  EXPECT_EQ (out, bytes ({ 0x01, 0x00 }));
  expect_request_round_trip<device, device::set_38_t> (
      frame_of (0x38 /*@r04*/, { 1 }));
  expect_request_round_trip<device, device::set_b2_t> (
      frame_of (0xB2 /*@r04*/, { 0 }));
  EXPECT_EQ (tx (frame_of (0x38 /*@r04*/, { 1 })),
             "TX device_i.set_38 (Enable the status messages) enable=1");
  EXPECT_EQ (tx (frame_of (0xB2 /*@r04*/, { 0 })),
             "TX device_i.set_b2 (Lock the keyboard) lock=0");
  layout::bytes_t const status = frame_of (0x4B /*@r04*/, { 0x34, 0x12 });
  expect_reply_round_trip<device::query_4b_t> (status);
  EXPECT_EQ (rx (status),
             "RX device_i.query_4b (Query the status) reply=4660");
  expect_reply_round_trip<device::query_c7_t> (
      frame_of (0xC7 /*@r04*/, { 1 }));
  EXPECT_EQ (rx (frame_of (0xC7 /*@r04*/, { 1 })),
             "RX device_i.query_c7 (Query the keyboard) reply=1");
  expect_reply_round_trip<device::query_8b_t> (
      frame_of (0x8B /*@r04*/, { 2 }));
  EXPECT_EQ (rx (frame_of (0x8B /*@r04*/, { 2 })),
             "RX device_i.query_8b (Query the state of the device) reply=2");
  expect_reply_round_trip<device::debug_99_t> (
      frame_of (0x99 /*@r04*/, { 0x78, 0x56, 0x34, 0x12 }));
  EXPECT_EQ (rx (frame_of (0x99 /*@r04*/, { 0x78, 0x56, 0x34, 0x12 })),
             "RX device_i.debug_99 (Report the debug word) reply=305419896");
}

TEST (LumexProtocolDeviceITest,
      GivenAnErrorMessage_WhenItHasOneOrTwoValues_ThenTheSecondIsATail)
{
  layout::bytes_t const shorter = frame_of (0xF8 /*@r04*/, { 0x05 });
  layout::bytes_t const longer
      = frame_of (0xF8 /*@r04*/, { 0x05, 0x34, 0x12 });
  expect_reply_round_trip<device::error_f8_t> (shorter);
  expect_reply_round_trip<device::error_f8_t> (longer);
  EXPECT_EQ (rx (shorter),
             "RX device_i.error_f8 (Report an error) code=5 value=absent");
  EXPECT_EQ (rx (longer),
             "RX device_i.error_f8 (Report an error) code=5 value={4660}");
  EXPECT_TRUE (reply_error<device::error_f8_t> (
                   frame_of (0xF8 /*@r04*/, { 0x05, 0x34 }))
               == protocol_errc::truncated);
}

TEST (
    LumexProtocolDeviceITest,
    GivenAWorkingParameter_WhenWrittenAndRead_ThenTheKeyAndTheValueAreChecked)
{
  layout::bytes_t out;
  ASSERT_TRUE ((layout::encode_request<device, device::param_set_00_t> (
                    out, tables::work::w_ad, 0, 1000.0)
                    .has_value ()));
  EXPECT_EQ (out, bytes ({ 0xAD /*@r04*/, 0x00, 0xE8, 0x03 }));
  layout::bytes_t const write
      = frame_of (0x00 /*@r04*/, { 0xAD /*@r04*/, 0x00, 0xE8, 0x03 });
  expect_request_round_trip<device, device::param_set_00_t> (write);
  EXPECT_EQ (tx (write), "TX device_i.param_set_00 (Write a working "
                         "parameter) parameter=w_ad zero=0 value=1000");
  out.clear ();
  layout::encode_result_t const over
      = layout::encode_request<device, device::param_set_00_t> (
          out, tables::work::w_ad, 0, 1001.0);
  ASSERT_FALSE (over.has_value ());
  EXPECT_TRUE (over.error () == protocol_errc::out_of_limits);
  EXPECT_TRUE (out.empty ());
  layout::bytes_t const request = frame_of (0xBB /*@r04*/, { 0x15 /*@r04*/ });
  layout::bytes_t const reply
      = frame_of (0xBB /*@r04*/, { 0x15 /*@r04*/, 0x00, 0x02, 0x00 });
  expect_request_round_trip<device, device::param_get_bb_t> (request);
  expect_reply_round_trip<device::param_get_bb_t> (reply);
  EXPECT_EQ (tx (request), "TX device_i.param_get_bb (Read a working "
                           "parameter) parameter=w_15");
  EXPECT_EQ (rx (reply), "RX device_i.param_get_bb (Read a working "
                         "parameter) parameter=w_15 zero=0 value=2");
  EXPECT_TRUE (reply_error<device::param_get_bb_t> (
                   frame_of (0xBB /*@r04*/, { 0x16, 0x00, 0x02, 0x00 }))
               == protocol_errc::unknown_case);
  registry::entries_t const entries = tables::work::entries ();
  std::size_t first = 0;
  std::size_t second = 0;
  EXPECT_FALSE (registry::find_duplicate_key (entries, first, second));
  EXPECT_EQ (entries.size (), 2u);
}

TEST (LumexProtocolDeviceITest,
      GivenTheSwitchSequences_WhenSent_ThenTheyAreFramesOfTheTwoProtocols)
{
  layout::bytes_t const in = tables::switch_in ();
  layout::bytes_t const back = tables::switch_back ();
  // The sequence into the main protocol is a frame of i_alt, the one back is
  // a frame of device_i.
  expect_request_round_trip<alt, alt::switch_f7_t> (in);
  expect_request_round_trip<device, device::switch_back_7b_t> (back);
  layout::bytes_t out;
  ASSERT_TRUE ((layout::encode_request<alt, alt::switch_f7_t> (
                    out, 0xF8 /*@r37*/, 0xC9 /*@r37*/, 0xAB /*@r37*/,
                    0x3E /*@r37*/, 0xC0)
                    .has_value ()));
  out.insert (out.begin (), 0xF7 /*@r37*/);
  EXPECT_EQ (out, in);
  EXPECT_EQ (alt_index ().describe (view_of (in), index::frame_direction::tx),
             "TX alt_i.switch_f7 (Switch to the main protocol) m1="
                 + dec (0xF8 /*@r37*/) + " m2=" + dec (0xC9 /*@r37*/)
                 + " m3=" + dec (0xAB /*@r37*/) + " m4=" + dec (0x3E /*@r37*/)
                 + " end=192");
  EXPECT_EQ (tx (back),
             "TX device_i.switch_back_7b (Switch back to the "
             "other protocol) m1="
                 + dec (0xF8 /*@r37*/) + " m2=" + dec (0xC9 /*@r37*/) + " m3="
                 + dec (0xAB /*@r37*/) + " m4=" + dec (0x3E /*@r37*/));
  // They differ in the first byte only, the first has the frame end of the
  // byte-stuffing link after the common part, and no byte of either is a
  // reserved one (end, escape, line feed, carriage return, ack, nak).
  ASSERT_EQ (in.size (), back.size () + 1);
  EXPECT_NE (in[0], back[0]);
  for (std::size_t i = 1; i < back.size (); ++i)
    EXPECT_EQ (in[i], back[i]);
  EXPECT_EQ (in.back (), 0xC0);
  for (std::size_t i = 0; i + 1 < in.size (); ++i)
    {
      EXPECT_NE (in[i], 0xC0);
      EXPECT_NE (in[i], 0xDB);
      EXPECT_NE (in[i], 0x0D);
      EXPECT_NE (in[i], 0x0A);
      EXPECT_NE (in[i], 0x06);
      EXPECT_NE (in[i], 0x15);
    }
  EXPECT_EQ (rx (in), "RX unknown key=0x"
                          + hexdump (layout::bytes_t (1, in[0]))
                          + " len=6: " + hexdump (in));
}
