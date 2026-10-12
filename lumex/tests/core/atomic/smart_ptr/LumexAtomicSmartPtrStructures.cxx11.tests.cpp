// The classic ABA victims written on top of atomic_shared_ptr: a Treiber
// stack and a Michael and Scott queue whose nodes are std::shared_ptr reached
// through the atomic. They run with many threads and four schedules and check
// the multiset of values (each pushed value comes out exactly once), the sum,
// the FIFO order per producer, a destructor ledger (every node constructed is
// destroyed once) and the allocator's balance (no leak). The same algorithm
// over raw atomic pointers with immediate address reuse is run through the
// forced ABA interleaving and must FAIL, which proves that these checks see
// ABA; the cure (a version tag next to an index) and the shared-pointer stack
// must pass the very same schedule.

#include <string>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/tests/core/atomic/LumexAtomicScenarios.hpp"
#include "lumex/tests/core/atomic/LumexAtomicScenariosStructures.hpp"
#include "lumex/tests/core/atomic/LumexAtomicTestSupport.hpp"
#include "lumex/tests/support/LumexTestThreads.hpp"

using namespace lumex_atomic_test;

namespace
{
void
expect_clean (lumex_test::Verdict const &verdict)
{
  EXPECT_TRUE (verdict.ok ()) << verdict.text ();
}
} // namespace

TEST (
    LumexAtomicSmartPtrStructuresTest,
    GivenATreiberStackOfSharedNodes_WhenThreadsPushAndPop_ThenEveryValueComesOutOnce)
{
  lumex_test::TestWatchdog const dog ("GivenATreiberStackOfSharedNodes");
  lumex_test::Verdict verdict;
  scenario::run_matrix (&scenario::treiber_stress<EngineUnderTest>,
                        lumex_test::scaled (600), verdict);
  expect_clean (verdict);
}

TEST (
    LumexAtomicSmartPtrStructuresTest,
    GivenAMichaelScottQueueOfSharedNodes_WhenProducersAndConsumersRun_ThenEveryItemComesOutOnceInOrder)
{
  lumex_test::TestWatchdog const dog ("GivenAMichaelScottQueueOfSharedNodes");
  lumex_test::Verdict verdict;
  scenario::run_matrix (&scenario::queue_stress<EngineUnderTest>,
                        lumex_test::scaled (500), verdict);
  expect_clean (verdict);
}

TEST (
    LumexAtomicSmartPtrStructuresTest,
    GivenAHarrisMichaelListOfSharedNodes_WhenThreadsInsertRemoveAndLookUp_ThenTheNetInsertsMatchTheList)
{
  // The links are immutable pairs (successor, mark) replaced as a whole, so
  // every compare-exchange compares the identity of a link.
  lumex_test::TestWatchdog const dog ("GivenAHarrisMichaelListOfSharedNodes");
  lumex_test::Verdict verdict;
  scenario::run_matrix (&scenario::list_stress<EngineUnderTest>,
                        lumex_test::scaled (700), verdict);
  expect_clean (verdict);
}

// --- the forced ABA interleaving
// ------------------------------------------------

TEST (
    LumexAtomicSmartPtrStructuresTest,
    GivenTheForcedAbaInterleaving_WhenTheStackIsOfSharedNodes_ThenTheStalledPopFails)
{
  // The stalled popper holds the old head, so its node cannot be recycled
  // and the compare-exchange sees a different owner.
  lumex_test::ObjectLedger ledger (64);
  lumex_test::ReusePool pool;
  lumex_test::Verdict verdict;
  {
    scenario::SharedStack<EngineUnderTest> stack (ledger, pool);
    scenario::treiber_aba_schedule (stack, verdict, "shared stack");
  }
  EngineUnderTest::quiesce ();
  scenario::check_balance (ledger, pool, verdict, "shared stack");
  expect_clean (verdict);
}

TEST (
    LumexAtomicSmartPtrStructuresTest,
    GivenTheForcedAbaInterleaving_WhenTheStackHasAVersionTag_ThenTheStalledPopFails)
{
  scenario::TaggedIndexStack stack;
  lumex_test::Verdict verdict;
  scenario::treiber_aba_schedule (stack, verdict, "tagged stack");
  expect_clean (verdict);
}

TEST (
    LumexAtomicSmartPtrStructuresTest,
    GivenTheForcedAbaInterleaving_WhenTheStackIsOverRawPointers_ThenTheCorruptionIsSeen)
{
  // The negative control: the same algorithm over raw atomic pointers with
  // the free list that returns the freed address at once. The stalled pop
  // succeeds against a recycled address and installs a freed node as the
  // head. The run is safe (see RawPointerStack); the checker must report it.
  scenario::RawPointerStack stack;
  lumex_test::Verdict verdict;
  scenario::treiber_aba_schedule (stack, verdict, "raw stack");
  EXPECT_FALSE (verdict.ok ())
      << "the checks did not see the ABA corruption of the raw stack";
  EXPECT_NE (verdict.text ().find ("freed node"), std::string::npos)
      << verdict.text ();
  EXPECT_NE (verdict.text ().find ("popped twice or lost"), std::string::npos)
      << verdict.text ();
}
