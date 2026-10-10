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

// The semantics table and the ledger shapes again, on a policy whose
// speculative reads of the word are torn: every k-th guess of a thread
// returns the low half of the current word with the high half of the value
// the thread guessed before. Every use of a guess must be validated by a
// whole-word compare-and-swap or by an acquire re-read, so the results,
// the counts and the ledger must not change with k = 1, 2, 3, 5, 7.

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountScenarios.hpp"
#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountTestSupport.hpp"

#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

namespace
{
using namespace split_test;

template <int N> struct torn_tag_t
{
};

std::uint32_t
period_of (int repetition)
{
  static std::uint32_t const periods[] = { 1u, 2u, 3u, 5u, 7u };
  return periods[repetition % 5];
}

TEST (LumexSplitCountTornTest,
      GivenTornGuesses_WhenTheSemanticsTableRuns_ThenNothingChanges)
{
  typedef torn_policy_t<torn_tag_t<1>> policy;
  for (int rep = 0; rep < 5; ++rep)
    {
      SCOPED_TRACE (period_of (rep));
      policy::set_period (period_of (rep));
      reset_ledger<policy> ();
      semantics_t<policy_engine_t<policy>>::run_all ();
      base_conversion_table<policy_engine_t<policy>> ();
      check_balanced<policy> ("semantics table");
    }
  policy::set_period (0);
}

TEST (
    LumexSplitCountTornTest,
    GivenTornGuesses_WhenAStrongCompareExchangeHasAnEquivalentExpected_ThenItNeverFails)
{
  typedef torn_policy_t<torn_tag_t<6>> policy;
  sp::shared_ptr<Obj> o1 = sp::make_shared<Obj> (1);
  sp::shared_ptr<Obj> o2 = sp::make_shared<Obj> (2);
  sp::shared_ptr<int> const values[] = {
    sp::shared_ptr<int> (o1, &o1->v), sp::shared_ptr<int> (o2, &o2->pad[1]),
    sp::shared_ptr<int> (o1, shifted (o1.get (), std::int64_t (1) << 41)),
    sp::shared_ptr<int> (o2, shifted (o2.get (), std::int64_t (1) << 41)),
    sp::shared_ptr<int> ()
  };
  for (std::uint32_t k = 1; k <= 7; ++k)
    {
      policy::set_period (k);
      shared_cell<int, policy> a (values[0]);
      for (int i = 0; i < 400; ++i)
        {
          sp::shared_ptr<int> const &now = values[i % 5];
          sp::shared_ptr<int> const &next = values[(i + 1) % 5];
          sp::shared_ptr<int> expected = now;
          EXPECT_TRUE (a.compare_exchange_strong (expected, next))
              << "period " << k << " step " << i
              << ": strong never fails while the value is equivalent";
        }
    }
  policy::set_period (0);
}

TEST (LumexSplitCountTornTest,
      GivenTornGuesses_WhenTheSharedShapeRuns_ThenTheLedgerBalances)
{
  Watchdog const dog ("torn shared");
  typedef torn_policy_t<torn_tag_t<2>> policy;
  for_each_shape (
      [] (int threads, int rep)
        {
          policy::set_period (period_of (rep));
          stress_shared<policy> (threads, rep);
        });
  policy::set_period (0);
}

TEST (
    LumexSplitCountTornTest,
    GivenTornGuesses_WhenTheSharedShapeRunsAtTheTickLimit_ThenTheLedgerBalances)
{
  Watchdog const dog ("torn limit");
  typedef torn_policy_t<torn_tag_t<3>, 2u, 0u> policy;
  for_each_shape (
      [] (int threads, int rep)
        {
          policy::set_period (period_of (rep));
          stress_shared<policy> (threads, rep);
        });
  policy::set_period (0);
}

TEST (LumexSplitCountTornTest,
      GivenTornGuesses_WhenTheHolderShapeRuns_ThenTheLedgerBalances)
{
  Watchdog const dog ("torn holder");
  typedef torn_policy_t<torn_tag_t<4>> policy;
  for_each_shape (
      [] (int threads, int rep)
        {
          policy::set_period (period_of (rep));
          stress_holder<policy> (threads, rep);
        });
  policy::set_period (0);
}

TEST (LumexSplitCountTornTest,
      GivenTornGuesses_WhenTheWeakShapeRuns_ThenTheLedgerBalances)
{
  Watchdog const dog ("torn weak");
  typedef torn_policy_t<torn_tag_t<5>, 0xFFFFFFu, 0u> policy;
  for_each_shape (
      [] (int threads, int rep)
        {
          policy::set_period (period_of (rep));
          stress_weak<policy> (threads, rep);
        });
  policy::set_period (0);
}
} // namespace

#else

TEST (LumexSplitCountTornTest,
      GivenAPlatformWithoutTheEngine_WhenCompiled_ThenSkipped)
{
  GTEST_SKIP () << "the split-count engine needs x86-64 and lumex::smart_ptr";
}

#endif
