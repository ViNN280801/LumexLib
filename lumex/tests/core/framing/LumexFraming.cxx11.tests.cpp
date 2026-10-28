// Properties every codec of lumex/core/framing keeps, checked through the
// umbrella header for each codec and for the set-ups of devices J and M2
// (the ideas of a protocol invariants suite): random payloads
// round trip; a stream of several frames gives the same frames however it is
// split; decode consumes exactly what it is given and never throws, for a
// valid frame cut at every length and for one with any single byte damaged;
// a strict prefix of a frame delivers nothing; a codec with a check value
// never delivers a damaged frame.
//
// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
// Devices J and M2 are those of the stage and header tests.

#include <cstddef>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/framing/LumexFraming"

#include "lumex/tests/core/framing/LumexFramingTestSupport.hpp"

namespace
{
using namespace lumex_framing_test;
namespace fr = lumex::core::framing;
namespace st = lumex::core::framing::stage;

bytes_t
random_bytes (std::mt19937 &rng, std::size_t size, byte_t avoid = 0,
              bool use_avoid = false)
{
  std::uniform_int_distribution<int> byte (0, 255);
  bytes_t out;
  while (out.size () < size)
    {
      byte_t const value = static_cast<byte_t> (byte (rng));
      if (!use_avoid || value != avoid)
        out.push_back (value);
    }
  return out;
}

struct slip_case
{
  typedef fr::slip::slip_codec codec_t;
  static codec_t
  make ()
  {
    return codec_t ();
  }
  static bool
  checked ()
  {
    return false;
  }
  static bytes_t
  payload (std::mt19937 &rng, std::size_t size)
  {
    return random_bytes (rng, size + 1);
  }
};

struct cobs_case
{
  typedef fr::cobs::cobs_codec codec_t;
  static codec_t
  make ()
  {
    return codec_t ();
  }
  static bool
  checked ()
  {
    return false;
  }
  static bytes_t
  payload (std::mt19937 &rng, std::size_t size)
  {
    return random_bytes (rng, size);
  }
};

struct delimiter_case
{
  typedef fr::delimiter::delimiter_codec codec_t;
  static codec_t
  make ()
  {
    fr::delimiter::delimiter_options_t options;
    options.rx_delimiter = bytes_of ("\r\n");
    options.tx_suffix = bytes_of ("\r\n");
    return codec_t (options);
  }
  static bool
  checked ()
  {
    return false;
  }
  static bytes_t
  payload (std::mt19937 &rng, std::size_t size)
  {
    return random_bytes (rng, size + 1, '\r', true);
  }
};

struct length_case
{
  typedef fr::length::length_prefix_codec codec_t;
  static codec_t
  make ()
  {
    fr::length::length_prefix_options_t options;
    options.width = 2;
    return codec_t (options);
  }
  static bool
  checked ()
  {
    return false;
  }
  static bytes_t
  payload (std::mt19937 &rng, std::size_t size)
  {
    return random_bytes (rng, size);
  }
};

struct header_case
{
  typedef fr::header::header_codec<st::word_reversed<
      st::crc<lumex::core::crc::parametric::crc32_mpeg2_spec_t,
              fr::codec::byte_order::little>,
      4>>
      codec_t;
  static codec_t
  make ()
  {
    fr::header::header_options_t options;
    options.tx_preamble = bytes_t{ 0xA0, 0xE0 };
    options.rx_preamble = bytes_t{ 0xA0, 0xE0 };
    options.header_size = 8;
    options.length_offset = 4;
    return codec_t (options);
  }
  static bool
  checked ()
  {
    return true;
  }
  static bytes_t
  payload (std::mt19937 &rng, std::size_t size)
  {
    return random_bytes (rng, 4 + 4 * (size / 4));
  }
};

struct modbus_case
{
  typedef fr::modbus_rtu::modbus_rtu_codec codec_t;
  static codec_t
  make ()
  {
    return codec_t ();
  }
  static bool
  checked ()
  {
    return true;
  }
  static bytes_t
  payload (std::mt19937 &rng, std::size_t size)
  {
    std::size_t const count = size % 200;
    bytes_t out{ 0x01, 0x03, static_cast<byte_t> (count) };
    return concat (out, random_bytes (rng, count));
  }
};

typedef st::pipeline<
    fr::delimiter::delimiter_codec, st::prefix_char_stage<'%'>,
    st::checksum_stage<st::xor8, st::ascii_hex_render>, st::ascii_hex_stage>
    text_strict_t;

struct text_case
{
  typedef text_strict_t codec_t;
  static codec_t
  make ()
  {
    return codec_t ();
  }
  static bool
  checked ()
  {
    return true;
  }
  static bytes_t
  payload (std::mt19937 &rng, std::size_t size)
  {
    return random_bytes (rng, size + 1);
  }
};

typedef st::pipeline<
    fr::delimiter::delimiter_codec, st::prefix_char_stage<'%'>,
    st::checksum_stage<st::xor8, st::ascii_hex_render, st::checksum_optional>,
    st::ascii_hex_stage>
    text_lenient_t;

struct text_lenient_case
{
  typedef text_lenient_t codec_t;
  static codec_t
  make ()
  {
    return codec_t ();
  }
  static bool
  checked ()
  {
    return false;
  }
  static bytes_t
  payload (std::mt19937 &rng, std::size_t size)
  {
    return random_bytes (rng, size + 1);
  }
};

template <typename Case>
class LumexFramingInvariantTest : public ::testing::Test
{
};

typedef ::testing::Types<slip_case, cobs_case, delimiter_case, length_case,
                         header_case, modbus_case, text_case,
                         text_lenient_case>
    every_case;
TYPED_TEST_SUITE (LumexFramingInvariantTest, every_case);

std::size_t const kSizes[] = { 0, 1, 2, 3, 7, 16, 63, 64, 255, 300 };

// Feeds a stream; checks that every call consumes what it is given.
template <typename Codec>
received_t
feed (Codec &codec, bytes_t const &stream)
{
  received_t received;
  recording_sink sink{ &received };
  std::size_t const consumed = codec.decode (bytes_view_t (stream), sink);
  EXPECT_EQ (consumed, stream.size ());
  return received;
}
} // namespace

