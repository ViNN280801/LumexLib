// ABA scenarios of atomic_shared_ptr and atomic_weak_ptr. A shared pointer
// that is held keeps its object and control block alive, so ABA cannot happen
// to a caller; it can happen INSIDE an implementation that reads a pointer
// word, is overtaken, and then acts on a word whose target was freed and
// reused. These tests make that overtaking likely and observable without
// touching the library: nodes come from a reuse-address allocator (the
// address of a destroyed node is the address of the next one), threads
// replace, exchange and compare-exchange as fast as they can, and every node
// loaded is checked for a canary, a ledger entry and the identity rule of the
// standard (same stored pointer AND same ownership). The checkers are
// templates over an engine (LumexAtomicScenarios.hpp); here they run on the
// engine under test, the falsification suite runs them on broken ones.
//
// Deterministic cases come first (address reuse forced by the allocator, the
// stalled holder of an expected value), then the schedules (thread counts
// LUMEX_TEST_THREADS, default 1, 2, 4, 8 and above the cores; four schedule
// kinds; the seed LUMEX_TEST_SEED is printed on failure).

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/tests/core/atomic/LumexAtomicScenarios.hpp"
#include "lumex/tests/core/atomic/LumexAtomicScenariosWeak.hpp"
#include "lumex/tests/core/atomic/LumexAtomicTestSupport.hpp"
#include "lumex/tests/support/LumexTestReplay.hpp"
#include "lumex/tests/support/LumexTestThreads.hpp"

using namespace lumex_atomic_test;

namespace
{
typedef scenario::VNode VNode;
typedef scenario::VNodePtr VNodePtr;

/// Fails the test with the verdict's text.
void
expect_clean (lumex_test::Verdict const &verdict)
{
  EXPECT_TRUE (verdict.ok ()) << verdict.text ();
}
} // namespace

// --- deterministic: the address comes back
// -------------------------------------

TEST (
    LumexAtomicSmartPtrAbaTest,
    GivenADeadNodesAddressReused_WhenCompareExchangeFromAHandleToThatAddress_ThenItFails)
{
  lumex_test::Verdict verdict;
  scenario::reuse_equivalence<EngineUnderTest> (verdict);
  expect_clean (verdict);
}

TEST (
    LumexAtomicSmartPtrAbaTest,
    GivenAToBAToA_WhenTheSameOwnerReturns_ThenCompareExchangeSucceedsAndANewObjectFails)
{
  lumex_test::Verdict verdict;
  scenario::aba_same_owner<EngineUnderTest> (verdict);
  expect_clean (verdict);
}

TEST (
    LumexAtomicSmartPtrAbaTest,
    GivenEveryPairOfIdentities_WhenCompareExchange_ThenOnlyTheEquivalentOnesMatch)
{
  lumex_test::Verdict verdict;
  scenario::identity_rule<EngineUnderTest> (verdict);
  expect_clean (verdict);
}

TEST (LumexAtomicSmartPtrAbaTest,
      GivenTheSameOwnerStoredAgain_WhenReplaced_ThenNoReferenceIsLeaked)
{
  // store () of the very pointer that is already there (the same address
  // again) must release the one it replaces, however often it is repeated.
  lumex_test::ObjectLedger ledger (16);
  lumex_test::ReusePool pool;
  {
    VNodePtr const a_owner = lumex_test::make_pooled<VNode> (pool, ledger, 1);
    VNodePtr const b_owner = lumex_test::make_pooled<VNode> (pool, ledger, 2);
    atomic_shared_ptr<VNode> a (a_owner);
    for (int i = 0; i < 6; ++i)
      a.store (a_owner);
    EngineUnderTest::quiesce ();
    if (EngineUnderTest::replaced_value_dies_in_call ())
      {
        EXPECT_EQ (a_owner.use_count (), 2L)
            << "one for the test, one for the atomic";
      }
    for (int i = 0; i < 3; ++i)
      {
        VNodePtr const old = a.exchange (a_owner);
        EXPECT_TRUE (equivalent_values (old, a_owner));
      }
    a.store (b_owner);
    EngineUnderTest::quiesce ();
    if (EngineUnderTest::replaced_value_dies_in_call ())
      {
        EXPECT_EQ (a_owner.use_count (), 1L);
      }
  }
  EngineUnderTest::quiesce ();
  EXPECT_TRUE (ledger.balanced ()) << ledger.report ();
  EXPECT_EQ (pool.live (), 0L);
}

