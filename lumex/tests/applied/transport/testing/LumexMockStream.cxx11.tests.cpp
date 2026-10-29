// The scripted mock stream: the write observer (order, chunks, strand, no
// inline call, feeding a reply from inside), timed delivery on the clock of
// the mock's timer source, read chunks, held writes, an empty read, several
// pending reads, and the shutdown of a context that still holds a mock's
// observer, reads and writes.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/LumexTransport"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"

namespace
{
namespace tr = lumex::applied::transport;
using ms = std::chrono::milliseconds;
using bytes_t = std::vector<std::uint8_t>;
using tr::error::transport_errc;
using tr::testing::make_mock_stream;
using tr::testing::mock_options_t;
using tr::testing::mock_stream;
using tr::testing::virtual_clock;

ms const KREAL_TIME_GUARD = ms (5000);

bytes_t
bytes_of (std::string const &text)
{
  return bytes_t (text.begin (), text.end ());
}

std::string
text_of (bytes_t const &bytes)
{
  return std::string (bytes.begin (), bytes.end ());
}

std::error_code
code (transport_errc value)
{
  return make_error_code (value);
}

// An observer that owns a copy of the control (a cycle with the mock until
// the observer is dropped) and the sentinel.
tr::testing::write_observer_t
cyclic_observer (tr::testing::mock_stream_control const &control,
                 std::shared_ptr<int> const &sentinel)
{
  return [control, sentinel] (bytes_t const &) {};
}

// A read or write handler that owns a copy of the control (a cycle with the
// mock until the handler is dropped) and the sentinel.
struct cyclic_io_handler
{
  tr::testing::mock_stream_control control;
  std::shared_ptr<int> sentinel;

  void
  operator() (std::error_code const &, std::size_t) const
  {
  }
};

// Starts two reads whose handlers own the control, so that no copy stays
// with the caller.
void
start_cyclic_reads (tr::stream::byte_stream &stream,
                    tr::testing::mock_stream_control const &control,
                    std::shared_ptr<int> const &sentinel, bytes_t &first,
                    bytes_t &second)
{
  cyclic_io_handler const handler = { control, sentinel };
  stream.async_read_some (tr::mutable_bytes_t (first), handler);
  stream.async_read_some (tr::mutable_bytes_t (second), handler);
}

// Starts a write (held by the mock) whose handler owns the control.
void
start_cyclic_write (tr::stream::byte_stream &stream,
                    tr::testing::mock_stream_control const &control,
                    std::shared_ptr<int> const &sentinel, bytes_t &data)
{
  cyclic_io_handler const handler = { control, sentinel };
  stream.async_write_some (tr::const_bytes_t (data), handler);
}

// What the handler of one read saw.
struct read_state
{
  bool done = false;
  std::error_code error;
  std::size_t count = 0;
  bytes_t buffer;

  std::string
  text () const
  {
    return std::string (buffer.begin (),
                        buffer.begin () + static_cast<std::ptrdiff_t> (count));
  }
};

// What the handler of one write saw.
struct write_state
{
  bool done = false;
  std::error_code error;
  std::size_t count = 0;
  bytes_t data;
};

// Runs every ready handler of `context` (restarting it when it ran out of
// work).
void
drain (asio::io_context &context)
{
  for (;;)
    {
      if (context.stopped ())
        context.restart ();
      if (context.poll () == 0)
        return;
    }
}

class LumexMockStreamTest : public ::testing::Test
{
protected:
  asio::io_context context_;
  virtual_clock clock_;
  mock_stream mock_ = make_mock_stream (context_.get_executor (), scripted ());

  mock_options_t
  scripted () const
  {
    mock_options_t options;
    options.name = "scripted";
    options.timers = clock_.source ();
    return options;
  }

  void
  open ()
  {
    bool done = false;
    std::error_code error;
    mock_.stream.async_open (
        [&done, &error] (std::error_code const &result)
          {
            done = true;
            error = result;
          });
    drain (context_);
    ASSERT_TRUE (done);
    ASSERT_FALSE (error);
  }

