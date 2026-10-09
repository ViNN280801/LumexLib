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

// Concurrency of atomic_shared_ptr with the C++20 thread synchronization of
// the standard library: threads started by a std::latch and std::jthread
// readers stopped through std::stop_token. The feature checks stay because a
// standard library may lack them at C++20 (libstdc++ 8); the tests are then
// skipped.

#include <atomic>
#include <cstddef>
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

TEST (LumexAtomicSmartPtrConcurrencyTest,
      GivenThreadsStartedByALatch_WhenExchangingAndLoading_ThenCountsBalance)
{
  LUMEX_ATOMIC_TEST_REQUIRE_PROMPT_DESTRUCTION ();
#if defined(__cpp_lib_latch)
  // C++20: std::latch starts every thread at once for maximum contention.
  Watchdog const dog ("GivenThreadsStartedByALatch");
  std::vector<int> const counts = stress_thread_counts ();
  for (std::size_t c = 0; c < counts.size (); ++c)
    {
      int const iterations = stress_iterations (3000);
      int const alive_before = Tracker::alive ().load ();
      {
        atomic_shared_ptr<Tracker> a (make_counted_tracker (0));
        std::latch start (counts[c]);
        std::vector<std::thread> threads;
        for (int t = 0; t < counts[c]; ++t)
          threads.push_back (std::thread (
              [&, t]
                {
                  start.arrive_and_wait ();
                  for (int i = 0; i < iterations; ++i)
                    {
                      if ((i + t) % 2 == 0)
                        a.exchange (make_counted_tracker (i));
                      else
                        {
                          std::shared_ptr<Tracker> const v = a.load ();
                          EXPECT_TRUE (v && v->intact ());
                        }
                    }
                }));
        join_all (threads);
      }
      EXPECT_EQ (Tracker::alive ().load (), alive_before);
    }
#else
  GTEST_SKIP () << "std::latch needs __cpp_lib_latch";
#endif
}

TEST (LumexAtomicSmartPtrConcurrencyTest,
      GivenJthreadReaders_WhenStopIsRequested_ThenTheyJoinAndCountsBalance)
{
#if defined(__cpp_lib_jthread)
  // C++20: std::jthread readers stop through std::stop_token.
  Watchdog const dog ("GivenJthreadReaders_WhenStopIsRequested");
  int const alive_before = Tracker::alive ().load ();
  {
    atomic_shared_ptr<Tracker> a (make_counted_tracker (0));
    std::atomic<long> broken (0);
    {
      std::vector<std::jthread> readers;
      for (int r = 0; r < 4; ++r)
        readers.emplace_back (
            [&] (std::stop_token stop)
              {
                while (!stop.stop_requested ())
                  {
                    std::shared_ptr<Tracker> const v = a.load ();
                    if (!v || !v->intact ())
                      broken.fetch_add (1);
                  }
              });
      for (int i = 0; i < stress_iterations (5000); ++i)
        a.store (make_counted_tracker (i));
    } // request_stop and join
    EXPECT_EQ (broken.load (), 0L);
  }
  EXPECT_EQ (Tracker::alive ().load (), alive_before);
#else
  GTEST_SKIP () << "std::jthread needs __cpp_lib_jthread";
#endif
}
