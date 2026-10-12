// lumex/tests/core/utility/sequence/LumexIndexSequence.cxx14.tests.cpp
// From C++14 the five names of LumexIndexSequence.hpp are still the class and
// the alias templates of this library, never the standard ones: the types are
// different, an integer_sequence converts implicitly to and from the
// std::integer_sequence of the same list (and to no other), and a function of
// the standard library that deduces std::index_sequence<I...> does not take a
// sequence of this library (the report of the places that need a std list).
#include <cstddef>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/utility/sequence/LumexIndexSequence.hpp"

namespace sequence = lumex::core::utility::sequence;

namespace
{
/** Takes the standard spelling. */
template <std::size_t... I>
std::size_t
std_count (std::index_sequence<I...>)
{
  return sizeof...(I);
}

/** Takes the spelling of this library. */
template <std::size_t... I>
std::size_t
lumex_count (sequence::index_sequence<I...>)
{
  return sizeof...(I);
}

/** Takes one named standard type, which an own sequence converts to. */
std::size_t
std_named_count (std::make_index_sequence<4>)
{
  return 4;
}

/** Is `std_count (Seq ())` well-formed? */
template <typename Seq, typename = void>
struct std_count_accepts : std::false_type
{
};

template <typename Seq>
struct std_count_accepts<Seq,
                         decltype (static_cast<void> (std_count (Seq ())))>
    : std::true_type
{
};
} // namespace

TEST (LumexIndexSequenceStdTest, GivenCxx14_WhenNames_ThenNotTheStdTypes)
{
  static_assert (!std::is_same<sequence::index_sequence<1, 2, 3>,
                               std::index_sequence<1, 2, 3>>::value,
                 "index_sequence is not std::index_sequence");
  static_assert (!std::is_same<sequence::integer_sequence<int, 1, -2>,
                               std::integer_sequence<int, 1, -2>>::value,
                 "integer_sequence is not std::integer_sequence");
  static_assert (!std::is_same<sequence::make_index_sequence<5>,
                               std::make_index_sequence<5>>::value,
                 "make_index_sequence is not std::make_index_sequence");
  static_assert (!std::is_same<sequence::make_integer_sequence<short, 4>,
                               std::make_integer_sequence<short, 4>>::value,
                 "make_integer_sequence is not std::make_integer_sequence");
  static_assert (
      !std::is_same<sequence::index_sequence_for<int, char, void>,
                    std::index_sequence_for<int, char, void>>::value,
      "index_sequence_for is not std::index_sequence_for");
  static_assert (!std::is_same<sequence::make_index_sequence<0>,
                               std::index_sequence<>>::value,
                 "the empty list too");
  SUCCEED ();
}

TEST (LumexIndexSequenceStdTest,
      GivenSameList_WhenConverted_ThenBothWaysImplicit)
{
  static_assert (std::is_convertible<sequence::make_index_sequence<4>,
                                     std::make_index_sequence<4>>::value,
                 "own to std");
  static_assert (std::is_convertible<std::make_index_sequence<4>,
                                     sequence::make_index_sequence<4>>::value,
                 "std to own");
  static_assert (std::is_convertible<sequence::integer_sequence<int, 1, -2>,
                                     std::integer_sequence<int, 1, -2>>::value,
                 "own integer_sequence to std");
  static_assert (
      std::is_convertible<std::integer_sequence<int, 1, -2>,
                          sequence::integer_sequence<int, 1, -2>>::value,
      "std integer_sequence to own");
  static_assert (std::is_convertible<sequence::integer_sequence<int>,
                                     std::integer_sequence<int>>::value,
                 "the empty list");
  static_assert (
      std::is_nothrow_constructible<std::index_sequence<7>,
                                    sequence::index_sequence<7>>::value
          && std::is_nothrow_constructible<sequence::index_sequence<7>,
                                           std::index_sequence<7>>::value,
      "the conversions are noexcept");
  SUCCEED ();
}

TEST (LumexIndexSequenceStdTest,
      GivenOtherList_WhenConverted_ThenNotConvertible)
{
  static_assert (!std::is_convertible<sequence::make_index_sequence<4>,
                                      std::make_index_sequence<5>>::value,
                 "another count");
  static_assert (!std::is_convertible<sequence::integer_sequence<int, 1>,
                                      std::integer_sequence<long, 1>>::value,
                 "another type");
  static_assert (!std::is_convertible<std::index_sequence<1, 2>,
                                      sequence::index_sequence<2, 1>>::value,
                 "another order");
  SUCCEED ();
}

TEST (LumexIndexSequenceStdTest,
      GivenSequences_WhenConverted_ThenConstexprAndEmpty)
{
  constexpr std::make_index_sequence<4> fromOwn
      = sequence::make_index_sequence<4> ();
  constexpr sequence::make_index_sequence<4> fromStd
      = std::make_index_sequence<4> ();
  static_assert (fromOwn.size () == 4 && fromStd.size () == 4,
                 "both are constant expressions with the same count");
  static_assert (
      std::is_trivially_copyable<sequence::make_index_sequence<4>>::value
          && std::is_trivially_default_constructible<
              sequence::make_index_sequence<4>>::value,
      "still trivial");
  SUCCEED ();
}

TEST (
    LumexIndexSequenceStdTest,
    GivenFunctionsOfEitherFamily_WhenPassedTheOther_ThenTheNamedTypeConvertsButDeductionDoesNot)
{
  // A parameter of a named standard type takes the sequence of this library
  // through the conversion.
  EXPECT_EQ (std_named_count (sequence::make_index_sequence<4> ()), 4U);
  // A function template that deduces the indices does not deduce through a
  // conversion: it takes the sequence of its own family only.
  EXPECT_TRUE ((std_count_accepts<std::make_index_sequence<3>>::value));
  EXPECT_FALSE ((std_count_accepts<sequence::make_index_sequence<3>>::value));
  EXPECT_EQ (std_count (std::make_index_sequence<6> ()), 6U);
  EXPECT_EQ (lumex_count (sequence::make_index_sequence<6> ()), 6U);
}
