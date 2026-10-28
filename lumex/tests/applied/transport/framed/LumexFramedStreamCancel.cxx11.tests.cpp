// framed_stream on an io_context the test polls itself (one thread, a scripted
// device): per-operation cancellation through the cancellation slot (terminal
// ends every pending read and write, partial and total change nothing, a
// signal after the result was handed on ends nothing, a destroyed signal is
// never touched), cancel () and close (), writes that never interleave,
// completion never inside the initiating call, handlers on their own
// executor and without outstanding work, destruction at any time and inside
// a handler, and the initiations reporting the strand.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "LumexFramedTestSupport.hpp"

namespace lumex_framed_probe
{
// A token that asks the initiation for its executor and starts nothing.
struct executor_probe
{
};

inline lumex::applied::transport::executor_type &
probed_executor ()
{
  static lumex::applied::transport::executor_type executor;
  return executor;
}
} // namespace lumex_framed_probe

namespace asio
{
template <typename Signature>
class async_result<lumex_framed_probe::executor_probe, Signature>
{
public:
  using return_type = void;

  template <typename Initiation, typename... Args>
  static void
  initiate (Initiation &&initiation, lumex_framed_probe::executor_probe,
            Args &&...)
  {
    lumex_framed_probe::probed_executor () = initiation.get_executor ();
  }
};
} // namespace asio

namespace
{
using namespace lumex_framed_test;
using tr::error::transport_errc;

using link_t = tr::framed::framed_stream<slip_case::codec_t>;

class LumexFramedStreamCancelTest : public ::testing::Test
{
protected:
  // Runs every handler that is ready, on this thread.
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

  // The end of a test that leaves a write on the device. The scripted
  // device, unlike a real stream, keeps the handler of that write itself;
  // that handler and the close a destroyed adapter queues on its strand keep
  // the adapter, the strand and the device alive for each other when the
  // context never runs the close (LeakSanitizer reports them). Destroying the
  // adapter here and running the context ends the pending handlers with
  // `cancelled` and breaks the cycle.
  void
  finish_with_a_write_on_the_device ()
  {
    link_.reset ();
    settle ();
  }

  asio::io_context context_;
  log_capture log_;
  std::unique_ptr<scripted_stream> scripted_;
  std::unique_ptr<link_t> link_;
  asio::cancellation_signal signal_;
  bytes_t payload_ = bytes_t (4, 'p');
};
} // namespace

TEST_F (
    LumexFramedStreamCancelTest,
    GivenAPendingRead_WhenTerminalIsEmitted_ThenItEndsCancelledAndTheDecoderKeepsReading)
{
  make ();
  seen read;
  link_->async_read_frame (
      asio::bind_cancellation_slot (signal_.slot (), frame_recorder{ &read }));
  settle ();
  ASSERT_EQ (read.calls, 0);
  ASSERT_TRUE (device ().read_pending ());
  signal_.emit (asio::cancellation_type::terminal);
  settle ();
  ASSERT_EQ (read.calls, 1);
  EXPECT_TRUE (read.error == transport_errc::cancelled);
  EXPECT_TRUE (read.frame.empty ());
  // No write was pending: the device was not cancelled, its read goes on.
  EXPECT_EQ (device ().cancel_calls, 0);
  EXPECT_TRUE (device ().read_pending ());
  EXPECT_EQ (device ().read_calls, 1);
  // The adapter is usable: the next frame is read.
  device ().device_delivers (slip_wire (1, 5));
  settle ();
  seen next;
  link_->async_read_frame (frame_recorder{ &next });
  settle ();
  ASSERT_EQ (next.calls, 1);
  EXPECT_FALSE (next.error);
  EXPECT_EQ (next.frame, bytes_t (1, 5));
}

TEST_F (LumexFramedStreamCancelTest,
        GivenAPendingRead_WhenPartialOrTotalIsEmitted_ThenItStaysPending)
{
  make ();
  seen read;
  link_->async_read_frame (
      asio::bind_cancellation_slot (signal_.slot (), frame_recorder{ &read }));
  settle ();
  signal_.emit (asio::cancellation_type::partial);
  settle ();
  signal_.emit (asio::cancellation_type::total);
  settle ();
  EXPECT_EQ (read.calls, 0);
  device ().device_delivers (slip_wire (1, 6));
  settle ();
  ASSERT_EQ (read.calls, 1);
  EXPECT_FALSE (read.error);
  EXPECT_EQ (read.frame, bytes_t (1, 6));
}

