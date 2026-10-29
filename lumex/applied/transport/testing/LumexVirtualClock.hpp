/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy
 * of this software and associated
 * documentation files (the "Software"), to deal
 * in the Software without
 * restriction, including without limitation the rights
 * to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the
 * Software, and to permit persons to whom the Software is
 * furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice
 * and this permission notice shall be included in
 * all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT
 * WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO
 * THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file LumexVirtualClock.hpp
 * @brief `virtual_clock`: a timer source whose time moves only when a test
 * advances it, so that code on the transport's timer seam (sessions,
 * `device_watcher`) runs its delays without waiting for them.
 * @details The clock starts at the time given (by default the epoch of
 * `time::clock_type`, `time_point_t ()`) and stands still. `source ()` gives
 * the `time::timer_source` to put in the options of the code under test;
 * every timer made from it waits on this clock. `advance (by)` moves the time
 * forward and ends every wait that is due by then, in the order of their
 * times and, for equal times, in the order they were started; each handler is
 * posted to its timer's executor (and runs on its own associated executor),
 * never called inside `advance`. A wait started for a time that already
 * passed (a delay of 0, a deadline in the past) is due: it ends at the next
 * `advance`, also `advance (0)`, not at once. `advance_to_next ()` moves the
 * time to the earliest pending wait and ends the waits due then; with
 * nothing pending it returns 0 and the time stays.
 *
 * The clock is thread-safe: a test may advance it while the handlers run on
 * an `io_context` thread. A pending virtual wait keeps no work on any
 * executor: `io_context::run ()` returns while one is pending. `run_until`
 * drives a context with `poll ()` and moves the clock wait by wait, which is
 * how a test lets a session go through its delays.
 *
 * Lifetime. Destroying the clock ends every pending wait with
 * `transport_errc::cancelled`; a timer made from it may outlive it, and a
 * wait started on such a timer ends with `cancelled` at once (posted). A
 * timer may also outlive its `io_context`: the destruction of the context
 * destroys the pending handler without calling it and forgets the timer's
 * executor (also a strand), and a later wait on that timer is dropped the
 * same way. Available with `LUMEX_TRANSPORT_WITH_TESTING`.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_TESTING_VIRTUAL_CLOCK_HPP
#define LUMEX_APPLIED_TRANSPORT_TESTING_VIRTUAL_CLOCK_HPP

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/time/LumexTimerSource.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif

namespace lumex
{
namespace applied
{
namespace transport
{
namespace testing
{
class virtual_clock_impl;
class virtual_clock;

/**
 * @brief Runs `context` with `poll ()` and moves `clock` until `done ()`
 * returns true or the clock went `budget` past its time at the call.
 * @details Each pass runs every ready handler (restarting the context when
 * it ran out of work), asks `done ()`, ends the waits due now and, when there
 * are none, moves the clock to the next pending wait. When the next wait
 * lies beyond the budget (or nothing is pending), the clock moves to the end
 * of the budget, the context runs once more and the result is `done ()`. No
 * other thread may run `context` meanwhile; a handler that starts a wait for
 * the current time on every pass keeps the loop going.
 * @return True when `done ()` became true.
 */
bool run_until (asio::io_context &context, virtual_clock &clock,
                std::function<bool ()> const &done,
                std::chrono::milliseconds budget);

/// @brief A clock for tests (see the file description).
class virtual_clock
{
public:
  /// @brief A clock that stands at `start`.
  explicit virtual_clock (time::time_point_t start = time::time_point_t ());

  /// @brief Ends every pending wait with `cancelled`.
  ~virtual_clock ();

  virtual_clock (virtual_clock const &) = delete;
  virtual_clock &operator= (virtual_clock const &) = delete;

  /// @brief The source to give the code under test (every call returns the
  /// same source).
  time::timer_source source () const;

  /// @brief The time now.
  time::time_point_t now () const;

  /// @brief Moves the time `by` forward (a negative value as 0) and ends the
  /// waits due by then; returns how many it ended.
  std::size_t advance (std::chrono::milliseconds by);

  /// @brief Moves the time to the earliest pending wait (or stays when it
  /// is due already) and ends the waits due then; returns how many it ended
  /// (0 with nothing pending).
  std::size_t advance_to_next ();

  /// @brief The waits pending now.
  std::size_t pending () const;

  /// @brief The time of the earliest pending wait (`time_point_t::max ()`
  /// with nothing pending).
  time::time_point_t next_expiry () const;

private:
  friend bool run_until (asio::io_context &context, virtual_clock &clock,
                         std::function<bool ()> const &done,
                         std::chrono::milliseconds budget);

  std::shared_ptr<virtual_clock_impl> impl_;
  time::timer_source source_;
};
} // namespace testing
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_TESTING_VIRTUAL_CLOCK_HPP