  std::shared_ptr<read_state>
  start_read (std::size_t capacity)
  {
    std::shared_ptr<read_state> const state = std::make_shared<read_state> ();
    state->buffer.assign (capacity, 0);
    mock_.stream.async_read_some (
        tr::mutable_bytes_t (state->buffer),
        [state] (std::error_code const &error, std::size_t count)
          {
            state->done = true;
            state->error = error;
            state->count = count;
          });
    return state;
  }

  std::shared_ptr<write_state>
  start_write (std::string const &text)
  {
    std::shared_ptr<write_state> const state
        = std::make_shared<write_state> ();
    state->data = bytes_of (text);
    mock_.stream.async_write_some (
        tr::const_bytes_t (state->data),
        [state] (std::error_code const &error, std::size_t count)
          {
            state->done = true;
            state->error = error;
            state->count = count;
          });
    return state;
  }

  std::shared_ptr<write_state>
  start_write_all (std::string const &text)
  {
    std::shared_ptr<write_state> const state
        = std::make_shared<write_state> ();
    state->data = bytes_of (text);
    mock_.stream.async_write_all (
        tr::const_bytes_t (state->data),
        [state] (std::error_code const &error, std::size_t count)
          {
            state->done = true;
            state->error = error;
            state->count = count;
          });
    return state;
  }
};
} // namespace

// --- the write observer ----------------------------------------------------

TEST_F (
    LumexMockStreamTest,
    GivenAnObserver_WhenWritesAreChunked_ThenItSeesEveryChunkInOrderOnTheStrandNeverInline)
{
  open ();
  std::vector<std::string> chunks;
  std::vector<bool> on_strand;
  asio::strand<tr::executor_type> const strand = mock_.stream.get_strand ();
  mock_.control.set_write_chunk (3);
  mock_.control.on_write (
      [&chunks, &on_strand, strand] (bytes_t const &chunk)
        {
          chunks.push_back (text_of (chunk));
          on_strand.push_back (strand.running_in_this_thread ());
        });
  std::shared_ptr<write_state> const write = start_write_all ("abcdefgh");
  EXPECT_TRUE (chunks.empty ()); // not inside the initiating call
  drain (context_);
  ASSERT_TRUE (write->done);
  EXPECT_FALSE (write->error);
  ASSERT_EQ (chunks.size (), 3u);
  EXPECT_EQ (chunks[0], "abc");
  EXPECT_EQ (chunks[1], "def");
  EXPECT_EQ (chunks[2], "gh");
  EXPECT_TRUE (on_strand[0] && on_strand[1] && on_strand[2]);
  EXPECT_EQ (text_of (mock_.control.written ()), "abcdefgh");
}

TEST_F (LumexMockStreamTest,
        GivenAnObserver_WhenItIsReplacedByAnEmptyOne_ThenItSeesNoMoreWrites)
{
  open ();
  int calls = 0;
  mock_.control.on_write ([&calls] (bytes_t const &) { ++calls; });
  start_write ("a");
  drain (context_);
  EXPECT_EQ (calls, 1);
  mock_.control.on_write (tr::testing::write_observer_t ());
  start_write ("b");
  drain (context_);
  EXPECT_EQ (calls, 1);
  EXPECT_EQ (text_of (mock_.control.written ()), "ab");
}

TEST_F (
    LumexMockStreamTest,
    GivenAWriteThatFails_WhenObserved_ThenTheObserverSeesOnlyAcceptedChunks)
{
  open ();
  std::vector<std::string> chunks;
  mock_.control.on_write ([&chunks] (bytes_t const &chunk)
                            { chunks.push_back (text_of (chunk)); });
  mock_.control.fail_next_write (std::make_error_code (std::errc::io_error));
  std::shared_ptr<write_state> const failed = start_write ("lost");
  drain (context_);
  ASSERT_TRUE (failed->done);
  EXPECT_EQ (failed->error, std::make_error_code (std::errc::io_error));
  EXPECT_TRUE (chunks.empty ());
  start_write ("kept");
  drain (context_);
  ASSERT_EQ (chunks.size (), 1u);
  EXPECT_EQ (chunks[0], "kept");
}

