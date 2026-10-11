// C++20 only: the iterators model std::random_access_iterator, the buffer is a
// std::ranges random access range, and operator<=> works for every element
// type that has <=> (partial orderings included).
#include <algorithm>
#include <iterator>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#if defined(__has_include)
#if __has_include(<compare>)
#include <compare>
#endif
#if __has_include(<concepts>)
#include <concepts>
#endif
#if __has_include(<ranges>)
#include <ranges>
#endif
#endif

#include <gtest/gtest.h>

#include "lumex/core/circular_buffer/CircularBuffer"

#include "CircularBufferTestSupport.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

using lumex::core::circular_buffer::circular_buffer;

#if LUMEX_HAS_STD_CONCEPTS
namespace
{
using buffer_t = circular_buffer<int>;

static_assert (std::random_access_iterator<buffer_t::iterator>,
               "iterator models random_access_iterator");
static_assert (std::random_access_iterator<buffer_t::const_iterator>,
               "const_iterator models random_access_iterator");
static_assert (std::random_access_iterator<buffer_t::reverse_iterator>,
               "reverse_iterator models random_access_iterator");
static_assert (
    std::sized_sentinel_for<buffer_t::iterator, buffer_t::const_iterator>,
    "mixed iterators are sized sentinels of each other");
static_assert (
    std::totally_ordered_with<buffer_t::iterator, buffer_t::const_iterator>,
    "mixed iterators are totally ordered with each other");
static_assert (std::equality_comparable_with<buffer_t::const_iterator,
                                             buffer_t::iterator>,
               "mixed equality in both directions");
static_assert (std::indirectly_writable<buffer_t::iterator, int>,
               "iterator writes ints");
static_assert (!std::indirectly_writable<buffer_t::const_iterator, int>,
               "const_iterator does not write");
static_assert (std::permutable<buffer_t::iterator>,
               "iterator can be permuted (sort)");
} // namespace

TEST (CircularBufferIterators20, Concepts_HoldForEveryElementType)
{
  static_assert (
      std::random_access_iterator<circular_buffer<std::string>::iterator>);
  static_assert (
      std::random_access_iterator<circular_buffer<double>::const_iterator>);
  SUCCEED ();
}
#else
TEST (CircularBufferIterators20, Concepts_HoldForEveryElementType)
{
  GTEST_SKIP () << "the standard concepts library is not available";
}
#endif

#if LUMEX_HAS_STD_RANGES && LUMEX_HAS_STD_CONCEPTS
static_assert (std::ranges::random_access_range<circular_buffer<int>>,
               "the buffer is a random access range");
static_assert (std::ranges::random_access_range<circular_buffer<int> const>,
               "the const buffer is a random access range");
static_assert (std::ranges::sized_range<circular_buffer<int>>,
               "the buffer is a sized range");
static_assert (!std::ranges::contiguous_range<circular_buffer<int>>,
               "the buffer is not contiguous");

TEST (CircularBufferIterators20, RangesAlgorithms_AcceptTheBuffer)
{
  circular_buffer<int> buffer (6);
  for (int i = 0; i < 4; ++i)
    {
      buffer.push_back (0);
      buffer.pop_front ();
    }
  for (int value : { 4, 6, 1, 5, 3, 2 })
    buffer.push_back (value);

  std::ranges::sort (buffer);
  EXPECT_EQ (cbtest::content_of (buffer),
             (std::vector<int>{ 1, 2, 3, 4, 5, 6 }));
  EXPECT_EQ (*std::ranges::find (buffer, 4), 4);
  EXPECT_EQ (std::ranges::lower_bound (buffer, 5) - buffer.begin (), 4);
  std::ranges::reverse (buffer);
  EXPECT_EQ (buffer.front (), 6);
  EXPECT_EQ (std::ranges::count_if (buffer, [] (int v) { return v % 2 == 0; }),
             3);

  circular_buffer<int> const &view = buffer;
  EXPECT_EQ (std::ranges::size (view), 6U);
  EXPECT_EQ (std::ranges::end (view) - std::ranges::begin (view), 6);
  std::vector<int> halves;
  for (int value : buffer | std::views::take (3))
    halves.push_back (value);
  EXPECT_EQ (halves, (std::vector<int>{ 6, 5, 4 }));
}
#else
TEST (CircularBufferIterators20, RangesAlgorithms_AcceptTheBuffer)
{
  GTEST_SKIP () << "std::ranges is not available";
}
#endif

