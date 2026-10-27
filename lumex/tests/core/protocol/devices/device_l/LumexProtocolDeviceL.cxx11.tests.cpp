// The protocol below was intentionally altered (names, codes, constants,
// strings) for NDA reasons; it does not describe any real device.
//
// Device L (LumexProtocolDeviceL.hpp): the reference write request and
// state reply through the Modbus RTU codec with the check high byte first,
// every request and reply of the register table, the key of a reply made
// of the function and the byte count, the registers checked by the table
// (limits, access, the consecutive blocks).

#include <cstdint>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/devices/LumexProtocolDevicesSupport.hpp"
#include "lumex/tests/core/protocol/devices/device_l/LumexProtocolDeviceL.hpp"

namespace
{
namespace dev = lumex_tests::devices;
namespace field = lumex::core::protocol::field;
namespace layout = lumex::core::protocol::layout;
namespace index = lumex::core::protocol::index;
namespace registry = lumex::core::protocol::registry;
namespace error = lumex::core::protocol::error;
namespace l = lumex_tests::device_l;
using dev::bytes;
using dev::bytes_t;
using dev::tail_of;
using l::kSlave;
using l::l_command;
using l::l_registers;
using l::l_replies;
using l::l_requests;

// The stop command as the host writes it.
bytes_t const kStop = bytes ({ 0x01, 0x06, 0x2C, 0xC8 /*@r42*/, 0x55,
                               0x90 /*@r42*/, 0x98, 0x3F /*@r24*/ });

// The state reply: 23 registers.
bytes_t const kState
    = bytes ({ 0x01,          0x03, 0x2E,          0x66, 0xF4 /*@r51*/, 0x2C,
               0x4E /*@r51*/, 0x75, 0xF9 /*@r51*/, 0x75, 0xF9 /*@r51*/, 0x75,
               0xF9 /*@r51*/, 0x75, 0xF9 /*@r51*/, 0x75, 0xF9 /*@r51*/, 0x75,
               0xF9 /*@r51*/, 0x66, 0xF4 /*@r51*/, 0xD8, 0x67 /*@r51*/, 0x75,
               0xF9 /*@r51*/, 0x75, 0xF9 /*@r51*/, 0x75, 0xF9 /*@r51*/, 0x75,
               0xF9 /*@r51*/, 0x75, 0xF9 /*@r51*/, 0x75, 0xF9 /*@r51*/, 0x75,
               0xF9 /*@r51*/, 0x75, 0xF9 /*@r51*/, 0x75, 0xF9 /*@r51*/, 0x75,
               0xF9 /*@r51*/, 0xC0, 0x37 /*@r51*/, 0x75, 0xF9 /*@r51*/, 0x7E,
               0x47 /*@r51*/, 0x51, 0xCF /*@r24*/ });

// The frame of a payload with the check of device L.
bytes_t
framed (bytes_t const &payload)
{
  l::codec const codec (l::codec_options (false));
  bytes_t out;
  EXPECT_TRUE (codec
                   .encode (lumex::core::framing::codec::bytes_view_t (
                                payload.data (), payload.size ()),
                            out)
                   .has_value ());
  return out;
}

// The payloads (without the check) the codec takes from a stream; the
// errors count.
struct received_t
{
  std::vector<bytes_t> payloads;
  std::size_t errors = 0;
};

received_t
unframed (bytes_t const &stream, bool requests)
{
  l::codec decoder (l::codec_options (requests));
  received_t received;
  decoder.decode (lumex::core::framing::codec::bytes_view_t (stream.data (),
                                                             stream.size ()),
                  [&received] (lumex::core::framing::codec::bytes_view_t frame,
                               std::error_code failure)
                    {
                      if (failure)
                        ++received.errors;
                      else
                        received.payloads.push_back (
                            bytes_t (frame.begin (), frame.end ()));
                    });
  return received;
}

template <typename Row, typename... Args>
bytes_t
request (std::uint8_t function, Args const &...args)
{
  bytes_t payload = bytes ({ kSlave, function });
  EXPECT_TRUE ((layout::encode_request<l_requests, Row> (payload, args...))
                   .has_value ());
  return payload;
}
} // namespace

TEST (
    LumexProtocolDeviceLTest,
    GivenTheStopCommand_WhenEncoded_ThenTheReferenceFrameHasItsCheckHighFirst)
{
  bytes_t const payload = request<l_requests::set_06_t> (
      0x06, l_registers::command,
      registry::parameter_value_t::of_raw (
          static_cast<std::int64_t> (l_command::stop)));
  EXPECT_EQ (framed (payload), kStop);
  // The device side takes the frame back and the index finds the row.
  received_t const received = unframed (kStop, true);
  ASSERT_EQ (received.payloads.size (), 1u);
  EXPECT_TRUE (dev::finds<l_requests::set_06_t> (dev::index_of<l_requests> (),
                                                 received.payloads[0], 2));
  layout::decode_result_t<layout::request_of<l_requests::set_06_t>::type> const
      decoded
      = layout::decode_request<l_requests, l_requests::set_06_t> (
          dev::view_of (tail_of (received.payloads[0], 2)));
  ASSERT_TRUE (decoded.has_value ());
  EXPECT_TRUE (field::get<0> (*decoded) == l_registers::command);
  EXPECT_EQ (field::get<1> (*decoded).raw, 0x5590 /*@r09*/);
  EXPECT_EQ (
      dev::index_of<l_requests> ().describe (
          dev::view_of (received.payloads[0]), index::frame_direction::tx),
      "TX l_requests.set_06 (Write a register) register=command value=stop");
}

