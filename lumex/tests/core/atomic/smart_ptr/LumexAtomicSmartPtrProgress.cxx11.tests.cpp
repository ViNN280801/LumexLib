// Progress of atomic_shared_ptr under load. The lock-free libc++ attempt of
// the library's author livelocked in exactly this shape: loaders keep reading
// while swappers compare-exchange between two owners, and a compare-exchange
// that waited for the loaders' counter never found its moment. A time-boxed
// run with a minimum number of successes per swapper turns such a livelock
// into a failure (and the watchdog into a failure of a hang). The bound on
// the retries of a strong compare-exchange loop (it cannot fail more often
// than the others succeeded) is in LumexAtomicSmartPtrLinearizability.

#include <algorithm>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/tests/core/atomic/LumexAtomicScenarios.hpp"
#include "lumex/tests/core/atomic/LumexAtomicTestSupport.hpp"
#include "lumex/tests/support/LumexTestThreads.hpp"

using namespace lumex_atomic_test;

namespace
{
struct Mix
{
  int loaders;
  int swappers;
};
} // namespace

TEST (LumexAtomicSmartPtrProgressTest,
      GivenLoadersBesideSwappers_WhenRunForAWhile_ThenEverySwapperKeepsWinning)
{
  lumex_test::TestWatchdog const dog ("GivenLoadersBesideSwappers");
  int const cores = static_cast<int> (lumex_test::hardware_threads ());
  Mix const mixes[]
      = { { 0, 2 },        { 1, 1 }, { 2, 2 },
          { 4, 4 },        { 8, 8 }, { cores / 2 + 1, cores / 2 + 1 },
          { cores + 4, 2 } };
  // A sanitizer slows the threads down, not the clock: the window stays long.
  int const milliseconds = lumex_test::instrumented_build () ? 250 : 100;
  lumex_test::Verdict verdict;
  std::uint64_t entry = 0;
  for (std::size_t i = 0; i < sizeof (mixes) / sizeof (mixes[0]); ++i)
    {
      lumex_test::Verdict run;
      scenario::progress_under_loaders<EngineUnderTest> (
          mixes[i].loaders, mixes[i].swappers, milliseconds, 1, 100,
          lumex_test::derive_seed (lumex_test::base_seed (), entry++), run);
      if (!run.ok ())
        verdict.fail (run.text ());
    }
  EXPECT_TRUE (verdict.ok ()) << verdict.text ();
}
