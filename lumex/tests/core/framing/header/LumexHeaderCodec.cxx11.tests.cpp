// header_codec (lumex/core/framing/header) with the frames of a binary
// device protocol (device M2): [PRE:2][VAR:2][SIZE:2][CMD:2][DATA][CRC32],
// little endian, one preamble to the device and another from it,
// CRC-32/MPEG-2 over PRE..DATA with every 4-byte word reversed, written
// little endian; a reply carries the code of its request. Six reference
// pairs of a command and its reply.
//
// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
// The field layout, the widths and the check algorithm are kept; the
// preambles, codes, strings and values are not those of a product, and
// the CRCs are recomputed.

#include <cstddef>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/framing/header/LumexHeaderCodec.hpp"

#include "lumex/tests/core/framing/LumexFramingTestSupport.hpp"

namespace
{
using namespace lumex_framing_test;
using lumex::core::framing::codec::byte_order;
using lumex::core::framing::codec::framing_errc;
using lumex::core::framing::header::header_codec;
using lumex::core::framing::header::header_options_t;
using lumex::core::framing::header::length_counts;
namespace st = lumex::core::framing::stage;

using word_crc = st::word_reversed<
    st::crc<lumex::core::crc::parametric::crc32_mpeg2_spec_t,
            byte_order::little>,
    4>;
using framed_codec = header_codec<word_crc>;

struct golden_t
{
  char const *command;
  char const *response;
};

golden_t const kGolden[] = {
  // 1
  { "A0E00000000059D1A127C942",
    "5E740000400059D151582D3533313042000000000000000052374B0051582D35333130420"
    "00000000000000039343457303331005100E3070911000000000000000000000000000093"
    "51CDAE" },
  // 2
  { "A0E00000000065914039E2BC",
    "5E740000140065913C424C414E4B3E20202020202020202020202000DA1B7490" },
  // 3
  { "A0E0000000003BF2103BD886",
    "5E74000014003BF23C424C414E4B3E0000000000000000000000000044E047FA" },
  // 4
  { "A0E000001400968E53544F524544204E4541522042454E434820340079171E56",
    "5E7400000000968E0139D117" },
  // 5
  { "A0E000000000B99A306F2FAB", "5E7400000400B99A0300000074078AB8" },
  // 6
  { "A0E000000400FA4400000000C1762DEB",
    "5E7400003C00FA4456455354412D303700000000000000003736373037393739373437333"
    "23636383434335834343038000000005100E30709111C00000000000000000036A08B6"
    "4" },
};

bytes_t
hex (std::string const &text)
{
  bytes_t out;
  for (std::size_t i = 0; i + 1 < text.size (); i += 2)
    out.push_back (
        static_cast<byte_t> (std::stoul (text.substr (i, 2), nullptr, 16)));
  return out;
}

// The payload of a frame: VAR, CMD and DATA (without PRE, SIZE and CRC).
bytes_t
payload_of (bytes_t const &frame)
{
  bytes_t payload (frame.begin () + 2, frame.begin () + 4);
  payload.insert (payload.end (), frame.begin () + 6, frame.end () - 4);
  return payload;
}

header_options_t
make_options (bytes_t const &tx, bytes_t const &rx,
              std::size_t max_frame_size = 65536)
{
  header_options_t options;
  options.tx_preamble = tx;
  options.rx_preamble = rx;
  options.header_size = 8;
  options.length_offset = 4;
  options.length_width = 2;
  options.length_order = byte_order::little;
  options.counts = length_counts::data;
  options.check_from = 0;
  options.max_frame_size = max_frame_size;
  return options;
}

header_options_t
host_options (std::size_t max_frame_size = 65536)
{
  return make_options (bytes_t{ 0xA0, 0xE0 }, bytes_t{ 0x5E, 0x74 },
                       max_frame_size);
}

header_options_t
device_options ()
{
  return make_options (bytes_t{ 0x5E, 0x74 }, bytes_t{ 0xA0, 0xE0 });
}
} // namespace

