// LumexOrdering.cxx11.tests.cpp
// strong_ordering, weak_ordering and partial_ordering of LumexOrdering.hpp:
// every value, the named constants, the conversions, the comparisons with the
// literal 0 in both operand orders, the comparisons between orderings,
// is_eq .. is_gteq, constexpr use, the type traits, the alias that names the
// ordering of the library at the standard in use, and the one-definition
// property of the constants. Every suite compiles this file;
// LumexOrdering.cxx20.tests.cpp compares the classes with the standard ones.

#include <cstddef>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/numeric/LumexOrdering.hpp"

using namespace lumex::core::utility::numeric;

namespace lumex_ordering_test
{
// The second translation unit of this suite (LumexOrderingLinkage) asks for
// the address of the same constant.
strong_ordering const *address_of_less_in_first_unit ();
strong_ordering const *address_of_less_in_second_unit ();
partial_ordering const *address_of_unordered_in_first_unit ();
partial_ordering const *address_of_unordered_in_second_unit ();

strong_ordering const *
address_of_less_in_first_unit ()
{
  return &strong_ordering::less;
}

partial_ordering const *
address_of_unordered_in_first_unit ()
{
  return &partial_ordering::unordered;
}
} // namespace lumex_ordering_test

namespace
{
// What each value answers to the six comparisons with 0, written out by hand
// (value op 0; 0 op value gives the mirrored answer).
struct zero_row
{
  bool eq; // value == 0
  bool ne; // value != 0
  bool lt; // value < 0
  bool le; // value <= 0
  bool gt; // value > 0
  bool ge; // value >= 0
};

zero_row const kLessRow = { false, true, true, true, false, false };
zero_row const kEquivalentRow = { true, false, false, true, false, true };
zero_row const kGreaterRow = { false, true, false, false, true, true };
// Neither less, equal nor greater: every comparison with 0 is false except !=.
zero_row const kUnorderedRow = { false, true, false, false, false, false };

template <typename Ordering>
void
expect_zero_comparisons (Ordering const &value, zero_row const &row,
                         char const *name)
{
  SCOPED_TRACE (name);
  EXPECT_EQ (value == 0, row.eq);
  EXPECT_EQ (0 == value, row.eq);
  EXPECT_EQ (value != 0, row.ne);
  EXPECT_EQ (0 != value, row.ne);
  EXPECT_EQ (value < 0, row.lt);
  EXPECT_EQ (0 > value, row.lt);
  EXPECT_EQ (value <= 0, row.le);
  EXPECT_EQ (0 >= value, row.le);
  EXPECT_EQ (value > 0, row.gt);
  EXPECT_EQ (0 < value, row.gt);
  EXPECT_EQ (value >= 0, row.ge);
  EXPECT_EQ (0 <= value, row.ge);
}

// The same, by the helpers.
template <typename Ordering>
void
expect_named_helpers (Ordering const &value, zero_row const &row,
                      char const *name)
{
  SCOPED_TRACE (name);
  EXPECT_EQ (is_eq (value), row.eq);
  EXPECT_EQ (is_neq (value), row.ne);
  EXPECT_EQ (is_lt (value), row.lt);
  EXPECT_EQ (is_lteq (value), row.le);
  EXPECT_EQ (is_gt (value), row.gt);
  EXPECT_EQ (is_gteq (value), row.ge);
}

// Detection of an expression that has to be ill-formed, as for the standard
// orderings.
template <typename Left, typename Right, typename = void>
struct can_relate : std::false_type
{
};

template <typename Left, typename Right>
struct can_relate<
    Left, Right,
    typename std::enable_if<
        std::is_same<decltype (std::declval<Left> () < std::declval<Right> ()),
                     bool>::value>::type> : std::true_type
{
};

template <typename Left, typename Right, typename = void>
struct can_equate : std::false_type
{
};

template <typename Left, typename Right>
struct can_equate<
    Left, Right,
    typename std::enable_if<std::is_same<decltype (std::declval<Left> ()
                                                   == std::declval<Right> ()),
                                         bool>::value>::type> : std::true_type
{
};

// The answer of a comparison with the literal 0 inside decltype: legal for a
// literal, which the detection above cannot spell.
template <typename Ordering>
auto
less_than_literal_zero (Ordering const &value) -> decltype (value < 0)
{
  return value < 0;
}
} // namespace

// === The values against 0 ===

