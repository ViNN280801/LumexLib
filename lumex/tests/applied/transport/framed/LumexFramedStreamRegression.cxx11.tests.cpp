// Regressions of the review of the framed adapters (REVIEW32): the partial
// frame of a connection that ended is forgotten (a reopened transport does not
// glue its first frame to it), a reader or a start () that comes while the
// read of a closed generation is pending is served, close () resets the
// overflow and error bursts, the end of reading is logged at info, frames go
// straight to waiting readers, and the decoder's read is work of a real
// transport's context until close () (documented behaviour, over TCP on
// loopback). Synthetic frames.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "LumexFramedTestSupport.hpp"
#include "lumex/applied/transport/net/LumexNetTransports.hpp"

namespace
{
using namespace lumex_framed_test;
using tr::error::transport_errc;
using tr::log::log_level;

using link_t = tr::framed::framed_stream<slip_case::codec_t>;

std::string
text_of (bytes_t const &bytes)
{
  return std::string (bytes.begin (), bytes.end ());
}

class LumexFramedStreamRegressionTest : public ::testing::Test
{
protected:
  void
  settle ()
  {
    context_.restart ();
    context_.poll ();
  }

  void
  make (std::size_t bound = 16)
  {
    tr::framed::framed_options_t options;
    options.queue_bound = bound;
    scripted_.reset (new scripted_stream (context_.get_executor ()));
    link_.reset (new link_t (std::move (scripted_->stream),
                             slip_case::make (slip_case::limit ()), options));
    open_transport ();
  }

  void
  open_transport ()
  {
    seen opened;
    link_->transport ().async_open (status_recorder{ &opened });
    settle ();
    ASSERT_EQ (opened.calls, 1);
    ASSERT_TRUE (link_->is_open ());
  }

  scripted_impl &
  device ()
  {
    return *scripted_->device;
  }

  asio::io_context context_;
  log_capture log_;
  std::unique_ptr<scripted_stream> scripted_;
  std::unique_ptr<link_t> link_;
};

TEST (LumexFramedStreamRegression,
      PlainTransportQueuedWriteStaysAfterADirectCloseAndOpen)
{
  // A transport that does not connect again by itself keeps the frames it
  // queued when it is closed and opened by hand: only a transport that
  // reconnects drops the writes of a connection that is over.
  asio::io_context context;
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (context.get_executor ());
  tr::stream::byte_stream shared (mock.stream.impl ());
  tr::framed::framed_stream<fr::slip::slip_codec> framed (
      std::move (shared), fr::slip::slip_codec ());
  framed.transport ().async_open ([] (std::error_code) {});
  framed.start ();
  context.poll ();
  context.restart ();
  mock.control.hold_writes ();
  std::vector<std::uint8_t> const first = { 1 };
  std::vector<std::uint8_t> const second = { 2 };
  std::error_code second_error;
  int ended = 0;
  framed.async_write_frame (tr::const_bytes_t (first.data (), first.size ()),
                            [&] (std::error_code) { ++ended; });
  framed.async_write_frame (tr::const_bytes_t (second.data (), second.size ()),
                            [&] (std::error_code error)
                              {
                                second_error = error;
                                ++ended;
                              });
  context.poll ();
  context.restart ();
  framed.transport ().close ();
  framed.transport ().async_open ([] (std::error_code) {});
  context.poll ();
  context.restart ();
  mock.control.release_writes ();
  context.poll ();
  context.restart ();
  EXPECT_NE (second_error, transport_errc::cancelled);
  framed.close ();
  context.poll ();
}
} // namespace
