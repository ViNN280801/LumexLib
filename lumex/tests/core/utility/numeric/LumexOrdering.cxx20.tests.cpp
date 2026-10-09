// LumexOrdering.cxx20.tests.cpp
// The ordering classes of LumexOrdering.hpp against std::strong_ordering,
// std::weak_ordering and std::partial_ordering of <compare>: the same values,
// the same answers to every comparison with 0 in both operand orders, the same
// comparisons between orderings and between categories, the same conversions,
// the same is_eq .. is_gteq, and the same type traits. The classes stay
// distinct from the standard ones and so do the aliases of the library
// (strong_ordering_t ... are the own classes); each own class converts
// implicitly to the standard classes of its category and the weaker ones, is
// built implicitly from those of its category and the stronger ones, and
// compares with every standard class by value, in both operand orders.
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

TEST (LumexOrderingStd, GivenCompare_WhenAliasesNamed_ThenTheyAreTheOwnClasses)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  static_assert (std::is_same_v<own::strong_ordering_t, own::strong_ordering>,
                 "strong_ordering_t is the own class");
  static_assert (std::is_same_v<own::weak_ordering_t, own::weak_ordering>,
                 "weak_ordering_t is the own class");
  static_assert (
      std::is_same_v<own::partial_ordering_t, own::partial_ordering>,
      "partial_ordering_t is the own class");
  static_assert (!std::is_same_v<own::strong_ordering_t, std::strong_ordering>,
                 "never the standard class");
  static_assert (!std::is_same_v<own::weak_ordering_t, std::weak_ordering>,
                 "never the standard class");
  static_assert (
      !std::is_same_v<own::partial_ordering_t, std::partial_ordering>,
      "never the standard class");
  SUCCEED ();
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (LumexOrderingStd,
      GivenConversionMatrix_WhenAsked_ThenOnlyTheDirectionsOfTheStandard)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  // own -> std: the same category or a weaker one.
  static_assert (
      std::is_convertible_v<own::strong_ordering, std::strong_ordering>);
  static_assert (
      std::is_convertible_v<own::strong_ordering, std::weak_ordering>);
  static_assert (
      std::is_convertible_v<own::strong_ordering, std::partial_ordering>);
  static_assert (
      !std::is_convertible_v<own::weak_ordering, std::strong_ordering>);
  static_assert (
      std::is_convertible_v<own::weak_ordering, std::weak_ordering>);
  static_assert (
      std::is_convertible_v<own::weak_ordering, std::partial_ordering>);
  static_assert (
      !std::is_convertible_v<own::partial_ordering, std::strong_ordering>);
  static_assert (
      !std::is_convertible_v<own::partial_ordering, std::weak_ordering>);
  static_assert (
      std::is_convertible_v<own::partial_ordering, std::partial_ordering>);
  // std -> own: the same category or a stronger one.
  static_assert (
      std::is_convertible_v<std::strong_ordering, own::strong_ordering>);
  static_assert (
      std::is_convertible_v<std::strong_ordering, own::weak_ordering>);
  static_assert (
      std::is_convertible_v<std::strong_ordering, own::partial_ordering>);
  static_assert (
      !std::is_convertible_v<std::weak_ordering, own::strong_ordering>);
  static_assert (
      std::is_convertible_v<std::weak_ordering, own::weak_ordering>);
  static_assert (
      std::is_convertible_v<std::weak_ordering, own::partial_ordering>);
  static_assert (
      !std::is_convertible_v<std::partial_ordering, own::strong_ordering>);
  static_assert (
      !std::is_convertible_v<std::partial_ordering, own::weak_ordering>);
  static_assert (
      std::is_convertible_v<std::partial_ordering, own::partial_ordering>);
  // noexcept, as the standard classes are.
  static_assert (
      std::is_nothrow_convertible_v<own::strong_ordering, std::weak_ordering>);
  static_assert (std::is_nothrow_convertible_v<std::weak_ordering,
                                               own::partial_ordering>);
  static_assert (std::is_nothrow_convertible_v<own::partial_ordering,
                                               std::partial_ordering>);
  // The conversions among the own classes are the ones they were.
  static_assert (
      std::is_convertible_v<own::strong_ordering, own::weak_ordering>);
  static_assert (
      !std::is_convertible_v<own::weak_ordering, own::strong_ordering>);
  SUCCEED ();
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (LumexOrderingStd,
      GivenConstantExpressions_WhenConverted_ThenConstexprValues)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  constexpr std::strong_ordering to_std_strong = own::strong_ordering::less;
  constexpr std::weak_ordering to_std_weak = own::strong_ordering::equal;
  constexpr std::partial_ordering to_std_partial
      = own::partial_ordering::unordered;
  constexpr own::strong_ordering from_std_strong
      = std::strong_ordering::greater;
  constexpr own::partial_ordering from_std_partial
      = std::partial_ordering::unordered;
  constexpr own::partial_ordering from_std_strong_value
      = std::strong_ordering::less;
  static_assert (to_std_strong == std::strong_ordering::less);
  static_assert (to_std_weak == std::weak_ordering::equivalent);
  static_assert (to_std_partial == std::partial_ordering::unordered);
  static_assert (from_std_strong == own::strong_ordering::greater);
  static_assert (from_std_partial == own::partial_ordering::unordered);
  static_assert (from_std_strong_value == own::partial_ordering::less);
  // `equal` of std::strong_ordering is the same value as `equivalent` of the
  // other classes.
  static_assert (std::strong_ordering (own::strong_ordering::equivalent)
                 == std::strong_ordering::equal);
  SUCCEED ();
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (LumexOrderingStd, GivenEveryValue_WhenConvertedBothWays_ThenSameValue)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  for (auto const &item : kStrong)
    {
      std::strong_ordering const out = item.own_value;
      own::strong_ordering const back = item.std_value;
      EXPECT_EQ (out, item.std_value);
      EXPECT_EQ (back, item.own_value);
    }
  for (auto const &item : kWeak)
    {
      std::weak_ordering const out = item.own_value;
      own::weak_ordering const back = item.std_value;
      EXPECT_EQ (out, item.std_value);
      EXPECT_EQ (back, item.own_value);
    }
  for (auto const &item : kPartial)
    {
      std::partial_ordering const out = item.own_value;
      own::partial_ordering const back = item.std_value;
      EXPECT_EQ (out, item.std_value);
      EXPECT_EQ (back, item.own_value);
    }
  // Across categories, own -> weaker std and std -> weaker own.
  for (auto const &item : kStrong)
    {
      std::weak_ordering const weak_out = item.own_value;
      std::partial_ordering const partial_out = item.own_value;
      own::weak_ordering const weak_back = item.std_value;
      own::partial_ordering const partial_back = item.std_value;
      EXPECT_EQ (zero_signature (weak_out), zero_signature (item.own_value));
      EXPECT_EQ (zero_signature (partial_out),
                 zero_signature (item.own_value));
      EXPECT_EQ (zero_signature (weak_back), zero_signature (item.std_value));
      EXPECT_EQ (zero_signature (partial_back),
                 zero_signature (item.std_value));
    }
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