TEST (LumexOrderingValues,
      GivenStrongOrdering_WhenComparedWithZero_ThenAnswersByValue)
{
  expect_zero_comparisons (strong_ordering::less, kLessRow, "less");
  expect_zero_comparisons (strong_ordering::equal, kEquivalentRow, "equal");
  expect_zero_comparisons (strong_ordering::equivalent, kEquivalentRow,
                           "equivalent");
  expect_zero_comparisons (strong_ordering::greater, kGreaterRow, "greater");
}

TEST (LumexOrderingValues,
      GivenWeakOrdering_WhenComparedWithZero_ThenAnswersByValue)
{
  expect_zero_comparisons (weak_ordering::less, kLessRow, "less");
  expect_zero_comparisons (weak_ordering::equivalent, kEquivalentRow,
                           "equivalent");
  expect_zero_comparisons (weak_ordering::greater, kGreaterRow, "greater");
}

TEST (LumexOrderingValues,
      GivenPartialOrdering_WhenComparedWithZero_ThenAnswersByValue)
{
  expect_zero_comparisons (partial_ordering::less, kLessRow, "less");
  expect_zero_comparisons (partial_ordering::equivalent, kEquivalentRow,
                           "equivalent");
  expect_zero_comparisons (partial_ordering::greater, kGreaterRow, "greater");
  expect_zero_comparisons (partial_ordering::unordered, kUnorderedRow,
                           "unordered");
}

TEST (LumexOrderingValues,
      GivenUnordered_WhenComparedWithZero_ThenOnlyNotEqualIsTrue)
{
  partial_ordering const unordered = partial_ordering::unordered;

  EXPECT_FALSE (unordered == 0);
  EXPECT_TRUE (unordered != 0);
  EXPECT_FALSE (unordered < 0);
  EXPECT_FALSE (unordered <= 0);
  EXPECT_FALSE (unordered > 0);
  EXPECT_FALSE (unordered >= 0);
  EXPECT_FALSE (0 == unordered);
  EXPECT_TRUE (0 != unordered);
  EXPECT_FALSE (0 < unordered);
  EXPECT_FALSE (0 <= unordered);
  EXPECT_FALSE (0 > unordered);
  EXPECT_FALSE (0 >= unordered);
}

// === The values against each other ===

TEST (LumexOrderingValues,
      GivenStrongOrdering_WhenCompared_ThenEqualsOnlyItself)
{
  strong_ordering const values[]
      = { strong_ordering::less, strong_ordering::equal,
          strong_ordering::greater };

  for (std::size_t i = 0; i < 3; ++i)
    for (std::size_t j = 0; j < 3; ++j)
      {
        SCOPED_TRACE (i * 10 + j);
        EXPECT_EQ (values[i] == values[j], i == j);
        EXPECT_EQ (values[i] != values[j], i != j);
      }
}

TEST (LumexOrderingValues, GivenWeakOrdering_WhenCompared_ThenEqualsOnlyItself)
{
  weak_ordering const values[]
      = { weak_ordering::less, weak_ordering::equivalent,
          weak_ordering::greater };

  for (std::size_t i = 0; i < 3; ++i)
    for (std::size_t j = 0; j < 3; ++j)
      {
        SCOPED_TRACE (i * 10 + j);
        EXPECT_EQ (values[i] == values[j], i == j);
        EXPECT_EQ (values[i] != values[j], i != j);
      }
}

TEST (LumexOrderingValues,
      GivenPartialOrdering_WhenCompared_ThenEqualsOnlyItself)
{
  partial_ordering const values[]
      = { partial_ordering::less, partial_ordering::equivalent,
          partial_ordering::greater, partial_ordering::unordered };

  for (std::size_t i = 0; i < 4; ++i)
    for (std::size_t j = 0; j < 4; ++j)
      {
        SCOPED_TRACE (i * 10 + j);
        EXPECT_EQ (values[i] == values[j], i == j);
        EXPECT_EQ (values[i] != values[j], i != j);
      }
}

TEST (LumexOrderingValues,
      GivenStrongOrdering_WhenEqualAndEquivalent_ThenSameValue)
{
  EXPECT_TRUE (strong_ordering::equal == strong_ordering::equivalent);
  EXPECT_FALSE (strong_ordering::equal != strong_ordering::equivalent);
  EXPECT_NE (strong_ordering::equal, strong_ordering::less);
  EXPECT_NE (strong_ordering::equal, strong_ordering::greater);
}

