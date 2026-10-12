// Self-tests of the concurrency checkers: each checker of the suites
// (LumexAtomicScenarios*.hpp) is run on deliberately broken copies of an
// atomic shared pointer (LumexAtomicTestEngines.hpp) and must report the
// defect, and on correct engines other than the library's (the mutex
// reference and a lock-free-shaped box engine that destroys replaced values
// late) and must stay silent. This is the mutation testing of the test code
// that the library itself may not be mutated for: without it a green suite
// could be a suite that cannot see.
//
// The broken engines that only misbehave logically (a split lock, a wrong
// equivalence, a store that keeps the old value, a stale load) run in the
// process. The ones that are undefined behavior by design (a compare-
// exchange without a lock, a lock-free box that frees at once and is
// read by others) run in a child process: a crash, a sanitizer report or a
// failed check of the child is the expected result. Where the platform has
// no fork those tests are skipped.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/tests/core/atomic/LumexAtomicScenarioBundle.hpp"
#include "lumex/tests/core/atomic/LumexAtomicScenarios.hpp"
#include "lumex/tests/core/atomic/LumexAtomicScenariosGap.hpp"
#include "lumex/tests/core/atomic/LumexAtomicScenariosLifecycle.hpp"
#include "lumex/tests/core/atomic/LumexAtomicScenariosStructures.hpp"
#include "lumex/tests/core/atomic/LumexAtomicTestEngines.hpp"
#include "lumex/tests/core/atomic/LumexAtomicTestSupport.hpp"
#include "lumex/tests/support/LumexTestSubprocess.hpp"
#include "lumex/tests/support/LumexTestThreads.hpp"

using namespace lumex_atomic_test;

namespace
{
using lumex_atomic_test::Flaw;
using lumex_atomic_test::FlawedEngine;

/// Asserts that a checker, called as `call`, reported a violation. With
/// LUMEX_TEST_VERBOSE set the report is printed, to show what was seen.
#define LUMEX_EXPECT_CAUGHT(engine_name, call)                                \
  do                                                                          \
    {                                                                         \
      lumex_test::Verdict verdict_under_test;                                 \
      call;                                                                   \
      EXPECT_FALSE (verdict_under_test.ok ())                                 \
          << (engine_name)                                                    \
          << ": the checker " #call " did not see the defect";                \
      if (lumex_test::env_flag ("LUMEX_TEST_VERBOSE"))                        \
        std::printf ("  caught on %s by %s: %s\n", (engine_name), #call,      \
                     verdict_under_test.text ().c_str ());                    \
    }                                                                         \
  while (false)

/// Widens the window between the comparison and the act of the engines that
/// have one: yields now and then and sleeps a little on every fifth call.
class WideningGap
{
public:
  WideningGap () : calls_ (0)
  {
    EngineHooks::cas_gap () = [this]
      {
        unsigned const call = calls_.fetch_add (1);
        if (call % 5u == 0u)
          std::this_thread::sleep_for (std::chrono::microseconds (40));
        else
          std::this_thread::yield ();
      };
  }

  ~WideningGap () { EngineHooks::cas_gap () = std::function<void ()> (); }

  WideningGap (WideningGap const &) = delete;
  WideningGap &operator= (WideningGap const &) = delete;

private:
  std::atomic<unsigned> calls_;
};

/**
 * Runs @p run (which returns 0 when its checker found nothing) in a child
 * process up to @p attempts times with different seeds; true as soon as one
 * child crashed, was stopped by a sanitizer or reported a violation.
 */
template <typename Run>
bool
caught_in_child (Run run, int attempts)
{
  for (int attempt = 0; attempt < attempts; ++attempt)
    {
      lumex_test::ChildResult const result = lumex_test::run_in_child (
          [&] { return run (static_cast<std::uint64_t> (attempt)); }, 90);
      std::printf ("  child of attempt %d: %s\n", attempt,
                   lumex_test::describe_child (result).c_str ());
      if (lumex_test::child_was_caught (result))
        return true;
    }
  return false;
}

} // namespace

