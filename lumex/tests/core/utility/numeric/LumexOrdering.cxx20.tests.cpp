// LumexOrdering.cxx20.tests.cpp
// The ordering classes of LumexOrdering.hpp against std::strong_ordering,
// std::weak_ordering and std::partial_ordering of <compare>: the same values,
// the same answers to every comparison with 0 in both operand orders, the same
// comparisons between orderings and between categories, the same conversions,
// the same is_eq .. is_gteq, and the same type traits. The aliases of the
// library name the standard classes where <compare> exists. The classes stay
// distinct from the standard ones: there is no conversion either way.
// GCC 8 accepts -std=c++2a without <compare>, so there the tests skip.

#include <cstddef>
#include <type_traits>
#include <utility>
#if defined(__has_include)
#if __has_include(<compare>)
#include <compare>
#endif
#endif

#include <gtest/gtest.h>

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/numeric/LumexOrdering.hpp"

#if LUMEX_HAS_THREE_WAY_COMPARISON

namespace own = lumex::core::utility::numeric;

namespace
{
// All the answers of an ordering to the comparisons with 0, in both operand
// orders, as bits of one number, so two orderings are compared in one check.
template <typename Ordering>
unsigned
zero_signature (Ordering const &value)
{
  unsigned signature = 0;
  unsigned bit = 1;
  bool const answers[] = { value == 0, 0 == value, value != 0, 0 != value,
                           value < 0,  0 < value,  value <= 0, 0 <= value,
                           value > 0,  0 > value,  value >= 0, 0 >= value };
  for (std::size_t i = 0; i < sizeof (answers) / sizeof (answers[0]); ++i)
    {
      if (answers[i])
        signature |= bit;
      bit <<= 1;
    }
  return signature;
}

// One value of both families.
template <typename Own, typename Std> struct pair_of
{
  Own own_value;
  Std std_value;
};

pair_of<own::strong_ordering, std::strong_ordering> const kStrong[]
    = { { own::strong_ordering::less, std::strong_ordering::less },
        { own::strong_ordering::equal, std::strong_ordering::equal },
        { own::strong_ordering::equivalent, std::strong_ordering::equivalent },
        { own::strong_ordering::greater, std::strong_ordering::greater } };

pair_of<own::weak_ordering, std::weak_ordering> const kWeak[]
    = { { own::weak_ordering::less, std::weak_ordering::less },
        { own::weak_ordering::equivalent, std::weak_ordering::equivalent },
        { own::weak_ordering::greater, std::weak_ordering::greater } };

pair_of<own::partial_ordering, std::partial_ordering> const kPartial[] = {
  { own::partial_ordering::less, std::partial_ordering::less },
  { own::partial_ordering::equivalent, std::partial_ordering::equivalent },
  { own::partial_ordering::greater, std::partial_ordering::greater },
  { own::partial_ordering::unordered, std::partial_ordering::unordered }
};

template <typename Left, typename Right, typename = void>
struct can_relate : std::false_type
{
};

template <typename Left, typename Right>
struct can_relate<
    Left, Right,
    std::void_t<decltype (std::declval<Left> () < std::declval<Right> ())>>
    : std::true_type
{
};

template <typename Left, typename Right, typename = void>
struct can_equate : std::false_type
{
};

template <typename Left, typename Right>
struct can_equate<
    Left, Right,
    std::void_t<decltype (std::declval<Left> () == std::declval<Right> ())>>
    : std::true_type
{
};

// Every trait of the class of this library equals that of the standard class.
template <typename Own, typename Std>
void
expect_same_traits ()
{
  EXPECT_EQ (sizeof (Own), sizeof (Std));
  EXPECT_EQ (alignof (Own), alignof (Std));
  EXPECT_EQ (std::is_trivially_copyable_v<Own>,
             std::is_trivially_copyable_v<Std>);
  EXPECT_EQ (std::is_trivially_destructible_v<Own>,
             std::is_trivially_destructible_v<Std>);
  EXPECT_EQ (std::is_standard_layout_v<Own>, std::is_standard_layout_v<Std>);
  EXPECT_EQ (std::is_empty_v<Own>, std::is_empty_v<Std>);
  EXPECT_EQ (std::is_polymorphic_v<Own>, std::is_polymorphic_v<Std>);
  EXPECT_EQ (std::is_aggregate_v<Own>, std::is_aggregate_v<Std>);
  EXPECT_EQ (std::is_default_constructible_v<Own>,
             std::is_default_constructible_v<Std>);
  EXPECT_EQ (std::is_copy_constructible_v<Own>,
             std::is_copy_constructible_v<Std>);
  EXPECT_EQ (std::is_move_constructible_v<Own>,
             std::is_move_constructible_v<Std>);
  EXPECT_EQ (std::is_nothrow_copy_constructible_v<Own>,
             std::is_nothrow_copy_constructible_v<Std>);
  EXPECT_EQ (std::is_copy_assignable_v<Own>, std::is_copy_assignable_v<Std>);
  EXPECT_EQ (std::is_nothrow_copy_assignable_v<Own>,
             std::is_nothrow_copy_assignable_v<Std>);
  EXPECT_EQ (std::is_nothrow_move_assignable_v<Own>,
             std::is_nothrow_move_assignable_v<Std>);
  EXPECT_EQ ((std::is_constructible_v<Own, int>),
             (std::is_constructible_v<Std, int>));
  EXPECT_EQ ((std::is_constructible_v<Own, signed char>),
             (std::is_constructible_v<Std, signed char>));
  EXPECT_EQ ((std::is_constructible_v<Own, bool>),
             (std::is_constructible_v<Std, bool>));
  EXPECT_EQ ((std::is_convertible_v<int, Own>),
             (std::is_convertible_v<int, Std>));
  EXPECT_EQ ((std::is_convertible_v<Own, int>),
             (std::is_convertible_v<Std, int>));
  EXPECT_EQ ((std::is_convertible_v<Own, bool>),
             (std::is_convertible_v<Std, bool>));
}
} // namespace