// === Conversions ===

TEST (LumexOrderingConversions,
      GivenStrongOrdering_WhenConvertedToWeak_ThenSameValue)
{
  weak_ordering const less = strong_ordering::less;
  weak_ordering const equal = strong_ordering::equal;
  weak_ordering const equivalent = strong_ordering::equivalent;
  weak_ordering const greater = strong_ordering::greater;

  EXPECT_TRUE (less == weak_ordering::less);
  EXPECT_TRUE (equal == weak_ordering::equivalent);
  EXPECT_TRUE (equivalent == weak_ordering::equivalent);
  EXPECT_TRUE (greater == weak_ordering::greater);
  EXPECT_NE (less, greater);
}

TEST (LumexOrderingConversions,
      GivenStrongOrdering_WhenConvertedToPartial_ThenSameValue)
{
  partial_ordering const less = strong_ordering::less;
  partial_ordering const equal = strong_ordering::equal;
  partial_ordering const greater = strong_ordering::greater;

  EXPECT_TRUE (less == partial_ordering::less);
  EXPECT_TRUE (equal == partial_ordering::equivalent);
  EXPECT_TRUE (greater == partial_ordering::greater);
  EXPECT_FALSE (greater == partial_ordering::unordered);
}

TEST (LumexOrderingConversions,
      GivenWeakOrdering_WhenConvertedToPartial_ThenSameValue)
{
  partial_ordering const less = weak_ordering::less;
  partial_ordering const equivalent = weak_ordering::equivalent;
  partial_ordering const greater = weak_ordering::greater;

  EXPECT_TRUE (less == partial_ordering::less);
  EXPECT_TRUE (equivalent == partial_ordering::equivalent);
  EXPECT_TRUE (greater == partial_ordering::greater);
}

TEST (LumexOrderingConversions,
      GivenOrderingsOfDifferentCategories_WhenCompared_ThenByValue)
{
  EXPECT_TRUE (strong_ordering::less == weak_ordering::less);
  EXPECT_TRUE (weak_ordering::less == strong_ordering::less);
  EXPECT_TRUE (strong_ordering::less == partial_ordering::less);
  EXPECT_TRUE (partial_ordering::less == strong_ordering::less);
  EXPECT_TRUE (weak_ordering::greater == partial_ordering::greater);
  EXPECT_TRUE (partial_ordering::greater == weak_ordering::greater);
  EXPECT_TRUE (strong_ordering::equal == weak_ordering::equivalent);
  EXPECT_TRUE (strong_ordering::equal == partial_ordering::equivalent);

  EXPECT_FALSE (strong_ordering::less == weak_ordering::greater);
  EXPECT_FALSE (partial_ordering::greater == strong_ordering::less);
  EXPECT_TRUE (strong_ordering::less != weak_ordering::greater);
  EXPECT_TRUE (partial_ordering::unordered != strong_ordering::equal);
  EXPECT_TRUE (strong_ordering::equal != partial_ordering::unordered);
  EXPECT_FALSE (partial_ordering::unordered == weak_ordering::equivalent);
}

TEST (
    LumexOrderingConversions,
    GivenConversions_WhenCheckedByTraits_ThenOnlyTheWeakerCategoryIsReachable)
{
  EXPECT_TRUE ((std::is_convertible<strong_ordering, weak_ordering>::value));
  EXPECT_TRUE (
      (std::is_convertible<strong_ordering, partial_ordering>::value));
  EXPECT_TRUE ((std::is_convertible<weak_ordering, partial_ordering>::value));

  EXPECT_FALSE ((std::is_convertible<weak_ordering, strong_ordering>::value));
  EXPECT_FALSE (
      (std::is_convertible<partial_ordering, strong_ordering>::value));
  EXPECT_FALSE ((std::is_convertible<partial_ordering, weak_ordering>::value));
}

// === What does not compile ===

TEST (LumexOrderingRejected,
      GivenTwoOrderings_WhenRelated_ThenNoOperatorExists)
{
  EXPECT_FALSE ((can_relate<strong_ordering, strong_ordering>::value));
  EXPECT_FALSE ((can_relate<weak_ordering, weak_ordering>::value));
  EXPECT_FALSE ((can_relate<partial_ordering, partial_ordering>::value));
  EXPECT_FALSE ((can_relate<strong_ordering, partial_ordering>::value));
  EXPECT_TRUE ((can_equate<strong_ordering, strong_ordering>::value));
  EXPECT_TRUE ((can_equate<strong_ordering, partial_ordering>::value));
}

