// Per-operation cancellation through the cancellation slot of the completion
// handler (asio::bind_cancellation_slot, asio::cancel_after) on byte_stream
// and message_transport: a terminal cancellation ends the pending operation
// with `cancelled`, and every other operation pending on that transport (as
// cancel () does; terminal permits it), when the signal is emitted before
// the transport handed the result on; partial and
// total alone are not supported and change nothing; once the handler ran, a
// signal emitted later reaches nothing, and the slot is left as Asio leaves
// it (a signal destroyed while the operation is pending is never touched
// again). Over the mocks on an io_context the test runs itself (one thread,
// so the signal is emitted where Asio wants it), a TCP read from a loopback
// peer that sends nothing, and asio::cancel_after on a runner with four
// threads. Regression of the review of WP13 (TK7: nothing took the
// cancellation) and of WP12b (cancel_after raced on a multi-threaded runner;
// the completion cleared a slot the user may have destroyed). Todo 163: a
// signal emitted after the transport handed the result on (the handler still
// waits on its own executor) ends nothing; one emitted before still acts.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <new>
#include <random>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexCountingHandler.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/net/LumexNetTransports.hpp"
#include "lumex/applied/transport/stream/LumexByteStream.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"
#include "lumex/applied/transport/testing/LumexMockDatagram.hpp"
#include "lumex/applied/transport/testing/LumexMockStream.hpp"

namespace
{
namespace tr = lumex::applied::transport;
using tr::datagram::message_t;
using tr::error::transport_errc;

std::vector<std::uint8_t>
three ()
{
  return std::vector<std::uint8_t> (3, std::uint8_t (0x2A));
}

// What a handler saw; `calls` counts the completions.
struct seen
{
  int calls = 0;
  std::error_code error;
  std::size_t bytes = 0;
};

struct status_recorder
{
  seen *where;
  void
  operator() (std::error_code error)
  {
    ++where->calls;
    where->error = error;
  }
};

struct io_recorder
{
  seen *where;
  void
  operator() (std::error_code error, std::size_t bytes)
  {
    ++where->calls;
    where->error = error;
    where->bytes = bytes;
  }
};

struct message_recorder
{
  seen *where;
  void
  operator() (std::error_code error, message_t message)
  {
    ++where->calls;
    where->error = error;
    where->bytes = message.payload.size ();
  }
};

class LumexTransportCancellationTest : public ::testing::Test
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
  open_both ()
  {
    seen stream_open;
    seen datagram_open;
    stream_.stream.async_open (status_recorder{ &stream_open });
    datagram_.transport.async_open (status_recorder{ &datagram_open });
    settle ();
    ASSERT_EQ (stream_open.calls, 1);
    ASSERT_EQ (datagram_open.calls, 1);
    ASSERT_TRUE (stream_.stream.is_open ());
    ASSERT_TRUE (datagram_.transport.is_open ());
  }

  asio::io_context context_;
  tr::testing::mock_stream stream_
      = tr::testing::make_mock_stream (context_.get_executor (), "stream");
  tr::testing::mock_datagram datagram_
      = tr::testing::make_mock_datagram (context_.get_executor (), "datagram");
  asio::cancellation_signal signal_;
  std::vector<std::uint8_t> buffer_ = std::vector<std::uint8_t> (3);
};
} // namespace

TEST_F (LumexTransportCancellationTest,
        GivenAPendingReadWithASlot_WhenTerminalIsEmitted_ThenItEndsCancelled)
{
  open_both ();
  seen read;
  stream_.stream.async_read_some (
      tr::mutable_bytes_t (buffer_),
      asio::bind_cancellation_slot (signal_.slot (), io_recorder{ &read }));
  settle ();
  ASSERT_EQ (read.calls, 0);
  signal_.emit (asio::cancellation_type::terminal);
  settle ();
  ASSERT_EQ (read.calls, 1);
  EXPECT_TRUE (read.error == transport_errc::cancelled);
  EXPECT_EQ (read.bytes, 0u);
  // The slot is left as Asio leaves it; what it holds is inert now.
  EXPECT_TRUE (signal_.slot ().has_handler ());
  // The stream stays open and usable.
  EXPECT_TRUE (stream_.stream.is_open ());
  seen next;
  stream_.stream.async_read_some (tr::mutable_bytes_t (buffer_),
                                  io_recorder{ &next });
  stream_.control.feed (three ());
  settle ();
  ASSERT_EQ (next.calls, 1);
  EXPECT_EQ (next.bytes, 3u);
}