TEST_F (
    LumexMockStreamTest,
    GivenAnObserverThatFeedsAReply_WhenARequestIsWritten_ThenThePendingReadGetsItWithoutDeadlockOrInlineCompletion)
{
  open ();
  tr::testing::mock_stream_control control = mock_.control;
  int read_calls_inside = 0;
  std::shared_ptr<read_state> const read = start_read (16);
  drain (context_);
  ASSERT_FALSE (read->done);
  control.on_write (
      [control, read, &read_calls_inside] (bytes_t const &chunk) mutable
        {
          control.feed (bytes_of ("re:" + text_of (chunk)));
          // The read is completed through the strand, never from here.
          read_calls_inside += read->done ? 1 : 0;
        });
  std::shared_ptr<write_state> const write = start_write ("ping");
  drain (context_);
  ASSERT_TRUE (write->done);
  ASSERT_TRUE (read->done);
  EXPECT_FALSE (read->error);
  EXPECT_EQ (read->text (), "re:ping");
  EXPECT_EQ (read_calls_inside, 0);
  control.on_write (tr::testing::write_observer_t ()); // breaks the cycle
}

TEST_F (LumexMockStreamTest,
        GivenAnObserverThatHoldsWrites_WhenItRuns_ThenTheNextWriteStaysPending)
{
  open ();
  tr::testing::mock_stream_control control = mock_.control;
  control.on_write ([control] (bytes_t const &) mutable
                      { control.hold_writes (); });
  std::shared_ptr<write_state> const first = start_write ("one");
  std::shared_ptr<write_state> const second = start_write ("two");
  drain (context_);
  EXPECT_TRUE (first->done);
  EXPECT_FALSE (second->done);
  EXPECT_EQ (text_of (mock_.control.written ()), "one");
  control.on_write (tr::testing::write_observer_t ());
  mock_.control.release_writes ();
  drain (context_);
  EXPECT_TRUE (second->done);
  EXPECT_EQ (text_of (mock_.control.written ()), "onetwo");
}

// --- timed delivery --------------------------------------------------------

TEST_F (LumexMockStreamTest,
        GivenFeedAfter_WhenTheClockAdvances_ThenTheBytesArriveAtTheirTime)
{
  open ();
  std::shared_ptr<read_state> const read = start_read (16);
  mock_.control.feed_after (ms (100), bytes_of ("late"));
  drain (context_);
  EXPECT_FALSE (read->done);
  clock_.advance (ms (99));
  drain (context_);
  EXPECT_FALSE (read->done);
  clock_.advance (ms (1));
  drain (context_);
  ASSERT_TRUE (read->done);
  EXPECT_FALSE (read->error);
  EXPECT_EQ (read->text (), "late");
}

TEST_F (
    LumexMockStreamTest,
    GivenSeveralFeedAfter_WhenTheClockPassesThem_ThenTheyArriveInTimeThenScheduleOrder)
{
  open ();
  mock_.control.feed_after (ms (20), bytes_of ("B"));
  mock_.control.feed_after (ms (10), bytes_of ("A"));
  mock_.control.feed_after (ms (10), bytes_of ("C"));
  clock_.advance (ms (20));
  drain (context_);
  std::shared_ptr<read_state> const read = start_read (16);
  drain (context_);
  ASSERT_TRUE (read->done);
  EXPECT_EQ (read->text (), "ACB");
}

TEST_F (LumexMockStreamTest,
        GivenFeedAfterOfZero_WhenNothingAdvances_ThenItWaitsForTheNextAdvance)
{
  open ();
  std::shared_ptr<read_state> const read = start_read (16);
  mock_.control.feed_after (ms (0), bytes_of ("now"));
  drain (context_);
  EXPECT_FALSE (read->done);
  clock_.advance (ms (0));
  drain (context_);
  ASSERT_TRUE (read->done);
  EXPECT_EQ (read->text (), "now");
}

TEST_F (LumexMockStreamTest,
        GivenFeedAfter_WhenTheStreamIsClosed_ThenTheScriptStillDelivers)
{
  open ();
  mock_.control.feed_after (ms (10), bytes_of ("kept"));
  mock_.stream.close ();
  drain (context_);
  clock_.advance (ms (10));
  drain (context_);
  open ();
  std::shared_ptr<read_state> const read = start_read (16);
  drain (context_);
  ASSERT_TRUE (read->done);
  EXPECT_EQ (read->text (), "kept");
}

