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

// Memory orders from C++17: the failure order of a compare-exchange may be
// stronger than the success order (P0418R2), so every combination of the
// six success orders and the four load orders as failure orders must work;
// the C++11 table covers only the combinations legal before C++17. Also the
// inline variable `is_always_lock_free`.

#include <atomic>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountTestSupport.hpp"

#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

namespace
{
using namespace split_test;

typedef asp::atomic_shared_ptr_lock_free_split_count<int> shared_atomic;
typedef asp::atomic_weak_ptr_lock_free_split_count<int> weak_atomic;

static_assert (shared_atomic::is_always_lock_free,
               "usable in a constant expression");
static_assert (weak_atomic::is_always_lock_free,
               "usable in a constant expression");

TEST (LumexSplitCountOrdersCxx17Test,
      GivenAnyFailureOrder_WhenCompareExchanged_ThenTheResultDoesNotDependOnIt)
{
  std::memory_order const successes[]
      = { std::memory_order_relaxed, std::memory_order_consume,
          std::memory_order_acquire, std::memory_order_release,
          std::memory_order_acq_rel, std::memory_order_seq_cst };
  std::memory_order const failures[]
      = { std::memory_order_relaxed, std::memory_order_consume,
          std::memory_order_acquire, std::memory_order_seq_cst };
  sp::shared_ptr<Obj> const o1 = sp::make_shared<Obj> (1);
  sp::shared_ptr<Obj> const o2 = sp::make_shared<Obj> (2);
  sp::shared_ptr<int> const v (o1, &o1->v);
  sp::shared_ptr<int> const w (o2,
                               shifted (o2.get (), std::int64_t (1) << 41));
  for (std::size_t s = 0; s < 6; ++s)
    for (std::size_t f = 0; f < 4; ++f)
      {
        shared_atomic a (v);
        sp::shared_ptr<int> expected = w;
        EXPECT_FALSE (a.compare_exchange_strong (expected, w, successes[s],
                                                 failures[f]));
        EXPECT_TRUE (same_value (expected, v));
        EXPECT_TRUE (a.compare_exchange_strong (expected, w, successes[s],
                                                failures[f]));
        expected = v;
        EXPECT_FALSE (
            a.compare_exchange_strong (expected, v, successes[s], failures[f]))
            << "the slot holds the holder word";
        EXPECT_TRUE (same_value (expected, w));
        bool done = false;
        for (int attempt = 0; attempt < 64 && !done; ++attempt)
          done = a.compare_exchange_weak (expected, v, successes[s],
                                          failures[f]);
        EXPECT_TRUE (done);
        sp::shared_ptr<int> const now = a.load ();
        EXPECT_TRUE (same_value (now, v));
      }
}

TEST (LumexSplitCountOrdersCxx17Test,
      GivenTheInlineVariable_WhenOdrUsed_ThenItLinks)
{
  bool const &a = shared_atomic::is_always_lock_free;
  bool const &b = weak_atomic::is_always_lock_free;
  EXPECT_TRUE (a);
  EXPECT_TRUE (b);
}
} // namespace

#else

TEST (LumexSplitCountOrdersCxx17Test,
      GivenAPlatformWithoutTheEngine_WhenCompiled_ThenSkipped)
{
  GTEST_SKIP () << "the split-count engine needs x86-64 and lumex::smart_ptr";
}

#endif