#endif // LUMEX_HAS_THREE_WAY_COMPARISON

TEST (LumexOrderingStd,
      GivenCompare_WhenAliasesNamed_ThenTheyAreTheStandardClasses)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  static_assert (std::is_same_v<own::strong_ordering_t, std::strong_ordering>,
                 "strong_ordering_t is std::strong_ordering");
  static_assert (std::is_same_v<own::weak_ordering_t, std::weak_ordering>,
                 "weak_ordering_t is std::weak_ordering");
  static_assert (
      std::is_same_v<own::partial_ordering_t, std::partial_ordering>,
      "partial_ordering_t is std::partial_ordering");
  static_assert (!std::is_same_v<own::strong_ordering, std::strong_ordering>,
                 "the class stays its own");
  static_assert (!std::is_same_v<own::weak_ordering, std::weak_ordering>,
                 "the class stays its own");
  static_assert (!std::is_same_v<own::partial_ordering, std::partial_ordering>,
                 "the class stays its own");
  SUCCEED ();
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (LumexOrderingStd,
      GivenStandardAndOwnClasses_WhenChecked_ThenOneIsOrderingTheOther)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  EXPECT_TRUE (own::is_ordering<std::strong_ordering>::value);
  EXPECT_TRUE (own::is_ordering<std::weak_ordering const &>::value);
  EXPECT_TRUE (own::is_ordering<std::partial_ordering>::value);
  EXPECT_TRUE (own::is_ordering<own::strong_ordering>::value);
  EXPECT_FALSE (own::is_ordering<std::strong_ordering *>::value);
  EXPECT_FALSE (own::is_ordering<std::partial_ordering *>::value);
  EXPECT_FALSE (own::is_ordering<int>::value);
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (LumexOrderingStd,
      GivenStrongOrdering_WhenComparedWithZero_ThenSameAnswersAsStd)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  for (auto const &item : kStrong)
    EXPECT_EQ (zero_signature (item.own_value),
               zero_signature (item.std_value));
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (LumexOrderingStd,
      GivenWeakOrdering_WhenComparedWithZero_ThenSameAnswersAsStd)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  for (auto const &item : kWeak)
    EXPECT_EQ (zero_signature (item.own_value),
               zero_signature (item.std_value));
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (LumexOrderingStd,
      GivenPartialOrdering_WhenComparedWithZero_ThenSameAnswersAsStd)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  for (auto const &item : kPartial)
    EXPECT_EQ (zero_signature (item.own_value),
               zero_signature (item.std_value));
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (LumexOrderingStd,
      GivenValues_WhenComparedBetweenThemselves_ThenSameAnswersAsStd)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  for (auto const &left : kStrong)
    for (auto const &right : kStrong)
      {
        EXPECT_EQ (left.own_value == right.own_value,
                   left.std_value == right.std_value);
        EXPECT_EQ (left.own_value != right.own_value,
                   left.std_value != right.std_value);
      }
  for (auto const &left : kWeak)
    for (auto const &right : kWeak)
      {
        EXPECT_EQ (left.own_value == right.own_value,
                   left.std_value == right.std_value);
        EXPECT_EQ (left.own_value != right.own_value,
                   left.std_value != right.std_value);
      }
  for (auto const &left : kPartial)
    for (auto const &right : kPartial)
      {
        EXPECT_EQ (left.own_value == right.own_value,
                   left.std_value == right.std_value);
        EXPECT_EQ (left.own_value != right.own_value,
                   left.std_value != right.std_value);
      }
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (LumexOrderingStd,
      GivenValuesOfDifferentCategories_WhenCompared_ThenSameAnswersAsStd)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  for (auto const &strong : kStrong)
    for (auto const &weak : kWeak)
      {
        EXPECT_EQ (strong.own_value == weak.own_value,
                   strong.std_value == weak.std_value);
        EXPECT_EQ (weak.own_value == strong.own_value,
                   weak.std_value == strong.std_value);
        EXPECT_EQ (strong.own_value != weak.own_value,
                   strong.std_value != weak.std_value);
      }
  for (auto const &strong : kStrong)
    for (auto const &partial : kPartial)
      {
        EXPECT_EQ (strong.own_value == partial.own_value,
                   strong.std_value == partial.std_value);
        EXPECT_EQ (partial.own_value == strong.own_value,
                   partial.std_value == strong.std_value);
        EXPECT_EQ (strong.own_value != partial.own_value,
                   strong.std_value != partial.std_value);
      }
  for (auto const &weak : kWeak)
    for (auto const &partial : kPartial)
      {
        EXPECT_EQ (weak.own_value == partial.own_value,
                   weak.std_value == partial.std_value);
        EXPECT_EQ (partial.own_value == weak.own_value,
                   partial.std_value == weak.std_value);
        EXPECT_EQ (weak.own_value != partial.own_value,
                   weak.std_value != partial.std_value);
      }
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (LumexOrderingStd, GivenConversions_WhenApplied_ThenSameValuesAsStd)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  for (auto const &item : kStrong)
    {
      own::weak_ordering const own_weak = item.own_value;
      std::weak_ordering const std_weak = item.std_value;
      own::partial_ordering const own_partial = item.own_value;
      std::partial_ordering const std_partial = item.std_value;
      EXPECT_EQ (zero_signature (own_weak), zero_signature (std_weak));
      EXPECT_EQ (zero_signature (own_partial), zero_signature (std_partial));
    }
  for (auto const &item : kWeak)
    {
      own::partial_ordering const own_partial = item.own_value;
      std::partial_ordering const std_partial = item.std_value;
      EXPECT_EQ (zero_signature (own_partial), zero_signature (std_partial));
    }
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (LumexOrderingStd, GivenHelpers_WhenAsked_ThenSameAnswersAsStd)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  for (auto const &item : kStrong)
    {
      EXPECT_EQ (own::is_eq (item.own_value), std::is_eq (item.std_value));
      EXPECT_EQ (own::is_neq (item.own_value), std::is_neq (item.std_value));
      EXPECT_EQ (own::is_lt (item.own_value), std::is_lt (item.std_value));
      EXPECT_EQ (own::is_lteq (item.own_value), std::is_lteq (item.std_value));
      EXPECT_EQ (own::is_gt (item.own_value), std::is_gt (item.std_value));
      EXPECT_EQ (own::is_gteq (item.own_value), std::is_gteq (item.std_value));
    }
  for (auto const &item : kWeak)
    {
      EXPECT_EQ (own::is_eq (item.own_value), std::is_eq (item.std_value));
      EXPECT_EQ (own::is_neq (item.own_value), std::is_neq (item.std_value));
      EXPECT_EQ (own::is_lt (item.own_value), std::is_lt (item.std_value));
      EXPECT_EQ (own::is_lteq (item.own_value), std::is_lteq (item.std_value));
      EXPECT_EQ (own::is_gt (item.own_value), std::is_gt (item.std_value));
      EXPECT_EQ (own::is_gteq (item.own_value), std::is_gteq (item.std_value));
    }
  for (auto const &item : kPartial)
    {
      EXPECT_EQ (own::is_eq (item.own_value), std::is_eq (item.std_value));
      EXPECT_EQ (own::is_neq (item.own_value), std::is_neq (item.std_value));
      EXPECT_EQ (own::is_lt (item.own_value), std::is_lt (item.std_value));
      EXPECT_EQ (own::is_lteq (item.own_value), std::is_lteq (item.std_value));
      EXPECT_EQ (own::is_gt (item.own_value), std::is_gt (item.std_value));
      EXPECT_EQ (own::is_gteq (item.own_value), std::is_gteq (item.std_value));
    }
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (
    LumexOrderingStd,
    GivenUnqualifiedHelpers_WhenCalledWithStandardClasses_ThenArgumentDependentLookupPicksTheStandardOnes)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  using namespace own;

  // With the namespace of this library in scope, the call finds the helper of
  // <compare> through the class of the argument, and the helper of this
  // library through the class of its own: no call is ambiguous.
  EXPECT_TRUE (is_eq (std::strong_ordering::equal));
  EXPECT_TRUE (is_lt (std::partial_ordering::less));
  EXPECT_FALSE (is_gteq (std::partial_ordering::unordered));
  EXPECT_TRUE (is_eq (own::strong_ordering::equal));
  EXPECT_TRUE (is_lt (own::partial_ordering::less));
  EXPECT_FALSE (is_gteq (own::partial_ordering::unordered));
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (LumexOrderingStd, GivenOrderingClasses_WhenInspected_ThenSameTraitsAsStd)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  expect_same_traits<own::strong_ordering, std::strong_ordering> ();
  expect_same_traits<own::weak_ordering, std::weak_ordering> ();
  expect_same_traits<own::partial_ordering, std::partial_ordering> ();

  // The conversions between categories have the same directions.
  EXPECT_EQ (
      (std::is_convertible_v<own::strong_ordering, own::weak_ordering>),
      (std::is_convertible_v<std::strong_ordering, std::weak_ordering>));
  EXPECT_EQ (
      (std::is_convertible_v<own::strong_ordering, own::partial_ordering>),
      (std::is_convertible_v<std::strong_ordering, std::partial_ordering>));
  EXPECT_EQ (
      (std::is_convertible_v<own::weak_ordering, own::partial_ordering>),
      (std::is_convertible_v<std::weak_ordering, std::partial_ordering>));
  EXPECT_EQ (
      (std::is_convertible_v<own::weak_ordering, own::strong_ordering>),
      (std::is_convertible_v<std::weak_ordering, std::strong_ordering>));
  EXPECT_EQ (
      (std::is_convertible_v<own::partial_ordering, own::strong_ordering>),
      (std::is_convertible_v<std::partial_ordering, std::strong_ordering>));
  EXPECT_EQ (
      (std::is_convertible_v<own::partial_ordering, own::weak_ordering>),
      (std::is_convertible_v<std::partial_ordering, std::weak_ordering>));
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (LumexOrderingStd, GivenWhatDoesNotCompile_WhenDetected_ThenSameAsStd)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  EXPECT_EQ ((can_relate<own::strong_ordering, own::strong_ordering>::value),
             (can_relate<std::strong_ordering, std::strong_ordering>::value));
  EXPECT_EQ (
      (can_relate<own::partial_ordering, own::partial_ordering>::value),
      (can_relate<std::partial_ordering, std::partial_ordering>::value));
  EXPECT_EQ ((can_relate<own::strong_ordering, int>::value),
             (can_relate<std::strong_ordering, int>::value));
  EXPECT_EQ ((can_relate<int, own::weak_ordering>::value),
             (can_relate<int, std::weak_ordering>::value));
  EXPECT_EQ ((can_equate<own::strong_ordering, int>::value),
             (can_equate<std::strong_ordering, int>::value));
  EXPECT_EQ ((can_equate<own::strong_ordering, own::partial_ordering>::value),
             (can_equate<std::strong_ordering, std::partial_ordering>::value));
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (LumexOrderingStd,
      GivenTheTwoFamilies_WhenMixed_ThenNoConversionAndNoComparison)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  EXPECT_FALSE (
      (std::is_convertible_v<own::strong_ordering, std::strong_ordering>));
  EXPECT_FALSE (
      (std::is_convertible_v<std::strong_ordering, own::strong_ordering>));
  EXPECT_FALSE (
      (std::is_convertible_v<own::partial_ordering, std::partial_ordering>));
  EXPECT_FALSE (
      (std::is_convertible_v<std::weak_ordering, own::partial_ordering>));
  EXPECT_FALSE (
      (can_equate<own::strong_ordering, std::strong_ordering>::value));
  EXPECT_FALSE (
      (can_equate<std::partial_ordering, own::partial_ordering>::value));
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (LumexOrderingStd, GivenNamedValues_WhenUsedInStaticAssert_ThenSameAsStd)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  static_assert (own::strong_ordering::less < 0);
  static_assert (std::strong_ordering::less < 0);
  static_assert (own::partial_ordering::unordered != 0);
  static_assert (std::partial_ordering::unordered != 0);
  static_assert (!(own::partial_ordering::unordered < 0));
  static_assert (!(std::partial_ordering::unordered < 0));
  static_assert (own::is_gteq (own::weak_ordering::greater));
  static_assert (std::is_gteq (std::weak_ordering::greater));
  SUCCEED ();
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}
