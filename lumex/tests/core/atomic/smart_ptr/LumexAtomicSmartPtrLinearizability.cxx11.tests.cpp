// Linearizability-style checks of atomic_shared_ptr. Short concurrent
// histories (2 or 3 threads, 3 to 5 operations each) of load, store,
// exchange and strong and weak compare-exchange are recorded over a value
// universe that includes aliasing pointers (same pointer under two owners,
// one owner under two pointers) and searched by the Wing and Gong algorithm
// for a legal sequential order of a register. Beside it run the invariants
// that need no history and so scale to thousands of operations: the counter
// built from compare-exchange loops (no lost update, no double winner), the
// permutation of exchanged tokens, monotone loads behind a single writer, the
// walk of the read-modify-write operations, and the precision of the strong
// compare-exchange. Every check is a template over the engine
// (LumexAtomicScenarios.hpp); the falsification suite runs them on broken
// engines.

#include <cstdint>

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
    LumexAtomicSmartPtrLinearizabilityTest,
    GivenShortRecordedHistories_WhenSearchedForALegalOrder_ThenEveryOneIsLinearizable)
{
  lumex_test::TestWatchdog const dog ("GivenShortRecordedHistories");
  lumex_test::Verdict verdict;
  scenario::run_matrix_for (scenario::history_thread_counts (),
                            &scenario::linearizable_register<EngineUnderTest>,
                            lumex_test::scaled (150), verdict);
  expect_clean (verdict);
}

TEST (
    LumexAtomicSmartPtrLinearizabilityTest,
    GivenCompareExchangeLoopsOnAVersionChain_WhenAllThreadsIncrement_ThenNoUpdateIsLost)
{
  lumex_test::TestWatchdog const dog (
      "GivenCompareExchangeLoopsOnAVersionChain");
  lumex_test::Verdict verdict;
  scenario::run_matrix (&scenario::cas_counter<EngineUnderTest>,
                        lumex_test::scaled (500), verdict);
  expect_clean (verdict);
}

TEST (LumexAtomicSmartPtrLinearizabilityTest,
      GivenThreadsExchangingUniqueTokens_WhenDone_ThenTheTokensArePermuted)
{
  lumex_test::TestWatchdog const dog ("GivenThreadsExchangingUniqueTokens");
  lumex_test::Verdict verdict;
  scenario::run_matrix (&scenario::exchange_permutation<EngineUnderTest>,
                        lumex_test::scaled (500), verdict);
  expect_clean (verdict);
}

TEST (
    LumexAtomicSmartPtrLinearizabilityTest,
    GivenOneWriterOfIncreasingVersions_WhenReadersLoad_ThenVersionsNeverGoBackwards)
{
  lumex_test::TestWatchdog const dog ("GivenOneWriterOfIncreasingVersions");
  lumex_test::Verdict verdict;
  scenario::run_matrix (&scenario::monotone_loads<EngineUnderTest>,
                        lumex_test::scaled (400), verdict);
  expect_clean (verdict);
}

TEST (
    LumexAtomicSmartPtrLinearizabilityTest,
    GivenReadModifyWriteOperationsOnAliasingValues_WhenAllRun_ThenTheyFormOneWalk)
{
  lumex_test::TestWatchdog const dog ("GivenReadModifyWriteOperations");
  lumex_test::Verdict verdict;
  scenario::run_matrix (&scenario::rmw_walk<EngineUnderTest>,
                        lumex_test::scaled (2000), verdict);
  expect_clean (verdict);
}

TEST (
    LumexAtomicSmartPtrLinearizabilityTest,
    GivenStrongCompareExchangeLoops_WhenContended_ThenFailuresAreRealAndBounded)
{
  lumex_test::TestWatchdog const dog ("GivenStrongCompareExchangeLoops");
  lumex_test::Verdict verdict;
  scenario::run_matrix (&scenario::strong_cas_precision<EngineUnderTest>,
                        lumex_test::scaled (400), verdict);
  expect_clean (verdict);
}

TEST (
    LumexAtomicSmartPtrLinearizabilityTest,
    GivenShortStackHistoriesOverAtomicSharedPointers_WhenSearched_ThenLifoHolds)
{
  lumex_test::TestWatchdog const dog ("GivenShortStackHistories");
  lumex_test::Verdict verdict;
  scenario::run_matrix_for (
      scenario::history_thread_counts (),
      &scenario::structure_linearizable<EngineUnderTest, false>,
      lumex_test::scaled (120), verdict);
  expect_clean (verdict);
}

TEST (
    LumexAtomicSmartPtrLinearizabilityTest,
    GivenShortQueueHistoriesOverAtomicSharedPointers_WhenSearched_ThenFifoHolds)
{
  lumex_test::TestWatchdog const dog ("GivenShortQueueHistories");
  lumex_test::Verdict verdict;
  scenario::run_matrix_for (
      scenario::history_thread_counts (),
      &scenario::structure_linearizable<EngineUnderTest, true>,
      lumex_test::scaled (120), verdict);
  expect_clean (verdict);
}

TEST (
    LumexAtomicSmartPtrLinearizabilityTest,
    GivenShortSortedListHistoriesOverAtomicSharedPointers_WhenSearched_ThenSetSemanticsHold)
{
  lumex_test::TestWatchdog const dog ("GivenShortSortedListHistories");
  lumex_test::Verdict verdict;
  scenario::run_matrix_for (scenario::history_thread_counts (),
                            &scenario::list_linearizable<EngineUnderTest>,
                            lumex_test::scaled (120), verdict);
  expect_clean (verdict);
}
