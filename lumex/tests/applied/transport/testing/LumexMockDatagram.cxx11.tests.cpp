// The scripted mock datagram transport, the twin of the scripted mock stream:
// the send observer (order, report IDs, strand, replying from inside), timed
// delivery on the clock of the mock's timer source, a held open, a send that
// fails after some succeeded, several pending receives, and the shutdown of a
// context that still holds a mock's observer and receives.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
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
using tr::datagram::message_t;
using tr::error::transport_errc;
using tr::testing::make_mock_datagram;
using tr::testing::mock_datagram;
using tr::testing::mock_options_t;
using tr::testing::virtual_clock;

ms const KREAL_TIME_GUARD = ms (5000);

message_t
message_of (std::uint8_t report_id, std::string const &text)
{
  message_t message;
  message.report_id = report_id;
  message.payload.assign (text.begin (), text.end ());
  return message;
}

std::string
text_of (message_t const &message)
{
  return std::string (message.payload.begin (), message.payload.end ());
}

std::error_code
code (transport_errc value)
{
  return make_error_code (value);
}

// An observer that owns a copy of the control (a cycle with the mock until
// the observer is dropped) and the sentinel.
tr::testing::send_observer_t
cyclic_observer (tr::testing::mock_datagram_control const &control,
                 std::shared_ptr<int> const &sentinel)
{
  return [control, sentinel] (message_t const &) {};
}

// A receive handler that owns a copy of the control (a cycle with the mock
// until the handler is dropped) and the sentinel.
struct cyclic_receive_handler_t
{
  tr::testing::mock_datagram_control control;
  std::shared_ptr<int> sentinel;

  void
  operator() (std::error_code const &, message_t) const
  {
  }
};

// A send handler that owns a copy of the control (a cycle with the mock until
// the handler is dropped) and the sentinel.
struct cyclic_send_handler_t
{
  tr::testing::mock_datagram_control control;
  std::shared_ptr<int> sentinel;

  void
  operator() (std::error_code const &) const
  {
  }
};

// Starts a send (held by the mock) whose handler owns the control.
void
start_cyclic_send (tr::datagram::message_transport &transport,
                   tr::testing::mock_datagram_control const &control,
                   std::shared_ptr<int> const &sentinel)
{
  cyclic_send_handler_t const handler = { control, sentinel };
  transport.async_send (message_t (), handler);
}

// Starts two receives whose handlers own the control, so that no copy stays
// with the caller.
void
start_cyclic_receives (tr::datagram::message_transport &transport,
                       tr::testing::mock_datagram_control const &control,
                       std::shared_ptr<int> const &sentinel)
{
  cyclic_receive_handler_t const handler = { control, sentinel };
  transport.async_receive (handler);
  transport.async_receive (handler);
}

// What the handler of one receive saw.
struct receive_state_t
{
  bool done = false;
  std::error_code error;
  message_t message;
};

// What the handler of one send saw.
struct send_state_t
{
  bool done = false;
  std::error_code error;
};

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

class LumexMockDatagramTest : public ::testing::Test
{
protected:
  asio::io_context context_;
  virtual_clock clock_;
  mock_datagram mock_
      = make_mock_datagram (context_.get_executor (), scripted ());

  mock_options_t
  scripted () const
  {
    mock_options_t options;
    options.name = "scripted";
    options.timers = clock_.source ();
    return options;
  }

  std::shared_ptr<send_state_t>
  start_open ()
  {
    std::shared_ptr<send_state_t> const state
        = std::make_shared<send_state_t> ();
    mock_.transport.async_open (
        [state] (std::error_code const &error)
          {
            state->done = true;
            state->error = error;
          });
    return state;
  }

  void
  open ()
  {
    std::shared_ptr<send_state_t> const state = start_open ();
    drain (context_);
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

  std::shared_ptr<send_state_t>
  start_send (message_t message)
  {
    std::shared_ptr<send_state_t> const state
        = std::make_shared<send_state_t> ();
    mock_.transport.async_send (std::move (message),
                                [state] (std::error_code const &error)
                                  {
                                    state->done = true;
                                    state->error = error;
                                  });
    return state;
  }
};
} // namespace

