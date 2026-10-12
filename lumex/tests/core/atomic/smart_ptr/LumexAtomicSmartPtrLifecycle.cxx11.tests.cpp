// Ownership under concurrency for atomic_shared_ptr: when a replaced value is
// released, reference-count balance under every operation and under
// self-assignment, one value shared by many atomics, threads that exit while
// they hold a reference, deleters that call the same atomic (every operation,
// nested, from many threads), a deleter parked on a latch while the other
// threads must still make progress, and atomics of static storage that are
// destroyed at exit. The balance checks are templates over the engine
// (LumexAtomicScenariosLifecycle.hpp); the falsification suite runs them on
// engines that leak or double count.

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/tests/core/atomic/LumexAtomicScenarios.hpp"
#include "lumex/tests/core/atomic/LumexAtomicScenariosLifecycle.hpp"
#include "lumex/tests/core/atomic/LumexAtomicTestSupport.hpp"
#include "lumex/tests/support/LumexTestLedger.hpp"
#include "lumex/tests/support/LumexTestThreads.hpp"

using namespace lumex_atomic_test;

namespace
{
void
expect_clean (lumex_test::Verdict const &verdict)
{
  EXPECT_TRUE (verdict.ok ()) << verdict.text ();
}

// --- a deleter that uses the atomic it was stored in
// ----------------------------

struct Payload
{
  explicit Payload (lumex_test::ObjectLedger &ledger) : entry (ledger) {}
  lumex_test::LedgerEntry entry;
};

struct ReentrantEnv
{
  ReentrantEnv (lumex_test::ObjectLedger &l)
      : atom (nullptr), ledger (&l), reentrant (true)
  {
  }

  atomic_shared_ptr<Payload> *atom;
  lumex_test::ObjectLedger *ledger;
  std::atomic<bool> reentrant;
};

std::shared_ptr<Payload> make_reentrant (ReentrantEnv *env);

/// Deleter that calls the atomic it was released from, with every operation,
/// at most two levels deep per thread.
struct ReentrantDeleter
{
  ReentrantEnv *env;

  void
  operator() (Payload *p) const
  {
    static thread_local int depth = 0;
    if (env->reentrant.load () && depth < 2)
      {
        ++depth;
        switch ((reinterpret_cast<std::uintptr_t> (p) >> 4) % 5u)
          {
          case 0:
            env->atom->store (make_reentrant (env));
            break;
          case 1:
            {
              std::shared_ptr<Payload> const old
                  = env->atom->exchange (make_reentrant (env));
              (void)old;
              break;
            }
          case 2:
            {
              std::shared_ptr<Payload> expected = env->atom->load ();
              env->atom->compare_exchange_strong (expected,
                                                  make_reentrant (env));
              break;
            }
          case 3:
            {
              std::shared_ptr<Payload> expected = env->atom->load ();
              env->atom->compare_exchange_weak (expected, expected);
              break;
            }
          default:
            {
              std::shared_ptr<Payload> const seen = env->atom->load ();
              (void)seen;
              break;
            }
          }
        --depth;
      }
    delete p;
  }
};

std::shared_ptr<Payload>
make_reentrant (ReentrantEnv *env)
{
  ReentrantDeleter const deleter = { env };
  return std::shared_ptr<Payload> (new Payload (*env->ledger), deleter);
}

// --- static storage
// -----------------------------------------------------------------

struct StaticCounts
{
  std::atomic<int> created;
  std::atomic<int> destroyed;
};

// Constant-initialized and trivially destructible, so it outlives every
// static destructor and can be read from an atexit handler.
StaticCounts static_counts = { { 0 }, { 0 } };

struct StaticPayload
{
  StaticPayload () { static_counts.created.fetch_add (1); }
  ~StaticPayload () { static_counts.destroyed.fetch_add (1); }
  StaticPayload (StaticPayload const &) = delete;
  StaticPayload &operator= (StaticPayload const &) = delete;
};

struct StaticHolder
{
  atomic_shared_ptr<StaticPayload> atom;
  atomic_weak_ptr<StaticPayload> weak;
};

StaticHolder &
static_holder ()
{
  static StaticHolder holder; // destroyed at exit, before the check below
  return holder;
}

void
check_static_counts_at_exit ()
{
  if (static_counts.created.load () != static_counts.destroyed.load ())
    {
      std::fprintf (stderr,
                    "\nSTATIC STORAGE: %d objects were created and %d were "
                    "destroyed by the end of the process\n",
                    static_counts.created.load (),
                    static_counts.destroyed.load ());
      std::fflush (stderr);
      std::_Exit (3);
    }
}
} // namespace

