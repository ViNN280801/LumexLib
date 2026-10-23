// The helpers every frame codec shares (lumex/core/framing/codec): the two
// sink forms, encode_failure and the bounded frame buffer.

#include <cstddef>
#include <system_error>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/framing/LumexFraming"

namespace
{
namespace codec = lumex::core::framing::codec;
using codec::bytes_t;
using codec::bytes_view_t;
using codec::framing_errc;

struct frame_only_sink
{
  std::vector<bytes_t> *frames;

  void
  operator() (bytes_view_t frame) const
  {
    frames->push_back (bytes_t (frame.begin (), frame.end ()));
  }
};

struct frame_and_error_sink
{
  std::vector<bytes_t> *frames;
  std::vector<std::error_code> *errors;

  void
  operator() (bytes_view_t frame, std::error_code error) const
  {
    frames->push_back (bytes_t (frame.begin (), frame.end ()));
    errors->push_back (error);
  }
};
} // namespace

TEST (LumexFramingCodecTest,
      GivenAFrameOnlySink_WhenAFrameIsDelivered_ThenItReceivesTheBytes)
{
  std::vector<bytes_t> frames;
  frame_only_sink sink{ &frames };
  bytes_t const payload{ 1, 2, 3 };

  codec::detail::deliver (sink, bytes_view_t (payload));

  ASSERT_EQ (frames.size (), 1u);
  EXPECT_EQ (frames[0], payload);
}

TEST (LumexFramingCodecTest,
      GivenAFrameOnlySink_WhenAnErrorIsDelivered_ThenItIsNotCalled)
{
  std::vector<bytes_t> frames;
  frame_only_sink sink{ &frames };

  codec::detail::deliver (sink, framing_errc::bad_checksum);

  EXPECT_TRUE (frames.empty ());
}

TEST (LumexFramingCodecTest,
      GivenAnErrorSink_WhenAFrameIsDelivered_ThenTheErrorIsEmpty)
{
  std::vector<bytes_t> frames;
  std::vector<std::error_code> errors;
  frame_and_error_sink sink{ &frames, &errors };
  bytes_t const payload{ 7 };

  codec::detail::deliver (sink, bytes_view_t (payload));

  ASSERT_EQ (errors.size (), 1u);
  EXPECT_FALSE (errors[0]);
  EXPECT_EQ (frames[0], payload);
}

TEST (LumexFramingCodecTest,
      GivenAnErrorSink_WhenAnErrorIsDelivered_ThenItGetsAnEmptyFrameAndTheCode)
{
  std::vector<bytes_t> frames;
  std::vector<std::error_code> errors;
  frame_and_error_sink sink{ &frames, &errors };

  codec::detail::deliver (sink, framing_errc::frame_too_large);

  ASSERT_EQ (errors.size (), 1u);
  EXPECT_TRUE (errors[0] == framing_errc::frame_too_large);
  EXPECT_TRUE (frames[0].empty ());
}

TEST (LumexFramingCodecTest, GivenALambdaSink_WhenDetected_ThenTheFormIsRight)
{
  auto frame_only = [] (bytes_view_t) {};
  auto with_error = [] (bytes_view_t, std::error_code) {};
  EXPECT_FALSE (codec::detail::sink_takes_error<decltype (frame_only)>::value);
  EXPECT_TRUE (codec::detail::sink_takes_error<decltype (with_error)>::value);
}

TEST (LumexFramingCodecTest,
      GivenAnError_WhenEncodeFailureIsMade_ThenTheResultHoldsTheCode)
{
  codec::encode_result_t const result
      = codec::encode_failure (framing_errc::payload_too_large);
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == framing_errc::payload_too_large);
}

TEST (LumexFramingCodecTest,
      GivenTheDefaultOptions_WhenRead_ThenTheLimitIs64KiB)
{
  codec::codec_options_t const options;
  EXPECT_EQ (options.max_frame_size, 65536u);
  EXPECT_EQ (codec::Constants::KDEFAULT_MAX_FRAME_SIZE, 65536u);
}

TEST (LumexFramingCodecTest,
      GivenABufferOfThree_WhenThreeBytesArePushed_ThenAllAreKept)
{
  codec::detail::frame_buffer buffer (3);
  EXPECT_TRUE (buffer.push (1));
  EXPECT_TRUE (buffer.push (2));
  EXPECT_TRUE (buffer.push (3));
  EXPECT_EQ (buffer.size (), 3u);
  EXPECT_FALSE (buffer.dropping ());
  EXPECT_EQ (bytes_t (buffer.view ().begin (), buffer.view ().end ()),
             (bytes_t{ 1, 2, 3 }));
}

TEST (LumexFramingCodecTest,
      GivenAFullBuffer_WhenOneMoreByteIsPushed_ThenItDropsUntilCleared)
{
  codec::detail::frame_buffer buffer (2);
  ASSERT_TRUE (buffer.push (1));
  ASSERT_TRUE (buffer.push (2));

  EXPECT_FALSE (buffer.push (3));
  EXPECT_TRUE (buffer.dropping ());
  EXPECT_TRUE (buffer.empty ());
  EXPECT_FALSE (buffer.push (4));

  buffer.clear ();
  EXPECT_FALSE (buffer.dropping ());
  EXPECT_TRUE (buffer.push (5));
  EXPECT_EQ (buffer.size (), 1u);
}

TEST (LumexFramingCodecTest,
      GivenABufferOfOne_WhenTheLimitIsReached_ThenTheMaxSizeIsReported)
{
  codec::detail::frame_buffer buffer (1);
  EXPECT_EQ (buffer.max_size (), 1u);
  EXPECT_TRUE (buffer.push (9));
  EXPECT_FALSE (buffer.push (9));
}

// The byte order of the codecs is the shared one of
// lumex/core/utility/bit/LumexByteOrder.hpp (the protocol fields use it
// without the framing module); the old spelling still works.
TEST (LumexFramingCodecTest,
      GivenTheByteOrderOfTheCodecs_WhenNamed_ThenItIsTheSharedUtilityType)
{
  EXPECT_TRUE ((std::is_same<codec::byte_order,
                             lumex::core::utility::bit::byte_order>::value));
  EXPECT_TRUE (codec::byte_order::big
               == lumex::core::utility::bit::byte_order::big);
}
