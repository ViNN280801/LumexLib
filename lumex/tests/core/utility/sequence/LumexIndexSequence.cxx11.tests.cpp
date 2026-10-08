// lumex/tests/core/utility/sequence/LumexIndexSequence.cxx11.tests.cpp
// integer_sequence, index_sequence, make_integer_sequence, make_index_sequence
// and index_sequence_for of LumexIndexSequence.hpp. Below C++14 they are the
// class and the alias templates of this library, from C++14 the aliases of the
// standard ones; the tests describe what both have in common (the lists, the
// members, the deduction), so the same file runs in every suite. The tests of
// the own form alone (the refused counts) are compiled only where it is used.
#include <array>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/sequence/LumexIndexSequence.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

namespace sequence = lumex::core::utility::sequence;

namespace
{
/** The numbers of a sequence, in order. */
template <typename T, T... Values>
std::vector<T>
to_vector (sequence::integer_sequence<T, Values...>)
{
  T const values[] = { Values..., T () };
  return std::vector<T> (values, values + sizeof...(Values));
}

/** Deduces the indices out of the alias. */
template <std::size_t... I>
std::size_t
sum_of (sequence::index_sequence<I...>)
{
  std::size_t const values[] = { I..., 0 };
  std::size_t total = 0;
  for (std::size_t value : values)
    total += value;
  return total;
}

template <typename T, T... Values>
std::size_t
count_of (sequence::integer_sequence<T, Values...>)
{
  return sizeof...(Values);
}

template <std::size_t... I>
std::array<std::size_t, sizeof...(I)>
make_squares (sequence::index_sequence<I...>)
{
  return std::array<std::size_t, sizeof...(I)>{ { (I * I)... } };
}

int
add3 (int first, int second, int third)
{
  return first * 100 + second * 10 + third;
}

template <typename Tuple, std::size_t... I>
int
call_with_tuple (Tuple const &tuple, sequence::index_sequence<I...>)
{
  return add3 (std::get<I> (tuple)...);
}

/** Does `make_integer_sequence<T, Count>` name a type? */
template <typename T, T Count, typename Enable = void>
struct can_make : std::false_type
{
};

template <typename T, T Count>
struct can_make<T, Count,
                lumex::core::utility::traits::meta::void_t<
                    sequence::make_integer_sequence<T, Count>>>
    : std::true_type
{
};
} // namespace

TEST (LumexIndexSequenceTest,
      GivenSmallCounts_WhenMakeIndexSequence_ThenIndices)
{
  static_assert (std::is_same<sequence::make_index_sequence<0>,
                              sequence::index_sequence<>>::value,
                 "0 gives the empty list");
  static_assert (std::is_same<sequence::make_index_sequence<1>,
                              sequence::index_sequence<0>>::value,
                 "1 gives 0");
  static_assert (std::is_same<sequence::make_index_sequence<2>,
                              sequence::index_sequence<0, 1>>::value,
                 "2 gives 0 1");
  static_assert (std::is_same<sequence::make_index_sequence<3>,
                              sequence::index_sequence<0, 1, 2>>::value,
                 "3 gives 0 1 2");
  static_assert (std::is_same<sequence::make_index_sequence<5>,
                              sequence::index_sequence<0, 1, 2, 3, 4>>::value,
                 "5 gives 0 .. 4");
  static_assert (
      std::is_same<sequence::make_index_sequence<9>,
                   sequence::index_sequence<0, 1, 2, 3, 4, 5, 6, 7, 8>>::value,
      "9 gives 0 .. 8");
  EXPECT_TRUE ((std::is_same<sequence::make_index_sequence<4>,
                             sequence::index_sequence<0, 1, 2, 3>>::value));
}

/** Counts every index of `make_index_sequence<Count>` once, in order. */
template <std::size_t Count>
void
expect_indices_in_order ()
{
  std::vector<std::size_t> const values
      = to_vector (sequence::make_index_sequence<Count> ());
  ASSERT_EQ (values.size (), Count);
  for (std::size_t index = 0; index < values.size (); ++index)
    ASSERT_EQ (values[index], index);
}

TEST (LumexIndexSequenceTest,
      GivenManyCounts_WhenMakeIndexSequence_ThenEveryIndexInOrder)
{
  // Counts around the powers of two, where a halving builder joins halves of
  // different lengths, and a count in the thousands.
  expect_indices_in_order<6> ();
  expect_indices_in_order<7> ();
  expect_indices_in_order<8> ();
  expect_indices_in_order<15> ();
  expect_indices_in_order<16> ();
  expect_indices_in_order<17> ();
  expect_indices_in_order<31> ();
  expect_indices_in_order<33> ();
  expect_indices_in_order<100> ();
  expect_indices_in_order<255> ();
  expect_indices_in_order<256> ();
  expect_indices_in_order<257> ();
  expect_indices_in_order<1000> ();
  expect_indices_in_order<4096> ();
}

