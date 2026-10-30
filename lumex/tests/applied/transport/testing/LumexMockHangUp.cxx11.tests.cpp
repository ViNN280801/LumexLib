// A hung-up link of the mocks (writes and sends fail, later bytes are
// dropped) and the connection a scheduled step belongs to (a step of a link
// that was closed or opened again is dropped).

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

class LumexMockHangUpStreamTest : public ::testing::Test
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

  std::shared_ptr<status_state_t>
  start_write (std::string const &text)
  {
    std::shared_ptr<status_state_t> const state
        = std::make_shared<status_state_t> ();
    std::shared_ptr<bytes_t> const data
        = std::make_shared<bytes_t> (bytes_of (text));
    mock_.stream.async_write_some (
        tr::const_bytes_t (*data),
        [state, data] (std::error_code const &error, std::size_t)
          {
            state->done = true;
            state->error = error;
          });
    drain (context_);
    return state;
  }

  void
  reopen ()
  {
    mock_.stream.close ();
    drain (context_);
    open ();
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

class LumexMockHangUpDatagramTest : public ::testing::Test
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

  std::shared_ptr<status_state_t>
  start_send (message_t message)
  {
    std::shared_ptr<status_state_t> const state
        = std::make_shared<status_state_t> ();
    mock_.transport.async_send (std::move (message),
                                [state] (std::error_code const &error)
                                  {
                                    state->done = true;
                                    state->error = error;
                                  });
    drain (context_);
    return state;
  }

  void
  reopen ()
  {
    mock_.transport.close ();
    drain (context_);
    open ();
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

std::string
text_of (message_t const &message)
{
  return std::string (message.payload.begin (), message.payload.end ());
}

message_t
message_of (std::uint8_t report_id, std::string const &text)
{
  message_t message;
  message.report_id = report_id;
  message.payload.assign (text.begin (), text.end ());
  return message;
}
} // namespace

// --- a hung-up stream

TEST_F (LumexMockHangUpStreamTest,
        GivenAHungUpLink_WhenWritten_ThenItFailsAndIsNeitherRecordedNorSeen)
{
  open ();
  int seen = 0;
  mock_.control.on_write ([&seen] (bytes_t const &) { ++seen; });
  mock_.control.disconnect ();
  drain (context_);
  std::shared_ptr<status_state_t> const write = start_write ("late");
  ASSERT_TRUE (write->done);
  EXPECT_EQ (write->error, code (transport_errc::disconnected));
  EXPECT_TRUE (mock_.control.written ().empty ());
  EXPECT_EQ (seen, 0);
  mock_.control.on_write (tr::testing::write_observer_t ());
}

TEST_F (LumexMockHangUpStreamTest,
        GivenAHungUpLinkAndHeldWrites_WhenWritten_ThenItFailsAtOnce)
{
  open ();
  mock_.control.hold_writes ();
  mock_.control.disconnect ();
  drain (context_);
  std::shared_ptr<status_state_t> const write = start_write ("late");
  ASSERT_TRUE (write->done);
  EXPECT_EQ (write->error, code (transport_errc::disconnected));
}

TEST_F (
    LumexMockHangUpStreamTest,
    GivenAHungUpLink_WhenBytesAreFed_ThenTheOnesBeforeAreReadAndTheOnesAfterAreDropped)
{
  open ();
  mock_.control.feed (bytes_of ("before"));
  mock_.control.disconnect ();
  mock_.control.feed (bytes_of ("after"));
  mock_.control.feed_after (ms (5), bytes_of ("timed"));
  mock_.control.feed_after_on_link (ms (5), bytes_of ("linked"));
  clock_.advance (ms (5));
  drain (context_);
  std::shared_ptr<read_state_t> const first = start_read (64);
  drain (context_);
  ASSERT_TRUE (first->done);
  EXPECT_EQ (first->text (), "before");
  std::shared_ptr<read_state_t> const second = start_read (64);
  drain (context_);
  ASSERT_TRUE (second->done);
  EXPECT_EQ (second->error, code (transport_errc::disconnected));
  // Nothing was kept for the next link.
  reopen ();
  std::shared_ptr<read_state_t> const third = start_read (64);
  clock_.advance (ms (10));
  drain (context_);
  EXPECT_FALSE (third->done);
}

TEST_F (LumexMockHangUpStreamTest,
        GivenAHungUpLink_WhenItIsOpenedAgain_ThenWritesWorkAgain)
{
  open ();
  mock_.control.disconnect ();
  drain (context_);
  reopen ();
  std::shared_ptr<status_state_t> const write = start_write ("again");
  ASSERT_TRUE (write->done);
  EXPECT_FALSE (write->error);
  EXPECT_EQ (mock_.control.written (), bytes_of ("again"));
}

TEST_F (LumexMockHangUpStreamTest,
        GivenAHeldWrite_WhenADisconnectAfterComes_ThenTheWriteEndsDisconnected)
{
  open ();
  mock_.control.hold_writes ();
  std::shared_ptr<status_state_t> const write = start_write ("held");
  EXPECT_FALSE (write->done);
  mock_.control.disconnect_after (ms (10));
  drain (context_);
  clock_.advance (ms (9));
  drain (context_);
  EXPECT_FALSE (write->done);
  clock_.advance (ms (1));
  drain (context_);
  ASSERT_TRUE (write->done);
  EXPECT_EQ (write->error, code (transport_errc::disconnected));
}

TEST_F (
    LumexMockHangUpStreamTest,
    GivenADisconnectAfter_WhenTheLinkIsClosedAndOpenedAgain_ThenTheNewLinkIsKept)
{
  open ();
  mock_.control.disconnect_after (ms (10));
  reopen ();
  clock_.advance (ms (20));
  drain (context_);
  std::shared_ptr<read_state_t> const read = start_read (16);
  mock_.control.feed (bytes_of ("alive"));
  drain (context_);
  ASSERT_TRUE (read->done);
  EXPECT_FALSE (read->error);
  EXPECT_EQ (read->text (), "alive");
  EXPECT_TRUE (start_write ("ok")->done);
}

TEST_F (LumexMockHangUpStreamTest,
        GivenAnEmptyReadAfter_WhenTheLinkIsOpenedAgain_ThenItIsDropped)
{
  open ();
  mock_.control.empty_read_after (ms (10));
  reopen ();
  clock_.advance (ms (20));
  drain (context_);
  mock_.control.feed (bytes_of ("data"));
  std::shared_ptr<read_state_t> const read = start_read (16);
  drain (context_);
  ASSERT_TRUE (read->done);
  EXPECT_EQ (read->text (), "data");
}

TEST_F (LumexMockHangUpStreamTest,
        GivenFeedsAfter_WhenTheLinkIsOpenedAgain_ThenOnlyTheLinkedOneIsDropped)
{
  open ();
  mock_.control.feed_after (ms (10), bytes_of ("plain"));
  mock_.control.feed_after_on_link (ms (10), bytes_of ("linked"));
  reopen ();
  clock_.advance (ms (10));
  drain (context_);
  std::shared_ptr<read_state_t> const read = start_read (64);
  drain (context_);
  ASSERT_TRUE (read->done);
  EXPECT_EQ (read->text (), "plain");
}

TEST_F (LumexMockHangUpStreamTest,
        GivenAFeedAfterOnLink_WhenTheLinkStaysUp_ThenItArrivesAtItsTime)
{
  open ();
  mock_.control.feed_after_on_link (ms (10), bytes_of ("linked"));
  std::shared_ptr<read_state_t> const read = start_read (64);
  clock_.advance (ms (9));
  drain (context_);
  EXPECT_FALSE (read->done);
  clock_.advance (ms (1));
  drain (context_);
  ASSERT_TRUE (read->done);
  EXPECT_EQ (read->text (), "linked");
}

TEST_F (
    LumexMockHangUpStreamTest,
    GivenAStepScheduledBeforeTheFirstOpen_WhenTheLinkOpens_ThenTheOpenStartsANewConnectionAndItIsDropped)
{
  mock_.control.disconnect_after (ms (10));
  open ();
  clock_.advance (ms (20));
  drain (context_);
  EXPECT_TRUE (start_write ("ok")->done);
  EXPECT_FALSE (start_write ("ok")->error);
}

TEST_F (
    LumexMockHangUpStreamTest,
    GivenAFeedAfterOnLink_WhenTheLinkIsClosedBeforeItComes_ThenNothingWaitsForTheNextOpen)
{
  open ();
  mock_.control.feed_after_on_link (ms (10), bytes_of ("linked"));
  mock_.stream.close ();
  drain (context_);
  clock_.advance (ms (20));
  drain (context_);
  open ();
  std::shared_ptr<read_state_t> const read = start_read (64);
  drain (context_);
  EXPECT_FALSE (read->done);
}

TEST_F (LumexMockHangUpStreamTest,
        GivenASourceThatReadsTheMockInNow_WhenOpened_ThenNothingDeadlocks)
{
  // The time is read before the mock takes its lock: a source may call the
  // mock's control (which takes it) from `now ()`.
  struct source_t : tr::timing::timer_source_impl
  {
    tr::testing::mock_stream_control *control = nullptr;

    tr::timing::time_point_t
    now () const override
    {
      if (control != nullptr)
        static_cast<void> (control->open_count ());
      return tr::timing::time_point_t ();
    }

    std::shared_ptr<tr::timing::timer_impl>
    make_timer (tr::executor_type const &) const override
    {
      return std::shared_ptr<tr::timing::timer_impl> ();
    }
  };
  std::shared_ptr<source_t> const source = std::make_shared<source_t> ();
  mock_options_t options;
  options.timers = tr::timing::timer_source (source);
  stream_mock_t made
      = tr::testing::make_mock_stream (context_.get_executor (), options);
  tr::testing::mock_stream_control control = made.control;
  source->control = &control;
  bool done = false;
  made.stream.async_open ([&done] (std::error_code const &) { done = true; });
  drain (context_);
  EXPECT_TRUE (done);
  source->control = nullptr;
}

// --- a hung-up datagram transport

TEST_F (LumexMockHangUpDatagramTest,
        GivenAHungUpLink_WhenSent_ThenItFailsAndIsNeitherRecordedNorSeen)
{
  open ();
  int seen = 0;
  mock_.control.on_send ([&seen] (message_t const &) { ++seen; });
  mock_.control.disconnect ();
  drain (context_);
  std::shared_ptr<status_state_t> const send
      = start_send (message_of (1, "late"));
  ASSERT_TRUE (send->done);
  EXPECT_EQ (send->error, code (transport_errc::disconnected));
  EXPECT_TRUE (mock_.control.sent ().empty ());
  EXPECT_EQ (seen, 0);
  mock_.control.on_send (tr::testing::send_observer_t ());
}

TEST_F (LumexMockHangUpDatagramTest,
        GivenAHungUpLinkAndHeldSends_WhenSent_ThenItFailsAtOnce)
{
  open ();
  mock_.control.hold_sends ();
  mock_.control.disconnect ();
  drain (context_);
  std::shared_ptr<status_state_t> const send
      = start_send (message_of (1, "late"));
  ASSERT_TRUE (send->done);
  EXPECT_EQ (send->error, code (transport_errc::disconnected));
}

TEST_F (
    LumexMockHangUpDatagramTest,
    GivenAHungUpLink_WhenMessagesAreDelivered_ThenTheOnesBeforeAreReceivedAndTheOnesAfterAreDropped)
{
  open ();
  mock_.control.deliver (message_of (1, "before"));
  mock_.control.disconnect ();
  mock_.control.deliver (message_of (1, "after"));
  mock_.control.deliver_after (ms (5), message_of (1, "timed"));
  mock_.control.deliver_after_on_link (ms (5), message_of (1, "linked"));
  clock_.advance (ms (5));
  drain (context_);
  std::shared_ptr<receive_state_t> const first = start_receive ();
  drain (context_);
  ASSERT_TRUE (first->done);
  EXPECT_EQ (text_of (first->message), "before");
  std::shared_ptr<receive_state_t> const second = start_receive ();
  drain (context_);
  ASSERT_TRUE (second->done);
  EXPECT_EQ (second->error, code (transport_errc::disconnected));
  reopen ();
  std::shared_ptr<receive_state_t> const third = start_receive ();
  clock_.advance (ms (10));
  drain (context_);
  EXPECT_FALSE (third->done);
}

TEST_F (LumexMockHangUpDatagramTest,
        GivenAHungUpLink_WhenItIsOpenedAgain_ThenSendsWorkAgain)
{
  open ();
  mock_.control.disconnect ();
  drain (context_);
  reopen ();
  std::shared_ptr<status_state_t> const send
      = start_send (message_of (1, "again"));
  ASSERT_TRUE (send->done);
  EXPECT_FALSE (send->error);
  EXPECT_EQ (mock_.control.sent ().size (), 1u);
}

TEST_F (LumexMockHangUpDatagramTest,
        GivenAHeldSend_WhenADisconnectAfterComes_ThenTheSendEndsDisconnected)
{
  open ();
  mock_.control.hold_sends ();
  std::shared_ptr<status_state_t> const send
      = start_send (message_of (1, "held"));
  EXPECT_FALSE (send->done);
  mock_.control.disconnect_after (ms (10));
  drain (context_);
  clock_.advance (ms (9));
  drain (context_);
  EXPECT_FALSE (send->done);
  clock_.advance (ms (1));
  drain (context_);
  ASSERT_TRUE (send->done);
  EXPECT_EQ (send->error, code (transport_errc::disconnected));
}

TEST_F (
    LumexMockHangUpDatagramTest,
    GivenADisconnectAfter_WhenTheLinkIsClosedAndOpenedAgain_ThenTheNewLinkIsKept)
{
  open ();
  mock_.control.disconnect_after (ms (10));
  reopen ();
  clock_.advance (ms (20));
  drain (context_);
  std::shared_ptr<receive_state_t> const receive = start_receive ();
  mock_.control.deliver (message_of (3, "alive"));
  drain (context_);
  ASSERT_TRUE (receive->done);
  EXPECT_FALSE (receive->error);
  EXPECT_FALSE (start_send (message_of (1, "ok"))->error);
}

TEST_F (
    LumexMockHangUpDatagramTest,
    GivenDeliveriesAfter_WhenTheLinkIsOpenedAgain_ThenOnlyTheLinkedOneIsDropped)
{
  open ();
  mock_.control.deliver_after (ms (10), message_of (1, "plain"));
  mock_.control.deliver_after_on_link (ms (10), message_of (1, "linked"));
  reopen ();
  clock_.advance (ms (10));
  drain (context_);
  std::shared_ptr<receive_state_t> const receive = start_receive ();
  drain (context_);
  ASSERT_TRUE (receive->done);
  EXPECT_EQ (text_of (receive->message), "plain");
  std::shared_ptr<receive_state_t> const none = start_receive ();
  drain (context_);
  EXPECT_FALSE (none->done);
}

TEST_F (LumexMockHangUpDatagramTest,
        GivenADeliveryAfterOnLink_WhenTheLinkStaysUp_ThenItArrivesAtItsTime)
{
  open ();
  mock_.control.deliver_after_on_link (ms (10), message_of (4, "linked"));
  std::shared_ptr<receive_state_t> const receive = start_receive ();
  clock_.advance (ms (9));
  drain (context_);
  EXPECT_FALSE (receive->done);
  clock_.advance (ms (1));
  drain (context_);
  ASSERT_TRUE (receive->done);
  EXPECT_EQ (receive->message.report_id, 4);
}

TEST_F (
    LumexMockHangUpDatagramTest,
    GivenADeliveryAfterOnLink_WhenTheLinkIsClosedBeforeItComes_ThenNothingWaitsForTheNextOpen)
{
  open ();
  mock_.control.deliver_after_on_link (ms (10), message_of (1, "linked"));
  mock_.transport.close ();
  drain (context_);
  clock_.advance (ms (20));
  drain (context_);
  open ();
  std::shared_ptr<receive_state_t> const receive = start_receive ();
  drain (context_);
  EXPECT_FALSE (receive->done);
}

TEST_F (
    LumexMockHangUpDatagramTest,
    GivenAStepScheduledBeforeTheFirstOpen_WhenTheLinkOpens_ThenTheOpenStartsANewConnectionAndItIsDropped)
{
  mock_.control.disconnect_after (ms (10));
  mock_.control.deliver_after_on_link (ms (10), message_of (1, "linked"));
  open ();
  clock_.advance (ms (20));
  drain (context_);
  std::shared_ptr<receive_state_t> const receive = start_receive ();
  drain (context_);
  EXPECT_FALSE (receive->done);
  EXPECT_FALSE (start_send (message_of (1, "ok"))->error);
}

TEST_F (LumexMockHangUpDatagramTest,
        GivenASourceThatReadsTheMockInNow_WhenOpened_ThenNothingDeadlocks)
{
  struct source_t : tr::timing::timer_source_impl
  {
    tr::testing::mock_datagram_control *control = nullptr;

    tr::timing::time_point_t
    now () const override
    {
      if (control != nullptr)
        control->deliver (message_t ());
      return tr::timing::time_point_t ();
    }

    std::shared_ptr<tr::timing::timer_impl>
    make_timer (tr::executor_type const &) const override
    {
      return std::shared_ptr<tr::timing::timer_impl> ();
    }
  };
  std::shared_ptr<source_t> const source = std::make_shared<source_t> ();
  mock_options_t options;
  options.timers = tr::timing::timer_source (source);
  datagram_mock_t made
      = tr::testing::make_mock_datagram (context_.get_executor (), options);
  tr::testing::mock_datagram_control control = made.control;
  source->control = &control;
  bool done = false;
  made.transport.async_open ([&done] (std::error_code const &)
                               { done = true; });
  drain (context_);
  EXPECT_TRUE (done);
  source->control = nullptr;
}