// --- correct engines other than the library's
// ------------------------------------

TEST (
    LumexAtomicSmartPtrFalsificationTest,
    GivenTheMutexReferenceEngine_WhenEveryCheckerRuns_ThenNoneReportsAnything)
{
  lumex_test::TestWatchdog const dog ("GivenTheMutexReferenceEngine");
  lumex_test::Verdict verdict;
  scenario::run_bundle<ReferenceEngine> (verdict, lumex_test::scaled (80));
  EXPECT_TRUE (verdict.ok ()) << verdict.text ();
}

TEST (
    LumexAtomicSmartPtrFalsificationTest,
    GivenALockFreeShapedBoxEngineThatDefersDestruction_WhenEveryCheckerRuns_ThenNoneReportsAnything)
{
  // The same checkers on an engine of the shape the lock-free engine will
  // have: an immutable box behind one pointer word, replaced values kept
  // until later. The checkers must not demand promptness of it.
  lumex_test::TestWatchdog const dog ("GivenALockFreeShapedBoxEngine");
  lumex_test::Verdict verdict;
  scenario::run_bundle<LeakyBoxEngine> (verdict, lumex_test::scaled (80));
  EXPECT_TRUE (verdict.ok ()) << verdict.text ();
}

// --- logical defects, in the process
// -----------------------------------------------------

TEST (
    LumexAtomicSmartPtrFalsificationTest,
    GivenACompareExchangeThatLocksTwice_WhenCheckersRun_ThenLostUpdatesAreSeen)
{
  lumex_test::TestWatchdog const dog ("GivenACompareExchangeThatLocksTwice");
  typedef FlawedEngine<Flaw::cas_split_lock> Engine;
  // Deterministic: the hook makes two threads meet inside the gap.
  LUMEX_EXPECT_CAUGHT (
      Engine::name (),
      (scenario::cas_double_winner<Engine> (
          scenario::make_params (2, 30, lumex_test::ScheduleKind::tight, 1),
          verdict_under_test)));
  LUMEX_EXPECT_CAUGHT (Engine::name (),
                       scenario::gap_aba<Engine> (verdict_under_test));
  // Statistical, with the window widened: the counter and the walk.
  WideningGap const widening;
  LUMEX_EXPECT_CAUGHT (Engine::name (),
                       scenario::matrix (scenario::four_and_eight (),
                                         &scenario::cas_counter<Engine>, 150,
                                         verdict_under_test));
  LUMEX_EXPECT_CAUGHT (Engine::name (),
                       scenario::matrix (scenario::four_and_eight (),
                                         &scenario::rmw_walk<Engine>, 400,
                                         verdict_under_test));
}

TEST (
    LumexAtomicSmartPtrFalsificationTest,
    GivenAnExchangeThatIsALoadThenAStore_WhenCheckersRun_ThenADuplicateIsSeen)
{
  lumex_test::TestWatchdog const dog ("GivenAnExchangeThatIsALoadThenAStore");
  typedef FlawedEngine<Flaw::exchange_split> Engine;
  LUMEX_EXPECT_CAUGHT (
      Engine::name (),
      (scenario::exchange_double_return<Engine> (
          scenario::make_params (2, 30, lumex_test::ScheduleKind::tight, 1),
          verdict_under_test)));
  WideningGap const widening;
  LUMEX_EXPECT_CAUGHT (
      Engine::name (),
      scenario::matrix (scenario::four_and_eight (),
                        &scenario::exchange_permutation<Engine>, 150,
                        verdict_under_test));
  LUMEX_EXPECT_CAUGHT (Engine::name (),
                       scenario::matrix (scenario::four_and_eight (),
                                         &scenario::rmw_walk<Engine>, 400,
                                         verdict_under_test));
}

