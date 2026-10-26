// delimiter_codec (lumex/core/framing/delimiter): the cases of the DChannel v4
// ASCII suite are ported (default CR, a custom multi-byte receive delimiter, a
// custom transmit suffix, split data, two frames in one chunk, a delimiter in
// the middle, a multi-byte delimiter fed byte by byte, reset); the v4 case
// "empty delimiters keep CR" is now a contract violation. The exchange of
// device M1 (LF out, CR LF ETB in) and the limits are added.
//
// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.

#include <cstddef>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

#include "lumex/core/framing/delimiter/LumexDelimiterCodec.hpp"

#include "lumex/tests/core/framing/LumexFramingTestSupport.hpp"

namespace
{
using namespace lumex_framing_test;
using lumex::core::framing::codec::framing_errc;
using lumex::core::framing::delimiter::delimiter_codec;
using lumex::core::framing::delimiter::delimiter_options_t;

byte_t const ACK = 0x06;
byte_t const ETB = 0x17;

delimiter_options_t
make_options (std::string const &rx, std::string const &tx,
              std::size_t max_frame_size = 65536)
{
  delimiter_options_t options;
  options.rx_delimiter = bytes_of (rx);
  options.tx_suffix = bytes_of (tx);
  options.max_frame_size = max_frame_size;
  return options;
}
} // namespace

TEST (LumexDelimiterCodecTest,
      GivenTheDefaultDelimiter_WhenACrTerminatedBufferIsFed_ThenTheCrIsCut)
{
  delimiter_codec codec;
  received_t const received = decode_all (codec, bytes_of ("PING\r"));
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], bytes_of ("PING"));
}

TEST (LumexDelimiterCodecTest,
      GivenTheDefaultSuffix_WhenEncoded_ThenCrIsAppended)
{
  delimiter_codec const codec;
  EXPECT_EQ (encode_ok (codec, bytes_of ("AB")), bytes_of ("AB\r"));
}

TEST (LumexDelimiterCodecTest,
      GivenACustomMultiByteRxDelimiter_WhenFed_ThenTheFrameEndsAtTheMatch)
{
  delimiter_codec codec (make_options ("\r\n", "\r\n"));
  received_t const received = decode_all (codec, bytes_of ("A\rB\r\n"));
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], bytes_of ("A\rB"));
}

TEST (LumexDelimiterCodecTest,
      GivenACustomTxSuffix_WhenEncoded_ThenItReplacesTheDefault)
{
  delimiter_codec const codec (make_options ("\r", "\n"));
  EXPECT_EQ (encode_ok (codec, bytes_of ("KI, 4")), bytes_of ("KI, 4\n"));
}

TEST (LumexDelimiterCodecTest,
      GivenDataSplitAcrossCalls_WhenFed_ThenTheFrameArrivesWithTheDelimiter)
{
  bytes_t const stream = bytes_of ("HELLO\r");
  for (std::size_t at = 0; at <= stream.size (); ++at)
    {
      delimiter_codec fresh;
      received_t const received = decode_split (fresh, stream, at);
      ASSERT_EQ (received.frames.size (), 1u) << "split at " << at;
      EXPECT_EQ (received.frames[0], bytes_of ("HELLO"));
    }
}

TEST (LumexDelimiterCodecTest,
      GivenTwoFramesInOneChunk_WhenFed_ThenTwoFramesArriveInOrder)
{
  delimiter_codec codec;
  received_t const received = decode_all (codec, bytes_of ("ONE\rTWO\r"));
  ASSERT_EQ (received.frames.size (), 2u);
  EXPECT_EQ (received.frames[0], bytes_of ("ONE"));
  EXPECT_EQ (received.frames[1], bytes_of ("TWO"));
}

TEST (LumexDelimiterCodecTest,
      GivenADelimiterInTheMiddle_WhenFed_ThenTheFrameEndsAtTheFirstOne)
{
  delimiter_codec codec;
  received_t const received = decode_all (codec, bytes_of ("AB\rCD"));
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], bytes_of ("AB"));
}

TEST (LumexDelimiterCodecTest,
      GivenAMultiByteDelimiterFedByteByByte_WhenFed_ThenItIsStillFound)
{
  delimiter_codec codec (make_options ("\r\n\x17", "\n"));
  received_t const received = decode_bytewise (codec, bytes_of ("12,34\r\n\x17"
                                                                "56\r\n\x17"));
  ASSERT_EQ (received.frames.size (), 2u);
  EXPECT_EQ (received.frames[0], bytes_of ("12,34"));
  EXPECT_EQ (received.frames[1], bytes_of ("56"));
}

TEST (LumexDelimiterCodecTest,
      GivenAPartialDelimiterThatBreaks_WhenFed_ThenItsBytesStayInTheFrame)
{
  // "\r\r\n": the first CR starts a match that the second CR breaks; the
  // prefix function keeps the second CR as the start of the real delimiter.
  delimiter_codec codec (make_options ("\r\n", "\r\n"));
  received_t const received = decode_all (codec, bytes_of ("A\r\r\n"));
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], bytes_of ("A\r"));
}