TEST_F (LumexFramedStreamCancelTest,
        GivenAPendingRead_WhenAllTypesAreEmitted_ThenTheTerminalBitEndsIt)
{
  make ();
  seen read;
  link_->async_read_frame (
      asio::bind_cancellation_slot (signal_.slot (), frame_recorder{ &read }));
  settle ();
  signal_.emit (asio::cancellation_type::all);
  settle ();
  ASSERT_EQ (read.calls, 1);
  EXPECT_TRUE (read.error == transport_errc::cancelled);
}

TEST_F (
    LumexFramedStreamCancelTest,
    GivenASignalBeforeTheReadReachedTheStrand_WhenItRuns_ThenItStillCancels)
{
  make ();
  seen read;
  link_->async_read_frame (
      asio::bind_cancellation_slot (signal_.slot (), frame_recorder{ &read }));
  signal_.emit (asio::cancellation_type::terminal);
  settle ();
  ASSERT_EQ (read.calls, 1);
  EXPECT_TRUE (read.error == transport_errc::cancelled);
}

TEST_F (
    LumexFramedStreamCancelTest,
    GivenAPendingWrite_WhenTerminalIsEmitted_ThenItEndsCancelledAndTheDecoderRestarts)
{
  make ();
  link_->start ();
  settle ();
  ASSERT_EQ (device ().read_calls, 1);
  seen write;
  link_->async_write_frame (tr::const_bytes_t (payload_),
                            asio::bind_cancellation_slot (
                                signal_.slot (), status_recorder{ &write }));
  settle ();
  ASSERT_EQ (device ().writes_pending (), 1u);
  signal_.emit (asio::cancellation_type::terminal);
  settle ();
  ASSERT_EQ (write.calls, 1);
  EXPECT_TRUE (write.error == transport_errc::cancelled);
  EXPECT_EQ (device ().cancel_calls, 1);
  EXPECT_EQ (link_->statistics ().frames_out, 0u);
  // The cancel ended the decoder's read too: it was started again.
  EXPECT_EQ (device ().read_calls, 2);
  EXPECT_TRUE (device ().read_pending ());
  device ().device_delivers (slip_wire (1, 8));
  settle ();
  EXPECT_EQ (link_->queued_frames (), 1u);
}

TEST_F (
    LumexFramedStreamCancelTest,
    GivenAReadAndAWrite_WhenTerminalIsEmittedOnTheRead_ThenTheWriteEndsCancelledToo)
{
  make ();
  seen read;
  seen write;
  link_->async_read_frame (
      asio::bind_cancellation_slot (signal_.slot (), frame_recorder{ &read }));
  link_->async_write_frame (tr::const_bytes_t (payload_),
                            status_recorder{ &write });
  settle ();
  ASSERT_EQ (device ().writes_pending (), 1u);
  signal_.emit (asio::cancellation_type::terminal);
  settle ();
  EXPECT_EQ (read.calls, 1);
  EXPECT_TRUE (read.error == transport_errc::cancelled);
  ASSERT_EQ (write.calls, 1);
  EXPECT_TRUE (write.error == transport_errc::cancelled);
}

TEST_F (
    LumexFramedStreamCancelTest,
    GivenReadsAndWrites_WhenCancelIsCalled_ThenAllEndCancelledAndTheAdapterWorksAfter)
{
  make ();
  seen read1;
  seen read2;
  seen write1;
  seen write2;
  link_->async_read_frame (frame_recorder{ &read1 });
  link_->async_read_frame (frame_recorder{ &read2 });
  link_->async_write_frame (tr::const_bytes_t (payload_),
                            status_recorder{ &write1 });
  link_->async_write_frame (tr::const_bytes_t (payload_),
                            status_recorder{ &write2 });
  settle ();
  // One write is on the device, the other waits behind it.
  ASSERT_EQ (device ().writes_pending (), 1u);
  link_->cancel ();
  settle ();
  EXPECT_EQ (read1.calls, 1);
  EXPECT_EQ (read2.calls, 1);
  EXPECT_EQ (write1.calls, 1);
  EXPECT_EQ (write2.calls, 1);
  EXPECT_TRUE (read1.error == transport_errc::cancelled);
  EXPECT_TRUE (read2.error == transport_errc::cancelled);
  EXPECT_TRUE (write1.error == transport_errc::cancelled);
  EXPECT_TRUE (write2.error == transport_errc::cancelled);
  EXPECT_EQ (device ().writes_pending (), 0u);
  EXPECT_EQ (link_->statistics ().frames_out, 0u);
  // Still open and usable.
  EXPECT_TRUE (link_->is_open ());
  seen again;
  link_->async_write_frame (tr::const_bytes_t (payload_),
                            status_recorder{ &again });
  settle ();
  ASSERT_EQ (device ().writes_pending (), 1u);
  device ().device_completes_write ();
  settle ();
  ASSERT_EQ (again.calls, 1);
  EXPECT_FALSE (again.error);
}

