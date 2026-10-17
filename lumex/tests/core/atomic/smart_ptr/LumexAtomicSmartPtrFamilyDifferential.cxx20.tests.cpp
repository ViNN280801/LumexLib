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

// Differential run of the library's own pointer family against the standard
// type: the scenario of LumexAtomicSmartPtrConfig.cxx20 (the Std-only run) is
// replayed on the engines over lumex::core::smart_ptr, the split-count engine
// and the lock-based engine over the family, and the trace of each is
// compared element by element with the trace of std::atomic<std::shared_ptr>
// recorded in the same test. A weak scenario does the same for the weak
// twins and std::atomic<std::weak_ptr>. Every read of a use_count is taken at
// rest: no thread operates on the atomic while it is read.
//
// Where the family may legitimately differ from the standard the behavior is
// asserted, not skipped:
//  - use_count of the split-count engine is count + ext (never below the
//    owners, exact at rest); every read here is at rest, so it equals the
//    standard value and stays inside the trace;
//  - an expired alias (empty owner, non-null stored pointer) keeps its stored
//    pointer in the family; the trace pins it ("stored pointer of an expired
//    alias" below) and the family's value is required to equal the
//    standard's.
// Without std::atomic<std::shared_ptr> (libc++, libstdc++ before 12) the test
// is skipped; without the family or the split-count engine the matching part
// is not compiled.

#include <atomic>
#include <cstddef>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/tests/core/atomic/LumexAtomicTestSupport.hpp"

#if LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY
#include "lumex/core/smart_ptr/LumexSmartPtr"
#endif

namespace smart_ptr = lumex::core::atomic::smart_ptr;

// Std-only: the standard type is the reference and the family engines are
// named explicitly, so the variants of the library's own family would only
// repeat the run.
#if !defined(LUMEX_ATOMIC_TEST_POINTER_FAMILY_LUMEX)
#if LUMEX_HAS_STD_ATOMIC_SHARED_PTR && LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY
namespace
{
struct Pair
{
  int a;
  int b;
};

int g_value = 3;

/// The pointers of one family, so the same script runs on both.
struct std_family
{
  template <typename T> using shared = std::shared_ptr<T>;
  template <typename T> using weak = std::weak_ptr<T>;

  template <typename T, typename... Args>
  static shared<T>
  make (Args &&...args)
  {
    return std::make_shared<T> (std::forward<Args> (args)...);
  }
};

struct lumex_family
{
  template <typename T> using shared = lumex::core::smart_ptr::shared_ptr<T>;
  template <typename T> using weak = lumex::core::smart_ptr::weak_ptr<T>;

  template <typename T, typename... Args>
  static shared<T>
  make (Args &&...args)
  {
    return lumex::core::smart_ptr::make_shared<T> (
        std::forward<Args> (args)...);
  }
};

/// The scenario of the Config.cxx20 differential run, on any atomic shared
/// pointer of the family @p Family; returns the trace.
template <typename Family, typename Atomic>
std::vector<long>
run_shared_script (typename Family::template shared<Pair> const &owner)
{
  using int_ptr = typename Family::template shared<int>;
  std::vector<long> trace;
  int_ptr const view_a (owner, &owner->a);
  int_ptr const view_b (owner, &owner->b);
  int_ptr const other_owner_same_pointer (Family::template make<int> (0),
                                          &g_value);
  int_ptr const empty_storing (int_ptr (), &g_value);
  int_ptr const null_owner (static_cast<int *> (nullptr));

  Atomic a (view_a);
  int_ptr e = view_b;
  trace.push_back (a.compare_exchange_strong (e, view_b) ? 1 : 0);
  trace.push_back (e.get () == view_a.get () ? 1 : 0);
  // At rest: the owner, view_a, view_b and the atomic's own copy of view_a.
  trace.push_back (owner.use_count ());
  e = view_a;
  trace.push_back (a.compare_exchange_strong (e, empty_storing) ? 1 : 0);
  e = int_ptr ();
  trace.push_back (a.compare_exchange_strong (e, null_owner) ? 1 : 0);
  // Stored pointer of an expired alias: the failed exchange hands back the
  // stored pointer of the atomic's value, in the family as in the standard.
  trace.push_back (e.get () == &g_value ? 1 : 0);
  trace.push_back (e.use_count ());
  trace.push_back (a.compare_exchange_strong (e, null_owner) ? 1 : 0);
  e = int_ptr ();
  trace.push_back (a.compare_exchange_strong (e, view_a) ? 1 : 0);
  trace.push_back (e.use_count ());
  e = null_owner;
  trace.push_back (
      a.compare_exchange_strong (e, other_owner_same_pointer) ? 1 : 0);
  e = int_ptr (Family::template make<int> (0), &g_value);
  trace.push_back (a.compare_exchange_strong (e, view_a) ? 1 : 0);
  int_ptr const previous = a.exchange (view_b);
  trace.push_back (previous.get () == &g_value ? 1 : 0);
  trace.push_back (a.load ().get () == &owner->b ? 1 : 0);
  // A load at rest: the loaded copy is one more owner than the atomic's.
  trace.push_back (a.load ().use_count ());
  a.store (nullptr);
  trace.push_back (a.load () ? 1 : 0);
  trace.push_back (owner.use_count ());
  return trace;
}

/// A scenario on atomic weak pointers; returns the trace.
template <typename Family, typename Atomic>
std::vector<long>
run_weak_script ()
{
  using int_ptr = typename Family::template shared<int>;
  using int_weak = typename Family::template weak<int>;
  std::vector<long> trace;
  int_ptr first = Family::template make<int> (1);
  int_ptr second = Family::template make<int> (2);
  int_ptr third = Family::template make<int> (3);

  int_weak const initial (first);
  Atomic a (initial);
  trace.push_back (a.load ().expired () ? 1 : 0);
  trace.push_back (a.load ().lock ().get () == first.get () ? 1 : 0);
  int_weak e (second);
  trace.push_back (a.compare_exchange_strong (e, int_weak (third)) ? 1 : 0);
  trace.push_back (e.lock ().get () == first.get () ? 1 : 0);
  trace.push_back (first.use_count ());
  trace.push_back (a.compare_exchange_strong (e, int_weak (second)) ? 1 : 0);
  int_weak const previous = a.exchange (int_weak (third));
  trace.push_back (previous.lock ().get () == second.get () ? 1 : 0);
  trace.push_back (a.load ().lock ().get () == third.get () ? 1 : 0);
  trace.push_back (third.use_count ());
  third.reset ();
  trace.push_back (a.load ().expired () ? 1 : 0);
  trace.push_back (a.load ().lock () ? 1 : 0);
  a.store (int_weak ());
  trace.push_back (a.load ().expired () ? 1 : 0);
  trace.push_back (second.use_count ());
  trace.push_back (first.use_count ());
  return trace;
}

void
expect_same_trace (std::vector<long> const &reference,
                   std::vector<long> const &other, char const *name)
{
  ASSERT_EQ (other.size (), reference.size ()) << name;
  for (std::size_t i = 0; i < reference.size (); ++i)
    EXPECT_EQ (other[i], reference[i]) << name << ", step " << i;
}
} // namespace
#endif