TEST_F (LumexMockStreamTest,
        GivenAPendingFeedAfter_WhenTheMockIsDestroyed_ThenTheScriptEnds)
{
  std::unique_ptr<mock_stream> made (new mock_stream (
      make_mock_stream (context_.get_executor (), scripted ())));
  made->control.feed_after (ms (50), bytes_of ("never"));
  EXPECT_EQ (clock_.pending (), 1u);
  made.reset ();
  drain (context_);
  EXPECT_EQ (clock_.pending (), 0u);
  clock_.advance (ms (100)); // nothing left to touch
  drain (context_);
}

TEST (LumexMockStream,
      GivenAPendingFeedAfter_WhenTheClockIsDestroyed_ThenNothingIsDelivered)
{
  asio::io_context context;
  std::unique_ptr<virtual_clock> clock (new virtual_clock);
  mock_options_t options;
  options.timers = clock->source ();
  mock_stream made = make_mock_stream (context.get_executor (), options);
  bool opened = false;
  made.stream.async_open ([&opened] (std::error_code const &)
                            { opened = true; });
  drain (context);
  ASSERT_TRUE (opened);
  std::shared_ptr<read_state> const read = std::make_shared<read_state> ();
  read->buffer.assign (8, 0);
  made.stream.async_read_some (
      tr::mutable_bytes_t (read->buffer),
      [read] (std::error_code const &error, std::size_t count)
        {
          read->done = true;
          read->error = error;
          read->count = count;
        });
  made.control.feed_after (ms (10), bytes_of ("never"));
  clock.reset (); // ends the pending wait with cancelled
  drain (context);
  EXPECT_FALSE (read->done);
  made.stream.close ();
  drain (context);
  ASSERT_TRUE (read->done);
  EXPECT_EQ (read->error, code (transport_errc::cancelled));
}

TEST (LumexMockStream,
      GivenTheDefaultOptions_WhenFeedAfterIsUsed_ThenItWaitsForRealTime)
{
  asio::io_context context;
  mock_stream made = make_mock_stream (context.get_executor (), "steady");
  EXPECT_EQ (made.stream.name (), "steady");
  bool opened = false;
  made.stream.async_open ([&opened] (std::error_code const &)
                            { opened = true; });
  drain (context);
  ASSERT_TRUE (opened);
  std::shared_ptr<read_state> const read = std::make_shared<read_state> ();
  read->buffer.assign (8, 0);
  made.stream.async_read_some (
      tr::mutable_bytes_t (read->buffer),
      [read] (std::error_code const &error, std::size_t count)
        {
          read->done = true;
          read->error = error;
          read->count = count;
        });
  std::chrono::steady_clock::time_point const start
      = std::chrono::steady_clock::now ();
  made.control.feed_after (ms (30), bytes_of ("real"));
  while (!read->done
         && std::chrono::steady_clock::now () - start < KREAL_TIME_GUARD)
    {
      if (context.stopped ())
        context.restart ();
      context.run_for (ms (10));
    }
  ASSERT_TRUE (read->done);
  EXPECT_EQ (read->text (), "real");
  EXPECT_GE (std::chrono::steady_clock::now () - start, ms (25));
}

// --- read chunks ------------------------------------------------------------

TEST_F (LumexMockStreamTest,
        GivenAReadChunk_WhenBytesAreAvailable_ThenEachReadTakesAtMostTheChunk)
{
  open ();
  mock_.control.set_read_chunk (4);
  mock_.control.feed (bytes_of ("0123456789"));
  std::vector<std::string> parts;
  for (int i = 0; i < 3; ++i)
    {
      std::shared_ptr<read_state> const read = start_read (16);
      drain (context_);
      ASSERT_TRUE (read->done);
      parts.push_back (read->text ());
    }
  ASSERT_EQ (parts.size (), 3u);
  EXPECT_EQ (parts[0], "0123");
  EXPECT_EQ (parts[1], "4567");
  EXPECT_EQ (parts[2], "89");
}

