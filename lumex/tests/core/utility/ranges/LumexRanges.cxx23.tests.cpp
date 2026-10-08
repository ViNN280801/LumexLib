// LumexRanges.cxx23.tests.cpp
// get_nearest_to checked through std::ranges::contains (C++23).
// The test needs the C++20 <ranges> and a standard library that has
// std::ranges::contains (__cpp_lib_ranges_contains); otherwise it skips. The
// suites from C++23 up compile this file together with the C++11 and C++20
// files of the directory.
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