TEST_F (
    LumexFramedStreamCancelTest,
    GivenAReadThatCompleted_WhenItsSignalIsEmittedLater_ThenNewerOperationsStayPending)
{
  make ();
  seen first;
  link_->async_read_frame (asio::bind_cancellation_slot (
      signal_.slot (), frame_recorder{ &first }));
  device ().device_delivers (slip_wire (1, 1));
  settle ();
  ASSERT_EQ (first.calls, 1);
  EXPECT_TRUE (signal_.slot ().has_handler ());
  seen second;
  seen write;
  link_->async_read_frame (frame_recorder{ &second });
  link_->async_write_frame (tr::const_bytes_t (payload_),
                            status_recorder{ &write });
  settle ();
  signal_.emit (asio::cancellation_type::terminal);
  settle ();
  EXPECT_EQ (second.calls, 0);
  EXPECT_EQ (write.calls, 0);
  EXPECT_EQ (device ().writes_pending (), 1u);
  finish_with_a_write_on_the_device ();
  EXPECT_EQ (second.calls, 1);
  EXPECT_EQ (write.calls, 1);
}

// The result was handed on (the frame went to the handler), which waits on
// its own executor; the signal comes then. The operation is over: a read and
// a write pending on the adapter go on.
TEST_F (
    LumexFramedStreamCancelTest,
    GivenAResultHandedOn_WhenTheSignalComesBeforeTheHandlerRuns_ThenNothingElseIsCancelled)
{
  make ();
  asio::io_context ui;
  seen first;
  link_->async_read_frame (asio::bind_cancellation_slot (
      signal_.slot (),
      asio::bind_executor (ui.get_executor (), frame_recorder{ &first })));
  device ().device_delivers (slip_wire (1, 3));
  settle ();
  // The handler waits in the queue of `ui`.
  EXPECT_EQ (first.calls, 0);
  seen second;
  seen write;
  link_->async_read_frame (frame_recorder{ &second });
  link_->async_write_frame (tr::const_bytes_t (payload_),
                            status_recorder{ &write });
  settle ();
  signal_.emit (asio::cancellation_type::terminal);
  settle ();
  EXPECT_EQ (second.calls, 0);
  EXPECT_EQ (write.calls, 0);
  EXPECT_EQ (device ().writes_pending (), 1u);
  ui.restart ();
  ui.poll ();
  ASSERT_EQ (first.calls, 1);
  EXPECT_FALSE (first.error);
  EXPECT_EQ (first.frame, bytes_t (1, 3));
  finish_with_a_write_on_the_device ();
  EXPECT_EQ (second.calls, 1);
  EXPECT_EQ (write.calls, 1);
}

TEST_F (
    LumexFramedStreamCancelTest,
    GivenASignalDestroyedWhilePending_WhenTheReadCompletes_ThenNothingIsTouched)
{
  make ();
  std::unique_ptr<asio::cancellation_signal> doomed (
      new asio::cancellation_signal ());
  seen read;
  link_->async_read_frame (
      asio::bind_cancellation_slot (doomed->slot (), frame_recorder{ &read }));
  settle ();
  doomed.reset ();
  device ().device_delivers (slip_wire (1, 4));
  settle ();
  ASSERT_EQ (read.calls, 1);
  EXPECT_FALSE (read.error);
  EXPECT_EQ (read.frame, bytes_t (1, 4));
}

TEST_F (LumexFramedStreamCancelTest,
        GivenAReadWithCancelAfter_WhenTheTimeoutPasses_ThenItEndsCancelled)
{
  make ();
  seen read;
  link_->async_read_frame (asio::cancel_after (std::chrono::milliseconds (20),
                                               frame_recorder{ &read }));
  context_.restart ();
  context_.run_for (std::chrono::milliseconds (300));
  ASSERT_EQ (read.calls, 1);
  EXPECT_TRUE (read.error == transport_errc::cancelled);
}

