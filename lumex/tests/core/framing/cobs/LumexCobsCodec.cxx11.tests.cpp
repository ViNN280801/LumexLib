// cobs_codec (lumex/core/framing/cobs): the reference encodings of
// Cheshire and Baker (the examples of the COBS article), the group boundaries
// around 254 bytes, a zero at every position, a non-zero delimiter, truncated
// frames, the frame limit and resynchronization.

#include <cstddef>
#include <system_error>

#include <gtest/gtest.h>

#include "lumex/core/framing/cobs/LumexCobsCodec.hpp"

#include "lumex/tests/core/framing/LumexFramingTestSupport.hpp"

namespace
{
using namespace lumex_framing_test;
using lumex::core::framing::cobs::cobs_codec;
using lumex::core::framing::cobs::cobs_encoded_bound;
using lumex::core::framing::cobs::cobs_options_t;
using lumex::core::framing::codec::framing_errc;

// first, first + 1, ... for `count` bytes, wrapping past 0xFF to 0x01.
bytes_t
counting (byte_t first, std::size_t count)
{
  bytes_t out;
  byte_t value = first;
  for (std::size_t i = 0; i < count; ++i)
    {
      out.push_back (value);
      value = static_cast<byte_t> (value == 0xFF ? 0x01 : value + 1);
    }
  return out;
}

cobs_options_t
options_with (std::size_t max_frame_size, byte_t delimiter = 0x00)
{
  cobs_options_t options;
  options.max_frame_size = max_frame_size;
  options.delimiter = delimiter;
  return options;
}

void
expect_round_trip (cobs_codec &codec, bytes_t const &payload)
{
  bytes_t const frame = encode_ok (codec, payload);
  ASSERT_FALSE (frame.empty ());
  received_t const received = decode_all (codec, frame);
  ASSERT_EQ (received.frames.size (), 1u) << payload.size () << " bytes";
  EXPECT_EQ (received.frames[0], payload) << payload.size () << " bytes";
  EXPECT_TRUE (received.errors.empty ());
}
} // namespace

TEST (LumexCobsCodecTest, GivenTheReferenceExamples_WhenEncoded_ThenBytesMatch)
{
  cobs_codec const codec;
  EXPECT_EQ (encode_ok (codec, bytes_t{ 0x00 }),
             (bytes_t{ 0x01, 0x01, 0x00 }));
  EXPECT_EQ (encode_ok (codec, bytes_t{ 0x00, 0x00 }),
             (bytes_t{ 0x01, 0x01, 0x01, 0x00 }));
  EXPECT_EQ (encode_ok (codec, bytes_t{ 0x00, 0x11, 0x00 }),
             (bytes_t{ 0x01, 0x02, 0x11, 0x01, 0x00 }));
  EXPECT_EQ (encode_ok (codec, bytes_t{ 0x11, 0x22, 0x00, 0x33 }),
             (bytes_t{ 0x03, 0x11, 0x22, 0x02, 0x33, 0x00 }));
  EXPECT_EQ (encode_ok (codec, bytes_t{ 0x11, 0x22, 0x33, 0x44 }),
             (bytes_t{ 0x05, 0x11, 0x22, 0x33, 0x44, 0x00 }));
  EXPECT_EQ (encode_ok (codec, bytes_t{ 0x11, 0x00, 0x00, 0x00 }),
             (bytes_t{ 0x02, 0x11, 0x01, 0x01, 0x01, 0x00 }));
}

TEST (LumexCobsCodecTest,
      GivenTheFullGroupExamples_WhenEncoded_ThenNoExtraCodeIsWritten)
{
  cobs_codec const codec;
  // 01..FE (254 bytes): FF 01..FE 00.
  bytes_t expected_254 = concat (bytes_t{ 0xFF }, counting (0x01, 254));
  expected_254.push_back (0x00);
  EXPECT_EQ (encode_ok (codec, counting (0x01, 254)), expected_254);

  // 00 01..FE (255 bytes): 01 FF 01..FE 00.
  bytes_t expected_lead = concat (bytes_t{ 0x01, 0xFF }, counting (0x01, 254));
  expected_lead.push_back (0x00);
  EXPECT_EQ (encode_ok (codec, concat (bytes_t{ 0x00 }, counting (0x01, 254))),
             expected_lead);

  // 01..FF (255 bytes): FF 01..FE 02 FF 00.
  bytes_t expected_255 = concat (bytes_t{ 0xFF }, counting (0x01, 254));
  expected_255.push_back (0x02);
  expected_255.push_back (0xFF);
  expected_255.push_back (0x00);
  EXPECT_EQ (encode_ok (codec, counting (0x01, 255)), expected_255);
}

TEST (LumexCobsCodecTest,
      GivenPayloadsAroundTheGroupSize_WhenRoundTripped_ThenTheyArriveWhole)
{
  cobs_codec codec;
  for (std::size_t size : { 1u, 253u, 254u, 255u, 508u, 509u, 1000u })
    expect_round_trip (codec, counting (0x01, size));
}

TEST (LumexCobsCodecTest,
      GivenAZeroAtEveryPosition_WhenRoundTripped_ThenTheyArriveWhole)
{
  cobs_codec codec;
  for (std::size_t at = 0; at < 300; ++at)
    {
      bytes_t payload = counting (0x01, 300);
      payload[at] = 0x00;
      expect_round_trip (codec, payload);
    }
}

TEST (LumexCobsCodecTest, GivenOnlyZeros_WhenRoundTripped_ThenTheyArriveWhole)
{
  cobs_codec codec;
  expect_round_trip (codec, bytes_t (600, 0x00));
}

