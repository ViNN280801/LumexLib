// LumexSpanRanges.cxx20.tests.cpp
//
// span in the C++20 library: the range concepts and the opt-ins, the
// standard algorithms and views over a span, the constructors from standard
// iterators and sentinels, and the conversions between the span of this
// library and std::span, in both directions. The tests skip where the
// toolchain claims C++20 without the library part (GCC 8 has no <span>,
// <ranges> or concepts at -std=c++2a).
#include <array>
#include <cstddef>
#include <deque>
#include <iterator>
#include <list>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#if __has_include(<ranges>)
#include <algorithm>
#include <concepts>
#include <ranges>
#endif
#if __has_include(<span>)
#include <span>
#endif

#include <gtest/gtest.h>

#include "lumex/core/span/LumexSpan"

using lumex::core::span::view::dynamic_extent;
using lumex::core::span::view::span;

#if LUMEX_SPAN_HAS_RANGES

// -- Concepts and opt-ins --

TEST (LumexSpanRangesTest,
      GivenSpan_WhenRangeConcepts_ThenContiguousSizedBorrowedView)
{
  static_assert (std::ranges::range<span<int>>);
  static_assert (std::ranges::sized_range<span<int>>);
  static_assert (std::ranges::common_range<span<int>>);
  static_assert (std::ranges::random_access_range<span<int>>);
  static_assert (std::ranges::contiguous_range<span<int>>);
  static_assert (std::ranges::borrowed_range<span<int>>);
  static_assert (std::ranges::view<span<int>>);
  static_assert (std::ranges::viewable_range<span<int>>);
  static_assert (std::ranges::output_range<span<int>, int>);
  static_assert (std::ranges::range<span<int, 4>>);
  static_assert (std::ranges::contiguous_range<span<int, 4>>);
  static_assert (std::ranges::borrowed_range<span<int, 4>>);
  static_assert (std::ranges::view<span<int, 4>>);
  static_assert (std::ranges::contiguous_range<span<int const>>);
  static_assert (!std::ranges::output_range<span<int const>, int>);
  SUCCEED ();
}

TEST (LumexSpanRangesTest, GivenSpan_WhenEnableVariables_ThenTrue)
{
  static_assert (std::ranges::enable_borrowed_range<span<int>>);
  static_assert (std::ranges::enable_borrowed_range<span<int const, 3>>);
  static_assert (std::ranges::enable_view<span<int>>);
  static_assert (std::ranges::enable_view<span<char const, 8>>);
  static_assert (!std::ranges::enable_borrowed_range<std::vector<int>>);
  SUCCEED ();
}

TEST (LumexSpanRangesTest,
      GivenSpan_WhenIteratorConcepts_ThenContiguousAndSized)
{
  using iterator = span<int>::iterator;
  using const_iterator = span<int>::const_iterator;
  static_assert (std::contiguous_iterator<iterator>);
  static_assert (std::contiguous_iterator<const_iterator>);
  static_assert (std::sized_sentinel_for<iterator, iterator>);
  static_assert (std::is_same_v<std::ranges::iterator_t<span<int>>, iterator>);
  static_assert (
      std::is_same_v<std::ranges::range_value_t<span<int const>>, int>);
  static_assert (
      std::is_same_v<std::ranges::range_reference_t<span<int>>, int &>);
  static_assert (
      std::is_same_v<std::ranges::range_reference_t<span<int const>>,
                     int const &>);
  static_assert (std::is_same_v<std::ranges::range_difference_t<span<int>>,
                                std::ptrdiff_t>);
  SUCCEED ();
}

TEST (LumexSpanRangesTest, GivenSpan_WhenRangeAccessors_ThenSpanMembers)
{
  int values[3] = { 1, 2, 3 };
  span<int> const view (values);
  EXPECT_EQ (std::ranges::data (view), values);
  EXPECT_EQ (std::ranges::size (view), 3u);
  EXPECT_FALSE (std::ranges::empty (view));
  EXPECT_EQ (std::ranges::begin (view), values);
  EXPECT_EQ (std::ranges::end (view), values + 3);
  EXPECT_EQ (std::ranges::distance (view), 3);
  EXPECT_EQ (std::ranges::ssize (view), 3);
  EXPECT_EQ (*std::ranges::rbegin (view), 3);
}

