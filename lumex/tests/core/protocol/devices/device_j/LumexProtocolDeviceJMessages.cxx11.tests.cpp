// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Device J (lumex/tests/core/protocol/devices/device_j): every row of every
// layer as a whole message. The sample request and reply of each row
// encode with the header and the key, the run-time index finds the row the
// header wrote with its fields right after the header, and dispatch hands
// the message back to the same row. A message is the payload of the frame
// codec: the report codec carries the length byte of j_hid, so the message
// of a row in j_hid is the bytes whose hexadecimal digits are its message in
// j_text.

#include <cstddef>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/devices/LumexProtocolDevicesMessages.hpp"
#include "lumex/tests/core/protocol/devices/device_j/LumexProtocolDeviceJTable.hpp"

namespace
{
namespace dev = lumex_tests::devices;
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace tables = lumex_tests::tables_j;

// The request and the reply of Row in j_hid are the bytes of its digits in
// j_text.
template <typename Row>
void
expect_same_message_in_both_layers ()
{
  layout::bytes_t binary;
  layout::bytes_t digits;
  ASSERT_TRUE ((layout::encode_message_record<tables::j_hid, Row> (
                    binary, field::sample_value<typename Row::fields_type> ())
                    .has_value ()));
  ASSERT_TRUE ((layout::encode_message_record<tables::j_text, Row> (
                    digits, field::sample_value<typename Row::fields_type> ())
                    .has_value ()));
  EXPECT_EQ (tables::hex_of (binary),
             std::string (digits.begin (), digits.end ()))
      << Row::identifier () << " request";
  layout::bytes_t binary_reply;
  layout::bytes_t digits_reply;
  ASSERT_TRUE (
      (layout::encode_reply_message<tables::j_hid, Row> (
           binary_reply, field::sample_value<typename Row::reply_type> ())
           .has_value ()));
  ASSERT_TRUE (
      (layout::encode_reply_message<tables::j_text, Row> (
           digits_reply, field::sample_value<typename Row::reply_type> ())
           .has_value ()));
  EXPECT_EQ (tables::hex_of (binary_reply),
             std::string (digits_reply.begin (), digits_reply.end ()))
      << Row::identifier () << " reply";
}

template <typename List> struct every_row;

template <typename... Rows> struct every_row<dsl::type_list<Rows...>>
{
  static std::size_t
  check ()
  {
    int const results[]
        = { 0, (expect_same_message_in_both_layers<Rows> (), 0)... };
    (void)results;
    return sizeof...(Rows);
  }
};
} // namespace

TEST (
    LumexProtocolDeviceJMessagesTest,
    GivenEveryRowOfEveryLayer_WhenItsMessagesAreEncoded_ThenTheIndexAndDispatchFindTheSameRow)
{
  EXPECT_EQ (dev::expect_every_row_message<tables::j_text> (),
             dsl::command_count<tables::j_text>::value);
  EXPECT_EQ (dev::expect_every_row_message<tables::j_hid> (),
             dsl::command_count<tables::j_hid>::value);
}

TEST (
    LumexProtocolDeviceJMessagesTest,
    GivenEveryRow_WhenItsMessagesAreEncodedInBothLayers_ThenTheReportBytesAreTheTextDigits)
{
  EXPECT_EQ (every_row<dsl::commands_of<tables::j_text>::type>::check (),
             dsl::command_count<tables::j_text>::value);
}