#if LUMEX_HAS_THREE_WAY_COMPARISON && LUMEX_HAS_STD_CONCEPTS
static_assert (
    std::is_same_v<
        decltype (std::declval<circular_buffer<int> const &> ()
                  <=> std::declval<circular_buffer<int> const &> ()),
        std::strong_ordering>,
    "int: strong_ordering");
static_assert (
    std::is_same_v<
        decltype (std::declval<circular_buffer<double> const &> ()
                  <=> std::declval<circular_buffer<double> const &> ()),
        std::partial_ordering>,
    "double: partial_ordering");
static_assert (
    std::is_same_v<
        decltype (std::declval<circular_buffer<std::string> const &> ()
                  <=> std::declval<circular_buffer<std::string> const &> ()),
        std::strong_ordering>,
    "std::string: strong_ordering");
static_assert (std::three_way_comparable<circular_buffer<int>>,
               "circular_buffer<int> is three_way_comparable");
static_assert (std::three_way_comparable<circular_buffer<double>>,
               "circular_buffer<double> is partially three_way_comparable");

namespace
{
/// Has only <, so it is not three_way_comparable.
struct less_only
{
  int value;
  bool
  operator< (less_only const &other) const
  {
    return value < other.value;
  }
};
static_assert (!std::three_way_comparable<circular_buffer<less_only>>,
               "no operator<=> without a <=> of the elements");
} // namespace

TEST (CircularBufferIterators20,
      SpaceshipOfStrongElements_OrdersLexicographically)
{
  circular_buffer<int> a (4);
  circular_buffer<int> b (6);
  for (int v : { 1, 2, 3 })
    a.push_back (v);
  for (int v : { 1, 2, 3 })
    b.push_back (v);
  EXPECT_TRUE ((a <=> b) == 0); // capacity is not compared
  b.push_back (0);
  EXPECT_TRUE ((a <=> b) < 0); // the shorter buffer is less
  EXPECT_TRUE ((b <=> a) > 0);
  a.push_back (9);
  EXPECT_TRUE ((a <=> b) > 0); // 9 > 0 at index 3
  b.pop_back ();
  b.push_back (9);
  EXPECT_TRUE ((a <=> b) == 0);
  b.push_front (0); // the buffer wraps: [0, 1, 2, 3, 9]
  EXPECT_TRUE ((b <=> a) < 0);
  EXPECT_TRUE (b < a);
  EXPECT_TRUE (a >= b);
}

TEST (CircularBufferIterators20, SpaceshipOfDoubles_IsAPartialOrdering)
{
  circular_buffer<double> a (3);
  circular_buffer<double> b (3);
  for (double v : { 1.0, 2.0 })
    {
      a.push_back (v);
      b.push_back (v);
    }
  EXPECT_EQ ((a <=> b), std::partial_ordering::equivalent);
  b.push_back (3.0);
  EXPECT_EQ ((a <=> b), std::partial_ordering::less);
  EXPECT_EQ ((b <=> a), std::partial_ordering::greater);

  circular_buffer<double> nan_buffer (3);
  nan_buffer.push_back (std::numeric_limits<double>::quiet_NaN ());
  circular_buffer<double> one (3);
  one.push_back (1.0);
  EXPECT_EQ ((nan_buffer <=> one), std::partial_ordering::unordered);
  EXPECT_EQ ((one <=> nan_buffer), std::partial_ordering::unordered);
  EXPECT_FALSE (nan_buffer < one);
  EXPECT_FALSE (nan_buffer > one);
}

TEST (CircularBufferIterators20, SpaceshipOfStrings_UsesTheElementOrder)
{
  circular_buffer<std::string> a (2);
  circular_buffer<std::string> b (2);
  a.push_back ("apple");
  b.push_back ("apple");
  b.push_back ("pear");
  EXPECT_TRUE ((a <=> b) < 0);
  a.push_back ("banana");
  EXPECT_TRUE ((a <=> b) < 0);
  a.pop_back ();
  a.push_back ("zebra");
  EXPECT_TRUE ((a <=> b) > 0);
}
#else
TEST (CircularBufferIterators20, Spaceship_NeedsThreeWayComparison)
{
  GTEST_SKIP () << "operator<=> and <compare> are not available";
}
#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