TEST_F (
    LumexMockStreamTest,
    GivenAReadChunk_WhenBytesArriveForAPendingRead_ThenItTakesAtMostTheChunk)
{
  open ();
  mock_.control.set_read_chunk (2);
  std::shared_ptr<read_state> const read = start_read (16);
  drain (context_);
  mock_.control.feed (bytes_of ("abcdef"));
  drain (context_);
  ASSERT_TRUE (read->done);
  EXPECT_EQ (read->text (), "ab");
  mock_.control.set_read_chunk (0); // no limit again
  std::shared_ptr<read_state> const rest = start_read (16);
  drain (context_);
  EXPECT_EQ (rest->text (), "cdef");
}

// --- held writes ------------------------------------------------------------

TEST_F (LumexMockStreamTest,
        GivenHeldWrites_WhenTheStreamIsClosed_ThenTheWriteEndsCancelled)
{
  open ();
  int observed = 0;
  mock_.control.on_write ([&observed] (bytes_t const &) { ++observed; });
  mock_.control.hold_writes ();
  std::shared_ptr<write_state> const write = start_write ("frozen");
  drain (context_);
  EXPECT_FALSE (write->done);
  EXPECT_TRUE (mock_.control.written ().empty ());
  mock_.stream.close ();
  drain (context_);
  ASSERT_TRUE (write->done);
  EXPECT_EQ (write->error, code (transport_errc::cancelled));
  EXPECT_TRUE (mock_.control.written ().empty ());
  EXPECT_EQ (observed, 0);
}

TEST_F (
    LumexMockStreamTest,
    GivenHeldWrites_WhenTheStreamIsCancelled_ThenEveryHeldWriteEndsCancelled)
{
  open ();
  mock_.control.hold_writes ();
  std::shared_ptr<write_state> const first = start_write ("a");
  std::shared_ptr<write_state> const second = start_write ("b");
  drain (context_);
  mock_.stream.cancel ();
  drain (context_);
  ASSERT_TRUE (first->done && second->done);
  EXPECT_EQ (first->error, code (transport_errc::cancelled));
  EXPECT_EQ (second->error, code (transport_errc::cancelled));
  EXPECT_TRUE (mock_.stream.is_open ());
}

TEST_F (
    LumexMockStreamTest,
    GivenHeldWrites_WhenReleased_ThenTheyGoInOrderAndALaterWriteQueuesBehindThem)
{
  open ();
  std::vector<std::string> chunks;
  mock_.control.on_write ([&chunks] (bytes_t const &chunk)
                            { chunks.push_back (text_of (chunk)); });
  mock_.control.hold_writes ();
  std::shared_ptr<write_state> const a = start_write ("A");
  std::shared_ptr<write_state> const b = start_write ("B");
  drain (context_);
  EXPECT_FALSE (a->done || b->done);
  mock_.control.release_writes ();
  std::shared_ptr<write_state> const c = start_write ("C");
  drain (context_);
  ASSERT_TRUE (a->done && b->done && c->done);
  EXPECT_EQ (text_of (mock_.control.written ()), "ABC");
  ASSERT_EQ (chunks.size (), 3u);
  EXPECT_EQ (chunks[0], "A");
  EXPECT_EQ (chunks[1], "B");
  EXPECT_EQ (chunks[2], "C");
}

TEST_F (
    LumexMockStreamTest,
    GivenAHeldWrite_WhenAnotherWriteIsStartedBeforeTheReleaseRuns_ThenItDoesNotOvertakeTheHeldOne)
{
  open ();
  mock_.control.hold_writes ();
  std::shared_ptr<write_state> const a = start_write ("A");
  drain (context_);                                         // A is held
  std::shared_ptr<write_state> const b = start_write ("B"); // not run yet
  mock_.control.release_writes ();
  drain (context_);
  ASSERT_TRUE (a->done && b->done);
  EXPECT_EQ (text_of (mock_.control.written ()), "AB");
}

TEST_F (LumexMockStreamTest, GivenHeldWrites_WhenReadsRun_ThenReadsAreNotHeld)
{
  open ();
  mock_.control.hold_writes ();
  std::shared_ptr<write_state> const write = start_write ("x");
  mock_.control.feed (bytes_of ("in"));
  std::shared_ptr<read_state> const read = start_read (8);
  drain (context_);
  EXPECT_TRUE (read->done);
  EXPECT_FALSE (write->done);
  mock_.control.release_writes ();
  drain (context_);
  EXPECT_TRUE (write->done);
}

