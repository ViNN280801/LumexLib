// The stages of lumex/core/framing/stage: prefix, ASCII hex, the check value
// algorithms and renderings with both policies, word reversal. The CRC-16 of
// the write request 01 06 2C C8 55 90 of device L is 98 3F on the wire,
// high byte first; the text frames are those of device J.
//
// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.

#include <cstddef>
#include <system_error>

#include <gtest/gtest.h>

#include "lumex/core/framing/stage/LumexFramingStages.hpp"

#include "lumex/tests/core/framing/LumexFramingTestSupport.hpp"

namespace
{
using namespace lumex_framing_test;
using lumex::core::framing::codec::byte_order;
using lumex::core::framing::codec::framing_errc;
namespace st = lumex::core::framing::stage;

using crc16_modbus_le
    = st::crc<lumex::core::crc::parametric::crc16_modbus_spec_t,
              byte_order::little>;

template <typename Stage>
bytes_t
encoded (Stage const &stage, bytes_t const &in)
{
  bytes_t out;
  auto const result = stage.encode (bytes_view_t (in), out);
  EXPECT_TRUE (result.has_value ());
  return out;
}

template <typename Stage>
std::error_code
decoded (Stage const &stage, bytes_t const &in, bytes_t &out)
{
  out.clear ();
  return stage.decode (bytes_view_t (in), out);
}
} // namespace

TEST (LumexFramingStagesTest,
      GivenAPrefixStage_WhenRoundTripped_ThenTheByteIsAddedAndRemoved)
{
  st::prefix_char_stage<'%'> const stage;
  bytes_t const frame = encoded (stage, bytes_of ("8D"));
  EXPECT_EQ (frame, bytes_of ("%8D"));
  bytes_t out;
  EXPECT_FALSE (decoded (stage, frame, out));
  EXPECT_EQ (out, bytes_of ("8D"));
}

TEST (LumexFramingStagesTest,
      GivenAWrongOrMissingPrefix_WhenDecoded_ThenBadHeader)
{
  st::prefix_char_stage<'%'> const stage;
  bytes_t out;
  EXPECT_TRUE (decoded (stage, bytes_of ("!8D"), out)
               == framing_errc::bad_header);
  EXPECT_TRUE (decoded (stage, bytes_t (), out) == framing_errc::bad_header);
}

TEST (LumexFramingStagesTest,
      GivenBytes_WhenHexEncoded_ThenTheDigitsAreUpperCase)
{
  st::ascii_hex_stage const stage;
  EXPECT_EQ (encoded (stage, bytes_t{ 0x00, 0x1D, 0xAB, 0xFF }),
             bytes_of ("001DABFF"));
}

TEST (LumexFramingStagesTest,
      GivenHexOfEitherCase_WhenDecoded_ThenTheBytesReturn)
{
  st::ascii_hex_stage const stage;
  bytes_t out;
  EXPECT_FALSE (decoded (stage, bytes_of ("001dAbfF"), out));
  EXPECT_EQ (out, (bytes_t{ 0x00, 0x1D, 0xAB, 0xFF }));
}

TEST (LumexFramingStagesTest, GivenBadHex_WhenDecoded_ThenTruncatedOrBadEscape)
{
  st::ascii_hex_stage const stage;
  bytes_t out;
  EXPECT_TRUE (decoded (stage, bytes_of ("ABC"), out)
               == framing_errc::truncated);
  EXPECT_TRUE (decoded (stage, bytes_of ("AG"), out)
               == framing_errc::bad_escape);
}

TEST (LumexFramingStagesTest,
      GivenTheAlgorithms_WhenComputed_ThenTheValuesAreKnown)
{
  bytes_t const data{ 0x38, 0x44 };
  EXPECT_EQ (st::xor8::compute (bytes_view_t (data)), 0x7Cu);
  EXPECT_EQ (st::sum8::compute (bytes_view_t (bytes_t{ 0xF0, 0x20 })), 0x10u);
  EXPECT_EQ (crc16_modbus_le::width (), 2u);
  EXPECT_EQ (crc16_modbus_le::compute (bytes_view_t (bytes_of ("123456789"))),
             0x4B37u);
}

TEST (LumexFramingStagesTest,
      GivenACrc16ModbusStage_WhenEncoded_ThenTheCrcIsLowByteFirst)
{
  // A read request of holding registers: CRC 0xCDC5, sent C5 CD.
  st::checksum_stage<crc16_modbus_le> const stage;
  EXPECT_EQ (encoded (stage, bytes_t{ 0x01, 0x03, 0x00, 0x00, 0x00, 0x0A }),
             (bytes_t{ 0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0xC5, 0xCD }));
}