// --- the send observer -----------------------------------------------------

TEST_F (
    LumexMockDatagramTest,
    GivenAnObserver_WhenMessagesAreSent_ThenItSeesEachInOrderWithItsReportIdOnTheStrandNeverInline)
{
  open ();
  std::vector<message_t> seen;
  std::vector<bool> on_strand;
  asio::strand<tr::executor_type> const strand = mock_.transport.get_strand ();
  mock_.control.on_send (
      [&seen, &on_strand, strand] (message_t const &message)
        {
          seen.push_back (message);
          on_strand.push_back (strand.running_in_this_thread ());
        });
  std::shared_ptr<send_state_t> const first
      = start_send (message_of (3, "one"));
  std::shared_ptr<send_state_t> const second
      = start_send (message_of (0, "two"));
  EXPECT_TRUE (seen.empty ());
  drain (context_);
  ASSERT_TRUE (first->done && second->done);
  ASSERT_EQ (seen.size (), 2u);
  EXPECT_EQ (seen[0].report_id, 3);
  EXPECT_EQ (text_of (seen[0]), "one");
  EXPECT_EQ (seen[1].report_id, 0);
  EXPECT_EQ (text_of (seen[1]), "two");
  EXPECT_TRUE (on_strand[0] && on_strand[1]);
  EXPECT_EQ (mock_.control.sent ().size (), 2u);
}

TEST_F (LumexMockDatagramTest,
        GivenAnObserver_WhenItIsReplacedByAnEmptyOne_ThenItSeesNoMoreSends)
{
  open ();
  int calls = 0;
  mock_.control.on_send ([&calls] (message_t const &) { ++calls; });
  start_send (message_of (0, "a"));
  drain (context_);
  EXPECT_EQ (calls, 1);
  mock_.control.on_send (tr::testing::send_observer_t ());
  start_send (message_of (0, "b"));
  drain (context_);
  EXPECT_EQ (calls, 1);
  EXPECT_EQ (mock_.control.sent ().size (), 2u);
}

TEST_F (LumexMockDatagramTest,
        GivenASendThatFails_WhenObserved_ThenTheObserverSeesOnlyAcceptedSends)
{
  open ();
  int calls = 0;
  mock_.control.on_send ([&calls] (message_t const &) { ++calls; });
  mock_.control.fail_next_send (std::make_error_code (std::errc::io_error));
  std::shared_ptr<send_state_t> const failed
      = start_send (message_of (0, "x"));
  drain (context_);
  ASSERT_TRUE (failed->done);
  EXPECT_TRUE (failed->error);
  EXPECT_EQ (calls, 0);
}

TEST_F (
    LumexMockDatagramTest,
    GivenAnObserverThatDeliversAReply_WhenARequestIsSent_ThenThePendingReceiveGetsItWithoutDeadlockOrInlineCompletion)
{
  open ();
  tr::testing::mock_datagram_control control = mock_.control;
  std::shared_ptr<receive_state_t> const receive = start_receive ();
  drain (context_);
  ASSERT_FALSE (receive->done);
  int done_inside = 0;
  control.on_send (
      [control, receive, &done_inside] (message_t const &request) mutable
        {
          control.deliver (
              message_of (request.report_id, "re:" + text_of (request)));
          done_inside += receive->done ? 1 : 0;
        });
  std::shared_ptr<send_state_t> const send
      = start_send (message_of (7, "ping"));
  drain (context_);
  ASSERT_TRUE (send->done);
  ASSERT_TRUE (receive->done);
  EXPECT_FALSE (receive->error);
  EXPECT_EQ (receive->message.report_id, 7);
  EXPECT_EQ (text_of (receive->message), "re:ping");
  EXPECT_EQ (done_inside, 0);
  control.on_send (tr::testing::send_observer_t ()); // breaks the cycle
}

// --- timed delivery --------------------------------------------------------

