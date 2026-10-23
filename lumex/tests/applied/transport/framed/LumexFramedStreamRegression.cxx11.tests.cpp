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

typedef tr::framed::framed_stream<slip_case::codec_t> link_t;

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
} // namespace

// --- a partial frame does not survive the connection that sent it -----------

TEST (
    LumexFramedStreamReconnectTest,
    GivenAPartialFrameThenADisconnect_WhenReopened_ThenTheNewFrameIsNotGluedToTheOld)
{
  log_capture log;
  stream_rig<delimiter_case> rig;
  rig.open ();
  rig.link.start ();
  rig.mock.control.feed (letters (2)); // "ab", no CRLF yet
  ASSERT_TRUE (
      eventually ([&rig] { return rig.link.statistics ().bytes_in == 2; }));
  rig.mock.control.disconnect ();
  tr::framed::frame_result_t const ended
      = rig.link.read_frame (tr::deadline_t (2000));
  ASSERT_FALSE (ended.has_value ());
  EXPECT_TRUE (ended.error () == transport_errc::disconnected)
      << ended.error ().message ();
  // The application reconnects the transport it opened itself.
  rig.link.transport ().close ();
  ASSERT_TRUE (
      rig.link.transport ().open (tr::deadline_t (2000)).has_value ());
  rig.mock.control.feed (
      concat (letters (2, 2), bytes_t{ 0x0D, 0x0A })); // "cd"
  tr::framed::frame_result_t const frame
      = rig.link.read_frame (tr::deadline_t (2000));
  ASSERT_TRUE (frame.has_value ()) << frame.error ().message ();
  EXPECT_EQ (text_of (*frame), std::string ("cd"))
      << "the decoder kept the partial frame of the connection that ended";
}

TEST (
    LumexFramedStreamReconnectTest,
    GivenAPartialFrame_WhenTheTransportIsClosedDirectlyAndReopened_ThenTheNewFrameIsNotGluedToTheOld)
{
  log_capture log;
  stream_rig<delimiter_case> rig;
  rig.open ();
  rig.link.start ();
  rig.mock.control.feed (letters (2));
  ASSERT_TRUE (
      eventually ([&rig] { return rig.link.statistics ().bytes_in == 2; }));
  // No wait between the close and the open: the aborted read of the old
  // connection comes back after the open (REVIEW32 R1b).
  rig.link.transport ().close ();
  ASSERT_TRUE (
      rig.link.transport ().open (tr::deadline_t (2000)).has_value ());
  rig.mock.control.feed (concat (letters (2, 2), bytes_t{ 0x0D, 0x0A }));
  tr::framed::frame_result_t const frame
      = rig.link.read_frame (tr::deadline_t (2000));
  ASSERT_TRUE (frame.has_value ()) << frame.error ().message ();
  EXPECT_EQ (text_of (*frame), std::string ("cd"));
}

// The close and the reopen of the transport are both on the strand before the
// cancelled read's handler runs (as with a TCP socket whose aborted read comes
// back through the queue): the handler sees an open transport, and only the
// close count of the transport tells that it is another connection (REVIEW32
// R1c).
TEST (
    LumexFramedStreamReconnectTest,
    GivenAPartialFrame_WhenCloseAndReopenAreQueuedBeforeTheCancelledRead_ThenTheNewFrameIsNotGluedToTheOld)
{
  log_capture log;
  asio::io_context context;
  scripted_stream scripted (context.get_executor ());
  std::shared_ptr<scripted_impl> device = scripted.device;
  tr::framed::framed_stream<delimiter_case::codec_t> link (
      std::move (scripted.stream),
      delimiter_case::make (delimiter_case::limit ()));
  seen opened;
  link.transport ().async_open (status_recorder{ &opened });
  context.poll ();
  ASSERT_EQ (opened.calls, 1);
  link.start ();
  context.restart ();
  context.poll ();
  ASSERT_TRUE (device->read_pending ());
  device->device_delivers (letters (2)); // "ab"
  context.restart ();
  context.poll ();
  ASSERT_EQ (link.statistics ().bytes_in, 2u);
  ASSERT_TRUE (device->read_pending ());
  link.transport ().close ();
  seen reopened;
  link.transport ().async_open (status_recorder{ &reopened });
  context.restart ();
  context.poll ();
  ASSERT_EQ (reopened.calls, 1);
  ASSERT_TRUE (link.is_open ());
  device->device_delivers (concat (letters (2, 2), bytes_t{ 0x0D, 0x0A }));
  seen read;
  link.async_read_frame (frame_recorder{ &read });
  context.restart ();
  context.poll ();
  ASSERT_EQ (read.calls, 1);
  EXPECT_FALSE (read.error) << read.error.message ();
  EXPECT_EQ (text_of (read.frame), std::string ("cd"))
      << "the cancelled read of the old connection came back after the reopen";
}

