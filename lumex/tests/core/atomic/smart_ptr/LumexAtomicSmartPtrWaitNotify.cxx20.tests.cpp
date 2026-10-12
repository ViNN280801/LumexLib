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

// wait / notify of atomic_shared_ptr with the C++20 thread synchronization of
// the standard library: waiters released by a std::latch and a std::jthread
// consumer stopped through std::stop_token. The feature checks stay because a
// standard library may lack them at C++20 (libstdc++ 8); the tests are then
// skipped.

#include <atomic>
#include <memory>
#include <thread>
#include <vector>
#if defined(__has_include)
#if __has_include(<version>)
#include <version>
#endif
#endif
#if defined(__cpp_lib_latch)
#include <latch>
#endif
#if defined(__cpp_lib_jthread)
#include <stop_token>
#endif

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/tests/core/atomic/LumexAtomicTestSupport.hpp"

using namespace lumex_atomic_test;

TEST (LumexAtomicSmartPtrWaitNotifyTest,
      GivenWaitersStartedByALatch_WhenNotifyAll_ThenEveryOneWakes)
{
#if defined(__cpp_lib_latch)
  // C++20: std::latch releases every waiter at the same moment.
  Watchdog const dog ("GivenWaitersStartedByALatch_WhenNotifyAll");
  int const waiter_count = 8;
  atomic_shared_ptr<int> a (std::make_shared<int> (0));
  std::latch start (waiter_count + 1);
  std::atomic<int> woke (0);
  std::vector<std::thread> waiters;
  for (int i = 0; i < waiter_count; ++i)
    waiters.push_back (std::thread (
        [&]
          {
            std::shared_ptr<int> const old = a.load ();
            start.arrive_and_wait ();
            a.wait (old);
            woke.fetch_add (1);
          }));
  start.arrive_and_wait ();
  a.store (std::make_shared<int> (1));
  a.notify_all ();
  join_all (waiters);
  EXPECT_EQ (woke.load (), waiter_count);
#else
  GTEST_SKIP () << "std::latch needs __cpp_lib_latch";
#endif
}

TEST (LumexAtomicSmartPtrWaitNotifyTest,
      GivenAJthreadConsumer_WhenStopIsRequested_ThenANotifiedChangeEndsIt)
{
#if defined(__cpp_lib_jthread)
  // C++20: std::jthread with std::stop_token; the stop request alone does
  // not end a wait, the notified change does.
  Watchdog const dog ("GivenAJthreadConsumer_WhenStopIsRequested");
  atomic_shared_ptr<int> a (std::make_shared<int> (0));
  std::atomic<int> seen (0);
  std::atomic<bool> started (false);
  {
    std::jthread consumer (
        [&] (std::stop_token stop)
          {
            while (!stop.stop_requested ())
              {
                std::shared_ptr<int> const current = a.load ();
                seen.store (*current);
                started.store (true);
                a.wait (current);
              }
          });
    wait_for_flag (started);
    a.store (std::make_shared<int> (1));
    a.notify_one ();
    while (seen.load () != 1)
      std::this_thread::yield ();
    consumer.request_stop ();
    a.store (std::make_shared<int> (2));
    a.notify_one ();
  } // jthread joins here
  EXPECT_GE (seen.load (), 1);
#else
  GTEST_SKIP () << "std::jthread needs __cpp_lib_jthread";
#endif
}
