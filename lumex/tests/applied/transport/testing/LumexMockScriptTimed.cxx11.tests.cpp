// The timed script of the mocks: an empty read and a disconnect after a delay
// of the mock's timer source (ordered with `feed_after` and `deliver_after`),
// and opens that fail for a while (a cable that is plugged back later).

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/LumexTransport"

namespace
{
namespace tr = lumex::applied::transport;
using ms = std::chrono::milliseconds;
using bytes_t = std::vector<std::uint8_t>;
using tr::datagram::message_t;
using tr::error::transport_errc;
using tr::testing::mock_options_t;
using tr::testing::virtual_clock;

using stream_mock_t = decltype (tr::testing::make_mock_stream (
    std::declval<tr::executor_type const &> (),
    std::declval<mock_options_t const &> ()));
using datagram_mock_t = decltype (tr::testing::make_mock_datagram (
    std::declval<tr::executor_type const &> (),
    std::declval<mock_options_t const &> ()));

bytes_t
bytes_of (std::string const &text)
{
  return bytes_t (text.begin (), text.end ());
}

std::error_code
code (transport_errc value)
{
  return make_error_code (value);
}

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

struct read_state_t
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

struct status_state_t
{
  bool done = false;
  std::error_code error;
};

struct receive_state_t
{
  bool done = false;
  std::error_code error;
  message_t message;
};

class LumexMockScriptTimedStreamTest : public ::testing::Test
{
protected:
  asio::io_context context_;
  virtual_clock clock_;
  stream_mock_t mock_
      = tr::testing::make_mock_stream (context_.get_executor (), scripted ());

  mock_options_t
  scripted () const
  {
    mock_options_t options;
    options.name = "timed";
    options.timers = clock_.source ();
    return options;
  }

  std::shared_ptr<status_state_t>
  start_open ()
  {
    std::shared_ptr<status_state_t> const state
        = std::make_shared<status_state_t> ();
    mock_.stream.async_open (
        [state] (std::error_code const &error)
          {
            state->done = true;
            state->error = error;
          });
    drain (context_);
    return state;
  }

  void
  open ()
  {
    std::shared_ptr<status_state_t> const state = start_open ();
    ASSERT_TRUE (state->done);
    ASSERT_FALSE (state->error);
  }

  std::shared_ptr<read_state_t>
  start_read (std::size_t capacity)
  {
    std::shared_ptr<read_state_t> const state
        = std::make_shared<read_state_t> ();
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
};

class LumexMockScriptTimedDatagramTest : public ::testing::Test
{
protected:
  asio::io_context context_;
  virtual_clock clock_;
  datagram_mock_t mock_ = tr::testing::make_mock_datagram (
      context_.get_executor (), scripted ());

  mock_options_t
  scripted () const
  {
    mock_options_t options;
    options.name = "timed";
    options.timers = clock_.source ();
    return options;
  }

  std::shared_ptr<status_state_t>
  start_open ()
  {
    std::shared_ptr<status_state_t> const state
        = std::make_shared<status_state_t> ();
    mock_.transport.async_open (
        [state] (std::error_code const &error)
          {
            state->done = true;
            state->error = error;
          });
    drain (context_);
    return state;
  }

  void
  open ()
  {
    std::shared_ptr<status_state_t> const state = start_open ();
    ASSERT_TRUE (state->done);
    ASSERT_FALSE (state->error);
  }

