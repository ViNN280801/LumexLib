// virtual_clock and the timers of its source: the order of the waits, cancel,
// a new wait over a pending one, the executor of the handler, waits started
// in the past, advance_to_next and run_until, per-operation cancellation
// (asio::cancel_after, a bound slot), destruction inside a handler, the clock
// and the io_context destroyed before their timers, no outstanding work, and
// advance from the test thread while the handlers run on another.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/LumexTransport"

namespace
{
namespace tr = lumex::applied::transport;
namespace ttime = lumex::applied::transport::time;
using ms = std::chrono::milliseconds;
using tr::testing::virtual_clock;

std::error_code
cancelled ()
{
  return make_error_code (tr::error::transport_errc::cancelled);
}

// What the handler of one wait saw.
struct wait_record
{
  int calls = 0;
  std::error_code error;
  bool on_strand = false;
  ttime::time_point_t at;
};

using strand_type = asio::strand<asio::io_context::executor_type>;

// A handler that records into `record`, on which strand it ran and the time
// of `clock` then.
struct recorder
{
  wait_record *record;
  strand_type const *strand;
  virtual_clock const *clock;

  void
  operator() (std::error_code const &error) const
  {
    ++record->calls;
    record->error = error;
    record->on_strand = strand != nullptr && strand->running_in_this_thread ();
    record->at = clock->now ();
  }
};

// Runs every ready handler of `context` (restarting it when it ran out of
// work); returns how many ran.
std::size_t
drain (asio::io_context &context)
{
  std::size_t total = 0;
  for (;;)
    {
      if (context.stopped ())
        context.restart ();
      std::size_t const ran = context.poll ();
      total += ran;
      if (ran == 0)
        return total;
    }
}

ttime::time_point_t
at_ms (long value)
{
  return ttime::time_point_t () + ms (value);
}

class LumexVirtualClockTest : public ::testing::Test
{
protected:
  asio::io_context context_;
  strand_type strand_ = asio::make_strand (context_.get_executor ());
  virtual_clock clock_;

  ttime::timer
  make_timer ()
  {
    return clock_.source ().make_timer (strand_);
  }

  recorder
  record_into (wait_record &record)
  {
    recorder made = { &record, &strand_, &clock_ };
    return made;
  }
};
} // namespace

TEST_F (LumexVirtualClockTest,
        GivenANewClock_WhenNothingAdvancesIt_ThenItStandsAtItsStart)
{
  EXPECT_EQ (clock_.now (), ttime::time_point_t ());
  EXPECT_EQ (clock_.pending (), 0u);
  EXPECT_EQ (clock_.next_expiry (), ttime::time_point_t::max ());
  virtual_clock later (at_ms (5000));
  EXPECT_EQ (later.now (), at_ms (5000));
  EXPECT_EQ (later.source ().now (), at_ms (5000));
  EXPECT_TRUE (clock_.source () == clock_.source ());
  EXPECT_TRUE (clock_.source () != later.source ());
  EXPECT_TRUE (clock_.source () != ttime::steady_timer_source ());
  std::this_thread::sleep_for (ms (5));
  EXPECT_EQ (clock_.now (), ttime::time_point_t ());
}

TEST_F (
    LumexVirtualClockTest,
    GivenWaitsStartedOutOfOrder_WhenTheClockAdvances_ThenTheyFireInTimeOrderThenInStartOrder)
{
  std::vector<std::string> order;
  ttime::timer a = make_timer ();
  ttime::timer b = make_timer ();
  ttime::timer c = make_timer ();
  ttime::timer d = make_timer ();
  a.async_wait_for (ms (300), [&order] (std::error_code const &error)
                      { order.push_back (error ? "a!" : "a"); });
  b.async_wait_for (ms (100), [&order] (std::error_code const &error)
                      { order.push_back (error ? "b!" : "b"); });
  c.async_wait_until (at_ms (200), [&order] (std::error_code const &error)
                        { order.push_back (error ? "c!" : "c"); });
  d.async_wait_for (ms (100), [&order] (std::error_code const &error)
                      { order.push_back (error ? "d!" : "d"); });
  EXPECT_EQ (clock_.pending (), 4u);
  EXPECT_EQ (clock_.next_expiry (), at_ms (100));
  EXPECT_EQ (clock_.advance (ms (1000)), 4u);
  EXPECT_EQ (clock_.now (), at_ms (1000));
  EXPECT_EQ (clock_.pending (), 0u);
  drain (context_);
  std::vector<std::string> const expected = { "b", "d", "c", "a" };
  EXPECT_EQ (order, expected);
}

