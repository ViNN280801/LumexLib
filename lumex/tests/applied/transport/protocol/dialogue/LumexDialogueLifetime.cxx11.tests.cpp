// The lifetime of the dialogue engine (lumex/applied/transport/protocol/
// dialogue): cancel and close, destruction (also inside its own handler and
// by the io_context), the cancellation of one exchange through its slot
// (also asio::cancel_after), handlers of the user that throw, a moved-from
// engine, and exchanges started from several threads.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/applied/transport/protocol/dialogue/LumexDialogueTestSupport.hpp"

namespace
{
namespace dt = lumex_dialogue_test;
using dt::bench_t;
using dt::bytes;
using dt::bytes_t;
using dt::ms;
using dt::outcome_ptr;
using dt::protocol_errc;
using dt::pump;
using dt::transport_errc;
namespace tr = dt::tr;
namespace pr = dt::pr;
namespace fr = dt::fr;

// The completion of an exchange that destroys the engine of the bench.
struct destroy_engine_t
{
  bench_t *bench;
  outcome_ptr out;

  void
  operator() (std::error_code error, pr::reply_frame_t frame)
  {
    ++out->calls;
    out->error = error;
    out->frame = std::move (frame);
    bench->the_dialogue.reset ();
  }
};

std::uint32_t
throwing_predicate (lumex::core::protocol::index::session_context_t const &,
                    std::error_code const &)
{
  throw std::runtime_error ("the predicate failed");
}

bool
throwing_matcher (lumex::core::protocol::index::row_t const *,
                  lumex::core::protocol::layout::bytes_view_t,
                  lumex::core::protocol::layout::bytes_view_t)
{
  throw std::runtime_error ("the matcher failed");
}

template <typename Condition>
bool
eventually (Condition condition, std::chrono::milliseconds limit
                                 = std::chrono::milliseconds (20000))
{
  std::chrono::steady_clock::time_point const end
      = std::chrono::steady_clock::now () + limit;
  while (!condition ())
    {
      if (std::chrono::steady_clock::now () >= end)
        return condition ();
      std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }
  return true;
}
} // namespace

TEST (
    LumexDialogueLifetimeTest,
    GivenQueuedAndPendingExchanges_WhenClosed_ThenAllEndCancelledAndLaterOnesClosed)
{
  pr::dialogue_options_t options;
  options.defaults.timeout_ms = 0;
  bench_t bench (options);
  bench.start ();
  outcome_ptr const pending
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  outcome_ptr const queued
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  bench.settle ();
  ASSERT_EQ (bench.requests ().size (), 1u);
  bench.the_dialogue->close ();
  bench.the_dialogue->close ();
  bench.settle ();
  ASSERT_EQ (pending->calls, 1);
  ASSERT_EQ (queued->calls, 1);
  EXPECT_EQ (pending->error, transport_errc::cancelled);
  EXPECT_EQ (queued->error, transport_errc::cancelled);
  outcome_ptr const later
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  bench.settle ();
  ASSERT_EQ (later->calls, 1);
  EXPECT_EQ (later->error, transport_errc::closed);
  // The link is closed: the queued request was never written.
  EXPECT_EQ (bench.requests ().size (), 1u);
}

TEST (
    LumexDialogueLifetimeTest,
    GivenQueuedAndPendingExchanges_WhenCancelled_ThenAllEndCancelledAndTheLinkStays)
{
  pr::dialogue_options_t options;
  options.defaults.timeout_ms = 0;
  bench_t bench (options);
  bench.start ();
  outcome_ptr const pending
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  outcome_ptr const queued
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  bench.settle ();
  bench.the_dialogue->cancel ();
  bench.settle ();
  EXPECT_EQ (pending->error, transport_errc::cancelled);
  EXPECT_EQ (queued->error, transport_errc::cancelled);
  // The late reply of the cancelled request, then a new exchange.
  bench.reply (bytes ({ 0x0C, 0x01 }));
  bench.settle ();
  EXPECT_EQ (bench.the_dialogue->statistics ().late_replies, 1u);
  bench.on_request ([&bench] (bytes_t const &)
                      { bench.reply (bytes ({ 0x0C, 0x02 })); });
  outcome_ptr const next
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  ASSERT_TRUE (bench.run_until ([&] () { return next->calls != 0; }));
  EXPECT_FALSE (next->error);
  EXPECT_EQ (next->frame, bytes ({ 0x0C, 0x02 }));
  EXPECT_EQ (pending->calls, 1);
  EXPECT_EQ (queued->calls, 1);
}

