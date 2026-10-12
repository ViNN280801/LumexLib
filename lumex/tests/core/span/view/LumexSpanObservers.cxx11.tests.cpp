// LumexSpanObservers.cxx11.tests.cpp
//
// The member types, size / size_bytes / empty, element access (operator[],
// at, front, back, data), the iterators of all four kinds and the algorithms
// that run over a span.
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <numeric>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "LumexSpanTestSupport.hpp"
#include "lumex/core/span/LumexSpan"

using lumex::core::span::view::dynamic_extent;
using lumex::core::span::view::span;
using lumex_span_test::counted_element;

namespace
{
// Drops the result of a nodiscard member on purpose: the value is not the
// point of a throwing call.
template <typename T>
void
discard (T const &)
{
}
} // namespace

// -- Member types and constants --

TEST (LumexSpanObserversTest, GivenSpanOfInt_WhenMemberTypes_ThenAsStandard)
{
  using view_type = span<int>;
  static_assert (std::is_same<view_type::element_type, int>::value, "");
  static_assert (std::is_same<view_type::value_type, int>::value, "");
  static_assert (std::is_same<view_type::size_type, std::size_t>::value, "");
  static_assert (
      std::is_same<view_type::difference_type, std::ptrdiff_t>::value, "");
  static_assert (std::is_same<view_type::pointer, int *>::value, "");
  static_assert (std::is_same<view_type::const_pointer, int const *>::value,
                 "");
  static_assert (std::is_same<view_type::reference, int &>::value, "");
  static_assert (std::is_same<view_type::const_reference, int const &>::value,
                 "");
  static_assert (std::is_same<view_type::iterator, int *>::value, "");
  static_assert (std::is_same<view_type::const_iterator, int const *>::value,
                 "");
  static_assert (std::is_same<view_type::reverse_iterator,
                              std::reverse_iterator<int *>>::value,
                 "");
  static_assert (std::is_same<view_type::const_reverse_iterator,
                              std::reverse_iterator<int const *>>::value,
                 "");
  SUCCEED ();
}

TEST (LumexSpanObserversTest,
      GivenSpanOfConstVolatile_WhenMemberTypes_ThenValueTypeDropsQualifiers)
{
  using view_type = span<int const volatile>;
  static_assert (
      std::is_same<view_type::element_type, int const volatile>::value, "");
  static_assert (std::is_same<view_type::value_type, int>::value, "");
  static_assert (std::is_same<view_type::pointer, int const volatile *>::value,
                 "");
  static_assert (
      std::is_same<view_type::reference, int const volatile &>::value, "");
  SUCCEED ();
}

TEST (LumexSpanObserversTest, GivenSpan_WhenExtent_ThenTheTemplateArgument)
{
  static_assert (span<int>::extent == dynamic_extent, "");
  static_assert (span<int, 0>::extent == 0, "");
  static_assert (span<int, 7>::extent == 7, "");
  static_assert (
      std::is_same<decltype (span<int>::extent), std::size_t const>::value,
      "extent has the type size_type");
  // An odr-use of the constant must link at every standard.
  std::size_t const *address = &span<int, 7>::extent;
  EXPECT_EQ (*address, 7u);
}

// -- Size observers --

TEST (LumexSpanObserversTest, GivenSpan_WhenSize_ThenElementCount)
{
  int values[5] = { 1, 2, 3, 4, 5 };
  EXPECT_EQ ((span<int> (values, 3)).size (), 3u);
  EXPECT_EQ ((span<int> (values)).size (), 5u);
  EXPECT_EQ ((span<int, 5> (values)).size (), 5u);
  EXPECT_EQ ((span<int> (values, 0)).size (), 0u);
}

