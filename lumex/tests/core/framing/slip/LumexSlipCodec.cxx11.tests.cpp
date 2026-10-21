// slip_codec (lumex/core/framing/slip): RFC 1055 framing. The cases of the
// DChannel v4 SLIP suite are ported (round trip, END and ESC in the payload,
// split frames, empty frames, reset, an escape across two calls); the "number
// byte" case is gone (RFC 1055 has no header), and the "byte after ESC" case
// now expects bad_escape, with lenient_escape for the RFC behaviour.

#include <cstddef>
#include <system_error>

#include <gtest/gtest.h>

#include "lumex/core/framing/slip/LumexSlipCodec.hpp"

#include "lumex/tests/core/framing/LumexFramingTestSupport.hpp"

namespace
{
using namespace lumex_framing_test;
using lumex::core::framing::codec::framing_errc;
using lumex::core::framing::slip::slip_codec;
using lumex::core::framing::slip::slip_options_t;

byte_t const END = 0xC0;
byte_t const ESC = 0xDB;
byte_t const ESC_END = 0xDC;
byte_t const ESC_ESC = 0xDD;

slip_options_t
limit (std::size_t max_frame_size)
{
  slip_options_t options;
  options.max_frame_size = max_frame_size;
  return options;
}
} // namespace

TEST (LumexSlipCodecTest,
      GivenOrdinaryBytes_WhenEncodeThenDecode_ThenTheyRoundTripExactly)
{
  slip_codec codec;
  bytes_t const payload{ 0x01, 0x02, 0x7F, 0x80, 0xFF };

  received_t const received = decode_all (codec, encode_ok (codec, payload));

  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], payload);
  EXPECT_TRUE (received.errors.empty ());
}

TEST (LumexSlipCodecTest,
      GivenAnEncodedFrame_WhenInspected_ThenItStartsAndEndsWithEnd)
{
  slip_codec const codec;
  bytes_t const frame = encode_ok (codec, bytes_t{ 0x11, 0x22 });
  EXPECT_EQ (frame, (bytes_t{ END, 0x11, 0x22, END }));
}

TEST (LumexSlipCodecTest,
      GivenEndAndEscInThePayload_WhenEncoded_ThenTheyAreEscaped)
{
  slip_codec const codec;
  bytes_t const frame = encode_ok (codec, bytes_t{ END, 0x05, ESC });
  EXPECT_EQ (frame, (bytes_t{ END, ESC, ESC_END, 0x05, ESC, ESC_ESC, END }));
}

TEST (LumexSlipCodecTest,
      GivenEndAndEscInThePayload_WhenRoundTripped_ThenTheySurvive)
{
  slip_codec codec;
  bytes_t const payload{ END, ESC, END, END, ESC, 0x00 };
  received_t const received = decode_all (codec, encode_ok (codec, payload));
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], payload);
}

TEST (LumexSlipCodecTest,
      GivenAFrameSplitAtEveryPosition_WhenDecoded_ThenItArrivesOnceWhole)
{
  bytes_t const payload{ 0x10, END, 0x20, ESC, 0x30 };
  bytes_t const stream = encode_ok (slip_codec (), payload);
  for (std::size_t at = 0; at <= stream.size (); ++at)
    {
      slip_codec codec;
      received_t const received = decode_split (codec, stream, at);
      ASSERT_EQ (received.frames.size (), 1u) << "split at " << at;
      EXPECT_EQ (received.frames[0], payload) << "split at " << at;
    }
}

TEST (LumexSlipCodecTest,
      GivenEscAsTheLastByteOfACall_WhenTheNextCallContinues_ThenTheEscapeHolds)
{
  slip_codec codec;
  received_t received;
  recording_sink sink{ &received };
  bytes_t const first{ END, 0x01, ESC };
  bytes_t const second{ ESC_END, 0x02, END };

  codec.decode (bytes_view_t (first), sink);
  codec.decode (bytes_view_t (second), sink);

  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], (bytes_t{ 0x01, END, 0x02 }));
}

TEST (LumexSlipCodecTest,
      GivenTwoConsecutiveEnds_WhenDecoded_ThenTheEmptyFrameIsSkipped)
{
  slip_codec codec;
  received_t const received
      = decode_all (codec, bytes_t{ END, END, 0x07, END, END, END });
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], (bytes_t{ 0x07 }));
  EXPECT_TRUE (received.errors.empty ());
}

TEST (LumexSlipCodecTest,
      GivenTwoFramesInOneChunk_WhenDecoded_ThenBothArriveInOrder)
{
  slip_codec codec;
  bytes_t const stream = concat (encode_ok (codec, bytes_t{ 1 }),
                                 encode_ok (codec, bytes_t{ 2, 3 }));
  received_t const received = decode_all (codec, stream);
  ASSERT_EQ (received.frames.size (), 2u);
  EXPECT_EQ (received.frames[0], (bytes_t{ 1 }));
  EXPECT_EQ (received.frames[1], (bytes_t{ 2, 3 }));
}

TEST (
    LumexSlipCodecTest,
    GivenAPartialFrame_WhenResetThenACompleteFrameIsFed_ThenOnlyTheNewArrives)
{
  slip_codec codec;
  received_t received;
  recording_sink sink{ &received };
  bytes_t const garbage{ 0x55, 0x66, ESC };
  codec.decode (bytes_view_t (garbage), sink);

  codec.reset ();
  bytes_t const frame = encode_ok (codec, bytes_t{ 0x09 });
  codec.decode (bytes_view_t (frame), sink);

  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], (bytes_t{ 0x09 }));
}

