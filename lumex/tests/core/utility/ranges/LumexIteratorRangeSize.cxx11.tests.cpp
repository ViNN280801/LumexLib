// lumex/tests/core/utility/ranges/LumexIteratorRangeSize.cxx11.tests.cpp
// iterator_range::size (): present for random-access iterators only, as
// end () - begin () in std::size_t. A range over a std::list, std::set,
// std::forward_list or a single-pass iterator has no size (), because counting
// those would walk the sequence. The member is a template with a default
// argument, so the constraint is a substitution failure of the member and not
// an error of the class: a range over an iterator without iterator_traits is
// still usable. Detection is by the compiler (a rejected call is not
// compiled).
#include <array>
#include <cstddef>
#include <deque>
#include <forward_list>
#include <iterator>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/ranges/LumexIteratorRange.hpp"

using lumex::core::utility::ranges::iterator_range;

namespace
{
template <typename Range, typename = void> struct has_size : std::false_type
{
};

template <typename Range>
struct has_size<Range,
                decltype (void (std::declval<Range const &> ().size ()))>
    : std::true_type
{
};

// A forward iterator that declares no iterator_traits members at all.
struct bare_iterator
{
  int
  operator* () const
  {
    return 0;
  }
  bare_iterator &
  operator++ ()
  {
    return *this;
  }
  bool
  operator== (bare_iterator const &) const
  {
    return true;
  }
  bool
  operator!= (bare_iterator const &) const
  {
    return false;
  }
};

// A random-access iterator written by hand: only the category and the
// difference make it one for size ().
struct index_iterator
{
  using iterator_category = std::random_access_iterator_tag;
  using value_type = int;
  using difference_type = std::ptrdiff_t;
  using pointer = int const *;
  using reference = int;

  std::ptrdiff_t position;

  int
  operator* () const
  {
    return static_cast<int> (position);
  }
  index_iterator &
  operator++ ()
  {
    ++position;
    return *this;
  }
  bool
  operator== (index_iterator const &other) const
  {
    return position == other.position;
  }
  bool
  operator!= (index_iterator const &other) const
  {
    return position != other.position;
  }
  std::ptrdiff_t
  operator- (index_iterator const &other) const
  {
    return position - other.position;
  }
};

// A category that only derives from the random-access tag (like C++20
// contiguous_iterator_tag does).
struct derived_category_tag : std::random_access_iterator_tag
{
};

struct derived_category_iterator : index_iterator
{
  using iterator_category = derived_category_tag;

  explicit derived_category_iterator (std::ptrdiff_t start)
      : index_iterator{ start }
  {
  }
};
} // namespace

TEST (IteratorRangeSizeTest,
      GivenRandomAccessIterators_WhenSizeDetected_ThenPresent)
{
  EXPECT_TRUE (has_size<iterator_range<int *>>::value);
  EXPECT_TRUE (has_size<iterator_range<int const *>>::value);
  EXPECT_TRUE (
      (has_size<iterator_range<std::vector<int>::const_iterator>>::value));
  EXPECT_TRUE ((has_size<iterator_range<std::vector<int>::iterator>>::value));
  EXPECT_TRUE (
      (has_size<iterator_range<std::deque<int>::const_iterator>>::value));
  EXPECT_TRUE ((has_size<iterator_range<std::string::const_iterator>>::value));
  EXPECT_TRUE (
      (has_size<iterator_range<std::array<int, 3>::const_iterator>>::value));
  EXPECT_TRUE (
      (has_size<
          iterator_range<std::vector<int>::const_reverse_iterator>>::value));
  EXPECT_TRUE (has_size<iterator_range<index_iterator>>::value);
  EXPECT_TRUE (has_size<iterator_range<derived_category_iterator>>::value);
}

TEST (IteratorRangeSizeTest, GivenOtherIterators_WhenSizeDetected_ThenAbsent)
{
  EXPECT_FALSE (
      (has_size<iterator_range<std::list<int>::const_iterator>>::value));
  EXPECT_FALSE (
      (has_size<iterator_range<std::set<int>::const_iterator>>::value));
  EXPECT_FALSE (
      (has_size<iterator_range<std::map<int, int>::const_iterator>>::value));
  EXPECT_FALSE (
      (has_size<
          iterator_range<std::forward_list<int>::const_iterator>>::value));
  EXPECT_FALSE ((has_size<iterator_range<std::istream_iterator<int>>>::value));
  EXPECT_FALSE (has_size<iterator_range<bare_iterator>>::value);
}

