// lumex/tests/core/utility/ranges/LumexIteratorRange.cxx11.tests.cpp
// iterator_range of LumexIteratorRange.hpp: a pair of iterators as a range
// for a range-based for loop. It works from C++11 and holds nothing but the
// two iterators, so the tests walk raw pointers, vector and list iterators,
// reverse iterators and a single-pass input iterator.
#include <cstddef>
#include <iterator>
#include <list>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/ranges/LumexIteratorRange.hpp"

using lumex::core::utility::ranges::iterator_range;

TEST (IteratorRangeTest,
      GivenAnArray_WhenRangeFor_ThenVisitsEveryElementInOrder)
{
  int const values[] = { 3, 1, 4, 1, 5 };
  iterator_range<int const *> const range (values, values + 5);
  std::vector<int> seen;
  for (int value : range)
    seen.push_back (value);
  ASSERT_EQ (seen.size (), 5U);
  EXPECT_EQ (seen[0], 3);
  EXPECT_EQ (seen[1], 1);
  EXPECT_EQ (seen[2], 4);
  EXPECT_EQ (seen[3], 1);
  EXPECT_EQ (seen[4], 5);
}

TEST (IteratorRangeTest,
      GivenAVector_WhenBeginAndEnd_ThenTheyAreTheStoredIterators)
{
  std::vector<std::string> const names (3, "x");
  iterator_range<std::vector<std::string>::const_iterator> const range (
      names.begin () + 1, names.end ());
  EXPECT_TRUE (range.begin () == names.begin () + 1);
  EXPECT_TRUE (range.end () == names.end ());
  EXPECT_EQ (std::distance (range.begin (), range.end ()), 2);
}

TEST (IteratorRangeTest,
      GivenEqualIterators_WhenEmpty_ThenTrueAndTheLoopDoesNotRun)
{
  std::vector<int> const values (4, 7);
  iterator_range<std::vector<int>::const_iterator> const range (
      values.begin () + 2, values.begin () + 2);
  EXPECT_TRUE (range.empty ());
  int visits = 0;
  for (int value : range)
    {
      (void)value;
      ++visits;
    }
  EXPECT_EQ (visits, 0);
}

TEST (IteratorRangeTest, GivenDifferentIterators_WhenEmpty_ThenFalse)
{
  std::vector<int> const values (4, 7);
  iterator_range<std::vector<int>::const_iterator> const range (
      values.begin (), values.begin () + 1);
  EXPECT_FALSE (range.empty ());
  iterator_range<std::vector<int>::const_iterator> const all (values.begin (),
                                                              values.end ());
  EXPECT_FALSE (all.empty ());
}

TEST (
    IteratorRangeTest,
    GivenAList_WhenRangeFor_ThenTheElementsAreModifiableThroughANonConstIterator)
{
  std::list<int> values;
  values.push_back (1);
  values.push_back (2);
  values.push_back (3);
  iterator_range<std::list<int>::iterator> const range (values.begin (),
                                                        values.end ());
  for (int &value : range)
    value *= 10;
  std::list<int>::const_iterator it = values.begin ();
  EXPECT_EQ (*it++, 10);
  EXPECT_EQ (*it++, 20);
  EXPECT_EQ (*it++, 30);
}

TEST (IteratorRangeTest,
      GivenReverseIterators_WhenRangeFor_ThenTheElementsComeBackwards)
{
  std::vector<int> const values = { 1, 2, 3 };
  typedef std::vector<int>::const_reverse_iterator reverse_t;
  iterator_range<reverse_t> const range (values.rbegin (), values.rend ());
  std::vector<int> seen;
  for (int value : range)
    seen.push_back (value);
  ASSERT_EQ (seen.size (), 3U);
  EXPECT_EQ (seen[0], 3);
  EXPECT_EQ (seen[1], 2);
  EXPECT_EQ (seen[2], 1);
}

TEST (IteratorRangeTest,
      GivenAnInputIteratorPair_WhenRangeFor_ThenTheStreamIsReadOnce)
{
  std::istringstream stream ("5 6 7");
  std::istream_iterator<int> const first (stream);
  std::istream_iterator<int> const last;
  iterator_range<std::istream_iterator<int>> const range (first, last);
  std::vector<int> seen;
  for (int value : range)
    seen.push_back (value);
  ASSERT_EQ (seen.size (), 3U);
  EXPECT_EQ (seen[0], 5);
  EXPECT_EQ (seen[2], 7);
}

TEST (IteratorRangeTest,
      GivenARange_WhenCopied_ThenTheCopyDescribesTheSameSequence)
{
  int const values[] = { 1, 2, 3 };
  iterator_range<int const *> const first (values, values + 3);
  iterator_range<int const *> const copy (first);
  EXPECT_TRUE (copy.begin () == first.begin ());
  EXPECT_TRUE (copy.end () == first.end ());
  iterator_range<int const *> assigned (values, values);
  assigned = first;
  EXPECT_FALSE (assigned.empty ());
  EXPECT_TRUE (assigned.end () == values + 3);
}

TEST (IteratorRangeTest, GivenTheTypedefs_WhenRead_ThenBothAreTheIteratorType)
{
  typedef iterator_range<int const *> range_t;
  static_assert (std::is_same<range_t::iterator, int const *>::value,
                 "iterator is the template argument");
  static_assert (std::is_same<range_t::const_iterator, int const *>::value,
                 "const_iterator is the template argument");
  static_assert (std::is_same<decltype (range_t (nullptr, nullptr).begin ()),
                              int const *>::value,
                 "begin () returns the iterator");
  static_assert (std::is_trivially_copyable<range_t>::value,
                 "a range of pointers is trivially copyable");
  SUCCEED ();
}
