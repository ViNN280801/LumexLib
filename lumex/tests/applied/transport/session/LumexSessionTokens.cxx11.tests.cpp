// Completion token styles through the sessions (REVIEW13 TK1 to TK10):
// use_future, asio::deferred, as_tuple, yield_context (with
// LUMEX_TRANSPORT_WITH_SPAWN), awaitables (C++20), the per-operation
// cancellation slot, and the work kept on a handler's executor: none (the
// session and the transports keep no work there; a user keeps an
// executor_work_guard on a context whose run () must not return before the
// handler).

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <system_error>
#include <thread>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "LumexSessionTestSupport.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"

namespace
{
using namespace lumex_session_test;
using tr::error::transport_errc;
using state = ss::channel_state;

std::vector<std::uint8_t>
three ()
{
  return std::vector<std::uint8_t> (3, std::uint8_t (0x2A));
}

// A completion token that records the executor of the initiation and then
// starts it with a handler that does nothing.
struct capture_executor_t
{
  std::shared_ptr<tr::executor_type> out;
};

struct ignore_all
{
  template <typename... Args>
  void
  operator() (Args &&...) const
  {
  }
};

class LumexSessionTokensTest : public ::testing::Test
{
protected:
  tr::stream::io_runner runner_{ 2 };
  // After the runner: the mock streams it keeps die before their context.
  std::shared_ptr<stream_factory_state> factory_
      = std::make_shared<stream_factory_state> ();
  std::shared_ptr<datagram_factory_state> datagrams_
      = std::make_shared<datagram_factory_state> ();
};
} // namespace

namespace asio
{
template <typename... Signatures>
struct async_result<capture_executor_t, Signatures...>
{
  template <typename Initiation, typename... Args>
  static void
  initiate (Initiation &&initiation, capture_executor_t token, Args &&...args)
  {
    *token.out = initiation.get_executor ();
    std::move (initiation) (ignore_all (), std::forward<Args> (args)...);
  }
};
} // namespace asio

namespace
{
// The strand the initiations of `session` report (observed through a token;
// the wait it starts is never completed by the test).
tr::executor_type
session_strand (ss::stream_session &session)
{
  capture_executor_t token = { std::make_shared<tr::executor_type> () };
  session.async_wait_state_change (session.state (), token);
  return *token.out;
}
} // namespace

