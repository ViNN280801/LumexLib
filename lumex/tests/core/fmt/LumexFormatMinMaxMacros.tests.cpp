// LumexFormatMinMaxMacros.tests.cpp
// A consumer that includes <windows.h> without NOMINMAX gets function-like
// `min` / `max` macros; every LumexFormat header (and what it includes) must
// still compile, so `std::numeric_limits<T>::max ()` has to be written
// `(std::numeric_limits<T>::max) ()`. The macros guard only the Lumex code:
// the MSVC standard library tolerates them, libstdc++ does not (its
// `tr1/*.tcc`, pulled by <cmath> from C++17, calls
// `std::numeric_limits<_Tp>::max ()` without parentheses), so every
// standard header of the LumexFormat include tree comes first, then
// GoogleTest, then the macros, then the library headers. A standard header
// added to LumexFormat that fails here on GCC belongs in this list.
#include <array>
#include <chrono>
#include <climits>
#include <clocale>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <locale>
#include <map>
#include <memory>
#include <ostream>
#include <ratio>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#if __cplusplus >= 201703L
#include <optional>
#include <string_view>
#if defined(__has_include)
#if __has_include(<charconv>)
#include <charconv>
#endif
#endif
#endif
#if __cplusplus >= 202002L
#include <concepts>
#endif

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
