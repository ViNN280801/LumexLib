// Multithreaded tests of shared_ptr and weak_ptr: copies of distinct objects
// that share one block from many threads, the last release racing on every
// owner at once, weak promotion against the last release, the lifetime of the
// block against the object, shared_from_this from many threads, hand-over of
// the last reference to other threads, mailboxes that swap objects, and
// conversions of one object. Each scenario is a checker of
// LumexSmartPtrTestScenarios.hpp that runs with 1, 2, 4, 8 and more than the
// number of cores threads and with the four schedule kinds, from a seed that
// is printed on a failure. These are Stress_ tests: they stay on in every
// build, including the sanitizers.

#include <string>

#include <gtest/gtest.h>

#include "lumex/tests/core/smart_ptr/LumexSmartPtrTestScenarios.hpp"
#include "lumex/tests/support/LumexTestReplay.hpp"

namespace
{
using namespace smart_ptr_test;

// Slow schedules do much less work per round: cache flushes and sleeps.
int
iterations_for (lumex_test::ScheduleKind kind, int base)
{
  int const scaled = lumex_test::scaled (base);
  int const divisor = (kind == lumex_test::ScheduleKind::tight
                       || kind == lumex_test::ScheduleKind::yielding)
                          ? 1
                          : 8;
  return std::max (4, scaled / divisor);
}

void
run_everywhere (scenario::Checker checker, int base_iterations,
                char const *name)
{
  lumex_test::TestWatchdog watchdog (name);
  std::uint64_t name_hash = 1469598103934665603ull;
  for (char const *c = name; *c != '\0'; ++c)
    name_hash
        = (name_hash ^ static_cast<unsigned char> (*c)) * 1099511628211ull;
  int run = 0;
  std::vector<int> const counts = lumex_test::thread_counts ();
  std::vector<lumex_test::ScheduleKind> const kinds
      = lumex_test::all_schedule_kinds ();
  for (std::size_t i = 0; i < counts.size (); ++i)
    for (std::size_t k = 0; k < kinds.size (); ++k)
      {
        scenario::Params p;
        p.threads = counts[i];
        p.schedule = kinds[k];
        p.iterations = iterations_for (kinds[k], base_iterations);
        p.seed = lumex_test::derive_seed (lumex_test::base_seed (), name_hash,
                                          static_cast<std::uint64_t> (run++));
        lumex_test::Verdict verdict;
        checker (p, verdict);
        lumex_test::ReplayNote note (name, p.seed, p.threads);
        ASSERT_TRUE (verdict.ok ())
            << name << " threads=" << p.threads
            << " schedule=" << lumex_test::schedule_name (p.schedule)
            << " iterations=" << p.iterations << "\n"
            << verdict.text ();
      }
}

TEST (LumexSharedPtrStressTest, Stress_CopiesOfOneObjectFromManyThreads)
{
  run_everywhere (&scenario::copy_release, 6000, "copy_release");
}

TEST (LumexSharedPtrStressTest, Stress_EveryOwnerDropsAtTheSameMoment)
{
  run_everywhere (&scenario::last_release_race, 4000, "last_release_race");
}

TEST (LumexSharedPtrStressTest, Stress_WeakPromotionAgainstTheLastRelease)
{
  run_everywhere (&scenario::weak_lock_vs_release, 4000,
                  "weak_lock_vs_release");
}

TEST (LumexSharedPtrStressTest,
      Stress_TheBlockOutlivesTheObjectUntilTheLastWeakPointer)
{
  run_everywhere (&scenario::block_lifetime, 400, "block_lifetime");
}

TEST (LumexSharedPtrStressTest, Stress_SharedFromThisFromManyThreads)
{
  run_everywhere (&scenario::esft_concurrent, 4000, "esft_concurrent");
}

TEST (LumexSharedPtrStressTest, Stress_LastReferencesAreDroppedOnOtherThreads)
{
  run_everywhere (&scenario::handoff, 800, "handoff");
}

TEST (LumexSharedPtrStressTest,
      Stress_MailboxesSwapObjectsAndPromoteReplacedOnes)
{
  run_everywhere (&scenario::mailbox_churn, 1200, "mailbox_churn");
}

TEST (LumexSharedPtrStressTest, Stress_ConversionsAliasesAndCastsOfOneObject)
{
  run_everywhere (&scenario::alias_and_convert, 3000, "alias_and_convert");
}

// --- A deleter that runs on the dropping thread, with its own work
// -------------------

TEST (LumexSharedPtrStressTest,
      Stress_DeletersRunExactlyOnceWhicheverThreadDropsLast)
{
  lumex_test::TestWatchdog watchdog ("deleters");
  for (int threads : lumex_test::thread_counts ())
    {
      int const rounds = lumex_test::scaled (300);
      std::atomic<int> calls (0);
      std::vector<sp::shared_ptr<int>> copies (
          static_cast<std::size_t> (threads));
      lumex_test::StartBarrier phase (threads);
      int bad_rounds = 0;
      std::string error = lumex_test::run_threads (
          threads,
          [&] (int index)
            {
              for (int round = 0; round < rounds; ++round)
                {
                  if (index == 0)
                    {
                      sp::shared_ptr<int> p (new int (round),
                                             CountingDeleter<int> (&calls));
                      for (std::size_t i = 0; i < copies.size (); ++i)
                        copies[i] = p;
                    }
                  phase.arrive_and_wait ();
                  copies[static_cast<std::size_t> (index)].reset ();
                  phase.arrive_and_wait ();
                  if (index == 0 && calls.load () != round + 1)
                    ++bad_rounds;
                  phase.arrive_and_wait ();
                }
            });
      EXPECT_EQ (error, "");
      EXPECT_EQ (bad_rounds, 0) << threads << " threads";
      EXPECT_EQ (calls.load (), rounds);
    }
}

// --- Static storage and thread exit
// --------------------------------------------------

TEST (LumexSharedPtrStressTest,
      Stress_ThreadsThatExitHoldingReferencesReleaseThemOnExit)
{
  lumex_test::ObjectLedger ledger (1);
  {
    sp::shared_ptr<Probe> origin = sp::make_shared<Probe> (ledger, 1);
    for (int round = 0; round < lumex_test::scaled (200); ++round)
      {
        std::vector<std::thread> threads;
        for (int i = 0; i < 4; ++i)
          {
            sp::shared_ptr<Probe> given = origin;
            threads.push_back (std::thread (
                [given]
                  {
                    sp::shared_ptr<Probe> inside = given;
                    sp::weak_ptr<Probe> weak = inside;
                    EXPECT_TRUE (inside->intact ());
                  }));
          }
        for (std::size_t i = 0; i < threads.size (); ++i)
          threads[i].join ();
        EXPECT_EQ (origin.use_count (), 1);
      }
  }
  EXPECT_TRUE (ledger.balanced ()) << ledger.report ();
}

// --- Soak
// ------------------------------------------------------------------------------

TEST (LumexSharedPtrSoakTest, Soak_AllScenariosForALongTime)
{
  if (!lumex_test::soak_requested ())
    GTEST_SKIP () << "soak run: set LUMEX_TEST_SOAK=1 (ctest -L soak)";
  struct Case
  {
    char const *name;
    scenario::Checker checker;
    int iterations;
  };
  Case const cases[] = {
    { "copy_release", &scenario::copy_release, 4000 },
    { "last_release_race", &scenario::last_release_race, 2000 },
    { "weak_lock_vs_release", &scenario::weak_lock_vs_release, 2000 },
    { "block_lifetime", &scenario::block_lifetime, 300 },
    { "esft_concurrent", &scenario::esft_concurrent, 2000 },
    { "handoff", &scenario::handoff, 600 },
    { "mailbox_churn", &scenario::mailbox_churn, 800 },
    { "alias_and_convert", &scenario::alias_and_convert, 2000 },
    { "address_reuse_generations", &scenario::address_reuse_generations,
      2000 },
  };
  lumex_test::SeededRandom random (
      lumex_test::derive_seed (lumex_test::base_seed (), 0x50AC));
  std::vector<int> const counts = lumex_test::thread_counts ();
  std::vector<lumex_test::ScheduleKind> const kinds
      = lumex_test::all_schedule_kinds ();
  auto const end = std::chrono::steady_clock::now ()
                   + std::chrono::seconds (lumex_test::soak_seconds ());
  std::uint64_t round = 0;
  lumex_test::TestWatchdog watchdog ("soak");
  while (std::chrono::steady_clock::now () < end)
    {
      Case const &c = cases[round % (sizeof (cases) / sizeof (cases[0]))];
      scenario::Params p;
      p.threads
          = counts[random.below (static_cast<std::uint32_t> (counts.size ()))];
      p.schedule
          = kinds[random.below (static_cast<std::uint32_t> (kinds.size ()))];
      p.iterations = iterations_for (p.schedule, c.iterations)
                     * (1 + static_cast<int> (random.below (3u)));
      p.seed
          = lumex_test::derive_seed (lumex_test::base_seed (), round, 0x50AC);
      lumex_test::Verdict verdict;
      c.checker (p, verdict);
      ASSERT_TRUE (verdict.ok ())
          << "soak round " << round << " " << c.name
          << " threads=" << p.threads
          << " schedule=" << lumex_test::schedule_name (p.schedule) << "\n"
          << lumex_test::replay_text (lumex_test::base_seed (), p.threads)
          << "\n"
          << verdict.text ();
      ++round;
    }
  std::printf ("[   SOAK   ] %llu rounds\n",
               static_cast<unsigned long long> (round));
}
} // namespace