TEST (LumexDelimiterCodecTest,
      GivenAPartialFrame_WhenResetThenFed_ThenTheStaleDataIsDiscarded)
{
  delimiter_codec codec (make_options ("\r\n", "\r\n"));
  received_t received;
  recording_sink sink{ &received };
  bytes_t const stale = bytes_of ("STALE\r");
  codec.decode (bytes_view_t (stale), sink);
  codec.reset ();
  bytes_t const fresh = bytes_of ("\nNEW\r\n");
  codec.decode (bytes_view_t (fresh), sink);
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], bytes_of ("\nNEW"));
}

TEST (LumexDelimiterCodecTest,
      GivenTheExchangeOfDeviceM1_WhenEncodedAndDecoded_ThenBytesMatch)
{
  // Request XX, data<LF>; replies <ACK><CR><LF><ETB> and XX,data<CR><LF><ETB>.
  delimiter_codec codec (make_options ("\r\n\x17", "\n"));
  EXPECT_EQ (encode_ok (codec, bytes_of ("KI, 4")), bytes_of ("KI, 4\n"));

  bytes_t stream{ ACK, '\r', '\n', ETB };
  stream = concat (stream, bytes_of ("VS,6\r\n\x17"));
  received_t const received = decode_all (codec, stream);
  ASSERT_EQ (received.frames.size (), 2u);
  EXPECT_EQ (received.frames[0], (bytes_t{ ACK }));
  EXPECT_EQ (received.frames[1], bytes_of ("VS,6"));
}

TEST (LumexDelimiterCodecTest, GivenEmptyFrames_WhenFed_ThenTheyAreSkipped)
{
  delimiter_codec codec;
  received_t const received = decode_all (codec, bytes_of ("\r\r\rX\r\r"));
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], bytes_of ("X"));
}

TEST (LumexDelimiterCodecTest,
      GivenAPayloadAtAndPastTheLimit_WhenEncoded_ThenOnlyTheLimitPasses)
{
  delimiter_codec const codec (make_options ("\r", "\r", 4));
  EXPECT_EQ (encode_ok (codec, bytes_of ("ABCD")), bytes_of ("ABCD\r"));
  bytes_t out;
  bytes_t const payload = bytes_of ("ABCDE");
  auto const result = codec.encode (bytes_view_t (payload), out);
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == framing_errc::payload_too_large);
  EXPECT_TRUE (out.empty ());
}

TEST (LumexDelimiterCodecTest,
      GivenFramesAtAndPastTheLimit_WhenDecoded_ThenTheLongOneIsDroppedOnce)
{
  delimiter_codec codec (make_options ("\r\n", "\r\n", 4));
  received_t const received
      = decode_all (codec, bytes_of ("ABCD\r\nABCDEFGH\r\nOK\r\n"));
  ASSERT_EQ (received.frames.size (), 2u);
  EXPECT_EQ (received.frames[0], bytes_of ("ABCD"));
  EXPECT_EQ (received.frames[1], bytes_of ("OK"));
  ASSERT_EQ (received.errors.size (), 1u);
  EXPECT_TRUE (received.errors[0] == framing_errc::frame_too_large);
  EXPECT_EQ (received.order, "FEF");
}

TEST (LumexDelimiterCodecTest,
      GivenAFrameOneByteTooLong_WhenDecoded_ThenItIsDropped)
{
  delimiter_codec codec (make_options ("\r\n", "\r\n", 4));
  received_t const received = decode_all (codec, bytes_of ("ABCDE\r\nX\r\n"));
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], bytes_of ("X"));
  ASSERT_EQ (received.errors.size (), 1u);
}

TEST (LumexDelimiterCodecTest,
      GivenADroppedFrame_WhenItsDelimiterIsSplitAcrossCalls_ThenResyncHolds)
{
  delimiter_codec codec (make_options ("\r\n", "\r\n", 2));
  received_t received;
  recording_sink sink{ &received };
  bytes_t const first = bytes_of ("TOOLONG\r");
  bytes_t const second = bytes_of ("\nOK\r\n");
  codec.decode (bytes_view_t (first), sink);
  codec.decode (bytes_view_t (second), sink);
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], bytes_of ("OK"));
  EXPECT_EQ (received.order, "EF");
}

#if GTEST_HAS_DEATH_TEST && LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC >= 3
TEST (LumexDelimiterCodecDeathTest,
      GivenAnEmptyRxDelimiter_WhenConstructed_ThenTheContractStops)
{
  EXPECT_DEATH (delimiter_codec codec (make_options ("", "\r")), "");
}

TEST (LumexDelimiterCodecDeathTest,
      GivenAnEmptyTxSuffix_WhenConstructed_ThenTheContractStops)
{
  EXPECT_DEATH (delimiter_codec codec (make_options ("\r", "")), "");
}

TEST (LumexDelimiterCodecDeathTest,
      GivenAZeroLimit_WhenConstructed_ThenTheContractStops)
{
  EXPECT_DEATH (delimiter_codec codec (make_options ("\r", "\r", 0)), "");
}
#endif