TEST (LumexOrderingStd,
      GivenOwnAndStandard_WhenEqualityTested_ThenByValueBothOrders)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  // Not ambiguous, and the same answer in both operand orders, for every pair
  // of categories (the answer is the one of the standard classes).
  for (auto const &own_item : kStrong)
    for (auto const &std_item : kStrong)
      {
        EXPECT_EQ (own_item.own_value == std_item.std_value,
                   own_item.std_value == std_item.std_value);
        EXPECT_EQ (std_item.std_value == own_item.own_value,
                   std_item.std_value == own_item.std_value);
        EXPECT_EQ (own_item.own_value != std_item.std_value,
                   own_item.std_value != std_item.std_value);
        EXPECT_EQ (std_item.std_value != own_item.own_value,
                   std_item.std_value != own_item.std_value);
      }
  for (auto const &own_item : kWeak)
    for (auto const &std_item : kWeak)
      {
        EXPECT_EQ (own_item.own_value == std_item.std_value,
                   own_item.std_value == std_item.std_value);
        EXPECT_EQ (std_item.std_value != own_item.own_value,
                   std_item.std_value != own_item.std_value);
      }
  for (auto const &own_item : kPartial)
    for (auto const &std_item : kPartial)
      {
        EXPECT_EQ (own_item.own_value == std_item.std_value,
                   own_item.std_value == std_item.std_value);
        EXPECT_EQ (std_item.std_value == own_item.own_value,
                   std_item.std_value == own_item.std_value);
        EXPECT_EQ (own_item.own_value != std_item.std_value,
                   own_item.std_value != std_item.std_value);
      }
  // Across categories: a strong value against a weak or a partial one, and a
  // weak against a partial one, in both orders.
  for (auto const &strong : kStrong)
    for (auto const &partial : kPartial)
      {
        EXPECT_EQ (strong.own_value == partial.std_value,
                   strong.std_value == partial.std_value);
        EXPECT_EQ (partial.std_value == strong.own_value,
                   partial.std_value == strong.std_value);
        EXPECT_EQ (partial.own_value == strong.std_value,
                   partial.std_value == strong.std_value);
        EXPECT_EQ (strong.std_value != partial.own_value,
                   strong.std_value != partial.std_value);
      }
  for (auto const &weak : kWeak)
    for (auto const &strong : kStrong)
      {
        EXPECT_EQ (weak.own_value == strong.std_value,
                   weak.std_value == strong.std_value);
        EXPECT_EQ (strong.std_value == weak.own_value,
                   strong.std_value == weak.std_value);
      }
  EXPECT_TRUE (own::partial_ordering::unordered
               == std::partial_ordering::unordered);
  EXPECT_TRUE (own::partial_ordering::unordered
               != std::partial_ordering::equivalent);
  EXPECT_TRUE (std::partial_ordering::unordered
               != own::partial_ordering::less);