TEST (
    LumexAtomicSmartPtrFamilyDifferentialTest,
    GivenTheStandardType_WhenRunningTheSharedScenarioOnTheFamily_ThenTracesMatch)
{
  LUMEX_ATOMIC_TEST_REQUIRE_PROMPT_DESTRUCTION ();
#if LUMEX_HAS_STD_ATOMIC_SHARED_PTR && LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY
  std::shared_ptr<Pair> const std_owner
      = std::make_shared<Pair> (Pair{ 10, 20 });
  std::vector<long> const standard
      = run_shared_script<std_family, std::atomic<std::shared_ptr<int>>> (
          std_owner);
  ASSERT_FALSE (standard.empty ());

  lumex_family::shared<Pair> const family_owner
      = lumex_family::make<Pair> (Pair{ 10, 20 });
  expect_same_trace (
      standard,
      run_shared_script<lumex_family,
                        smart_ptr::atomic_shared_ptr_lock_based_lumex<int>> (
          family_owner),
      "lock_based_lumex");
#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT
  lumex_family::shared<Pair> const split_owner
      = lumex_family::make<Pair> (Pair{ 10, 20 });
  expect_same_trace (
      standard,
      run_shared_script<
          lumex_family,
          smart_ptr::atomic_shared_ptr_lock_free_split_count<int>> (
          split_owner),
      "lock_free_split_count");
  // Everything the script held is released: the owners are the test's own.
  EXPECT_EQ (split_owner.use_count (), 1);
#endif
  EXPECT_EQ (family_owner.use_count (), 1);
  EXPECT_EQ (std_owner.use_count (), 1);
#else
  GTEST_SKIP () << "needs std::atomic<std::shared_ptr<T>> and the pointer "
                   "family";
#endif
}

TEST (
    LumexAtomicSmartPtrFamilyDifferentialTest,
    GivenTheStandardType_WhenRunningTheWeakScenarioOnTheFamily_ThenTracesMatch)
{
  LUMEX_ATOMIC_TEST_REQUIRE_PROMPT_DESTRUCTION ();
#if LUMEX_HAS_STD_ATOMIC_SHARED_PTR && LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY
  std::vector<long> const standard
      = run_weak_script<std_family, std::atomic<std::weak_ptr<int>>> ();
  ASSERT_FALSE (standard.empty ());
  expect_same_trace (
      standard,
      run_weak_script<lumex_family,
                      smart_ptr::atomic_weak_ptr_lock_based_lumex<int>> (),
      "weak lock_based_lumex");
#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT
  expect_same_trace (
      standard,
      run_weak_script<
          lumex_family,
          smart_ptr::atomic_weak_ptr_lock_free_split_count<int>> (),
      "weak lock_free_split_count");
#endif
#else
  GTEST_SKIP () << "needs std::atomic<std::weak_ptr<T>> and the pointer "
                   "family";
#endif
}
#endif