// --- when the replaced value dies
// ---------------------------------------------------

TEST (
    LumexAtomicSmartPtrLifecycleTest,
    GivenEveryReplacingOperation_WhenItReturns_ThenTheReplacedValueIsReleasedAsPromised)
{
  lumex_test::Verdict verdict;
  scenario::replaced_value_released<EngineUnderTest> (verdict);
  expect_clean (verdict);
}

// --- balance
// ------------------------------------------------------------------------------

TEST (
    LumexAtomicSmartPtrLifecycleTest,
    GivenSelfAssignmentsAndHeldCopies_WhenManyThreadsRun_ThenEveryOwnerIsBackAtOne)
{
  lumex_test::TestWatchdog const dog ("GivenSelfAssignmentsAndHeldCopies");
  lumex_test::Verdict verdict;
  scenario::run_matrix (&scenario::owner_balance<EngineUnderTest>,
                        lumex_test::scaled (1500), verdict);
  expect_clean (verdict);
}

TEST (
    LumexAtomicSmartPtrLifecycleTest,
    GivenManyAtomicsSharingOneValue_WhenThreadsCopyItAround_ThenEveryOwnerIsBackAtOne)
{
  lumex_test::TestWatchdog const dog ("GivenManyAtomicsSharingOneValue");
  lumex_test::Verdict verdict;
  scenario::run_matrix (&scenario::shared_value_fanout<EngineUnderTest>,
                        lumex_test::scaled (1500), verdict);
  expect_clean (verdict);
}

TEST (
    LumexAtomicSmartPtrLifecycleTest,
    GivenThreadsThatExitWhileHoldingAReference_WhenWavesRun_ThenNothingLeaksOrDoubleFrees)
{
  lumex_test::TestWatchdog const dog (
      "GivenThreadsThatExitWhileHoldingAReference");
  lumex_test::Verdict verdict;
  scenario::run_matrix_for (std::vector<int> (1, 4),
                            &scenario::thread_churn<EngineUnderTest>,
                            lumex_test::scaled (80), verdict);
  scenario::run_matrix_for (std::vector<int> (1, 12),
                            &scenario::thread_churn<EngineUnderTest>,
                            lumex_test::scaled (40), verdict);
  expect_clean (verdict);
}

// --- deleters that use the atomic
// -------------------------------------------------------------

TEST (
    LumexAtomicSmartPtrLifecycleTest,
    GivenDeletersThatCallEveryOperationOnTheSameAtomic_WhenManyThreadsReplaceIt_ThenNoDeadlockAndNoLeak)
{
  LUMEX_ATOMIC_TEST_REQUIRE_PROMPT_DESTRUCTION ();
  lumex_test::TestWatchdog const dog ("GivenDeletersThatCallEveryOperation");
  std::vector<int> const counts = lumex_test::thread_counts ();
  for (std::size_t c = 0; c < counts.size (); ++c)
    {
      int const iterations = lumex_test::scaled (1200);
      lumex_test::ObjectLedger ledger (
          static_cast<std::size_t> (counts[c])
              * static_cast<std::size_t> (iterations) * 12
          + 64);
      ReentrantEnv env (ledger);
      {
        atomic_shared_ptr<Payload> a (make_reentrant (&env));
        env.atom = &a;
        std::string const error = lumex_test::run_threads (
            counts[c],
            [&] (int t)
              {
                lumex_test::SeededRandom random (lumex_test::derive_seed (
                    lumex_test::base_seed (), static_cast<std::uint64_t> (t),
                    31));
                for (int i = 0; i < iterations; ++i)
                  switch (random.below (4u))
                    {
                    case 0:
                      a.store (make_reentrant (&env));
                      break;
                    case 1:
                      {
                        std::shared_ptr<Payload> const old
                            = a.exchange (make_reentrant (&env));
                        (void)old;
                        break;
                      }
                    case 2:
                      {
                        std::shared_ptr<Payload> expected = a.load ();
                        a.compare_exchange_strong (expected,
                                                   make_reentrant (&env));
                        break;
                      }
                    default:
                      {
                        std::shared_ptr<Payload> const seen = a.load ();
                        if (seen && !seen->entry.intact ())
                          ADD_FAILURE () << "a destroyed payload was loaded";
                        break;
                      }
                    }
              });
        EXPECT_TRUE (error.empty ()) << error;
        env.reentrant.store (false);
        a.store (nullptr);
      }
      EngineUnderTest::quiesce ();
      EXPECT_TRUE (ledger.balanced ())
          << "threads=" << counts[c] << " " << ledger.report ();
    }
}