TEST (
    LumexDialogueLifetimeTest,
    GivenTwoExchanges_WhenTheFirstHandlerDestroysTheEngine_ThenTheSecondEndsCancelledOnce)
{
  pr::dialogue_options_t options;
  options.max_in_flight = 2;
  options.defaults.timeout_ms = 0;
  bench_t bench (options);
  bench.start ();
  bench.on_request (
      [&bench] (bytes_t const &request)
        {
          if (dt::key_of (request) == 0x0C)
            bench.reply (bytes ({ 0x0C, 0x01 }));
        });
  outcome_ptr const first = std::make_shared<dt::outcome_t> ();
  destroy_engine_t destroyer = { &bench, first };
  bench.the_dialogue->async_exchange (dt::make_exchange<pump::POLL_t> (),
                                      std::move (destroyer));
  outcome_ptr const second
      = bench.start_exchange (dt::make_exchange<pump::INFO_t> ());
  ASSERT_TRUE (bench.run_until ([&] () { return second->calls != 0; }));
  bench.settle ();
  EXPECT_EQ (first->calls, 1);
  EXPECT_FALSE (first->error);
  EXPECT_EQ (second->calls, 1);
  EXPECT_EQ (second->error, transport_errc::cancelled);
  EXPECT_EQ (bench.the_dialogue, nullptr);
}

TEST (
    LumexDialogueLifetimeTest,
    GivenPendingExchanges_WhenTheEngineIsDestroyed_ThenTheyEndCancelledAndTheDestructorReturnsAtOnce)
{
  pr::dialogue_options_t options;
  options.defaults.timeout_ms = 0;
  bench_t bench (options);
  bench.start ();
  outcome_ptr const pending
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  outcome_ptr const queued
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  bench.settle ();
  // Nothing runs the context during the destruction: it must not wait.
  bench.the_dialogue.reset ();
  EXPECT_EQ (pending->calls, 0);
  bench.settle ();
  EXPECT_EQ (pending->error, transport_errc::cancelled);
  EXPECT_EQ (queued->error, transport_errc::cancelled);
}

TEST (
    LumexDialogueLifetimeTest,
    GivenAHandlerThatOwnsTheEngine_WhenTheIoContextIsDestroyed_ThenItDiesUncalledAndTheEngineIsFreed)
{
  struct owner_t
  {
    std::unique_ptr<pr::dialogue_engine> engine;
  };
  std::weak_ptr<owner_t> watch;
  std::shared_ptr<int> calls = std::make_shared<int> (0);
  {
    asio::io_context context;
    tr::testing::virtual_clock clock;
    tr::testing::mock_options_t mock_options;
    mock_options.timers = clock.source ();
    tr::testing::mock_stream mock = tr::testing::make_mock_stream (
        context.get_executor (), mock_options);
    mock.stream.async_open ([] (std::error_code) {});
    context.poll ();
    pr::dialogue_options_t options;
    options.timers = clock.source ();
    std::shared_ptr<owner_t> owner = std::make_shared<owner_t> ();
    owner->engine.reset (new pr::dialogue_engine (
        pr::frame_link (tr::framed::framed_stream<fr::slip::slip_codec> (
            std::move (mock.stream), fr::slip::slip_codec ())),
        dt::pump_index (), options));
    watch = owner;
    owner->engine->async_exchange (
        dt::make_exchange<pump::POLL_t> (),
        [owner, calls] (std::error_code, pr::reply_frame_t) { ++*calls; });
    context.restart ();
    context.poll ();
    owner.reset ();
    EXPECT_FALSE (watch.expired ());
  }
  EXPECT_TRUE (watch.expired ());
  EXPECT_EQ (*calls, 0);
}

