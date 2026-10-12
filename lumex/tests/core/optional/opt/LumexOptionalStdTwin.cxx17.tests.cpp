// LumexOptionalStdTwin.cxx17.tests.cpp
//
// The optional of this library is its own class in every standard and is
// never an alias of std::optional. From C++17 it converts implicitly to and
// from std::optional<T> (the same T) and compares with it in both orders.
// Each check below is falsified by removing the member or the operator it
// names.
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/optional/opt/LumexOptional.hpp"

namespace
{
using lumex::core::optional::opt::nullopt;
template <typename T>
using lumex_opt = lumex::core::optional::opt::optional<T>;

// Copy of this type may throw, a move may not.
struct throwing_copy
{
  throwing_copy () = default;
  throwing_copy (throwing_copy const &) noexcept (false) {}
  throwing_copy (throwing_copy &&) noexcept = default;
  throwing_copy &operator= (throwing_copy const &) = default;
  throwing_copy &operator= (throwing_copy &&) = default;
};

int
which (std::optional<int> const &)
{
  return 1;
}

int
which (lumex_opt<int> const &)
{
  return 2;
}

int
take_std (std::optional<int> value)
{
  return value.has_value () ? *value : -1;
}

int
take_lumex (lumex_opt<int> const &value)
{
  return value.has_value () ? *value : -1;
}
} // namespace

TEST (LumexOptionalStdTwinTest, GivenAnyStandard_WhenTypes_ThenNotTheSameClass)
{
  static_assert (!std::is_same<lumex_opt<int>, std::optional<int>>::value,
                 "the optional of the library is its own class");
  SUCCEED ();
}

TEST (LumexOptionalStdTwinTest, GivenSameT_WhenConvertibleTraits_ThenBothWays)
{
  static_assert (
      std::is_convertible<std::optional<int>, lumex_opt<int>>::value,
      "std rvalue to lumex");
  static_assert (
      std::is_convertible<std::optional<int> const &, lumex_opt<int>>::value,
      "std const lvalue to lumex");
  static_assert (
      std::is_convertible<std::optional<int> &, lumex_opt<int>>::value,
      "std lvalue to lumex");
  static_assert (
      std::is_convertible<lumex_opt<int>, std::optional<int>>::value,
      "lumex rvalue to std");
  static_assert (
      std::is_convertible<lumex_opt<int> const &, std::optional<int>>::value,
      "lumex const lvalue to std");
  static_assert (
      std::is_convertible<lumex_opt<int> &, std::optional<int>>::value,
      "lumex lvalue to std");
  SUCCEED ();
}

TEST (LumexOptionalStdTwinTest, GivenOtherT_WhenConvertibleTraits_ThenNot)
{
  // Exactly the same T: nothing else converts, so no overload set changes.
  static_assert (
      !std::is_convertible<std::optional<long>, lumex_opt<int>>::value,
      "another T is not converted");
  static_assert (
      !std::is_convertible<lumex_opt<long>, std::optional<int>>::value,
      "another T is not converted back");
  static_assert (
      !std::is_convertible<std::optional<std::string>, lumex_opt<int>>::value,
      "an unrelated T is not converted");
  SUCCEED ();
}

TEST (LumexOptionalStdTwinTest,
      GivenMoveOnlyT_WhenConvertibleTraits_ThenOnlyRvalue)
{
  using ptr = std::unique_ptr<int>;
  static_assert (
      std::is_convertible<std::optional<ptr>, lumex_opt<ptr>>::value,
      "an rvalue std::optional of a move-only type converts");
  static_assert (
      !std::is_convertible<std::optional<ptr> const &, lumex_opt<ptr>>::value,
      "a const lvalue cannot be copied");
  static_assert (
      std::is_convertible<lumex_opt<ptr>, std::optional<ptr>>::value,
      "an rvalue lumex optional of a move-only type converts");
  static_assert (
      !std::is_convertible<lumex_opt<ptr> const &, std::optional<ptr>>::value,
      "a const lvalue cannot be copied back");
  SUCCEED ();
}

