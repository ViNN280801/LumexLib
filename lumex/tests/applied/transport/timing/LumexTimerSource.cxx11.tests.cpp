// The steady timer source and the timer front: the time, a wait that ends
// after its delay, cancel and a new wait over a pending one (the transport's
// cancelled, not Asio's code), asio::cancel_after and a bound slot, the
// executor of the handler, no outstanding work on it, destruction with a
// pending wait and inside the handler, an empty timer, a deferred wait and
// saturating_add, and a signal after the timer was destroyed.

#include <chrono>
#include <cstddef>
#include <future>
#include <memory>
#include <system_error>
#include <thread>

#include <gtest/gtest.h>

#include "lumex/applied/transport/LumexTransport"

namespace
{
namespace tr = lumex::applied::transport;
namespace timing = lumex::applied::transport::timing;
using ms = std::chrono::milliseconds;
using steady = std::chrono::steady_clock;
using strand_type = asio::strand<tr::executor_type>;

std::error_code
cancelled ()
{
  return make_error_code (tr::error::transport_errc::cancelled);
}

// The result of one wait, set from the runner's thread.
struct wait_result
{
  std::promise<std::error_code> error;
  std::promise<bool> on_strand;
};

class LumexTimerSourceTest : public ::testing::Test
{
protected:
  tr::stream::io_runner runner_{ 1 };
  strand_type strand_ = asio::make_strand (runner_.get_executor ());
  timing::timer_source source_ = timing::steady_timer_source ();

  // Waits for `future` at most 10 s.
  template <typename T>
  static T
  get (std::future<T> &future)
  {
    EXPECT_EQ (future.wait_for (std::chrono::seconds (10)),
               std::future_status::ready);
    return future.get ();
  }

  // Waits until the strand ran everything posted before: the completion
  // that set a future may still be unwinding there (it destroys its copy of
  // the strand executor). ThreadSanitizer does not see the fence of Asio's
  // reference count, so a timer destroyed on this thread meanwhile is
  // reported as a race.
  void
  quiesce ()
  {
    on_strand ([] () {});
  }

  // Runs `step` on the strand and waits for it.
  template <typename Step>
  void
  on_strand (Step step)
  {
    std::promise<void> done;
    asio::post (strand_,
                [&step, &done] ()
                  {
                    step ();
                    done.set_value ();
                  });
    done.get_future ().wait ();
  }
};
} // namespace

TEST_F (LumexTimerSourceTest,
        GivenTheSteadySource_WhenAskedForTheTime_ThenItReadsTheSteadyClock)
{
  steady::time_point const before = steady::now ();
  timing::time_point_t const read = source_.now ();
  steady::time_point const after = steady::now ();
  // The same time, on the seam's own clock type.
  EXPECT_LE (before.time_since_epoch (), read.time_since_epoch ());
  EXPECT_LE (read.time_since_epoch (), after.time_since_epoch ());
  EXPECT_TRUE (timing::timer_clock::is_steady);
  EXPECT_TRUE (timing::timer_source () == source_);
  EXPECT_TRUE (timing::timer_source (nullptr) == source_);
  EXPECT_TRUE (timing::steady_timer_source () == source_);
  timing::timer timer = source_.make_timer (strand_);
  EXPECT_LE (after.time_since_epoch (), timer.now ().time_since_epoch ());
  EXPECT_TRUE (timer.get_executor () == tr::executor_type (strand_));
  quiesce ();
}

TEST_F (LumexTimerSourceTest,
        GivenASteadyWait_WhenItsTimeComes_ThenItEndsWithSuccessAfterTheDelay)
{
  timing::timer timer = source_.make_timer (strand_);
  wait_result result;
  steady::time_point const start = steady::now ();
  timer.async_wait_for (ms (40),
                        [this, &result] (std::error_code const &error)
                          {
                            result.on_strand.set_value (
                                strand_.running_in_this_thread ());
                            result.error.set_value (error);
                          });
  std::future<std::error_code> error = result.error.get_future ();
  std::future<bool> where = result.on_strand.get_future ();
  EXPECT_FALSE (get (error));
  EXPECT_GE (steady::now () - start, ms (40));
  EXPECT_TRUE (get (where));
  quiesce ();
}