TEST_F (
    LumexTransportCancellationTest,
    GivenAPendingReadWithASlot_WhenPartialOrTotalIsEmitted_ThenItStaysPending)
{
  open_both ();
  seen read;
  stream_.stream.async_read_some (
      tr::mutable_bytes_t (buffer_),
      asio::bind_cancellation_slot (signal_.slot (), io_recorder{ &read }));
  settle ();
  signal_.emit (asio::cancellation_type::partial);
  settle ();
  signal_.emit (asio::cancellation_type::total);
  settle ();
  EXPECT_EQ (read.calls, 0);
  EXPECT_TRUE (signal_.slot ().has_handler ());
  stream_.control.feed (three ());
  settle ();
  ASSERT_EQ (read.calls, 1);
  EXPECT_FALSE (read.error);
  EXPECT_EQ (read.bytes, 3u);
}

TEST_F (
    LumexTransportCancellationTest,
    GivenAPendingReadWithASlot_WhenAllTypesAreEmitted_ThenTheTerminalBitEndsIt)
{
  open_both ();
  seen read;
  stream_.stream.async_read_some (
      tr::mutable_bytes_t (buffer_),
      asio::bind_cancellation_slot (signal_.slot (), io_recorder{ &read }));
  settle ();
  signal_.emit (asio::cancellation_type::all);
  settle ();
  ASSERT_EQ (read.calls, 1);
  EXPECT_TRUE (read.error == transport_errc::cancelled);
}

TEST_F (
    LumexTransportCancellationTest,
    GivenAReadThatCompleted_WhenItsSignalIsEmittedLater_ThenANewerReadStaysPending)
{
  open_both ();
  seen first;
  stream_.stream.async_read_some (
      tr::mutable_bytes_t (buffer_),
      asio::bind_cancellation_slot (signal_.slot (), io_recorder{ &first }));
  stream_.control.feed (three ());
  settle ();
  ASSERT_EQ (first.calls, 1);
  EXPECT_TRUE (signal_.slot ().has_handler ());
  seen second;
  stream_.stream.async_read_some (tr::mutable_bytes_t (buffer_),
                                  io_recorder{ &second });
  settle ();
  signal_.emit (asio::cancellation_type::terminal);
  settle ();
  EXPECT_EQ (second.calls, 0);
  stream_.control.feed (three ());
  settle ();
  ASSERT_EQ (second.calls, 1);
  EXPECT_FALSE (second.error);
}

TEST_F (
    LumexTransportCancellationTest,
    GivenAReadAndAReceiveWithASlot_WhenTerminalIsEmitted_ThenOnlyTheTransportOfTheReceiveIsCancelled)
{
  open_both ();
  asio::cancellation_signal other;
  seen read;
  seen receive;
  stream_.stream.async_read_some (
      tr::mutable_bytes_t (buffer_),
      asio::bind_cancellation_slot (other.slot (), io_recorder{ &read }));
  datagram_.transport.async_receive (asio::bind_cancellation_slot (
      signal_.slot (), message_recorder{ &receive }));
  settle ();
  ASSERT_EQ (receive.calls, 0);
  signal_.emit (asio::cancellation_type::terminal);
  settle ();
  ASSERT_EQ (receive.calls, 1);
  EXPECT_TRUE (receive.error == transport_errc::cancelled);
  // The cancellation is the transport's: the read of the stream goes on.
  EXPECT_EQ (read.calls, 0);
  EXPECT_TRUE (other.slot ().has_handler ());
  other.emit (asio::cancellation_type::terminal);
  settle ();
  ASSERT_EQ (read.calls, 1);
  EXPECT_TRUE (read.error == transport_errc::cancelled);
}

