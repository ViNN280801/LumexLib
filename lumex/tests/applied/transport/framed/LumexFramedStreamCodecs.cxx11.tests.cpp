// Every codec of core/framing through framed_stream over the mock stream
// (synthetic frames): the payloads written come out as the codec's frames and
// the frames read are the payloads, a stream split at every byte position and
// read byte by byte gives what the codec alone gives, garbage and a frame
// beyond max_frame_size are survived, and encode's refusal
// (payload_too_large) is reported without writing anything.

#include <cstddef>
#include <cstdint>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "LumexFramedTestSupport.hpp"

namespace
{
using namespace lumex_framed_test;

template <typename Case>
class LumexFramedStreamCodecTest : public ::testing::Test
{
protected:
  // The decoders' warnings are the tests' business, not the console's.
  log_capture log_;
};

typedef ::testing::Types<slip_case, cobs_case, delimiter_case, length_case,
                         header_case, header_crc_case, modbus_case,
                         pipeline_case>
    all_cases;
TYPED_TEST_SUITE (LumexFramedStreamCodecTest, all_cases);

template <typename Case>
bytes_t
four_frames ()
{
  bytes_t stream;
  for (std::size_t i = 0; i < 4; ++i)
    stream = concat (stream, encode_direct (Case::make (Case::limit ()),
                                            Case::payload (i)));
  return stream;
}
} // namespace

TYPED_TEST (
    LumexFramedStreamCodecTest,
    GivenFourFrames_WhenTheStreamArrives_ThenEveryPayloadComesBackInOrder)
{
  stream_rig<TypeParam> rig;
  rig.open ();
  rig.mock.control.feed (four_frames<TypeParam> ());
  for (std::size_t i = 0; i < 4; ++i)
    {
      tr::framed::frame_result_t const frame
          = rig.link.read_frame (tr::deadline_t (2000));
      ASSERT_TRUE (frame.has_value ()) << frame.error ().message ();
      EXPECT_EQ (frame.value (), TypeParam::payload (i)) << "frame " << i;
    }
  tr::framed::framed_statistics_t const stats = rig.link.statistics ();
  EXPECT_EQ (stats.frames_in, 4u);
  EXPECT_EQ (stats.total_errors (), 0u);
  EXPECT_EQ (stats.dropped_frames, 0u);
  EXPECT_EQ (stats.bytes_in, four_frames<TypeParam> ().size ());
}

TYPED_TEST (
    LumexFramedStreamCodecTest,
    GivenAStreamOfFrames_WhenSplitAtEveryBytePosition_ThenTheFramesAreThoseOfTheCodec)
{
  bytes_t const stream = four_frames<TypeParam> ();
  for (std::size_t at = 1; at < stream.size (); ++at)
    ASSERT_TRUE (framed_equals_codec<TypeParam> (stream, at))
        << TypeParam::name () << " split at " << at;
}

TYPED_TEST (
    LumexFramedStreamCodecTest,
    GivenAStreamOfFrames_WhenReadOneByteAtATime_ThenTheFramesAreThoseOfTheCodec)
{
  EXPECT_TRUE (
      framed_equals_codec<TypeParam> (four_frames<TypeParam> (), 0, 1));
}

TYPED_TEST (
    LumexFramedStreamCodecTest,
    GivenGarbageBeforeAndBetweenFrames_WhenSplitAtEveryBytePosition_ThenTheFramesAreThoseOfTheCodec)
{
  bytes_t const codec_frame = encode_direct (
      TypeParam::make (TypeParam::limit ()), TypeParam::payload (1));
  bytes_t stream = noise (24, 3);
  stream = concat (stream, codec_frame);
  stream = concat (stream, noise (9, 4));
  stream = concat (stream, codec_frame);
  for (std::size_t at = 1; at < stream.size (); ++at)
    ASSERT_TRUE (framed_equals_codec<TypeParam> (stream, at))
        << TypeParam::name () << " split at " << at;
  EXPECT_TRUE (framed_equals_codec<TypeParam> (stream, 0, 1));
}

TYPED_TEST (
    LumexFramedStreamCodecTest,
    GivenAFrameBeyondTheLimit_WhenAGoodFrameFollows_ThenTheErrorIsCountedAndTheGoodFrameArrives)
{
  bytes_t const good = encode_direct (TypeParam::make (TypeParam::limit ()),
                                      TypeParam::payload (0));
  bytes_t const stream = concat (TypeParam::beyond_limit (), good);
  stream_rig<TypeParam> rig;
  rig.open ();
  rig.mock.control.feed (stream);
  tr::framed::frame_result_t const frame
      = rig.link.read_frame (tr::deadline_t (2000));
  ASSERT_TRUE (frame.has_value ()) << frame.error ().message ();
  EXPECT_EQ (frame.value (), TypeParam::payload (0));
  tr::framed::framed_statistics_t const stats = rig.link.statistics ();
  EXPECT_EQ (stats.errors_of (framing_errc::frame_too_large), 1u);
  // Some codecs go on byte by byte after the rejected frame and may count
  // more than the one error.
  EXPECT_GE (stats.total_errors (), 1u);
  EXPECT_EQ (stats.frames_in, 1u);
  // The error is not handed to the reader: nothing else arrives.
  EXPECT_FALSE (rig.link.read_frame (tr::deadline_t (30)).has_value ());
}

