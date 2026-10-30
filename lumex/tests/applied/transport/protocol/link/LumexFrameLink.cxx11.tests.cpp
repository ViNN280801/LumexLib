// frame_link (lumex/applied/transport/protocol/link): one type over a framed
// stream and a framed datagram transport on the mocks: frames both ways,
// the report ID of the datagram link, cancel and close, and an empty link.
// The payloads are made-up bytes.

#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <system_error>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/LumexTransport"
#include "lumex/core/framing/LumexFraming"

namespace
{
namespace tr = lumex::applied::transport;
namespace pr = lumex::applied::transport::protocol;
namespace fr = lumex::core::framing;
using bytes_t = std::vector<std::uint8_t>;
using tr::error::transport_errc;

bytes_t
slip_of (bytes_t const &payload)
{
  bytes_t wire;
  fr::slip::slip_codec ().encode (
      fr::codec::bytes_view_t (payload.data (), payload.size ()), wire);
  return wire;
}

struct read_t
{
  int calls = 0;
  std::error_code error;
  pr::link_frame_t frame;
};

struct write_t
{
  int calls = 0;
  std::error_code error;
};

void
open_stream (asio::io_context &context, tr::stream::byte_stream &stream)
{
  bool opened = false;
  stream.async_open (
      [&opened] (std::error_code error)
        {
          EXPECT_FALSE (error);
          opened = true;
        });
  context.poll ();
  context.restart ();
  ASSERT_TRUE (opened);
}
} // namespace

TEST (LumexFrameLinkTest,
      GivenAStreamLink_WhenAFrameIsWrittenAndOneRead_ThenThePayloadsCrossWhole)
{
  asio::io_context context;
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (context.get_executor (), "link");
  open_stream (context, mock.stream);
  pr::frame_link link (tr::framed::framed_stream<fr::slip::slip_codec> (
      std::move (mock.stream), fr::slip::slip_codec ()));
  EXPECT_FALSE (link.empty ());
  EXPECT_TRUE (link.is_open ());
  EXPECT_EQ (link.name (), "link");
  write_t written;
  link.async_write_frame (bytes_t{ 0x01, 0xC0, 0x02 },
                          [&written] (std::error_code error)
                            {
                              ++written.calls;
                              written.error = error;
                            });
  EXPECT_EQ (written.calls, 0);
  context.poll ();
  context.restart ();
  ASSERT_EQ (written.calls, 1);
  EXPECT_FALSE (written.error);
  EXPECT_EQ (mock.control.written (), slip_of (bytes_t{ 0x01, 0xC0, 0x02 }));
  read_t got;
  link.async_read_frame (
      [&got] (std::error_code error, pr::link_frame_t frame)
        {
          ++got.calls;
          got.error = error;
          got.frame = std::move (frame);
        });
  mock.control.feed (slip_of (bytes_t{ 0x0A, 0x0B }));
  context.poll ();
  ASSERT_EQ (got.calls, 1);
  EXPECT_FALSE (got.error);
  EXPECT_EQ (got.frame, (bytes_t{ 0x0A, 0x0B }));
}