TEST (LumexCobsCodecTest,
      GivenAnEmptyPayload_WhenEncoded_ThenItIsDeliveredAsAnEmptyFrame)
{
  cobs_codec codec;
  bytes_t const frame = encode_ok (codec, bytes_t ());
  EXPECT_EQ (frame, (bytes_t{ 0x01, 0x00 }));
  received_t const received = decode_all (codec, frame);
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_TRUE (received.frames[0].empty ());
}

TEST (LumexCobsCodecTest,
      GivenDelimitersAlone_WhenDecoded_ThenTheyAreSkippedAsNoise)
{
  cobs_codec codec;
  received_t const received = decode_all (codec, bytes_t{ 0x00, 0x00, 0x00 });
  EXPECT_TRUE (received.frames.empty ());
  EXPECT_TRUE (received.errors.empty ());
}

TEST (LumexCobsCodecTest,
      GivenANonZeroDelimiter_WhenEncoded_ThenItAppearsOnlyAtTheEnd)
{
  cobs_codec codec (options_with (1024, 0x7E));
  bytes_t payload = counting (0x01, 400);
  payload[10] = 0x00;
  payload[20] = 0x7E;
  bytes_t const frame = encode_ok (codec, payload);
  ASSERT_FALSE (frame.empty ());
  for (std::size_t i = 0; i + 1 < frame.size (); ++i)
    ASSERT_NE (frame[i], 0x7E) << "at " << i;
  EXPECT_EQ (frame.back (), 0x7E);
  expect_round_trip (codec, payload);
}

TEST (LumexCobsCodecTest,
      GivenACodeThatPointsPastTheFrame_WhenDecoded_ThenTruncatedAndResync)
{
  cobs_codec codec;
  bytes_t stream{ 0x05, 0x11, 0x22, 0x00 };
  stream = concat (stream, encode_ok (codec, bytes_t{ 0x33 }));
  received_t const received = decode_all (codec, stream);
  ASSERT_EQ (received.errors.size (), 1u);
  EXPECT_TRUE (received.errors[0] == framing_errc::truncated);
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], (bytes_t{ 0x33 }));
  EXPECT_EQ (received.order, "EF");
}

TEST (LumexCobsCodecTest,
      GivenAFrameSplitAtEveryPosition_WhenDecoded_ThenItArrivesOnceWhole)
{
  bytes_t payload = counting (0x01, 300);
  payload[100] = 0x00;
  bytes_t const stream = encode_ok (cobs_codec (), payload);
  for (std::size_t at = 0; at <= stream.size (); ++at)
    {
      cobs_codec codec;
      received_t const received = decode_split (codec, stream, at);
      ASSERT_EQ (received.frames.size (), 1u) << "split at " << at;
      EXPECT_EQ (received.frames[0], payload) << "split at " << at;
    }
}

TEST (LumexCobsCodecTest,
      GivenAPayloadOnePastTheLimit_WhenEncoded_ThenPayloadTooLarge)
{
  cobs_codec codec (options_with (8));
  bytes_t out;
  bytes_t const payload (9, 0x01);
  auto const result = codec.encode (bytes_view_t (payload), out);
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == framing_errc::payload_too_large);
  EXPECT_TRUE (out.empty ());
  expect_round_trip (codec, bytes_t (8, 0x01));
}

TEST (LumexCobsCodecTest,
      GivenAnOversizedFrame_WhenDecoded_ThenFrameTooLargeOnceAndResync)
{
  cobs_codec big;
  cobs_codec small (options_with (8));
  bytes_t const stream = concat (encode_ok (big, bytes_t (50, 0x01)),
                                 encode_ok (big, bytes_t{ 0x05, 0x00 }));
  received_t const received = decode_all (small, stream);
  ASSERT_EQ (received.errors.size (), 1u);
  EXPECT_TRUE (received.errors[0] == framing_errc::frame_too_large);
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], (bytes_t{ 0x05, 0x00 }));
}

TEST (LumexCobsCodecTest,
      GivenAFrameWhoseZerosPushItPastTheLimit_WhenDecoded_ThenFrameTooLarge)
{
  // 255 zeros encode to 256 bytes, exactly the encoded bound of a limit of
  // 254, but decode to 255 bytes: the decoded size check catches them.
  cobs_codec small (options_with (254));
  bytes_t const frame = encode_ok (cobs_codec (), bytes_t (255, 0x00));
  ASSERT_EQ (frame.size () - 1, cobs_encoded_bound (254));
  received_t const received = decode_all (small, frame);
  EXPECT_TRUE (received.frames.empty ());
  ASSERT_EQ (received.errors.size (), 1u);
  EXPECT_TRUE (received.errors[0] == framing_errc::frame_too_large);
}

TEST (
    LumexCobsCodecTest,
    GivenAPartialFrame_WhenResetThenACompleteFrameIsFed_ThenOnlyTheNewArrives)
{
  cobs_codec codec;
  received_t received;
  recording_sink sink{ &received };
  bytes_t const partial{ 0x05, 0x11 };
  codec.decode (bytes_view_t (partial), sink);
  codec.reset ();
  bytes_t const frame = encode_ok (codec, bytes_t{ 0x42 });
  codec.decode (bytes_view_t (frame), sink);
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], (bytes_t{ 0x42 }));
  EXPECT_TRUE (received.errors.empty ());
}

#if GTEST_HAS_DEATH_TEST && LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC >= 3
TEST (LumexCobsCodecDeathTest,
      GivenAZeroLimit_WhenConstructed_ThenTheContractStops)
{
  EXPECT_DEATH (cobs_codec codec (options_with (0)), "");
}
#endif