TEST (
    LumexAtomicSmartPtrFalsificationTest,
    GivenAnEquivalenceOfThePointerOnly_WhenCheckersRun_ThenTheIdentityRuleIsViolated)
{
  lumex_test::TestWatchdog const dog ("GivenAnEquivalenceOfThePointerOnly");
  typedef FlawedEngine<Flaw::cas_pointer_only> Engine;
  LUMEX_EXPECT_CAUGHT (Engine::name (),
                       scenario::identity_rule<Engine> (verdict_under_test));
  // The address-reuse check: a handle that names only the address of a dead
  // node must not match the new node at that address.
  LUMEX_EXPECT_CAUGHT (Engine::name (), scenario::reuse_equivalence<Engine> (
                                            verdict_under_test));
  LUMEX_EXPECT_CAUGHT (Engine::name (),
                       scenario::matrix (scenario::four_and_eight (),
                                         &scenario::rmw_walk<Engine>, 600,
                                         verdict_under_test));
  LUMEX_EXPECT_CAUGHT (
      Engine::name (),
      scenario::run_matrix_for (scenario::history_thread_counts (),
                                &scenario::linearizable_register<Engine>, 200,
                                verdict_under_test));
}

TEST (
    LumexAtomicSmartPtrFalsificationTest,
    GivenAnEquivalenceOfTheOwnerOnly_WhenCheckersRun_ThenTheIdentityRuleIsViolated)
{
  lumex_test::TestWatchdog const dog ("GivenAnEquivalenceOfTheOwnerOnly");
  typedef FlawedEngine<Flaw::cas_owner_only> Engine;
  LUMEX_EXPECT_CAUGHT (Engine::name (),
                       scenario::identity_rule<Engine> (verdict_under_test));
  LUMEX_EXPECT_CAUGHT (Engine::name (),
                       scenario::matrix (scenario::four_and_eight (),
                                         &scenario::rmw_walk<Engine>, 600,
                                         verdict_under_test));
}

TEST (
    LumexAtomicSmartPtrFalsificationTest,
    GivenAnEquivalenceByContent_WhenCheckersRun_ThenTheNewEqualObjectIsSeenAsMatching)
{
  lumex_test::TestWatchdog const dog ("GivenAnEquivalenceByContent");
  typedef FlawedEngine<Flaw::cas_content_equal> Engine;
  // A -> B -> A' with an equal new object: only the identity of the object
  // tells it from A.
  LUMEX_EXPECT_CAUGHT (Engine::name (),
                       scenario::aba_same_owner<Engine> (verdict_under_test));
  LUMEX_EXPECT_CAUGHT (Engine::name (),
                       scenario::identity_rule<Engine> (verdict_under_test));
}

TEST (
    LumexAtomicSmartPtrFalsificationTest,
    GivenAStrongCompareExchangeThatFailsSpuriously_WhenCheckersRun_ThenTheFailureIsSeen)
{
  lumex_test::TestWatchdog const dog (
      "GivenAStrongCompareExchangeThatFailsSpuriously");
  typedef FlawedEngine<Flaw::strong_fails_spuriously> Engine;
  LUMEX_EXPECT_CAUGHT (
      Engine::name (),
      scenario::run_matrix_for (scenario::history_thread_counts (),
                                &scenario::strong_cas_precision<Engine>, 200,
                                verdict_under_test));
  LUMEX_EXPECT_CAUGHT (Engine::name (),
                       scenario::identity_rule<Engine> (verdict_under_test));
}

TEST (LumexAtomicSmartPtrFalsificationTest,
      GivenAStoreThatKeepsTheReplacedValue_WhenCheckersRun_ThenTheLeakIsSeen)
{
  lumex_test::TestWatchdog const dog ("GivenAStoreThatKeepsTheReplacedValue");
  typedef FlawedEngine<Flaw::store_keeps_old> Engine;
  LUMEX_EXPECT_CAUGHT (
      Engine::name (),
      scenario::replaced_value_released<Engine> (verdict_under_test));
  // While the atomic lives only the stored value may be alive; the leak
  // hides in the balance after the atomic is gone (the engine frees its
  // graveyard then), so the in-flight check of the stress catches it.
  LUMEX_EXPECT_CAUGHT (Engine::name (), scenario::run_matrix_for (
                                            scenario::history_thread_counts (),
                                            &scenario::reuse_stress<Engine>,
                                            300, verdict_under_test));
}

