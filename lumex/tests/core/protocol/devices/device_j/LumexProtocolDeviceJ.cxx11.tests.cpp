// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Device J (lumex/tests/core/protocol/devices/device_j): the same logical
// commands in two layers. j_text carries every byte as two hexadecimal
// digits (the frame around the digits has a start character, an XOR of the
// characters and a carriage return), j_hid carries the bytes in a report
// (length, command, bytes). Every command is checked in both layers on the
// same reference vectors: the digits are the hexadecimal text of the bytes,
// the checksum covers the characters, and the log line differs only by the
// name of the layer.

#include <cstdint>
#include <initializer_list>
#include <string>
#include <system_error>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/devices/device_j/LumexProtocolDeviceJTable.hpp"

namespace
{
namespace tables = lumex_tests::tables_j;
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace index = lumex::core::protocol::index;
using lumex::core::protocol::error::protocol_errc;
using tables::bytes;
using tables::bytes_of;
using tables::hex_of;
using tables::view_of;
using tables::xor_text;

using text = tables::j_text;
using hid = tables::j_hid;

template <typename P>
index::protocol_index_t const &
index_of ()
{
  index::index_result_t const &result = index::protocol_index<P> ();
  EXPECT_TRUE (result.has_value ());
  return *result;
}

// The error of decoding bytes as the reply of a row in a layer (no error
// when they decode).
template <typename P, typename Row>
std::error_code
reply_error (layout::bytes_t const &data)
{
  layout::decode_result_t<typename layout::reply_value_of<Row>::type> const
      decoded
      = layout::decode_reply<P, Row> (view_of (data));
  return decoded ? std::error_code () : decoded.error ();
}

// The settings of the reference vectors (35 bytes).
layout::bytes_t
settings_bytes ()
{
  return bytes ({ 0x31, 0x31, 0x1E, 0x03, 0x50, 0x50, 0x28, 0x00, 0x00,
                  0x00, 0x64, 0x80, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00,
                  0x0A, 0x80, 0x00, 0x00, 0x2D, 0x60, 0x73, 0x05, 0x0A,
                  0x05, 0x0A, 0x14, 0x14, 0x0A, 0x05, 0x0A, 0x05 });
}

layout::bytes_t
with_status (int status, layout::bytes_t const &tail)
{
  layout::bytes_t out = bytes ({ status });
  out.insert (out.end (), tail.begin (), tail.end ());
  return out;
}

// The frame the text link carries around the digits: a start character, the
// digits, the XOR of their characters as two digits, a carriage return.
std::string
wire_of (std::string const &digits)
{
  return "%" + digits + xor_text (digits) + "\r";
}

// The frame has the digits in the middle and a check that covers them.
void
expect_wire_frame (std::string const &digits)
{
  std::string const wire = wire_of (digits);
  ASSERT_EQ (wire.size (), digits.size () + 4u);
  EXPECT_EQ (wire[0], '%');
  EXPECT_EQ (wire[wire.size () - 1], '\r');
  EXPECT_EQ (wire.substr (1, digits.size ()), digits);
  EXPECT_EQ (wire.substr (wire.size () - 3, 2), xor_text (digits));
}

// The frame of a report: the length of what follows it, the command, the
// bytes.
layout::bytes_t
report_frame (std::uint8_t key, layout::bytes_t const &payload)
{
  layout::bytes_t out
      = bytes ({ static_cast<int> (payload.size ()) + 1, key });
  out.insert (out.end (), payload.begin (), payload.end ());
  return out;
}

// The frame of the text encoding without the start character, the checksum
// and the carriage return: the command, then the digits.
layout::bytes_t
digits_frame (std::uint8_t key, layout::bytes_t const &payload)
{
  return bytes_of (hex_of (bytes ({ key })) + hex_of (payload));
}

template <typename P>
std::string
described (layout::bytes_t const &frame, index::frame_direction direction)
{
  return index_of<P> ().describe (view_of (frame), direction);
}

// The line of a layer with the name of the other layer.
std::string
renamed (std::string line)
{
  std::string::size_type const at = line.find ("j_hid");
  if (at != std::string::npos)
    line.replace (at, 5, "j_text");
  return line;
}

// A request of `Row` in both layers: the payload bytes (after the command),
// the text frame (wire) and the log lines.
template <typename Row>
void
expect_request_in_both_layers (std::uint8_t key,
                               layout::bytes_t const &payload,
                               std::string const &line)
{
  expect_wire_frame (hex_of (bytes ({ key })) + hex_of (payload));
  layout::decode_result_t<typename layout::request_of<Row>::type> const
      binary_record
      = layout::decode_request<hid, Row> (view_of (payload));
  ASSERT_TRUE (binary_record.has_value ());
  layout::bytes_t digits;
  ASSERT_TRUE (
      (layout::encode_request_record<text, Row> (digits, *binary_record)
           .has_value ()));
  EXPECT_EQ (std::string (digits.begin (), digits.end ()), hex_of (payload));
  layout::decode_result_t<typename layout::request_of<Row>::type> const
      text_record
      = layout::decode_request<text, Row> (view_of (digits));
  ASSERT_TRUE (text_record.has_value ());
  layout::bytes_t again;
  ASSERT_TRUE ((layout::encode_request_record<hid, Row> (again, *text_record)
                    .has_value ()));
  EXPECT_EQ (again, payload);
  layout::bytes_t const report = report_frame (key, payload);
  EXPECT_EQ (described<hid> (report, index::frame_direction::tx), line);
  EXPECT_EQ (described<text> (digits_frame (key, payload),
                              index::frame_direction::tx),
             renamed (line));
  EXPECT_EQ (index_of<hid> ().match (view_of (report)).payload_offset, 2u);
  EXPECT_EQ (index_of<text> ()
                 .match (view_of (digits_frame (key, payload)))
                 .payload_offset,
             2u);
}

template <typename Row>
void
expect_reply_in_both_layers (std::uint8_t key, layout::bytes_t const &payload,
                             std::string const &line)
{
  expect_wire_frame (hex_of (bytes ({ key })) + hex_of (payload));
  layout::decode_result_t<typename layout::reply_value_of<Row>::type> const
      binary_value
      = layout::decode_reply<hid, Row> (view_of (payload));
  ASSERT_TRUE (binary_value.has_value ());
  layout::bytes_t digits;
  ASSERT_TRUE (
      (layout::encode_reply<text, Row> (digits, *binary_value).has_value ()));
  EXPECT_EQ (std::string (digits.begin (), digits.end ()), hex_of (payload));
  layout::decode_result_t<typename layout::reply_value_of<Row>::type> const
      text_value
      = layout::decode_reply<text, Row> (view_of (digits));
  ASSERT_TRUE (text_value.has_value ());
  layout::bytes_t again;
  ASSERT_TRUE (
      (layout::encode_reply<hid, Row> (again, *text_value).has_value ()));
  EXPECT_EQ (again, payload);
  EXPECT_EQ (
      described<hid> (report_frame (key, payload), index::frame_direction::rx),
      line);
  EXPECT_EQ (described<text> (digits_frame (key, payload),
                              index::frame_direction::rx),
             renamed (line));
}

std::string const kStatusOk = "status={result=r_0 state=s_0}";
} // namespace

