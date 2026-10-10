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

// C++20: the atomic objects are usable as `constinit` variables and in
// constant initialization (LWG 3661), and a `constinit` object holding a
// value is destroyed at exit.

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountTestSupport.hpp"

#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT && defined(__cpp_constinit)

namespace
{
namespace sp = lumex::core::smart_ptr;
namespace asp = lumex::core::atomic::smart_ptr;

constinit std::atomic<long> g_live20 (0);

struct counted20_t
{
  counted20_t () { g_live20.fetch_add (1); }
  counted20_t (counted20_t const &) = delete;
  counted20_t &operator= (counted20_t const &) = delete;
  ~counted20_t () { g_live20.fetch_sub (1); }
};

constinit asp::atomic_shared_ptr_lock_free_split_count<counted20_t> g_default;
constinit asp::atomic_shared_ptr_lock_free_split_count<counted20_t>
    g_from_nullptr (nullptr);
constinit asp::atomic_weak_ptr_lock_free_split_count<counted20_t> g_weak20;

struct exit_check20_t
{
  ~exit_check20_t ()
  {
    if (g_live20.load () != 0)
      {
        std::fprintf (stderr, "constinit atomics: %ld objects alive at exit\n",
                      g_live20.load ());
        std::fflush (stderr);
        std::_Exit (3);
      }
  }
};

// Declared before the constinit atomics that it checks? A constinit variable
// is initialized statically, so it is destroyed in the reverse order of its
// definition among the dynamically-destructed statics: define the checker
// first.
exit_check20_t g_exit_check20;
constinit asp::atomic_shared_ptr_lock_free_split_count<counted20_t> g_holder20;

TEST (LumexSplitCountConstinitCxx20Test,
      GivenConstinitAtomics_WhenUsed_ThenTheyBehaveAndDieAtExit)
{
  EXPECT_FALSE (static_cast<bool> (g_default.load ()));
  EXPECT_FALSE (static_cast<bool> (g_from_nullptr.load ()));
  EXPECT_TRUE (g_weak20.load ().expired ());
  sp::shared_ptr<counted20_t> const value = sp::make_shared<counted20_t> ();
  g_holder20.store (value);
  EXPECT_EQ (g_live20.load (), 1);
  EXPECT_EQ (value.use_count (), 2);
  EXPECT_TRUE (
      std::is_nothrow_default_constructible<
          asp::atomic_shared_ptr_lock_free_split_count<counted20_t>>::value);
  // The value stays in g_holder20 until exit; g_exit_check20 verifies it.
}
} // namespace

#else

TEST (LumexSplitCountConstinitCxx20Test,
      GivenAPlatformWithoutTheEngineOrConstinit_WhenCompiled_ThenSkipped)
{
  GTEST_SKIP () << "needs the split-count engine and constinit";
}

#endif
