/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/**
 * @file LumexTestThreads.hpp
 * @brief Thread helpers of the concurrency tests: a start barrier, a gate,
 * a stall point, a runner that starts N threads together, a watchdog and a
 * thread-safe verdict.
 * @details Everything is C++11 and uses a mutex plus a condition variable,
 * never a spin on a shared flag, so the helpers behave on a machine with
 * fewer cores than threads. Each helper is usable inside a deleter or an
 * allocator hook of the code under test, which is how a test stalls a thread
 * at a precise moment without touching the library.
 */
#ifndef LUMEX_TESTS_SUPPORT_TEST_THREADS_HPP
#define LUMEX_TESTS_SUPPORT_TEST_THREADS_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "lumex/tests/support/LumexTestConfig.hpp"

namespace lumex_test
{
/**
 * @brief Reusable barrier: `arrive_and_wait ()` returns when @p count
 * threads have arrived, then the barrier is ready for the next round.
 */
class StartBarrier
{
public:
  explicit StartBarrier (int count)
      : count_ (count), waiting_ (0), generation_ (0)
  {
  }

  StartBarrier (StartBarrier const &) = delete;
  StartBarrier &operator= (StartBarrier const &) = delete;

  /// Blocks until `count` threads have called it.
  void
  arrive_and_wait ()
  {
    std::unique_lock<std::mutex> guard (mutex_);
    std::uint64_t const generation = generation_;
    if (++waiting_ == count_)
      {
        waiting_ = 0;
        ++generation_;
        condition_.notify_all ();
        return;
      }
    condition_.wait (guard, [&] { return generation_ != generation; });
  }

private:
  int const count_;
  int waiting_;
  std::uint64_t generation_;
  std::mutex mutex_;
  std::condition_variable condition_;
};

/// A manual-reset event: `wait ()` blocks until some thread calls `open ()`.
class Gate
{
public:
  Gate () : open_ (false) {}

  Gate (Gate const &) = delete;
  Gate &operator= (Gate const &) = delete;

  /// Opens the gate and wakes every waiter; stays open.
  void
  open ()
  {
    {
      std::lock_guard<std::mutex> guard (mutex_);
      open_ = true;
    }
    condition_.notify_all ();
  }

  /// Closes the gate again.
  void
  close ()
  {
    std::lock_guard<std::mutex> guard (mutex_);
    open_ = false;
  }

  /// Blocks until the gate is open.
  void
  wait ()
  {
    std::unique_lock<std::mutex> guard (mutex_);
    condition_.wait (guard, [this] { return open_; });
  }

  /// Waits at most @p ms milliseconds; true when the gate is open.
  bool
  wait_for_ms (int ms)
  {
    std::unique_lock<std::mutex> guard (mutex_);
    return condition_.wait_for (guard, std::chrono::milliseconds (ms),
                                [this] { return open_; });
  }

  /// True while the gate is open.
  bool
  is_open ()
  {
    std::lock_guard<std::mutex> guard (mutex_);
    return open_;
  }

private:
  bool open_;
  std::mutex mutex_;
  std::condition_variable condition_;
};

/**
 * @brief A place where a thread under test is parked until the test thread
 * lets it go.
 * @details The parked thread calls `park ()`, usually from a deleter, an
 * allocator hook or a copy constructor of a test type. The test thread calls
 * `wait_until_parked (n)` to know that @p n threads are inside, does
 * whatever must happen "while the other thread is stopped there", then
 * `release ()`. `arm (false)` makes `park ()` return at once, so a hook can
 * stay installed. A park that is never released ends at the timeout (the
 * test then reports it) instead of hanging the binary.
 */
class StallPoint
{
public:
  StallPoint () : armed_ (false), released_ (false), parked_ (0), passed_ (0)
  {
  }

  StallPoint (StallPoint const &) = delete;
  StallPoint &operator= (StallPoint const &) = delete;