TEST_F (LumexTimerSourceTest,
        GivenASteadyWaitUntilAPastTime_WhenStarted_ThenItEndsSoonWithSuccess)
{
  timing::timer timer = source_.make_timer (strand_);
  std::promise<std::error_code> result;
  timer.async_wait_until (timer.now () - ms (100),
                          [&result] (std::error_code const &error)
                            { result.set_value (error); });
  std::future<std::error_code> error = result.get_future ();
  EXPECT_FALSE (get (error));
  quiesce ();
}

TEST_F (
    LumexTimerSourceTest,
    GivenASteadyWait_WhenCancelled_ThenItEndsWithTheTransportsCancelledNotAsiosCode)
{
  timing::timer timer = source_.make_timer (strand_);
  std::promise<std::error_code> result;
  std::size_t ended = 0;
  on_strand (
      [&] ()
        {
          timer.async_wait_for (ms (60000),
                                [&result] (std::error_code const &error)
                                  { result.set_value (error); });
          ended = timer.cancel ();
        });
  EXPECT_EQ (ended, 1u);
  std::future<std::error_code> error = result.get_future ();
  std::error_code const seen = get (error);
  EXPECT_EQ (seen, cancelled ());
  EXPECT_NE (seen, std::error_code (asio::error::operation_aborted));
  std::size_t again = 1;
  on_strand ([&] () { again = timer.cancel (); });
  EXPECT_EQ (again, 0u);
  quiesce ();
}

TEST_F (
    LumexTimerSourceTest,
    GivenAPendingSteadyWait_WhenANewWaitStarts_ThenTheFirstEndsWithCancelled)
{
  timing::timer timer = source_.make_timer (strand_);
  std::promise<std::error_code> first;
  std::promise<std::error_code> second;
  on_strand (
      [&] ()
        {
          timer.async_wait_for (ms (60000),
                                [&first] (std::error_code const &error)
                                  { first.set_value (error); });
          timer.async_wait_for (ms (10),
                                [&second] (std::error_code const &error)
                                  { second.set_value (error); });
        });
  std::future<std::error_code> one = first.get_future ();
  std::future<std::error_code> two = second.get_future ();
  EXPECT_EQ (get (one), cancelled ());
  EXPECT_FALSE (get (two));
  quiesce ();
}

TEST_F (
    LumexTimerSourceTest,
    GivenASteadyWaitUnderCancelAfter_WhenTheDeadlinePasses_ThenItEndsWithCancelled)
{
  timing::timer timer = source_.make_timer (strand_);
  std::promise<std::error_code> result;
  // Started in the strand: Asio's rule for cancel_after.
  on_strand (
      [&] ()
        {
          timer.async_wait_for (
              ms (60000), asio::cancel_after (
                              ms (20), [&result] (std::error_code const &error)
                                { result.set_value (error); }));
        });
  std::future<std::error_code> error = result.get_future ();
  EXPECT_EQ (get (error), cancelled ());
  quiesce ();
}

TEST_F (
    LumexTimerSourceTest,
    GivenASteadyWaitWithABoundSlot_WhenATerminalSignalIsEmitted_ThenItEndsWithCancelled)
{
  timing::timer timer = source_.make_timer (strand_);
  asio::cancellation_signal signal;
  std::promise<std::error_code> result;
  on_strand (
      [&] ()
        {
          timer.async_wait_for (ms (60000),
                                asio::bind_cancellation_slot (
                                    signal.slot (),
                                    [&result] (std::error_code const &error)
                                      { result.set_value (error); }));
        });
  on_strand ([&signal] ()
               { signal.emit (asio::cancellation_type::terminal); });
  std::future<std::error_code> error = result.get_future ();
  EXPECT_EQ (get (error), cancelled ());
  quiesce ();
}