// The signal came while the handler of the operation waited on its own
// executor; the handler ran before the cancellation reached the strand: the
// operation is over, so the read pending on the stream goes on.
TEST_F (
    LumexTransportCancellationTest,
    GivenASignalBeforeTheHandlerRan_WhenTheHandlerRunsFirst_ThenThePostedCancellationEndsNothing)
{
  open_both ();
  seen read;
  stream_.stream.async_read_some (tr::mutable_bytes_t (buffer_),
                                  io_recorder{ &read });
  asio::io_context handler_context;
  std::vector<std::uint8_t> const data = three ();
  seen write;
  stream_.stream.async_write_some (
      tr::const_bytes_t (data),
      asio::bind_cancellation_slot (
          signal_.slot (),
          asio::bind_executor (handler_context, io_recorder{ &write })));
  settle ();
  ASSERT_EQ (write.calls, 0);
  // The write completed; its handler waits in handler_context.
  signal_.emit (asio::cancellation_type::terminal);
  handler_context.run ();
  ASSERT_EQ (write.calls, 1);
  EXPECT_TRUE (signal_.slot ().has_handler ());
  settle ();
  EXPECT_EQ (read.calls, 0);
  stream_.control.feed (three ());
  settle ();
  ASSERT_EQ (read.calls, 1);
  EXPECT_FALSE (read.error);
}

// The other order: the device completed the write, its handler still waits
// on its own executor, and the cancellation reaches the strand first. The
// stream handed the result on before the signal (todo 163): the signal ends
// nothing, the read pending there goes on, and the write still succeeds.
TEST_F (
    LumexTransportCancellationTest,
    GivenASignalAfterTheDeviceCompleted_WhenTheCancellationRunsBeforeTheHandler_ThenThePendingReadGoesOn)
{
  open_both ();
  seen read;
  stream_.stream.async_read_some (tr::mutable_bytes_t (buffer_),
                                  io_recorder{ &read });
  asio::io_context handler_context;
  std::vector<std::uint8_t> const data = three ();
  seen write;
  stream_.stream.async_write_some (
      tr::const_bytes_t (data),
      asio::bind_cancellation_slot (
          signal_.slot (),
          asio::bind_executor (handler_context, io_recorder{ &write })));
  settle ();
  ASSERT_EQ (write.calls, 0);
  signal_.emit (asio::cancellation_type::terminal);
  settle ();
  EXPECT_EQ (read.calls, 0) << "a signal after the result cancelled the read";
  handler_context.run ();
  ASSERT_EQ (write.calls, 1);
  EXPECT_FALSE (write.error);
  EXPECT_EQ (write.bytes, 3u);
  stream_.control.feed (three ());
  settle ();
  ASSERT_EQ (read.calls, 1);
  EXPECT_FALSE (read.error);
  EXPECT_EQ (read.bytes, 3u);
}

// REVIEW163 M8: the settle of one operation leaves the mark of another
// pending operation with its own slot alone: that operation's signal still
// cancels the stream.
TEST_F (
    LumexTransportCancellationTest,
    GivenTwoOperationsWithSlots_WhenOneCompletes_ThenTheSignalOfTheOtherStillCancels)
{
  open_both ();
  seen read;
  stream_.stream.async_read_some (
      tr::mutable_bytes_t (buffer_),
      asio::bind_cancellation_slot (signal_.slot (), io_recorder{ &read }));
  asio::cancellation_signal write_signal;
  std::vector<std::uint8_t> const data = three ();
  seen write;
  stream_.stream.async_write_some (
      tr::const_bytes_t (data),
      asio::bind_cancellation_slot (write_signal.slot (),
                                    io_recorder{ &write }));
  settle ();
  ASSERT_EQ (write.calls, 1);
  ASSERT_EQ (read.calls, 0);
  signal_.emit (asio::cancellation_type::terminal);
  settle ();
  ASSERT_EQ (read.calls, 1);
  EXPECT_TRUE (read.error == transport_errc::cancelled);
}

// The same for a message transport: the send completed, its handler waits on
// its own executor, the signal comes now: the receive pending goes on.
TEST_F (
    LumexTransportCancellationTest,
    GivenASignalAfterASendCompleted_WhenTheCancellationRunsBeforeTheHandler_ThenThePendingReceiveGoesOn)
{
  open_both ();
  seen receive;
  datagram_.transport.async_receive (message_recorder{ &receive });
  asio::io_context handler_context;
  seen send;
  message_t message;
  message.payload = three ();
  datagram_.transport.async_send (
      message,
      asio::bind_cancellation_slot (
          signal_.slot (),
          asio::bind_executor (handler_context, status_recorder{ &send })));
  settle ();
  ASSERT_EQ (send.calls, 0);
  signal_.emit (asio::cancellation_type::terminal);
  settle ();
  EXPECT_EQ (receive.calls, 0)
      << "a signal after the result cancelled the receive";
  handler_context.run ();
  ASSERT_EQ (send.calls, 1);
  EXPECT_FALSE (send.error);
}