TEST_F (LumexMockDatagramTest,
        GivenDeliverAfter_WhenTheClockAdvances_ThenTheMessageArrivesAtItsTime)
{
  open ();
  std::shared_ptr<receive_state_t> const receive = start_receive ();
  mock_.control.deliver_after (ms (100), message_of (2, "late"));
  drain (context_);
  EXPECT_FALSE (receive->done);
  clock_.advance (ms (99));
  drain (context_);
  EXPECT_FALSE (receive->done);
  clock_.advance (ms (1));
  drain (context_);
  ASSERT_TRUE (receive->done);
  EXPECT_FALSE (receive->error);
  EXPECT_EQ (receive->message.report_id, 2);
  EXPECT_EQ (text_of (receive->message), "late");
}

TEST_F (
    LumexMockDatagramTest,
    GivenSeveralDeliverAfter_WhenTheClockPassesThem_ThenTheyArriveInTimeThenScheduleOrder)
{
  open ();
  mock_.control.deliver_after (ms (20), message_of (0, "B"));
  mock_.control.deliver_after (ms (10), message_of (0, "A"));
  mock_.control.deliver_after (ms (10), message_of (0, "C"));
  clock_.advance (ms (20));
  drain (context_);
  std::string order;
  for (int i = 0; i < 3; ++i)
    {
      std::shared_ptr<receive_state_t> const receive = start_receive ();
      drain (context_);
      ASSERT_TRUE (receive->done);
      order += text_of (receive->message);
    }
  EXPECT_EQ (order, "ACB");
}

TEST_F (LumexMockDatagramTest,
        GivenAPendingDeliverAfter_WhenTheMockIsDestroyed_ThenTheScriptEnds)
{
  std::unique_ptr<mock_datagram> made (new mock_datagram (
      make_mock_datagram (context_.get_executor (), scripted ())));
  made->control.deliver_after (ms (50), message_of (0, "never"));
  EXPECT_EQ (clock_.pending (), 1u);
  made.reset ();
  drain (context_);
  EXPECT_EQ (clock_.pending (), 0u);
  clock_.advance (ms (100));
  drain (context_);
}

TEST (LumexMockDatagram,
      GivenAPendingDeliverAfter_WhenTheClockIsDestroyed_ThenNothingIsDelivered)
{
  asio::io_context context;
  std::unique_ptr<virtual_clock> clock (new virtual_clock);
  mock_options_t options;
  options.timers = clock->source ();
  mock_datagram made = make_mock_datagram (context.get_executor (), options);
  bool opened = false;
  made.transport.async_open ([&opened] (std::error_code const &)
                               { opened = true; });
  drain (context);
  ASSERT_TRUE (opened);
  std::shared_ptr<receive_state_t> const receive
      = std::make_shared<receive_state_t> ();
  made.transport.async_receive (
      [receive] (std::error_code const &error, message_t message)
        {
          receive->done = true;
          receive->error = error;
          receive->message = std::move (message);
        });
  made.control.deliver_after (ms (10), message_of (0, "never"));
  clock.reset (); // ends the pending wait with cancelled
  drain (context);
  EXPECT_FALSE (receive->done);
  made.transport.close ();
  drain (context);
  ASSERT_TRUE (receive->done);
  EXPECT_EQ (receive->error, code (transport_errc::cancelled));
}

TEST (LumexMockDatagram,
      GivenTheDefaultOptions_WhenDeliverAfterIsUsed_ThenItWaitsForRealTime)
{
  asio::io_context context;
  mock_datagram made = make_mock_datagram (context.get_executor (), "steady");
  EXPECT_EQ (made.transport.name (), "steady");
  bool opened = false;
  made.transport.async_open ([&opened] (std::error_code const &)
                               { opened = true; });
  drain (context);
  ASSERT_TRUE (opened);
  std::shared_ptr<receive_state_t> const receive
      = std::make_shared<receive_state_t> ();
  made.transport.async_receive (
      [receive] (std::error_code const &error, message_t message)
        {
          receive->done = true;
          receive->error = error;
          receive->message = std::move (message);
        });
  std::chrono::steady_clock::time_point const start
      = std::chrono::steady_clock::now ();
  made.control.deliver_after (ms (30), message_of (0, "real"));
  while (!receive->done
         && std::chrono::steady_clock::now () - start < KREAL_TIME_GUARD)
    {
      if (context.stopped ())
        context.restart ();
      context.run_for (ms (10));
    }
  ASSERT_TRUE (receive->done);
  EXPECT_EQ (text_of (receive->message), "real");
  EXPECT_GE (std::chrono::steady_clock::now () - start, ms (25));
}