  /// Enables or disables parking; enabling also closes the release.
  void
  arm (bool on)
  {
    std::lock_guard<std::mutex> guard (mutex_);
    armed_ = on;
    if (on)
      released_ = false;
  }

  /// Parks the caller while armed and not released. Returns false on timeout.
  bool
  park (int timeout_ms = 20000)
  {
    std::unique_lock<std::mutex> guard (mutex_);
    if (!armed_)
      return true;
    ++parked_;
    changed_.notify_all ();
    bool const ok
        = changed_.wait_for (guard, std::chrono::milliseconds (timeout_ms),
                             [this] { return released_ || !armed_; });
    --parked_;
    ++passed_;
    changed_.notify_all ();
    return ok;
  }

  /// Blocks until @p count threads are parked; false on timeout.
  bool
  wait_until_parked (int count, int timeout_ms = 20000)
  {
    std::unique_lock<std::mutex> guard (mutex_);
    return changed_.wait_for (guard, std::chrono::milliseconds (timeout_ms),
                              [&] { return parked_ >= count; });
  }

  /// Lets every parked thread go (and every later one while released).
  void
  release ()
  {
    {
      std::lock_guard<std::mutex> guard (mutex_);
      released_ = true;
    }
    changed_.notify_all ();
  }

  /// Blocks until @p count threads have left `park ()`; false on timeout.
  bool
  wait_until_passed (int count, int timeout_ms = 20000)
  {
    std::unique_lock<std::mutex> guard (mutex_);
    return changed_.wait_for (guard, std::chrono::milliseconds (timeout_ms),
                              [&] { return passed_ >= count; });
  }

  /// Threads parked right now.
  int
  parked ()
  {
    std::lock_guard<std::mutex> guard (mutex_);
    return parked_;
  }

private:
  bool armed_;
  bool released_;
  int parked_;
  int passed_;
  std::mutex mutex_;
  std::condition_variable changed_;
};

/**
 * @brief A meeting point that gives up: `meet (ms)` waits for the other
 * parties for at most @p ms milliseconds.
 * @details Used inside a hook of a fixture that may or may not be called by
 * the code under test: a thread that reaches it waits for the partners, and
 * the wait ends when all have arrived or the time is up, so a hook that only
 * one thread reaches costs a bounded delay and never a hang. `reset ()`
 * prepares the next round (call it while no thread is inside).
 */
class Rendezvous
{
public:
  explicit Rendezvous (int parties) : parties_ (parties), arrived_ (0) {}

  Rendezvous (Rendezvous const &) = delete;
  Rendezvous &operator= (Rendezvous const &) = delete;

  /// Arrives and waits for the others; true when all parties arrived.
  bool
  meet (int milliseconds)
  {
    std::unique_lock<std::mutex> guard (mutex_);
    ++arrived_;
    condition_.notify_all ();
    return condition_.wait_for (guard,
                                std::chrono::milliseconds (milliseconds),
                                [this] { return arrived_ >= parties_; });
  }

  /// Forgets the arrivals of the last round.
  void
  reset ()
  {
    std::lock_guard<std::mutex> guard (mutex_);
    arrived_ = 0;
  }