// The mark (todo 163): a request acts until the transport settles the
// operation; one made before the settle still acts; a later request acts
// again once the step took the first; after the settle nothing acts.
TEST (LumexTransportOperationMark,
      GivenAPendingMark_WhenRequestedAndSettled_ThenOnlyTheEarlierRequestActs)
{
  namespace det = tr::detail;
  det::operation_mark_ptr const mark
      = std::make_shared<det::operation_mark> ();
  std::weak_ptr<det::operation_mark> const weak (mark);
  EXPECT_FALSE (det::take_cancellation_request (weak));
  EXPECT_TRUE (det::request_cancellation (weak));
  EXPECT_FALSE (det::request_cancellation (weak)) << "one step at a time";
  EXPECT_TRUE (det::take_cancellation_request (weak));
  EXPECT_TRUE (det::request_cancellation (weak)) << "pending again";

  asio::cancellation_signal signal;
  det::mark_registry registry;
  registry.watch (signal.slot (), mark);
  auto const handler
      = asio::bind_cancellation_slot (signal.slot (), [] (std::error_code) {});
  registry.settle (handler);
  EXPECT_TRUE (det::take_cancellation_request (weak))
      << "a request made before the settle acts";
  EXPECT_FALSE (det::request_cancellation (weak)) << "settled";
  EXPECT_FALSE (det::take_cancellation_request (weak));
}

// REVIEW163 F1: the signal of a pending operation was destroyed and a new
// signal took its memory: the slot of the old entry equals the new one while
// the old mark is still alive. The settle belongs to the newer operation.
TEST (
    LumexTransportOperationMark,
    GivenAStaleLiveEntryWithTheSameSlot_WhenSettled_ThenTheNewerMarkIsSettled)
{
  namespace det = tr::detail;
  asio::cancellation_signal signal;
  det::mark_registry registry;
  det::operation_mark_ptr const older
      = std::make_shared<det::operation_mark> ();
  det::operation_mark_ptr const newer
      = std::make_shared<det::operation_mark> ();
  registry.watch (signal.slot (), older);
  registry.watch (signal.slot (), newer);
  auto const handler
      = asio::bind_cancellation_slot (signal.slot (), [] (std::error_code) {});
  registry.settle (handler);
  EXPECT_FALSE (det::request_cancellation (newer)) << "newer not settled";
  EXPECT_TRUE (det::request_cancellation (older));
}

TEST (LumexTransportOperationMark,
      GivenAReusedSlot_WhenSettled_ThenOnlyTheLiveMarkIsSettled)
{
  namespace det = tr::detail;
  asio::cancellation_signal signal;
  det::mark_registry registry;
  det::operation_mark_ptr gone = std::make_shared<det::operation_mark> ();
  registry.watch (signal.slot (), gone);
  gone.reset ();
  det::operation_mark_ptr const live
      = std::make_shared<det::operation_mark> ();
  registry.watch (signal.slot (), live);
  auto const handler
      = asio::bind_cancellation_slot (signal.slot (), [] (std::error_code) {});
  registry.settle (handler);
  EXPECT_FALSE (det::request_cancellation (live));
  // A handler without a slot settles nothing.
  det::operation_mark_ptr const other
      = std::make_shared<det::operation_mark> ();
  asio::cancellation_signal other_signal;
  registry.watch (other_signal.slot (), other);
  registry.settle ([] (std::error_code) {});
  EXPECT_TRUE (det::request_cancellation (other));
}

// An operation that completes at once still takes the cancellation while it
// is pending: emitted before it ran, the terminal cancellation of the
// transport ends the read pending there too.
TEST_F (
    LumexTransportCancellationTest,
    GivenAPendingRead_WhenTerminalIsEmittedForAnotherStreamOperation_ThenTheReadEndsCancelled)
{
  open_both ();
  std::vector<std::uint8_t> const data = three ();
  stream_.control.set_write_chunk (1);
  for (int operation = 0; operation < 3; ++operation)
    {
      seen read;
      stream_.stream.async_read_some (tr::mutable_bytes_t (buffer_),
                                      io_recorder{ &read });
      settle ();
      ASSERT_EQ (read.calls, 0);
      asio::cancellation_signal signal;
      seen other;
      if (operation == 0)
        stream_.stream.async_open (asio::bind_cancellation_slot (
            signal.slot (), status_recorder{ &other }));
      else if (operation == 1)
        stream_.stream.async_write_some (
            tr::const_bytes_t (data),
            asio::bind_cancellation_slot (signal.slot (),
                                          io_recorder{ &other }));
      else
        stream_.stream.async_write_all (
            tr::const_bytes_t (data),
            asio::bind_cancellation_slot (signal.slot (),
                                          io_recorder{ &other }));
      signal.emit (asio::cancellation_type::terminal);
      settle ();
      EXPECT_EQ (other.calls, 1) << "operation " << operation;
      ASSERT_EQ (read.calls, 1) << "operation " << operation;
      EXPECT_TRUE (read.error == transport_errc::cancelled)
          << "operation " << operation;
    }
}

