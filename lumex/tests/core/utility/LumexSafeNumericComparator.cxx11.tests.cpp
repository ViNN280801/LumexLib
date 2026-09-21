// LumexSafeNumericComparator.cxx11.tests.cpp
// Compile-time regression coverage for comma-bearing template expressions
// passed through LUMEX_STATIC_ASSERT_MSG on the C++11 code path.

#include <gtest/gtest.h>

#include "lumex/core/utility/numeric/LumexSafeNumericComparator.hpp"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

using namespace lumex::core::utility::numeric;

TEST (LumexSafeNumericComparatorCxx11Test,
      MemberMixedTypeComparisonsCompileAndReturnExpectedResults)
{
  SafeComparator<int> value (42);

  EXPECT_TRUE (value.safe_compare (42U));
  EXPECT_TRUE (value.safe_greater_equal (41U));
  EXPECT_TRUE (value.safe_less_equal (43U));
  EXPECT_TRUE (value.safe_less (43U));
  EXPECT_TRUE (value.safe_greater (41U));
  EXPECT_TRUE (value.safe_equal (42U));
  EXPECT_TRUE (value.safe_not_equal (43U));
}

TEST (LumexSafeNumericComparatorCxx11Test,
      FreeMixedTypeComparisonsCompileAndReturnExpectedResults)
{
  EXPECT_TRUE (safe_compare (42, 42U));
  EXPECT_TRUE (safe_greater_equal (42, 41U));
  EXPECT_TRUE (safe_less_equal (42, 43U));
  EXPECT_TRUE (safe_less (42, 43U));
  EXPECT_TRUE (safe_greater (42, 41U));
  EXPECT_TRUE (safe_equal (42, 42U));
  EXPECT_TRUE (safe_not_equal (42, 43U));
  EXPECT_TRUE ((fits_in_type<int> (42)));
}