TEST (
    LumexDialogueLifetimeTest,
    GivenASignalOnOneExchange_WhenItIsEmitted_ThenThatExchangeEndsAndTheOthersAndTheLinkGoOn)
{
  pr::dialogue_options_t options;
  options.max_in_flight = 2;
  options.defaults.timeout_ms = 0;
  bench_t bench (options);
  bench.start ();
  asio::cancellation_signal on_info;
  asio::cancellation_signal on_queued;
  outcome_ptr const poll
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  outcome_ptr const info = std::make_shared<dt::outcome_t> ();
  dt::record_t info_handler = { info, &bench.clock };
  bench.the_dialogue->async_exchange (
      dt::make_exchange<pump::INFO_t> (),
      asio::bind_cancellation_slot (on_info.slot (),
                                    std::move (info_handler)));
  outcome_ptr const queued = std::make_shared<dt::outcome_t> ();
  dt::record_t queued_handler = { queued, &bench.clock };
  bench.the_dialogue->async_exchange (
      dt::make_exchange<pump::POLL_t> (),
      asio::bind_cancellation_slot (on_queued.slot (),
                                    std::move (queued_handler)));
  bench.settle ();
  ASSERT_EQ (bench.requests ().size (), 2u);
  pr::dialogue_engine::strand_type const strand
      = bench.the_dialogue->get_strand ();
  // A total cancellation ends a queued exchange, not one already written.
  asio::post (strand,
              [&on_queued, &on_info] ()
                {
                  on_queued.emit (asio::cancellation_type::total);
                  on_info.emit (asio::cancellation_type::total);
                });
  bench.settle ();
  ASSERT_EQ (queued->calls, 1);
  EXPECT_EQ (queued->error, transport_errc::cancelled);
  EXPECT_EQ (info->calls, 0);
  // A terminal one ends the written one; the other goes on.
  asio::post (strand, [&on_info] ()
                { on_info.emit (asio::cancellation_type::terminal); });
  bench.settle ();
  ASSERT_EQ (info->calls, 1);
  EXPECT_EQ (info->error, transport_errc::cancelled);
  EXPECT_EQ (poll->calls, 0);
  bench.reply (bytes ({ 0x0C, 0x07 }));
  bench.reply (bytes ({ 0x10, 'A', 'B', 'C', 0, 0, 0, 0, 0, 0x02, 0x01 }));
  bench.settle ();
  ASSERT_EQ (poll->calls, 1);
  EXPECT_FALSE (poll->error);
  EXPECT_EQ (bench.the_dialogue->statistics ().late_replies, 1u);
  // A signal after the result was handed on ends nothing.
  asio::post (strand, [&on_info] ()
                { on_info.emit (asio::cancellation_type::terminal); });
  bench.settle ();
  EXPECT_EQ (info->calls, 1);
  EXPECT_EQ (poll->calls, 1);
}

TEST (
    LumexDialogueLifetimeTest,
    GivenCancelAfterOnOneExchange_WhenItExpires_ThenThatExchangeEndsCancelledAndTheOtherIsAnswered)
{
  pr::dialogue_options_t options;
  options.max_in_flight = 2;
  options.defaults.timeout_ms = 0;
  bench_t bench (options);
  bench.start ();
  outcome_ptr const info
      = bench.start_exchange (dt::make_exchange<pump::INFO_t> ());
  outcome_ptr const poll = std::make_shared<dt::outcome_t> ();
  pr::dialogue_engine *const engine = bench.the_dialogue.get ();
  dt::record_t handler = { poll, &bench.clock };
  // Started inside the strand, as asio::cancel_after asks.
  asio::post (engine->get_strand (),
              [engine, handler] ()
                {
                  engine->async_exchange (
                      dt::make_exchange<pump::POLL_t> (),
                      asio::cancel_after (ms (20), handler));
                });
  bench.context.restart ();
  bench.context.run_for (std::chrono::seconds (10));
  ASSERT_EQ (poll->calls, 1);
  EXPECT_EQ (poll->error, transport_errc::cancelled);
  EXPECT_EQ (info->calls, 0);
  bench.reply (bytes ({ 0x10, 'A', 'B', 'C', 0, 0, 0, 0, 0, 0x02, 0x01 }));
  bench.settle ();
  ASSERT_EQ (info->calls, 1);
  EXPECT_FALSE (info->error);
  EXPECT_EQ (bench.requests ().size (), 2u);
}