  std::shared_ptr<receive_state_t>
  start_receive ()
  {
    std::shared_ptr<receive_state_t> const state
        = std::make_shared<receive_state_t> ();
    mock_.transport.async_receive (
        [state] (std::error_code const &error, message_t message)
          {
            state->done = true;
            state->error = error;
            state->message = std::move (message);
          });
    return state;
  }
};

message_t
message_of (std::uint8_t report_id, std::string const &text)
{
  message_t message;
  message.report_id = report_id;
  message.payload.assign (text.begin (), text.end ());
  return message;
}
} // namespace

// --- the stream

TEST_F (LumexMockScriptTimedStreamTest,
        GivenEmptyReadAfter_WhenTheClockReachesIt_ThenThePendingReadEndsEmpty)
{
  open ();
  std::shared_ptr<read_state_t> const read = start_read (16);
  mock_.control.empty_read_after (ms (50));
  drain (context_);
  clock_.advance (ms (49));
  drain (context_);
  EXPECT_FALSE (read->done);
  clock_.advance (ms (1));
  drain (context_);
  ASSERT_TRUE (read->done);
  EXPECT_FALSE (read->error);
  EXPECT_EQ (read->count, 0u);
}

TEST_F (
    LumexMockScriptTimedStreamTest,
    GivenEmptyReadAfter_WhenBytesWaitWhenItComes_ThenTheNextReadIsEmptyAndTheOneAfterGetsTheBytes)
{
  open ();
  mock_.control.feed (bytes_of ("ab"));
  mock_.control.empty_read_after (ms (10));
  clock_.advance (ms (10));
  drain (context_);
  std::shared_ptr<read_state_t> const first = start_read (16);
  drain (context_);
  ASSERT_TRUE (first->done);
  EXPECT_EQ (first->count, 0u);
  std::shared_ptr<read_state_t> const second = start_read (16);
  drain (context_);
  ASSERT_TRUE (second->done);
  EXPECT_EQ (second->text (), "ab");
}

TEST_F (
    LumexMockScriptTimedStreamTest,
    GivenEmptyReadAndFeedAtTheSameTime_WhenTheyCome_ThenTheyKeepTheScheduleOrder)
{
  open ();
  mock_.control.empty_read_after (ms (10));
  mock_.control.feed_after (ms (10), bytes_of ("x"));
  mock_.control.empty_read_after (ms (10));
  mock_.control.feed_after (ms (10), bytes_of ("y"));
  clock_.advance (ms (10));
  drain (context_);
  std::vector<std::shared_ptr<read_state_t>> reads;
  for (int index = 0; index < 3; ++index)
    {
      reads.push_back (start_read (16));
      drain (context_);
    }
  ASSERT_TRUE (reads[0]->done && reads[1]->done && reads[2]->done);
  EXPECT_EQ (reads[0]->count, 0u);
  EXPECT_EQ (reads[1]->count, 0u);
  EXPECT_EQ (reads[2]->text (), "xy");
}

TEST_F (LumexMockScriptTimedStreamTest,
        GivenDisconnectAfter_WhenTheClockReachesIt_ThenThePendingReadEnds)
{
  open ();
  std::shared_ptr<read_state_t> const read = start_read (16);
  mock_.control.disconnect_after (ms (30));
  drain (context_);
  clock_.advance (ms (29));
  drain (context_);
  EXPECT_FALSE (read->done);
  clock_.advance (ms (1));
  drain (context_);
  ASSERT_TRUE (read->done);
  EXPECT_EQ (read->error, code (transport_errc::disconnected));
}

TEST_F (
    LumexMockScriptTimedStreamTest,
    GivenFeedThenDisconnectAtTheSameTime_WhenTheyCome_ThenTheBytesAreReadFirst)
{
  open ();
  mock_.control.feed_after (ms (10), bytes_of ("last"));
  mock_.control.disconnect_after (ms (10));
  clock_.advance (ms (10));
  drain (context_);
  std::shared_ptr<read_state_t> const first = start_read (16);
  drain (context_);
  ASSERT_TRUE (first->done);
  EXPECT_EQ (first->text (), "last");
  std::shared_ptr<read_state_t> const second = start_read (16);
  drain (context_);
  ASSERT_TRUE (second->done);
  EXPECT_EQ (second->error, code (transport_errc::disconnected));
}

TEST_F (
    LumexMockScriptTimedStreamTest,
    GivenDisconnectAfterThenAReopen_WhenItComesBeforeTheReopen_ThenTheReopenClearsIt)
{
  open ();
  mock_.control.disconnect_after (ms (10));
  clock_.advance (ms (10));
  drain (context_);
  std::shared_ptr<read_state_t> const read = start_read (16);
  drain (context_);
  ASSERT_TRUE (read->done);
  EXPECT_EQ (read->error, code (transport_errc::disconnected));
  // A reopen clears the disconnect: the script is spent, the link is fine.
  std::shared_ptr<status_state_t> const again = start_open ();
  ASSERT_TRUE (again->done);
  EXPECT_FALSE (again->error);
  mock_.control.feed (bytes_of ("ok"));
  std::shared_ptr<read_state_t> const after = start_read (16);
  drain (context_);
  ASSERT_TRUE (after->done);
  EXPECT_EQ (after->text (), "ok");
}

TEST_F (
    LumexMockScriptTimedStreamTest,
    GivenTimedActionsPending_WhenTheMockIsDestroyed_ThenTheClockFindsNothing)
{
  std::unique_ptr<stream_mock_t> made (new stream_mock_t (
      tr::testing::make_mock_stream (context_.get_executor (), scripted ())));
  made->control.empty_read_after (ms (10));
  made->control.disconnect_after (ms (10));
  made->control.feed_after (ms (10), bytes_of ("never"));
  EXPECT_EQ (clock_.pending (), 3u);
  made.reset ();
  drain (context_);
  EXPECT_EQ (clock_.pending (), 0u);
  clock_.advance (ms (20));
  drain (context_);
}

TEST_F (LumexMockScriptTimedStreamTest,
        GivenFailOpensFor_WhenOpensComeInsideTheWindow_ThenEachFails)
{
  mock_.control.fail_opens_for (ms (100), code (transport_errc::not_open));
  for (int attempt = 0; attempt < 3; ++attempt)
    {
      std::shared_ptr<status_state_t> const state = start_open ();
      ASSERT_TRUE (state->done);
      EXPECT_EQ (state->error, code (transport_errc::not_open));
    }
  EXPECT_EQ (mock_.control.open_count (), 0u);
  clock_.advance (ms (99));
  drain (context_);
  std::shared_ptr<status_state_t> const late = start_open ();
  ASSERT_TRUE (late->done);
  EXPECT_EQ (late->error, code (transport_errc::not_open));
}

TEST_F (LumexMockScriptTimedStreamTest,
        GivenFailOpensFor_WhenTheWindowEnds_ThenTheOpenSucceeds)
{
  mock_.control.fail_opens_for (ms (100), code (transport_errc::not_open));
  clock_.advance (ms (100));
  drain (context_);
  std::shared_ptr<status_state_t> const state = start_open ();
  ASSERT_TRUE (state->done);
  EXPECT_FALSE (state->error);
  EXPECT_EQ (mock_.control.open_count (), 1u);
}

TEST_F (LumexMockScriptTimedStreamTest,
        GivenFailOpensForOfZero_WhenOpened_ThenItSucceeds)
{
  mock_.control.fail_opens_for (ms (0), code (transport_errc::not_open));
  std::shared_ptr<status_state_t> const state = start_open ();
  ASSERT_TRUE (state->done);
  EXPECT_FALSE (state->error);
}

TEST_F (
    LumexMockScriptTimedStreamTest,
    GivenFailOpensForAfterTheTimeStarted_WhenOpened_ThenTheWindowCountsFromTheCall)
{
  clock_.advance (ms (500));
  mock_.control.fail_opens_for (ms (100), code (transport_errc::not_open));
  clock_.advance (ms (99));
  drain (context_);
  EXPECT_EQ (start_open ()->error, code (transport_errc::not_open));
  clock_.advance (ms (1));
  drain (context_);
  EXPECT_FALSE (start_open ()->error);
}

// --- the datagram

TEST_F (LumexMockScriptTimedDatagramTest,
        GivenDisconnectAfter_WhenTheClockReachesIt_ThenThePendingReceiveEnds)
{
  open ();
  std::shared_ptr<receive_state_t> const receive = start_receive ();
  mock_.control.disconnect_after (ms (30));
  drain (context_);
  clock_.advance (ms (29));
  drain (context_);
  EXPECT_FALSE (receive->done);
  clock_.advance (ms (1));
  drain (context_);
  ASSERT_TRUE (receive->done);
  EXPECT_EQ (receive->error, code (transport_errc::disconnected));
}

TEST_F (
    LumexMockScriptTimedDatagramTest,
    GivenDeliveryThenDisconnectAtTheSameTime_WhenTheyCome_ThenTheMessageIsReceivedFirst)
{
  open ();
  mock_.control.deliver_after (ms (10), message_of (2, "last"));
  mock_.control.disconnect_after (ms (10));
  clock_.advance (ms (10));
  drain (context_);
  std::shared_ptr<receive_state_t> const first = start_receive ();
  drain (context_);
  ASSERT_TRUE (first->done);
  EXPECT_FALSE (first->error);
  EXPECT_EQ (first->message.report_id, 2);
  std::shared_ptr<receive_state_t> const second = start_receive ();
  drain (context_);
  ASSERT_TRUE (second->done);
  EXPECT_EQ (second->error, code (transport_errc::disconnected));
}

TEST_F (
    LumexMockScriptTimedDatagramTest,
    GivenTimedActionsPending_WhenTheMockIsDestroyed_ThenTheClockFindsNothing)
{
  std::unique_ptr<datagram_mock_t> made (
      new datagram_mock_t (tr::testing::make_mock_datagram (
          context_.get_executor (), scripted ())));
  made->control.disconnect_after (ms (10));
  made->control.deliver_after (ms (10), message_of (0, "never"));
  EXPECT_EQ (clock_.pending (), 2u);
  made.reset ();
  drain (context_);
  EXPECT_EQ (clock_.pending (), 0u);
  clock_.advance (ms (20));
  drain (context_);
}

TEST_F (
    LumexMockScriptTimedDatagramTest,
    GivenFailOpensFor_WhenOpensComeInsideTheWindow_ThenEachFailsUntilItEnds)
{
  mock_.control.fail_opens_for (ms (100), code (transport_errc::not_open));
  for (int attempt = 0; attempt < 3; ++attempt)
    {
      std::shared_ptr<status_state_t> const state = start_open ();
      ASSERT_TRUE (state->done);
      EXPECT_EQ (state->error, code (transport_errc::not_open));
    }
  clock_.advance (ms (99));
  drain (context_);
  EXPECT_EQ (start_open ()->error, code (transport_errc::not_open));
  clock_.advance (ms (1));
  drain (context_);
  EXPECT_FALSE (start_open ()->error);
}