TEST (LumexSpanRangesTest,
      GivenTemporarySpan_WhenRangeAlgorithm_ThenIteratorNotDangling)
{
  int values[4] = { 5, 3, 8, 1 };
  auto found = std::ranges::find (span<int> (values), 8);
  static_assert (!std::is_same_v<decltype (found), std::ranges::dangling>,
                 "a span is a borrowed range");
  EXPECT_EQ (found, values + 2);
  auto smallest = std::ranges::min_element (span<int, 4> (values));
  EXPECT_EQ (*smallest, 1);
}

// -- Algorithms and views --

TEST (LumexSpanRangesTest, GivenSpan_WhenRangesSort_ThenSortsTheViewedStorage)
{
  std::vector<int> values = { 5, 3, 9, 1 };
  std::ranges::sort (span<int> (values));
  EXPECT_EQ (values, (std::vector<int>{ 1, 3, 5, 9 }));
  std::ranges::reverse (span<int, 4> (values.data (), 4));
  EXPECT_EQ (values, (std::vector<int>{ 9, 5, 3, 1 }));
  std::ranges::fill (span<int> (values).first (2), 0);
  EXPECT_EQ (values, (std::vector<int>{ 0, 0, 3, 1 }));
}

TEST (LumexSpanRangesTest, GivenSpan_WhenViewsApplied_ThenLazyAdaptersWork)
{
  int values[5] = { 1, 2, 3, 4, 5 };
  span<int> const view (values);
  std::vector<int> taken;
  for (int element : view | std::views::take (3))
    {
      taken.push_back (element);
    }
  EXPECT_EQ (taken, (std::vector<int>{ 1, 2, 3 }));
  std::vector<int> reversed;
  for (int element : view | std::views::reverse)
    {
      reversed.push_back (element);
    }
  EXPECT_EQ (reversed, (std::vector<int>{ 5, 4, 3, 2, 1 }));
  std::vector<int> doubled;
  for (int element :
       view | std::views::transform ([] (int value) { return value * 2; }))
    {
      doubled.push_back (element);
    }
  EXPECT_EQ (doubled, (std::vector<int>{ 2, 4, 6, 8, 10 }));
  std::vector<int> evens;
  for (int element :
       view | std::views::filter ([] (int value) { return value % 2 == 0; }))
    {
      evens.push_back (element);
    }
  EXPECT_EQ (evens, (std::vector<int>{ 2, 4 }));
}

TEST (LumexSpanRangesTest, GivenSpan_WhenSubrangeAndCommon_ThenRoundTrips)
{
  int values[4] = { 1, 2, 3, 4 };
  span<int> const view (values);
  std::ranges::subrange<int *> const sub (view);
  EXPECT_EQ (sub.size (), 4u);
  auto common = view | std::views::common;
  EXPECT_EQ (std::distance (common.begin (), common.end ()), 4);
}

// -- Constructors from standard iterators and sentinels --

TEST (LumexSpanRangesTest,
      GivenStandardContiguousIterators_WhenCountForm_ThenViewsThem)
{
  std::vector<int> values = { 1, 2, 3, 4 };
  span<int> const from_vector (values.begin () + 1, 2);
  EXPECT_EQ (from_vector.data (), values.data () + 1);
  EXPECT_EQ (from_vector.size (), 2u);
  span<int const> const from_const (values.cbegin (), values.size ());
  EXPECT_EQ (from_const.size (), 4u);
  std::string text = "hello";
  span<char> const from_string (text.begin (), 3);
  EXPECT_EQ (from_string.data (), text.data ());
  std::array<int, 3> array_values = { { 1, 2, 3 } };
  span<int> const from_array (array_values.begin (), 3);
  EXPECT_EQ (from_array.data (), array_values.data ());
  std::string_view const text_view = "world";
  span<char const> const from_view (text_view.begin (), text_view.size ());
  EXPECT_EQ (from_view.data (), text_view.data ());
}