TEST (LumexOptionalStdTwinTest, GivenTraits_WhenNoexcept_ThenFollowsT)
{
  static_assert (
      std::is_nothrow_constructible<lumex_opt<int>,
                                    std::optional<int> const &>::value,
      "copy from std::optional<int> is noexcept");
  static_assert (std::is_nothrow_constructible<lumex_opt<int>,
                                               std::optional<int> &&>::value,
                 "move from std::optional<int> is noexcept");
  static_assert (std::is_nothrow_constructible<std::optional<int>,
                                               lumex_opt<int> const &>::value,
                 "copy to std::optional<int> is noexcept");
  static_assert (std::is_nothrow_constructible<std::optional<int>,
                                               lumex_opt<int> &&>::value,
                 "move to std::optional<int> is noexcept");
  static_assert (!std::is_nothrow_constructible<
                     lumex_opt<throwing_copy>,
                     std::optional<throwing_copy> const &>::value,
                 "a throwing copy of T is not noexcept");
  static_assert (
      std::is_nothrow_constructible<lumex_opt<throwing_copy>,
                                    std::optional<throwing_copy> &&>::value,
      "a noexcept move of T is noexcept");
  SUCCEED ();
}

TEST (LumexOptionalStdTwinTest, GivenStdOptional_WhenToLumex_ThenValueOrEmpty)
{
  std::optional<int> const full = 7;
  std::optional<int> const none;
  lumex_opt<int> fromFull = full;
  lumex_opt<int> fromNone = none;
  ASSERT_TRUE (fromFull.has_value ());
  EXPECT_EQ (*fromFull, 7);
  EXPECT_FALSE (fromNone.has_value ());
  EXPECT_TRUE (full.has_value ()) << "the source of a copy is unchanged";
}

TEST (LumexOptionalStdTwinTest, GivenLumexOptional_WhenToStd_ThenValueOrEmpty)
{
  lumex_opt<int> const full = 9;
  lumex_opt<int> const none;
  std::optional<int> fromFull = full;
  std::optional<int> fromNone = none;
  ASSERT_TRUE (fromFull.has_value ());
  EXPECT_EQ (*fromFull, 9);
  EXPECT_FALSE (fromNone.has_value ());
}

TEST (LumexOptionalStdTwinTest, GivenValueTypes_WhenMoved_ThenSourceMovedFrom)
{
  std::optional<std::string> source = std::string (40, 'x');
  lumex_opt<std::string> moved = std::move (source);
  ASSERT_TRUE (moved.has_value ());
  EXPECT_EQ (*moved, std::string (40, 'x'));
  EXPECT_TRUE (source.has_value ());
  EXPECT_TRUE (source->empty ()) << "the value of the source was moved out";

  lumex_opt<std::string> from = std::string (40, 'y');
  std::optional<std::string> back = std::move (from);
  ASSERT_TRUE (back.has_value ());
  EXPECT_EQ (*back, std::string (40, 'y'));
  EXPECT_TRUE (from->empty ()) << "the value of the source was moved out";
}

TEST (LumexOptionalStdTwinTest,
      GivenMoveOnlyValue_WhenConverted_ThenOwnershipMoves)
{
  std::optional<std::unique_ptr<int>> stdValue = std::make_unique<int> (5);
  lumex_opt<std::unique_ptr<int>> lumexValue = std::move (stdValue);
  ASSERT_TRUE (lumexValue.has_value ());
  EXPECT_EQ (**lumexValue, 5);
  std::optional<std::unique_ptr<int>> back = std::move (lumexValue);
  ASSERT_TRUE (back.has_value ());
  EXPECT_EQ (**back, 5);
}

TEST (LumexOptionalStdTwinTest,
      GivenAssignment_WhenEitherWay_ThenCopiesTheState)
{
  lumex_opt<int> lumexValue = 1;
  std::optional<int> stdValue;
  lumexValue = std::optional<int> (4);
  EXPECT_EQ (*lumexValue, 4);
  lumexValue = std::optional<int> ();
  EXPECT_FALSE (lumexValue.has_value ());
  stdValue = lumex_opt<int> (6);
  ASSERT_TRUE (stdValue.has_value ());
  EXPECT_EQ (*stdValue, 6);
  stdValue = lumex_opt<int> ();
  EXPECT_FALSE (stdValue.has_value ());
}

TEST (LumexOptionalStdTwinTest,
      GivenBool_WhenAssignedBothWays_ThenValueNotTruthiness)
{
  // explicit operator bool of the lumex optional must not be taken for a bool
  // value when std::optional<bool> is assigned from it.
  std::optional<bool> stdFlag = false;
  lumex_opt<bool> lumexFlag = true;
  stdFlag = lumexFlag;
  ASSERT_TRUE (stdFlag.has_value ());
  EXPECT_TRUE (*stdFlag);
  lumexFlag = std::optional<bool> (false);
  ASSERT_TRUE (lumexFlag.has_value ());
  EXPECT_FALSE (*lumexFlag);
}