TEST (
    LumexAtomicSmartPtrFalsificationTest,
    GivenALoadThatSometimesReturnsAnOlderValue_WhenCheckersRun_ThenStalenessIsSeen)
{
  lumex_test::TestWatchdog const dog (
      "GivenALoadThatSometimesReturnsAnOlderValue");
  typedef FlawedEngine<Flaw::stale_load> Engine;
  LUMEX_EXPECT_CAUGHT (Engine::name (),
                       scenario::matrix (scenario::two_and_four (),
                                         &scenario::monotone_loads<Engine>,
                                         100, verdict_under_test));
  LUMEX_EXPECT_CAUGHT (
      Engine::name (),
      scenario::run_matrix_for (scenario::history_thread_counts (),
                                &scenario::linearizable_register<Engine>, 300,
                                verdict_under_test));
}

// --- the ABA of a pointer word, forced
// ---------------------------------------------------------

TEST (
    LumexAtomicSmartPtrFalsificationTest,
    GivenALockFreeBoxThatFreesAtOnce_WhenTheBoxIsRecycledInTheGap_ThenTheCompareExchangeWronglySucceeds)
{
  // The naive lock-free engine: no hazard protection, the replaced box goes
  // back to the pool and the next box takes its address. The compare-
  // exchange stopped after its comparison succeeds against the recycled box.
  lumex_test::TestWatchdog const dog ("GivenALockFreeBoxThatFreesAtOnce");
  LUMEX_EXPECT_CAUGHT (NaiveBoxEngine::name (),
                       scenario::gap_aba<NaiveBoxEngine> (verdict_under_test));
}

TEST (
    LumexAtomicSmartPtrFalsificationTest,
    GivenALockFreeBoxThatKeepsTheReplacedBox_WhenTheSameInterleavingRuns_ThenTheCompareExchangeFails)
{
  // The same interleaving with the box kept until later (as a hazard
  // protected engine keeps it while it is announced): no ABA, the checker
  // stays silent. This is the discriminating pair of the previous test.
  lumex_test::TestWatchdog const dog (
      "GivenALockFreeBoxThatKeepsTheReplacedBox");
  lumex_test::Verdict verdict;
  scenario::gap_aba<LeakyBoxEngine> (verdict);
  EXPECT_TRUE (verdict.ok ()) << verdict.text ();
}

// --- undefined behavior by design, in a child process
// --------------------------------------------------

TEST (
    LumexAtomicSmartPtrFalsificationTest,
    GivenACompareExchangeWithoutALock_WhenManyThreadsIncrement_ThenTheChildFails)
{
  if (!lumex_test::subprocess_available ())
    GTEST_SKIP () << "no fork on this platform";
  typedef FlawedEngine<Flaw::cas_unlocked> Engine;
  bool const caught = caught_in_child (
      [] (std::uint64_t attempt)
        {
          lumex_test::Verdict verdict;
          scenario::cas_counter<Engine> (
              scenario::make_params (8, 4000, lumex_test::ScheduleKind::tight,
                                     7 + attempt),
              verdict);
          return verdict.ok () ? 0 : 1;
        },
      4);
  EXPECT_TRUE (caught) << "a compare-exchange without a lock was not noticed";
}

TEST (
    LumexAtomicSmartPtrFalsificationTest,
    GivenALockFreeBoxThatFreesAtOnceAndIsReadByOthers_WhenThreadsHammerIt_ThenTheChildFails)
{
  if (!lumex_test::subprocess_available ())
    GTEST_SKIP () << "no fork on this platform";
  bool const caught = caught_in_child (
      [] (std::uint64_t attempt)
        {
          lumex_test::Verdict verdict;
          scenario::reuse_stress<NaiveBoxEngine> (
              scenario::make_params (8, 6000, lumex_test::ScheduleKind::tight,
                                     11 + attempt),
              verdict);
          return verdict.ok () ? 0 : 1;
        },
      4);
  EXPECT_TRUE (caught)
      << "the use-after-free of the naive box was not noticed";
}