// --- a held open and a late send failure ------------------------------------

TEST_F (LumexMockDatagramTest,
        GivenAHeldOpen_WhenTheTransportIsClosed_ThenTheOpenEndsCancelled)
{
  mock_.control.hold_next_open ();
  std::shared_ptr<send_state_t> const opening = start_open ();
  drain (context_);
  EXPECT_FALSE (opening->done);
  EXPECT_FALSE (mock_.transport.is_open ());
  mock_.transport.close ();
  drain (context_);
  ASSERT_TRUE (opening->done);
  EXPECT_EQ (opening->error, code (transport_errc::cancelled));
  EXPECT_FALSE (mock_.transport.is_open ());
  open (); // the hold was one-shot
  EXPECT_TRUE (mock_.transport.is_open ());
}

TEST_F (LumexMockDatagramTest,
        GivenAHeldOpen_WhenTheTransportIsCancelled_ThenTheOpenEndsCancelled)
{
  mock_.control.hold_next_open ();
  std::shared_ptr<send_state_t> const opening = start_open ();
  drain (context_);
  mock_.transport.cancel ();
  drain (context_);
  ASSERT_TRUE (opening->done);
  EXPECT_EQ (opening->error, code (transport_errc::cancelled));
}

TEST_F (LumexMockDatagramTest,
        GivenFailSendAfter_WhenSendsRun_ThenTheNextAfterTheCountFails)
{
  open ();
  std::error_code const failure = std::make_error_code (std::errc::io_error);
  mock_.control.fail_send_after (2, failure);
  std::shared_ptr<send_state_t> const a = start_send (message_of (0, "a"));
  std::shared_ptr<send_state_t> const b = start_send (message_of (0, "b"));
  std::shared_ptr<send_state_t> const c = start_send (message_of (0, "c"));
  std::shared_ptr<send_state_t> const d = start_send (message_of (0, "d"));
  drain (context_);
  ASSERT_TRUE (a->done && b->done && c->done && d->done);
  EXPECT_FALSE (a->error);
  EXPECT_FALSE (b->error);
  EXPECT_EQ (c->error, failure);
  EXPECT_FALSE (d->error); // the failure is one-shot
  EXPECT_EQ (mock_.control.sent ().size (), 3u);
}

TEST_F (LumexMockDatagramTest,
        GivenFailSendAfterOfZero_WhenASendRuns_ThenItFailsAtOnce)
{
  open ();
  std::error_code const failure = std::make_error_code (std::errc::io_error);
  mock_.control.fail_send_after (0, failure);
  std::shared_ptr<send_state_t> const a = start_send (message_of (0, "a"));
  drain (context_);
  ASSERT_TRUE (a->done);
  EXPECT_EQ (a->error, failure);
  EXPECT_TRUE (mock_.control.sent ().empty ());
}

// --- several pending receives -----------------------------------------------

TEST_F (
    LumexMockDatagramTest,
    GivenTwoPendingReceives_WhenMessagesArrive_ThenBothAreServedInTheOrderStarted)
{
  open ();
  std::shared_ptr<receive_state_t> const first = start_receive ();
  std::shared_ptr<receive_state_t> const second = start_receive ();
  drain (context_);
  EXPECT_FALSE (first->done || second->done);
  mock_.control.deliver (message_of (1, "x"));
  drain (context_);
  ASSERT_TRUE (first->done);
  EXPECT_EQ (text_of (first->message), "x");
  EXPECT_FALSE (second->done);
  mock_.control.deliver (message_of (2, "y"));
  drain (context_);
  ASSERT_TRUE (second->done);
  EXPECT_EQ (text_of (second->message), "y");
  EXPECT_EQ (second->message.report_id, 2);
}