TEST (LumexOptionalStdTwinTest,
      GivenOverloads_WhenCalled_ThenExactTypeWinsWithoutAmbiguity)
{
  EXPECT_EQ (which (std::optional<int> (1)), 1);
  EXPECT_EQ (which (lumex_opt<int> (1)), 2);
  EXPECT_EQ (take_std (lumex_opt<int> (3)), 3);
  EXPECT_EQ (take_std (lumex_opt<int> ()), -1);
  EXPECT_EQ (take_lumex (std::optional<int> (4)), 4);
  EXPECT_EQ (take_lumex (std::optional<int> ()), -1);
}

TEST (LumexOptionalStdTwinTest, GivenEmptyBoth_WhenCompared_ThenEqual)
{
  // A comparison with a plain value would call two empty optionals unequal.
  lumex_opt<int> const none;
  std::optional<int> const stdNone;
  EXPECT_TRUE (none == stdNone);
  EXPECT_TRUE (stdNone == none);
  EXPECT_FALSE (none != stdNone);
  EXPECT_FALSE (stdNone != none);
}

TEST (LumexOptionalStdTwinTest, GivenValues_WhenCompared_ThenLikeStdOptionals)
{
  lumex_opt<int> const one = 1;
  lumex_opt<int> const two = 2;
  lumex_opt<int> const none;
  std::optional<int> const stdOne = 1;
  std::optional<int> const stdTwo = 2;
  std::optional<int> const stdNone;

  EXPECT_TRUE (one == stdOne);
  EXPECT_TRUE (stdOne == one);
  EXPECT_FALSE (one == stdTwo);
  EXPECT_FALSE (stdTwo == one);
  EXPECT_FALSE (one == stdNone);
  EXPECT_FALSE (stdNone == one);
  EXPECT_TRUE (one != stdTwo);
  EXPECT_TRUE (stdTwo != one);
  EXPECT_TRUE (none != stdOne);

  EXPECT_TRUE (one < stdTwo);
  EXPECT_FALSE (two < stdOne);
  EXPECT_TRUE (stdOne < two);
  EXPECT_TRUE (none < stdOne);
  EXPECT_FALSE (one < stdNone);
  EXPECT_TRUE (stdNone < one);

  EXPECT_TRUE (one <= stdOne);
  EXPECT_TRUE (one <= stdTwo);
  EXPECT_FALSE (two <= stdOne);
  EXPECT_TRUE (stdNone <= one);
  EXPECT_FALSE (one <= stdNone);
  EXPECT_TRUE (stdOne <= two);

  EXPECT_TRUE (two > stdOne);
  EXPECT_FALSE (one > stdTwo);
  EXPECT_TRUE (one > stdNone);
  EXPECT_FALSE (none > stdOne);
  EXPECT_TRUE (stdTwo > one);
  EXPECT_FALSE (stdNone > one);

  EXPECT_TRUE (two >= stdOne);
  EXPECT_TRUE (one >= stdOne);
  EXPECT_FALSE (one >= stdTwo);
  EXPECT_TRUE (one >= stdNone);
  EXPECT_FALSE (none >= stdOne);
  EXPECT_TRUE (stdTwo >= one);
  EXPECT_TRUE (stdNone >= none);
}

TEST (LumexOptionalStdTwinTest,
      GivenDifferentValueTypes_WhenCompared_ThenByValue)
{
  lumex_opt<int> const lumexInt = 5;
  std::optional<long> const stdLong = 5L;
  std::optional<long> const stdNone;
  EXPECT_TRUE (lumexInt == stdLong);
  EXPECT_TRUE (stdLong == lumexInt);
  EXPECT_FALSE (lumexInt == stdNone);
  EXPECT_TRUE (lumex_opt<int> () == stdNone);
  EXPECT_TRUE (lumexInt <= stdLong);
}

TEST (LumexOptionalStdTwinTest,
      GivenNullopt_WhenComparedWithBoth_ThenStillWorks)
{
  // The existing comparisons with the nullopt of the library are not
  // disturbed by the new operators.
  lumex_opt<int> const none;
  lumex_opt<int> const one = 1;
  EXPECT_TRUE (none == nullopt);
  EXPECT_TRUE (nullopt == none);
  EXPECT_TRUE (one != nullopt);
  EXPECT_TRUE (one == 1);
  EXPECT_TRUE (1 == one);
  EXPECT_TRUE (one == lumex_opt<int> (1));
}
