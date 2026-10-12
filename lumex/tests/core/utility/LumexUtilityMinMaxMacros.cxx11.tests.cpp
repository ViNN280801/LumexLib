// LumexUtilityMinMaxMacros.cxx11.tests.cpp
// A consumer that includes <windows.h> without NOMINMAX gets function-like
// `min` / `max` macros; every utility header (and what it includes) must
// still compile, so `std::numeric_limits<T>::min ()` / `max ()` has to be
// written `(std::numeric_limits<T>::max) ()`. The macros guard only the
// Lumex code: the MSVC standard library tolerates them, libstdc++ does not,
// so every standard header of the utility include tree comes first, then
// GoogleTest, then the macros, then the library headers. A standard header
// added to that tree which fails here on GCC belongs in this list.
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cctype>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>
#if __cplusplus >= 201703L
#include <optional>
#include <string_view>
#endif
#if __cplusplus >= 202002L
#include <bit>
#include <compare>
#include <concepts>
#include <ranges>
#include <span>
#include <version>
#endif

#include <gtest/gtest.h>

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#if defined(min) || defined(max)
#error "the test must start without min / max macros"
#endif
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
// NOLINTEND(cppcoreguidelines-macro-usage)

#include "lumex/core/utility/LumexUtility"

using namespace lumex::core::utility::numeric;

TEST (LumexUtilityMinMaxMacrosTest,
      GivenMinMaxMacros_WhenComparing_ThenHeadersWork)
{
  safe_comparator<int> const comparator (42);
  EXPECT_TRUE (comparator.safe_equal (42));
  EXPECT_TRUE (comparator.safe_less (100));
  EXPECT_FALSE (comparator.safe_greater (100));
  // The macros are still in effect here.
  EXPECT_EQ (min (1, 2), 1);
  EXPECT_EQ (max (1, 2), 2);
}