TEST_F (LumexTimerSourceTest,
        GivenAHandlerBoundToAnotherStrand_WhenTheWaitEnds_ThenItRunsThere)
{
  strand_type other = asio::make_strand (runner_.get_executor ());
  timing::timer timer = source_.make_timer (strand_);
  std::promise<bool> where;
  timer.async_wait_for (
      ms (5), asio::bind_executor (
                  other, [&other, &where] (std::error_code const &)
                    { where.set_value (other.running_in_this_thread ()); }));
  std::future<bool> seen = where.get_future ();
  EXPECT_TRUE (get (seen));
  quiesce ();
}

TEST_F (
    LumexTimerSourceTest,
    GivenAHandlerBoundToAnotherContext_WhileTheSteadyWaitIsPending_ThenThatContextHoldsNoWork)
{
  asio::io_context other;
  timing::timer timer = source_.make_timer (strand_);
  std::promise<std::error_code> result;
  on_strand (
      [&] ()
        {
          timer.async_wait_for (
              ms (60000),
              asio::bind_executor (other.get_executor (),
                                   [&result] (std::error_code const &error)
                                     { result.set_value (error); }));
        });
  EXPECT_EQ (other.poll (), 0u);
  EXPECT_TRUE (other.stopped ());
  on_strand ([&timer] () { timer.cancel (); });
  // The cancelled handler is posted to `other` only now.
  std::future<std::error_code> error = result.get_future ();
  for (int i = 0; i < 10000; ++i)
    {
      other.restart ();
      other.poll ();
      if (error.wait_for (ms (0)) == std::future_status::ready)
        break;
      std::this_thread::sleep_for (ms (1));
    }
  EXPECT_EQ (get (error), cancelled ());
  quiesce ();
}

TEST_F (
    LumexTimerSourceTest,
    GivenAPendingSteadyWait_WhenTheTimerIsDestroyed_ThenTheWaitEndsWithCancelled)
{
  std::promise<std::error_code> result;
  on_strand (
      [&] ()
        {
          timing::timer timer = source_.make_timer (strand_);
          timer.async_wait_for (ms (60000),
                                [&result] (std::error_code const &error)
                                  { result.set_value (error); });
        });
  std::future<std::error_code> error = result.get_future ();
  EXPECT_EQ (get (error), cancelled ());
  quiesce ();
}

TEST_F (
    LumexTimerSourceTest,
    GivenASteadyTimer_WhenItIsDestroyedInsideItsOwnHandler_ThenTheWaitStartedThereEndsWithCancelled)
{
  std::shared_ptr<std::unique_ptr<timing::timer>> const owner
      = std::make_shared<std::unique_ptr<timing::timer>> (
          new timing::timer (source_.make_timer (strand_)));
  std::promise<std::error_code> second;
  (*owner)->async_wait_for (ms (1),
                            [owner, &second] (std::error_code const &)
                              {
                                (*owner)->async_wait_for (
                                    ms (60000),
                                    [&second] (std::error_code const &error)
                                      { second.set_value (error); });
                                owner->reset ();
                              });
  std::future<std::error_code> error = second.get_future ();
  EXPECT_EQ (get (error), cancelled ());
  quiesce ();
}

TEST_F (LumexTimerSourceTest,
        GivenASteadyWait_WhenItStarts_ThenItsHandlerIsNotCalledInsideTheCall)
{
  timing::timer timer = source_.make_timer (strand_);
  bool inside = true;
  bool called_inside = false;
  std::promise<void> done;
  on_strand (
      [&] ()
        {
          timer.async_wait_until (timing::time_point_t (),
                                  [&] (std::error_code const &)
                                    {
                                      called_inside = inside;
                                      done.set_value ();
                                    });
          timer.cancel ();
          inside = false;
        });
  done.get_future ().wait ();
  EXPECT_FALSE (called_inside);
  quiesce ();
}

