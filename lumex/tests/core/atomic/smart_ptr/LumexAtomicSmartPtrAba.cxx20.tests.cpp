// C++20 additions to the ABA and linearizability suites: the same checkers on
// std::atomic<std::shared_ptr> and std::atomic<std::weak_ptr> where the
// library has them (an implementation that is independent of both LumexLib
// engines, so a checker that disagrees with it is the suspect), and the
// reuse-address allocator behind std::allocate_shared<T[]>.

#include <atomic>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/tests/core/atomic/LumexAtomicScenarioBundle.hpp"
#include "lumex/tests/core/atomic/LumexAtomicScenarios.hpp"
#include "lumex/tests/core/atomic/LumexAtomicTestSupport.hpp"
#include "lumex/tests/support/LumexTestLedger.hpp"
#include "lumex/tests/support/LumexTestReusePool.hpp"
#include "lumex/tests/support/LumexTestThreads.hpp"

using namespace lumex_atomic_test;

#if defined(__cpp_lib_atomic_shared_ptr)                                      \
    && __cpp_lib_atomic_shared_ptr >= 201711L
#define LUMEX_ATOMIC_TEST_HAS_STD_ATOMIC_SHARED 1
#else
#define LUMEX_ATOMIC_TEST_HAS_STD_ATOMIC_SHARED 0
#endif

#if LUMEX_ATOMIC_TEST_HAS_STD_ATOMIC_SHARED
namespace
{
/// std::atomic<std::shared_ptr<T>> as an engine of the checkers.
struct StdAtomicEngine
{
  template <typename T> using shared = std::atomic<std::shared_ptr<T>>;
  template <typename T> using weak = std::atomic<std::weak_ptr<T>>;

  static bool
  replaced_value_dies_in_call ()
  {
    return true;
  }

  static void
  quiesce ()
  {
  }

  static char const *
  name ()
  {
    return "std::atomic<std::shared_ptr>";
  }
};
} // namespace
#endif

TEST (LumexAtomicSmartPtrAbaCxx20Test,
      GivenStdAtomicSharedPtr_WhenEveryCheckerRuns_ThenNoneReportsAnything)
{
#if LUMEX_ATOMIC_TEST_HAS_STD_ATOMIC_SHARED
  lumex_test::TestWatchdog const dog ("GivenStdAtomicSharedPtr");
  lumex_test::Verdict verdict;
  scenario::run_bundle<StdAtomicEngine> (verdict, lumex_test::scaled (80));
  scenario::run_weak_bundle<StdAtomicEngine> (verdict,
                                              lumex_test::scaled (80));
  EXPECT_TRUE (verdict.ok ()) << verdict.text ();
#else
  GTEST_SKIP () << "the standard library has no std::atomic<std::shared_ptr>";
#endif
}

TEST (
    LumexAtomicSmartPtrAbaCxx20Test,
    GivenAnArrayAddressReusedByAllocateShared_WhenCompareExchangeFromAStaleHandle_ThenItFails)
{
  // allocate_shared<T[]> over the reuse pool: the array of a dead owner and
  // the array of the next owner share an address, and a handle that names
  // only the address does not match.
  lumex_test::ReusePool pool;
  atomic_shared_ptr<int[]> a;
  std::shared_ptr<int[]> stale;
  {
    std::shared_ptr<int[]> first = std::allocate_shared<int[]> (
        lumex_test::ReuseAllocator<int> (pool), 4);
    int *const raw = first.get ();
    stale = std::shared_ptr<int[]> (std::shared_ptr<void> (), raw);
    a.store (first);
    a.store (std::allocate_shared<int[]> (
        lumex_test::ReuseAllocator<int> (pool), 4));
    first.reset ();
    EngineUnderTest::quiesce ();
    std::shared_ptr<int[]> second = std::allocate_shared<int[]> (
        lumex_test::ReuseAllocator<int> (pool), 4);
    if (EngineUnderTest::replaced_value_dies_in_call ())
      {
        ASSERT_EQ (second.get (), raw)
            << "the allocator did not reuse the array";
      }
    a.store (second);
    std::shared_ptr<int[]> desired = std::allocate_shared<int[]> (
        lumex_test::ReuseAllocator<int> (pool), 4);
    EXPECT_FALSE (a.compare_exchange_strong (stale, desired));
    EXPECT_TRUE (!stale.owner_before (second) && !second.owner_before (stale))
        << "expected must hold the stored array";
  }
  a.store (nullptr);
  EngineUnderTest::quiesce ();
}
