// length_prefix_codec (lumex/core/framing/length): the cases of the DChannel
// v4 TCP suite are ported (round trip, the first byte is the length, a payload
// at the cap, frames split at the length byte and inside the payload, two
// frames in one chunk, a zero length, reset); the v4 finding "one past the cap
// is silently truncated" is now payload_too_large. Every width and byte order,
// the prefix-inclusive length, an oversized frame skipped and a bad header
// are added.

#include <cstddef>
#include <system_error>

#include <gtest/gtest.h>

#include "lumex/core/framing/length/LumexLengthPrefixCodec.hpp"

#include "lumex/tests/core/framing/LumexFramingTestSupport.hpp"

namespace
{
using namespace lumex_framing_test;
using lumex::core::framing::codec::byte_order;
using lumex::core::framing::codec::framing_errc;
using lumex::core::framing::length::length_prefix_codec;
using lumex::core::framing::length::length_prefix_options_t;

length_prefix_options_t
make_options (std::size_t width, byte_order order = byte_order::big,
              bool includes_prefix = false, std::size_t max_frame_size = 65536)
{
  length_prefix_options_t options;
  options.width = width;
  options.order = order;
  options.length_includes_prefix = includes_prefix;
  options.max_frame_size = max_frame_size;
  return options;
}
} // namespace

TEST (LumexLengthPrefixCodecTest,
      GivenAPayload_WhenEncodedThenDecoded_ThenItRoundTrips)
{
  length_prefix_codec codec;
  bytes_t const payload{ 1, 2, 3 };
  received_t const received = decode_all (codec, encode_ok (codec, payload));
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], payload);
}

TEST (LumexLengthPrefixCodecTest,
      GivenAPayload_WhenEncoded_ThenTheFirstByteIsItsLength)
{
  length_prefix_codec const codec;
  EXPECT_EQ (encode_ok (codec, bytes_t{ 9, 8 }), (bytes_t{ 2, 9, 8 }));
}

TEST (
    LumexLengthPrefixCodecTest,
    GivenAPayloadExactlyAtTheFieldMaximum_WhenRoundTripped_ThenItArrivesWhole)
{
  length_prefix_codec codec;
  bytes_t const payload (255, 0x5A);
  received_t const received = decode_all (codec, encode_ok (codec, payload));
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], payload);
}

TEST (LumexLengthPrefixCodecTest,
      GivenAPayloadOnePastTheFieldMaximum_WhenEncoded_ThenPayloadTooLarge)
{
  length_prefix_codec const codec;
  bytes_t const payload (256, 0x5A);
  bytes_t out;
  auto const result = codec.encode (bytes_view_t (payload), out);
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == framing_errc::payload_too_large);
  EXPECT_TRUE (out.empty ());
}

TEST (LumexLengthPrefixCodecTest,
      GivenAPayloadOnePastMaxFrameSize_WhenEncoded_ThenPayloadTooLarge)
{
  length_prefix_codec const codec (
      make_options (2, byte_order::big, false, 10));
  bytes_t out;
  bytes_t const payload (11, 1);
  auto const result = codec.encode (bytes_view_t (payload), out);
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == framing_errc::payload_too_large);
}

TEST (LumexLengthPrefixCodecTest,
      GivenAFrameSplitAtEveryPosition_WhenDecoded_ThenItArrivesOnceWhole)
{
  bytes_t const payload{ 10, 20, 30, 40 };
  for (std::size_t width : { 1u, 2u, 4u })
    {
      bytes_t const stream
          = encode_ok (length_prefix_codec (make_options (width)), payload);
      for (std::size_t at = 0; at <= stream.size (); ++at)
        {
          length_prefix_codec codec (make_options (width));
          received_t const received = decode_split (codec, stream, at);
          ASSERT_EQ (received.frames.size (), 1u)
              << "width " << width << " split at " << at;
          EXPECT_EQ (received.frames[0], payload);
        }
    }
}

TEST (LumexLengthPrefixCodecTest,
      GivenTwoFramesInOneChunk_WhenDecoded_ThenBothArrive)
{
  length_prefix_codec codec;
  bytes_t const stream = concat (encode_ok (codec, bytes_t{ 1 }),
                                 encode_ok (codec, bytes_t{ 2, 3 }));
  received_t const received = decode_all (codec, stream);
  ASSERT_EQ (received.frames.size (), 2u);
  EXPECT_EQ (received.frames[0], (bytes_t{ 1 }));
  EXPECT_EQ (received.frames[1], (bytes_t{ 2, 3 }));
}

TEST (LumexLengthPrefixCodecTest,
      GivenAZeroLength_WhenDecoded_ThenAnEmptyFrameArrivesAtOnce)
{
  length_prefix_codec codec;
  received_t const received = decode_all (codec, bytes_t{ 0, 1, 7 });
  ASSERT_EQ (received.frames.size (), 2u);
  EXPECT_TRUE (received.frames[0].empty ());
  EXPECT_EQ (received.frames[1], (bytes_t{ 7 }));
}