TEST (LumexOrderingRejected,
      GivenNumberOtherThanLiteralZero_WhenRelated_ThenNoOperatorExists)
{
  // Only the literal 0 fits: an int variable (even one of value 0) does not.
  EXPECT_FALSE ((can_relate<strong_ordering, int>::value));
  EXPECT_FALSE ((can_relate<int, strong_ordering>::value));
  EXPECT_FALSE ((can_relate<partial_ordering, long>::value));
  EXPECT_FALSE ((can_relate<weak_ordering, unsigned>::value));
  EXPECT_FALSE ((can_equate<strong_ordering, int>::value));
  EXPECT_FALSE ((can_equate<int, partial_ordering>::value));
  EXPECT_FALSE ((can_equate<weak_ordering, bool>::value));
  EXPECT_FALSE ((can_equate<strong_ordering, double>::value));

  EXPECT_TRUE (less_than_literal_zero (strong_ordering::less));
}

// === The named values are constants and usable in constant expressions ===

TEST (LumexOrderingConstexpr,
      GivenNamedValues_WhenUsedInStaticAssert_ThenEvaluatedAtCompileTime)
{
  static_assert (strong_ordering::less < 0, "less < 0");
  static_assert (strong_ordering::equal == 0, "equal == 0");
  static_assert (strong_ordering::greater > 0, "greater > 0");
  static_assert (0 < strong_ordering::greater, "0 < greater");
  static_assert (0 > strong_ordering::less, "0 > less");
  static_assert (0 == strong_ordering::equivalent, "0 == equivalent");
  static_assert (strong_ordering::less != strong_ordering::greater,
                 "less != greater");
  static_assert (strong_ordering::equal == strong_ordering::equivalent,
                 "equal == equivalent");
  static_assert (weak_ordering::less < 0, "weak less < 0");
  static_assert (weak_ordering::equivalent >= 0, "weak equivalent >= 0");
  static_assert (weak_ordering::greater != 0, "weak greater != 0");
  static_assert (partial_ordering::less <= 0, "partial less <= 0");
  static_assert (partial_ordering::greater >= 0, "partial greater >= 0");
  static_assert (partial_ordering::unordered != 0, "unordered != 0");
  static_assert (!(partial_ordering::unordered == 0), "unordered == 0");
  static_assert (!(partial_ordering::unordered < 0), "unordered < 0");
  static_assert (!(partial_ordering::unordered > 0), "unordered > 0");
  static_assert (!(partial_ordering::unordered <= 0), "unordered <= 0");
  static_assert (!(partial_ordering::unordered >= 0), "unordered >= 0");
  static_assert (!(0 >= partial_ordering::unordered), "0 >= unordered");
  static_assert (!(0 <= partial_ordering::unordered), "0 <= unordered");
  static_assert (strong_ordering::less == weak_ordering::less,
                 "strong less == weak less");
  static_assert (weak_ordering::greater == partial_ordering::greater,
                 "weak greater == partial greater");
  static_assert (partial_ordering::unordered != strong_ordering::equal,
                 "unordered != equal");
  SUCCEED ();
}

namespace
{
constexpr strong_ordering kConstexprLess = strong_ordering::less;
constexpr weak_ordering kConstexprEquivalent = weak_ordering::equivalent;
constexpr partial_ordering kConstexprUnordered = partial_ordering::unordered;
constexpr partial_ordering kConvertedToPartial = strong_ordering::greater;
constexpr weak_ordering kConvertedToWeak = strong_ordering::less;

constexpr strong_ordering
pick (int selector)
{
  return selector < 0   ? strong_ordering::less
         : selector > 0 ? strong_ordering::greater
                        : strong_ordering::equal;
}

constexpr bool kPickedLess = pick (-5) < 0;
constexpr bool kPickedGreater = pick (7) > 0;
constexpr bool kPickedEqual = pick (0) == 0;
} // namespace

