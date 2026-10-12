// Long soak runs of the concurrency checkers. They are not part of the
// default run: without LUMEX_TEST_SOAK=1 every test here skips at once, and
// CTest registers them only when the tree is configured with
// -DLUMEX_BUILD_SOAK_TESTS=ON, under the label `soak` (ctest -L soak).
// Each test cycles through its checkers with random thread counts, schedule
// kinds, iteration counts and seeds until its time is up;
// LUMEX_TEST_SOAK_SECONDS is the length of the whole soak of one suite
// (default 20 s, split between the soak tests), LUMEX_TEST_SEED fixes the
// sequence of seeds and every failure names the checker, thread count,
// schedule and seed that found it.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/tests/core/atomic/LumexAtomicScenarios.hpp"
#include "lumex/tests/core/atomic/LumexAtomicScenariosLifecycle.hpp"
#include "lumex/tests/core/atomic/LumexAtomicScenariosStructures.hpp"
#include "lumex/tests/core/atomic/LumexAtomicScenariosWeak.hpp"
#include "lumex/tests/core/atomic/LumexAtomicTestSupport.hpp"
#include "lumex/tests/support/LumexTestThreads.hpp"

using namespace lumex_atomic_test;

namespace
{
struct SoakCase
{
  char const *name;
  scenario::Checker checker;
  int iterations;
  bool history; // searched histories: 2 or 3 threads only
};

/// Runs the cases in turn until @p seconds are over; the verdict collects.
void
soak (SoakCase const *cases, std::size_t count, int seconds,
      lumex_test::Verdict &verdict)
{
  lumex_test::SeededRandom random (
      lumex_test::derive_seed (lumex_test::base_seed (), 0x50AC));
  std::vector<int> const counts = lumex_test::thread_counts ();
  std::vector<lumex_test::ScheduleKind> const kinds
      = lumex_test::all_schedule_kinds ();
  std::chrono::steady_clock::time_point const end
      = std::chrono::steady_clock::now () + std::chrono::seconds (seconds);
  std::uint64_t round = 0;
  while (std::chrono::steady_clock::now () < end && verdict.ok ())
    {
      SoakCase const &c = cases[round % count];
      scenario::Params p;
      p.threads = c.history ? 2 + static_cast<int> (random.below (2u))
                            : counts[random.below (static_cast<std::uint32_t> (
                                  counts.size ()))];
      p.schedule
          = kinds[random.below (static_cast<std::uint32_t> (kinds.size ()))];
      p.iterations = std::max (
          1, lumex_test::scaled (c.iterations)
                 * (1 + static_cast<int> (random.below (3u)))
                 / (p.schedule == lumex_test::ScheduleKind::tight
                            || p.schedule == lumex_test::ScheduleKind::yielding
                        ? 1
                        : 6));
      p.seed
          = lumex_test::derive_seed (lumex_test::base_seed (), round, 0x50AC);
      lumex_test::Verdict run;
      c.checker (p, run);
      if (!run.ok ())
        verdict.fail (
            std::string ("soak round ") + std::to_string (round) + " " + c.name
            + " threads=" + std::to_string (p.threads)
            + " schedule=" + lumex_test::schedule_name (p.schedule)
            + " iterations=" + std::to_string (p.iterations) + " "
            + lumex_test::replay_text (lumex_test::base_seed (), p.threads)
            + ": " + run.text ());
      ++round;
    }
  std::printf ("[   SOAK   ] %llu rounds in %d s\n",
               static_cast<unsigned long long> (round), seconds);
}

bool
skip_unless_requested ()
{
  return !lumex_test::soak_requested ();
}
} // namespace

TEST (LumexAtomicSmartPtrSoakTest,
      GivenTheRegisterCheckers_WhenRunForALongTime_ThenNothingIsViolated)
{
  if (skip_unless_requested ())
    GTEST_SKIP () << "soak run: set LUMEX_TEST_SOAK=1 (ctest -L soak)";
  SoakCase const cases[] = {
    { "cas_counter", &scenario::cas_counter<EngineUnderTest>, 400, false },
    { "exchange_permutation", &scenario::exchange_permutation<EngineUnderTest>,
      400, false },
    { "monotone_loads", &scenario::monotone_loads<EngineUnderTest>, 300,
      false },
    { "rmw_walk", &scenario::rmw_walk<EngineUnderTest>, 1500, false },
    { "strong_cas_precision", &scenario::strong_cas_precision<EngineUnderTest>,
      300, false },
    { "reuse_stress", &scenario::reuse_stress<EngineUnderTest>, 600, false },
    { "linearizable_register",
      &scenario::linearizable_register<EngineUnderTest>, 100, true }
  };
  lumex_test::Verdict verdict;
  soak (cases, sizeof (cases) / sizeof (cases[0]),
        std::max (1, lumex_test::soak_seconds () / 2), verdict);
  EXPECT_TRUE (verdict.ok ()) << verdict.text ();
}

TEST (
    LumexAtomicSmartPtrSoakTest,
    GivenTheStructureAndLifecycleCheckers_WhenRunForALongTime_ThenNothingIsViolated)
{
  if (skip_unless_requested ())
    GTEST_SKIP () << "soak run: set LUMEX_TEST_SOAK=1 (ctest -L soak)";
  SoakCase const cases[] = {
    { "treiber_stress", &scenario::treiber_stress<EngineUnderTest>, 500,
      false },
    { "queue_stress", &scenario::queue_stress<EngineUnderTest>, 400, false },
    { "list_stress", &scenario::list_stress<EngineUnderTest>, 500, false },
    { "owner_balance", &scenario::owner_balance<EngineUnderTest>, 1200,
      false },
    { "shared_value_fanout", &scenario::shared_value_fanout<EngineUnderTest>,
      1200, false },
    { "thread_churn", &scenario::thread_churn<EngineUnderTest>, 60, false },
    { "promotion_race", &scenario::promotion_race<EngineUnderTest, true>, 300,
      false },
    { "promotion_race_late", &scenario::promotion_race<EngineUnderTest, false>,
      300, false },
    { "expired_race", &scenario::expired_race<EngineUnderTest>, 150, false },
    { "stack_linearizable",
      &scenario::structure_linearizable<EngineUnderTest, false>, 100, true },
    { "queue_linearizable",
      &scenario::structure_linearizable<EngineUnderTest, true>, 100, true },
    { "list_linearizable", &scenario::list_linearizable<EngineUnderTest>, 100,
      true }
  };
  lumex_test::Verdict verdict;
  soak (cases, sizeof (cases) / sizeof (cases[0]),
        std::max (1, lumex_test::soak_seconds () / 2), verdict);
  EXPECT_TRUE (verdict.ok ()) << verdict.text ();
}