TEST (LumexSpanRangesTest,
      GivenStandardContiguousIterators_WhenPairForm_ThenViewsThem)
{
  std::vector<int> values = { 1, 2, 3, 4 };
  span<int> const whole (values.begin (), values.end ());
  EXPECT_EQ (whole.size (), 4u);
  EXPECT_EQ (whole.data (), values.data ());
  span<int const> const part (values.cbegin () + 1, values.cend () - 1);
  EXPECT_EQ (part.size (), 2u);
  EXPECT_EQ (part.front (), 2);
  span<int, 4> const fixed (values.begin (), values.end ());
  EXPECT_EQ (fixed.size (), 4u);
}

TEST (LumexSpanRangesTest,
      GivenEmptyStandardRange_WhenSpan_ThenDataIsTheIteratorAddress)
{
  std::vector<int> values;
  span<int> const view (values.begin (), values.end ());
  EXPECT_TRUE (view.empty ());
  EXPECT_EQ (view.data (), std::to_address (values.begin ()));
}

TEST (LumexSpanRangesTest,
      GivenCountedIteratorAndDefaultSentinel_WhenPair_ThenViewsTheCount)
{
  int values[5] = { 1, 2, 3, 4, 5 };
  std::counted_iterator<int *> const first (values + 1, 3);
  span<int> const view (first, std::default_sentinel);
  EXPECT_EQ (view.size (), 3u);
  EXPECT_EQ (view.data (), values + 1);
  static_assert (
      std::is_constructible_v<span<int>, std::counted_iterator<int *>,
                              std::default_sentinel_t>);
}

TEST (LumexSpanRangesTest, GivenNonContiguousIterators_WhenSpan_ThenRejected)
{
  static_assert (!std::is_constructible_v<span<int>, std::list<int>::iterator,
                                          std::size_t>);
  static_assert (!std::is_constructible_v<span<int>, std::deque<int>::iterator,
                                          std::size_t>);
  static_assert (
      !std::is_constructible_v<span<int>, std::reverse_iterator<int *>,
                               std::size_t>);
  static_assert (!std::is_constructible_v<span<int>, std::move_iterator<int *>,
                                          std::size_t>);
  static_assert (
      !std::is_constructible_v<span<int>, std::vector<bool>::iterator,
                               std::size_t>);
  static_assert (
      !std::is_constructible_v<span<int>, std::vector<long>::iterator,
                               std::size_t>,
      "elements of another type");
  static_assert (
      !std::is_constructible_v<span<int>, std::vector<int>::const_iterator,
                               std::size_t>,
      "const elements must not become non-const");
  SUCCEED ();
}

// -- Conversions to and from std::span --

#if LUMEX_HAS_STD_SPAN

TEST (LumexSpanRangesTest,
      GivenStdSpan_WhenLumexSpan_ThenImplicitForDynamicExtent)
{
  int values[4] = { 1, 2, 3, 4 };
  std::span<int> const standard (values);
  span<int> const from_lvalue = standard;
  EXPECT_EQ (from_lvalue.data (), values);
  EXPECT_EQ (from_lvalue.size (), 4u);
  span<int> const from_rvalue = std::span<int> (values, 2);
  EXPECT_EQ (from_rvalue.size (), 2u);
  span<int const> const to_const = std::span<int> (values);
  EXPECT_EQ (to_const.size (), 4u);
  static_assert (std::is_convertible_v<std::span<int>, span<int>>);
  static_assert (std::is_convertible_v<std::span<int> &, span<int>>);
  static_assert (std::is_convertible_v<std::span<int>, span<int const>>);
  static_assert (std::is_convertible_v<std::span<int const>, span<int const>>);
  static_assert (!std::is_constructible_v<span<int>, std::span<int const>>,
                 "const must not be dropped");
}