TEST_F (
    LumexTransportCancellationTest,
    GivenAPendingReceive_WhenTerminalIsEmittedForAnotherMessageOperation_ThenTheReceiveEndsCancelled)
{
  open_both ();
  for (int operation = 0; operation < 2; ++operation)
    {
      seen receive;
      datagram_.transport.async_receive (message_recorder{ &receive });
      settle ();
      ASSERT_EQ (receive.calls, 0);
      asio::cancellation_signal signal;
      seen other;
      if (operation == 0)
        datagram_.transport.async_open (asio::bind_cancellation_slot (
            signal.slot (), status_recorder{ &other }));
      else
        datagram_.transport.async_send (
            message_t (), asio::bind_cancellation_slot (
                              signal.slot (), status_recorder{ &other }));
      signal.emit (asio::cancellation_type::terminal);
      settle ();
      EXPECT_EQ (other.calls, 1) << "operation " << operation;
      ASSERT_EQ (receive.calls, 1) << "operation " << operation;
      EXPECT_TRUE (receive.error == transport_errc::cancelled)
          << "operation " << operation;
    }
}

// Once the handler of an operation ran, its signal reaches nothing: an
// operation started later on the same transport stays pending.
TEST_F (
    LumexTransportCancellationTest,
    GivenEveryStreamOperationWithASlot_WhenItCompleted_ThenALaterSignalEndsNothing)
{
  std::vector<std::uint8_t> const data = three ();
  for (int operation = 0; operation < 3; ++operation)
    {
      asio::cancellation_signal signal;
      seen done;
      if (operation == 0)
        stream_.stream.async_open (asio::bind_cancellation_slot (
            signal.slot (), status_recorder{ &done }));
      else if (operation == 1)
        stream_.stream.async_write_some (
            tr::const_bytes_t (data),
            asio::bind_cancellation_slot (signal.slot (),
                                          io_recorder{ &done }));
      else
        stream_.stream.async_write_all (
            tr::const_bytes_t (data),
            asio::bind_cancellation_slot (signal.slot (),
                                          io_recorder{ &done }));
      settle ();
      ASSERT_EQ (done.calls, 1) << "operation " << operation;
      EXPECT_FALSE (done.error) << "operation " << operation;
      seen read;
      stream_.stream.async_read_some (tr::mutable_bytes_t (buffer_),
                                      io_recorder{ &read });
      settle ();
      signal.emit (asio::cancellation_type::terminal);
      settle ();
      EXPECT_EQ (read.calls, 0) << "operation " << operation;
      stream_.control.feed (three ());
      settle ();
      ASSERT_EQ (read.calls, 1) << "operation " << operation;
      EXPECT_FALSE (read.error) << "operation " << operation;
    }
}

TEST_F (
    LumexTransportCancellationTest,
    GivenEveryMessageOperationWithASlot_WhenItCompleted_ThenALaterSignalEndsNothing)
{
  message_t message;
  message.payload = three ();
  for (int operation = 0; operation < 3; ++operation)
    {
      asio::cancellation_signal signal;
      seen done;
      if (operation == 0)
        datagram_.transport.async_open (asio::bind_cancellation_slot (
            signal.slot (), status_recorder{ &done }));
      else if (operation == 1)
        datagram_.transport.async_send (
            message, asio::bind_cancellation_slot (signal.slot (),
                                                   status_recorder{ &done }));
      else
        {
          datagram_.transport.async_receive (asio::bind_cancellation_slot (
              signal.slot (), message_recorder{ &done }));
          datagram_.control.deliver (message);
        }
      settle ();
      ASSERT_EQ (done.calls, 1) << "operation " << operation;
      EXPECT_FALSE (done.error) << "operation " << operation;
      seen receive;
      datagram_.transport.async_receive (message_recorder{ &receive });
      settle ();
      signal.emit (asio::cancellation_type::terminal);
      settle ();
      EXPECT_EQ (receive.calls, 0) << "operation " << operation;
      datagram_.control.deliver (message);
      settle ();
      ASSERT_EQ (receive.calls, 1) << "operation " << operation;
      EXPECT_FALSE (receive.error) << "operation " << operation;
    }
}

