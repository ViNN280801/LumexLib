// Ownership and lifetime: every object handed to an atomic smart pointer is
// deleted exactly once and only after its last owner is gone, deleters run
// after the internal lock is released (so a deleter may use the same atomic
// object), and weak references never keep a control block longer than
// needed. Extends the refcount test of the libc++ implementation of
// llvm-project pull request 194215.

#include <atomic>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/tests/core/atomic/LumexAtomicTestSupport.hpp"

using namespace lumex_atomic_test;

namespace
{
/// What a reentrant deleter does with the atomic object.
enum class Reentry
{
  load_and_store,
  exchange,
  compare_exchange,
  wait_and_notify
};

/// Deleter that uses the atomic object which may have held the pointer.
struct ReentrantDeleter
{
  atomic_shared_ptr<int> *watch;
  std::atomic<int> *calls;
  Reentry action;

  void
  operator() (int *p) const
  {
    switch (action)
      {
      case Reentry::load_and_store:
        {
          std::shared_ptr<int> const current = watch->load ();
          watch->store (current);
          break;
        }
      case Reentry::exchange:
        {
          std::shared_ptr<int> const current = watch->load ();
          std::shared_ptr<int> const previous = watch->exchange (current);
          break;
        }
      case Reentry::compare_exchange:
        {
          std::shared_ptr<int> expected = watch->load ();
          watch->compare_exchange_strong (expected, expected);
          break;
        }
      case Reentry::wait_and_notify:
        {
          // A non-equivalent value: wait returns at once.
          watch->wait (std::make_shared<int> (-1));
          watch->notify_all ();
          break;
        }
      }
    calls->fetch_add (1);
    delete p;
  }
};

std::shared_ptr<int>
make_reentrant (atomic_shared_ptr<int> &watch, std::atomic<int> &calls,
                Reentry action, int value)
{
  ReentrantDeleter const deleter = { &watch, &calls, action };
  return std::shared_ptr<int> (new int (value), deleter);
}

std::vector<Reentry>
every_reentry ()
{
  std::vector<Reentry> actions;
  actions.push_back (Reentry::load_and_store);
  actions.push_back (Reentry::exchange);
  actions.push_back (Reentry::compare_exchange);
  actions.push_back (Reentry::wait_and_notify);
  return actions;
}
} // namespace

// --- atomic_shared_ptr_refcount.pass.cpp
// --------------------------------------

TEST (LumexAtomicSharedPtrLifetimeTest,
      GivenTrackers_WhenStoreReplacesThem_ThenOnlyTheHeldOneIsAlive)
{
  LUMEX_ATOMIC_TEST_REQUIRE_PROMPT_DESTRUCTION ();
  int const before = Tracker::alive ().load ();
  {
    atomic_shared_ptr<Tracker> a;
    a.store (std::make_shared<Tracker> (1));
    EXPECT_EQ (Tracker::alive ().load (), before + 1);
    a.store (std::make_shared<Tracker> (2));
    EXPECT_EQ (Tracker::alive ().load (), before + 1);
    a.store (nullptr);
    EXPECT_EQ (Tracker::alive ().load (), before);
  }
  EXPECT_EQ (Tracker::alive ().load (), before);
}

TEST (LumexAtomicSharedPtrLifetimeTest,
      GivenAnExchange_WhenOwnershipIsHandedOver_ThenCountsArePreserved)
{
  LUMEX_ATOMIC_TEST_REQUIRE_PROMPT_DESTRUCTION ();
  int const before = Tracker::alive ().load ();
  {
    std::shared_ptr<Tracker> const first = std::make_shared<Tracker> (1);
    atomic_shared_ptr<Tracker> a (first);
    EXPECT_EQ (first.use_count (), 2L);
    std::shared_ptr<Tracker> const second = std::make_shared<Tracker> (2);
    std::shared_ptr<Tracker> const old = a.exchange (second);
    EXPECT_EQ (old.get (), first.get ());
    EXPECT_EQ (first.use_count (), 2L);
    EXPECT_EQ (second.use_count (), 2L);
    EXPECT_EQ (Tracker::alive ().load (), before + 2);
  }
  EXPECT_EQ (Tracker::alive ().load (), before);
}