TEST (LumexSpanObserversTest,
      GivenSpan_WhenSizeBytes_ThenCountTimesElementSize)
{
  std::int32_t wide[5] = { 1, 2, 3, 4, 5 };
  EXPECT_EQ ((span<std::int32_t> (wide)).size_bytes (), 20u);
  EXPECT_EQ ((span<std::int32_t, 5> (wide)).size_bytes (), 20u);
  EXPECT_EQ ((span<std::int32_t> (wide, 0)).size_bytes (), 0u);
  char narrow[3] = { 'a', 'b', 'c' };
  EXPECT_EQ ((span<char> (narrow)).size_bytes (), 3u);
  double reals[2] = { 1.0, 2.0 };
  EXPECT_EQ ((span<double> (reals)).size_bytes (), 16u);
  struct odd_size
  {
    char bytes[7];
  };
  odd_size odd[3] = {};
  EXPECT_EQ ((span<odd_size> (odd)).size_bytes (), 21u);
}

TEST (LumexSpanObserversTest, GivenSpan_WhenEmpty_ThenSizeIsZero)
{
  int values[2] = { 1, 2 };
  EXPECT_TRUE ((span<int> ()).empty ());
  EXPECT_TRUE ((span<int, 0> ()).empty ());
  EXPECT_TRUE ((span<int> (values, 0)).empty ());
  EXPECT_FALSE ((span<int> (values)).empty ());
  EXPECT_FALSE ((span<int, 2> (values)).empty ());
}

TEST (LumexSpanObserversTest, GivenObservers_WhenNoexcept_ThenAllNoexcept)
{
  span<int> view;
  static_assert (noexcept (view.size ()), "");
  static_assert (noexcept (view.size_bytes ()), "");
  static_assert (noexcept (view.empty ()), "");
  static_assert (noexcept (view.data ()), "");
  static_assert (noexcept (view[0]), "");
  static_assert (noexcept (view.front ()), "");
  static_assert (noexcept (view.back ()), "");
  static_assert (noexcept (view.begin ()), "");
  static_assert (noexcept (view.end ()), "");
  static_assert (noexcept (view.cbegin ()), "");
  static_assert (noexcept (view.cend ()), "");
  static_assert (noexcept (view.rbegin ()), "");
  static_assert (noexcept (view.rend ()), "");
  static_assert (noexcept (view.crbegin ()), "");
  static_assert (noexcept (view.crend ()), "");
  static_assert (!noexcept (view.at (0)), "at may throw");
  SUCCEED ();
}

// -- Element access --

TEST (LumexSpanObserversTest, GivenSpan_WhenIndex_ThenReferenceToTheElement)
{
  int values[3] = { 10, 20, 30 };
  span<int> const view (values);
  EXPECT_EQ (view[0], 10);
  EXPECT_EQ (view[2], 30);
  EXPECT_EQ (&view[1], values + 1);
  view[1] = 99;
  EXPECT_EQ (values[1], 99);
}

TEST (LumexSpanObserversTest,
      GivenConstSpanObject_WhenIndex_ThenElementsStayWritable)
{
  int values[2] = { 1, 2 };
  span<int> const view (values);
  static_assert (std::is_same<decltype (view[0]), int &>::value,
                 "the constness of a span is shallow");
  static_assert (std::is_same<decltype (view.front ()), int &>::value, "");
  static_assert (std::is_same<decltype (view.back ()), int &>::value, "");
  static_assert (std::is_same<decltype (view.data ()), int *>::value, "");
  view.front () = 5;
  view.back () = 6;
  EXPECT_EQ (values[0], 5);
  EXPECT_EQ (values[1], 6);
}

TEST (LumexSpanObserversTest, GivenSpanOfConst_WhenElementAccess_ThenReadOnly)
{
  int values[2] = { 1, 2 };
  span<int const> const view (values);
  static_assert (std::is_same<decltype (view[0]), int const &>::value, "");
  static_assert (std::is_same<decltype (view.front ()), int const &>::value,
                 "");
  static_assert (std::is_same<decltype (view.at (0)), int const &>::value, "");
  static_assert (std::is_same<decltype (view.data ()), int const *>::value,
                 "");
  static_assert (!std::is_assignable<decltype (view[0]), int>::value,
                 "elements of a span of const are not assignable");
  EXPECT_EQ (view[1], 2);
}