// --- close
// -------------------------------------------------------------------

TEST_F (
    LumexFramedStreamCancelTest,
    GivenQueuedFramesAndAPartialFrame_WhenClosedAndReopened_ThenTheyAreForgotten)
{
  make ();
  link_->start ();
  settle ();
  // Two frames and the start of a third (END, 'c' without the closing END).
  bytes_t wire = slip_wire (2, 10);
  wire.push_back (0xC0);
  wire.push_back ('c');
  device ().device_delivers (wire);
  settle ();
  ASSERT_EQ (link_->queued_frames (), 2u);
  link_->close ();
  settle ();
  EXPECT_FALSE (link_->is_open ());
  EXPECT_EQ (link_->queued_frames (), 0u);
  seen after;
  link_->async_read_frame (frame_recorder{ &after });
  settle ();
  ASSERT_EQ (after.calls, 1);
  EXPECT_TRUE (after.error == transport_errc::not_open)
      << after.error.message ();
  // Reopen; the END that would have closed the third frame ends nothing.
  seen opened;
  link_->transport ().async_open (status_recorder{ &opened });
  settle ();
  ASSERT_TRUE (link_->is_open ());
  link_->start ();
  settle ();
  std::uint64_t const frames_before = link_->statistics ().frames_in;
  device ().device_delivers (bytes_t (1, 0xC0));
  settle ();
  EXPECT_EQ (link_->statistics ().frames_in, frames_before);
  EXPECT_EQ (link_->queued_frames (), 0u);
}

TEST_F (
    LumexFramedStreamCancelTest,
    GivenAPendingRead_WhenTheTransportIsClosedDirectly_ThenTheReaderEndsCancelled)
{
  make ();
  seen read;
  link_->async_read_frame (frame_recorder{ &read });
  settle ();
  ASSERT_EQ (read.calls, 0);
  link_->transport ().close ();
  settle ();
  ASSERT_EQ (read.calls, 1);
  EXPECT_TRUE (read.error == transport_errc::cancelled);
  EXPECT_FALSE (link_->is_open ());
}

TEST_F (
    LumexFramedStreamCancelTest,
    GivenPendingReadsAndWrites_WhenClosed_ThenAllEndCancelledAndTheDeviceIsClosed)
{
  make ();
  seen read;
  seen write1;
  seen write2;
  link_->async_read_frame (frame_recorder{ &read });
  link_->async_write_frame (tr::const_bytes_t (payload_),
                            status_recorder{ &write1 });
  link_->async_write_frame (tr::const_bytes_t (payload_),
                            status_recorder{ &write2 });
  settle ();
  link_->close ();
  settle ();
  EXPECT_EQ (read.calls, 1);
  EXPECT_EQ (write1.calls, 1);
  EXPECT_EQ (write2.calls, 1);
  EXPECT_TRUE (read.error == transport_errc::cancelled);
  EXPECT_TRUE (write1.error == transport_errc::cancelled);
  EXPECT_TRUE (write2.error == transport_errc::cancelled);
  EXPECT_FALSE (link_->is_open ());
  link_->close ();
  settle ();
}

// The handler of a read that close () ended sees the transport closed: the
// close of the transport is posted before the handlers are completed.
TEST_F (LumexFramedStreamCancelTest,
        GivenAPendingRead_WhenClosed_ThenItsHandlerSeesTheTransportClosed)
{
  make ();
  bool open_in_handler = true;
  int calls = 0;
  link_->async_read_frame (
      [this, &open_in_handler, &calls] (std::error_code, tr::framed::frame_t)
        {
          ++calls;
          open_in_handler = link_->is_open ();
        });
  settle ();
  link_->close ();
  settle ();
  ASSERT_EQ (calls, 1);
  EXPECT_FALSE (open_in_handler);
}

TEST_F (
    LumexFramedStreamCancelTest,
    GivenAReadThatCompletesAfterClose_WhenItsBytesArrive_ThenTheyAreDropped)
{
  make ();
  link_->start ();
  settle ();
  ASSERT_TRUE (device ().read_pending ());
  // The close is first on the strand; the bytes of the read it cancels
  // arrive behind it (a device that had them ready).
  link_->close ();
  device ().device_delivers (slip_wire (2, 1));
  settle ();
  EXPECT_EQ (link_->statistics ().frames_in, 0u);
  EXPECT_EQ (link_->queued_frames (), 0u);
}

