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

// Static storage: an atomic object is constant-initialized (so a dynamic
// initializer that runs before its own definition can already use it) and it
// is destroyed at exit with the right counts. The check at exit runs in the
// destructor of a static declared BEFORE the atomic objects, which is
// destroyed after them; it ends the process with a non-zero status when an
// object is still alive.

#include <atomic>
#include <cstdio>
#include <cstdlib>

#include <gtest/gtest.h>

#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountTestSupport.hpp"

#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

namespace
{
namespace sp = lumex::core::smart_ptr;
namespace asp = lumex::core::atomic::smart_ptr;

// Constant-initialized, trivially destructible: usable at any time.
std::atomic<long> g_live (0);

struct counted_t
{
  counted_t () { g_live.fetch_add (1); }
  counted_t (counted_t const &) = delete;
  counted_t &operator= (counted_t const &) = delete;
  ~counted_t () { g_live.fetch_sub (1); }
  int v = 0;
};

typedef asp::atomic_shared_ptr_lock_free_split_count<counted_t> counted_atomic;
typedef asp::atomic_weak_ptr_lock_free_split_count<counted_t> counted_weak;

/// Runs after the destruction of every atomic object below.
struct exit_check_t
{
  ~exit_check_t ()
  {
    if (g_live.load () != 0)
      {
        std::fprintf (
            stderr, "split-count static storage: %ld objects alive at exit\n",
            g_live.load ());
        std::fflush (stderr);
        std::_Exit (3);
      }
  }
} g_exit_check;

extern counted_atomic g_early;

/// A dynamic initializer that uses `g_early` before its definition below: it
/// works only if `g_early` was constant-initialized.
struct early_user_t
{
  early_user_t () { g_early.store (sp::make_shared<counted_t> ()); }
} g_early_user;

counted_atomic g_early;
counted_atomic g_static;
counted_atomic g_from_null (nullptr);
counted_weak g_weak;

TEST (
    LumexSplitCountConstinitTest,
    GivenAStaticAtomicUsedByAnEarlierInitializer_WhenMainRuns_ThenTheValueSurvived)
{
  sp::shared_ptr<counted_t> const early = g_early.load ();
  EXPECT_TRUE (static_cast<bool> (early))
      << "g_early was constant-initialized; its own initializer did not "
         "overwrite the value stored before it";
  EXPECT_EQ (early.use_count (), 2) << "the slot and early";
}

TEST (
    LumexSplitCountConstinitTest,
    GivenStaticAtomics_WhenMainEndsWithValuesInThem_ThenTheyAreDestroyedAtExit)
{
  sp::shared_ptr<counted_t> const strong = sp::make_shared<counted_t> ();
  g_static.store (strong);
  g_weak.store (sp::weak_ptr<counted_t> (strong));
  g_from_null.store (sp::make_shared<counted_t> ());
  EXPECT_GE (g_live.load (), 3);
  EXPECT_EQ (strong.use_count (), 2);
  // No reset: the values stay in the static atomics, and the check in
  // `g_exit_check` fails the process if exit does not destroy them.
}
} // namespace

#else

TEST (LumexSplitCountConstinitTest,
      GivenAPlatformWithoutTheEngine_WhenCompiled_ThenSkipped)
{
  GTEST_SKIP () << "the split-count engine needs x86-64 and lumex::smart_ptr";
}

#endif
