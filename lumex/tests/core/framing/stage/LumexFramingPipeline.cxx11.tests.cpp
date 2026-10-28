// pipeline (lumex/core/framing/stage) with the frame of a text device
// (device J): '%', the hex text, the XOR of that text as two hex digits, CR.
// The checksum covers the hex characters after '%', so the check stage sits
// outside the hex stage. The reference frames: the query command
// "%8D7C\r" and the 77-byte reply to it.
//
// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.

#include <cstddef>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

#include "lumex/core/framing/delimiter/LumexDelimiterCodec.hpp"
#include "lumex/core/framing/length/LumexLengthPrefixCodec.hpp"
#include "lumex/core/framing/stage/LumexFramingPipeline.hpp"
#include "lumex/core/framing/stage/LumexFramingStages.hpp"

#include "lumex/tests/core/framing/LumexFramingTestSupport.hpp"

namespace
{
using namespace lumex_framing_test;
using lumex::core::framing::codec::framing_errc;
using lumex::core::framing::delimiter::delimiter_codec;
using lumex::core::framing::length::length_prefix_codec;
namespace st = lumex::core::framing::stage;

using text_codec = st::pipeline<
    delimiter_codec, st::prefix_char_stage<'%'>,
    st::checksum_stage<st::xor8, st::ascii_hex_render, st::checksum_optional>,
    st::ascii_hex_stage>;

using text_strict_codec
    = st::pipeline<delimiter_codec, st::prefix_char_stage<'%'>,
                   st::checksum_stage<st::xor8, st::ascii_hex_render>,
                   st::ascii_hex_stage>;

// The reply to %8D: '%' 8D 00, 46 zero digits, the serial number in four
// digits, 20 zero digits, the XOR, CR.
std::string
query_reply (bool break_checksum)
{
  std::string frame = "%8D00";
  frame.append (46, '0');
  frame += "6073";
  frame.append (20, '0');
  unsigned checksum = 0;
  for (std::size_t i = 1; i < frame.size (); ++i)
    checksum ^= static_cast<unsigned char> (frame[i]);
  if (break_checksum)
    checksum ^= 0xFFu;
  char const digits[] = "0123456789ABCDEF";
  frame += digits[(checksum >> 4u) & 0xFu];
  frame += digits[checksum & 0xFu];
  return frame + "\r";
}
} // namespace

TEST (LumexFramingPipelineTest,
      GivenTheQueryCommand_WhenEncoded_ThenItIsPercent8D7CCr)
{
  text_codec const codec;
  EXPECT_EQ (encode_ok (codec, bytes_t{ 0x8D }), bytes_of ("%8D7C\r"));
}

TEST (LumexFramingPipelineTest,
      GivenTheCommandBytes_WhenDecoded_ThenTheOpcodeReturns)
{
  text_codec codec;
  received_t const received = decode_all (codec, bytes_of ("%8D7C\r"));
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], (bytes_t{ 0x8D }));
}

TEST (LumexFramingPipelineTest,
      GivenThe77ByteReply_WhenDecoded_ThenThe37DataBytesArrive)
{
  std::string const reply = query_reply (false);
  ASSERT_EQ (reply.size (), 78u);
  text_strict_codec codec;
  received_t const received = decode_all (codec, bytes_of (reply));
  ASSERT_EQ (received.frames.size (), 1u);
  bytes_t expected (37, 0x00);
  expected[0] = 0x8D;
  expected[25] = 0x60;
  expected[26] = 0x73;
  EXPECT_EQ (received.frames[0], expected);
}

TEST (LumexFramingPipelineTest,
      GivenTheReplyWithABrokenCheck_WhenStrict_ThenBadChecksum)
{
  text_strict_codec codec;
  received_t const received
      = decode_all (codec, bytes_of (query_reply (true)));
  EXPECT_TRUE (received.frames.empty ());
  ASSERT_EQ (received.errors.size (), 1u);
  EXPECT_TRUE (received.errors[0] == framing_errc::bad_checksum);
}

TEST (LumexFramingPipelineTest,
      GivenAShortReplyWithoutCheck_WhenOptional_ThenItPassesWhole)
{
  // %AC00\r: the state reply sent without the check value; "00" is no valid
  // XOR of "AC", so the whole text is data.
  text_codec codec;
  received_t const received = decode_all (codec, bytes_of ("%AC00\r"));
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], (bytes_t{ 0xAC, 0x00 }));
}

TEST (LumexFramingPipelineTest,
      GivenTheShortStateReply_WhenEncoded_ThenItIsEightBytes)
{
  text_codec const codec;
  bytes_t const frame = encode_ok (codec, bytes_t{ 0xAC, 0x00 });
  EXPECT_EQ (frame, bytes_of ("%AC0002\r"));
  EXPECT_EQ (frame.size (), 8u);
}

TEST (LumexFramingPipelineTest,
      GivenAWrongPrefix_WhenDecoded_ThenBadHeaderAndTheNextFrameArrives)
{
  text_codec codec;
  received_t const received = decode_all (codec, bytes_of ("!8D7C\r%8D7C\r"));
  ASSERT_EQ (received.errors.size (), 1u);
  EXPECT_TRUE (received.errors[0] == framing_errc::bad_header);
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.order, "EF");
}

TEST (LumexFramingPipelineTest,
      GivenTheReplySplitAtEveryPosition_WhenDecoded_ThenItArrivesOnce)
{
  bytes_t const stream = bytes_of (query_reply (false));
  for (std::size_t at = 0; at <= stream.size (); ++at)
    {
      text_strict_codec codec;
      received_t const received = decode_split (codec, stream, at);
      ASSERT_EQ (received.frames.size (), 1u) << "split at " << at;
      EXPECT_EQ (received.frames[0].size (), 37u);
    }
}

TEST (LumexFramingPipelineTest,
      GivenAFrameOnlySink_WhenAStageFails_ThenOnlyGoodFramesArrive)
{
  text_strict_codec codec;
  std::vector<bytes_t> frames;
  auto sink = [&frames] (bytes_view_t frame)
    { frames.push_back (bytes_t (frame.begin (), frame.end ())); };
  bytes_t const stream = bytes_of ("%8D7D\r%8D7C\r");
  codec.decode (bytes_view_t (stream), sink);
  ASSERT_EQ (frames.size (), 1u);
  EXPECT_EQ (frames[0], (bytes_t{ 0x8D }));
}

TEST (LumexFramingPipelineTest,
      GivenOuterOptionsAndStages_WhenConstructed_ThenBothAreUsed)
{
  using framed
      = st::pipeline<length_prefix_codec, st::prefix_char_stage<0x7E>>;
  length_prefix_codec::options_type options;
  options.width = 2;
  framed const codec (options, st::prefix_char_stage<0x7E> ());
  EXPECT_EQ (encode_ok (codec, bytes_t{ 1, 2 }),
             (bytes_t{ 0, 3, 0x7E, 1, 2 }));
}

TEST (LumexFramingPipelineTest,
      GivenAPartialFrame_WhenReset_ThenTheOuterCodecForgetsIt)
{
  text_codec codec;
  received_t received;
  recording_sink sink{ &received };
  bytes_t const partial = bytes_of ("%8D");
  codec.decode (bytes_view_t (partial), sink);
  codec.reset ();
  bytes_t const whole = bytes_of ("%8D7C\r");
  codec.decode (bytes_view_t (whole), sink);
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], (bytes_t{ 0x8D }));
}