TEST (
    LumexDialogueLifetimeTest,
    GivenAPredicateThatThrows_WhenTheAttemptsRunOut_ThenTheExchangeEndsAndTheExceptionLeavesRun)
{
  bench_t bench;
  bench.start ();
  pr::exchange_t request = dt::make_exchange<pump::POLL_t> ();
  request.policy = dt::policy (50, 0, &throwing_predicate);
  outcome_ptr const poll = bench.start_exchange (request);
  EXPECT_THROW (bench.advance (ms (100)), std::runtime_error);
  bench.settle ();
  ASSERT_EQ (poll->calls, 1);
  EXPECT_EQ (poll->error, protocol_errc::retries_exhausted);
  // The engine goes on.
  bench.on_request ([&bench] (bytes_t const &)
                      { bench.reply (bytes ({ 0x0C, 0x02 })); });
  outcome_ptr const next
      = bench.start_exchange (dt::make_exchange<pump::POLL_t> ());
  ASSERT_TRUE (bench.run_until ([&] () { return next->calls != 0; }));
  EXPECT_FALSE (next->error);
}

TEST (
    LumexDialogueLifetimeTest,
    GivenAMatcherThatThrows_WhenAFrameComes_ThenItAcceptsNothingAndTheExceptionLeavesRun)
{
  bench_t bench;
  bench.start ();
  pr::exchange_t request = dt::make_exchange<pump::POLL_t> ();
  request.how = dt::dialogue::correlation::custom;
  request.matcher = &throwing_matcher;
  request.policy = dt::policy (100, 0);
  outcome_ptr const poll = bench.start_exchange (request);
  bench.settle ();
  bench.reply (bytes ({ 0x0C, 0x02 }));
  EXPECT_THROW (bench.settle (), std::runtime_error);
  EXPECT_EQ (poll->calls, 0);
  EXPECT_EQ (bench.the_dialogue->statistics ().unexpected_replies, 1u);
  // The reading goes on and the exchange times out as usual.
  bench.reply (bytes ({ 0x7E }));
  EXPECT_THROW (bench.settle (), std::runtime_error);
  EXPECT_EQ (bench.the_dialogue->statistics ().unknown_frames, 1u);
  ASSERT_TRUE (bench.run_until ([&] () { return poll->calls != 0; }));
  EXPECT_EQ (poll->error, protocol_errc::timeout);
}

TEST (LumexDialogueLifetimeTest,
      GivenAMovedFromEngine_WhenUsed_ThenItAnswersNotOpenAndTheNewOwnerWorks)
{
  bench_t bench;
  bench.start ();
  pr::dialogue_engine moved (std::move (*bench.the_dialogue));
  std::future<pr::reply_frame_t> refused = bench.the_dialogue->async_exchange (
      dt::make_exchange<pump::POLL_t> (), asio::use_future);
  try
    {
      refused.get ();
      ADD_FAILURE () << "a moved-from engine took an exchange";
    }
  catch (std::system_error const &error)
    {
      EXPECT_EQ (error.code (), transport_errc::not_open);
    }
  EXPECT_EQ (bench.the_dialogue->statistics ().requests, 0u);
  EXPECT_EQ (bench.the_dialogue->name (), "");
  bench.the_dialogue->cancel ();
  bench.the_dialogue->close ();
  bench.the_dialogue->reset_statistics ();
  EXPECT_EQ (moved.name (), "pump");
  bench.on_request ([&bench] (bytes_t const &)
                      { bench.reply (bytes ({ 0x0C, 0x03 })); });
  outcome_ptr const out = std::make_shared<dt::outcome_t> ();
  dt::record_t handler = { out, &bench.clock };
  moved.async_exchange (dt::make_exchange<pump::POLL_t> (), handler);
  ASSERT_TRUE (bench.run_until ([&] () { return out->calls != 0; }));
  EXPECT_FALSE (out->error);
  EXPECT_EQ (moved.statistics ().replies, 1u);
}

