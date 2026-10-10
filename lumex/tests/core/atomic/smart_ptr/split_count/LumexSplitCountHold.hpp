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
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
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

// A group of threads held by a gate policy at a point of the protocol, for the
// scenario tests (exactness, deleters, the tick limit).
#ifndef LUMEX_TESTS_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_HOLD_HPP
#define LUMEX_TESTS_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_HOLD_HPP

#include <atomic>
#include <thread>
#include <vector>

#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountTestSupport.hpp"

#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

namespace split_test
{
/**
 * @brief Threads that run a body and stop at a point of the protocol until
 * `release ()`.
 * @tparam Policy A `gate_policy_t`.
 * @details The destructor opens the gate and joins, so a failed assertion
 * never leaves a thread behind. `arrived ()` of the policy counts the
 * threads that reached their hold, over all groups; the test resets it to 0
 * when it starts and `wait_arrived` takes the cumulative number.
 */
template <typename Policy> class hold_group_t
{
public:
  hold_group_t () : gate_ (false), threads_ () {}

  ~hold_group_t () { release (); }

  hold_group_t (hold_group_t const &) = delete;
  hold_group_t &operator= (hold_group_t const &) = delete;

  /// Starts @p count threads; thread i runs @p body (i) with its gate armed
  /// at the (@p skip + 1)-th @p point.
  template <typename Body>
  void
  start (int count, Body const &body,
         split_point_t point = split_point_t::load_count, int skip = 0)
  {
    for (int i = 0; i < count; ++i)
      threads_.push_back (std::thread (
          [this, body, i, point, skip]
            {
              arm_gate (gate_, point, skip);
              body (i);
              disarm_gate ();
            }));
  }

  /// True when @p count threads (cumulative over all groups of the policy)
  /// reached their hold within the timeout.
  bool
  wait_arrived (int count) const
  {
    return spin_until ([count]
                         { return Policy::arrived ().load () >= count; });
  }

  /// Opens the gate and joins the threads.
  void
  release ()
  {
    gate_.store (true);
    for (std::size_t i = 0; i < threads_.size (); ++i)
      if (threads_[i].joinable ())
        threads_[i].join ();
    threads_.clear ();
  }

private:
  std::atomic<bool> gate_;
  std::vector<std::thread> threads_;
};
} // namespace split_test

#endif // LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

#endif // !LUMEX_TESTS_CORE_ATOMIC_SMART_PTR_SPLIT_COUNT_HOLD_HPP