TEST (LumexSpanObserversTest,
      GivenSpan_WhenFrontAndBack_ThenFirstAndLastElement)
{
  int values[4] = { 7, 8, 9, 10 };
  span<int> const view (values);
  EXPECT_EQ (view.front (), 7);
  EXPECT_EQ (view.back (), 10);
  EXPECT_EQ (&view.front (), values);
  EXPECT_EQ (&view.back (), values + 3);
  span<int> const single (values, 1);
  EXPECT_EQ (&single.front (), &single.back ());
}

TEST (LumexSpanObserversTest, GivenSpan_WhenData_ThenPointerToTheFirstElement)
{
  int values[3] = { 1, 2, 3 };
  EXPECT_EQ ((span<int> (values)).data (), values);
  EXPECT_EQ ((span<int> (values + 1, 2)).data (), values + 1);
  EXPECT_EQ ((span<int> ()).data (), nullptr);
}

TEST (LumexSpanObserversTest,
      GivenSpan_WhenAtInRange_ThenReferenceToTheElement)
{
  int values[3] = { 4, 5, 6 };
  span<int> const view (values);
  EXPECT_EQ (view.at (0), 4);
  EXPECT_EQ (view.at (2), 6);
  EXPECT_EQ (&view.at (1), values + 1);
  view.at (1) = 50;
  EXPECT_EQ (values[1], 50);
}

TEST (LumexSpanObserversTest, GivenSpan_WhenAtOutOfRange_ThenOutOfRange)
{
  int values[3] = { 4, 5, 6 };
  span<int> const view (values);
  EXPECT_THROW (discard (view.at (3)), std::out_of_range);
  EXPECT_THROW (discard (view.at (4)), std::out_of_range);
  EXPECT_THROW (discard (view.at (static_cast<std::size_t> (-1))),
                std::out_of_range);
  EXPECT_THROW (discard (view.at (dynamic_extent)), std::out_of_range);
}

TEST (LumexSpanObserversTest, GivenEmptySpan_WhenAt_ThenAlwaysOutOfRange)
{
  span<int> const empty_view;
  EXPECT_THROW (discard (empty_view.at (0)), std::out_of_range);
  span<int, 0> const zero;
  EXPECT_THROW (discard (zero.at (0)), std::out_of_range);
  int values[2] = { 1, 2 };
  span<int> const none (values, 0);
  EXPECT_THROW (discard (none.at (0)), std::out_of_range);
}

TEST (LumexSpanObserversTest, GivenStaticSpan_WhenAt_ThenChecksTheExtent)
{
  int values[2] = { 1, 2 };
  span<int, 2> const view (values);
  EXPECT_EQ (view.at (1), 2);
  EXPECT_THROW (discard (view.at (2)), std::out_of_range);
}

TEST (LumexSpanObserversTest, GivenAtFailure_WhenWhat_ThenNamesTheSpan)
{
  span<int> const view;
  try
    {
      discard (view.at (0));
      FAIL () << "at (0) of an empty span did not throw";
    }
  catch (std::out_of_range const &error)
    {
      EXPECT_NE (std::string (error.what ()).find ("span"), std::string::npos);
    }
}

TEST (LumexSpanObserversTest, GivenAtFailure_WhenCaught_ThenAsLogicError)
{
  span<int> const view;
  EXPECT_THROW (discard (view.at (1)), std::logic_error);
  EXPECT_THROW (discard (view.at (1)), std::exception);
}

// -- Iterators --