TEST (
    LumexAtomicSmartPtrLifecycleTest,
    GivenADeleterParkedOnALatch_WhenOtherThreadsUseTheAtomic_ThenTheyMakeProgress)
{
  // A thread is parked inside the deleter of a value it released through the
  // atomic: by store () (the replaced value), by an exchange () whose result
  // is discarded, and by a compare_exchange () that fails and drops its
  // desired value. Every other operation on the same atomic, from another
  // thread, must still complete: user code never runs under a lock the
  // others need.
  lumex_test::TestWatchdog const dog ("GivenADeleterParkedOnALatch");
  char const *const names[]
      = { "store", "exchange", "failed compare_exchange" };
  for (int op = 0; op < 3; ++op)
    {
      if (op < 2 && !EngineUnderTest::replaced_value_dies_in_call ())
        continue; // the engine destroys a replaced value later than the call
      lumex_test::StallPoint stall;
      stall.arm (true);
      auto const make_parking = [&]
        {
          return std::shared_ptr<int> (new int (1),
                                       [&] (int *p)
                                         {
                                           stall.park ();
                                           delete p;
                                         });
        };
      atomic_shared_ptr<int> a (op == 2 ? std::make_shared<int> (1)
                                        : make_parking ());
      std::thread parked (
          [&]
            {
              switch (op)
                {
                case 0:
                  a.store (std::make_shared<int> (2));
                  break;
                case 1:
                  {
                    // The previous value is returned and dropped here.
                    std::shared_ptr<int> const old
                        = a.exchange (std::make_shared<int> (2));
                    (void)old;
                    break;
                  }
                default:
                  {
                    std::shared_ptr<int> expected
                        = std::make_shared<int> (77); // stale
                    a.compare_exchange_strong (expected, make_parking ());
                    break;
                  }
                }
            });
      if (!stall.wait_until_parked (1))
        {
          stall.release ();
          parked.join ();
          FAIL () << names[op] << ": the deleter was not reached";
        }
      lumex_test::Gate done;
      std::thread others (
          [&]
            {
              for (int i = 0; i < 200; ++i)
                {
                  std::shared_ptr<int> seen = a.load ();
                  a.compare_exchange_strong (seen,
                                             std::make_shared<int> (3 + i));
                }
              a.store (std::make_shared<int> (500));
              std::shared_ptr<int> const old
                  = a.exchange (std::make_shared<int> (501));
              EXPECT_EQ (*old, 500);
              done.open ();
            });
      bool const progressed = done.wait_for_ms (20000);
      EXPECT_TRUE (progressed) << names[op]
                               << ": operations on the atomic were blocked "
                                  "while a deleter was parked";
      stall.release ();
      others.join ();
      parked.join ();
      std::shared_ptr<int> const last = a.load ();
      ASSERT_TRUE (static_cast<bool> (last));
      EXPECT_EQ (*last, 501) << names[op];
    }
}

// --- static storage
// ---------------------------------------------------------------------------------

TEST (
    LumexAtomicSmartPtrLifecycleTest,
    GivenAtomicsOfStaticStorage_WhenTheProcessExits_ThenTheyReleaseWhatTheyHold)
{
  LUMEX_ATOMIC_TEST_REQUIRE_PROMPT_DESTRUCTION ();
  // The check runs from an atexit handler registered before the static is
  // built, so it runs after the static is destroyed; a mismatch ends the
  // process with a non-zero code (see check_static_counts_at_exit).
  static bool registered = false;
  if (!registered)
    {
      registered = true;
      ASSERT_EQ (std::atexit (&check_static_counts_at_exit), 0);
    }
  StaticHolder &holder = static_holder ();
  std::shared_ptr<StaticPayload> kept = std::make_shared<StaticPayload> ();
  holder.atom.store (kept);
  holder.weak.store (std::weak_ptr<StaticPayload> (kept));
  holder.atom.store (std::make_shared<StaticPayload> ()); // replaces kept
  kept.reset ();
  if (EngineUnderTest::replaced_value_dies_in_call ())
    {
      EXPECT_GE (static_counts.destroyed.load (), 1)
          << "the replaced payload is released at once";
    }
  EXPECT_EQ (static_counts.created.load () - static_counts.destroyed.load (),
             1)
      << "one payload is held by the static atomic until the process exits";
}