TEST (LumexHeaderCodecTest,
      GivenTheWordReversedCrc_WhenComputed_ThenItMatchesTheReference)
{
  EXPECT_EQ (word_crc::compute (bytes_view_t (bytes_of ("12345678"))),
             0xFEFC54F9u);
}

TEST (LumexHeaderCodecTest,
      GivenEveryReferenceCommand_WhenEncoded_ThenTheBytesMatch)
{
  framed_codec const host (host_options ());
  for (golden_t const &golden : kGolden)
    {
      bytes_t const frame = hex (golden.command);
      EXPECT_EQ (encode_ok (host, payload_of (frame)), frame)
          << golden.command;
    }
}

TEST (LumexHeaderCodecTest,
      GivenEveryReferenceResponse_WhenDecoded_ThenThePayloadArrives)
{
  framed_codec host (host_options ());
  for (golden_t const &golden : kGolden)
    {
      bytes_t const frame = hex (golden.response);
      received_t const received = decode_all (host, frame);
      ASSERT_EQ (received.frames.size (), 1u) << golden.response;
      EXPECT_EQ (received.frames[0], payload_of (frame));
      EXPECT_TRUE (received.errors.empty ());
    }
}

TEST (LumexHeaderCodecTest,
      GivenTheReferenceCommands_WhenTheDeviceDecodes_ThenEveryOneArrives)
{
  framed_codec device (device_options ());
  bytes_t stream;
  for (golden_t const &golden : kGolden)
    stream = concat (stream, hex (golden.command));
  received_t const received = decode_all (device, stream);
  ASSERT_EQ (received.frames.size (), sizeof (kGolden) / sizeof (kGolden[0]));
  EXPECT_EQ (received.frames[3], payload_of (hex (kGolden[3].command)));
}

TEST (LumexHeaderCodecTest,
      GivenAResponseSplitAtEveryPosition_WhenDecoded_ThenItArrivesOnce)
{
  bytes_t const frame = hex (kGolden[5].response);
  for (std::size_t at = 0; at <= frame.size (); ++at)
    {
      framed_codec host (host_options ());
      received_t const received = decode_split (host, frame, at);
      ASSERT_EQ (received.frames.size (), 1u) << "split at " << at;
      EXPECT_EQ (received.frames[0], payload_of (frame));
    }
}

TEST (LumexHeaderCodecTest,
      GivenNoiseBeforeAFrame_WhenDecoded_ThenTheNoiseIsDroppedQuietly)
{
  framed_codec host (host_options ());
  bytes_t const stream = concat (bytes_t{ 0x00, 0x5E, 0x55, 0x74, 0x5E },
                                 hex (kGolden[4].response));
  received_t const received = decode_all (host, stream);
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_TRUE (received.errors.empty ());
}

TEST (LumexHeaderCodecTest,
      GivenABadCrc_WhenDecoded_ThenBadChecksumAndTheNextFrameArrives)
{
  framed_codec host (host_options ());
  bytes_t broken = hex (kGolden[4].response);
  broken.back () ^= 0x01;
  received_t const received
      = decode_all (host, concat (broken, hex (kGolden[1].response)));
  ASSERT_EQ (received.errors.size (), 1u);
  EXPECT_TRUE (received.errors[0] == framing_errc::bad_checksum);
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], payload_of (hex (kGolden[1].response)));
  EXPECT_EQ (received.order, "EF");
}

TEST (LumexHeaderCodecTest,
      GivenALengthAboveTheLimit_WhenDecoded_ThenFrameTooLargeAndResync)
{
  framed_codec host (host_options (16));
  received_t const received = decode_all (
      host, concat (hex (kGolden[0].response), hex (kGolden[4].response)));
  ASSERT_GE (received.errors.size (), 1u);
  EXPECT_TRUE (received.errors[0] == framing_errc::frame_too_large);
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], payload_of (hex (kGolden[4].response)));
}