TEST (LumexAtomicSharedPtrLifetimeTest,
      GivenCompareExchangeOutcomes_WhenRepeated_ThenOwnershipStaysBalanced)
{
  LUMEX_ATOMIC_TEST_REQUIRE_PROMPT_DESTRUCTION ();
  int const before = Tracker::alive ().load ();
  {
    std::shared_ptr<Tracker> const p1 = std::make_shared<Tracker> (1);
    std::shared_ptr<Tracker> const p2 = std::make_shared<Tracker> (2);
    atomic_shared_ptr<Tracker> a (p1);
    for (int i = 0; i < 100; ++i)
      {
        std::shared_ptr<Tracker> expected = p2;
        EXPECT_FALSE (a.compare_exchange_strong (expected, p1));
        expected = p1;
        EXPECT_TRUE (a.compare_exchange_strong (expected, p2));
        expected = p2;
        EXPECT_TRUE (a.compare_exchange_weak (expected, p1)
                     || a.compare_exchange_strong (expected, p1));
      }
    EXPECT_EQ (p1.use_count (), 2L);
    EXPECT_EQ (p2.use_count (), 1L);
  }
  EXPECT_EQ (Tracker::alive ().load (), before);
}

TEST (LumexAtomicSharedPtrLifetimeTest,
      GivenAStoredOwner_WhenTheAtomicIsDestroyed_ThenTheOwnerIsReleased)
{
  std::shared_ptr<Tracker> const p = std::make_shared<Tracker> (1);
  {
    atomic_shared_ptr<Tracker> const a (p);
    EXPECT_EQ (p.use_count (), 2L);
  }
  EXPECT_EQ (p.use_count (), 1L);
}

// --- deletion ledger: exactly once, never early
// --------------------------------

TEST (LumexAtomicSharedPtrLifetimeTest,
      GivenEveryReplacingOperation_WhenDone_ThenEachObjectIsDeletedOnce)
{
  LUMEX_ATOMIC_TEST_REQUIRE_PROMPT_DESTRUCTION ();
  DeletionLedger ledger (32);
  {
    atomic_shared_ptr<int> a (ledger.make ()); // slot 0
    a.store (ledger.make ());                  // 1, deletes 0
    EXPECT_EQ (ledger.deletions (0), 1);
    a = ledger.make (); // 2, deletes 1
    EXPECT_EQ (ledger.deletions (1), 1);
    {
      std::shared_ptr<int> const old = a.exchange (ledger.make ()); // 3
      EXPECT_EQ (ledger.deletions (2), 0) << "the exchange result holds it";
    }
    EXPECT_EQ (ledger.deletions (2), 1);
    {
      std::shared_ptr<int> expected = a.load ();
      EXPECT_TRUE (a.compare_exchange_strong (expected, ledger.make ())); // 4
      EXPECT_EQ (ledger.deletions (3), 0) << "expected still owns slot 3";
    }
    EXPECT_EQ (ledger.deletions (3), 1);
    {
      std::shared_ptr<int> stale = ledger.make ();                      // 5
      EXPECT_FALSE (a.compare_exchange_strong (stale, ledger.make ())); // 6
      EXPECT_EQ (ledger.deletions (5), 1) << "replaced in expected";
      EXPECT_EQ (ledger.deletions (6), 1) << "the unused desired value";
    }
    a = nullptr; // deletes 4
    EXPECT_EQ (ledger.deletions (4), 1);
    a.store (ledger.make ()); // 7
  }
  EXPECT_EQ (ledger.deletions (7), 1) << "deleted with the atomic";
  EXPECT_EQ (ledger.deleted_twice (), 0u);
  EXPECT_EQ (ledger.not_deleted (), 0u);
}

TEST (LumexAtomicSharedPtrLifetimeTest,
      GivenALongChainOfReplacements_WhenDone_ThenNothingLeaks)
{
  LUMEX_ATOMIC_TEST_REQUIRE_PROMPT_DESTRUCTION ();
  int const before = Tracker::alive ().load ();
  long const allocations_before = live_allocations ().load ();
  {
    atomic_shared_ptr<Tracker> a;
    atomic_weak_ptr<Tracker> w;
    for (int i = 0; i < 20000; ++i)
      {
        a.store (make_counted_tracker (i));
        w.store (a.load ());
        if ((i & 7) == 0)
          {
            std::shared_ptr<Tracker> expected = a.load ();
            a.compare_exchange_strong (expected, make_counted_tracker (-i));
          }
      }
    EXPECT_LE (Tracker::alive ().load (), before + 1);
  }
  EXPECT_EQ (Tracker::alive ().load (), before);
  EXPECT_EQ (live_allocations ().load (), allocations_before);
}

// --- deleters run after the lock
// --------------------------------------------------