TEST_F (
    LumexVirtualClockTest,
    GivenWaitsOnSeveralTimers_WhenTheClockAdvancesStepByStep_ThenEachFiresOnlyWhenItsTimeCame)
{
  wait_record first;
  wait_record second;
  ttime::timer one = make_timer ();
  ttime::timer two = make_timer ();
  one.async_wait_for (ms (100), record_into (first));
  two.async_wait_for (ms (250), record_into (second));
  EXPECT_EQ (clock_.advance (ms (99)), 0u);
  drain (context_);
  EXPECT_EQ (first.calls, 0);
  EXPECT_EQ (clock_.advance (ms (1)), 1u);
  drain (context_);
  EXPECT_EQ (first.calls, 1);
  EXPECT_FALSE (first.error);
  EXPECT_EQ (first.at, at_ms (100));
  EXPECT_EQ (second.calls, 0);
  EXPECT_EQ (clock_.advance (ms (150)), 1u);
  drain (context_);
  EXPECT_EQ (second.calls, 1);
  EXPECT_FALSE (second.error);
}

TEST_F (LumexVirtualClockTest,
        GivenAPendingWait_WhenCancelled_ThenItEndsWithCancelledAndNeverFires)
{
  wait_record record;
  ttime::timer timer = make_timer ();
  timer.async_wait_for (ms (100), record_into (record));
  EXPECT_EQ (timer.cancel (), 1u);
  EXPECT_EQ (clock_.pending (), 0u);
  // Not inside cancel ().
  EXPECT_EQ (record.calls, 0);
  drain (context_);
  EXPECT_EQ (record.calls, 1);
  EXPECT_EQ (record.error, cancelled ());
  EXPECT_EQ (timer.cancel (), 0u);
  EXPECT_EQ (clock_.advance (ms (1000)), 0u);
  drain (context_);
  EXPECT_EQ (record.calls, 1);
}

TEST_F (LumexVirtualClockTest,
        GivenAPendingWait_WhenANewWaitStarts_ThenTheFirstEndsWithCancelled)
{
  wait_record first;
  wait_record second;
  ttime::timer timer = make_timer ();
  timer.async_wait_for (ms (100), record_into (first));
  timer.async_wait_for (ms (200), record_into (second));
  EXPECT_EQ (clock_.pending (), 1u);
  drain (context_);
  EXPECT_EQ (first.calls, 1);
  EXPECT_EQ (first.error, cancelled ());
  EXPECT_EQ (second.calls, 0);
  EXPECT_EQ (clock_.advance (ms (150)), 0u);
  EXPECT_EQ (clock_.advance (ms (50)), 1u);
  drain (context_);
  EXPECT_EQ (second.calls, 1);
  EXPECT_FALSE (second.error);
  EXPECT_EQ (first.calls, 1);
}

TEST_F (
    LumexVirtualClockTest,
    GivenAStrandTimer_WhenTheClockAdvances_ThenTheHandlerRunsOnTheStrandAfterAdvanceReturned)
{
  wait_record record;
  ttime::timer timer = make_timer ();
  EXPECT_TRUE (timer.get_executor () == ttime::timer::executor_type (strand_));
  timer.async_wait_for (ms (10), record_into (record));
  EXPECT_EQ (clock_.advance (ms (10)), 1u);
  // Posted, never called inside advance ().
  EXPECT_EQ (record.calls, 0);
  drain (context_);
  EXPECT_EQ (record.calls, 1);
  EXPECT_TRUE (record.on_strand);
}