TEST_F (LumexMockDatagramTest,
        GivenTwoPendingReceives_WhenCancelled_ThenBothEndCancelled)
{
  open ();
  std::shared_ptr<receive_state_t> const first = start_receive ();
  std::shared_ptr<receive_state_t> const second = start_receive ();
  drain (context_);
  mock_.transport.cancel ();
  drain (context_);
  ASSERT_TRUE (first->done && second->done);
  EXPECT_EQ (first->error, code (transport_errc::cancelled));
  EXPECT_EQ (second->error, code (transport_errc::cancelled));
}

TEST_F (LumexMockDatagramTest,
        GivenTwoPendingReceives_WhenTheMockDisconnects_ThenBothEndDisconnected)
{
  open ();
  std::shared_ptr<receive_state_t> const first = start_receive ();
  std::shared_ptr<receive_state_t> const second = start_receive ();
  drain (context_);
  mock_.control.disconnect ();
  drain (context_);
  ASSERT_TRUE (first->done && second->done);
  EXPECT_EQ (first->error, code (transport_errc::disconnected));
  EXPECT_EQ (second->error, code (transport_errc::disconnected));
}

TEST_F (
    LumexMockDatagramTest,
    GivenTwoPendingReceives_WhenTheNextReceiveFails_ThenTheOldestEndsWithTheError)
{
  open ();
  std::shared_ptr<receive_state_t> const first = start_receive ();
  std::shared_ptr<receive_state_t> const second = start_receive ();
  drain (context_);
  mock_.control.fail_next_receive (std::make_error_code (std::errc::io_error));
  drain (context_);
  ASSERT_TRUE (first->done);
  EXPECT_EQ (first->error, std::make_error_code (std::errc::io_error));
  EXPECT_FALSE (second->done);
  mock_.control.deliver (message_of (0, "k"));
  drain (context_);
  ASSERT_TRUE (second->done);
  EXPECT_EQ (text_of (second->message), "k");
}

// --- shutdown ---------------------------------------------------------------

TEST (
    LumexMockDatagram,
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
  std::unique_ptr<mock_datagram> made (new mock_datagram (
      make_mock_datagram (context->get_executor (), options)));
  std::unique_ptr<tr::datagram::message_transport> transport (
      new tr::datagram::message_transport (std::move (made->transport)));
  tr::testing::mock_datagram_control &control = made->control;
  bool opened = false;
  transport->async_open ([&opened] (std::error_code const &)
                           { opened = true; });
  drain (*context);
  ASSERT_TRUE (opened);
  control.on_send (cyclic_observer (control, sentinel));
  transport->async_receive (
      [sentinel] (std::error_code const &, message_t) {});
  transport->async_receive (
      [sentinel] (std::error_code const &, message_t) {});
  control.deliver_after (ms (10), message_of (0, "later"));
  drain (*context);
  EXPECT_GT (sentinel.use_count (), 1);
  transport.reset ();
  made.reset ();
  context.reset ();
  EXPECT_EQ (sentinel.use_count (), 1);
}

// --- another thread ---------------------------------------------------------

TEST (
    LumexMockDatagram,
    GivenAControlUsedFromAnotherThread_WhenTheRunnerServesReceives_ThenEveryScriptedMessageArrives)
{
  tr::stream::io_runner runner (1);
  mock_datagram made = make_mock_datagram (runner.get_executor (), "threads");
  ASSERT_TRUE (made.transport.open (ms (2000)).has_value ());
  std::atomic<int> observed (0);
  made.control.on_send ([&observed] (message_t const &) { ++observed; });
  tr::testing::mock_datagram_control control = made.control;
  std::thread script (
      [control] () mutable
        {
          for (int i = 0; i < 100; ++i)
            control.deliver_after (
                ms (i % 3), message_of (static_cast<std::uint8_t> (i), "m"));
        });
  for (int i = 0; i < 20; ++i)
    ASSERT_TRUE (
        made.transport.send (message_of (1, "ping"), ms (2000)).has_value ());
  int total = 0;
  while (total < 100)
    {
      tr::datagram::message_result_t const result
          = made.transport.receive (ms (3000));
      ASSERT_TRUE (result.has_value ());
      ++total;
    }
  script.join ();
  EXPECT_EQ (observed.load (), 20);
  made.control.on_send (tr::testing::send_observer_t ());
  made.transport.close ();
}