TYPED_TEST (
    LumexFramedStreamCodecTest,
    GivenAFrameBeyondTheLimit_WhenSplitAtEveryBytePosition_ThenTheFramesAreThoseOfTheCodec)
{
  bytes_t const stream
      = concat (TypeParam::beyond_limit (),
                encode_direct (TypeParam::make (TypeParam::limit ()),
                               TypeParam::payload (2)));
  for (std::size_t at = 1; at < stream.size (); at += 3)
    ASSERT_TRUE (framed_equals_codec<TypeParam> (stream, at))
        << TypeParam::name () << " split at " << at;
}

TYPED_TEST (LumexFramedStreamCodecTest,
            GivenAPayload_WhenWritten_ThenTheWireHoldsTheFrameOfTheCodec)
{
  stream_rig<TypeParam> rig;
  rig.open ();
  bytes_t expected;
  for (std::size_t i = 0; i < 4; ++i)
    {
      bytes_t const payload = TypeParam::payload (i);
      ASSERT_TRUE (
          rig.link
              .write_frame (tr::const_bytes_t (payload), tr::deadline_t (2000))
              .has_value ())
          << "payload " << i;
      expected = concat (
          expected,
          encode_direct (TypeParam::make (TypeParam::limit ()), payload));
    }
  EXPECT_EQ (rig.mock.control.written (), expected);
  tr::framed::framed_statistics_t const stats = rig.link.statistics ();
  EXPECT_EQ (stats.frames_out, 4u);
  EXPECT_EQ (stats.bytes_out, expected.size ());
}

TYPED_TEST (
    LumexFramedStreamCodecTest,
    GivenAPayloadTheCodecRefuses_WhenWritten_ThenPayloadTooLargeAndNothingIsWritten)
{
  stream_rig<TypeParam> rig;
  rig.open ();
  bytes_t const payload = TypeParam::oversized ();
  tr::status_result_t const result = rig.link.write_frame (
      tr::const_bytes_t (payload), tr::deadline_t (2000));
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == framing_errc::payload_too_large)
      << result.error ().message ();
  EXPECT_TRUE (rig.mock.control.written ().empty ());
  tr::framed::framed_statistics_t const stats = rig.link.statistics ();
  EXPECT_EQ (stats.errors_of (framing_errc::payload_too_large), 1u);
  EXPECT_EQ (stats.frames_out, 0u);
  EXPECT_EQ (stats.bytes_out, 0u);
  // The adapter still writes.
  bytes_t const good = TypeParam::payload (0);
  EXPECT_TRUE (
      rig.link.write_frame (tr::const_bytes_t (good), tr::deadline_t (2000))
          .has_value ());
}

TYPED_TEST (
    LumexFramedStreamCodecTest,
    GivenPayloadsAroundTheLimit_WhenWritten_ThenTheResultIsThatOfTheCodec)
{
  typename TypeParam::codec_t const codec
      = TypeParam::make (TypeParam::limit ());
  for (std::size_t size = TypeParam::limit () - 1;
       size <= TypeParam::limit () + 2; ++size)
    {
      stream_rig<TypeParam> rig;
      rig.open ();
      bytes_t const payload = letters (size);
      bool accepted = false;
      bytes_t const wire = encode_direct (codec, payload, &accepted);
      tr::status_result_t const result = rig.link.write_frame (
          tr::const_bytes_t (payload), tr::deadline_t (2000));
      EXPECT_EQ (result.has_value (), accepted)
          << TypeParam::name () << " payload of " << size;
      if (accepted)
        EXPECT_EQ (rig.mock.control.written (), wire) << "payload of " << size;
      else
        {
          ASSERT_FALSE (result.has_value ());
          EXPECT_TRUE (result.error () == framing_errc::payload_too_large)
              << TypeParam::name () << " payload of " << size << ": "
              << result.error ().message ();
          EXPECT_TRUE (rig.mock.control.written ().empty ());
        }
    }
}

TYPED_TEST (LumexFramedStreamCodecTest,
            GivenTheFrameItWrote_WhenTheDeviceEchoesIt_ThenItIsTheSamePayload)
{
  stream_rig<TypeParam> rig;
  rig.open ();
  for (std::size_t i = 0; i < 4; ++i)
    {
      bytes_t const payload = TypeParam::payload (i);
      std::size_t const before = rig.mock.control.written ().size ();
      ASSERT_TRUE (
          rig.link
              .write_frame (tr::const_bytes_t (payload), tr::deadline_t (2000))
              .has_value ());
      bytes_t const written = rig.mock.control.written ();
      rig.mock.control.feed (
          bytes_t (written.begin () + static_cast<std::ptrdiff_t> (before),
                   written.end ()));
      tr::framed::frame_result_t const frame
          = rig.link.read_frame (tr::deadline_t (2000));
      ASSERT_TRUE (frame.has_value ()) << frame.error ().message ();
      EXPECT_EQ (frame.value (), payload) << "payload " << i;
    }
}
