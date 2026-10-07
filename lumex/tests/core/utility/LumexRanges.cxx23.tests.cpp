// LumexRanges.cxx23.tests.cpp
// get_nearest_to checked through std::ranges::contains (C++23).
// LumexRanges.hpp needs C++20 <ranges> and the standard library must have
// std::ranges::contains (__cpp_lib_ranges_contains); otherwise the test
// skips. The suites from C++23 up compile this file together with
// LumexRanges.cxx20.tests.cpp.
#include <algorithm>
#include <ranges>
#include <vector>
#include <version>

#include <gtest/gtest.h>

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/ranges/LumexRanges.hpp"

TEST (LumexRangesTest,
      GivenCpp23Contains_WhenCheckingSortedSource_ThenConfirmsMembership)
{
#if LUMEX_HAS_STD_RANGES && defined(__cpp_lib_ranges_contains)                \
    && __cpp_lib_ranges_contains >= 202207L
  using lumex::core::utility::ranges::Algorithm::get_nearest_to;

  std::vector<int> const values{ 1, 3, 5, 7, 9 };
  auto const it = get_nearest_to (values, 7);
  ASSERT_NE (it, values.end ());
  EXPECT_TRUE (std::ranges::contains (values, *it));
#else
  GTEST_SKIP () << "LumexRanges.hpp or std::ranges::contains is not "
                   "available";
#endif
}
