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

// Holders (aliases whose offset from the anchor does not fit in 40 bits):
// the holder never escapes, it is created once per install, it owns one
// reference on the owner and keeps it alive while the word holds it (the
// reference leaves with the word's unit, so a replaced holder word costs the
// owner nothing, and the exchange hand-over gives it to its result), the
// compare-exchange on a holder word compares the
// owner and the pointer, and running out of memory for a holder ends the
// program.

#include <csignal>
#include <cstdint>
#include <cstdio>
#include <vector>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <gtest/gtest.h>

#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountHold.hpp"
#include "lumex/tests/core/atomic/smart_ptr/split_count/LumexSplitCountTestSupport.hpp"

#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT

namespace
{
using namespace split_test;

typedef asp::atomic_shared_ptr_lock_free_split_count<int> int_atomic;

sp::shared_ptr<int>
far_alias (sp::shared_ptr<Obj> const &owner, std::int64_t extra = 0)
{
  return sp::shared_ptr<int> (
      owner, shifted (owner.get (), (std::int64_t (1) << 41) + extra));
}

TEST (LumexSplitCountHolderTest,
      GivenAFarAlias_WhenLoaded_ThenTheOwnerComesBackAndTheHolderStaysHidden)
{
  warm_up_framework ();
  sp::shared_ptr<Obj> owner = sp::make_shared<Obj> (1);
  sp::shared_ptr<int> const alias = far_alias (owner);
  long const live = live_allocations ().load ();
  {
    int_atomic a (alias);
    EXPECT_EQ (owner.use_count (), 3)
        << "owner, alias, the holder's reference";
    sp::shared_ptr<int> loaded = a.load ();
    EXPECT_EQ (loaded.get (), alias.get ());
    EXPECT_EQ (block_of (loaded), block_of (owner))
        << "the loaded pointer owns the owner, never the holder";
    EXPECT_FALSE (loaded.owner_before (alias));
    EXPECT_FALSE (alias.owner_before (loaded));
    EXPECT_EQ (owner.use_count (), 4);
    EXPECT_EQ (words_of (owner).ext, 0);
  }
  EXPECT_EQ (owner.use_count (), 2);
  EXPECT_EQ (live_allocations ().load (), live) << "the holder is gone";
}

TEST (LumexSplitCountHolderTest,
      GivenInstalls_WhenCounted_ThenOneHolderPerInstallAndNoneForLoads)
{
  warm_up_framework ();
  sp::shared_ptr<Obj> owner = sp::make_shared<Obj> (1);
  sp::shared_ptr<int> const alias = far_alias (owner);
  sp::shared_ptr<int> const near_alias (owner, &owner->v);
  int_atomic a;
  unsigned long before = new_calls ();
  a.store (alias);
  expect_allocations (before, 1ul, "a store of a far alias");
  before = new_calls ();
  a.store (alias);
  expect_allocations (before, 1ul,
                      "the same value again is a new install, a new holder");
  EXPECT_EQ (owner.use_count (), 4)
      << "owner, alias, near_alias and the second holder; the first died";
  before = new_calls ();
  for (int i = 0; i < 5; ++i)
    {
      sp::shared_ptr<int> r = a.load ();
      EXPECT_EQ (r.get (), alias.get ());
    }
  expect_allocations (before, 0ul, "loads allocate nothing");
  before = new_calls ();
  sp::shared_ptr<int> old = a.exchange (near_alias);
  expect_allocations (before, 0ul,
                      "an in-window value needs no holder; the hand-over "
                      "allocates nothing");
  EXPECT_EQ (old.get (), alias.get ());
  before = new_calls ();
  sp::shared_ptr<int> expected = near_alias;
  EXPECT_TRUE (a.compare_exchange_strong (expected, alias));
  expect_allocations (before, 1ul, "the desired far alias");
  before = new_calls ();
  expected = alias;
  EXPECT_TRUE (a.compare_exchange_strong (expected, near_alias));
  expect_allocations (before, 0ul,
                      "comparing with a far expected allocates nothing");
  expected = alias;
  before = new_calls ();
  EXPECT_FALSE (a.compare_exchange_strong (expected, alias));
  expect_allocations (before, 0ul, "a failed compare-exchange");
}

TEST (
    LumexSplitCountHolderTest,
    GivenAHolderWord_WhenTheOtherOwnersDie_ThenTheOwnerLivesUntilTheSlotIsCleared)
{
  long const alive = alive_objects ().load ();
  sp::shared_ptr<Obj> owner = sp::make_shared<Obj> (1);
  int_atomic a (far_alias (owner));
  owner.reset ();
  EXPECT_EQ (alive_objects ().load (), alive + 1)
      << "the holder keeps the owner alive";
  sp::shared_ptr<int> r = a.load ();
  a.store (nullptr);
  EXPECT_EQ (alive_objects ().load (), alive + 1) << "the loaded pointer";
  r.reset ();
  EXPECT_EQ (alive_objects ().load (), alive);
}

TEST (LumexSplitCountHolderTest,
      GivenAHolderWord_WhenExchanged_ThenTheResultTakesOverTheHoldersReference)
{
  warm_up_framework ();
  sp::shared_ptr<Obj> owner = sp::make_shared<Obj> (1);
  sp::shared_ptr<Obj> other = sp::make_shared<Obj> (2);
  sp::shared_ptr<int> const alias = far_alias (owner);
  sp::shared_ptr<int> const value (other, &other->v);
  long const live = live_allocations ().load ();
  int_atomic a (alias);
  EXPECT_EQ (owner.use_count (), 3);
  sp::shared_ptr<int> old = a.exchange (value);
  expect_exact (owner, 3, "owner, alias and the returned pointer");
  EXPECT_EQ (old.get (), alias.get ());
  old.reset ();
  expect_exact (owner, 2, "owner and alias");
  EXPECT_EQ (live_allocations ().load (), live) << "the holder was freed";
}

TEST (LumexSplitCountHolderTest,
      GivenAHolderWord_WhenCompareExchangedWithAnotherValue_ThenItFails)
{
  sp::shared_ptr<Obj> owner = sp::make_shared<Obj> (1);
  sp::shared_ptr<Obj> other = sp::make_shared<Obj> (2);
  sp::shared_ptr<int> const alias = far_alias (owner);
  sp::shared_ptr<int> const another_far = far_alias (owner, 8);
  sp::shared_ptr<int> const near_alias (owner, &owner->v);
  sp::shared_ptr<int> const other_far = far_alias (other);
  sp::shared_ptr<int> const none;
  sp::shared_ptr<int> const *const candidates[]
      = { &another_far, &near_alias, &other_far, &none };
  for (std::size_t i = 0; i < sizeof candidates / sizeof candidates[0]; ++i)
    {
      int_atomic a (alias);
      sp::shared_ptr<int> expected = *candidates[i];
      EXPECT_FALSE (a.compare_exchange_strong (expected, none)) << i;
      EXPECT_EQ (expected.get (), alias.get ()) << i;
      EXPECT_EQ (block_of (expected), block_of (owner));
      expected = alias;
      EXPECT_TRUE (a.compare_exchange_strong (expected, none)) << i;
      EXPECT_FALSE (static_cast<bool> (a.load ()));
    }
  expect_exact (owner, 4, "owner, alias, another_far, near_alias");
  expect_exact (other, 2, "other and other_far");
}

TEST (LumexSplitCountHolderTest,
      GivenANullAliasOfALiveOwner_WhenStoredAndLoaded_ThenItKeepsTheOwner)
{
  sp::shared_ptr<Obj> owner = sp::make_shared<Obj> (1);
  sp::shared_ptr<int> const alias (owner, static_cast<int *> (nullptr));
  int_atomic a (alias);
  sp::shared_ptr<int> r = a.load ();
  EXPECT_EQ (r.get (), static_cast<int *> (nullptr));
  EXPECT_EQ (block_of (r), block_of (owner));
  EXPECT_EQ (owner.use_count (), 4) << "owner, alias, the slot, r";
  r.reset ();
  a.store (nullptr);
  expect_exact (owner, 2, "owner and alias");
}

TEST (LumexSplitCountHolderTest,
      GivenASoleOwnerHolderWord_WhenExchanged_ThenTheResultKeepsTheObject)
{
  long const alive = alive_objects ().load ();
  {
    int_atomic a;
    {
      sp::shared_ptr<Obj> owner = sp::make_shared<Obj> (1);
      a.store (far_alias (owner));
    }
    EXPECT_EQ (alive_objects ().load (), alive + 1);
    sp::shared_ptr<int> old = a.exchange (nullptr);
    EXPECT_EQ (alive_objects ().load (), alive + 1) << "the result owns it";
    EXPECT_EQ (old.use_count (), 1)
        << "the hand-over takes the holder's reference over, it adds none";
    old.reset ();
    EXPECT_EQ (alive_objects ().load (), alive);
  }
}

#if defined(__unix__) || defined(__APPLE__)
/// Runs @p body in a forked child and reports whether the child was killed by
/// `SIGABRT` (a `std::terminate`). A forked child is used instead of a gtest
/// death test because the old googletest of the C++11 suites starts its death
/// test children with `clone`, which AddressSanitizer cannot survive.
template <typename Body>
bool
terminates_by_abort (Body const &body)
{
  std::fflush (nullptr);
  pid_t const pid = ::fork ();
  if (pid == 0)
    {
      struct rlimit no_core = { 0, 0 };
      ::setrlimit (RLIMIT_CORE, &no_core);
      body ();
      ::_exit (0);
    }
  int status = 0;
  ::waitpid (pid, &status, 0);
  return WIFSIGNALED (status) && WTERMSIG (status) == SIGABRT;
}

TEST (
    LumexSplitCountHolderNoMemoryTest,
    GivenNoMemoryForAHolder_WhenAFarAliasIsInstalled_ThenTheProgramTerminates)
{
  if (!alloc_hooks_active ())
    GTEST_SKIP () << "the allocation hooks are off under ThreadSanitizer";
  sp::shared_ptr<Obj> owner = sp::make_shared<Obj> (1);
  sp::shared_ptr<int> const alias = far_alias (owner);
  EXPECT_TRUE (terminates_by_abort (
      [&]
        {
          fail_next_new () = true;
          int_atomic a (alias);
        }))
      << "construction";
  EXPECT_TRUE (terminates_by_abort (
      [&]
        {
          int_atomic a;
          fail_next_new () = true;
          a.store (alias);
        }))
      << "store";
  EXPECT_TRUE (terminates_by_abort (
      [&]
        {
          int_atomic a;
          fail_next_new () = true;
          a.exchange (alias);
        }))
      << "exchange";
  EXPECT_TRUE (terminates_by_abort (
      [&]
        {
          int_atomic a;
          sp::shared_ptr<int> expected;
          fail_next_new () = true;
          a.compare_exchange_strong (expected, alias);
        }))
      << "compare-exchange";
  EXPECT_FALSE (terminates_by_abort ([&] { int_atomic a (alias); }))
      << "control: with memory the same code does not terminate";
  EXPECT_EQ (owner.use_count (), 2) << "the parent was not touched";
}
#else
TEST (
    LumexSplitCountHolderNoMemoryTest,
    GivenNoMemoryForAHolder_WhenAFarAliasIsInstalled_ThenTheProgramTerminates)
{
  GTEST_SKIP () << "the check forks a child process (POSIX only)";
}
#endif
} // namespace

#else

TEST (LumexSplitCountHolderTest,
      GivenAPlatformWithoutTheEngine_WhenCompiled_ThenSkipped)
{
  GTEST_SKIP () << "the split-count engine needs x86-64 and lumex::smart_ptr";
}

#endif