TEST (LumexSpanObserversTest, GivenSpan_WhenBeginEnd_ThenPointers)
{
  int values[3] = { 1, 2, 3 };
  span<int> const view (values);
  EXPECT_EQ (view.begin (), values);
  EXPECT_EQ (view.end (), values + 3);
  EXPECT_EQ (view.cbegin (), values);
  EXPECT_EQ (view.cend (), values + 3);
  EXPECT_EQ (std::distance (view.begin (), view.end ()), 3);
  EXPECT_EQ (static_cast<std::size_t> (view.end () - view.begin ()),
             view.size ());
}

TEST (LumexSpanObserversTest, GivenEmptySpan_WhenBeginEnd_ThenEqual)
{
  EXPECT_EQ ((span<int> ()).begin (), (span<int> ()).end ());
  EXPECT_EQ ((span<int, 0> ()).cbegin (), (span<int, 0> ()).cend ());
  EXPECT_EQ ((span<int> ()).rbegin (), (span<int> ()).rend ());
  EXPECT_EQ ((span<int> ()).crbegin (), (span<int> ()).crend ());
}

TEST (LumexSpanObserversTest,
      GivenSpan_WhenConstIterators_ThenElementsAreConst)
{
  int values[2] = { 1, 2 };
  span<int> const view (values);
  static_assert (std::is_same<decltype (view.cbegin ()), int const *>::value,
                 "cbegin gives a constant iterator");
  static_assert (std::is_same<decltype (view.cend ()), int const *>::value,
                 "");
  static_assert (std::is_same<decltype (*view.cbegin ()), int const &>::value,
                 "");
  static_assert (std::is_same<decltype (*view.begin ()), int &>::value, "");
  static_assert (std::is_same<decltype (view.crbegin ()),
                              std::reverse_iterator<int const *>>::value,
                 "");
  static_assert (!std::is_assignable<decltype (*view.cbegin ()), int>::value,
                 "");
  SUCCEED ();
}

TEST (LumexSpanObserversTest, GivenSpan_WhenReverseIterators_ThenBackwards)
{
  int values[4] = { 1, 2, 3, 4 };
  span<int> const view (values);
  std::vector<int> reversed (view.rbegin (), view.rend ());
  EXPECT_EQ (reversed, (std::vector<int>{ 4, 3, 2, 1 }));
  std::vector<int> const_reversed (view.crbegin (), view.crend ());
  EXPECT_EQ (const_reversed, reversed);
  EXPECT_EQ (&*view.rbegin (), values + 3);
  EXPECT_EQ (&*(view.rend () - 1), values);
}

TEST (LumexSpanObserversTest, GivenSpan_WhenRangeFor_ThenVisitsInOrder)
{
  int values[4] = { 1, 2, 3, 4 };
  span<int> const view (values);
  int total = 0;
  int last = 0;
  for (int element : view)
    {
      total += element;
      EXPECT_GT (element, last);
      last = element;
    }
  EXPECT_EQ (total, 10);
  for (int &element : view)
    {
      element *= 2;
    }
  EXPECT_EQ (values[3], 8);
}

TEST (LumexSpanObserversTest, GivenSpanOfConst_WhenRangeFor_ThenReadsElements)
{
  std::vector<int> const values = { 3, 4, 5 };
  span<int const> const view (values);
  int total = 0;
  for (int element : view)
    {
      total += element;
    }
  EXPECT_EQ (total, 12);
}

TEST (LumexSpanObserversTest,
      GivenSpan_WhenIteratorTraits_ThenRandomAccessOverTheElementType)
{
  using iterator = span<int>::iterator;
  static_assert (
      std::is_same<std::iterator_traits<iterator>::iterator_category,
                   std::random_access_iterator_tag>::value,
      "");
  static_assert (
      std::is_same<std::iterator_traits<iterator>::value_type, int>::value,
      "");
  static_assert (
      std::is_same<std::iterator_traits<iterator>::reference, int &>::value,
      "");
  static_assert (std::is_same<std::iterator_traits<iterator>::difference_type,
                              std::ptrdiff_t>::value,
                 "");
  SUCCEED ();
}