TEST (LumexFramingStagesTest,
      GivenTheHighFirstFrames_WhenCheckedBigEndian_ThenTheyMatch)
{
  // Device L sends the Modbus CRC high byte first: the write request
  // 01 06 2C C8 55 90 98 3F and a read reply ending in 51 CF.
  using crc16_modbus_be
      = st::crc<lumex::core::crc::parametric::crc16_modbus_spec_t,
                byte_order::big>;
  st::checksum_stage<crc16_modbus_be> const stage;
  EXPECT_EQ (encoded (stage, bytes_t{ 0x01, 0x06, 0x2C, 0xC8, 0x55, 0x90 }),
             (bytes_t{ 0x01, 0x06, 0x2C, 0xC8, 0x55, 0x90, 0x98, 0x3F }));
  bytes_t reply{ 0x01, 0x03, 0x2E };
  bytes_t const data{ 0x66, 0xF4, 0x2C, 0x4E, 0x75, 0xF9, 0x75, 0xF9,
                      0x75, 0xF9, 0x75, 0xF9, 0x75, 0xF9, 0x75, 0xF9,
                      0x66, 0xF4, 0xD8, 0x67, 0x75, 0xF9, 0x75, 0xF9,
                      0x75, 0xF9, 0x75, 0xF9, 0x75, 0xF9, 0x75, 0xF9,
                      0x75, 0xF9, 0x75, 0xF9, 0x75, 0xF9, 0x75, 0xF9,
                      0xC0, 0x37, 0x75, 0xF9, 0x7E, 0x47 };
  reply = concat (reply, data);
  reply.push_back (0x51);
  reply.push_back (0xCF);
  bytes_t out;
  EXPECT_FALSE (decoded (stage, reply, out));
  EXPECT_EQ (out.size (), reply.size () - 2);
}

TEST (LumexFramingStagesTest, GivenARequiredCheck_WhenValid_ThenItIsRemoved)
{
  st::checksum_stage<crc16_modbus_le> const stage;
  bytes_t out;
  EXPECT_FALSE (decoded (
      stage, bytes_t{ 0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0xC5, 0xCD }, out));
  EXPECT_EQ (out, (bytes_t{ 0x01, 0x03, 0x00, 0x00, 0x00, 0x0A }));
}

TEST (LumexFramingStagesTest,
      GivenARequiredCheck_WhenWrongOrShort_ThenTheFrameIsRefused)
{
  st::checksum_stage<crc16_modbus_le> const stage;
  bytes_t out;
  EXPECT_TRUE (decoded (stage, bytes_t{ 0x01, 0x06, 0x98, 0x3E }, out)
               == framing_errc::bad_checksum);
  EXPECT_TRUE (decoded (stage, bytes_t{ 0x01 }, out)
               == framing_errc::truncated);
}

TEST (LumexFramingStagesTest,
      GivenAnAsciiHexRender_WhenEncoded_ThenTheCheckIsTwoDigits)
{
  st::checksum_stage<st::xor8, st::ascii_hex_render> const stage;
  EXPECT_EQ (encoded (stage, bytes_of ("8D")), bytes_of ("8D7C"));
  bytes_t out;
  EXPECT_FALSE (decoded (stage, bytes_of ("8D7C"), out));
  EXPECT_EQ (out, bytes_of ("8D"));
  EXPECT_FALSE (decoded (stage, bytes_of ("8D7C"), out));
  EXPECT_TRUE (decoded (stage, bytes_of ("8D7D"), out)
               == framing_errc::bad_checksum);
  EXPECT_TRUE (decoded (stage, bytes_of ("8DZZ"), out)
               == framing_errc::bad_checksum);
}

TEST (LumexFramingStagesTest,
      GivenAnOptionalCheck_WhenMissingOrWrong_ThenTheWholeFramePasses)
{
  st::checksum_stage<st::xor8, st::ascii_hex_render,
                     st::checksum_optional> const stage;
  bytes_t out;
  EXPECT_FALSE (decoded (stage, bytes_of ("8D7C"), out));
  EXPECT_EQ (out, bytes_of ("8D"));
  EXPECT_FALSE (decoded (stage, bytes_of ("AC00"), out));
  EXPECT_EQ (out, bytes_of ("AC00"));
  EXPECT_FALSE (decoded (stage, bytes_of ("1"), out));
  EXPECT_EQ (out, bytes_of ("1"));
}

TEST (LumexFramingStagesTest,
      GivenWords_WhenReversed_ThenEveryWordIsTurnedAround)
{
  st::word_reverse_stage<4> const stage;
  bytes_t const in{ 1, 2, 3, 4, 5, 6, 7, 8 };
  EXPECT_EQ (encoded (stage, in), (bytes_t{ 4, 3, 2, 1, 8, 7, 6, 5 }));
  bytes_t out;
  EXPECT_FALSE (decoded (stage, bytes_t{ 4, 3, 2, 1, 8, 7, 6, 5 }, out));
  EXPECT_EQ (out, in);
}

TEST (LumexFramingStagesTest, GivenAPartialWord_WhenReversed_ThenTruncated)
{
  st::word_reverse_stage<4> const stage;
  bytes_t out;
  EXPECT_TRUE (decoded (stage, bytes_t{ 1, 2, 3 }, out)
               == framing_errc::truncated);
  bytes_t const partial{ 1, 2, 3, 4, 5 };
  auto const result = stage.encode (bytes_view_t (partial), out);
  ASSERT_FALSE (result.has_value ());
  EXPECT_TRUE (result.error () == framing_errc::truncated);
}