TEST (
    LumexAtomicSmartPtrAbaTest,
    GivenAStalledHolderOfExpected_WhenAnotherThreadRunsAToBToA_ThenOwnershipDecides)
{
  // The holder loads A and is held back (a gate, outside the library) while
  // another thread replaces A by B and puts A back, 0 to 5 redundant stores
  // in between. Then it compares: with the original A back the compare must
  // succeed, with a new object A' (equal content, other address and owner)
  // it must fail and report A'.
  for (int redundant = 0; redundant <= 5; ++redundant)
    for (int variant = 0; variant < 2; ++variant)
      {
        lumex_test::ObjectLedger ledger (64);
        lumex_test::ReusePool pool;
        lumex_test::Gate loaded;
        lumex_test::Gate go;
        bool succeeded = false;
        bool refreshed_to_new = false;
        {
          VNodePtr const a_owner
              = lumex_test::make_pooled<VNode> (pool, ledger, 7);
          VNodePtr const b_owner
              = lumex_test::make_pooled<VNode> (pool, ledger, 8);
          atomic_shared_ptr<VNode> a (a_owner);
          VNodePtr replacement;
          std::thread holder (
              [&]
                {
                  VNodePtr expected = a.load ();
                  loaded.open ();
                  go.wait ();
                  succeeded = a.compare_exchange_strong (
                      expected,
                      lumex_test::make_pooled<VNode> (pool, ledger, 99));
                  refreshed_to_new
                      = !succeeded && expected->version == 7 && replacement
                        && equivalent_values (expected, replacement);
                });
          loaded.wait ();
          a.store (b_owner);
          for (int i = 0; i < redundant; ++i)
            a.store (b_owner);
          if (variant == 0)
            a.store (a_owner);
          else
            {
              replacement = lumex_test::make_pooled<VNode> (pool, ledger, 7);
              a.store (replacement);
            }
          for (int i = 0; i < redundant; ++i)
            a.store (variant == 0 ? a_owner : replacement);
          go.open ();
          holder.join ();
        }
        EngineUnderTest::quiesce ();
        if (variant == 0)
          EXPECT_TRUE (succeeded)
              << "A -> B -> A with the same owner, redundant=" << redundant;
        else
          {
            EXPECT_FALSE (succeeded)
                << "A -> B -> A' must not match, redundant=" << redundant;
            EXPECT_TRUE (refreshed_to_new)
                << "expected must hold A' after the failure";
          }
        EXPECT_TRUE (ledger.balanced ()) << ledger.report ();
        EXPECT_EQ (pool.live (), 0L);
      }
}

TEST (LumexAtomicSmartPtrAbaTest,
      GivenAWeakOwnerThatExpired_WhenCompared_ThenTheOwnerNotTheObjectDecides)
{
  lumex_test::Verdict verdict;
  scenario::weak_owner_identity<EngineUnderTest> (verdict);
  expect_clean (verdict);
}

TEST (
    LumexAtomicSmartPtrAbaTest,
    GivenAnObjectAddressReusedUnderAPinnedControlBlock_WhenWeakPointersAreCompared_ThenTheOwnerDecides)
{
  lumex_test::Verdict verdict;
  scenario::weak_split_allocation_identity<EngineUnderTest> (verdict);
  expect_clean (verdict);
}

// --- the schedules
// --------------------------------------------------------------

TEST (
    LumexAtomicSmartPtrAbaTest,
    GivenNodesFromAReusePool_WhenManyThreadsReplaceExchangeAndCompare_ThenNoNodeIsStaleOrCorrupt)
{
  lumex_test::TestWatchdog const dog ("GivenNodesFromAReusePool");
  lumex_test::Verdict verdict;
  scenario::run_matrix (&scenario::reuse_stress<EngineUnderTest>,
                        lumex_test::scaled (600), verdict);
  expect_clean (verdict);
}

