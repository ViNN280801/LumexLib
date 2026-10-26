// modbus_rtu_codec (lumex/core/framing/modbus_rtu): standard frames (CRC low
// byte first) and the frames of a device that sends its CRC high byte first
// (device L): the write request 01 06 2C C8 55 90 98 3F and a read reply
// that ends in 51 CF.
//
// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.

#include <cstddef>
#include <system_error>

#include <gtest/gtest.h>

#include "lumex/core/framing/modbus_rtu/LumexModbusRtuCodec.hpp"

#include "lumex/tests/core/framing/LumexFramingTestSupport.hpp"

namespace
{
using namespace lumex_framing_test;
using lumex::core::framing::codec::byte_order;
using lumex::core::framing::codec::framing_errc;
using lumex::core::framing::modbus_rtu::modbus_rtu_codec;
using lumex::core::framing::modbus_rtu::modbus_rtu_options_t;
using lumex::core::framing::modbus_rtu::modbus_rtu_requests;

modbus_rtu_options_t
high_first (bool requests)
{
  modbus_rtu_options_t options;
  options.crc_order = byte_order::big;
  if (requests)
    options.functions = modbus_rtu_requests ();
  return options;
}

modbus_rtu_options_t
standard_requests ()
{
  modbus_rtu_options_t options;
  options.functions = modbus_rtu_requests ();
  return options;
}

// 01 03 2E, 46 data bytes, CRC 51 CF (high byte first).
bytes_t
high_first_read_reply ()
{
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
  return reply;
}
} // namespace

TEST (LumexModbusRtuCodecTest,
      GivenAStandardRequest_WhenEncoded_ThenTheCrcIsLowByteFirst)
{
  modbus_rtu_codec const codec;
  EXPECT_EQ (encode_ok (codec, bytes_t{ 0x01, 0x03, 0x00, 0x00, 0x00, 0x0A }),
             (bytes_t{ 0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0xC5, 0xCD }));
}

TEST (LumexModbusRtuCodecTest,
      GivenTheHighFirstWriteRequest_WhenEncoded_ThenTheStoredBytesResult)
{
  modbus_rtu_codec const codec (high_first (true));
  EXPECT_EQ (encode_ok (codec, bytes_t{ 0x01, 0x06, 0x2C, 0xC8, 0x55, 0x90 }),
             (bytes_t{ 0x01, 0x06, 0x2C, 0xC8, 0x55, 0x90, 0x98, 0x3F }));
}

TEST (LumexModbusRtuCodecTest,
      GivenTheHighFirstWriteRequest_WhenTheDeviceDecodes_ThenItArrives)
{
  modbus_rtu_codec codec (high_first (true));
  received_t const received = decode_all (
      codec, bytes_t{ 0x01, 0x06, 0x2C, 0xC8, 0x55, 0x90, 0x98, 0x3F });
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0],
             (bytes_t{ 0x01, 0x06, 0x2C, 0xC8, 0x55, 0x90 }));
}

TEST (LumexModbusRtuCodecTest,
      GivenTheHighFirstReadReply_WhenDecoded_ThenTheByteCountGivesItsLength)
{
  modbus_rtu_codec codec (high_first (false));
  bytes_t const reply = high_first_read_reply ();
  received_t const received = decode_all (codec, concat (reply, reply));
  ASSERT_EQ (received.frames.size (), 2u);
  EXPECT_EQ (received.frames[0].size (), 3u + 0x2Eu);
  EXPECT_TRUE (received.errors.empty ());
}

TEST (LumexModbusRtuCodecTest,
      GivenAnExceptionReply_WhenDecoded_ThenItIsFiveBytes)
{
  modbus_rtu_codec codec;
  received_t const received
      = decode_all (codec, bytes_t{ 0x01, 0x83, 0x02, 0xC0, 0xF1 });
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], (bytes_t{ 0x01, 0x83, 0x02 }));
}

TEST (
    LumexModbusRtuCodecTest,
    GivenAWriteMultipleRequest_WhenTheDeviceDecodes_ThenItsCountIsAtOffsetSix)
{
  modbus_rtu_codec codec (standard_requests ());
  bytes_t const payload{ 0x01, 0x10, 0x00, 0x01, 0x00, 0x02,
                         0x04, 0x00, 0x01, 0x00, 0x02 };
  bytes_t const frame = encode_ok (codec, payload);
  EXPECT_EQ (frame, concat (payload, bytes_t{ 0xE2, 0x62 }));
  received_t const received = decode_all (codec, frame);
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0], payload);
}