TEST (LumexSlipCodecTest,
      GivenABadEscape_WhenDecoded_ThenTheFrameIsDroppedWithBadEscape)
{
  slip_codec codec;
  received_t const received = decode_all (
      codec, bytes_t{ END, 0x01, ESC, 0x41, 0x02, END, 0x03, END });
  ASSERT_EQ (received.errors.size (), 1u);
  EXPECT_TRUE (received.errors[0] == framing_errc::bad_escape);
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], (bytes_t{ 0x03 }));
  EXPECT_EQ (received.order, "EF");
}

TEST (LumexSlipCodecTest,
      GivenEscFollowedByEnd_WhenDecoded_ThenBadEscapeAndTheEndStillEndsIt)
{
  slip_codec codec;
  received_t const received
      = decode_all (codec, bytes_t{ END, 0x01, ESC, END, 0x04, END });
  ASSERT_EQ (received.errors.size (), 1u);
  EXPECT_TRUE (received.errors[0] == framing_errc::bad_escape);
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], (bytes_t{ 0x04 }));
}

TEST (LumexSlipCodecTest,
      GivenLenientEscape_WhenAByteFollowsEscIllegally_ThenItIsStoredAsItIs)
{
  slip_options_t options;
  options.lenient_escape = true;
  slip_codec codec (options);
  received_t const received
      = decode_all (codec, bytes_t{ END, 0x01, ESC, 0x41, 0x02, END });
  EXPECT_TRUE (received.errors.empty ());
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], (bytes_t{ 0x01, 0x41, 0x02 }));
}

TEST (LumexSlipCodecTest,
      GivenAPayloadAtTheLimit_WhenEncodedAndDecoded_ThenItRoundTrips)
{
  slip_codec codec (limit (4));
  bytes_t const payload{ END, ESC, 3, 4 };
  bytes_t const frame = encode_ok (codec, payload);
  ASSERT_FALSE (frame.empty ());
  received_t const received = decode_all (codec, frame);
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], payload);
}

TEST (LumexSlipCodecTest,
      GivenAPayloadOnePastTheLimit_WhenEncoded_ThenPayloadTooLargeAndNoOutput)
{
  slip_codec const codec (limit (4));
  bytes_t const payload{ 1, 2, 3, 4, 5 };
  bytes_t out{ 0xAA };
  auto const result = codec.encode (bytes_view_t (payload), out);
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == framing_errc::payload_too_large);
  EXPECT_EQ (out, (bytes_t{ 0xAA }));
}

TEST (LumexSlipCodecTest,
      GivenAFrameOnePastTheLimit_WhenDecoded_ThenFrameTooLargeOnceAndResync)
{
  slip_codec codec (limit (3));
  received_t const received = decode_all (
      codec, bytes_t{ END, 1, 2, 3, 4, 5, 6, END, 7, 8, 9, END });
  ASSERT_EQ (received.errors.size (), 1u);
  EXPECT_TRUE (received.errors[0] == framing_errc::frame_too_large);
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], (bytes_t{ 7, 8, 9 }));
  EXPECT_EQ (received.order, "EF");
}

TEST (LumexSlipCodecTest,
      GivenAnEmptyPayload_WhenEncoded_ThenTwoEndsAndNothingIsDecoded)
{
  slip_codec codec;
  bytes_t const frame = encode_ok (codec, bytes_t ());
  EXPECT_EQ (frame, (bytes_t{ END, END }));
  received_t const received = decode_all (codec, frame);
  EXPECT_TRUE (received.frames.empty ());
  EXPECT_TRUE (received.errors.empty ());
}

TEST (LumexSlipCodecTest,
      GivenAStreamFedOneByteAtATime_WhenDecoded_ThenTheSameFramesArrive)
{
  slip_codec codec;
  bytes_t const stream = concat (encode_ok (codec, bytes_t{ END, 1 }),
                                 encode_ok (codec, bytes_t{ ESC, 2 }));
  received_t const received = decode_bytewise (codec, stream);
  ASSERT_EQ (received.frames.size (), 2u);
  EXPECT_EQ (received.frames[0], (bytes_t{ END, 1 }));
  EXPECT_EQ (received.frames[1], (bytes_t{ ESC, 2 }));
}

TEST (LumexSlipCodecTest,
      GivenAFrameOnlySink_WhenAFrameIsDropped_ThenOnlyGoodFramesArrive)
{
  slip_codec codec (limit (2));
  std::vector<bytes_t> frames;
  auto sink = [&frames] (bytes_view_t frame)
    { frames.push_back (bytes_t (frame.begin (), frame.end ())); };
  bytes_t const stream{ END, 1, 2, 3, END, 4, END };

  std::size_t const consumed = codec.decode (bytes_view_t (stream), sink);

  EXPECT_EQ (consumed, stream.size ());
  ASSERT_EQ (frames.size (), 1u);
  EXPECT_EQ (frames[0], (bytes_t{ 4 }));
}

// Invalid options are a contract violation (point 2 of the framing plan): with
// a semantic that stops the program, a zero limit does.
#if GTEST_HAS_DEATH_TEST && LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC >= 3
TEST (LumexSlipCodecDeathTest,
      GivenAZeroLimit_WhenConstructed_ThenTheContractStops)
{
  EXPECT_DEATH (slip_codec codec (limit (0)), "");
}
#endif