TEST (LumexSpanRangesTest,
      GivenLumexSpan_WhenStdSpan_ThenImplicitForDynamicExtent)
{
  int values[4] = { 1, 2, 3, 4 };
  span<int> const own (values);
  std::span<int> const from_lvalue = own;
  EXPECT_EQ (from_lvalue.data (), values);
  EXPECT_EQ (from_lvalue.size (), 4u);
  std::span<int> const from_rvalue = span<int> (values, 3);
  EXPECT_EQ (from_rvalue.size (), 3u);
  std::span<int const> const to_const = span<int> (values);
  EXPECT_EQ (to_const.size (), 4u);
  static_assert (std::is_convertible_v<span<int>, std::span<int>>);
  static_assert (std::is_convertible_v<span<int> &, std::span<int>>);
  static_assert (std::is_convertible_v<span<int>, std::span<int const>>);
  static_assert (!std::is_constructible_v<std::span<int>, span<int const>>,
                 "const must not be dropped");
}

TEST (LumexSpanRangesTest,
      GivenStaticExtents_WhenConverted_ThenExplicitLikeTheStandard)
{
  int values[4] = { 1, 2, 3, 4 };
  std::span<int, 4> const standard (values);
  span<int, 4> const own (standard);
  EXPECT_EQ (own.size (), 4u);
  std::span<int, 4> const back (own);
  EXPECT_EQ (back.data (), values);
  static_assert (std::is_constructible_v<span<int, 4>, std::span<int, 4>>);
  static_assert (!std::is_convertible_v<std::span<int, 4>, span<int, 4>>,
                 "a static extent from a range is explicit");
  static_assert (std::is_constructible_v<std::span<int, 4>, span<int, 4>>);
  static_assert (!std::is_convertible_v<span<int, 4>, std::span<int, 4>>,
                 "the standard makes the same conversion explicit");
  static_assert (std::is_convertible_v<std::span<int, 4>, span<int>>,
                 "static to dynamic is implicit");
  static_assert (std::is_convertible_v<span<int, 4>, std::span<int>>,
                 "static to dynamic is implicit");
  static_assert (!std::is_convertible_v<std::span<int>, span<int, 4>>,
                 "dynamic to static is explicit");
  static_assert (std::is_constructible_v<span<int, 4>, std::span<int>>);
}

TEST (LumexSpanRangesTest,
      GivenFunctionTakingStdSpan_WhenLumexSpanPassed_ThenConverts)
{
  int values[3] = { 1, 2, 3 };
  auto sum_std = [] (std::span<int const> view)
    {
      int total = 0;
      for (int element : view)
        {
          total += element;
        }
      return total;
    };
  auto sum_own = [] (span<int const> view)
    {
      int total = 0;
      for (int element : view)
        {
          total += element;
        }
      return total;
    };
  EXPECT_EQ (sum_std (span<int> (values)), 6);
  EXPECT_EQ (sum_own (std::span<int> (values)), 6);
  EXPECT_EQ (sum_std (span<int, 3> (values)), 6);
  EXPECT_EQ (sum_own (std::span<int, 3> (values)), 6);
}

TEST (LumexSpanRangesTest,
      GivenSubviewsOfBothTypes_WhenCompared_ThenSameElements)
{
  std::vector<int> values = { 10, 20, 30, 40, 50 };
  span<int> const own (values);
  std::span<int> const standard (values);
  EXPECT_TRUE (
      std::ranges::equal (own.subspan (1, 3), standard.subspan (1, 3)));
  EXPECT_TRUE (std::ranges::equal (own.first<2> (), standard.first<2> ()));
  EXPECT_TRUE (std::ranges::equal (own.last (2), standard.last (2)));
}

#endif // LUMEX_HAS_STD_SPAN

TEST (LumexSpanRangesTest, GivenStringView_WhenSpanOfConstChar_ThenImplicit)
{
  std::string_view const text = "span";
  span<char const> const view = text;
  EXPECT_EQ (view.size (), 4u);
  EXPECT_EQ (view.data (), text.data ());
  static_assert (std::is_convertible_v<std::string_view, span<char const>>);
  static_assert (!std::is_constructible_v<span<char>, std::string_view>);
}

#else // LUMEX_SPAN_HAS_RANGES

TEST (LumexSpanRangesTest, GivenToolchainWithoutRanges_WhenCxx20_ThenSkipped)
{
  GTEST_SKIP () << "the standard library has no <ranges> or concepts";
}

#endif // LUMEX_SPAN_HAS_RANGES