#else
  GTEST_SKIP () << "the toolchain has no three-way comparison";
#endif
}

namespace
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
int
overload (std::strong_ordering)
{
  return 1;
}

int
overload (own::strong_ordering)
{
  return 2;
}

int
overload_weak (std::partial_ordering)
{
  return 3;
}
#endif
} // namespace

TEST (LumexOrderingStd,
      GivenOverloads_WhenCalled_ThenTheExactTypeWinsWithoutAmbiguity)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  EXPECT_EQ (overload (std::strong_ordering::less), 1);
  EXPECT_EQ (overload (own::strong_ordering::less), 2);
  // A function that takes a standard class takes the own one of its category.
  EXPECT_EQ (overload_weak (own::strong_ordering::less), 3);
  EXPECT_EQ (overload_weak (own::weak_ordering::less), 3);
  EXPECT_EQ (overload_weak (own::partial_ordering::unordered), 3);
  // The standard helper takes an own ordering through the conversion.
  EXPECT_TRUE (std::is_eq (own::strong_ordering::equal));
  EXPECT_TRUE (std::is_lt (own::partial_ordering::less));
  EXPECT_FALSE (std::is_lteq (own::partial_ordering::unordered));
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
      GivenTheTwoFamilies_WhenMixed_ThenConvertAndCompareByTheCategoryRules)
{
#if LUMEX_HAS_THREE_WAY_COMPARISON
  EXPECT_TRUE (
      (std::is_convertible_v<own::strong_ordering, std::strong_ordering>));
  EXPECT_TRUE (
      (std::is_convertible_v<std::strong_ordering, own::strong_ordering>));
  EXPECT_TRUE (
      (std::is_convertible_v<own::partial_ordering, std::partial_ordering>));
  EXPECT_TRUE (
      (std::is_convertible_v<std::weak_ordering, own::partial_ordering>));
  EXPECT_FALSE (
      (std::is_convertible_v<std::partial_ordering, own::weak_ordering>));
  EXPECT_FALSE (
      (std::is_convertible_v<own::partial_ordering, std::weak_ordering>));
  EXPECT_TRUE (
      (can_equate<own::strong_ordering, std::strong_ordering>::value));
  EXPECT_TRUE (
      (can_equate<std::partial_ordering, own::partial_ordering>::value));
  // The relational operators between two orderings stay absent, mixed too.
  EXPECT_FALSE (
      (can_relate<own::strong_ordering, std::strong_ordering>::value));
  EXPECT_FALSE (
      (can_relate<std::partial_ordering, own::partial_ordering>::value));
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
