// Performance smoke of the atomic smart pointers: bounded loops with a
// generous wall-clock budget, to catch a regression that makes a hot path
// sleep or spin far more than it should (for example a lock that always
// parks). Release builds without sanitizers only; elsewhere the cases skip.

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/tests/core/atomic/LumexAtomicTestSupport.hpp"
#include "lumex/tests/support/LumexPerfSkip.hpp"

using namespace lumex_atomic_test;

TEST (LumexAtomicSmartPtrPerfTest,
      Perf_GivenUncontendedOperations_WhenRepeated_ThenWithinTheBudget)
{
#if LUMEX_PERF_WALL_CLOCK_ENABLED
  int const rounds = 100000;
  std::shared_ptr<int> const a_value = std::make_shared<int> (1);
  std::shared_ptr<int> const b_value = std::make_shared<int> (2);
  atomic_shared_ptr<int> a (a_value);
  std::chrono::steady_clock::time_point const start
      = std::chrono::steady_clock::now ();
  long checksum = 0;
  for (int i = 0; i < rounds; ++i)
    {
      std::shared_ptr<int> expected = a.load ();
      checksum += *expected;
      a.compare_exchange_strong (expected, (i & 1) == 0 ? b_value : a_value);
      a.store (a.exchange (a_value));
    }
  double const seconds = std::chrono::duration<double> (
                             std::chrono::steady_clock::now () - start)
                             .count ();
  EXPECT_GT (checksum, 0L);
  // Four operations per round; about 2 microseconds per operation on a
  // laptop. The budget is 20 times that.
  EXPECT_LT (seconds, 16.0) << rounds << " rounds took " << seconds << " s";
#else
  GTEST_SKIP () << "wall-clock budgets run in Release without sanitizers";
#endif
}

TEST (LumexAtomicSmartPtrPerfTest,
      Perf_GivenTwoThreadsTradingTheLock_WhenRepeated_ThenWithinTheBudget)
{
#if LUMEX_PERF_WALL_CLOCK_ENABLED
  int const rounds = 50000;
  atomic_shared_ptr<int> a (std::make_shared<int> (0));
  std::chrono::steady_clock::time_point const start
      = std::chrono::steady_clock::now ();
  std::thread other (
      [&]
        {
          for (int i = 0; i < rounds; ++i)
            a.store (a.load ());
        });
  for (int i = 0; i < rounds; ++i)
    a.store (a.load ());
  other.join ();
  double const seconds = std::chrono::duration<double> (
                             std::chrono::steady_clock::now () - start)
                             .count ();
  // A lock that always went to sleep would cost a wake-up per handover,
  // tens of microseconds each: seconds for these rounds.
  EXPECT_LT (seconds, 10.0) << rounds << " rounds took " << seconds << " s";
#else
  GTEST_SKIP () << "wall-clock budgets run in Release without sanitizers";
#endif
}