TEST (LumexAtomicSharedPtrLifetimeTest,
      GivenADeleterThatUsesTheAtomic_WhenStoreReleasesIt_ThenNoDeadlock)
{
  LUMEX_ATOMIC_TEST_REQUIRE_PROMPT_DESTRUCTION ();
  Watchdog const dog ("GivenADeleterThatUsesTheAtomic_WhenStoreReleasesIt");
  std::vector<Reentry> const actions = every_reentry ();
  for (std::size_t i = 0; i < actions.size (); ++i)
    {
      std::atomic<int> calls (0);
      atomic_shared_ptr<int> a;
      a.store (make_reentrant (a, calls, actions[i], 7));
      a.store (std::make_shared<int> (8)); // releases the last owner
      EXPECT_EQ (calls.load (), 1) << "action " << i;
      EXPECT_EQ (*a.load (), 8);
    }
}

TEST (LumexAtomicSharedPtrLifetimeTest,
      GivenADeleterThatUsesTheAtomic_WhenAssignmentReleasesIt_ThenNoDeadlock)
{
  LUMEX_ATOMIC_TEST_REQUIRE_PROMPT_DESTRUCTION ();
  Watchdog const dog ("GivenADeleterThatUsesTheAtomic_WhenAssignment");
  std::vector<Reentry> const actions = every_reentry ();
  for (std::size_t i = 0; i < actions.size (); ++i)
    {
      std::atomic<int> calls (0);
      atomic_shared_ptr<int> a;
      a = make_reentrant (a, calls, actions[i], 7);
      a = nullptr;
      EXPECT_EQ (calls.load (), 1) << "action " << i;
    }
}

TEST (LumexAtomicSharedPtrLifetimeTest,
      GivenADeleterThatUsesTheAtomic_WhenTheExchangeResultDies_ThenNoDeadlock)
{
  LUMEX_ATOMIC_TEST_REQUIRE_PROMPT_DESTRUCTION ();
  Watchdog const dog ("GivenADeleterThatUsesTheAtomic_WhenExchange");
  std::vector<Reentry> const actions = every_reentry ();
  for (std::size_t i = 0; i < actions.size (); ++i)
    {
      std::atomic<int> calls (0);
      atomic_shared_ptr<int> a;
      a.store (make_reentrant (a, calls, actions[i], 7));
      a.exchange (std::make_shared<int> (9)); // the result dies here
      EXPECT_EQ (calls.load (), 1) << "action " << i;
    }
}

TEST (
    LumexAtomicSharedPtrLifetimeTest,
    GivenADeleterThatUsesTheAtomic_WhenFailedCompareDropsDesired_ThenNoDeadlock)
{
  Watchdog const dog ("GivenADeleterThatUsesTheAtomic_WhenFailedCompare");
  std::vector<Reentry> const actions = every_reentry ();
  for (std::size_t i = 0; i < actions.size (); ++i)
    {
      std::atomic<int> calls (0);
      atomic_shared_ptr<int> a (std::make_shared<int> (1));
      std::shared_ptr<int> stale = std::make_shared<int> (2);
      EXPECT_FALSE (a.compare_exchange_strong (
          stale, make_reentrant (a, calls, actions[i], 3)));
      EXPECT_EQ (calls.load (), 1) << "action " << i;
    }
}

TEST (LumexAtomicSharedPtrLifetimeTest,
      GivenADeleterThatUsesTheAtomic_WhenFailedCompareReplacesExpected_ThenOk)
{
  Watchdog const dog ("GivenADeleterThatUsesTheAtomic_WhenReplacesExpected");
  std::vector<Reentry> const actions = every_reentry ();
  for (std::size_t i = 0; i < actions.size (); ++i)
    {
      std::atomic<int> calls (0);
      atomic_shared_ptr<int> a (std::make_shared<int> (1));
      std::shared_ptr<int> expected = make_reentrant (a, calls, actions[i], 4);
      EXPECT_FALSE (
          a.compare_exchange_weak (expected, std::make_shared<int> (5)));
      EXPECT_EQ (calls.load (), 1) << "action " << i;
      EXPECT_EQ (*expected, 1);
    }
}

TEST (
    LumexAtomicSharedPtrLifetimeTest,
    GivenADeleterThatUsesTheAtomic_WhenSuccessfulCompareReleases_ThenNoDeadlock)
{
  LUMEX_ATOMIC_TEST_REQUIRE_PROMPT_DESTRUCTION ();
  Watchdog const dog ("GivenADeleterThatUsesTheAtomic_WhenSuccessfulCompare");
  std::vector<Reentry> const actions = every_reentry ();
  for (std::size_t i = 0; i < actions.size (); ++i)
    {
      std::atomic<int> calls (0);
      atomic_shared_ptr<int> a;
      a.store (make_reentrant (a, calls, actions[i], 6));
      {
        std::shared_ptr<int> expected = a.load ();
        EXPECT_TRUE (
            a.compare_exchange_strong (expected, std::make_shared<int> (7)));
        EXPECT_EQ (calls.load (), 0) << "expected still owns it";
      }
      EXPECT_EQ (calls.load (), 1) << "action " << i;
    }
}