TEST (LumexFrameLinkTest,
      GivenADatagramLink_WhenAFrameIsWritten_ThenItGoesInAReportWithTheLinksId)
{
  asio::io_context context;
  tr::testing::mock_datagram mock
      = tr::testing::make_mock_datagram (context.get_executor (), "report");
  bool opened = false;
  mock.transport.async_open ([&opened] (std::error_code) { opened = true; });
  context.poll ();
  context.restart ();
  ASSERT_TRUE (opened);
  pr::frame_link link (
      tr::framed::framed_datagram<fr::slip::slip_codec> (
          std::move (mock.transport), fr::slip::slip_codec ()),
      std::uint8_t (5));
  write_t written;
  link.async_write_frame (bytes_t{ 0x31, 0x32 },
                          [&written] (std::error_code error)
                            {
                              ++written.calls;
                              written.error = error;
                            });
  context.poll ();
  context.restart ();
  ASSERT_EQ (written.calls, 1);
  EXPECT_FALSE (written.error);
  std::vector<tr::datagram::message_t> const sent = mock.control.sent ();
  ASSERT_EQ (sent.size (), 1u);
  EXPECT_EQ (sent[0].report_id, 5);
  EXPECT_EQ (sent[0].payload, slip_of (bytes_t{ 0x31, 0x32 }));
  // A frame of any report comes as its payload.
  read_t got;
  link.async_read_frame (
      [&got] (std::error_code error, pr::link_frame_t frame)
        {
          ++got.calls;
          got.error = error;
          got.frame = std::move (frame);
        });
  tr::datagram::message_t incoming;
  incoming.report_id = 9;
  incoming.payload = slip_of (bytes_t{ 0x41 });
  mock.control.deliver (incoming);
  context.poll ();
  ASSERT_EQ (got.calls, 1);
  EXPECT_FALSE (got.error);
  EXPECT_EQ (got.frame, (bytes_t{ 0x41 }));
}

TEST (
    LumexFrameLinkTest,
    GivenAPendingRead_WhenTheLinkIsCancelled_ThenItEndsCancelledAndTheLinkStaysOpenUntilClosed)
{
  asio::io_context context;
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (context.get_executor (), "link");
  open_stream (context, mock.stream);
  pr::frame_link link (tr::framed::framed_stream<fr::slip::slip_codec> (
      std::move (mock.stream), fr::slip::slip_codec ()));
  read_t first;
  link.async_read_frame (
      [&first] (std::error_code error, pr::link_frame_t)
        {
          ++first.calls;
          first.error = error;
        });
  context.poll ();
  context.restart ();
  link.cancel ();
  context.poll ();
  context.restart ();
  ASSERT_EQ (first.calls, 1);
  EXPECT_EQ (first.error, transport_errc::cancelled);
  EXPECT_TRUE (link.is_open ());
  read_t second;
  link.async_read_frame (
      [&second] (std::error_code error, pr::link_frame_t)
        {
          ++second.calls;
          second.error = error;
        });
  context.poll ();
  context.restart ();
  link.close ();
  context.poll ();
  ASSERT_EQ (second.calls, 1);
  EXPECT_EQ (second.error, transport_errc::cancelled);
  EXPECT_FALSE (link.is_open ());
}

TEST (LumexFrameLinkTest,
      GivenAnEmptyLink_WhenReadOrWritten_ThenEachEndsWithNotOpen)
{
  pr::frame_link empty;
  EXPECT_TRUE (empty.empty ());
  EXPECT_FALSE (empty.is_open ());
  EXPECT_EQ (empty.name (), "");
  empty.cancel ();
  empty.close ();
  std::future<pr::link_frame_t> read
      = empty.async_read_frame (asio::use_future);
  std::future<void> write
      = empty.async_write_frame (bytes_t{ 1 }, asio::use_future);
  try
    {
      read.get ();
      ADD_FAILURE () << "an empty link read a frame";
    }
  catch (std::system_error const &error)
    {
      EXPECT_EQ (error.code (), transport_errc::not_open);
    }
  try
    {
      write.get ();
      ADD_FAILURE () << "an empty link wrote a frame";
    }
  catch (std::system_error const &error)
    {
      EXPECT_EQ (error.code (), transport_errc::not_open);
    }
  // A moved-from link is empty too.
  asio::io_context context;
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (context.get_executor (), "moved");
  pr::frame_link full (tr::framed::framed_stream<fr::slip::slip_codec> (
      std::move (mock.stream), fr::slip::slip_codec ()));
  pr::frame_link taker (std::move (full));
  EXPECT_TRUE (full.empty ());
  EXPECT_FALSE (taker.empty ());
  EXPECT_EQ (taker.name (), "moved");
}