TEST_F (LumexMockStreamTest,
        GivenNoHold_WhenWritesAreReleased_ThenNothingChanges)
{
  open ();
  mock_.control.release_writes ();
  std::shared_ptr<write_state> const write = start_write ("ok");
  drain (context_);
  EXPECT_TRUE (write->done);
  EXPECT_EQ (text_of (mock_.control.written ()), "ok");
}

// --- an empty read ----------------------------------------------------------

TEST_F (
    LumexMockStreamTest,
    GivenAnEmptyRead_WhenItCompletes_ThenItIsNeitherAnErrorNorADisconnectAndBytesStayForTheNextRead)
{
  open ();
  mock_.control.feed (bytes_of ("data"));
  mock_.control.complete_next_read_empty ();
  std::shared_ptr<read_state> const empty = start_read (8);
  drain (context_);
  ASSERT_TRUE (empty->done);
  EXPECT_FALSE (empty->error);
  EXPECT_EQ (empty->count, 0u);
  EXPECT_TRUE (mock_.stream.is_open ());
  std::shared_ptr<read_state> const next = start_read (8);
  drain (context_);
  ASSERT_TRUE (next->done);
  EXPECT_FALSE (next->error);
  EXPECT_EQ (next->text (), "data");
}

TEST_F (LumexMockStreamTest,
        GivenAPendingRead_WhenAnEmptyReadIsScripted_ThenThatReadEndsEmptyNow)
{
  open ();
  std::shared_ptr<read_state> const read = start_read (8);
  drain (context_);
  ASSERT_FALSE (read->done);
  mock_.control.complete_next_read_empty ();
  drain (context_);
  ASSERT_TRUE (read->done);
  EXPECT_FALSE (read->error);
  EXPECT_EQ (read->count, 0u);
  // Only one read was emptied: the next one waits.
  std::shared_ptr<read_state> const next = start_read (8);
  drain (context_);
  EXPECT_FALSE (next->done);
  mock_.control.feed (bytes_of ("z"));
  drain (context_);
  ASSERT_TRUE (next->done);
  EXPECT_EQ (next->text (), "z");
}

// --- several pending reads (a second read used to drop the first) -----------

TEST_F (
    LumexMockStreamTest,
    GivenTwoPendingReads_WhenBytesArrive_ThenBothAreServedInTheOrderStarted)
{
  open ();
  std::shared_ptr<read_state> const first = start_read (8);
  std::shared_ptr<read_state> const second = start_read (8);
  drain (context_);
  EXPECT_FALSE (first->done || second->done);
  mock_.control.feed (bytes_of ("abc"));
  drain (context_);
  ASSERT_TRUE (first->done);
  EXPECT_EQ (first->text (), "abc");
  EXPECT_FALSE (second->done);
  mock_.control.feed (bytes_of ("de"));
  drain (context_);
  ASSERT_TRUE (second->done);
  EXPECT_EQ (second->text (), "de");
}

TEST_F (LumexMockStreamTest,
        GivenTwoPendingReadsAndMoreBytes_WhenFed_ThenOneFeedServesBoth)
{
  open ();
  mock_.control.set_read_chunk (2);
  std::shared_ptr<read_state> const first = start_read (8);
  std::shared_ptr<read_state> const second = start_read (8);
  drain (context_);
  mock_.control.feed (bytes_of ("wxyz"));
  drain (context_);
  ASSERT_TRUE (first->done && second->done);
  EXPECT_EQ (first->text (), "wx");
  EXPECT_EQ (second->text (), "yz");
}

TEST_F (LumexMockStreamTest,
        GivenTwoPendingReads_WhenCancelled_ThenBothEndCancelled)
{
  open ();
  std::shared_ptr<read_state> const first = start_read (8);
  std::shared_ptr<read_state> const second = start_read (8);
  drain (context_);
  mock_.stream.cancel ();
  drain (context_);
  ASSERT_TRUE (first->done && second->done);
  EXPECT_EQ (first->error, code (transport_errc::cancelled));
  EXPECT_EQ (second->error, code (transport_errc::cancelled));
}