TEST (LumexModbusRtuCodecTest,
      GivenAWrongCrc_WhenDecoded_ThenBadChecksumAndTheNextFrameArrives)
{
  modbus_rtu_codec codec (high_first (true));
  bytes_t const stream{ 0x01, 0x06, 0x2C, 0xC8, 0x55, 0x90, 0x3F, 0x98,
                        0x01, 0x06, 0x2C, 0xC8, 0x55, 0x90, 0x98, 0x3F };
  received_t const received = decode_all (codec, stream);
  ASSERT_GE (received.errors.size (), 1u);
  EXPECT_TRUE (received.errors[0] == framing_errc::bad_checksum);
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0],
             (bytes_t{ 0x01, 0x06, 0x2C, 0xC8, 0x55, 0x90 }));
}

TEST (LumexModbusRtuCodecTest,
      GivenAnUnknownFunction_WhenDecoded_ThenBadHeader)
{
  modbus_rtu_codec codec;
  received_t const received = decode_all (codec, bytes_t{ 0x01, 0x2B });
  ASSERT_EQ (received.errors.size (), 1u);
  EXPECT_TRUE (received.errors[0] == framing_errc::bad_header);
}

TEST (LumexModbusRtuCodecTest,
      GivenAFrameOfAnotherAddress_WhenFiltered_ThenItIsDroppedQuietly)
{
  modbus_rtu_options_t options = standard_requests ();
  options.filter_address = true;
  options.address = 0x02;
  modbus_rtu_codec codec (options);
  modbus_rtu_codec const sender (standard_requests ());
  bytes_t const stream = concat (
      encode_ok (sender, bytes_t{ 0x01, 0x06, 0x00, 0x01, 0x00, 0x02 }),
      encode_ok (sender, bytes_t{ 0x02, 0x06, 0x00, 0x01, 0x00, 0x03 }));
  received_t const received = decode_all (codec, stream);
  EXPECT_TRUE (received.errors.empty ());
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_EQ (received.frames[0][0], 0x02);
}

TEST (LumexModbusRtuCodecTest,
      GivenAReplySplitAtEveryPosition_WhenDecoded_ThenItArrivesOnce)
{
  bytes_t const reply = high_first_read_reply ();
  for (std::size_t at = 0; at <= reply.size (); ++at)
    {
      modbus_rtu_codec codec (high_first (false));
      received_t const received = decode_split (codec, reply, at);
      ASSERT_EQ (received.frames.size (), 1u) << "split at " << at;
    }
}

TEST (LumexModbusRtuCodecTest,
      GivenAByteCountAboveTheLimit_WhenDecoded_ThenFrameTooLarge)
{
  modbus_rtu_options_t options;
  options.max_frame_size = 16;
  modbus_rtu_codec codec (options);
  received_t const received = decode_all (codec, bytes_t{ 0x01, 0x03, 0x40 });
  ASSERT_GE (received.errors.size (), 1u);
  EXPECT_TRUE (received.errors[0] == framing_errc::frame_too_large);
}

TEST (LumexModbusRtuCodecTest,
      GivenTooShortOrTooLongPayloads_WhenEncoded_ThenTheyAreRefused)
{
  modbus_rtu_codec const codec;
  bytes_t out;
  bytes_t const one{ 0x01 };
  auto const short_result = codec.encode (bytes_view_t (one), out);
  ASSERT_FALSE (short_result.has_value ());
  EXPECT_TRUE (short_result.error () == framing_errc::truncated);
  bytes_t const big (255, 0x01);
  auto const long_result = codec.encode (bytes_view_t (big), out);
  ASSERT_FALSE (long_result.has_value ());
  EXPECT_TRUE (long_result.error () == framing_errc::payload_too_large);
  EXPECT_TRUE (out.empty ());
}

TEST (LumexModbusRtuCodecTest, GivenAPartialFrame_WhenReset_ThenItIsForgotten)
{
  modbus_rtu_codec codec (high_first (true));
  received_t received;
  recording_sink sink{ &received };
  bytes_t const partial{ 0x01, 0x06, 0x2C };
  codec.decode (bytes_view_t (partial), sink);
  codec.reset ();
  bytes_t const whole{ 0x01, 0x06, 0x2C, 0xC8, 0x55, 0x90, 0x98, 0x3F };
  codec.decode (bytes_view_t (whole), sink);
  ASSERT_EQ (received.frames.size (), 1u);
  EXPECT_TRUE (received.errors.empty ());
}

#if GTEST_HAS_DEATH_TEST && LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC >= 3
TEST (LumexModbusRtuCodecDeathTest,
      GivenALimitBelowTheShortestFrame_WhenConstructed_ThenTheContractStops)
{
  modbus_rtu_options_t options;
  options.max_frame_size = 4;
  EXPECT_DEATH (modbus_rtu_codec codec (options), "");
}
#endif