TEST_F (
    LumexVirtualClockTest,
    GivenAHandlerBoundToAnotherStrand_WhenTheWaitEnds_ThenItRunsOnThatStrand)
{
  strand_type other = asio::make_strand (context_.get_executor ());
  wait_record record;
  recorder on_other = { &record, &other, &clock_ };
  ttime::timer timer = make_timer ();
  timer.async_wait_for (ms (10), asio::bind_executor (other, on_other));
  clock_.advance (ms (10));
  drain (context_);
  EXPECT_EQ (record.calls, 1);
  EXPECT_TRUE (record.on_strand);
}

TEST_F (LumexVirtualClockTest,
        GivenAWaitStartedInThePast_WhenAdvancedByZero_ThenItFiresThenNotBefore)
{
  clock_.advance (ms (500));
  wait_record past;
  wait_record zero;
  ttime::timer one = make_timer ();
  ttime::timer two = make_timer ();
  one.async_wait_until (at_ms (100), record_into (past));
  two.async_wait_for (ms (0), record_into (zero));
  drain (context_);
  EXPECT_EQ (past.calls, 0);
  EXPECT_EQ (zero.calls, 0);
  EXPECT_EQ (clock_.pending (), 2u);
  EXPECT_EQ (clock_.advance (ms (0)), 2u);
  EXPECT_EQ (clock_.now (), at_ms (500));
  drain (context_);
  EXPECT_EQ (past.calls, 1);
  EXPECT_FALSE (past.error);
  EXPECT_EQ (zero.calls, 1);
  EXPECT_FALSE (zero.error);
}

TEST_F (LumexVirtualClockTest,
        GivenANegativeAdvance_WhenCalled_ThenTheTimeDoesNotGoBack)
{
  clock_.advance (ms (300));
  EXPECT_EQ (clock_.advance (ms (-100)), 0u);
  EXPECT_EQ (clock_.now (), at_ms (300));
}

TEST_F (
    LumexVirtualClockTest,
    GivenNothingPending_WhenAdvancedToNext_ThenItReturnsZeroAndTheTimeStays)
{
  clock_.advance (ms (42));
  EXPECT_EQ (clock_.advance_to_next (), 0u);
  EXPECT_EQ (clock_.now (), at_ms (42));
  // Also on a clock that starts before the epoch of the clock type.
  virtual_clock early (at_ms (-3600000));
  EXPECT_EQ (early.advance_to_next (), 0u);
  EXPECT_EQ (early.now (), at_ms (-3600000));
}

TEST_F (
    LumexVirtualClockTest,
    GivenPendingWaits_WhenAdvancedToNext_ThenTheTimeJumpsToTheEarliestAndOnlyItsWaitsFire)
{
  wait_record early;
  wait_record also_early;
  wait_record late;
  ttime::timer one = make_timer ();
  ttime::timer two = make_timer ();
  ttime::timer three = make_timer ();
  three.async_wait_for (ms (300), record_into (late));
  one.async_wait_for (ms (100), record_into (early));
  two.async_wait_for (ms (100), record_into (also_early));
  EXPECT_EQ (clock_.advance_to_next (), 2u);
  EXPECT_EQ (clock_.now (), at_ms (100));
  EXPECT_EQ (clock_.next_expiry (), at_ms (300));
  drain (context_);
  EXPECT_EQ (early.calls, 1);
  EXPECT_EQ (also_early.calls, 1);
  EXPECT_EQ (late.calls, 0);
  EXPECT_EQ (clock_.advance_to_next (), 1u);
  EXPECT_EQ (clock_.now (), at_ms (300));
  drain (context_);
  EXPECT_EQ (late.calls, 1);
  EXPECT_EQ (late.at, at_ms (300));
}

namespace
{
// Waits `step` again and again on `timer` until `left` waits ended.
struct chain
{
  ttime::timer *timer;
  int *left;
  ms step;

