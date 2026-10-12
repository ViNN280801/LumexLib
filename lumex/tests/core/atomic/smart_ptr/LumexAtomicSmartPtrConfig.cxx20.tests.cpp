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

// Implementation selection of the atomic smart pointers at C++20: where the
// standard library has std::atomic<std::shared_ptr<T>>
// (LUMEX_HAS_STD_ATOMIC_SHARED_PTR), a differential run of the same scenario
// on atomic_shared_ptr and on the standard type. Without it (libc++,
// libstdc++ before 12) the test is skipped.

#include <atomic>
#include <cstddef>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/tests/core/atomic/LumexAtomicTestSupport.hpp"

using namespace lumex_atomic_test;

namespace smart_ptr = lumex::core::atomic::smart_ptr;

#if LUMEX_HAS_STD_ATOMIC_SHARED_PTR
namespace
{
struct Pair
{
  int a;
  int b;
};

int g_value = 3;

/// One scripted run on any atomic shared pointer type; returns the trace.
template <typename Atomic>
std::vector<long>
run_script (std::shared_ptr<Pair> const &owner)
{
  std::vector<long> trace;
  std::shared_ptr<int> const view_a (owner, &owner->a);
  std::shared_ptr<int> const view_b (owner, &owner->b);
  std::shared_ptr<int> const other_owner_same_pointer (
      std::make_shared<int> (0), &g_value);
  std::shared_ptr<int> const empty_storing (std::shared_ptr<int> (), &g_value);
  std::shared_ptr<int> const null_owner (static_cast<int *> (nullptr));

  Atomic a (view_a);
  std::shared_ptr<int> e = view_b;
  trace.push_back (a.compare_exchange_strong (e, view_b) ? 1 : 0);
  trace.push_back (e.get () == view_a.get () ? 1 : 0);
  e = view_a;
  trace.push_back (a.compare_exchange_strong (e, empty_storing) ? 1 : 0);
  e = std::shared_ptr<int> ();
  trace.push_back (a.compare_exchange_strong (e, null_owner) ? 1 : 0);
  trace.push_back (e.get () == &g_value ? 1 : 0);
  trace.push_back (e.use_count ());
  trace.push_back (a.compare_exchange_strong (e, null_owner) ? 1 : 0);
  e = std::shared_ptr<int> ();
  trace.push_back (a.compare_exchange_strong (e, view_a) ? 1 : 0);
  trace.push_back (e.use_count ());
  e = null_owner;
  trace.push_back (
      a.compare_exchange_strong (e, other_owner_same_pointer) ? 1 : 0);
  e = std::shared_ptr<int> (std::make_shared<int> (0), &g_value);
  trace.push_back (a.compare_exchange_strong (e, view_a) ? 1 : 0);
  std::shared_ptr<int> const previous = a.exchange (view_b);
  trace.push_back (previous.get () == &g_value ? 1 : 0);
  trace.push_back (a.load ().get () == &owner->b ? 1 : 0);
  a.store (nullptr);
  trace.push_back (a.load () ? 1 : 0);
  trace.push_back (owner.use_count ());
  return trace;
}
} // namespace
#endif

TEST (LumexAtomicSmartPtrConfigTest,
      GivenTheStandardType_WhenRunningTheSameScenario_ThenOutcomesMatch)
{
  LUMEX_ATOMIC_TEST_REQUIRE_PROMPT_DESTRUCTION ();
#if LUMEX_HAS_STD_ATOMIC_SHARED_PTR
  std::shared_ptr<Pair> const owner = std::make_shared<Pair> (Pair{ 10, 20 });
  std::vector<long> const standard
      = run_script<std::atomic<std::shared_ptr<int>>> (owner);
  // Every engine of the library, the wrapper and the common name behave as
  // the standard type on the scripted scenario.
  std::vector<long> const wrapped
      = run_script<smart_ptr::atomic_shared_ptr_std_backed<int>> (owner);
  std::vector<long> const locked
      = run_script<smart_ptr::atomic_shared_ptr_lock_based<int>> (owner);
  std::vector<long> const common = run_script<atomic_shared_ptr<int>> (owner);
#if LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE
  std::vector<long> const free_engine
      = run_script<smart_ptr::atomic_shared_ptr_lock_free<int>> (owner);
#endif
  ASSERT_EQ (wrapped.size (), standard.size ());
  ASSERT_EQ (locked.size (), standard.size ());
  ASSERT_EQ (common.size (), standard.size ());
  for (std::size_t i = 0; i < standard.size (); ++i)
    {
      EXPECT_EQ (wrapped[i], standard[i]) << "std_backed, step " << i;
      EXPECT_EQ (locked[i], standard[i]) << "lock_based, step " << i;
      EXPECT_EQ (common[i], standard[i]) << "common, step " << i;
#if LUMEX_ATOMIC_SMART_PTR_HAS_LOCK_FREE
      EXPECT_EQ (free_engine[i], standard[i]) << "lock_free, step " << i;
#endif
    }
#else
  GTEST_SKIP ()
      << "the standard library has no std::atomic<std::shared_ptr<T>>";
#endif
}