TEST (
    LumexDialogueLifetimeTest,
    GivenSeveralThreads_WhenTheyStartExchangesAndTheEngineIsDestroyedMeanwhile_ThenEachEndsExactlyOnce)
{
  std::size_t const kthreads = 4;
  std::size_t const kanswered = 50;
  std::size_t const kunanswered = 10;
  std::size_t const total = kthreads * (kanswered + kunanswered);
  asio::io_context context;
  asio::executor_work_guard<asio::io_context::executor_type> guard
      = asio::make_work_guard (context);
  std::vector<std::thread> runners;
  for (int i = 0; i < 2; ++i)
    runners.push_back (std::thread ([&context] () { context.run (); }));
  tr::testing::mock_stream mock
      = tr::testing::make_mock_stream (context.get_executor (), "threads");
  std::promise<std::error_code> opened;
  mock.stream.async_open ([&opened] (std::error_code error)
                            { opened.set_value (error); });
  ASSERT_FALSE (opened.get_future ().get ());
  // The device answers every request at once, with the key of the request.
  std::shared_ptr<fr::slip::slip_codec> const decoder
      = std::make_shared<fr::slip::slip_codec> ();
  tr::testing::mock_stream_control control = mock.control;
  mock.control.on_write (
      [decoder, control] (bytes_t const &chunk) mutable
        {
          decoder->decode (
              fr::codec::bytes_view_t (chunk.data (), chunk.size ()),
              [&control] (fr::codec::bytes_view_t frame)
                {
                  if (frame.empty ())
                    return;
                  bytes_t wire;
                  bytes_t const reply = bytes ({ frame[0], 0x01 });
                  fr::slip::slip_codec ().encode (
                      fr::codec::bytes_view_t (reply.data (), reply.size ()),
                      wire);
                  control.feed (wire);
                });
        });
  pr::dialogue_options_t options;
  options.max_in_flight = 4;
  options.queue_bound = total;
  options.defaults.timeout_ms = 0;
  std::unique_ptr<pr::dialogue_engine> engine (new pr::dialogue_engine (
      pr::frame_link (tr::framed::framed_stream<fr::slip::slip_codec> (
          std::move (mock.stream), fr::slip::slip_codec ())),
      dt::pump_index (), options));
  std::vector<std::atomic<int>> calls (total);
  for (std::size_t i = 0; i < total; ++i)
    calls[i].store (0);
  std::atomic<std::size_t> answered (0);
  std::atomic<std::size_t> cancelled (0);
  std::atomic<std::size_t> other (0);
  std::vector<std::thread> starters;
  for (std::size_t t = 0; t < kthreads; ++t)
    starters.push_back (std::thread (
        [&, t] ()
          {
            for (std::size_t n = 0; n < kanswered + kunanswered; ++n)
              {
                std::size_t const slot = t * (kanswered + kunanswered) + n;
                pr::exchange_t request
                    = n < kanswered ? dt::make_exchange<pump::POLL_t> ()
                                    : dt::make_exchange<pump::INFO_t> ();
                engine->async_exchange (
                    std::move (request),
                    [&, slot] (std::error_code error, pr::reply_frame_t)
                      {
                        calls[slot].fetch_add (1);
                        if (!error)
                          answered.fetch_add (1);
                        else if (error == transport_errc::cancelled)
                          cancelled.fetch_add (1);
                        else
                          other.fetch_add (1);
                      });
              }
          }));
  for (std::size_t t = 0; t < starters.size (); ++t)
    starters[t].join ();
  // The engine is destroyed while the exchanges flow: the rest end
  // cancelled.
  EXPECT_TRUE (eventually ([&] () { return answered.load () >= total / 4; }));
  engine.reset ();
  EXPECT_TRUE (eventually (
      [&] ()
        {
          return answered.load () + cancelled.load () + other.load () == total;
        }));
  mock.control.on_write (tr::testing::write_observer_t ());
  guard.reset ();
  for (std::size_t i = 0; i < runners.size (); ++i)
    runners[i].join ();
  EXPECT_EQ (other.load (), 0u);
  EXPECT_EQ (answered.load () + cancelled.load (), total);
  for (std::size_t i = 0; i < total; ++i)
    EXPECT_EQ (calls[i].load (), 1) << i;
}