  void
  operator() (std::error_code const &error) const
  {
    if (error || --*left == 0)
      return;
    timer->async_wait_for (step, *this);
  }
};
} // namespace

TEST_F (
    LumexVirtualClockTest,
    GivenAChainOfWaits_WhenRunUntilIsCalled_ThenItEndsWhenDoneOrAtTheEndOfTheBudget)
{
  ttime::timer timer = make_timer ();
  int left = 3;
  chain step = { &timer, &left, ms (100) };
  timer.async_wait_for (ms (100), step);
  EXPECT_TRUE (tr::testing::run_until (
      context_, clock_, [&left] { return left == 0; }, ms (10000)));
  EXPECT_EQ (clock_.now (), at_ms (300));

  left = 100;
  timer.async_wait_for (ms (100), step);
  EXPECT_FALSE (tr::testing::run_until (
      context_, clock_, [] { return false; }, ms (450)));
  // Waits at 400, 500, 600 and 700 ran; the one at 800 lies beyond the
  // budget, which ends at 750.
  EXPECT_EQ (clock_.now (), at_ms (750));
  EXPECT_EQ (left, 96);
  EXPECT_EQ (clock_.next_expiry (), at_ms (800));
  EXPECT_TRUE (
      tr::testing::run_until (context_, clock_, [] { return true; }, ms (0)));
  EXPECT_EQ (clock_.now (), at_ms (750));
  timer.cancel ();
  drain (context_);
}

TEST_F (LumexVirtualClockTest,
        GivenAWaitDueNow_WhenRunUntilIsCalled_ThenItFiresWithoutMovingTheClock)
{
  wait_record record;
  ttime::timer timer = make_timer ();
  timer.async_wait_for (ms (0), record_into (record));
  EXPECT_TRUE (tr::testing::run_until (
      context_, clock_, [&record] { return record.calls == 1; }, ms (0)));
  EXPECT_EQ (clock_.now (), ttime::time_point_t ());
}

TEST_F (
    LumexVirtualClockTest,
    GivenAWaitUnderCancelAfter_WhenTheRealDeadlinePasses_ThenOnlyThatWaitEndsWithCancelled)
{
  wait_record cut;
  wait_record kept;
  ttime::timer one = make_timer ();
  ttime::timer two = make_timer ();
  recorder record_cut = record_into (cut);
  // Started in the strand: Asio's rule for cancel_after.
  asio::post (strand_,
              [&one, record_cut] ()
                {
                  one.async_wait_for (
                      ms (10000), asio::cancel_after (ms (20), record_cut));
                });
  two.async_wait_for (ms (10000), record_into (kept));
  // The real timer of cancel_after keeps the context running until it fires.
  context_.run ();
  EXPECT_EQ (cut.calls, 1);
  EXPECT_EQ (cut.error, cancelled ());
  EXPECT_EQ (kept.calls, 0);
  EXPECT_EQ (clock_.pending (), 1u);
  EXPECT_EQ (clock_.now (), ttime::time_point_t ());
  EXPECT_EQ (clock_.advance (ms (10000)), 1u);
  drain (context_);
  EXPECT_EQ (kept.calls, 1);
  EXPECT_FALSE (kept.error);
  EXPECT_EQ (cut.calls, 1);
}

TEST_F (
    LumexVirtualClockTest,
    GivenABoundCancellationSlot_WhenEachCancellationTypeIsEmitted_ThenTheWaitEndsWithCancelled)
{
  asio::cancellation_type const types[]
      = { asio::cancellation_type::terminal, asio::cancellation_type::partial,
          asio::cancellation_type::total };
  for (std::size_t i = 0; i < 3; ++i)
    {
      asio::cancellation_signal signal;
      wait_record record;
      ttime::timer timer = make_timer ();
      timer.async_wait_for (
          ms (100),
          asio::bind_cancellation_slot (signal.slot (), record_into (record)));
      // Nothing runs the strand meanwhile: the emit is on it.
      signal.emit (types[i]);
      EXPECT_EQ (clock_.pending (), 0u) << i;
      EXPECT_EQ (record.calls, 0) << i;
      drain (context_);
      EXPECT_EQ (record.calls, 1) << i;
      EXPECT_EQ (record.error, cancelled ()) << i;
      // After the wait ended a signal reaches nothing.
      signal.emit (asio::cancellation_type::terminal);
      drain (context_);
      EXPECT_EQ (record.calls, 1) << i;
    }
}