// TK1
TEST_F (LumexSessionTokensTest,
        GivenUseFuture_WhenOpenAndStateWait_ThenTheyCompleteAndCancelThrows)
{
  ss::stream_session session (runner_.get_executor (),
                              make_stream_factory (factory_));
  std::future<void> opened = session.async_open (asio::use_future);
  ASSERT_EQ (opened.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  EXPECT_NO_THROW (opened.get ());
  std::future<state> changed
      = session.async_wait_state_change (state::open, asio::use_future);
  session.close ();
  ASSERT_EQ (changed.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  try
    {
      changed.get ();
      ADD_FAILURE () << "no exception";
    }
  catch (std::system_error const &failure)
    {
      EXPECT_TRUE (failure.code () == transport_errc::cancelled);
    }
}

// TK2: asio::deferred (the return types of the async_* members name the
// arguments as the bodies pass them, lvalues).
TEST_F (LumexSessionTokensTest,
        GivenDeferred_WhenLaunchedLater_ThenReadAndStateWaitComplete)
{
  ss::stream_session session (runner_.get_executor (),
                              make_stream_factory (factory_));
  ASSERT_TRUE (session.open (ms (2000)).has_value ());
  std::vector<std::uint8_t> buffer (3);
  auto read
      = session.async_read_some (tr::mutable_bytes_t (buffer), asio::deferred);
  auto wait = session.async_wait_state_change (state::closed, asio::deferred);
  factory_->last ().feed (three ());
  std::promise<std::size_t> got;
  std::move (read) ([&got] (std::error_code e, std::size_t n)
                      { got.set_value (e ? 0 : n); });
  std::future<std::size_t> bytes = got.get_future ();
  ASSERT_EQ (bytes.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  EXPECT_EQ (bytes.get (), 3u);
  std::promise<state> now;
  std::move (wait) ([&now] (std::error_code, state s) { now.set_value (s); });
  std::future<state> seen = now.get_future ();
  ASSERT_EQ (seen.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  EXPECT_EQ (seen.get (), state::open);
}

TEST_F (
    LumexSessionTokensTest,
    GivenDeferred_WhenAMessageSessionReceivesAndOpensLater_ThenTheyComplete)
{
  ss::message_session session (runner_.get_executor (),
                               make_datagram_factory (datagrams_));
  auto open = session.async_open (asio::deferred);
  std::promise<std::error_code> opened;
  std::move (open) ([&opened] (std::error_code e) { opened.set_value (e); });
  std::future<std::error_code> open_result = opened.get_future ();
  ASSERT_EQ (open_result.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  EXPECT_FALSE (open_result.get ());
  auto receive = session.async_receive (asio::deferred);
  tr::datagram::message_t message;
  message.payload = three ();
  std::promise<std::size_t> got;
  std::move (receive) ([&got] (std::error_code e, tr::datagram::message_t m)
                         { got.set_value (e ? 0 : m.payload.size ()); });
  datagrams_->last ().deliver (message);
  std::future<std::size_t> bytes = got.get_future ();
  ASSERT_EQ (bytes.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  EXPECT_EQ (bytes.get (), 3u);
}

// TK2b: the same on a plain byte_stream.
TEST (LumexSessionTokensBaseTest,
      GivenDeferredOnAPlainByteStream_WhenLaunchedLater_ThenTheReadCompletes)
{
  tr::stream::io_runner runner (1);
  tr::testing::mock_stream made
      = tr::testing::make_mock_stream (runner.get_executor (), "plain");
  ASSERT_NO_THROW (made.stream.async_open (asio::use_future).get ());
  std::vector<std::uint8_t> buffer (3);
  auto read = made.stream.async_read_some (tr::mutable_bytes_t (buffer),
                                           asio::deferred);
  made.control.feed (three ());
  std::promise<std::size_t> got;
  std::move (read) ([&got] (std::error_code e, std::size_t n)
                      { got.set_value (e ? 0 : n); });
  std::future<std::size_t> bytes = got.get_future ();
  ASSERT_EQ (bytes.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  EXPECT_EQ (bytes.get (), 3u);
  made.stream.close ();
}

// TK3
TEST_F (
    LumexSessionTokensTest,
    GivenAsTupleUseFuture_WhenTheStateWaitIsCancelled_ThenTheTupleCarriesTheError)
{
  ss::stream_session session (runner_.get_executor (),
                              make_stream_factory (factory_));
  ASSERT_TRUE (session.open (ms (2000)).has_value ());
  std::future<std::tuple<std::error_code, state>> changed
      = session.async_wait_state_change (state::open,
                                         asio::as_tuple (asio::use_future));
  session.cancel ();
  ASSERT_EQ (changed.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  std::tuple<std::error_code, state> const result = changed.get ();
  EXPECT_TRUE (std::get<0> (result) == transport_errc::cancelled);
  EXPECT_EQ (std::get<1> (result), state::open);
}

// TK4
TEST_F (LumexSessionTokensTest,
        GivenAMessageSession_WhenReceivedByCallbackAndFuture_ThenBothComplete)
{
  ss::message_session session (runner_.get_executor (),
                               make_datagram_factory (datagrams_));
  ASSERT_TRUE (session.open (ms (2000)).has_value ());
  tr::datagram::message_t message;
  message.payload = three ();
  std::future<tr::datagram::message_t> first
      = session.async_receive (asio::use_future);
  datagrams_->last ().deliver (message);
  ASSERT_EQ (first.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  EXPECT_EQ (first.get ().payload, three ());
  std::promise<std::size_t> got;
  session.async_receive ([&got] (std::error_code e, tr::datagram::message_t m)
                           { got.set_value (e ? 0 : m.payload.size ()); });
  datagrams_->last ().deliver (message);
  std::future<std::size_t> second = got.get_future ();
  ASSERT_EQ (second.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  EXPECT_EQ (second.get (), 3u);
}

#if defined(LUMEX_TRANSPORT_WITH_SPAWN)
// TK5
TEST_F (LumexSessionTokensTest,
        GivenAYieldContext_WhenOpenReadAndStateWait_ThenTheyComplete)
{
  ss::stream_session session (runner_.get_executor (),
                              make_stream_factory (factory_));
  std::shared_ptr<stream_factory_state> const factory = factory_;
  std::promise<std::size_t> done;
  asio::spawn (
      runner_.get_executor (),
      [&session, &done, factory] (asio::yield_context yield)
        {
          std::error_code error;
          session.async_open (yield[error]);
          if (error)
            {
              done.set_value (100);
              return;
            }
          std::vector<std::uint8_t> buffer (3);
          factory->last ().feed (three ());
          std::size_t const n = session.async_read_some (
              tr::mutable_bytes_t (buffer), yield[error]);
          state const s
              = session.async_wait_state_change (state::closed, yield[error]);
          done.set_value (s == state::open ? n : 200);
        },
      asio::detached);
  std::future<std::size_t> result = done.get_future ();
  ASSERT_EQ (result.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  EXPECT_EQ (result.get (), 3u);
}
#endif

#if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
// TK6
TEST_F (LumexSessionTokensTest,
        GivenAnAwaitable_WhenOpenReadReceiveAndStateWait_ThenTheyComplete)
{
  ss::stream_session session (runner_.get_executor (),
                              make_stream_factory (factory_));
  ss::message_session messages (runner_.get_executor (),
                                make_datagram_factory (datagrams_));
  std::shared_ptr<stream_factory_state> const factory = factory_;
  std::shared_ptr<datagram_factory_state> const datagrams = datagrams_;
  std::future<std::size_t> result = asio::co_spawn (
      runner_.get_executor (),
      [&session, &messages, factory,
       datagrams] () -> asio::awaitable<std::size_t>
        {
          co_await session.async_open (asio::use_awaitable);
          co_await messages.async_open (asio::use_awaitable);
          std::vector<std::uint8_t> buffer (3);
          factory->last ().feed (three ());
          std::size_t const n = co_await session.async_read_some (
              tr::mutable_bytes_t (buffer), asio::use_awaitable);
          tr::datagram::message_t message;
          message.payload = three ();
          datagrams->last ().deliver (message);
          tr::datagram::message_t const got
              = co_await messages.async_receive (asio::use_awaitable);
          state const s = co_await session.async_wait_state_change (
              state::closed, asio::use_awaitable);
          co_return s == state::open ? n + got.payload.size () : 0;
        },
      asio::use_future);
  ASSERT_EQ (result.wait_for (std::chrono::seconds (2)),
             std::future_status::ready);
  EXPECT_EQ (result.get (), 6u);
}
#endif

// TK7: a terminal cancellation of the user's slot ends the pending read
// (the session forwards the slot; the transport base handles it).
TEST_F (
    LumexSessionTokensTest,
    GivenACancellationSlot_WhenTheSignalIsEmitted_ThenThePendingReadEndsCancelled)
{
  ss::stream_session session (runner_.get_executor (),
                              make_stream_factory (factory_));
  ASSERT_TRUE (session.open (ms (2000)).has_value ());
  std::vector<std::uint8_t> buffer (3);
  asio::cancellation_signal signal;
  outcome<int> read;
  session.async_read_some (tr::mutable_bytes_t (buffer),
                           asio::bind_cancellation_slot (
                               signal.slot (),
                               [&read] (std::error_code e, std::size_t n)
                                 { read.set (e, static_cast<int> (n)); }));
  std::this_thread::sleep_for (ms (30));
  asio::post (runner_.get_executor (), [&signal] ()
                { signal.emit (asio::cancellation_type::terminal); });
  bool const ended = read.wait (ms (300));
  EXPECT_TRUE (ended) << "the pending read ignores the cancellation slot";
  if (ended)
    EXPECT_TRUE (read.error == transport_errc::cancelled);
  session.close ();
  ASSERT_TRUE (read.wait (ms (2000)));
}

// TK8, TK8b, TK9: the session keeps no work on a handler's executor (as the
// transports): without a work guard, run () of the handler's own context
// returns before the handler runs; with an executor_work_guard on it, run ()
// waits for the handler.
using context_guard
    = asio::executor_work_guard<asio::io_context::executor_type>;

// TK8
TEST_F (
    LumexSessionTokensTest,
    GivenAStateWaitBoundToAnotherContext_WhenThatContextRuns_ThenOnlyAWorkGuardKeepsItRunningUntilTheHandler)
{
  ss::stream_session session (runner_.get_executor (),
                              make_stream_factory (factory_));
  ASSERT_TRUE (session.open (ms (2000)).has_value ());
  asio::io_context other;
  std::atomic<bool> called (false);
  std::unique_ptr<context_guard> guard;
  session.async_wait_state_change (
      state::open,
      asio::bind_executor (other,
                           [&called, &guard] (std::error_code, state)
                             {
                               called.store (true);
                               guard.reset ();
                             }));
  std::this_thread::sleep_for (ms (30));
  other.run_for (std::chrono::seconds (2));
  EXPECT_FALSE (called.load ()) << "no work: run () returned at once";
  guard.reset (new context_guard (asio::make_work_guard (other)));
  std::thread closer (
      [&session] ()
        {
          std::this_thread::sleep_for (ms (100));
          session.close ();
        });
  other.restart ();
  clock_type::time_point const start = clock_type::now ();
  other.run_for (std::chrono::seconds (2));
  closer.join ();
  EXPECT_TRUE (called.load ())
      << "the work guard kept run () going until the handler";
  EXPECT_LT (clock_type::now () - start, ms (1500));
  // The thread that posted the handler to `other` may still be finishing its
  // wake-up of that context: `other` dies at the end of this body, before the
  // runner of the fixture, so the runner stops first.
  runner_.stop ();
}

// TK8b
TEST_F (
    LumexSessionTokensTest,
    GivenAnOpenWaitBoundToAnotherContext_WhenThatContextRuns_ThenOnlyAWorkGuardKeepsItRunningUntilTheHandler)
{
  factory_->hold_call = 1;
  ss::stream_session session (runner_.get_executor (),
                              make_stream_factory (factory_));
  asio::io_context other;
  std::atomic<bool> called (false);
  std::unique_ptr<context_guard> guard;
  session.async_open (asio::bind_executor (other,
                                           [&called, &guard] (std::error_code)
                                             {
                                               called.store (true);
                                               guard.reset ();
                                             }));
  ASSERT_TRUE (eventually ([this] { return factory_->call_count () == 1; }));
  std::this_thread::sleep_for (ms (30));
  other.run_for (std::chrono::seconds (2));
  EXPECT_FALSE (called.load ()) << "no work: run () returned at once";
  guard.reset (new context_guard (asio::make_work_guard (other)));
  std::thread closer (
      [&session] ()
        {
          std::this_thread::sleep_for (ms (100));
          session.close ();
        });
  other.restart ();
  clock_type::time_point const start = clock_type::now ();
  other.run_for (std::chrono::seconds (2));
  closer.join ();
  EXPECT_TRUE (called.load ())
      << "the work guard kept run () going until the handler";
  EXPECT_LT (clock_type::now () - start, ms (1500));
  // The thread that posted the handler to `other` may still be finishing its
  // wake-up of that context: `other` dies at the end of this body, before the
  // runner of the fixture, so the runner stops first.
  runner_.stop ();
}

// TK9
TEST_F (
    LumexSessionTokensTest,
    GivenAReadBoundToAnotherContext_WhenThatContextRuns_ThenOnlyAWorkGuardKeepsItRunningUntilTheHandler)
{
  ss::stream_session session (runner_.get_executor (),
                              make_stream_factory (factory_));
  ASSERT_TRUE (session.open (ms (2000)).has_value ());
  asio::io_context other;
  std::atomic<bool> called (false);
  std::unique_ptr<context_guard> guard;
  std::vector<std::uint8_t> buffer (3);
  session.async_read_some (
      tr::mutable_bytes_t (buffer),
      asio::bind_executor (other,
                           [&called, &guard] (std::error_code, std::size_t)
                             {
                               called.store (true);
                               guard.reset ();
                             }));
  std::this_thread::sleep_for (ms (30));
  other.run_for (std::chrono::seconds (2));
  EXPECT_FALSE (called.load ()) << "no work: run () returned at once";
  guard.reset (new context_guard (asio::make_work_guard (other)));
  tr::testing::mock_stream_control control = factory_->last ();
  std::thread feeder (
      [control] () mutable
        {
          std::this_thread::sleep_for (ms (100));
          control.feed (three ());
        });
  other.restart ();
  clock_type::time_point const start = clock_type::now ();
  other.run_for (std::chrono::seconds (2));
  feeder.join ();
  EXPECT_TRUE (called.load ())
      << "the work guard kept run () going until the handler";
  EXPECT_LT (clock_type::now () - start, ms (1500));
  // The thread that posted the handler to `other` may still be finishing its
  // wake-up of that context: `other` dies at the end of this body, before the
  // runner of the fixture, so the runner stops first.
  runner_.stop ();
}

// TK10: a plain byte_stream keeps no work on the handler's executor either:
// the handler runs at the next run () after its completion.
TEST (
    LumexSessionTokensBaseTest,
    GivenAPlainStreamReadBoundToAnotherContext_WhenThatContextRuns_ThenItWaitsForTheHandler)
{
  tr::stream::io_runner runner (1);
  tr::testing::mock_stream made
      = tr::testing::make_mock_stream (runner.get_executor (), "plain");
  ASSERT_NO_THROW (made.stream.async_open (asio::use_future).get ());
  asio::io_context other;
  std::atomic<bool> called (false);
  std::vector<std::uint8_t> buffer (3);
  made.stream.async_read_some (
      tr::mutable_bytes_t (buffer),
      asio::bind_executor (other, [&called] (std::error_code, std::size_t)
                             { called.store (true); }));
  std::this_thread::sleep_for (ms (30));
  tr::testing::mock_stream_control control = made.control;
  std::thread feeder (
      [control] () mutable
        {
          std::this_thread::sleep_for (ms (100));
          control.feed (three ());
        });
  other.run_for (std::chrono::seconds (2));
  feeder.join ();
  EXPECT_FALSE (called.load ())
      << "the base keeps no work on the handler's executor";
  other.restart ();
  other.run_for (std::chrono::seconds (2));
  EXPECT_TRUE (called.load ());
  made.stream.close ();
}

// D3 for the session's waiters: a terminal cancellation on the slot of a
// state wait does what cancel () does: the wait ends with cancelled, the open
// session keeps its link.
TEST_F (
    LumexSessionTokensTest,
    GivenAStateWaitWithACancellationSlot_WhenTerminalIsEmitted_ThenItEndsCancelledAndTheSessionStaysOpen)
{
  ss::stream_session session (runner_.get_executor (),
                              make_stream_factory (factory_));
  ASSERT_TRUE (session.open (ms (2000)).has_value ());
  asio::cancellation_signal signal;
  outcome<int> changed;
  session.async_wait_state_change (
      state::open,
      asio::bind_cancellation_slot (
          signal.slot (), [&changed] (std::error_code e, state now)
            { changed.set (e, static_cast<int> (now)); }));
  std::this_thread::sleep_for (ms (30));
  EXPECT_FALSE (changed.is_done ());
  asio::post (runner_.get_executor (), [&signal] ()
                { signal.emit (asio::cancellation_type::terminal); });
  ASSERT_TRUE (changed.wait (ms (1000)))
      << "the state wait ignores the cancellation slot";
  EXPECT_TRUE (changed.error == transport_errc::cancelled);
  std::this_thread::sleep_for (ms (20));
  EXPECT_EQ (session.state (), state::open);
  session.close ();
}

// The same for an open wait: the connecting cycle ends in closed.
TEST_F (
    LumexSessionTokensTest,
    GivenAnOpenWaitWithACancellationSlot_WhenTerminalIsEmitted_ThenItEndsCancelledAndTheCycleEnds)
{
  factory_->hold_call = 1;
  ss::stream_session session (runner_.get_executor (),
                              make_stream_factory (factory_));
  asio::cancellation_signal signal;
  outcome<int> opened;
  session.async_open (asio::bind_cancellation_slot (
      signal.slot (), [&opened] (std::error_code e) { opened.set (e, 0); }));
  ASSERT_TRUE (eventually ([this] { return factory_->call_count () == 1; }));
  asio::post (runner_.get_executor (), [&signal] ()
                { signal.emit (asio::cancellation_type::terminal); });
  ASSERT_TRUE (opened.wait (ms (1000)))
      << "the open wait ignores the cancellation slot";
  EXPECT_TRUE (opened.error == transport_errc::cancelled);
  EXPECT_TRUE (
      eventually ([&session] { return session.state () == state::closed; }));
  EXPECT_EQ (factory_->call_count (), 1u);
}

// partial and total alone change nothing (as for the transports).
TEST_F (
    LumexSessionTokensTest,
    GivenAStateWaitWithACancellationSlot_WhenOnlyPartialIsEmitted_ThenItKeepsWaiting)
{
  ss::stream_session session (runner_.get_executor (),
                              make_stream_factory (factory_));
  ASSERT_TRUE (session.open (ms (2000)).has_value ());
  asio::cancellation_signal signal;
  outcome<int> changed;
  session.async_wait_state_change (
      state::open,
      asio::bind_cancellation_slot (
          signal.slot (), [&changed] (std::error_code e, state now)
            { changed.set (e, static_cast<int> (now)); }));
  asio::post (runner_.get_executor (),
              [&signal] ()
                {
                  signal.emit (asio::cancellation_type::partial
                               | asio::cancellation_type::total);
                });
  EXPECT_FALSE (changed.wait (ms (100)));
  session.close ();
  ASSERT_TRUE (changed.wait ());
  EXPECT_TRUE (changed.error == transport_errc::cancelled);
}

// The initiations report the session strand: asio::cancel_after on a
// session read ends it with cancelled at the timeout.
TEST_F (LumexSessionTokensTest,
        GivenCancelAfter_WhenNothingArrives_ThenTheReadEndsCancelledInTime)
{
  ss::stream_session session (runner_.get_executor (),
                              make_stream_factory (factory_));
  ASSERT_TRUE (session.open (ms (2000)).has_value ());
  std::vector<std::uint8_t> buffer (3);
  outcome<int> read;
  clock_type::time_point const start = clock_type::now ();
  // asio::cancel_after must be started inside a strand (Asio's documented
  // thread safety: the timer's handler and the initiation both touch the
  // cancellation slot of the operation): a step of the session strand.
  asio::post (session.get_strand (),
              [&session, &buffer, &read] ()
                {
                  session.async_read_some (
                      tr::mutable_bytes_t (buffer),
                      asio::cancel_after (
                          ms (50), [&read] (std::error_code e, std::size_t n)
                            { read.set (e, static_cast<int> (n)); }));
                });
  ASSERT_TRUE (read.wait (ms (2000)));
  EXPECT_TRUE (read.error == transport_errc::cancelled);
  EXPECT_GE (clock_type::now () - start, ms (40));
  EXPECT_EQ (session.state (), state::open);
  session.close ();
}

// C1 (round 3 re-review, M1): a terminal cancellation emitted after an open
// waiter completed does not reach the session.
TEST_F (
    LumexSessionTokensTest,
    GivenAnOpenWaitThatCompleted_WhenItsSlotIsEmittedLater_ThenAPendingReadIsNotCancelled)
{
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      runner_.get_executor (), make_stream_factory (factory_)));
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  asio::cancellation_signal signal;
  outcome<int> opened;
  session->async_open (asio::bind_cancellation_slot (
      signal.slot (), [&opened] (std::error_code e) { opened.set (e, 0); }));
  ASSERT_TRUE (opened.wait ());
  ASSERT_FALSE (opened.error);
  std::vector<std::uint8_t> buffer (4);
  outcome<std::size_t> read;
  session->async_read_some (tr::mutable_bytes_t (buffer),
                            [&read] (std::error_code e, std::size_t n)
                              { read.set (e, n); });
  std::this_thread::sleep_for (ms (30));
  signal.emit (asio::cancellation_type::terminal);
  bool const ended = read.wait (ms (300));
  EXPECT_FALSE (ended)
      << "a signal of a completed waiter cancelled the session";
  session->close ();
  ASSERT_TRUE (read.wait ());
}

// C2 (M1): the same with a state wait.
TEST_F (
    LumexSessionTokensTest,
    GivenAStateWaitThatCompleted_WhenItsSlotIsEmittedLater_ThenAPendingReadIsNotCancelled)
{
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      runner_.get_executor (), make_stream_factory (factory_)));
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  asio::cancellation_signal signal;
  outcome<int> changed;
  session->async_wait_state_change (
      state::closed,
      asio::bind_cancellation_slot (
          signal.slot (), [&changed] (std::error_code e, state s)
            { changed.set (e, static_cast<int> (s)); }));
  ASSERT_TRUE (changed.wait ());
  std::vector<std::uint8_t> buffer (4);
  outcome<std::size_t> read;
  session->async_read_some (tr::mutable_bytes_t (buffer),
                            [&read] (std::error_code e, std::size_t n)
                              { read.set (e, n); });
  std::this_thread::sleep_for (ms (30));
  signal.emit (asio::cancellation_type::terminal);
  bool const ended = read.wait (ms (300));
  EXPECT_FALSE (ended)
      << "a signal of a completed waiter cancelled the session: "
      << read.error.message ();
  session->close ();
  ASSERT_TRUE (read.wait ());
}

namespace
{
// Every step posted to the session strand before it has run: a state wait
// that completes at once is posted behind them (the strand runs in order).
void
strand_barrier (ss::stream_session &session)
{
  outcome<int> passed;
  session.async_wait_state_change (state::closed,
                                   [&passed] (std::error_code e, state)
                                     { passed.set (e, 0); });
  ASSERT_TRUE (passed.wait ());
}
} // namespace

// Todo 163: the state wait completed and its handler waits on an executor
// nobody runs yet; the signal emitted now ends nothing (the session handed
// the result on), the pending read goes on.
TEST_F (
    LumexSessionTokensTest,
    GivenAStateWaitWhoseHandlerHasNotRun_WhenItsSlotIsEmitted_ThenAPendingReadIsNotCancelled)
{
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      runner_.get_executor (), make_stream_factory (factory_)));
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  std::vector<std::uint8_t> buffer (4);
  outcome<std::size_t> read;
  session->async_read_some (tr::mutable_bytes_t (buffer),
                            [&read] (std::error_code e, std::size_t n)
                              { read.set (e, n); });
  asio::io_context later;
  asio::cancellation_signal signal;
  int changed = 0;
  session->async_wait_state_change (
      state::closed,
      asio::bind_cancellation_slot (
          signal.slot (),
          asio::bind_executor (later, [&changed] (std::error_code, state)
                                 { ++changed; })));
  // The wait completes at once on the strand; its handler goes to `later`.
  strand_barrier (*session);
  ASSERT_EQ (changed, 0);
  signal.emit (asio::cancellation_type::terminal);
  bool const ended = read.wait (ms (300));
  EXPECT_FALSE (ended) << "a signal after the result cancelled the session: "
                       << read.error.message ();
  EXPECT_TRUE (session->is_open ());
  later.run ();
  EXPECT_EQ (changed, 1);
  session->close ();
  ASSERT_TRUE (read.wait ());
}

// REVIEW163 F2: the same for a snapshot wait and an open wait.
TEST_F (
    LumexSessionTokensTest,
    GivenASnapshotAndAnOpenWaitWhoseHandlersHaveNotRun_WhenTheirSlotsAreEmitted_ThenAPendingReadIsNotCancelled)
{
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      runner_.get_executor (), make_stream_factory (factory_)));
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  std::vector<std::uint8_t> buffer (4);
  outcome<std::size_t> read;
  session->async_read_some (tr::mutable_bytes_t (buffer),
                            [&read] (std::error_code e, std::size_t n)
                              { read.set (e, n); });
  asio::io_context later;
  int done = 0;
  asio::cancellation_signal snapshot_signal;
  // A snapshot older than the session's: the wait completes at once.
  session->async_wait_state_change (
      ss::state_snapshot_t (),
      asio::bind_cancellation_slot (
          snapshot_signal.slot (),
          asio::bind_executor (later,
                               [&done] (std::error_code, ss::state_snapshot_t)
                                 { ++done; })));
  asio::cancellation_signal open_signal;
  // The session is open: the open wait completes at once.
  session->async_open (asio::bind_cancellation_slot (
      open_signal.slot (),
      asio::bind_executor (later, [&done] (std::error_code) { ++done; })));
  strand_barrier (*session);
  ASSERT_EQ (done, 0);
  snapshot_signal.emit (asio::cancellation_type::terminal);
  open_signal.emit (asio::cancellation_type::terminal);
  bool const ended = read.wait (ms (300));
  EXPECT_FALSE (ended) << "a signal after the result cancelled the session: "
                       << read.error.message ();
  EXPECT_TRUE (session->is_open ());
  later.run ();
  EXPECT_EQ (done, 2);
  session->close ();
  ASSERT_TRUE (read.wait ());
}

// C3 (M2): a terminal cancellation emitted right after a session read is
// called (from a handler on the session strand, so the step that starts the
// read cannot run first) ends the read.
TEST_F (
    LumexSessionTokensTest,
    GivenASessionReadWithASlot_WhenTerminalIsEmittedRightAfterTheCall_ThenTheReadEndsCancelled)
{
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      runner_.get_executor (), make_stream_factory (factory_)));
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  std::vector<std::uint8_t> buffer (4);
  asio::cancellation_signal signal;
  outcome<std::size_t> read;
  ss::stream_session *const raw = session.get ();
  outcome<int> on_strand;
  session->async_wait_state_change (
      state::closed,
      [raw, &buffer, &signal, &read, &on_strand] (std::error_code, state)
        {
          raw->async_read_some (tr::mutable_bytes_t (buffer),
                                asio::bind_cancellation_slot (
                                    signal.slot (),
                                    [&read] (std::error_code e, std::size_t n)
                                      { read.set (e, n); }));
          signal.emit (asio::cancellation_type::terminal);
          on_strand.set (std::error_code (), 0);
        });
  ASSERT_TRUE (on_strand.wait ());
  bool const ended = read.wait (ms (300));
  EXPECT_TRUE (ended) << "the terminal cancellation was lost";
  session->close ();
  ASSERT_TRUE (read.wait ());
}

// C3b: the control on a plain byte_stream.
TEST (
    LumexSessionTokensBaseTest,
    GivenAPlainStreamReadWithASlot_WhenTerminalIsEmittedRightAfterTheCall_ThenTheReadEndsCancelled)
{
  tr::stream::io_runner runner (1);
  tr::testing::mock_stream made
      = tr::testing::make_mock_stream (runner.get_executor (), "plain");
  ASSERT_NO_THROW (made.stream.async_open (asio::use_future).get ());
  std::vector<std::uint8_t> buffer (4);
  asio::cancellation_signal signal;
  outcome<std::size_t> read;
  std::promise<void> done;
  asio::post (runner.get_executor (),
              [&] ()
                {
                  made.stream.async_read_some (
                      tr::mutable_bytes_t (buffer),
                      asio::bind_cancellation_slot (
                          signal.slot (),
                          [&read] (std::error_code e, std::size_t n)
                            { read.set (e, n); }));
                  signal.emit (asio::cancellation_type::terminal);
                  done.set_value ();
                });
  done.get_future ().wait ();
  EXPECT_TRUE (read.wait (ms (300)));
  made.stream.close ();
  ASSERT_TRUE (read.wait ());
}

// C4: the executor the initiations report, observed through a token.
TEST_F (
    LumexSessionTokensTest,
    GivenASessionAndAMovedFromOne_WhenATokenAsksTheInitiation_ThenItReportsTheStrandAndTheSystemExecutor)
{
  ss::stream_session session (runner_.get_executor (),
                              make_stream_factory (factory_));
  capture_executor_t token = { std::make_shared<tr::executor_type> () };
  session.async_wait_state_change (state::closed, token);
  using strand_type = asio::strand<tr::executor_type>;
  EXPECT_TRUE (token.out->target<strand_type> () != nullptr)
      << "the state wait does not report the session strand";
  capture_executor_t io = { std::make_shared<tr::executor_type> () };
  std::vector<std::uint8_t> buffer (4);
  session.async_read_some (tr::mutable_bytes_t (buffer), io);
  EXPECT_TRUE (io.out->target<strand_type> () != nullptr)
      << "the read does not report the session strand";
  ss::stream_session moved (std::move (session));
  capture_executor_t gone = { std::make_shared<tr::executor_type> () };
  session.async_open (gone);
  EXPECT_TRUE (gone.out->target<asio::system_executor> () != nullptr);
}

// C5: terminal cancellations racing the completion of the same waiters:
// every handler ends exactly once.
TEST_F (
    LumexSessionTokensTest,
    GivenManyStateWaitsCancelledWhileTheyComplete_WhenRacing_ThenEachEndsOnce)
{
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      runner_.get_executor (), make_stream_factory (factory_)));
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  std::shared_ptr<std::atomic<int>> const calls
      = std::make_shared<std::atomic<int>> (0);
  std::shared_ptr<std::atomic<int>> const twice
      = std::make_shared<std::atomic<int>> (0);
  int const rounds = 300;
  for (int i = 0; i < rounds; ++i)
    {
      std::shared_ptr<asio::cancellation_signal> const signal
          = std::make_shared<asio::cancellation_signal> ();
      std::shared_ptr<std::atomic<bool>> const once
          = std::make_shared<std::atomic<bool>> (false);
      session->async_wait_state_change (
          i % 2 ? state::closed : state::open,
          asio::bind_cancellation_slot (
              signal->slot (),
              [calls, twice, once, signal] (std::error_code, state)
                {
                  if (once->exchange (true))
                    ++*twice;
                  else
                    ++*calls;
                }));
      signal->emit (asio::cancellation_type::terminal);
    }
  EXPECT_TRUE (eventually ([&] { return calls->load () == rounds; }))
      << calls->load ();
  EXPECT_EQ (twice->load (), 0);
  EXPECT_EQ (session->state (), state::open);
}