// The same for a reader that waits when the transport is closed and opened
// again at once: it ends cancelled instead of waiting on a read nobody
// restarted (message transport).
TEST (
    LumexFramedDatagramReconnectTest,
    GivenAWaitingReader_WhenTheTransportIsClosedAndOpenedAtOnce_ThenItEndsCancelled)
{
  log_capture log;
  asio::io_context context;
  tr::testing::mock_datagram mock
      = tr::testing::make_mock_datagram (context.get_executor (), "dg");
  tr::framed::framed_datagram<slip_case::codec_t> link (
      std::move (mock.transport), slip_case::make (slip_case::limit ()));
  seen opened;
  link.transport ().async_open (status_recorder{ &opened });
  context.poll ();
  ASSERT_EQ (opened.calls, 1);
  seen reader;
  link.async_read_frame (message_recorder{ &reader });
  context.restart ();
  context.poll ();
  ASSERT_EQ (reader.calls, 0);
  link.transport ().close ();
  seen reopened;
  link.transport ().async_open (status_recorder{ &reopened });
  context.restart ();
  context.poll ();
  ASSERT_EQ (reopened.calls, 1);
  ASSERT_TRUE (link.is_open ());
  ASSERT_EQ (reader.calls, 1)
      << "the reader waits for a read of a connection that is over";
  EXPECT_TRUE (reader.error == tr::error::transport_errc::cancelled)
      << reader.error.message ();
}

// The link fails and carries on over the same transport object, with no close
// and no new open (a device that reconnects by itself): only the end of the
// reading tells the codec that its partial frame is over.
TEST_F (
    LumexFramedStreamRegressionTest,
    GivenAPartialFrame_WhenTheReadFailsAndDataComesAgain_ThenTheNewFrameIsNotGluedToTheOld)
{
  make ();
  seen first;
  link_->async_read_frame (frame_recorder{ &first });
  settle ();
  // END and "ab": a frame that is never finished.
  device ().device_delivers (bytes_t{ 0xC0, 'a', 'b' });
  settle ();
  ASSERT_EQ (link_->statistics ().bytes_in, 3u);
  device ().device_fails_read (
      tr::error::make_error_code (transport_errc::disconnected));
  settle ();
  ASSERT_EQ (first.calls, 1);
  EXPECT_TRUE (first.error == transport_errc::disconnected)
      << first.error.message ();
  seen second;
  link_->async_read_frame (frame_recorder{ &second });
  settle ();
  device ().device_delivers (bytes_t{ 'c', 'd', 0xC0 });
  settle ();
  ASSERT_EQ (second.calls, 1);
  EXPECT_EQ (text_of (second.frame), std::string ("cd"))
      << "the codec kept the partial frame of the failed read";
}

// --- the generation of a closed read ----------------------------------------