TEST_F (
    LumexVirtualClockTest,
    GivenASlotOfAnEndedWait_WhenTheTimerWaitsAgainWithoutItAndTheSignalIsEmitted_ThenTheNewWaitStays)
{
  asio::cancellation_signal signal;
  wait_record first;
  wait_record second;
  ttime::timer timer = make_timer ();
  timer.async_wait_for (ms (10), asio::bind_cancellation_slot (
                                     signal.slot (), record_into (first)));
  clock_.advance (ms (10));
  drain (context_);
  EXPECT_EQ (first.calls, 1);
  timer.async_wait_for (ms (10), record_into (second));
  signal.emit (asio::cancellation_type::terminal);
  EXPECT_EQ (clock_.pending (), 1u);
  clock_.advance (ms (10));
  drain (context_);
  EXPECT_EQ (second.calls, 1);
  EXPECT_FALSE (second.error);
}

namespace
{
// The first wait's handler starts a second wait on the same timer, then
// destroys the timer.
struct rearm_then_destroy
{
  std::unique_ptr<ttime::timer> *owner;
  wait_record *first;
  wait_record *second;

  void
  operator() (std::error_code const &error) const
  {
    ++first->calls;
    first->error = error;
    wait_record *const record = second;
    (*owner)->async_wait_for (ms (50),
                              [record] (std::error_code const &e)
                                {
                                  ++record->calls;
                                  record->error = e;
                                });
    owner->reset ();
  }
};
} // namespace

TEST_F (
    LumexVirtualClockTest,
    GivenATimer_WhenItIsDestroyedInsideItsOwnHandler_ThenTheWaitStartedThereEndsWithCancelled)
{
  std::unique_ptr<ttime::timer> timer (new ttime::timer (make_timer ()));
  std::weak_ptr<ttime::timer_impl> const impl = timer->impl ();
  wait_record first;
  wait_record second;
  rearm_then_destroy handler = { &timer, &first, &second };
  timer->async_wait_for (ms (10), handler);
  clock_.advance (ms (10));
  drain (context_);
  EXPECT_EQ (first.calls, 1);
  EXPECT_FALSE (first.error);
  EXPECT_EQ (timer.get (), nullptr);
  EXPECT_EQ (second.calls, 1);
  EXPECT_EQ (second.error, cancelled ());
  EXPECT_EQ (clock_.pending (), 0u);
  EXPECT_TRUE (impl.expired ());
  EXPECT_EQ (clock_.advance (ms (1000)), 0u);
}

TEST_F (LumexVirtualClockTest,
        GivenAPendingWait_WhenTheTimerIsDestroyed_ThenTheWaitEndsWithCancelled)
{
  wait_record record;
  {
    ttime::timer timer = make_timer ();
    timer.async_wait_for (ms (100), record_into (record));
  }
  EXPECT_EQ (clock_.pending (), 0u);
  EXPECT_EQ (record.calls, 0);
  drain (context_);
  EXPECT_EQ (record.calls, 1);
  EXPECT_EQ (record.error, cancelled ());
}