TEST (
    LumexProtocolDeviceJTest,
    GivenTheTable_WhenTheIndicesAreBuilt_ThenBothLayersHaveTheSameRowsAndNoDuplicateKey)
{
  index::protocol_index_t const &text_index = index_of<text> ();
  index::protocol_index_t const &hid_index = index_of<hid> ();
  EXPECT_STREQ (text_index.protocol_name (), "j_text");
  EXPECT_STREQ (text_index.base_name (), "");
  EXPECT_STREQ (hid_index.protocol_name (), "j_hid");
  EXPECT_STREQ (hid_index.base_name (), "j_text");
  EXPECT_EQ (text_index.rows ().size (), 7u);
  EXPECT_EQ (hid_index.rows ().size (), 7u);
  EXPECT_TRUE (index::keys_unique<text>::value);
  EXPECT_TRUE (index::keys_unique<hid>::value);
  EXPECT_EQ (text_index.key_width (), 1u);
  EXPECT_EQ (hid_index.key_width (), 1u);
  for (std::size_t i = 0; i < text_index.rows ().size (); ++i)
    {
      EXPECT_STREQ (text_index.rows ()[i].entry->identifier,
                    hid_index.rows ()[i].entry->identifier);
      EXPECT_EQ (text_index.rows ()[i].key.integer,
                 hid_index.rows ()[i].key.integer);
    }
  EXPECT_TRUE ((std::is_same<text::stop_35_t, hid::stop_35_t>::value));
  EXPECT_FALSE ((std::is_same<layout::layout_of<text>::type,
                              layout::layout_of<hid>::type>::value));
}