TEST_F (
    LumexFramedStreamRegressionTest,
    GivenAReaderThatComesBehindAClose_WhenTheStaleReadEnds_ThenItIsServedByANewRead)
{
  make ();
  link_->start ();
  settle ();
  ASSERT_TRUE (device ().read_pending ());
  // The close is first on the strand; the reader parks behind it while the
  // read of the closed generation is still pending.
  link_->close ();
  seen reader;
  link_->async_read_frame (frame_recorder{ &reader });
  settle ();
  ASSERT_EQ (reader.calls, 1)
      << "the reader waits for ever: nothing read for it";
  EXPECT_TRUE (reader.error == transport_errc::not_open)
      << reader.error.message ();
}

TEST_F (
    LumexFramedStreamRegressionTest,
    GivenAStartBehindAClose_WhenTheStaleReadEnds_ThenTheDecoderIsStartedAgain)
{
  make ();
  link_->start ();
  settle ();
  link_->close ();
  link_->start ();
  // The bytes of the read the close cancels arrive before the transport is
  // closed: that read ends while the transport is still open, and the new
  // read the start () asks for finds it closed a moment later (the stream
  // answers not_open without asking the device).
  device ().device_delivers (slip_wire (1, 3));
  settle ();
  EXPECT_EQ (log_capture::count (log_level::info, "reading ended"), 1u)
      << "the start () behind the close was lost: " << log_capture::dump ();
  EXPECT_EQ (link_->statistics ().frames_in, 0u);
}

// --- close () resets the bursts
// ----------------------------------------------

TEST_F (
    LumexFramedStreamRegressionTest,
    GivenAnOverflowBurst_WhenClosedAndReopened_ThenTheNextOverflowWarnsAgain)
{
  make (2);
  link_->start ();
  settle ();
  device ().device_delivers (slip_wire (5));
  settle ();
  ASSERT_EQ (link_->statistics ().dropped_frames, 3u);
  ASSERT_EQ (log_capture::count (log_level::warning, "queue is full"), 1u);
  link_->close ();
  settle ();
  open_transport ();
  link_->start ();
  settle ();
  device ().device_delivers (slip_wire (5, 10));
  settle ();
  EXPECT_EQ (link_->statistics ().frames_in, 10u);
  EXPECT_EQ (link_->statistics ().dropped_frames, 6u);
  EXPECT_EQ (log_capture::count (log_level::warning, "queue is full"), 2u)
      << log_capture::dump ();
}

TEST_F (LumexFramedStreamRegressionTest,
        GivenAnErrorBurst_WhenClosedAndReopened_ThenTheNextErrorWarnsAgain)
{
  make ();
  link_->start ();
  settle ();
  // END ESC 0x01 END: an escape that SLIP does not define.
  bytes_t const broken{ 0xC0, 0xDB, 0x01, 0xC0 };
  device ().device_delivers (broken);
  settle ();
  ASSERT_EQ (link_->statistics ().errors_of (framing_errc::bad_escape), 1u);
  ASSERT_EQ (log_capture::count (log_level::warning, "dropped a frame"), 1u);
  link_->close ();
  settle ();
  open_transport ();
  link_->start ();
  settle ();
  device ().device_delivers (broken);
  settle ();
  EXPECT_EQ (link_->statistics ().errors_of (framing_errc::bad_escape), 2u);
  EXPECT_EQ (log_capture::count (log_level::warning, "dropped a frame"), 2u)
      << log_capture::dump ();
}

TEST_F (
    LumexFramedStreamRegressionTest,
    GivenAPartialFrame_WhenClosedAndReopenedWithoutAnyRead_ThenTheEndThatFollowsEndsNothing)
{
  make ();
  link_->start ();
  settle ();
  // END and one byte: the start of a frame that is never finished.
  device ().device_delivers (bytes_t{ 0xC0, 'c' });
  settle ();
  ASSERT_EQ (link_->statistics ().bytes_in, 2u);
  link_->close ();
  settle ();
  open_transport ();
  link_->start ();
  settle ();
  // Had the codec kept 'c', this END would deliver it as a frame.
  device ().device_delivers (bytes_t (1, 0xC0));
  settle ();
  EXPECT_EQ (link_->statistics ().frames_in, 0u);
  EXPECT_EQ (link_->queued_frames (), 0u);
}