TEST_F (
    LumexVirtualClockTest,
    GivenAMovedTimer_WhenTheMovedFromOneIsUsed_ThenItIsEmptyAndTheWaitStaysWithTheNewOwner)
{
  wait_record moved;
  ttime::timer first = make_timer ();
  first.async_wait_for (ms (100), record_into (moved));
  ttime::timer second (std::move (first));
  EXPECT_EQ (first.impl (), nullptr);
  EXPECT_EQ (first.cancel (), 0u);
  // The system executor runs the answer of an empty timer.
  std::promise<std::error_code> empty;
  first.async_wait_for (ms (100), [&empty] (std::error_code const &error)
                          { empty.set_value (error); });
  EXPECT_EQ (clock_.pending (), 1u);
  std::future<std::error_code> answer = empty.get_future ();
  ASSERT_EQ (answer.wait_for (std::chrono::seconds (10)),
             std::future_status::ready);
  EXPECT_EQ (answer.get (), cancelled ());
  EXPECT_EQ (clock_.advance (ms (100)), 1u);
  drain (context_);
  EXPECT_EQ (moved.calls, 1);
  EXPECT_FALSE (moved.error);
  // Assigning over a timer ends its pending wait.
  wait_record replaced;
  ttime::timer third = make_timer ();
  third.async_wait_for (ms (100), record_into (replaced));
  third = make_timer ();
  drain (context_);
  EXPECT_EQ (replaced.calls, 1);
  EXPECT_EQ (replaced.error, cancelled ());
}

TEST (
    LumexVirtualClockLifetimeTest,
    GivenPendingWaits_WhenTheClockIsDestroyed_ThenTheyEndWithCancelledAndSoDoLaterWaits)
{
  asio::io_context context;
  std::unique_ptr<virtual_clock> clock (new virtual_clock (at_ms (70)));
  ttime::timer_source const source = clock->source ();
  ttime::timer timer = source.make_timer (context.get_executor ());
  std::error_code first;
  int first_calls = 0;
  timer.async_wait_for (ms (100),
                        [&] (std::error_code const &error)
                          {
                            ++first_calls;
                            first = error;
                          });
  clock.reset ();
  drain (context);
  EXPECT_EQ (first_calls, 1);
  EXPECT_EQ (first, cancelled ());
  EXPECT_EQ (source.now (), at_ms (70));
  EXPECT_EQ (timer.now (), at_ms (70));

  std::error_code later;
  int later_calls = 0;
  timer.async_wait_for (ms (100),
                        [&] (std::error_code const &error)
                          {
                            ++later_calls;
                            later = error;
                          });
  EXPECT_EQ (later_calls, 0);
  drain (context);
  EXPECT_EQ (later_calls, 1);
  EXPECT_EQ (later, cancelled ());

  // A timer made after the clock is gone answers the same way.
  ttime::timer made_late = source.make_timer (context.get_executor ());
  int late_calls = 0;
  made_late.async_wait_for (ms (1), [&late_calls] (std::error_code const &)
                              { ++late_calls; });
  drain (context);
  EXPECT_EQ (late_calls, 1);
}

TEST (
    LumexVirtualClockLifetimeTest,
    GivenAPendingWaitOnAStrand_WhenTheIoContextIsDestroyedFirst_ThenTheHandlerDiesUncalledAndTheTimerOutlivesIt)
{
  virtual_clock clock;
  std::unique_ptr<asio::io_context> context (new asio::io_context ());
  ttime::timer timer = clock.source ().make_timer (
      asio::make_strand (context->get_executor ()));
  std::shared_ptr<int> token = std::make_shared<int> (1);
  std::weak_ptr<int> const watched = token;
  int calls = 0;
  timer.async_wait_for (ms (100), [token, &calls] (std::error_code const &)
                          { ++calls; });
  token.reset ();
  EXPECT_FALSE (watched.expired ());
  context.reset ();
  EXPECT_TRUE (watched.expired ());
  EXPECT_EQ (calls, 0);
  EXPECT_EQ (clock.pending (), 0u);
  EXPECT_FALSE (timer.get_executor ());

  // A wait started now is dropped the same way.
  std::shared_ptr<int> second = std::make_shared<int> (2);
  std::weak_ptr<int> const second_watched = second;
  timer.async_wait_for (ms (100), [second, &calls] (std::error_code const &)
                          { ++calls; });
  second.reset ();
  EXPECT_TRUE (second_watched.expired ());
  EXPECT_EQ (clock.pending (), 0u);
  EXPECT_EQ (timer.cancel (), 0u);
  EXPECT_EQ (clock.advance (ms (1000)), 0u);
  EXPECT_EQ (calls, 0);
}