TEST_F (LumexMockStreamTest,
        GivenTwoPendingReads_WhenTheMockDisconnects_ThenBothEndDisconnected)
{
  open ();
  std::shared_ptr<read_state> const first = start_read (8);
  std::shared_ptr<read_state> const second = start_read (8);
  drain (context_);
  mock_.control.disconnect ();
  drain (context_);
  ASSERT_TRUE (first->done && second->done);
  EXPECT_EQ (first->error, code (transport_errc::disconnected));
  EXPECT_EQ (second->error, code (transport_errc::disconnected));
}

TEST_F (
    LumexMockStreamTest,
    GivenTwoPendingReads_WhenTheNextReadFails_ThenTheOldestEndsWithTheError)
{
  open ();
  std::shared_ptr<read_state> const first = start_read (8);
  std::shared_ptr<read_state> const second = start_read (8);
  drain (context_);
  mock_.control.fail_next_read (std::make_error_code (std::errc::io_error));
  drain (context_);
  ASSERT_TRUE (first->done);
  EXPECT_EQ (first->error, std::make_error_code (std::errc::io_error));
  EXPECT_FALSE (second->done);
  mock_.control.feed (bytes_of ("k"));
  drain (context_);
  ASSERT_TRUE (second->done);
  EXPECT_EQ (second->text (), "k");
}

TEST_F (LumexMockStreamTest,
        GivenTwoPendingReads_WhenTheStreamIsClosed_ThenBothEndCancelled)
{
  open ();
  std::shared_ptr<read_state> const first = start_read (8);
  std::shared_ptr<read_state> const second = start_read (8);
  drain (context_);
  mock_.stream.close ();
  drain (context_);
  ASSERT_TRUE (first->done && second->done);
  EXPECT_EQ (first->error, code (transport_errc::cancelled));
  EXPECT_EQ (second->error, code (transport_errc::cancelled));
}

// --- shutdown ---------------------------------------------------------------

TEST (
    LumexMockStream,
    GivenAScriptHoldingHandlers_WhenTheContextIsDestroyed_ThenEverythingItKeptIsReleased)
{
  std::shared_ptr<int> const sentinel = std::make_shared<int> (0);
  std::unique_ptr<asio::io_context> context (new asio::io_context);
  virtual_clock clock;
  mock_options_t options;
  options.timers = clock.source ();
  // The test keeps no reference to the implementation past the shutdown: the
  // observer (which owns a control) is the last one, and the shutdown drops
  // it.
  std::unique_ptr<mock_stream> made (
      new mock_stream (make_mock_stream (context->get_executor (), options)));
  std::unique_ptr<tr::stream::byte_stream> stream (
      new tr::stream::byte_stream (std::move (made->stream)));
  tr::testing::mock_stream_control &control = made->control;
  bool opened = false;
  stream->async_open ([&opened] (std::error_code const &) { opened = true; });
  drain (*context);
  ASSERT_TRUE (opened);
  control.on_write (cyclic_observer (control, sentinel));
  bytes_t first_buffer (4);
  bytes_t second_buffer (4);
  bytes_t write_data = bytes_of ("w");
  stream->async_read_some (
      tr::mutable_bytes_t (first_buffer),
      [sentinel] (std::error_code const &, std::size_t) {});
  stream->async_read_some (
      tr::mutable_bytes_t (second_buffer),
      [sentinel] (std::error_code const &, std::size_t) {});
  control.hold_writes ();
  stream->async_write_some (
      tr::const_bytes_t (write_data),
      [sentinel] (std::error_code const &, std::size_t) {});
  control.feed_after (ms (10), bytes_of ("later"));
  drain (*context);
  EXPECT_GT (sentinel.use_count (), 1);
  // The stream and the test's control go first (the close step of the stream
  // dies queued with the context); the shutdown drops what the mock kept.
  stream.reset ();
  made.reset ();
  context.reset ();
  EXPECT_EQ (sentinel.use_count (), 1);
}

// --- another thread ---------------------------------------------------------