// --- the log and the direct delivery
// -----------------------------------------

TEST (LumexFramedStreamLogTest,
      GivenTheStreamEnds_WhenAReaderWaits_ThenTheEndOfReadingIsLoggedAtInfo)
{
  log_capture log;
  stream_rig<slip_case> rig;
  rig.open ();
  rig.mock.control.disconnect ();
  tr::framed::frame_result_t const frame
      = rig.link.read_frame (tr::deadline_t (2000));
  ASSERT_FALSE (frame.has_value ());
  EXPECT_EQ (log_capture::count (log_level::info, "reading ended"), 1u)
      << log_capture::dump ();
}

// Two readers wait, one chunk holds two frames, and the queue holds one: the
// frames go straight to the readers. Through the queue the second frame would
// drop the first one before a reader took it.
TEST_F (
    LumexFramedStreamRegressionTest,
    GivenWaitingReaders_WhenAChunkWithTwoFramesArrives_ThenNoFrameGoesThroughTheQueue)
{
  make (1);
  seen first;
  seen second;
  link_->async_read_frame (frame_recorder{ &first });
  link_->async_read_frame (frame_recorder{ &second });
  settle ();
  ASSERT_TRUE (device ().read_pending ());
  device ().device_delivers (slip_wire (2, 20));
  settle ();
  ASSERT_EQ (first.calls, 1);
  ASSERT_EQ (second.calls, 1);
  EXPECT_EQ (first.frame, bytes_t (1, 20));
  EXPECT_EQ (second.frame, bytes_t (1, 21));
  EXPECT_EQ (link_->statistics ().dropped_frames, 0u);
  EXPECT_EQ (link_->queued_frames (), 0u);
}

// --- a real transport: the decoder's read is work of its context ------------

TEST (
    LumexFramedStreamTcpTest,
    GivenTcpAndOneFrameRead_WhenTheContextRuns_ThenRunDoesNotReturnBeforeClose)
{
  log_capture log;
  asio::io_context context;
  asio::ip::tcp::acceptor acceptor (
      context, asio::ip::tcp::endpoint (asio::ip::address_v4::loopback (), 0));
  tr::net::tcp_options_t options;
  options.host = "127.0.0.1";
  options.port = acceptor.local_endpoint ().port ();
  asio::ip::tcp::socket peer (context);
  acceptor.async_accept (peer, [] (std::error_code) {});
  link_t link (tr::net::make_tcp_stream (context.get_executor (), options),
               slip_case::make (slip_case::limit ()));
  seen opened;
  link.transport ().async_open (status_recorder{ &opened });
  context.run_for (std::chrono::seconds (2));
  ASSERT_EQ (opened.calls, 1);
  ASSERT_FALSE (opened.error) << opened.error.message ();
  context.restart ();
  bytes_t const wire = slip_wire (1, 7);
  asio::write (peer, asio::buffer (wire));
  seen read;
  link.async_read_frame (frame_recorder{ &read });
  context.run_for (std::chrono::milliseconds (400));
  EXPECT_EQ (read.calls, 1);
  // The decoder's read is pending: the context still has work (documented in
  // LumexFramedStream.hpp).
  EXPECT_FALSE (context.stopped ());
  // close () ends it, and run () returns.
  link.close ();
  context.restart ();
  clock_type::time_point const start = clock_type::now ();
  context.run_for (std::chrono::seconds (5));
  long long const ran
      = std::chrono::duration_cast<ms> (clock_type::now () - start).count ();
  EXPECT_TRUE (context.stopped ());
  EXPECT_LT (ran, 4000);
}