TEST (LumexIndexSequenceTest,
      GivenIntegerTypes_WhenMakeIntegerSequence_ThenTypeAndValues)
{
  static_assert (
      std::is_same<sequence::make_integer_sequence<int, 4>,
                   sequence::integer_sequence<int, 0, 1, 2, 3>>::value,
      "int");
  static_assert (
      std::is_same<sequence::make_integer_sequence<unsigned char, 3>,
                   sequence::integer_sequence<unsigned char, 0, 1, 2>>::value,
      "unsigned char");
  static_assert (
      std::is_same<sequence::make_integer_sequence<long long, 2>,
                   sequence::integer_sequence<long long, 0, 1>>::value,
      "long long");
  static_assert (
      std::is_same<
          sequence::make_integer_sequence<signed char, 5>,
          sequence::integer_sequence<signed char, 0, 1, 2, 3, 4>>::value,
      "signed char");
  static_assert (std::is_same<sequence::make_integer_sequence<short, 0>,
                              sequence::integer_sequence<short>>::value,
                 "empty");
  static_assert (std::is_same<sequence::make_integer_sequence<std::size_t, 3>,
                              sequence::index_sequence<0, 1, 2>>::value,
                 "index_sequence is integer_sequence<std::size_t, ...>");
  static_assert (
      std::is_same<sequence::integer_sequence<int, 7>::value_type, int>::value,
      "value_type is T");
  static_assert (std::is_same<sequence::index_sequence<7>::value_type,
                              std::size_t>::value,
                 "value_type of index_sequence is std::size_t");

  std::vector<int> const ints
      = to_vector (sequence::make_integer_sequence<int, 5> ());
  ASSERT_EQ (ints.size (), 5U);
  for (std::size_t index = 0; index < ints.size (); ++index)
    EXPECT_EQ (ints[index], static_cast<int> (index));
  // GCC 8 refuses std::make_integer_sequence<unsigned char, N> for N above 127
  // (its __integer_pack reads the count as signed), so the byte count stays
  // below that and the wider type goes beyond the range of a byte.
  std::vector<unsigned char> const bytes
      = to_vector (sequence::make_integer_sequence<unsigned char, 100> ());
  ASSERT_EQ (bytes.size (), 100U);
  for (std::size_t index = 0; index < bytes.size (); ++index)
    EXPECT_EQ (bytes[index], static_cast<unsigned char> (index));
  std::vector<unsigned short> const shorts
      = to_vector (sequence::make_integer_sequence<unsigned short, 300> ());
  ASSERT_EQ (shorts.size (), 300U);
  for (std::size_t index = 0; index < shorts.size (); ++index)
    EXPECT_EQ (shorts[index], static_cast<unsigned short> (index));
}

TEST (LumexIndexSequenceTest,
      GivenPack_WhenIndexSequenceFor_ThenOneIndexPerType)
{
  static_assert (std::is_same<sequence::index_sequence_for<>,
                              sequence::index_sequence<>>::value,
                 "no types");
  static_assert (std::is_same<sequence::index_sequence_for<char>,
                              sequence::index_sequence<0>>::value,
                 "one type");
  static_assert (std::is_same<sequence::index_sequence_for<int, char, double>,
                              sequence::index_sequence<0, 1, 2>>::value,
                 "three types");
  static_assert (std::is_same<sequence::index_sequence_for<int, int, int, int>,
                              sequence::make_index_sequence<4>>::value,
                 "the same count as make_index_sequence");
  EXPECT_EQ (count_of (sequence::index_sequence_for<void, void> ()), 2U);
}

TEST (LumexIndexSequenceTest,
      GivenSequences_WhenSize_ThenCountInConstantExpression)
{
  static_assert (sequence::make_index_sequence<0>::size () == 0, "empty");
  static_assert (sequence::make_index_sequence<7>::size () == 7, "seven");
  static_assert (sequence::index_sequence<4, 5>::size () == 2, "two");
  static_assert (sequence::integer_sequence<int>::size () == 0, "none");
  static_assert (sequence::make_integer_sequence<int, 12>::size () == 12,
                 "twelve");
  EXPECT_EQ (sequence::make_index_sequence<7>::size (), 7U);
  EXPECT_EQ (sequence::make_index_sequence<7> ().size (), 7U);
  EXPECT_EQ ((sequence::index_sequence<1, 1, 1> ().size ()), 3U);
}