// E1 (round 4 re-review): the session is not open when its I/O step runs:
// the read ends with not_open; a terminal emitted right after the call
// changes nothing.
TEST_F (
    LumexSessionTokensTest,
    GivenAClosedSession_WhenAReadWithASlotIsCancelledRightAfterTheCall_ThenItEndsNotOpenOnce)
{
  ss::stream_session session (runner_.get_executor (),
                              make_stream_factory (factory_));
  std::vector<std::uint8_t> buffer (4);
  for (int i = 0; i < 200; ++i)
    {
      asio::cancellation_signal signal;
      std::shared_ptr<std::atomic<int>> const calls
          = std::make_shared<std::atomic<int>> (0);
      std::promise<std::error_code> done;
      session.async_read_some (
          tr::mutable_bytes_t (buffer),
          asio::bind_cancellation_slot (
              signal.slot (),
              [calls, &done] (std::error_code e, std::size_t)
                {
                  if (calls->fetch_add (1) == 0)
                    done.set_value (e);
                }));
      signal.emit (asio::cancellation_type::terminal);
      std::future<std::error_code> f = done.get_future ();
      ASSERT_EQ (f.wait_for (std::chrono::seconds (2)),
                 std::future_status::ready);
      EXPECT_TRUE (f.get () == transport_errc::not_open);
      std::this_thread::sleep_for (ms (1));
      EXPECT_EQ (calls->load (), 1);
    }
}