// --- weak references and control blocks
// -------------------------------------------

TEST (LumexAtomicWeakPtrLifetimeTest,
      GivenAStoredWeakPointer_WhenTheAtomicIsDestroyed_ThenTheBlockIsFreed)
{
  long const before = live_allocations ().load ();
  {
    std::shared_ptr<Tracker> owner = make_counted_tracker (1);
    atomic_weak_ptr<Tracker> a ((std::weak_ptr<Tracker> (owner)));
    owner.reset ();
    EXPECT_TRUE (a.load ().expired ());
    EXPECT_EQ (live_allocations ().load (), before + 1)
        << "the weak reference keeps the control block";
  }
  EXPECT_EQ (live_allocations ().load (), before);
}

TEST (LumexAtomicWeakPtrLifetimeTest,
      GivenAWeakPointerReplacedByStore_WhenTheObjectIsGone_ThenBlockIsFreed)
{
  LUMEX_ATOMIC_TEST_REQUIRE_PROMPT_DESTRUCTION ();
  long const before = live_allocations ().load ();
  std::shared_ptr<Tracker> const keep = make_counted_tracker (2);
  atomic_weak_ptr<Tracker> a;
  {
    std::shared_ptr<Tracker> const first = make_counted_tracker (1);
    a.store (std::weak_ptr<Tracker> (first));
  }
  EXPECT_EQ (live_allocations ().load (), before + 2)
      << "the stored weak reference keeps the first control block";
  a.store (std::weak_ptr<Tracker> (keep));
  EXPECT_EQ (live_allocations ().load (), before + 1)
      << "store released the previous weak reference";
}

TEST (LumexAtomicWeakPtrLifetimeTest,
      GivenEveryWeakOperation_WhenDone_ThenNoWeakReferenceLeaks)
{
  LUMEX_ATOMIC_TEST_REQUIRE_PROMPT_DESTRUCTION ();
  long const before = live_allocations ().load ();
  int const alive_before = Tracker::alive ().load ();
  {
    std::shared_ptr<Tracker> const p1 = make_counted_tracker (1);
    std::shared_ptr<Tracker> const p2 = make_counted_tracker (2);
    atomic_weak_ptr<Tracker> a ((std::weak_ptr<Tracker> (p1)));
    a.store (std::weak_ptr<Tracker> (p2));
    a = std::weak_ptr<Tracker> (p1);
    std::weak_ptr<Tracker> const old
        = a.exchange (std::weak_ptr<Tracker> (p2));
    std::weak_ptr<Tracker> expected = p2;
    EXPECT_TRUE (
        a.compare_exchange_strong (expected, std::weak_ptr<Tracker> (p1)));
    expected = p2;
    EXPECT_FALSE (
        a.compare_exchange_strong (expected, std::weak_ptr<Tracker> (p2)));
    bool exchanged = false;
    while (!exchanged)
      exchanged
          = a.compare_exchange_weak (expected, std::weak_ptr<Tracker> (p2));
    std::weak_ptr<Tracker> const converted = a;
    a.wait (std::weak_ptr<Tracker> (p1));
    a.notify_all ();
    EXPECT_EQ (live_allocations ().load (), before + 2);
    EXPECT_EQ (p1.use_count (), 1L);
    EXPECT_EQ (p2.use_count (), 1L);
  }
  EXPECT_EQ (live_allocations ().load (), before);
  EXPECT_EQ (Tracker::alive ().load (), alive_before);
}

TEST (LumexAtomicWeakPtrLifetimeTest,
      GivenAnExpiredStoredPointer_WhenCompareExchangedAway_ThenBlockIsFreed)
{
  LUMEX_ATOMIC_TEST_REQUIRE_PROMPT_DESTRUCTION ();
  long const before = live_allocations ().load ();
  {
    atomic_weak_ptr<Tracker> a;
    {
      std::shared_ptr<Tracker> const p = make_counted_tracker (1);
      a.store (std::weak_ptr<Tracker> (p));
    }
    EXPECT_TRUE (a.load ().expired ());
    EXPECT_EQ (live_allocations ().load (), before + 1);
    {
      std::weak_ptr<Tracker> expected = a.load ();
      EXPECT_TRUE (
          a.compare_exchange_strong (expected, std::weak_ptr<Tracker> ()));
      EXPECT_EQ (live_allocations ().load (), before + 1)
          << "expected still holds the weak reference";
    }
    EXPECT_EQ (live_allocations ().load (), before);
  }
  EXPECT_EQ (live_allocations ().load (), before);
}