TEST (LumexProtocolDeviceJTest,
      GivenTheVersionQuery_WhenSentInBothLayers_ThenTheBytesAreTheDigits)
{
  expect_request_in_both_layers<text::query_3f_t> (
      0x3F /*@r04*/, layout::bytes_t (),
      "TX j_hid.query_3f (Query the version)");
  expect_reply_in_both_layers<text::query_3f_t> (
      0x3F /*@r04*/, bytes ({ 0x00, 0x01, 0x2C }),
      "RX j_hid.query_3f (Query the version) " + kStatusOk
          + " version={major=1 minor=44}");
}

TEST (
    LumexProtocolDeviceJTest,
    GivenTheStateQuery_WhenTheReplyHasTheStatusOnly_ThenTheNibblesAreTheResultAndTheState)
{
  expect_request_in_both_layers<text::report_ac_t> (
      0xAC /*@r04*/, layout::bytes_t (),
      "TX j_hid.report_ac (Report the state)");
  expect_reply_in_both_layers<text::report_ac_t> (
      0xAC /*@r04*/, bytes ({ 0x03 }),
      "RX j_hid.report_ac (Report the state) "
      "status={result=r_0 state=s_3}");
}

TEST (
    LumexProtocolDeviceJTest,
    GivenTheSettingsRead_WhenTheReplyComes_ThenTheSizesMatchTheReportOf37Bytes)
{
  layout::bytes_t const payload = with_status (0x00, settings_bytes ());
  layout::bytes_t const report = report_frame (0x8D /*@r04*/, payload);
  EXPECT_EQ (report[0], 37);
  EXPECT_EQ (report.size (), 38u);
  EXPECT_EQ (settings_bytes ().size (), 35u);
  expect_request_in_both_layers<text::param_get_8d_t> (
      0x8D /*@r04*/, layout::bytes_t (),
      "TX j_hid.param_get_8d (Read the settings)");
  expect_reply_in_both_layers<text::param_get_8d_t> (
      0x8D /*@r04*/, payload,
      "RX j_hid.param_get_8d (Read the settings) "
      "status={result=r_0 state=s_0} settings={small=[49, 49, 30, 3, 80, 80, "
      "40] corrections=[100, -50, 10, -45] serial=24691 volumes=[500 uL, "
      "1000 uL, 500 uL, 1000 uL, 2000 uL, 2000 uL, 1000 uL, 500 uL, "
      "1000 uL, 500 uL]}");
  // The hexadecimal text of a signed-magnitude number keeps its sign bit.
  EXPECT_NE (hex_of (settings_bytes ()).find ("80000032"), std::string::npos);
}

TEST (LumexProtocolDeviceJTest,
      GivenTheSettingsWrite_WhenTheReplyCarriesAnError_ThenTheStatusNamesIt)
{
  expect_request_in_both_layers<text::param_set_c9_t> (
      0xC9 /*@r04*/, settings_bytes (),
      "TX j_hid.param_set_c9 (Write the settings) settings={small=[49, 49, "
      "30, 3, 80, 80, 40] corrections=[100, -50, 10, -45] serial=24691 "
      "volumes=[500 uL, 1000 uL, 500 uL, 1000 uL, 2000 uL, 2000 uL, 1000 uL, "
      "500 uL, 1000 uL, 500 uL]}");
  expect_reply_in_both_layers<text::param_set_c9_t> (
      0xC9 /*@r04*/, bytes ({ 0x00 }),
      "RX j_hid.param_set_c9 (Write the settings) "
      "status={result=r_0 state=s_0}");
  expect_reply_in_both_layers<text::param_set_c9_t> (
      0xC9 /*@r04*/, bytes ({ 0x31 }),
      "RX j_hid.param_set_c9 (Write the settings) "
      "status={result=r_3 state=s_1}");
}

TEST (LumexProtocolDeviceJTest,
      GivenTheClean_WhenTheVolumesAreWritten_ThenTheyAreBigEndianWords)
{
  layout::bytes_t out;
  ASSERT_TRUE ((layout::encode_request<hid, hid::start_f4_t> (out, 100, 200)
                    .has_value ()));
  EXPECT_EQ (out, bytes ({ 0x00, 0x64, 0x00, 0xC8 }));
  out.clear ();
  ASSERT_TRUE ((layout::encode_request<text, text::start_f4_t> (out, 100, 200)
                    .has_value ()));
  EXPECT_EQ (std::string (out.begin (), out.end ()), "006400C8");
  expect_request_in_both_layers<text::start_f4_t> (
      0xF4 /*@r04*/, bytes ({ 0x00, 0x64, 0x00, 0xC8 }),
      "TX j_hid.start_f4 (Run the cleaning cycle) first=100 second=200");
  expect_reply_in_both_layers<text::start_f4_t> (
      0xF4 /*@r04*/, bytes ({ 0x00 }),
      "RX j_hid.start_f4 (Run the cleaning cycle) "
      "status={result=r_0 state=s_0}");
}