TEST (
    LumexMockDatagram,
    GivenAnObserverThatOwnsTheControl_WhenOnlyTheContextEnds_ThenTheShutdownDropsIt)
{
  std::shared_ptr<int> const sentinel = std::make_shared<int> (0);
  std::unique_ptr<asio::io_context> context (new asio::io_context);
  std::unique_ptr<mock_datagram> made (new mock_datagram (
      make_mock_datagram (context->get_executor (), "cycle")));
  made->control.on_send (cyclic_observer (made->control, sentinel));
  EXPECT_GT (sentinel.use_count (), 1);
  made.reset ();
  context.reset ();
  EXPECT_EQ (sentinel.use_count (), 1);
}

TEST (
    LumexMockDatagram,
    GivenPendingReceivesOwningTheControl_WhenTheContextIsDestroyed_ThenTheShutdownDropsThem)
{
  std::shared_ptr<int> const sentinel = std::make_shared<int> (0);
  std::unique_ptr<asio::io_context> context (new asio::io_context);
  std::unique_ptr<mock_datagram> made (new mock_datagram (
      make_mock_datagram (context->get_executor (), "cycle")));
  std::unique_ptr<tr::datagram::message_transport> transport (
      new tr::datagram::message_transport (std::move (made->transport)));
  bool opened = false;
  transport->async_open ([&opened] (std::error_code const &)
                           { opened = true; });
  drain (*context);
  ASSERT_TRUE (opened);
  start_cyclic_receives (*transport, made->control, sentinel);
  drain (*context);
  EXPECT_GT (sentinel.use_count (), 2);
  transport.reset ();
  made.reset ();
  context.reset (); // the handlers of the two receives were the last owners
  EXPECT_EQ (sentinel.use_count (), 1);
}

// --- review round: a stateful observer, a throwing one, held sends ----------

TEST_F (LumexMockDatagramTest,
        GivenAStatefulObserver_WhenSeveralSendsRun_ThenItKeepsItsState)
{
  open ();
  std::vector<int> seen;
  std::vector<int> *out = &seen;
  int count = 0;
  mock_.control.on_send ([count, out] (message_t const &) mutable
                           { out->push_back (++count); });
  start_send (message_of (0, "a"));
  drain (context_);
  start_send (message_of (0, "b"));
  drain (context_);
  start_send (message_of (0, "c"));
  drain (context_);
  ASSERT_EQ (seen.size (), 3u);
  EXPECT_EQ (seen[0], 1);
  EXPECT_EQ (seen[1], 2);
  EXPECT_EQ (seen[2], 3);
}

TEST_F (LumexMockDatagramTest,
        GivenALateFailedSend_WhenObserved_ThenTheObserverDoesNotSeeIt)
{
  open ();
  std::vector<std::string> seen;
  mock_.control.on_send ([&seen] (message_t const &message)
                           { seen.push_back (text_of (message)); });
  mock_.control.fail_send_after (1,
                                 std::make_error_code (std::errc::io_error));
  std::shared_ptr<send_state_t> const a = start_send (message_of (0, "a"));
  std::shared_ptr<send_state_t> const b = start_send (message_of (0, "b"));
  drain (context_);
  ASSERT_TRUE (a->done && b->done);
  EXPECT_FALSE (a->error);
  EXPECT_TRUE (b->error);
  ASSERT_EQ (seen.size (), 1u);
  EXPECT_EQ (seen[0], "a");
}