// -- Algorithms and containers over a span --

TEST (LumexSpanObserversTest, GivenSpan_WhenSort_ThenSortsTheViewedStorage)
{
  std::vector<int> values = { 5, 3, 9, 1, 7 };
  span<int> const view (values);
  std::sort (view.begin (), view.end ());
  EXPECT_EQ (values, (std::vector<int>{ 1, 3, 5, 7, 9 }));
  std::reverse (view.begin (), view.end ());
  EXPECT_EQ (values, (std::vector<int>{ 9, 7, 5, 3, 1 }));
}

TEST (LumexSpanObserversTest,
      GivenSpan_WhenCopyAndFill_ThenWritesThroughTheView)
{
  int source[3] = { 1, 2, 3 };
  int target[3] = { 0, 0, 0 };
  span<int const> const from (source);
  span<int> const to (target);
  std::copy (from.begin (), from.end (), to.begin ());
  EXPECT_TRUE (std::equal (from.begin (), from.end (), target));
  std::fill (to.begin (), to.end (), 7);
  EXPECT_EQ (target[0], 7);
  EXPECT_EQ (target[2], 7);
}

TEST (LumexSpanObserversTest,
      GivenSpan_WhenAccumulateAndFind_ThenStandardAlgorithmsWork)
{
  std::array<int, 5> values = { { 4, 8, 15, 16, 23 } };
  span<int const> const view (values);
  EXPECT_EQ (std::accumulate (view.begin (), view.end (), 0), 66);
  EXPECT_EQ (std::find (view.begin (), view.end (), 15) - view.begin (), 2);
  EXPECT_EQ (std::find (view.begin (), view.end (), 99), view.end ());
  EXPECT_TRUE (std::binary_search (view.begin (), view.end (), 16));
  EXPECT_EQ (std::count_if (view.begin (), view.end (),
                            [] (int value) { return value % 2 == 0; }),
             3);
}

TEST (LumexSpanObserversTest,
      GivenSpan_WhenAssignedToVector_ThenCopiesTheElements)
{
  int values[3] = { 1, 2, 3 };
  span<int const> const view (values);
  std::vector<int> copy (view.begin (), view.end ());
  EXPECT_EQ (copy, (std::vector<int>{ 1, 2, 3 }));
  copy.assign (view.rbegin (), view.rend ());
  EXPECT_EQ (copy, (std::vector<int>{ 3, 2, 1 }));
  std::string text;
  char const letters[] = { 'a', 'b', 'c' };
  span<char const> const letter_view (letters);
  text.assign (letter_view.begin (), letter_view.end ());
  EXPECT_EQ (text, "abc");
}

TEST (LumexSpanObserversTest,
      GivenSpanOfClassType_WhenIterated_ThenNoElementIsCopied)
{
  counted_element elements[3]
      = { counted_element (1), counted_element (2), counted_element (3) };
  counted_element::copies () = 0;
  span<counted_element> const view (elements);
  int total = 0;
  for (counted_element const &element : view)
    {
      total += element.value;
    }
  EXPECT_EQ (total, 6);
  EXPECT_EQ (view.front ().value, 1);
  EXPECT_EQ (view.back ().value, 3);
  EXPECT_EQ (view.subspan (1).size (), 2u);
  span<counted_element> const copy = view;
  EXPECT_EQ (copy.size (), 3u);
  EXPECT_EQ (counted_element::copies (), 0);
}

TEST (LumexSpanObserversTest,
      GivenSpanOfStrings_WhenModified_ThenOriginalChanges)
{
  std::vector<std::string> words = { "a", "b", "c" };
  span<std::string> const view (words);
  view[1] += "x";
  view.back () = "z";
  EXPECT_EQ (words[1], "bx");
  EXPECT_EQ (words[2], "z");
}