TEST (LumexProtocolDeviceJTest,
      GivenThePositionAndStopCommands_WhenSent_ThenTheyAreOneByteAndNoByte)
{
  expect_request_in_both_layers<text::start_64_t> (
      0x64 /*@r04*/, bytes ({ 0x2A }),
      "TX j_hid.start_64 (Run the load cycle) position=42");
  expect_reply_in_both_layers<text::start_64_t> (
      0x64 /*@r04*/, bytes ({ 0x00 }),
      "RX j_hid.start_64 (Run the load cycle) "
      "status={result=r_0 state=s_0}");
  expect_request_in_both_layers<text::stop_35_t> (
      0x35 /*@r04*/, layout::bytes_t (), "TX j_hid.stop_35 (Stop)");
  expect_reply_in_both_layers<text::stop_35_t> (
      0x35 /*@r04*/, bytes ({ 0x00 }),
      "RX j_hid.stop_35 (Stop) status={result=r_0 state=s_0}");
}

TEST (LumexProtocolDeviceJTest,
      GivenAWrongChecksum_WhenTheCharactersAreCounted_ThenTheTextFrameDiffers)
{
  // The check is over the characters of the digits, not over the bytes they
  // stand for: the same bytes give another check.
  EXPECT_EQ (xor_text ("AB"), "03");
  EXPECT_EQ (xor_text ("A"), "41");
  EXPECT_NE (xor_text ("AB"), xor_text ("AC"));
  EXPECT_NE (xor_text ("AB"), xor_text (std::string ("\xAB", 1)));
  EXPECT_EQ (xor_text (""), "00");
}

TEST (
    LumexProtocolDeviceJTest,
    GivenAStatusByteWithAValueOutsideTheEnumeration_WhenDescribed_ThenTheNumberIsShownInBothLayers)
{
  // A nibble part is not a strict enumeration: the value decodes and the log
  // shows it as a number.
  layout::bytes_t const odd = bytes ({ 0x73 });
  EXPECT_FALSE ((reply_error<hid, hid::report_ac_t> (odd)));
  EXPECT_FALSE ((reply_error<text, text::report_ac_t> (bytes_of ("73"))));
  EXPECT_EQ (described<hid> (report_frame (0xAC /*@r04*/, odd),
                             index::frame_direction::rx),
             "RX j_hid.report_ac (Report the state) "
             "status={result=7 state=s_3}");
}

TEST (
    LumexProtocolDeviceJTest,
    GivenTruncatedAndLongFrames_WhenDecoded_ThenTheErrorsAreTheSameInBothLayers)
{
  layout::bytes_t const too_short = bytes ({ 0x00, 0x01 });
  EXPECT_TRUE ((reply_error<hid, hid::query_3f_t> (too_short)
                == protocol_errc::truncated));
  layout::bytes_t const short_digits = bytes_of ("0001");
  EXPECT_TRUE ((reply_error<text, text::query_3f_t> (short_digits)
                == protocol_errc::truncated));
  layout::bytes_t const long_bytes = bytes ({ 0x00, 0x01, 0x2C, 0x00 });
  EXPECT_TRUE ((reply_error<hid, hid::query_3f_t> (long_bytes)
                == protocol_errc::trailing_bytes));
  layout::bytes_t const long_digits = bytes_of ("00012C00");
  EXPECT_TRUE ((reply_error<text, text::query_3f_t> (long_digits)
                == protocol_errc::trailing_bytes));
  layout::bytes_t const not_digits = bytes_of ("00012Z");
  EXPECT_TRUE ((reply_error<text, text::query_3f_t> (not_digits)
                == protocol_errc::invalid_value));
}

TEST (LumexProtocolDeviceJTest,
      GivenAFrameOfAnotherKey_WhenDescribed_ThenItIsUnknownInBothLayers)
{
  EXPECT_EQ (
      described<hid> (bytes ({ 0x01, 0x77 }), index::frame_direction::tx),
      "TX unknown key=0x77 len=2: 01 77");
  EXPECT_EQ (described<text> (bytes_of ("77"), index::frame_direction::tx),
             "TX unknown key=0x77 len=2: 37 37");
}