  /// Parties that have arrived in this round.
  int
  arrived ()
  {
    std::lock_guard<std::mutex> guard (mutex_);
    return arrived_;
  }

private:
  int const parties_;
  int arrived_;
  std::mutex mutex_;
  std::condition_variable condition_;
};

/**
 * @brief Runs @p body on @p count threads that start together.
 * @details Every thread calls `body (index)` after all of them reached the
 * start barrier. An exception that leaves `body` is caught (an uncaught one
 * would call `std::terminate`) and its message is returned; an empty string
 * means every body returned normally.
 */
inline std::string
run_threads (int count, std::function<void (int)> const &body)
{
  StartBarrier start (count);
  std::mutex error_mutex;
  std::string error;
  std::vector<std::thread> threads;
  threads.reserve (static_cast<std::size_t> (count));
  for (int i = 0; i < count; ++i)
    threads.push_back (std::thread (
        [&, i]
          {
            start.arrive_and_wait ();
            try
              {
                body (i);
              }
            catch (std::exception const &e)
              {
                std::lock_guard<std::mutex> guard (error_mutex);
                if (error.empty ())
                  error = std::string ("exception in thread ")
                          + std::to_string (i) + ": " + e.what ();
              }
            catch (...)
              {
                std::lock_guard<std::mutex> guard (error_mutex);
                if (error.empty ())
                  error = std::string ("unknown exception in thread ")
                          + std::to_string (i);
              }
          }));
  for (std::size_t i = 0; i < threads.size (); ++i)
    threads[i].join ();
  return error;
}

/**
 * @brief Aborts the process with a message if it is not destroyed in time.
 * @details A lost wake-up or a deadlock would hang the test binary; the
 * watchdog turns that into a failure with a clear message, also outside
 * CTest. The limit is `LUMEX_TEST_WATCHDOG_SECONDS` (default 120).
 */
class TestWatchdog
{
public:
  explicit TestWatchdog (char const *what)
      : what_ (what), seconds_ (watchdog_limit_seconds ()), done_ (false)
  {
    thread_ = std::thread (&TestWatchdog::run, this);
  }

  ~TestWatchdog ()
  {
    {
      std::lock_guard<std::mutex> guard (mutex_);
      done_ = true;
    }
    condition_.notify_all ();
    thread_.join ();
  }

  TestWatchdog (TestWatchdog const &) = delete;
  TestWatchdog &operator= (TestWatchdog const &) = delete;

private:
  void
  run ()
  {
    std::unique_lock<std::mutex> guard (mutex_);
    if (!condition_.wait_for (guard, std::chrono::seconds (seconds_),
                              [this] { return done_; }))
      {
        std::fprintf (stderr,
                      "\nWATCHDOG: '%s' did not finish within %d s (a thread "
                      "is probably asleep forever)\n",
                      what_, seconds_);
        std::fflush (stderr);
        std::abort ();
      }
  }

  char const *what_;
  int seconds_;
  bool done_;
  std::mutex mutex_;
  std::condition_variable condition_;
  std::thread thread_;
};

/**
 * @brief Collects the violations a concurrent scenario finds.
 * @details Thread-safe. A scenario calls `fail ("...")` for every broken
 * invariant; the first few messages are kept for the report. A clean run has
 * `ok () == true`. The checkers of the test suites return a `Verdict`, so
 * the same scenario serves a test that expects no violation and a self-test
 * that runs it against a deliberately broken implementation and expects one.
 */
class Verdict
{
public:
  Verdict () : count_ (0) {}

  Verdict (Verdict const &) = delete;
  Verdict &operator= (Verdict const &) = delete;

  /// Records one violation.
  void
  fail (std::string const &what)
  {
    count_.fetch_add (1, std::memory_order_relaxed);
    std::lock_guard<std::mutex> guard (mutex_);
    if (messages_.size () < 6)
      messages_.push_back (what);
  }

  /// Records a violation when @p condition is false.
  void
  require (bool condition, std::string const &what)
  {
    if (!condition)
      fail (what);
  }

  /// True when no violation was recorded.
  bool
  ok () const
  {
    return count_.load (std::memory_order_relaxed) == 0;
  }

  /// Number of violations.
  long
  violations () const
  {
    return count_.load (std::memory_order_relaxed);
  }

  /// The kept messages, one per line, with the total.
  std::string
  text () const
  {
    std::lock_guard<std::mutex> guard (mutex_);
    std::string out = std::to_string (violations ()) + " violation(s)";
    for (std::size_t i = 0; i < messages_.size (); ++i)
      out += "\n  - " + messages_[i];
    return out;
  }

private:
  std::atomic<long> count_;
  mutable std::mutex mutex_;
  std::vector<std::string> messages_;
};
} // namespace lumex_test

#endif // !LUMEX_TESTS_SUPPORT_TEST_THREADS_HPP
