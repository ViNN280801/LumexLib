// LumexFormatMinMaxMacros.tests.cpp
// A consumer that includes <windows.h> without NOMINMAX gets function-like
// `min` / `max` macros; every LumexFormat header (and what it includes) must
// still compile, so `std::numeric_limits<T>::max ()` has to be written
// `(std::numeric_limits<T>::max) ()`. The standard headers and GoogleTest
// come first (as they would after <windows.h>), then the macros, then the
// library headers.
#include <chrono>
#include <cstddef>
#include <limits>
#include <locale>
#include <map>
#include <string>
#include <vector>

#include <gtest/gtest.h>

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#if defined(min) || defined(max)
#error "the test must start without min / max macros"
#endif
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
// NOLINTEND(cppcoreguidelines-macro-usage)

#include "lumex/core/fmt/LumexFormat"

namespace fmt = lumex::core::fmt;

TEST (LumexFormatMinMaxMacrosTest,
      GivenMinMaxMacros_WhenFormatting_ThenHeadersWork)
{
  // Paths that use numeric limits: {:c} range check, dynamic width
  // limits, floats, chrono, ranges.
  EXPECT_EQ (fmt::format ("{:c}", 65), "A");
  EXPECT_THROW (fmt::format (fmt::runtime ("{:c}"), 100000), fmt::FormatError);
  EXPECT_EQ (fmt::format ("{:{}}", 1, 3), "  1");
  EXPECT_EQ (fmt::format ("{}", 1e300), "1e+300");
  EXPECT_EQ (fmt::format ("{:%T}", std::chrono::seconds (61)), "00:01:01");
  EXPECT_EQ (fmt::format ("{}", std::map<int, int>{ { 1, 2 } }), "{1: 2}");
  // The macros are still in effect here.
  EXPECT_EQ (min (1, 2), 1);
  EXPECT_EQ (max (1, 2), 2);
}