// E2: one slot reused for two reads: the first completes with data, a later
// emit does nothing; the second read takes the slot and is cancelled by the
// next emit, and only it.
TEST_F (
    LumexSessionTokensTest,
    GivenOneSlotReusedForTwoReads_WhenEmittedBetweenAndAfter_ThenOnlyThePendingReadIsCancelled)
{
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      runner_.get_executor (), make_stream_factory (factory_)));
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  asio::cancellation_signal signal;
  std::vector<std::uint8_t> buffer (3);
  outcome<std::size_t> first;
  session->async_read_some (
      tr::mutable_bytes_t (buffer),
      asio::bind_cancellation_slot (signal.slot (),
                                    [&first] (std::error_code e, std::size_t n)
                                      { first.set (e, n); }));
  factory_->last ().feed (std::vector<std::uint8_t> (3, 1));
  ASSERT_TRUE (first.wait ());
  EXPECT_FALSE (first.error);
  // An unrelated read without a slot is pending; a stale emit must not
  // cancel it.
  outcome<std::size_t> other;
  session->async_read_some (tr::mutable_bytes_t (buffer),
                            [&other] (std::error_code e, std::size_t n)
                              { other.set (e, n); });
  std::this_thread::sleep_for (ms (20));
  signal.emit (asio::cancellation_type::terminal);
  EXPECT_FALSE (other.wait (ms (200))) << other.error.message ();
  factory_->last ().feed (std::vector<std::uint8_t> (3, 2));
  ASSERT_TRUE (other.wait ());
  outcome<std::size_t> second;
  session->async_read_some (tr::mutable_bytes_t (buffer),
                            asio::bind_cancellation_slot (
                                signal.slot (),
                                [&second] (std::error_code e, std::size_t n)
                                  { second.set (e, n); }));
  std::this_thread::sleep_for (ms (20));
  signal.emit (asio::cancellation_type::terminal);
  ASSERT_TRUE (second.wait (ms (500)));
  EXPECT_TRUE (second.error == transport_errc::cancelled);
  EXPECT_EQ (session->state (), state::open);
}