// asio::cancel_after: its timer runs on the executor of the initiation (the
// transport's), and its signal ends the operation like any other.
TEST_F (
    LumexTransportCancellationTest,
    GivenAPendingReadWithCancelAfter_WhenTheTimeoutPasses_ThenItEndsCancelled)
{
  open_both ();
  seen read;
  stream_.stream.async_read_some (
      tr::mutable_bytes_t (buffer_),
      asio::cancel_after (std::chrono::milliseconds (20),
                          io_recorder{ &read }));
  context_.restart ();
  context_.run_for (std::chrono::seconds (5));
  ASSERT_EQ (read.calls, 1);
  EXPECT_TRUE (read.error == transport_errc::cancelled);
}

TEST_F (
    LumexTransportCancellationTest,
    GivenAPendingReceiveWithCancelAfter_WhenTheTimeoutPasses_ThenItEndsCancelled)
{
  open_both ();
  seen receive;
  datagram_.transport.async_receive (asio::cancel_after (
      std::chrono::milliseconds (20), message_recorder{ &receive }));
  context_.restart ();
  context_.run_for (std::chrono::seconds (5));
  ASSERT_EQ (receive.calls, 1);
  EXPECT_TRUE (receive.error == transport_errc::cancelled);
}

TEST_F (
    LumexTransportCancellationTest,
    GivenAReadWithCancelAfter_WhenTheBytesArriveFirst_ThenItCompletesAndTheTimerEnds)
{
  open_both ();
  seen read;
  stream_.stream.async_read_some (
      tr::mutable_bytes_t (buffer_),
      asio::cancel_after (std::chrono::seconds (30), io_recorder{ &read }));
  stream_.control.feed (three ());
  context_.restart ();
  std::size_t const ran = context_.run_for (std::chrono::seconds (5));
  ASSERT_EQ (read.calls, 1);
  EXPECT_FALSE (read.error);
  EXPECT_EQ (read.bytes, 3u);
  // run_for returned because the work ran out (the timer was cancelled).
  EXPECT_TRUE (context_.stopped ());
  EXPECT_GT (ran, 0u);
}

// The operation is gone before its handler ran (the context died with it):
// the signal emitted afterwards reaches nothing.
TEST (
    LumexTransportCancellationLifetime,
    GivenAReadDestroyedUncalled_WhenItsSignalIsEmittedLater_ThenNothingHappens)
{
  asio::cancellation_signal signal;
  std::vector<std::uint8_t> buffer (3);
  seen read;
  {
    asio::io_context context;
    tr::testing::mock_stream mock
        = tr::testing::make_mock_stream (context.get_executor (), "m");
    mock.stream.async_read_some (
        tr::mutable_bytes_t (buffer),
        asio::bind_cancellation_slot (signal.slot (), io_recorder{ &read }));
  }
  signal.emit (asio::cancellation_type::terminal);
  EXPECT_EQ (read.calls, 0);
}

namespace
{
// A slot handler that counts its destruction.
struct counted_handler
{
  std::shared_ptr<int> destroyed;
  explicit counted_handler (std::shared_ptr<int> const &count)
      : destroyed (count)
  {
  }
  counted_handler (counted_handler const &other) : destroyed (other.destroyed)
  {
  }
  ~counted_handler ()
  {
    if (destroyed)
      ++*destroyed;
  }
  counted_handler &operator= (counted_handler const &) = delete;
  void
  operator() (asio::cancellation_type_t)
  {
  }
};
} // namespace