TEST (
    LumexAtomicSmartPtrAbaTest,
    GivenAllocatorBoundariesThatYield_WhenNodesAreRecycledAtOnce_ThenNoNodeIsStaleOrCorrupt)
{
  // The pool's release and allocation hooks yield now and then, so a thread
  // is overtaken exactly where a node changes hands.
  lumex_test::TestWatchdog const dog ("GivenAllocatorBoundariesThatYield");
  lumex_test::ObjectLedger ledger (1u << 20);
  lumex_test::ReusePool pool;
  std::atomic<unsigned> tick (0);
  pool.set_hook (
      [&] (lumex_test::PoolEvent event, void *)
        {
          if (event == lumex_test::PoolEvent::before_allocate
              || event == lumex_test::PoolEvent::after_release)
            if ((tick.fetch_add (1) & 7u) == 0u)
              std::this_thread::yield ();
        });
  lumex_test::Verdict verdict;
  std::atomic<int> serial (0);
  int const iterations = lumex_test::scaled (1500);
  std::vector<int> const counts = lumex_test::thread_counts ();
  for (std::size_t c = 0; c < counts.size (); ++c)
    {
      {
        atomic_shared_ptr<VNode> a (lumex_test::make_pooled<VNode> (
            pool, ledger, serial.fetch_add (1)));
        std::string const error = lumex_test::run_threads (
            counts[c],
            [&] (int t)
              {
                lumex_test::SeededRandom random (lumex_test::derive_seed (
                    lumex_test::base_seed (), static_cast<std::uint64_t> (t),
                    21));
                for (int i = 0; i < iterations; ++i)
                  {
                    VNodePtr seen = a.load ();
                    scenario::check_node (seen, verdict, "allocator yield");
                    VNodePtr fresh = lumex_test::make_pooled<VNode> (
                        pool, ledger, serial.fetch_add (1));
                    if (random.below (2u) == 0u)
                      a.compare_exchange_weak (seen, fresh);
                    else
                      a.store (fresh);
                  }
              });
        verdict.require (error.empty (), error);
      }
      EngineUnderTest::quiesce ();
    }
  pool.set_hook (lumex_test::ReusePool::hook_type ());
  scenario::check_balance (ledger, pool, verdict, "allocator yield");
  expect_clean (verdict);
  if (EngineUnderTest::replaced_value_dies_in_call ())
    {
      EXPECT_GT (pool.reuses (), 0L);
    }
}

TEST (
    LumexAtomicSmartPtrAbaTest,
    GivenAliasingPointersOfSeveralOwners_WhenExchangedAndCompared_ThenTheWalkOfValuesIsUnbroken)
{
  lumex_test::TestWatchdog const dog ("GivenAliasingPointersOfSeveralOwners");
  lumex_test::Verdict verdict;
  scenario::run_matrix (&scenario::rmw_walk<EngineUnderTest>,
                        lumex_test::scaled (1500), verdict);
  expect_clean (verdict);
}

// --- weak-to-shared promotion
// ---------------------------------------------------------

TEST (
    LumexAtomicSmartPtrAbaTest,
    GivenLockersAndAnOwner_WhenTheLastStrongReferenceGoes_ThenPromotionIsLiveOrNull)
{
  lumex_test::TestWatchdog const dog ("GivenLockersAndAnOwner");
  lumex_test::Verdict verdict;
  scenario::run_matrix (&scenario::promotion_race<EngineUnderTest, true>,
                        lumex_test::scaled (400), verdict);
  expect_clean (verdict);
}

TEST (
    LumexAtomicSmartPtrAbaTest,
    GivenTheWeakPointerPublishedAfterTheStrongOne_WhenLockRacesStore_ThenPromotionIsLiveOrNull)
{
  lumex_test::TestWatchdog const dog (
      "GivenTheWeakPointerPublishedAfterTheStrongOne");
  lumex_test::Verdict verdict;
  scenario::run_matrix (&scenario::promotion_race<EngineUnderTest, false>,
                        lumex_test::scaled (400), verdict);
  expect_clean (verdict);
}

TEST (
    LumexAtomicSmartPtrAbaTest,
    GivenObserversOfAnExpiringPointer_WhenTheOwnerLetsGo_ThenExpiryIsMonotone)
{
  lumex_test::TestWatchdog const dog ("GivenObserversOfAnExpiringPointer");
  lumex_test::Verdict verdict;
  scenario::run_matrix (&scenario::expired_race<EngineUnderTest>,
                        lumex_test::scaled (200), verdict);
  expect_clean (verdict);
}