TEST (LumexLengthPrefixCodecTest,
      GivenAPartialFrame_WhenResetThenFed_ThenTheStaleDataIsDiscarded)
{
  length_prefix_codec codec;
  received_t received;
  recording_sink sink{ &received };
  bytes_t const stale{ 5, 1, 2 };
  codec.decode (bytes_view_t (stale), sink);
  codec.reset ();
  bytes_t const fresh{ 1, 9 };
  codec.decode (bytes_view_t (fresh), sink);
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], (bytes_t{ 9 }));
}

TEST (LumexLengthPrefixCodecTest,
      GivenEveryWidthAndOrder_WhenEncoded_ThenTheFieldIsLaidOutAsAsked)
{
  EXPECT_EQ (
      encode_ok (length_prefix_codec (make_options (2, byte_order::big)),
                 bytes_t (4, 0xEE)),
      concat (bytes_t{ 0x00, 0x04 }, bytes_t (4, 0xEE)));
  EXPECT_EQ (
      encode_ok (length_prefix_codec (make_options (2, byte_order::little)),
                 bytes_t (4, 0xEE)),
      concat (bytes_t{ 0x04, 0x00 }, bytes_t (4, 0xEE)));
  EXPECT_EQ (
      encode_ok (length_prefix_codec (make_options (4, byte_order::big)),
                 bytes_t (4, 0xEE)),
      concat (bytes_t{ 0, 0, 0, 4 }, bytes_t (4, 0xEE)));
  EXPECT_EQ (
      encode_ok (length_prefix_codec (make_options (4, byte_order::little)),
                 bytes_t (4, 0xEE)),
      concat (bytes_t{ 4, 0, 0, 0 }, bytes_t (4, 0xEE)));
}

TEST (LumexLengthPrefixCodecTest,
      GivenALittleEndianLengthAbove255_WhenRoundTripped_ThenItArrivesWhole)
{
  length_prefix_codec codec (make_options (2, byte_order::little));
  bytes_t const payload (300, 0x33);
  bytes_t const frame = encode_ok (codec, payload);
  ASSERT_EQ (frame.size (), 302u);
  EXPECT_EQ (frame[0], 0x2C);
  EXPECT_EQ (frame[1], 0x01);
  received_t const received = decode_all (codec, frame);
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], payload);
}

TEST (LumexLengthPrefixCodecTest,
      GivenALengthThatIncludesThePrefix_WhenRoundTripped_ThenTheFieldCountsIt)
{
  length_prefix_codec codec (make_options (2, byte_order::big, true));
  bytes_t const frame = encode_ok (codec, bytes_t{ 7, 7, 7 });
  EXPECT_EQ (frame, (bytes_t{ 0x00, 0x05, 7, 7, 7 }));
  received_t const received = decode_all (codec, frame);
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], (bytes_t{ 7, 7, 7 }));
}

TEST (LumexLengthPrefixCodecTest,
      GivenAnInclusiveLengthBelowTheField_WhenDecoded_ThenBadHeader)
{
  length_prefix_codec codec (make_options (2, byte_order::big, true));
  received_t const received
      = decode_all (codec, bytes_t{ 0x00, 0x01, 0x00, 0x03, 9 });
  ASSERT_EQ (received.errors.size (), 1u);
  EXPECT_TRUE (received.errors[0] == framing_errc::bad_header);
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], (bytes_t{ 9 }));
}

TEST (LumexLengthPrefixCodecTest,
      GivenALengthAboveTheLimit_WhenDecoded_ThenTheFrameIsSkippedOnce)
{
  length_prefix_codec codec (make_options (1, byte_order::big, false, 3));
  received_t const received
      = decode_all (codec, bytes_t{ 5, 1, 2, 3, 4, 5, 2, 8, 9 });
  ASSERT_EQ (received.errors.size (), 1u);
  EXPECT_TRUE (received.errors[0] == framing_errc::frame_too_large);
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], (bytes_t{ 8, 9 }));
  EXPECT_EQ (received.order, "EF");
}

TEST (LumexLengthPrefixCodecTest,
      GivenAFrameAtTheLimit_WhenDecoded_ThenItArrives)
{
  length_prefix_codec codec (make_options (1, byte_order::big, false, 3));
  received_t const received = decode_all (codec, bytes_t{ 3, 1, 2, 3 });
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_TRUE (received.errors.empty ());
}

#if GTEST_HAS_DEATH_TEST && LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC >= 3
TEST (LumexLengthPrefixCodecDeathTest,
      GivenAWidthOfThree_WhenConstructed_ThenTheContractStops)
{
  EXPECT_DEATH (length_prefix_codec codec (make_options (3)), "");
}

TEST (LumexLengthPrefixCodecDeathTest,
      GivenAZeroLimit_WhenConstructed_ThenTheContractStops)
{
  EXPECT_DEATH (
      length_prefix_codec codec (make_options (1, byte_order::big, false, 0)),
      "");
}
#endif