// E3: a session read with a slot, many rounds where the data and the emit
// race: each handler ends once, the session stays open.
TEST_F (LumexSessionTokensTest,
        GivenReadsWhoseDataAndCancelRace_WhenRepeated_ThenEachEndsOnce)
{
  std::unique_ptr<ss::stream_session> session (new ss::stream_session (
      runner_.get_executor (), make_stream_factory (factory_)));
  ASSERT_TRUE (session->open (ms (2000)).has_value ());
  std::vector<std::uint8_t> buffer (1);
  std::shared_ptr<std::atomic<int>> const twice
      = std::make_shared<std::atomic<int>> (0);
  for (int i = 0; i < 300; ++i)
    {
      std::shared_ptr<asio::cancellation_signal> const signal
          = std::make_shared<asio::cancellation_signal> ();
      std::shared_ptr<std::atomic<int>> const calls
          = std::make_shared<std::atomic<int>> (0);
      std::shared_ptr<std::promise<void>> const done
          = std::make_shared<std::promise<void>> ();
      session->async_read_some (
          tr::mutable_bytes_t (buffer),
          asio::bind_cancellation_slot (
              signal->slot (),
              [calls, done, twice] (std::error_code, std::size_t)
                {
                  if (calls->fetch_add (1) == 0)
                    done->set_value ();
                  else
                    ++*twice;
                }));
      if (i % 2)
        factory_->last ().feed (std::vector<std::uint8_t> (1, 9));
      signal->emit (asio::cancellation_type::terminal);
      ASSERT_EQ (done->get_future ().wait_for (std::chrono::seconds (2)),
                 std::future_status::ready)
          << "round " << i;
    }
  EXPECT_EQ (twice->load (), 0);
  EXPECT_EQ (session->state (), state::open);
}

// get_strand () is the strand the initiations of a session report.
TEST_F (LumexSessionTokensTest,
        GivenASession_WhenGetStrand_ThenItIsTheStrandTheInitiationsReport)
{
  ss::stream_session session (runner_.get_executor (),
                              make_stream_factory (factory_));
  EXPECT_TRUE (tr::executor_type (session.get_strand ())
               == session_strand (session));
  session.close ();
}