TEST_F (LumexMockDatagramTest,
        GivenHeldSends_WhenTheTransportIsClosed_ThenTheyEndCancelled)
{
  open ();
  int observed = 0;
  mock_.control.on_send ([&observed] (message_t const &) { ++observed; });
  mock_.control.hold_sends ();
  std::shared_ptr<send_state_t> const a = start_send (message_of (0, "a"));
  std::shared_ptr<send_state_t> const b = start_send (message_of (0, "b"));
  drain (context_);
  EXPECT_FALSE (a->done || b->done);
  EXPECT_TRUE (mock_.control.sent ().empty ());
  mock_.transport.close ();
  drain (context_);
  ASSERT_TRUE (a->done && b->done);
  EXPECT_EQ (a->error, code (transport_errc::cancelled));
  EXPECT_EQ (b->error, code (transport_errc::cancelled));
  EXPECT_TRUE (mock_.control.sent ().empty ());
  EXPECT_EQ (observed, 0);
}

TEST_F (LumexMockDatagramTest,
        GivenHeldSends_WhenTheTransportIsCancelled_ThenTheyEndCancelled)
{
  open ();
  mock_.control.hold_sends ();
  std::shared_ptr<send_state_t> const a = start_send (message_of (0, "a"));
  drain (context_);
  mock_.transport.cancel ();
  drain (context_);
  ASSERT_TRUE (a->done);
  EXPECT_EQ (a->error, code (transport_errc::cancelled));
  EXPECT_TRUE (mock_.transport.is_open ());
}

TEST_F (
    LumexMockDatagramTest,
    GivenHeldSends_WhenReleased_ThenTheyGoInOrderAndALaterSendQueuesBehindThem)
{
  open ();
  std::vector<std::string> seen;
  mock_.control.on_send ([&seen] (message_t const &message)
                           { seen.push_back (text_of (message)); });
  mock_.control.hold_sends ();
  std::shared_ptr<send_state_t> const a = start_send (message_of (0, "A"));
  drain (context_);
  std::shared_ptr<send_state_t> const b = start_send (message_of (0, "B"));
  mock_.control.release_sends ();
  std::shared_ptr<send_state_t> const c = start_send (message_of (0, "C"));
  drain (context_);
  ASSERT_TRUE (a->done && b->done && c->done);
  ASSERT_EQ (seen.size (), 3u);
  EXPECT_EQ (seen[0], "A");
  EXPECT_EQ (seen[1], "B");
  EXPECT_EQ (seen[2], "C");
  EXPECT_EQ (mock_.control.sent ().size (), 3u);
}

TEST_F (LumexMockDatagramTest,
        GivenAHoldWithNothingHeld_WhenReleased_ThenLaterSendsGoAtOnce)
{
  open ();
  mock_.control.hold_sends ();
  mock_.control.release_sends ();
  std::shared_ptr<send_state_t> const a = start_send (message_of (0, "go"));
  drain (context_);
  EXPECT_TRUE (a->done);
  EXPECT_EQ (mock_.control.sent ().size (), 1u);
}

TEST_F (LumexMockDatagramTest,
        GivenHeldSends_WhenReceivesRun_ThenReceivesAreNotHeld)
{
  open ();
  mock_.control.hold_sends ();
  std::shared_ptr<send_state_t> const send = start_send (message_of (0, "x"));
  mock_.control.deliver (message_of (1, "in"));
  std::shared_ptr<receive_state_t> const receive = start_receive ();
  drain (context_);
  EXPECT_TRUE (receive->done);
  EXPECT_FALSE (send->done);
  mock_.control.release_sends ();
  drain (context_);
  EXPECT_TRUE (send->done);
}

TEST_F (
    LumexMockDatagramTest,
    GivenAHoldSetAgainByTheObserver_WhenTheReleaseRuns_ThenTheRestStaysHeld)
{
  open ();
  tr::testing::mock_datagram_control control = mock_.control;
  control.on_send (
      [control] (message_t const &message) mutable
        {
          if (text_of (message) == "A")
            control.hold_sends ();
        });
  control.hold_sends ();
  std::shared_ptr<send_state_t> const a = start_send (message_of (0, "A"));
  std::shared_ptr<send_state_t> const b = start_send (message_of (0, "B"));
  drain (context_);
  control.release_sends ();
  drain (context_);
  EXPECT_TRUE (a->done);
  EXPECT_FALSE (b->done);
  control.on_send (tr::testing::send_observer_t ());
  control.release_sends ();
  drain (context_);
  EXPECT_TRUE (b->done);
}