TEST (LumexAtomicSmartPtrFalsificationTest,
      GivenTheNaiveBoxStack_WhenThreadsPushAndPop_ThenTheChildFails)
{
  if (!lumex_test::subprocess_available ())
    GTEST_SKIP () << "no fork on this platform";
  bool const caught = caught_in_child (
      [] (std::uint64_t attempt)
        {
          lumex_test::Verdict verdict;
          scenario::treiber_stress<NaiveBoxEngine> (
              scenario::make_params (8, 3000, lumex_test::ScheduleKind::tight,
                                     13 + attempt),
              verdict);
          return verdict.ok () ? 0 : 1;
        },
      4);
  EXPECT_TRUE (caught)
      << "the Treiber stack over the naive box was not noticed";
}

TEST (
    LumexAtomicSmartPtrFalsificationTest,
    GivenALockFreeBoxPublishedWithRelaxedOrders_WhenThreadsReadIt_ThenOnlyThreadSanitizerSeesTheRace)
{
  // A load that returns without acquiring: the box is built before it is
  // published, but nothing orders the construction before the readers. On
  // x86 the hardware hides it, no checker of the values can see it, and only
  // ThreadSanitizer reports the data race. Under ThreadSanitizer the child
  // must end with its report; elsewhere the engine must pass, which
  // documents what the checkers cannot see.
#if LUMEX_TEST_HAS_TSAN
  if (!lumex_test::subprocess_available ())
    GTEST_SKIP () << "no fork on this platform";
  bool const caught = caught_in_child (
      [] (std::uint64_t attempt)
        {
          lumex_test::Verdict verdict;
          scenario::monotone_loads<RelaxedBoxEngine> (
              scenario::make_params (4, 3000, lumex_test::ScheduleKind::tight,
                                     17 + attempt),
              verdict);
          return verdict.ok () ? 0 : 1;
        },
      3);
  EXPECT_TRUE (caught)
      << "ThreadSanitizer did not report the relaxed publication";
#elif defined(__x86_64__) || defined(_M_X64)
  lumex_test::Verdict verdict;
  scenario::monotone_loads<RelaxedBoxEngine> (
      scenario::make_params (4, 1000, lumex_test::ScheduleKind::tight, 17),
      verdict);
  EXPECT_TRUE (verdict.ok ()) << verdict.text ();
#else
  GTEST_SKIP () << "relaxed publication is visible only under ThreadSanitizer";
#endif
}

TEST (
    LumexAtomicSmartPtrFalsificationTest,
    GivenAStoreThatLeaksAReferenceForever_WhenOwnersAreCounted_ThenTheBalanceIsWrong)
{
  // A real leak: the child process ends without the leak report of a leak
  // sanitizer, and what the checkers must see is the reference counts that do
  // not return to one, however long the atomic lives.
  if (!lumex_test::subprocess_available ())
    GTEST_SKIP () << "no fork on this platform";
  typedef FlawedEngine<Flaw::store_leaks_forever> Engine;
  struct Case
  {
    char const *name;
    scenario::Checker checker;
  };
  Case const cases[]
      = { { "owner_balance", &scenario::owner_balance<Engine> },
          { "shared_value_fanout", &scenario::shared_value_fanout<Engine> },
          { "thread_churn", &scenario::thread_churn<Engine> } };
  for (std::size_t i = 0; i < sizeof (cases) / sizeof (cases[0]); ++i)
    {
      scenario::Checker const checker = cases[i].checker;
      lumex_test::ChildResult const result = lumex_test::run_in_child (
          [checker]
            {
              lumex_test::Verdict verdict;
              checker (scenario::make_params (
                           4, 400, lumex_test::ScheduleKind::tight, 5),
                       verdict);
              return verdict.ok () ? 0 : 1;
            });
      EXPECT_TRUE (lumex_test::child_was_caught (result))
          << cases[i].name << ": " << lumex_test::describe_child (result);
    }
}