// The signal dies while the read is pending (Asio's own operations never
// touch a slot again once they put their handler in it): the completion must
// not touch it either. A new signal took the same storage and holds a
// handler of the user's; the completion leaves it alone.
TEST (
    LumexTransportCancellationLifetime,
    GivenASignalDestroyedWhileTheReadIsPending_WhenTheReadCompletes_ThenANewSignalAtThatAddressIsUntouched)
{
  asio::io_context context;
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (context.get_executor (), "m");
  seen open;
  mock.stream.async_open (status_recorder{ &open });
  context.poll ();
  ASSERT_EQ (open.calls, 1);
  std::vector<std::uint8_t> buffer (3);
  alignas (asio::cancellation_signal) unsigned char
      storage[sizeof (asio::cancellation_signal)];
  asio::cancellation_signal *const first
      = new (storage) asio::cancellation_signal;
  seen read;
  mock.stream.async_read_some (
      tr::mutable_bytes_t (buffer),
      asio::bind_cancellation_slot (first->slot (), io_recorder{ &read }));
  context.restart ();
  context.poll ();
  first->~cancellation_signal ();
  asio::cancellation_signal *const second
      = new (storage) asio::cancellation_signal;
  std::shared_ptr<int> const destroyed = std::make_shared<int> (0);
  second->slot ().emplace<counted_handler> (destroyed);
  int const before = *destroyed;
  mock.control.feed (three ());
  context.restart ();
  context.poll ();
  ASSERT_EQ (read.calls, 1);
  EXPECT_TRUE (second->slot ().has_handler ())
      << "the completion cleared the slot of another signal";
  EXPECT_EQ (*destroyed, before);
  second->~cancellation_signal ();
}

// Control: an Asio timer in the same scenario.
TEST (
    LumexTransportCancellationLifetime,
    GivenASignalDestroyedWhileAnAsioTimerWaits_WhenTheTimerCompletes_ThenANewSignalAtThatAddressIsUntouched)
{
  asio::io_context context;
  asio::steady_timer timer (context, std::chrono::milliseconds (10));
  alignas (asio::cancellation_signal) unsigned char
      storage[sizeof (asio::cancellation_signal)];
  asio::cancellation_signal *const first
      = new (storage) asio::cancellation_signal;
  seen wait;
  timer.async_wait (
      asio::bind_cancellation_slot (first->slot (), status_recorder{ &wait }));
  first->~cancellation_signal ();
  asio::cancellation_signal *const second
      = new (storage) asio::cancellation_signal;
  std::shared_ptr<int> const destroyed = std::make_shared<int> (0);
  second->slot ().emplace<counted_handler> (destroyed);
  int const before = *destroyed;
  context.run ();
  ASSERT_EQ (wait.calls, 1);
  EXPECT_TRUE (second->slot ().has_handler ());
  EXPECT_EQ (*destroyed, before);
  second->~cancellation_signal ();
}

// A TCP read from a peer that sends nothing: the terminal cancellation ends
// it with `cancelled` (the socket's operation is cancelled).
TEST (LumexTransportCancellationTcp,
      GivenAReadFromASilentPeer_WhenTerminalIsEmitted_ThenItEndsCancelled)
{
  typedef asio::ip::tcp tcp;
  asio::io_context peer_context;
  tcp::acceptor acceptor (
      peer_context, tcp::endpoint (asio::ip::address_v4::loopback (), 0));
  tcp::socket peer (peer_context);
  tr::stream::io_runner runner (1);
  tr::net::tcp_options_t options;
  options.host = "127.0.0.1";
  options.port = acceptor.local_endpoint ().port ();
  tr::stream::byte_stream stream
      = tr::net::make_tcp_stream (runner.get_executor (), options);
  ASSERT_TRUE (stream.open (tr::deadline_t (5000)).has_value ());
  acceptor.accept (peer);

  std::vector<std::uint8_t> buffer (16);
  asio::cancellation_signal signal;
  std::promise<std::error_code> error;
  stream.async_read_some (
      tr::mutable_bytes_t (buffer),
      asio::bind_cancellation_slot (signal.slot (),
                                    [&error] (std::error_code e, std::size_t)
                                      { error.set_value (e); }));
  std::future<std::error_code> ended = error.get_future ();
  ASSERT_EQ (ended.wait_for (std::chrono::milliseconds (100)),
             std::future_status::timeout);
  // The read cannot complete before the signal, so emitting here does not
  // race its completion.
  signal.emit (asio::cancellation_type::terminal);
  bool const in_time
      = ended.wait_for (std::chrono::seconds (5)) == std::future_status::ready;
  EXPECT_TRUE (in_time) << "the terminal cancellation did not end the read";
  if (!in_time)
    {
      // The handler still uses the locals: end it first.
      stream.close ();
      ended.wait ();
      return;
    }
  EXPECT_TRUE (ended.get () == transport_errc::cancelled);
  stream.close ();
}