// --- writes
// ------------------------------------------------------------------

TEST_F (
    LumexFramedStreamCancelTest,
    GivenTwoWrites_WhenTheDeviceTakesThemInPieces_ThenTheWiresDoNotInterleave)
{
  make ();
  bytes_t const first (5, 'a');
  bytes_t const second (3, 'b');
  seen done1;
  seen done2;
  link_->async_write_frame (tr::const_bytes_t (first),
                            status_recorder{ &done1 });
  link_->async_write_frame (tr::const_bytes_t (second),
                            status_recorder{ &done2 });
  settle ();
  // The second write is not on the device before the first one is over.
  ASSERT_EQ (device ().writes_pending (), 1u);
  device ().device_completes_write (2);
  settle ();
  ASSERT_EQ (device ().writes_pending (), 1u);
  device ().device_completes_write (3);
  settle ();
  ASSERT_EQ (device ().writes_pending (), 1u);
  EXPECT_EQ (done1.calls, 0);
  device ().device_completes_write (2);
  settle ();
  EXPECT_EQ (done1.calls, 1);
  EXPECT_FALSE (done1.error);
  EXPECT_EQ (done2.calls, 0);
  ASSERT_EQ (device ().writes_pending (), 1u);
  device ().device_completes_write ();
  settle ();
  EXPECT_EQ (done2.calls, 1);
  bytes_t expected
      = encode_direct (slip_case::make (slip_case::limit ()), first);
  expected = concat (
      expected, encode_direct (slip_case::make (slip_case::limit ()), second));
  EXPECT_EQ (device ().written, expected);
  EXPECT_EQ (link_->statistics ().frames_out, 2u);
  EXPECT_EQ (link_->statistics ().bytes_out, expected.size ());
}

TEST_F (LumexFramedStreamCancelTest,
        GivenAWriteThatFails_WhenAnotherWaitsBehindIt_ThenTheNextOneIsSent)
{
  make ();
  seen done1;
  seen done2;
  link_->async_write_frame (tr::const_bytes_t (payload_),
                            status_recorder{ &done1 });
  link_->async_write_frame (tr::const_bytes_t (payload_),
                            status_recorder{ &done2 });
  settle ();
  device ().device_fails_write (std::make_error_code (std::errc::io_error));
  settle ();
  ASSERT_EQ (done1.calls, 1);
  EXPECT_TRUE (done1.error == std::errc::io_error);
  EXPECT_EQ (link_->statistics ().frames_out, 0u);
  ASSERT_EQ (device ().writes_pending (), 1u);
  device ().device_completes_write ();
  settle ();
  ASSERT_EQ (done2.calls, 1);
  EXPECT_FALSE (done2.error);
  EXPECT_EQ (link_->statistics ().frames_out, 1u);
}

TEST_F (
    LumexFramedStreamCancelTest,
    GivenAPayloadTheCodecRefuses_WhenWrittenWhileAnotherWriteRuns_ThenItFailsAtOnceAndTheOtherGoesOn)
{
  make ();
  bytes_t const big = slip_case::oversized ();
  seen running;
  seen refused;
  link_->async_write_frame (tr::const_bytes_t (payload_),
                            status_recorder{ &running });
  link_->async_write_frame (tr::const_bytes_t (big),
                            status_recorder{ &refused });
  settle ();
  ASSERT_EQ (refused.calls, 1);
  EXPECT_TRUE (refused.error == framing_errc::payload_too_large);
  EXPECT_EQ (running.calls, 0);
  device ().device_completes_write ();
  settle ();
  EXPECT_EQ (running.calls, 1);
  EXPECT_FALSE (running.error);
}

// --- the contracts of the initiation
// -----------------------------------------

TEST_F (
    LumexFramedStreamCancelTest,
    GivenAFrameInTheQueue_WhenReadAsynchronously_ThenTheHandlerWaitsForTheExecutor)
{
  make ();
  link_->start ();
  settle ();
  device ().device_delivers (slip_wire (1, 2));
  settle ();
  ASSERT_EQ (link_->queued_frames (), 1u);
  seen read;
  link_->async_read_frame (frame_recorder{ &read });
  EXPECT_EQ (read.calls, 0);
  settle ();
  ASSERT_EQ (read.calls, 1);
  EXPECT_EQ (read.frame, bytes_t (1, 2));
}