TYPED_TEST (LumexFramingInvariantTest,
            GivenRandomPayloads_WhenRoundTripped_ThenEachArrivesWhole)
{
  std::mt19937 rng (42);
  for (std::size_t size : kSizes)
    {
      typename TypeParam::codec_t codec = TypeParam::make ();
      bytes_t const payload = TypeParam::payload (rng, size);
      bytes_t const frame = encode_ok (codec, payload);
      ASSERT_FALSE (frame.empty ()) << size;
      received_t const received = feed (codec, frame);
      ASSERT_EQ (received.frames.size (), 1u) << size;
      EXPECT_EQ (received.frames[0], payload) << size;
      EXPECT_TRUE (received.errors.empty ()) << size;
    }
}

TYPED_TEST (LumexFramingInvariantTest,
            GivenThreeFrames_WhenSplitAtEveryPosition_ThenTheSameFramesArrive)
{
  std::mt19937 rng (7);
  typename TypeParam::codec_t const encoder = TypeParam::make ();
  std::vector<bytes_t> payloads;
  bytes_t stream;
  for (std::size_t size : { 5u, 0u, 12u })
    {
      payloads.push_back (TypeParam::payload (rng, size));
      stream = concat (stream, encode_ok (encoder, payloads.back ()));
    }
  for (std::size_t at = 0; at <= stream.size (); ++at)
    {
      typename TypeParam::codec_t codec = TypeParam::make ();
      received_t const received = decode_split (codec, stream, at);
      ASSERT_EQ (received.frames, payloads) << "split at " << at;
    }
}

TYPED_TEST (
    LumexFramingInvariantTest,
    GivenAFrameCutAtEveryLength_WhenDecoded_ThenNothingArrivesAndNothingThrows)
{
  std::mt19937 rng (11);
  typename TypeParam::codec_t const encoder = TypeParam::make ();
  bytes_t const frame = encode_ok (encoder, TypeParam::payload (rng, 20));
  for (std::size_t length = 0; length < frame.size (); ++length)
    {
      typename TypeParam::codec_t codec = TypeParam::make ();
      bytes_t const prefix (frame.begin (),
                            frame.begin ()
                                + static_cast<std::ptrdiff_t> (length));
      received_t received;
      EXPECT_NO_THROW (received = feed (codec, prefix)) << length;
      EXPECT_TRUE (received.frames.empty ()) << "cut at " << length;
    }
}

TYPED_TEST (
    LumexFramingInvariantTest,
    GivenAFrameWithAnyByteDamaged_WhenDecoded_ThenNothingThrowsAndChecksHold)
{
  std::mt19937 rng (13);
  typename TypeParam::codec_t const encoder = TypeParam::make ();
  bytes_t const payload = TypeParam::payload (rng, 20);
  bytes_t const frame = encode_ok (encoder, payload);
  for (std::size_t at = 0; at < frame.size (); ++at)
    {
      bytes_t damaged = frame;
      damaged[at] = static_cast<byte_t> (damaged[at] ^ 0x5A);
      typename TypeParam::codec_t codec = TypeParam::make ();
      received_t received;
      EXPECT_NO_THROW (received = feed (codec, damaged)) << at;
      if (TypeParam::checked ())
        for (bytes_t const &delivered : received.frames)
          EXPECT_EQ (delivered, payload) << "damaged at " << at;
    }
}

TYPED_TEST (LumexFramingInvariantTest,
            GivenAResetBetweenFrames_WhenDecoded_ThenTheNextFrameStillArrives)
{
  std::mt19937 rng (17);
  typename TypeParam::codec_t codec = TypeParam::make ();
  bytes_t const payload = TypeParam::payload (rng, 9);
  bytes_t const frame = encode_ok (codec, payload);
  feed (codec, bytes_t (frame.begin (), frame.begin () + 3));
  codec.reset ();
  received_t const received = feed (codec, frame);
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], payload);
}