namespace
{
// Counts the calls of one handler; a second call is a defect.
struct once_handler
{
  std::shared_ptr<std::promise<std::error_code>> done;
  std::shared_ptr<std::atomic<int>> calls;
  std::atomic<int> *doubles;
  void
  operator() (std::error_code error, std::size_t)
  {
    if (calls->fetch_add (1) != 0)
      {
        doubles->fetch_add (1);
        return;
      }
    done->set_value (error);
  }
};

struct stress_result
{
  int done = 0;
  int lost = 0;
  int hung = 0;
  int doubles = 0;
};

// Reads with asio::cancel_after of 0..60 us on a runner with four threads,
// the bytes fed before each read, so the timer and the completion race.
stress_result
cancel_after_stress (bool bind_strand, int iterations)
{
  stress_result result;
  tr::stream::io_runner runner (4);
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (runner.get_executor (), "stress");
  if (!mock.stream.open (tr::deadline_t (2000)).has_value ())
    return result;
  std::vector<std::uint8_t> buffer (1);
  std::mt19937 random (12345);
  std::uniform_int_distribution<int> delay (0, 60);
  std::atomic<int> doubles (0);
  asio::strand<tr::executor_type> const strand
      = asio::make_strand (runner.get_executor ());
  for (int i = 0; i < iterations; ++i)
    {
      mock.control.feed (std::vector<std::uint8_t> (1, std::uint8_t (1)));
      once_handler handler
          = { std::make_shared<std::promise<std::error_code>> (),
              std::make_shared<std::atomic<int>> (0), &doubles };
      std::future<std::error_code> ended = handler.done->get_future ();
      std::chrono::microseconds const timeout (delay (random));
      // asio::cancel_after needs the operation to be started inside a strand
      // (its documented thread safety: the timer's handler and the
      // initiation both touch the cancellation slot of the operation), the
      // strand the timer's handler runs on: the associated executor of the
      // handler, which is the stream's own strand when the handler has none.
      // Started from this thread it races the timer; see the transport
      // README.
      asio::post (
          bind_strand ? strand : mock.stream.impl ()->strand (),
          [&mock, &buffer, bind_strand, &strand, timeout, handler] ()
            {
              if (bind_strand)
                mock.stream.async_read_some (
                    tr::mutable_bytes_t (buffer),
                    asio::cancel_after (
                        timeout, asio::bind_executor (strand, handler)));
              else
                mock.stream.async_read_some (
                    tr::mutable_bytes_t (buffer),
                    asio::cancel_after (timeout, handler));
            });
      if (ended.wait_for (std::chrono::seconds (5))
          != std::future_status::ready)
        {
          ++result.hung;
          break;
        }
      try
        {
          ended.get ();
          ++result.done;
        }
      catch (std::future_error const &)
        {
          ++result.lost; // the handler was destroyed uncalled
        }
    }
  result.doubles = doubles.load ();
  mock.stream.close ();
  return result;
}
} // namespace

// The timer of asio::cancel_after runs on the executor of the initiation,
// the transport's strand, so it never races the completion of the operation
// (Asio's timed_cancel_op is not thread-safe). Every handler runs once.
TEST (
    LumexTransportCancellationStress,
    GivenCancelAfterOnFourThreads_WhenTheHandlerHasNoExecutor_ThenEveryHandlerRunsOnce)
{
  int const iterations = 3000;
  stress_result const result = cancel_after_stress (false, iterations);
  EXPECT_EQ (result.done, iterations);
  EXPECT_EQ (result.lost, 0);
  EXPECT_EQ (result.hung, 0);
  EXPECT_EQ (result.doubles, 0);
}

TEST (
    LumexTransportCancellationStress,
    GivenCancelAfterOnFourThreads_WhenTheHandlerIsOnAStrand_ThenEveryHandlerRunsOnce)
{
  int const iterations = 3000;
  stress_result const result = cancel_after_stress (true, iterations);
  EXPECT_EQ (result.done, iterations);
  EXPECT_EQ (result.lost, 0);
  EXPECT_EQ (result.hung, 0);
  EXPECT_EQ (result.doubles, 0);
}
