// LumexFormatPerf.tests.cpp
// One wall-clock budget: formatting a typical log line must stay in the
// same range as building it with std::ostringstream (Release only, no
// sanitizers; see LumexPerfSkip.hpp).
#include <chrono>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/fmt/LumexFormat.hpp"
#include "lumex/tests/support/LumexPerfSkip.hpp"

namespace fmt = lumex::core::fmt;

TEST (LumexFormatPerfTest, Perf_LogLinesWithinBudgetOfOstringstream)
{
#if LUMEX_PERF_WALL_CLOCK_ENABLED
  int const iterations = 200000;
  std::size_t total_format = 0;
  std::size_t total_stream = 0;

  std::chrono::steady_clock::time_point const format_start
      = std::chrono::steady_clock::now ();
  for (int i = 0; i < iterations; ++i)
    {
      std::string const line
          = fmt::format ("channel {:>3} flow {:8.3f} ml/min state {}", i % 16,
                         i * 0.001, "running");
      total_format += line.size ();
    }
  std::chrono::steady_clock::duration const format_time
      = std::chrono::steady_clock::now () - format_start;

  std::chrono::steady_clock::time_point const stream_start
      = std::chrono::steady_clock::now ();
  for (int i = 0; i < iterations; ++i)
    {
      std::ostringstream stream;
      stream << "channel " << std::setw (3) << i % 16 << " flow "
             << std::setw (8) << std::fixed << std::setprecision (3)
             << i * 0.001 << " ml/min state " << "running";
      total_stream += stream.str ().size ();
    }
  std::chrono::steady_clock::duration const stream_time
      = std::chrono::steady_clock::now () - stream_start;

  EXPECT_EQ (total_format, total_stream);
  long long const format_us
      = std::chrono::duration_cast<std::chrono::microseconds> (format_time)
            .count ();
  long long const stream_us
      = std::chrono::duration_cast<std::chrono::microseconds> (stream_time)
            .count ();
  // Correctness first (decision 15): "reasonable" means no worse than twice
  // the ostringstream time, plus a small constant for timer noise.
  EXPECT_LT (format_us, 2 * stream_us + 20000)
      << "format " << format_us << " us, ostringstream " << stream_us << " us";
#else
  GTEST_SKIP () << "wall-clock budget runs only in Release without sanitizers";
#endif
}