TEST (LumexVirtualClockWorkTest,
      GivenAPendingWait_WhenNothingElseIsPending_ThenTheContextsRunOutOfWork)
{
  asio::io_context context;
  asio::io_context other;
  virtual_clock clock;
  ttime::timer timer = clock.source ().make_timer (context.get_executor ());
  int calls = 0;
  timer.async_wait_for (ms (100),
                        asio::bind_executor (other.get_executor (),
                                             [&calls] (std::error_code const &)
                                               { ++calls; }));
  // Neither the timer's context nor the handler's holds work for the wait.
  EXPECT_EQ (context.run (), 0u);
  EXPECT_EQ (other.poll (), 0u);
  EXPECT_TRUE (other.stopped ());
  EXPECT_EQ (clock.pending (), 1u);
  clock.advance (ms (100));
  context.restart ();
  context.run ();
  other.restart ();
  other.run ();
  EXPECT_EQ (calls, 1);
}

namespace
{
// One timer of the stress test: waits 1 ms again and again on its strand
// until `rounds` waits ended, and checks the times never go back.
struct stress_timer
{
  std::shared_ptr<ttime::timer> timer;
  std::shared_ptr<std::atomic<int>> fired;
  std::shared_ptr<std::atomic<bool>> back_in_time;
  ttime::time_point_t last;
  int rounds;

  void
  operator() (std::error_code const &error)
  {
    if (error)
      return;
    ttime::time_point_t const seen = timer->now ();
    if (seen < last)
      back_in_time->store (true);
    last = seen;
    if (fired->fetch_add (1) + 1 >= rounds)
      return;
    timer->async_wait_for (ms (1), *this);
  }
};
} // namespace

TEST (
    LumexVirtualClockThreadTest,
    GivenHandlersOnRunnerThreads_WhenTheTestThreadAdvances_ThenEveryWaitFiresOnceAndInOrder)
{
  virtual_clock clock;
  int const timers = 8;
  int const rounds = 200;
  std::vector<std::shared_ptr<std::atomic<int>>> fired;
  std::shared_ptr<std::atomic<bool>> const back_in_time
      = std::make_shared<std::atomic<bool>> (false);
  std::vector<std::shared_ptr<ttime::timer>> made;
  {
    tr::stream::io_runner runner (2);
    for (int i = 0; i < timers; ++i)
      {
        std::shared_ptr<ttime::timer> const timer
            = std::make_shared<ttime::timer> (clock.source ().make_timer (
                asio::make_strand (runner.get_executor ())));
        made.push_back (timer);
        fired.push_back (std::make_shared<std::atomic<int>> (0));
        stress_timer step
            = { timer, fired.back (), back_in_time, clock.now (), rounds };
        asio::post (timer->get_executor (), [timer, step] () mutable
                      { timer->async_wait_for (ms (1), step); });
      }
    std::chrono::steady_clock::time_point const end
        = std::chrono::steady_clock::now () + std::chrono::seconds (60);
    for (;;)
      {
        int done = 0;
        for (int i = 0; i < timers; ++i)
          done += fired[static_cast<std::size_t> (i)]->load () >= rounds;
        if (done == timers || std::chrono::steady_clock::now () > end)
          break;
        clock.advance (ms (1));
      }
    // The timers die on their strands before the runner stops.
    for (int i = 0; i < timers; ++i)
      {
        std::shared_ptr<ttime::timer> const timer
            = made[static_cast<std::size_t> (i)];
        std::promise<void> gone;
        asio::post (timer->get_executor (),
                    [timer, &gone] ()
                      {
                        timer->cancel ();
                        gone.set_value ();
                      });
        gone.get_future ().wait ();
      }
    runner.stop ();
  }
  for (int i = 0; i < timers; ++i)
    EXPECT_EQ (fired[static_cast<std::size_t> (i)]->load (), rounds) << i;
  EXPECT_FALSE (back_in_time->load ());
}
