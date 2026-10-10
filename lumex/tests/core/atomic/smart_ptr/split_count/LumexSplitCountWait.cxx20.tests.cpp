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

// wait and notify of the split-count engine with the C++20 thread
// synchronization of the standard library: waiters released by a std::latch
// and a std::jthread consumer stopped through std::stop_token. The feature
// checks stay because a standard library may lack them at C++20; the tests
// are then skipped.

#include <atomic>
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

#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountTestSupport.hpp"

#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

namespace
{
using namespace split_test;

typedef asp::atomic_shared_ptr_lock_free_split_count<int> shared_atomic;

TEST (LumexSplitCountWaitCxx20Test,
      GivenWaitersStartedByALatch_WhenNotifyAll_ThenEveryOneWakes)
{
#if defined(__cpp_lib_latch)
  Watchdog const dog ("latch waiters");
  int const waiter_count = 8;
  sp::shared_ptr<int> const first = sp::make_shared<int> (0);
  shared_atomic a (first);
  std::latch start (waiter_count + 1);
  std::atomic<int> woke (0);
  std::vector<std::thread> waiters;
  for (int i = 0; i < waiter_count; ++i)
    waiters.push_back (std::thread (
        [&]
          {
            sp::shared_ptr<int> const old = a.load ();
            start.arrive_and_wait ();
            a.wait (old);
            woke.fetch_add (1);
          }));
  start.arrive_and_wait ();
  a.store (sp::make_shared<int> (1));
  a.notify_all ();
  for (std::size_t i = 0; i < waiters.size (); ++i)
    waiters[i].join ();
  EXPECT_EQ (woke.load (), waiter_count);
#else
  GTEST_SKIP () << "std::latch needs __cpp_lib_latch";
#endif
}

TEST (LumexSplitCountWaitCxx20Test,
      GivenAJthreadConsumer_WhenStopIsRequested_ThenANotifiedChangeEndsIt)
{
#if defined(__cpp_lib_jthread)
  Watchdog const dog ("jthread consumer");
  shared_atomic a (sp::make_shared<int> (0));
  std::atomic<int> seen (0);
  std::atomic<bool> started (false);
  {
    std::jthread consumer (
        [&] (std::stop_token stop)
          {
            while (!stop.stop_requested ())
              {
                sp::shared_ptr<int> const current = a.load ();
                seen.store (*current);
                started.store (true);
                a.wait (current);
              }
          });
    spin_until ([&] { return started.load (); });
    a.store (sp::make_shared<int> (1));
    a.notify_one ();
    spin_until ([&] { return seen.load () == 1; });
    consumer.request_stop ();
    a.store (sp::make_shared<int> (2));
    a.notify_one ();
  }
  EXPECT_GE (seen.load (), 1);
#else
  GTEST_SKIP () << "std::jthread needs __cpp_lib_jthread";
#endif
}
} // namespace

#else

TEST (LumexSplitCountWaitCxx20Test,
      GivenAPlatformWithoutTheEngine_WhenCompiled_ThenSkipped)
{
  GTEST_SKIP () << "the split-count engine needs x86-64 and lumex::smart_ptr";
}

#endif