TEST (LumexOrderingConstexpr,
      GivenConstexprVariables_WhenInitializedFromNamedValues_ThenKeepTheValues)
{
  EXPECT_TRUE (kConstexprLess == strong_ordering::less);
  EXPECT_TRUE (kConstexprEquivalent == weak_ordering::equivalent);
  EXPECT_TRUE (kConstexprUnordered == partial_ordering::unordered);
  EXPECT_TRUE (kConvertedToPartial == partial_ordering::greater);
  EXPECT_TRUE (kConvertedToWeak == weak_ordering::less);
  EXPECT_TRUE (kPickedLess);
  EXPECT_TRUE (kPickedGreater);
  EXPECT_TRUE (kPickedEqual);
}

TEST (LumexOrderingConstexpr,
      GivenHelpers_WhenUsedInStaticAssert_ThenEvaluatedAtCompileTime)
{
  static_assert (is_eq (strong_ordering::equal), "is_eq");
  static_assert (!is_neq (strong_ordering::equal), "is_neq");
  static_assert (is_lt (weak_ordering::less), "is_lt");
  static_assert (is_lteq (weak_ordering::equivalent), "is_lteq");
  static_assert (is_gt (partial_ordering::greater), "is_gt");
  static_assert (is_gteq (partial_ordering::equivalent), "is_gteq");
  static_assert (is_neq (partial_ordering::unordered), "unordered is_neq");
  static_assert (!is_gteq (partial_ordering::unordered),
                 "unordered is not gteq");
  static_assert (!is_lteq (partial_ordering::unordered),
                 "unordered is not lteq");
  SUCCEED ();
}

// === is_eq, is_neq, is_lt, is_lteq, is_gt, is_gteq ===

TEST (LumexOrderingHelpers,
      GivenEveryValue_WhenHelpersAsked_ThenTheSameAnswersAsComparisonsWithZero)
{
  expect_named_helpers (strong_ordering::less, kLessRow, "strong less");
  expect_named_helpers (strong_ordering::equal, kEquivalentRow,
                        "strong equal");
  expect_named_helpers (strong_ordering::greater, kGreaterRow,
                        "strong greater");
  expect_named_helpers (weak_ordering::less, kLessRow, "weak less");
  expect_named_helpers (weak_ordering::equivalent, kEquivalentRow,
                        "weak equivalent");
  expect_named_helpers (weak_ordering::greater, kGreaterRow, "weak greater");
  expect_named_helpers (partial_ordering::less, kLessRow, "partial less");
  expect_named_helpers (partial_ordering::equivalent, kEquivalentRow,
                        "partial equivalent");
  expect_named_helpers (partial_ordering::greater, kGreaterRow,
                        "partial greater");
  expect_named_helpers (partial_ordering::unordered, kUnorderedRow,
                        "partial unordered");
}

// === Copy, assignment ===

TEST (LumexOrderingValueSemantics,
      GivenOrdering_WhenCopiedAndAssigned_ThenKeepsTheValue)
{
  strong_ordering value = strong_ordering::less;
  strong_ordering const copy = value;

  EXPECT_TRUE (copy < 0);
  value = strong_ordering::greater;
  EXPECT_TRUE (value > 0);
  EXPECT_TRUE (copy < 0);

  partial_ordering partial = partial_ordering::unordered;
  partial = strong_ordering::equal;
  EXPECT_TRUE (partial == 0);
  partial = weak_ordering::greater;
  EXPECT_TRUE (partial > 0);
}

// === Type traits ===

template <typename Ordering>
class LumexOrderingTraitsTest : public ::testing::Test
{
};

using OrderingClasses
    = ::testing::Types<strong_ordering, weak_ordering, partial_ordering>;
TYPED_TEST_SUITE (LumexOrderingTraitsTest, OrderingClasses);

TYPED_TEST (
    LumexOrderingTraitsTest,
    GivenOrderingClass_WhenInspected_ThenTrivialSmallAndNotConstructibleFromNumbers)
{
  EXPECT_TRUE (std::is_trivially_copyable<TypeParam>::value);
  EXPECT_TRUE (std::is_trivially_destructible<TypeParam>::value);
  EXPECT_TRUE (std::is_standard_layout<TypeParam>::value);
  EXPECT_TRUE (std::is_copy_constructible<TypeParam>::value);
  EXPECT_TRUE (std::is_copy_assignable<TypeParam>::value);
  EXPECT_TRUE (std::is_nothrow_copy_constructible<TypeParam>::value);
  EXPECT_TRUE (std::is_nothrow_copy_assignable<TypeParam>::value);
  EXPECT_EQ (sizeof (TypeParam), 1U);

  EXPECT_FALSE (std::is_default_constructible<TypeParam>::value);
  EXPECT_FALSE ((std::is_constructible<TypeParam, int>::value));
  EXPECT_FALSE ((std::is_constructible<TypeParam, signed char>::value));
  EXPECT_FALSE ((std::is_constructible<TypeParam, bool>::value));
  EXPECT_FALSE ((std::is_convertible<int, TypeParam>::value));
  EXPECT_FALSE ((std::is_convertible<TypeParam, int>::value));
  EXPECT_FALSE ((std::is_convertible<TypeParam, bool>::value));
  EXPECT_FALSE (std::is_empty<TypeParam>::value);
}