TEST_F (LumexMockDatagramTest,
        GivenAFailedHeldSend_WhenReleased_ThenTheOthersStillGo)
{
  open ();
  mock_.control.hold_sends ();
  mock_.control.fail_next_send (std::make_error_code (std::errc::io_error));
  std::shared_ptr<send_state_t> const a = start_send (message_of (0, "A"));
  std::shared_ptr<send_state_t> const b = start_send (message_of (0, "B"));
  drain (context_);
  mock_.control.release_sends ();
  drain (context_);
  ASSERT_TRUE (a->done && b->done);
  EXPECT_TRUE (a->error);
  EXPECT_FALSE (b->error);
  EXPECT_EQ (mock_.control.sent ().size (), 1u);
}

TEST_F (
    LumexMockDatagramTest,
    GivenAnObserverThatThrowsDuringTheRelease_WhenTheRestIsHeld_ThenANewStepFlushesThem)
{
  open ();
  int calls = 0;
  mock_.control.on_send (
      [&calls] (message_t const &)
        {
          if (++calls == 1)
            throw std::runtime_error ("observer");
        });
  mock_.control.hold_sends ();
  std::shared_ptr<send_state_t> const a = start_send (message_of (0, "a"));
  std::shared_ptr<send_state_t> const b = start_send (message_of (0, "b"));
  drain (context_);
  mock_.control.release_sends ();
  bool thrown = false;
  try
    {
      drain (context_);
    }
  catch (std::runtime_error const &)
    {
      thrown = true;
    }
  drain (context_);
  EXPECT_TRUE (thrown);
  EXPECT_TRUE (a->done);
  EXPECT_TRUE (b->done);
  EXPECT_EQ (mock_.control.sent ().size (), 2u);
}

TEST_F (
    LumexMockDatagramTest,
    GivenScriptedSettings_WhenTheTransportIsClosedAndOpenedAgain_ThenTheyStillApply)
{
  open ();
  mock_.control.hold_sends ();
  mock_.transport.close ();
  drain (context_);
  open ();
  std::shared_ptr<send_state_t> const a = start_send (message_of (0, "a"));
  drain (context_);
  EXPECT_FALSE (a->done);
}

TEST (
    LumexMockDatagram,
    GivenAHeldSendOwningTheControl_WhenTheContextIsDestroyed_ThenTheShutdownDropsIt)
{
  std::shared_ptr<int> const sentinel = std::make_shared<int> (0);
  std::unique_ptr<asio::io_context> context (new asio::io_context);
  std::unique_ptr<mock_datagram> made (new mock_datagram (
      make_mock_datagram (context->get_executor (), "cycle")));
  std::unique_ptr<tr::datagram::message_transport> transport (
      new tr::datagram::message_transport (std::move (made->transport)));
  bool opened = false;
  transport->async_open ([&opened] (std::error_code const &)
                           { opened = true; });
  drain (*context);
  ASSERT_TRUE (opened);
  made->control.hold_sends ();
  start_cyclic_send (*transport, made->control, sentinel);
  drain (*context);
  EXPECT_GT (sentinel.use_count (), 1);
  transport.reset ();
  made.reset ();
  context.reset (); // the handler of the held send was the last owner
  EXPECT_EQ (sentinel.use_count (), 1);
}

TEST_F (LumexMockDatagramTest,
        GivenHeldSends_WhenTheMockDisconnects_ThenTheyEndDisconnected)
{
  open ();
  mock_.control.hold_sends ();
  std::shared_ptr<send_state_t> const a = start_send (message_of (0, "a"));
  std::shared_ptr<send_state_t> const b = start_send (message_of (0, "b"));
  drain (context_);
  mock_.control.disconnect ();
  drain (context_);
  ASSERT_TRUE (a->done && b->done);
  EXPECT_EQ (a->error, code (transport_errc::disconnected));
  EXPECT_EQ (b->error, code (transport_errc::disconnected));
  EXPECT_TRUE (mock_.control.sent ().empty ());
}