TEST_F (LumexTimerSourceTest,
        GivenADeferredWait_WhenLaunchedLater_ThenItWaitsTheDelayFromTheLaunch)
{
  timing::timer timer = source_.make_timer (strand_);
  auto deferred = timer.async_wait_for (ms (50), asio::deferred);
  std::this_thread::sleep_for (ms (60));
  std::promise<std::error_code> result;
  steady::time_point const launched = steady::now ();
  std::move (deferred) ([&result] (std::error_code const &error)
                          { result.set_value (error); });
  std::future<std::error_code> error = result.get_future ();
  EXPECT_FALSE (get (error));
  EXPECT_GE (steady::now () - launched, ms (50));
  quiesce ();
}

TEST (
    LumexTimerFrontTest,
    GivenAnEmptyTimer_WhenAWaitStarts_ThenItEndsWithCancelledAndCancelEndsNothing)
{
  timing::timer timer;
  EXPECT_EQ (timer.impl (), nullptr);
  EXPECT_FALSE (timer.get_executor ());
  EXPECT_EQ (timer.cancel (), 0u);
  timing::time_point_t const before = timing::timer_clock::now ();
  EXPECT_LE (before, timer.now ());
  std::promise<std::error_code> result;
  timer.async_wait_for (ms (10), [&result] (std::error_code const &error)
                          { result.set_value (error); });
  std::future<std::error_code> error = result.get_future ();
  ASSERT_EQ (error.wait_for (std::chrono::seconds (10)),
             std::future_status::ready);
  EXPECT_EQ (error.get (), cancelled ());
}

TEST (LumexTimerFrontTest,
      GivenSaturatingAdd_WhenTheSumWouldLeaveTheRange_ThenItStopsAtTheEnds)
{
  timing::time_point_t const zero;
  EXPECT_EQ (tr::detail::saturating_add (zero, ms (5)), zero + ms (5));
  EXPECT_EQ (tr::detail::saturating_add (zero + ms (5), ms (-10)),
             zero + ms (5));
  EXPECT_EQ (tr::detail::saturating_add (zero, ms::max ()),
             timing::time_point_t::max ());
  EXPECT_EQ (tr::detail::saturating_add (timing::time_point_t::max () - ms (1),
                                         ms (2)),
             timing::time_point_t::max ());
  EXPECT_EQ (tr::detail::saturating_add (timing::time_point_t::max (), ms (0)),
             timing::time_point_t::max ());

  // Before the epoch of the clock (a virtual clock may start there).
  timing::time_point_t const early = zero - std::chrono::seconds (5);
  EXPECT_EQ (tr::detail::saturating_add (early, ms (100)), early + ms (100));
  EXPECT_EQ (tr::detail::saturating_add (early, ms (5000)), zero);
  EXPECT_EQ (tr::detail::saturating_add (early, ms::max ()),
             timing::time_point_t::max ());
  timing::time_point_t const earliest = timing::time_point_t::min ();
  EXPECT_EQ (tr::detail::saturating_add (earliest, ms (1)), earliest + ms (1));
}

TEST (
    LumexTimerFrontTest,
    GivenASteadyTimerDestroyedWithAWaitOnABoundSlot_WhenTheSignalComesBeforeTheCompletion_ThenItReachesNothing)
{
  asio::io_context context;
  asio::cancellation_signal signal;
  strand_type strand = asio::make_strand (context.get_executor ());
  int calls = 0;
  std::error_code seen;
  {
    timing::timer timer = timing::steady_timer_source ().make_timer (strand);
    asio::post (strand,
                [&] ()
                  {
                    timer.async_wait_for (ms (60000),
                                          asio::bind_cancellation_slot (
                                              signal.slot (),
                                              [&] (std::error_code const &e)
                                                {
                                                  ++calls;
                                                  seen = e;
                                                }));
                  });
    context.run_one ();
    context.restart ();
  }
  // The timer is gone, its cancelled completion not run yet: nothing runs
  // the strand, so these signals are emitted on it.
  signal.emit (asio::cancellation_type::terminal);
  signal.emit (asio::cancellation_type::all);
  context.run ();
  EXPECT_EQ (calls, 1);
  EXPECT_EQ (seen, cancelled ());
  // And after the handler ran.
  signal.emit (asio::cancellation_type::terminal);
  EXPECT_EQ (calls, 1);
}