TEST_F (
    LumexFramedStreamCancelTest,
    GivenARefusedPayload_WhenWrittenAsynchronously_ThenTheHandlerWaitsForTheExecutor)
{
  make ();
  bytes_t const big = slip_case::oversized ();
  seen write;
  link_->async_write_frame (tr::const_bytes_t (big),
                            status_recorder{ &write });
  EXPECT_EQ (write.calls, 0);
  settle ();
  EXPECT_EQ (write.calls, 1);
}

TEST_F (
    LumexFramedStreamCancelTest,
    GivenHandlersBoundToAnotherContext_WhenOperationsAreStarted_ThenTheyRunThereAndNoWorkIsKept)
{
  make ();
  asio::io_context ui;
  seen read;
  seen write;
  link_->async_read_frame (
      asio::bind_executor (ui.get_executor (), frame_recorder{ &read }));
  link_->async_write_frame (
      tr::const_bytes_t (payload_),
      asio::bind_executor (ui.get_executor (), status_recorder{ &write }));
  settle ();
  // Pending operations keep no work on `ui`: its run () has nothing to wait
  // for and returns at once.
  ui.restart ();
  ui.run_for (std::chrono::milliseconds (500));
  EXPECT_TRUE (ui.stopped ());
  EXPECT_EQ (read.calls, 0);
  EXPECT_EQ (write.calls, 0);
  device ().device_delivers (slip_wire (1, 9));
  device ().device_completes_write ();
  settle ();
  EXPECT_EQ (read.calls, 0);
  ui.restart ();
  ui.poll ();
  EXPECT_EQ (read.calls, 1);
  EXPECT_EQ (write.calls, 1);
  EXPECT_EQ (read.frame, bytes_t (1, 9));
}

TEST_F (LumexFramedStreamCancelTest,
        GivenTheInitiations_WhenAskedForTheirExecutor_ThenTheyReportTheStrand)
{
  make ();
  tr::executor_type const strand (link_->transport ().impl ()->strand ());
  lumex_framed_probe::probed_executor () = tr::executor_type ();
  link_->async_read_frame (lumex_framed_probe::executor_probe ());
  EXPECT_TRUE (lumex_framed_probe::probed_executor () == strand);
  lumex_framed_probe::probed_executor () = tr::executor_type ();
  link_->async_write_frame (tr::const_bytes_t (payload_),
                            lumex_framed_probe::executor_probe ());
  EXPECT_TRUE (lumex_framed_probe::probed_executor () == strand);
  // The copy of the strand must not outlive the context it belongs to.
  lumex_framed_probe::probed_executor () = tr::executor_type ();
}

// --- destruction at any time
// -------------------------------------------------

TEST_F (
    LumexFramedStreamCancelTest,
    GivenPendingHandlers_WhenTheAdapterIsDestroyed_ThenEachRunsOnceWithCancelled)
{
  make ();
  seen read1;
  seen read2;
  seen write1;
  seen write2;
  link_->async_read_frame (frame_recorder{ &read1 });
  link_->async_read_frame (frame_recorder{ &read2 });
  link_->async_write_frame (tr::const_bytes_t (payload_),
                            status_recorder{ &write1 });
  link_->async_write_frame (tr::const_bytes_t (payload_),
                            status_recorder{ &write2 });
  settle ();
  link_.reset ();
  settle ();
  EXPECT_EQ (read1.calls, 1);
  EXPECT_EQ (read2.calls, 1);
  EXPECT_EQ (write1.calls, 1);
  EXPECT_EQ (write2.calls, 1);
  EXPECT_TRUE (read1.error == transport_errc::cancelled);
  EXPECT_TRUE (write2.error == transport_errc::cancelled);
  // The device was closed with it.
  settle ();
  EXPECT_EQ (device ().writes_pending (), 0u);
  EXPECT_FALSE (device ().read_pending ());
}

