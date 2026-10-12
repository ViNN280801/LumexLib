// LumexSafeNumericComparator.cxx20.tests.cpp
// The three-way comparison of safe_comparator and the ordering helpers
// (is_less, is_equal, ...) against the standard orderings. The result is the
// class of LumexOrdering.hpp in every standard; where the compiler and the
// standard library have operator<=> and <compare>
// (LUMEX_HAS_THREE_WAY_COMPARISON) it compares with the standard orderings
// and converts to them, which these tests check. GCC 8 accepts -std=c++2a
// without <compare>, so there they skip. The
// suites from C++20 up compile this file together with
// LumexSafeNumericComparator.cxx11.tests.cpp.
#include <limits>
#if defined(__has_include)
#if __has_include(<compare>)
#include <compare>
#endif
#endif

#include <gtest/gtest.h>

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/numeric/LumexSafeNumericComparator.hpp"

using namespace lumex::core::utility::numeric;

TEST (SafeComparatorCpp20Tests,
      GivenThreeWayCompare_WhenIntegral_ThenStrongOrdering)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  safe_comparator<int> comparator (42);
  int const other_value = 50;

  auto const result = comparator.safe_three_way_compare (other_value);

  EXPECT_TRUE (result == std::strong_ordering::less);
  EXPECT_FALSE (result == std::strong_ordering::equal);
  EXPECT_FALSE (result == std::strong_ordering::greater);
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (SafeComparatorCpp20Tests,
      GivenThreeWayCompare_WhenFloating_ThenPartialOrdering)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  safe_comparator<float> comparator (3.14f);
  float const other_value = 2.71f;

  auto const result = comparator.safe_three_way_compare (other_value);

  EXPECT_TRUE (result == std::partial_ordering::greater);
  EXPECT_FALSE (result == std::partial_ordering::equivalent);
  EXPECT_FALSE (result == std::partial_ordering::less);
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (SafeComparatorCpp20Tests, GivenThreeWayCompare_WhenNaN_ThenUnordered)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  float const nan_value = std::numeric_limits<float>::quiet_NaN ();
  safe_comparator<float> comparator (nan_value);
  float const normal_value = 1.0f;

  auto const result = comparator.safe_three_way_compare (normal_value);

  EXPECT_FALSE (result == std::partial_ordering::equivalent);
  EXPECT_FALSE (result == std::partial_ordering::less);
  EXPECT_FALSE (result == std::partial_ordering::greater);
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (SafeComparatorCpp20Tests,
      GivenUtilityFunctions_WhenCalled_ThenWorkCorrectly)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  safe_comparator<int> comparator (42);
  int const smaller_value = 30;
  int const larger_value = 50;
  int const equal_value = 42;

  auto const result1 = comparator.safe_three_way_compare (smaller_value);
  EXPECT_TRUE (result1 == std::strong_ordering::greater);
  EXPECT_FALSE (result1 == std::strong_ordering::less);
  EXPECT_FALSE (result1 == std::strong_ordering::equal);

  auto const result2 = comparator.safe_three_way_compare (larger_value);
  EXPECT_TRUE (result2 == std::strong_ordering::less);
  EXPECT_FALSE (result2 == std::strong_ordering::greater);
  EXPECT_FALSE (result2 == std::strong_ordering::equal);

  auto const result3 = comparator.safe_three_way_compare (equal_value);
  EXPECT_TRUE (result3 == std::strong_ordering::equal);
  EXPECT_FALSE (result3 == std::strong_ordering::less);
  EXPECT_FALSE (result3 == std::strong_ordering::greater);
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (SafeComparatorCpp20Tests,
      GivenUnsignedCharVersusLargerInt_WhenThreeWay_ThenLess)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  unsigned char const size = 200;
  int const threshold = 300;
  SafeUCharComparator const safe_size (size);

  auto const result = safe_size.safe_three_way_compare (threshold);
  EXPECT_TRUE (is_less (result));
  EXPECT_TRUE (is_less_equal (result));
  EXPECT_TRUE (is_not_equal (result));
  EXPECT_FALSE (is_equal (result));
  EXPECT_FALSE (is_greater (result));
  EXPECT_FALSE (is_greater_equal (result));
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (SafeComparatorCpp20Tests, GivenEqualMixedValues_WhenThreeWay_ThenEqual)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  auto const result
      = safe_three_way_compare (static_cast<unsigned char> (42), 42);
  EXPECT_TRUE (is_equal (result));
  EXPECT_TRUE (is_less_equal (result));
  EXPECT_TRUE (is_greater_equal (result));
  EXPECT_FALSE (is_less (result));
  EXPECT_FALSE (is_greater (result));
  EXPECT_FALSE (is_not_equal (result));
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (SafeComparatorCpp20Tests,
      GivenLargerUnsignedChar_WhenThreeWay_ThenGreater)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  auto const result
      = safe_three_way_compare (static_cast<unsigned char> (200), 150);
  EXPECT_TRUE (is_greater (result));
  EXPECT_TRUE (is_greater_equal (result));
  EXPECT_TRUE (is_not_equal (result));
  EXPECT_FALSE (is_less (result));
  EXPECT_FALSE (is_less_equal (result));
  EXPECT_FALSE (is_equal (result));
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (SafeComparatorCpp20Tests,
      GivenNamedOrderings_WhenHelpers_ThenMatchCategory)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  EXPECT_TRUE (is_less (std::strong_ordering::less));
  EXPECT_TRUE (is_equal (std::strong_ordering::equal));
  EXPECT_TRUE (is_greater (std::strong_ordering::greater));
  EXPECT_TRUE (is_less (std::partial_ordering::less));
  EXPECT_TRUE (is_equal (std::partial_ordering::equivalent));
  EXPECT_TRUE (is_greater (std::partial_ordering::greater));
  EXPECT_FALSE (is_less (std::partial_ordering::unordered));
  EXPECT_FALSE (is_greater (std::partial_ordering::unordered));
  EXPECT_FALSE (is_equal (std::partial_ordering::unordered));
  EXPECT_TRUE (is_less (std::weak_ordering::less));
  EXPECT_TRUE (is_equal (std::weak_ordering::equivalent));
  EXPECT_TRUE (is_greater (std::weak_ordering::greater));
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}
