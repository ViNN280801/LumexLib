// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Device G (lumex/tests/core/protocol/devices/device_g): every row of every
// layer as a whole message. The sample request and reply of each row
// encode with the header and the key, the run-time index finds the row the
// header wrote with its fields right after the header, and dispatch hands
// the message back to the same row. The length byte of the older protocol
// is the length-prefix codec's: the codec's payload is the message, which
// starts with the command letter.

#include <cstddef>
#include <cstdint>
#include <system_error>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/framing/length/LumexLengthPrefixCodec.hpp"
#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/devices/LumexProtocolDevicesMessages.hpp"
#include "lumex/tests/core/protocol/devices/device_g/LumexProtocolDeviceGTable.hpp"

namespace
{
namespace dev = lumex_tests::devices;
namespace dsl = lumex::core::protocol::dsl;
namespace field = lumex::core::protocol::field;
namespace tables = lumex_tests::tables_g;
namespace layout = lumex::core::protocol::layout;
namespace index = lumex::core::protocol::index;
namespace framing = lumex::core::framing;

// The message of Row in the older protocol, framed by the length-prefix
// codec (one byte that counts the payload) and taken from the stream again:
// the payload is the message, it starts with the command letter, and the
// index and dispatch find Row in it.
template <typename Row>
void
expect_length_prefixed (layout::bytes_t const &message,
                        index::frame_direction direction)
{
  ASSERT_FALSE (message.empty ());
  EXPECT_EQ (message[0], static_cast<std::uint8_t> (Row::key ()))
      << Row::identifier ();
  framing::length::length_prefix_codec const codec;
  framing::codec::bytes_t frame;
  ASSERT_TRUE (codec
                   .encode (framing::codec::bytes_view_t (message.data (),
                                                          message.size ()),
                            frame)
                   .has_value ());
  ASSERT_EQ (frame.size (), message.size () + 1u);
  EXPECT_EQ (frame[0], message.size ());
  framing::length::length_prefix_codec decoder;
  std::vector<layout::bytes_t> payloads;
  decoder.decode (framing::codec::bytes_view_t (frame.data (), frame.size ()),
                  [&payloads] (framing::codec::bytes_view_t payload,
                               std::error_code failure)
                    {
                      if (!failure)
                        payloads.push_back (layout::bytes_t (payload.begin (),
                                                             payload.end ()));
                    });
  ASSERT_EQ (payloads.size (), 1u);
  EXPECT_EQ (payloads[0], message);
  EXPECT_TRUE (
      dev::finds<Row> (dev::index_of<tables::old_g> (), payloads[0], 1u));
  dev::dispatch_record<tables::old_g> seen;
  EXPECT_FALSE (layout::dispatch<tables::old_g> (dev::view_of (payloads[0]),
                                                 direction, seen));
  EXPECT_EQ (seen.row, Row::identifier ());
}

// The sample reply of a row; an empty record for a row without a reply.
template <typename Row, bool None = std::is_same<typename Row::reply_type,
                                                 dsl::no_reply>::value>
struct sample_reply
{
  static typename layout::reply_message_of<Row>::type
  get ()
  {
    return field::sample_value<typename Row::reply_type> ();
  }
};

template <typename Row> struct sample_reply<Row, true>
{
  static field::record<>
  get ()
  {
    return field::record<> ();
  }
};

template <typename Row>
void
expect_both_length_prefixed ()
{
  layout::bytes_t request;
  ASSERT_TRUE ((layout::encode_message_record<tables::old_g, Row> (
                    request, field::sample_value<typename Row::fields_type> ())
                    .has_value ()));
  expect_length_prefixed<Row> (request, index::frame_direction::tx);
  layout::bytes_t reply;
  ASSERT_TRUE ((layout::encode_reply_message<tables::old_g, Row> (
                    reply, sample_reply<Row>::get ())
                    .has_value ()));
  expect_length_prefixed<Row> (reply, index::frame_direction::rx);
}
} // namespace

TEST (
    LumexProtocolDeviceGMessagesTest,
    GivenEveryRowOfEveryLayer_WhenItsMessagesAreEncoded_ThenTheIndexAndDispatchFindTheSameRow)
{
  EXPECT_EQ (dev::expect_every_row_message<tables::family_g> (),
             dsl::command_count<tables::family_g>::value);
  EXPECT_EQ (dev::expect_every_row_message<tables::device_g> (),
             dsl::command_count<tables::device_g>::value);
  EXPECT_EQ (dev::expect_every_row_message<tables::old_g> (),
             dsl::command_count<tables::old_g>::value);
}

TEST (
    LumexProtocolDeviceGMessagesTest,
    GivenTheOlderProtocol_WhenItsMessagesAreFramedByTheLengthPrefixCodec_ThenTheMessageStartsWithTheCommandLetter)
{
  expect_both_length_prefixed<tables::old_g::letter_a_t> ();
  expect_both_length_prefixed<tables::old_g::letter_b_t> ();
}