TEST (LumexIndexSequenceTest, GivenAliases_WhenDeducing_ThenPackIsDeduced)
{
  // A function parameter spelled with the alias deduces the indices of a
  // value built with another spelling.
  EXPECT_EQ (sum_of (sequence::make_index_sequence<5> ()), 10U);
  EXPECT_EQ (sum_of (sequence::index_sequence<3, 4> ()), 7U);
  EXPECT_EQ (sum_of (sequence::index_sequence_for<char, char, char> ()), 3U);
  EXPECT_EQ (sum_of (sequence::make_integer_sequence<std::size_t, 4> ()), 6U);
  EXPECT_EQ (sum_of (sequence::index_sequence<> ()), 0U);
  EXPECT_EQ (count_of (sequence::make_integer_sequence<long, 6> ()), 6U);
  EXPECT_EQ (count_of (sequence::make_index_sequence<0> ()), 0U);
}

TEST (LumexIndexSequenceTest, GivenTuple_WhenExpanded_ThenEveryElementReached)
{
  std::tuple<int, int, int> const tuple (1, 2, 3);
  EXPECT_EQ (call_with_tuple (tuple, sequence::make_index_sequence<3> ()),
             123);
  EXPECT_EQ (
      call_with_tuple (tuple, sequence::index_sequence_for<int, int, int> ()),
      123);
  EXPECT_EQ (call_with_tuple (tuple, sequence::index_sequence<2, 1, 0> ()),
             321);
}

TEST (LumexIndexSequenceTest, GivenTableSize_WhenExpanded_ThenTableBuilt)
{
  // The way a compile-time table is filled: one initializer per index.
  std::array<std::size_t, 6> const squares
      = make_squares (sequence::make_index_sequence<6> ());
  EXPECT_EQ (squares[0], 0U);
  EXPECT_EQ (squares[1], 1U);
  EXPECT_EQ (squares[2], 4U);
  EXPECT_EQ (squares[3], 9U);
  EXPECT_EQ (squares[4], 16U);
  EXPECT_EQ (squares[5], 25U);
}

TEST (LumexIndexSequenceTest,
      GivenSequenceTypes_WhenInspected_ThenEmptyAndTrivial)
{
  typedef sequence::make_index_sequence<8> eight_t;
  static_assert (std::is_empty<eight_t>::value, "no data");
  static_assert (std::is_default_constructible<eight_t>::value, "default");
  static_assert (std::is_copy_constructible<eight_t>::value, "copy");
  static_assert (std::is_trivially_copyable<eight_t>::value, "trivial");
  static_assert (std::is_class<eight_t>::value, "a class");
  // A literal type: it can be a constant.
  constexpr eight_t instance = eight_t ();
  EXPECT_EQ (instance.size (), 8U);
  EXPECT_EQ (sizeof (eight_t), sizeof (sequence::index_sequence<>));
}

#if !LUMEX_HAS_STD_INTEGER_SEQUENCE
TEST (LumexIndexSequenceTest,
      GivenBadCounts_WhenMakeIntegerSequence_ThenNoType)
{
  // The own form refuses what would run a compiler out of memory: a negative
  // count converts to an unsigned one near 2^64, and N - 1 with N == 0 is the
  // usual way to get there. The alias then has no type (SFINAE-friendly).
  EXPECT_TRUE ((can_make<int, 0>::value));
  EXPECT_TRUE ((can_make<int, 1>::value));
  EXPECT_TRUE ((can_make<int, 4096>::value));
  EXPECT_FALSE ((can_make<int, -1>::value));
  EXPECT_FALSE ((can_make<int, -4>::value));
  EXPECT_FALSE ((can_make<long long, -1>::value));
  EXPECT_FALSE ((can_make<signed char, -128>::value));
  EXPECT_FALSE ((can_make<std::size_t, static_cast<std::size_t> (-1)>::value));
  EXPECT_FALSE ((can_make<std::size_t, 16385>::value));
  // Clang 23 turns a list of 32768 or 65536 integers into an empty one
  // without a diagnostic, so the limit has to keep the count away from them.
  EXPECT_FALSE ((can_make<std::size_t, 32768>::value));
  EXPECT_FALSE ((can_make<std::size_t, 65536>::value));
  EXPECT_FALSE ((can_make<std::size_t, 65537>::value));
  EXPECT_FALSE ((can_make<long long, 100000000000LL>::value));
}

TEST (LumexIndexSequenceTest,
      GivenLimitCount_WhenMakeIntegerSequence_ThenBuilt)
{
  // The limit itself is a valid count.
  EXPECT_TRUE ((can_make<std::size_t, 16384>::value));
  EXPECT_EQ (sequence::make_index_sequence<16384>::size (), 16384U);
}
#endif
