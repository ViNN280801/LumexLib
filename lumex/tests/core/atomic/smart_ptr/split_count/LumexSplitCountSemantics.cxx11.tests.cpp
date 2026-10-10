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

// The semantics table of the split-count engine on a single thread: every
// operation crossed with every kind of value (empty, empty-owner alias, delta
// zero, member alias, base-class conversion, negative delta, out-of-window
// alias, `nullptr` alias of a live owner, and the aliases at the four edges of
// the 40-bit window). The templates are in LumexSplitCountScenarios.hpp; the
// torn file runs them again on a torn policy.

#include <atomic>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountScenarios.hpp"
#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountTestSupport.hpp"

#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

namespace
{
using namespace split_test;

typedef semantics_t<public_engine_t> public_semantics;
typedef semantics_t<policy_engine_t<split_native_policy_t>> native_semantics;

TEST (LumexSplitCountSemanticsTest,
      GivenTheKindSet_WhenBuilt_ThenEveryKindHasTheOffsetItIsNamedFor)
{
  kind_set_t const set = make_kind_set ();
  std::int64_t const half = std::int64_t (1) << 39;
  struct expectation_t
  {
    char const *name;
    bool holder;
  };
  expectation_t const wanted[] = { { "empty", false },
                                   { "empty alias a", false },
                                   { "empty alias b", false },
                                   { "delta zero", false },
                                   { "member", false },
                                   { "below the anchor", false },
                                   { "far", true },
                                   { "another far", true },
                                   { "last in window", false },
                                   { "first out above", true },
                                   { "lowest in window", false },
                                   { "first out below", true },
                                   { "other owner", false },
                                   { "other owner far", true } };
  for (std::size_t i = 0; i < set.kinds.size (); ++i)
    {
      kind_t const &kind = set.kinds[i];
      for (std::size_t j = 0; j < sizeof wanted / sizeof wanted[0]; ++j)
        if (std::string (wanted[j].name) == kind.name)
          {
            EXPECT_EQ (needs_holder (kind.value), wanted[j].holder)
                << kind.name;
          }
    }
  EXPECT_EQ (offset_of_value (set.kinds[3].value), 0) << "delta zero";
  EXPECT_EQ (offset_of_value (set.kinds[5].value), -64);
  EXPECT_EQ (offset_of_value (set.kinds[9].value), half - 1);
  EXPECT_EQ (offset_of_value (set.kinds[10].value), half);
  EXPECT_EQ (offset_of_value (set.kinds[11].value), -half);
  EXPECT_EQ (offset_of_value (set.kinds[12].value), -half - 1);
}

TEST (LumexSplitCountSemanticsTest,
      GivenEveryKind_WhenLoaded_ThenValueCountsAndAllocationsAreRight)
{
  public_semantics::load_table ();
}

TEST (LumexSplitCountSemanticsTest,
      GivenEveryPairOfKinds_WhenStoredAndExchanged_ThenValuesAndCountsAreRight)
{
  public_semantics::replace_table ();
}

TEST (
    LumexSplitCountSemanticsTest,
    GivenEveryTripleOfKinds_WhenCompareExchanged_ThenItSucceedsExactlyOnEquivalence)
{
  public_semantics::cas_table ();
}

TEST (LumexSplitCountSemanticsTest,
      GivenEveryMemoryOrder_WhenUsed_ThenEveryOverloadWorks)
{
  public_semantics::order_table ();
}

TEST (LumexSplitCountSemanticsTest,
      GivenTheObject_WhenConstructedAssignedAndConverted_ThenItBehaves)
{
  public_semantics::object_table ();
}

TEST (LumexSplitCountSemanticsTest,
      GivenReplacedValues_WhenNoLoadIsInFlight_ThenTheyDieInTheCall)
{
  public_semantics::destruction_table ();
}

TEST (
    LumexSplitCountSemanticsTest,
    GivenTheCellWithTheNativePolicy_WhenTheTableRuns_ThenItMatchesThePublicClass)
{
  native_semantics::object_table ();
  native_semantics::load_table ();
  native_semantics::replace_table ();
  native_semantics::order_table ();
  native_semantics::destruction_table ();
}

TEST (LumexSplitCountSemanticsTest,
      GivenABaseConversion_WhenStoredLoadedAndSwapped_ThenTheDeltaSurvives)
{
  base_conversion_table<public_engine_t> ();
}

TEST (LumexSplitCountSemanticsTest,
      GivenTheTypes_WhenInspected_ThenTheyMatchThePlan)
{
  typedef asp::atomic_shared_ptr_lock_free_split_count<Obj> shared_atomic;
  typedef asp::atomic_weak_ptr_lock_free_split_count<Obj> weak_atomic;
  EXPECT_EQ (sizeof (shared_atomic), 32u);
  EXPECT_EQ (sizeof (weak_atomic), 32u);
  EXPECT_EQ (alignof (shared_atomic), 16u);
  EXPECT_TRUE (std::is_nothrow_default_constructible<shared_atomic>::value);
  EXPECT_TRUE (std::is_nothrow_default_constructible<weak_atomic>::value);
  EXPECT_FALSE (std::is_copy_constructible<shared_atomic>::value);
  EXPECT_FALSE (std::is_copy_assignable<shared_atomic>::value);
  shared_atomic a;
  weak_atomic w;
  EXPECT_TRUE (a.is_lock_free ());
  EXPECT_TRUE (w.is_lock_free ());
}

TEST (
    LumexSplitCountSemanticsTest,
    GivenAnEmptyOwnerAlias_WhenCompareExchangedWithTheSameRawPointer_ThenItIsEquivalent)
{
  int first = 1;
  int second = 2;
  sp::shared_ptr<int> const none;
  asp::atomic_shared_ptr_lock_free_split_count<int> a (
      sp::shared_ptr<int> (none, &first));
  sp::shared_ptr<int> expected (none, &first);
  EXPECT_TRUE (a.compare_exchange_strong (expected,
                                          sp::shared_ptr<int> (none, &second)))
      << "same raw pointer, both without an owner";
  expected = sp::shared_ptr<int> (none, &first);
  EXPECT_FALSE (a.compare_exchange_strong (expected, sp::shared_ptr<int> ()));
  EXPECT_EQ (expected.get (), &second)
      << "another raw pointer is not equivalent";
  expected = sp::shared_ptr<int> ();
  EXPECT_FALSE (a.compare_exchange_strong (expected, sp::shared_ptr<int> ()))
      << "an empty pointer is not equivalent to a raw alias";
}
} // namespace

#else

TEST (LumexSplitCountSemanticsTest,
      GivenAPlatformWithoutTheEngine_WhenCompiled_ThenSkipped)
{
  GTEST_SKIP () << "the split-count engine needs x86-64 and lumex::smart_ptr";
}

#endif