TEST (
    LumexMockStream,
    GivenAControlUsedFromAnotherThread_WhenTheRunnerServesReads_ThenEveryScriptedByteArrives)
{
  tr::stream::io_runner runner (1);
  mock_stream made = make_mock_stream (runner.get_executor (), "threads");
  ASSERT_TRUE (made.stream.open (ms (2000)).has_value ());
  std::atomic<int> observed (0);
  made.control.on_write ([&observed] (bytes_t const &) { ++observed; });
  tr::testing::mock_stream_control control = made.control;
  std::thread script (
      [control] () mutable
        {
          for (int i = 0; i < 100; ++i)
            {
              control.feed_after (ms (i % 3),
                                  bytes_t (1, static_cast<std::uint8_t> (i)));
              if (i % 10 == 0)
                control.set_read_chunk (static_cast<std::size_t> (1 + i % 4));
            }
        });
  bytes_t const data = bytes_of ("ping");
  for (int i = 0; i < 20; ++i)
    ASSERT_TRUE (made.stream.write_all (tr::const_bytes_t (data), ms (2000))
                     .has_value ());
  std::size_t total = 0;
  bytes_t buffer (16);
  while (total < 100)
    {
      tr::io_result_t const result
          = made.stream.read_some (tr::mutable_bytes_t (buffer), ms (3000));
      ASSERT_TRUE (result.has_value ());
      total += *result;
    }
  script.join ();
  EXPECT_EQ (total, 100u);
  EXPECT_EQ (observed.load (), 20);
  made.control.on_write (tr::testing::write_observer_t ());
  made.stream.close ();
}

TEST (
    LumexMockStream,
    GivenAnObserverThatOwnsTheControl_WhenOnlyTheContextEnds_ThenTheShutdownDropsIt)
{
  std::shared_ptr<int> const sentinel = std::make_shared<int> (0);
  std::unique_ptr<asio::io_context> context (new asio::io_context);
  std::unique_ptr<mock_stream> made (
      new mock_stream (make_mock_stream (context->get_executor (), "cycle")));
  // No operation was ever started: only the observer enlists the mock.
  made->control.on_write (cyclic_observer (made->control, sentinel));
  EXPECT_GT (sentinel.use_count (), 1);
  made.reset ();
  context.reset ();
  EXPECT_EQ (sentinel.use_count (), 1);
}

TEST (
    LumexMockStream,
    GivenPendingReadsOwningTheControl_WhenTheContextIsDestroyed_ThenTheShutdownDropsThem)
{
  std::shared_ptr<int> const sentinel = std::make_shared<int> (0);
  std::unique_ptr<asio::io_context> context (new asio::io_context);
  std::unique_ptr<mock_stream> made (
      new mock_stream (make_mock_stream (context->get_executor (), "cycle")));
  std::unique_ptr<tr::stream::byte_stream> stream (
      new tr::stream::byte_stream (std::move (made->stream)));
  bool opened = false;
  stream->async_open ([&opened] (std::error_code const &) { opened = true; });
  drain (*context);
  ASSERT_TRUE (opened);
  bytes_t first (4);
  bytes_t second (4);
  start_cyclic_reads (*stream, made->control, sentinel, first, second);
  drain (*context);
  EXPECT_GT (sentinel.use_count (), 2);
  stream.reset ();
  made.reset ();
  context.reset (); // the handlers of the two reads were the last owners
  EXPECT_EQ (sentinel.use_count (), 1);
}

TEST (
    LumexMockStream,
    GivenAHeldWriteOwningTheControl_WhenTheContextIsDestroyed_ThenTheShutdownDropsIt)
{
  std::shared_ptr<int> const sentinel = std::make_shared<int> (0);
  std::unique_ptr<asio::io_context> context (new asio::io_context);
  std::unique_ptr<mock_stream> made (
      new mock_stream (make_mock_stream (context->get_executor (), "cycle")));
  std::unique_ptr<tr::stream::byte_stream> stream (
      new tr::stream::byte_stream (std::move (made->stream)));
  bool opened = false;
  stream->async_open ([&opened] (std::error_code const &) { opened = true; });
  drain (*context);
  ASSERT_TRUE (opened);
  made->control.hold_writes ();
  bytes_t data = bytes_of ("w");
  start_cyclic_write (*stream, made->control, sentinel, data);
  drain (*context);
  EXPECT_GT (sentinel.use_count (), 1);
  stream.reset ();
  made.reset ();
  context.reset (); // the handler of the held write was the last owner
  EXPECT_EQ (sentinel.use_count (), 1);
}