TYPED_TEST (LumexOrderingTraitsTest,
            GivenOrderingClass_WhenCheckedByIsOrdering_ThenTrueWithQualifiers)
{
  EXPECT_TRUE (is_ordering<TypeParam>::value);
  EXPECT_TRUE (is_ordering<TypeParam const>::value);
  EXPECT_TRUE (is_ordering<TypeParam &>::value);
  EXPECT_TRUE (is_ordering<TypeParam const &>::value);
  EXPECT_TRUE (is_ordering<TypeParam volatile>::value);
}

TEST (LumexOrderingTraits, GivenOtherTypes_WhenCheckedByIsOrdering_ThenFalse)
{
  EXPECT_FALSE (is_ordering<int>::value);
  EXPECT_FALSE (is_ordering<bool>::value);
  EXPECT_FALSE (is_ordering<void>::value);
  EXPECT_FALSE (is_ordering<strong_ordering *>::value);
  EXPECT_FALSE ((is_ordering<std::pair<int, int>>::value));
  EXPECT_FALSE (is_ordering<Detail::ordering_value>::value);
  EXPECT_FALSE (is_ordering<Detail::literal_zero_t>::value);
}

// === The ordering of the library at the standard in use ===

TEST (LumexOrderingAlias,
      GivenStandard_WhenAliasesNamed_ThenOwnClassesBelowCxx20)
{
  // Below C++20 (or without <compare>) the aliases are the classes of this
  // library; with <compare> they are the standard ones (the C++20 file of this
  // suite checks that).
  bool const own = !LUMEX_HAS_THREE_WAY_COMPARISON;

  EXPECT_EQ ((std::is_same<strong_ordering_t, strong_ordering>::value), own);
  EXPECT_EQ ((std::is_same<weak_ordering_t, weak_ordering>::value), own);
  EXPECT_EQ ((std::is_same<partial_ordering_t, partial_ordering>::value), own);
#if __cplusplus < 202002L
  EXPECT_TRUE (own);
#endif
}

TEST (LumexOrderingAlias,
      GivenAlias_WhenUsed_ThenHasTheNamedValuesAndComparisons)
{
  strong_ordering_t const strong = strong_ordering_t::less;
  weak_ordering_t const weak = weak_ordering_t::equivalent;
  partial_ordering_t const partial = partial_ordering_t::unordered;

  EXPECT_TRUE (strong < 0);
  EXPECT_TRUE (strong_ordering_t::equal == 0);
  EXPECT_TRUE (weak == 0);
  EXPECT_TRUE (partial != 0);
  EXPECT_FALSE (partial < 0);
  EXPECT_FALSE (partial > 0);
  EXPECT_TRUE (is_ordering<strong_ordering_t>::value);
  EXPECT_TRUE (is_ordering<weak_ordering_t>::value);
  EXPECT_TRUE (is_ordering<partial_ordering_t>::value);
}

// === One definition of the constants across translation units ===

TEST (LumexOrderingLinkage,
      GivenTwoTranslationUnits_WhenAddressOfConstantTaken_ThenSameObject)
{
  EXPECT_EQ (lumex_ordering_test::address_of_less_in_first_unit (),
             lumex_ordering_test::address_of_less_in_second_unit ());
  EXPECT_EQ (lumex_ordering_test::address_of_unordered_in_first_unit (),
             lumex_ordering_test::address_of_unordered_in_second_unit ());
  EXPECT_NE (static_cast<void const *> (
                 lumex_ordering_test::address_of_less_in_first_unit ()),
             static_cast<void const *> (
                 lumex_ordering_test::address_of_unordered_in_first_unit ()));
}
