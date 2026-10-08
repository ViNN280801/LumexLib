// lumex/tests/core/utility/sequence/LumexIndexSequence.cxx14.tests.cpp
// From C++14 the five names of LumexIndexSequence.hpp are the standard ones:
// the types are identical, so a value of one spelling is accepted wherever the
// other is expected. The header decides by LUMEX_HAS_STD_INTEGER_SEQUENCE; a
// toolchain that reports C++14 without the standard sequences uses the own
// class and the tests skip.
#include <cstddef>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
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
} // namespace

TEST (LumexIndexSequenceStdTest, GivenCxx14_WhenNames_ThenStdTypes)
{
#if LUMEX_HAS_STD_INTEGER_SEQUENCE
  static_assert (std::is_same<sequence::index_sequence<1, 2, 3>,
                              std::index_sequence<1, 2, 3>>::value,
                 "index_sequence is std::index_sequence");
  static_assert (std::is_same<sequence::integer_sequence<int, 1, -2>,
                              std::integer_sequence<int, 1, -2>>::value,
                 "integer_sequence is std::integer_sequence");
  static_assert (std::is_same<sequence::make_index_sequence<5>,
                              std::make_index_sequence<5>>::value,
                 "make_index_sequence is std::make_index_sequence");
  static_assert (std::is_same<sequence::make_integer_sequence<short, 4>,
                              std::make_integer_sequence<short, 4>>::value,
                 "make_integer_sequence is std::make_integer_sequence");
  static_assert (std::is_same<sequence::index_sequence_for<int, char, void>,
                              std::index_sequence_for<int, char, void>>::value,
                 "index_sequence_for is std::index_sequence_for");
  static_assert (std::is_same<sequence::make_index_sequence<0>,
                              std::index_sequence<>>::value,
                 "the empty list");
  SUCCEED ();
#else
  GTEST_SKIP () << "the standard library has no integer sequences";
#endif
}

TEST (LumexIndexSequenceStdTest, GivenEitherSpelling_WhenPassed_ThenAccepted)
{
#if LUMEX_HAS_STD_INTEGER_SEQUENCE
  EXPECT_EQ (std_count (sequence::make_index_sequence<4> ()), 4U);
  EXPECT_EQ (lumex_count (std::make_index_sequence<6> ()), 6U);
  EXPECT_EQ (std_count (sequence::index_sequence_for<int, int> ()), 2U);
  EXPECT_EQ (lumex_count (std::index_sequence_for<int, int, int> ()), 3U);
  EXPECT_EQ (std_count (sequence::index_sequence<7> ()), 1U);
#else
  GTEST_SKIP () << "the standard library has no integer sequences";
#endif
}