TEST (IteratorRangeSizeTest,
      GivenRangeOverBareIterator_WhenUsed_ThenStillWorks)
{
  // The class is usable although the iterator has no iterator_traits; only
  // size () is absent.
  iterator_range<bare_iterator> const range ((bare_iterator ()),
                                             (bare_iterator ()));
  EXPECT_TRUE (range.empty ());
}

TEST (IteratorRangeSizeTest, GivenVectorRange_WhenSize_ThenCountOfElements)
{
  std::vector<int> const values = { 5, 6, 7, 8 };
  iterator_range<std::vector<int>::const_iterator> const range (
      values.begin (), values.end ());
  EXPECT_EQ (range.size (), static_cast<std::size_t> (4));
  EXPECT_EQ (range.size (), values.size ());
  static_assert (std::is_same<decltype (range.size ()), std::size_t>::value,
                 "size () is std::size_t, like the size () of a vector");
}

TEST (IteratorRangeSizeTest, GivenEmptyRange_WhenSize_ThenZeroAndEmpty)
{
  std::vector<int> const none;
  iterator_range<std::vector<int>::const_iterator> const range (none.begin (),
                                                                none.end ());
  EXPECT_EQ (range.size (), static_cast<std::size_t> (0));
  EXPECT_TRUE (range.empty ());
}

TEST (IteratorRangeSizeTest, GivenPointers_WhenSize_ThenDistance)
{
  int const values[] = { 1, 2, 3, 4, 5, 6 };
  iterator_range<int const *> const whole (values, values + 6);
  iterator_range<int const *> const middle (values + 2, values + 5);
  iterator_range<int const *> const none (values + 3, values + 3);
  EXPECT_EQ (whole.size (), static_cast<std::size_t> (6));
  EXPECT_EQ (middle.size (), static_cast<std::size_t> (3));
  EXPECT_EQ (none.size (), static_cast<std::size_t> (0));
  EXPECT_EQ (middle.size (), static_cast<std::size_t> (std::distance (
                                 middle.begin (), middle.end ())));
}

TEST (IteratorRangeSizeTest, GivenDequeAndString_WhenSize_ThenCountOfElements)
{
  std::deque<int> const queue (7, 1);
  iterator_range<std::deque<int>::const_iterator> const queue_range (
      queue.begin (), queue.end ());
  EXPECT_EQ (queue_range.size (), static_cast<std::size_t> (7));

  std::string const text = "range";
  iterator_range<std::string::const_iterator> const text_range (text.begin (),
                                                                text.end ());
  EXPECT_EQ (text_range.size (), static_cast<std::size_t> (5));
}

TEST (IteratorRangeSizeTest,
      GivenReverseIterators_WhenSize_ThenCountOfElements)
{
  std::vector<int> const values = { 1, 2, 3 };
  iterator_range<std::vector<int>::const_reverse_iterator> const range (
      values.rbegin (), values.rend ());
  EXPECT_EQ (range.size (), static_cast<std::size_t> (3));
}

TEST (IteratorRangeSizeTest, GivenHandWrittenIterators_WhenSize_ThenDifference)
{
  iterator_range<index_iterator> const range (index_iterator{ 10 },
                                              index_iterator{ 25 });
  EXPECT_EQ (range.size (), static_cast<std::size_t> (15));
  iterator_range<derived_category_iterator> const derived (
      derived_category_iterator (3), derived_category_iterator (4));
  EXPECT_EQ (derived.size (), static_cast<std::size_t> (1));
}

TEST (IteratorRangeSizeTest, GivenSizeAndAWalk_WhenCounted_ThenTheyAgree)
{
  std::vector<int> const values = { 9, 8, 7, 6, 5 };
  iterator_range<std::vector<int>::const_iterator> const range (
      values.begin (), values.end ());
  std::size_t counted = 0;
  for (std::vector<int>::const_iterator at = range.begin ();
       at != range.end (); ++at)
    ++counted;
  EXPECT_EQ (counted, range.size ());
}