TEST_F (
    LumexFramedStreamCancelTest,
    GivenTheHandlerOfAReadDestroysTheAdapter_WhenItRuns_ThenTheOthersEndCancelledAndNothingIsTouched)
{
  make ();
  seen second;
  seen write;
  seen first;
  link_->async_read_frame (
      [this, &first] (std::error_code error, tr::framed::frame_t frame)
        {
          ++first.calls;
          first.error = error;
          first.frame = std::move (frame);
          // The adapter goes while its own handler runs.
          link_.reset ();
        });
  link_->async_read_frame (frame_recorder{ &second });
  link_->async_write_frame (tr::const_bytes_t (payload_),
                            status_recorder{ &write });
  settle ();
  device ().device_delivers (slip_wire (1, 12));
  settle ();
  ASSERT_EQ (first.calls, 1);
  EXPECT_FALSE (first.error);
  EXPECT_EQ (first.frame, bytes_t (1, 12));
  EXPECT_EQ (link_.get (), nullptr);
  settle ();
  EXPECT_EQ (second.calls, 1);
  EXPECT_TRUE (second.error == transport_errc::cancelled);
  EXPECT_EQ (write.calls, 1);
  EXPECT_TRUE (write.error == transport_errc::cancelled);
}

TEST_F (
    LumexFramedStreamCancelTest,
    GivenTheHandlerOfAWriteDestroysTheAdapter_WhenItRuns_ThenTheQueuedWriteEndsCancelled)
{
  make ();
  seen queued;
  seen first;
  link_->async_write_frame (tr::const_bytes_t (payload_),
                            [this, &first] (std::error_code error)
                              {
                                ++first.calls;
                                first.error = error;
                                link_.reset ();
                              });
  link_->async_write_frame (tr::const_bytes_t (payload_),
                            status_recorder{ &queued });
  settle ();
  device ().device_completes_write ();
  settle ();
  ASSERT_EQ (first.calls, 1);
  EXPECT_FALSE (first.error);
  settle ();
  EXPECT_EQ (queued.calls, 1);
  EXPECT_TRUE (queued.error == transport_errc::cancelled);
}

TEST_F (
    LumexFramedStreamCancelTest,
    GivenAMovedFromAdapter_WhenUsed_ThenOperationsEndNotOpenAndTheNewOneWorks)
{
  make ();
  link_t moved (std::move (*link_));
  EXPECT_EQ (link_->name (), std::string ());
  EXPECT_FALSE (link_->is_open ());
  EXPECT_EQ (link_->queued_frames (), 0u);
  EXPECT_EQ (link_->statistics ().frames_in, 0u);
  link_->cancel ();
  link_->close ();
  link_->start ();
  // The answer of an empty adapter comes on the system executor (a handler
  // without an executor of its own has none else).
  std::promise<std::error_code> read;
  std::promise<std::error_code> write;
  link_->async_read_frame ([&read] (std::error_code error, tr::framed::frame_t)
                             { read.set_value (error); });
  link_->async_write_frame (tr::const_bytes_t (payload_),
                            [&write] (std::error_code error)
                              { write.set_value (error); });
  std::future<std::error_code> read_result = read.get_future ();
  std::future<std::error_code> write_result = write.get_future ();
  ASSERT_EQ (read_result.wait_for (std::chrono::seconds (3)),
             std::future_status::ready);
  ASSERT_EQ (write_result.wait_for (std::chrono::seconds (3)),
             std::future_status::ready);
  EXPECT_TRUE (read_result.get () == transport_errc::not_open);
  EXPECT_TRUE (write_result.get () == transport_errc::not_open);
  // The adapter that took over keeps working.
  EXPECT_TRUE (moved.is_open ());
  seen next;
  moved.async_read_frame (frame_recorder{ &next });
  device ().device_delivers (slip_wire (1, 21));
  settle ();
  ASSERT_EQ (next.calls, 1);
  EXPECT_EQ (next.frame, bytes_t (1, 21));
}

TEST_F (
    LumexFramedStreamCancelTest,
    GivenAnAdapterAssignedOverAnother_WhenItIsReplaced_ThenThePendingHandlersOfTheOldOneEndCancelled)
{
  make ();
  seen read;
  link_->async_read_frame (frame_recorder{ &read });
  settle ();
  scripted_stream other (context_.get_executor ());
  link_t replacement (std::move (other.stream),
                      slip_case::make (slip_case::limit ()));
  *link_ = std::move (replacement);
  settle ();
  ASSERT_EQ (read.calls, 1);
  EXPECT_TRUE (read.error == transport_errc::cancelled);
}
