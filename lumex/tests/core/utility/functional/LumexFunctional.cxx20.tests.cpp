// lumex/tests/core/utility/functional/LumexFunctional.cxx20.tests.cpp
// At C++20 `identity` still is the class of this library, never std::identity;
// it converts implicitly to and from std::identity, and both objects are
// drop-in replacements for the standard ones in the range algorithms: less
// agrees with std::ranges::less on the same operands and models the concepts
// the algorithms require. GCC 8 accepts -std=c++2a without <ranges> or
// std::identity, so there the tests skip.
#include <algorithm>
#if __has_include(<concepts>)
#include <concepts>
#endif
#include <functional>
#include <iterator>
#if __has_include(<ranges>)
#include <ranges>
#endif
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/functional/LumexFunctional.hpp"

namespace functional = lumex::core::utility::functional;

TEST (LumexFunctionalCxx20Test,
      GivenRangesLibrary_WhenIdentity_ThenOwnClassConvertingToStdIdentity)
{
#if LUMEX_HAS_STD_RANGES
  static_assert (!std::is_same<functional::identity, std::identity>::value,
                 "identity is not std::identity");
  static_assert (
      std::is_convertible<functional::identity, std::identity>::value,
      "own to std");
  static_assert (
      std::is_convertible<std::identity, functional::identity>::value,
      "std to own");
  static_assert (
      std::is_nothrow_constructible<std::identity, functional::identity>::value
          && std::is_nothrow_constructible<functional::identity,
                                           std::identity>::value,
      "the conversions are noexcept");
  constexpr std::identity toStd = functional::identity ();
  constexpr functional::identity fromStd = std::identity ();
  static_assert (toStd (5) == 5 && fromStd (6) == 6,
                 "usable in constant expressions");
  std::vector<int> numbers{ 3, 1, 2 };
  std::ranges::sort (numbers, std::ranges::less (), functional::identity ());
  EXPECT_TRUE (std::ranges::is_sorted (numbers));
  SUCCEED ();
#else
  GTEST_SKIP () << "the standard library has no <ranges>";
#endif
}

TEST (LumexFunctionalCxx20Test,
      GivenOperands_WhenLess_ThenAgreesWithRangesLess)
{
#if LUMEX_HAS_STD_RANGES
  functional::less const less;
  std::ranges::less const ranges_less;
  EXPECT_EQ (less (1, 2), ranges_less (1, 2));
  EXPECT_EQ (less (2, 1), ranges_less (2, 1));
  EXPECT_EQ (less (2, 2), ranges_less (2, 2));
  EXPECT_EQ (less (1, 2.5), ranges_less (1, 2.5));
  EXPECT_EQ (less (std::string ("a"), "b"),
             ranges_less (std::string ("a"), "b"));
  int first[2] = { 0, 0 };
  int second[2] = { 0, 0 };
  EXPECT_EQ (less (first, second), ranges_less (first, second));
  EXPECT_EQ (less (&second[1], &first[0]),
             ranges_less (&second[1], &first[0]));
  EXPECT_EQ (less (&first[0], &first[1]), ranges_less (&first[0], &first[1]));
#else
  GTEST_SKIP () << "the standard library has no <ranges>";
#endif
}

TEST (LumexFunctionalCxx20Test,
      GivenRangeAlgorithms_WhenLessAndIdentity_ThenUsable)
{
#if LUMEX_HAS_STD_RANGES
  static_assert (std::strict_weak_order<functional::less, int &, int &>,
                 "less is a strict weak order on ints");
  static_assert (std::indirect_strict_weak_order<functional::less,
                                                 std::vector<int>::iterator,
                                                 std::vector<int>::iterator>,
                 "less is an indirect strict weak order");
  static_assert (std::regular_invocable<functional::identity, int &>,
                 "identity is a projection");

  std::vector<int> numbers{ 5, 2, 9, 1 };
  std::ranges::sort (numbers, functional::less (), functional::identity ());
  EXPECT_TRUE (std::ranges::is_sorted (numbers));
  std::vector<int>::const_iterator const found = std::ranges::lower_bound (
      numbers, 5, functional::less (), functional::identity ());
  ASSERT_NE (found, numbers.end ());
  EXPECT_EQ (*found, 5);
#else
  GTEST_SKIP () << "the standard library has no <ranges>";
#endif
}