TEST (LumexProtocolDeviceLTest,
      GivenTheCheckLowByteFirst_WhenTheDeviceReadsIt_ThenTheFrameIsRefused)
{
  bytes_t const swapped = bytes ({ 0x01, 0x06, 0x2C, 0xC8 /*@r42*/, 0x55,
                                   0x90 /*@r42*/, 0x3F, 0x98 /*@r35*/ });
  received_t const received = unframed (swapped, true);
  EXPECT_TRUE (received.payloads.empty ());
  // The codec drops a byte at a time to find the next frame: one error per
  // byte it tries.
  EXPECT_GE (received.errors, 1u);
}

TEST (LumexProtocolDeviceLTest,
      GivenTheStateReply_WhenDecoded_ThenTheFunctionAndCountAreItsKey)
{
  received_t const received = unframed (kState, false);
  ASSERT_EQ (received.payloads.size (), 1u);
  bytes_t const &payload = received.payloads[0];
  EXPECT_TRUE (dev::finds<l_replies::report_032e_t> (
      dev::index_of<l_replies> (), payload, 3));
  layout::decode_result_t<
      layout::reply_value_of<l_replies::report_032e_t>::type> const state
      = layout::decode_reply<l_replies, l_replies::report_032e_t> (
          dev::view_of (tail_of (payload, 3)));
  ASSERT_TRUE (state.has_value ());
  EXPECT_EQ (field::get<0> (*state), 0x66F4u /*@r13*/);
  EXPECT_EQ (field::get<1> (*state), 0x2C4Eu /*@r13*/);
  EXPECT_EQ (field::get<5> (*state), 0x66F4u /*@r13*/);
  EXPECT_EQ (field::get<6> (*state), 0xD867u /*@r13*/);
  EXPECT_EQ (field::get<13> (*state),
             (0x75F9u /*@r13*/ << 16) | 0xC037u /*@r13*/);
  EXPECT_EQ (field::get<14> (*state),
             (0x75F9u /*@r13*/ << 16) | 0x7E47u /*@r13*/);
  EXPECT_TRUE ((dev::reply_round_trip<l_replies, l_replies::report_032e_t> (
      tail_of (payload, 3))));
  EXPECT_EQ (framed (payload), kState);
  // An echo of a write is no reply of this table: the request table reads
  // it.
  EXPECT_TRUE (
      dev::index_of<l_replies> ()
          .match (dev::view_of (bytes_t (kStop.begin (), kStop.end () - 2)))
          .row
      == nullptr);
}

TEST (LumexProtocolDeviceLTest,
      GivenTheOtherRequests_WhenEncoded_ThenTheFramesAreTheReference)
{
  EXPECT_EQ (framed (request<l_requests::set_06_t> (
                 0x06, l_registers::tray_temperature, 24.5)),
             bytes ({ 0x01, 0x06, 0x23, 0x3B /*@r42*/, 0x00, 0xF5, 0xC4,
                      0x33 /*@r24*/ }));
  EXPECT_EQ (
      framed (request<l_requests::query_03_t> (0x03, l_registers::state, 23)),
      bytes ({ 0x01, 0x03, 0x10, 0x00, 0x00, 0x17, 0x04, 0x01 /*@r24*/ }));
  std::vector<std::uint16_t> const injection{ 1, 7, 0, 125, 50 };
  bytes_t const block = request<l_requests::set_10_t> (
      0x10, l_registers::inject_mode, 5, 10, injection);
  bytes_t const frame = bytes (
      { 0x01, 0x10, 0xF6, 0x7E /*@r42*/, 0x00, 0x05, 0x0A, 0x00, 0x01, 0x00,
        0x07, 0x00, 0x00, 0x00, 0x7D, 0x00, 0x32, 0x5D, 0x63 /*@r24*/ });
  EXPECT_EQ (framed (block), frame);
  received_t const received = unframed (frame, true);
  ASSERT_EQ (received.payloads.size (), 1u);
  EXPECT_TRUE (dev::finds<l_requests::set_10_t> (dev::index_of<l_requests> (),
                                                 received.payloads[0], 2));
  EXPECT_TRUE ((dev::request_round_trip<l_requests, l_requests::set_10_t> (
      tail_of (received.payloads[0], 2))));
  // A byte count that is no number of registers.
  bytes_t odd = tail_of (block, 2);
  odd[4] = 0x09;
  EXPECT_FALSE ((layout::decode_request<l_requests, l_requests::set_10_t> (
                     dev::view_of (odd)))
                    .has_value ());
}

TEST (LumexProtocolDeviceLTest,
      GivenTheRegisterTable_WhenWritten_ThenLimitsAccessAndBlocksHold)
{
  bytes_t out;
  EXPECT_TRUE ((layout::encode_request<l_requests, l_requests::set_06_t> (
                    out, l_registers::inject_volume, 150.0))
                   .error ()
               == error::protocol_errc::out_of_limits);
  EXPECT_TRUE ((layout::encode_request<l_requests, l_requests::set_06_t> (
                    out, l_registers::state, 1))
                   .error ()
               == error::protocol_errc::access_denied);
  EXPECT_TRUE (out.empty ());
  registry::registry_entry_t const *const entries
      = l_registers::entries ().data ();
  std::size_t const mode = static_cast<std::size_t> (l_registers::inject_mode);
  for (std::size_t next = 1; next < 5; ++next)
    EXPECT_EQ (entries[mode + next].key.first, entries[mode].key.first + next);
  EXPECT_EQ (
      entries[static_cast<std::size_t> (l_registers::wash_b_volume)].key.first,
      entries[static_cast<std::size_t> (l_registers::wash_b)].key.first + 1u);
  index::index_result_t const built
      = index::build_index<l_requests, l_registers> ();
  ASSERT_TRUE (built.has_value ());
}