TEST (LumexHeaderCodecTest,
      GivenDataAboveTheLimit_WhenEncoded_ThenPayloadTooLarge)
{
  framed_codec const host (host_options (4));
  bytes_t out;
  bytes_t const payload (4 + 8, 0x11);
  auto const result = host.encode (bytes_view_t (payload), out);
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == framing_errc::payload_too_large);
  EXPECT_TRUE (out.empty ());
}

TEST (LumexHeaderCodecTest,
      GivenAPayloadShorterThanTheFields_WhenEncoded_ThenTruncated)
{
  framed_codec const host (host_options ());
  bytes_t out;
  bytes_t const payload{ 0x00, 0x00, 0x00 };
  auto const result = host.encode (bytes_view_t (payload), out);
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == framing_errc::truncated);
}

TEST (LumexHeaderCodecTest,
      GivenAWholeFrameLength_WhenRoundTripped_ThenTheFieldCountsEverything)
{
  header_options_t options;
  options.tx_preamble = bytes_t{ 0x7E };
  options.rx_preamble = bytes_t{ 0x7E };
  options.header_size = 3;
  options.length_offset = 1;
  options.length_width = 1;
  options.counts = length_counts::frame;
  options.check_from = 1;
  header_codec<st::xor8> codec (options);
  bytes_t const frame = encode_ok (codec, bytes_t{ 0x05, 0xA1, 0xA2 });
  // 7E, length 6 (3 header + 2 data + 1 check), 05, A1 A2, XOR of 06 05 A1 A2.
  EXPECT_EQ (frame, (bytes_t{ 0x7E, 0x06, 0x05, 0xA1, 0xA2,
                              0x06 ^ 0x05 ^ 0xA1 ^ 0xA2 }));
  received_t const received = decode_all (codec, frame);
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], (bytes_t{ 0x05, 0xA1, 0xA2 }));
}

TEST (LumexHeaderCodecTest,
      GivenAWholeFrameLengthBelowTheHeader_WhenDecoded_ThenBadHeader)
{
  header_options_t options;
  options.tx_preamble = bytes_t{ 0x7E };
  options.rx_preamble = bytes_t{ 0x7E };
  options.header_size = 2;
  options.length_offset = 1;
  options.length_width = 1;
  options.counts = length_counts::frame;
  header_codec<> codec (options);
  received_t const received
      = decode_all (codec, bytes_t{ 0x7E, 0x01, 0x7E, 0x03, 0x42 });
  ASSERT_EQ (received.errors.size (), 1u);
  EXPECT_TRUE (received.errors[0] == framing_errc::bad_header);
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], (bytes_t{ 0x42 }));
}

TEST (LumexHeaderCodecTest, GivenAPartialFrame_WhenReset_ThenItIsForgotten)
{
  framed_codec host (host_options ());
  received_t received;
  recording_sink sink{ &received };
  bytes_t const frame = hex (kGolden[1].response);
  host.decode (bytes_view_t (frame.data (), 10), sink);
  host.reset ();
  host.decode (bytes_view_t (frame), sink);
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_TRUE (received.errors.empty ());
}

#if GTEST_HAS_DEATH_TEST && LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC >= 3
TEST (LumexHeaderCodecDeathTest,
      GivenPreamblesOfDifferentLength_WhenConstructed_ThenTheContractStops)
{
  header_options_t options = host_options ();
  options.rx_preamble = bytes_t{ 0x5E };
  EXPECT_DEATH (framed_codec codec (options), "");
}

TEST (LumexHeaderCodecDeathTest,
      GivenALengthFieldOutsideTheHeader_WhenConstructed_ThenTheContractStops)
{
  header_options_t options = host_options ();
  options.length_offset = 7;
  EXPECT_DEATH (framed_codec codec (options), "");
}
#endif
