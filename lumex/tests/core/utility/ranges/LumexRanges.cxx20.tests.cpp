// LumexRanges.cxx20.tests.cpp
// get_nearest_to with what needs C++20 <ranges>: the standard comparison
// objects, borrowed ranges and views (iota, subrange, span, reverse, filter),
// the iterators whose iterator_category is weaker than their iterator_concept,
// sentinels of another type, and a comparison with the C++20 implementation
// that LumexRanges.hpp had before it worked from C++11 (copied below). The
// header uses the standard concepts and std::ranges::lower_bound there; where
// the toolchain lacks <ranges> (GCC 8 accepts -std=c++2a without it) a single
// test reports the skip, and the C++11 files run the same calls with the
// C++11 forms. LumexRanges.cxx23.tests.cpp adds a check through
// std::ranges::contains (C++23).
#if defined(__has_include)
#if __has_include(<ranges>)
#include <ranges>
#endif
#if __has_include(<span>)
#include <span>
#endif
#endif
#include <cstddef>
#include <functional>
#include <iterator>
#include <list>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/math/LumexMath"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/ranges/LumexRanges.hpp"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

#if LUMEX_HAS_STD_RANGES

using lumex::core::utility::ranges::Algorithm::get_nearest_to;

namespace
{
// The C++20 get_nearest_to before it worked from C++11, with its constraints
// and its use of std::ranges and std::invoke.
template <std::bidirectional_iterator IteratorType,
          std::sentinel_for<IteratorType> Sentinel, typename ValueType,
          typename Projection = std::identity,
          std::indirect_strict_weak_order<
              ValueType const *, std::projected<IteratorType, Projection>>
              Predicate
          = std::ranges::less>
  requires lumex::core::math::ops::traits::NumericConcept<ValueType>
constexpr IteratorType
original_get_nearest_to (IteratorType first, Sentinel last,
                         ValueType const &value, Predicate pred = {},
                         Projection proj = {})
{
  if (first == last)
    return last;

  auto iter = std::ranges::lower_bound (first, last, value, pred, proj);
  if (iter == last)
    return std::ranges::prev (iter);
  if (iter == first)
    return iter;

  auto const prev = std::ranges::prev (iter);

  auto const distBetweenFoundAndSpecified
      = lumex::core::math::ops::distance (std::invoke (proj, *iter), value);
  auto const distBetweenPrevAndSpecified
      = lumex::core::math::ops::distance (std::invoke (proj, *prev), value);

  return (distBetweenFoundAndSpecified < distBetweenPrevAndSpecified) ? iter
                                                                      : prev;
}

struct item
{
  int key;
  int tag;
};

template <typename... Args>
concept can_get_nearest = requires (Args &&...args) {
  get_nearest_to (std::forward<Args> (args)...);
};

// Every query in [low, high] against every non-decreasing vector of `length`
// values taken from `values`: the two implementations must return the same
// position.
template <typename Compare>
std::size_t
count_disagreements (std::vector<int> const &values, std::size_t length,
                     int low, int high, Compare const &compare)
{
  std::size_t disagreements = 0;
  std::vector<std::size_t> picks (length, 0);
  for (;;)
    {
      std::vector<int> sorted;
      for (std::size_t index = 0; index < length; ++index)
        sorted.push_back (values[picks[index]]);
      for (int query = low; query <= high; ++query)
        if (!compare (sorted, query))
          ++disagreements;
      // The next non-decreasing choice of indices.
      std::size_t position = length;
      while (position > 0 && picks[position - 1] + 1 == values.size ())
        --position;
      if (position == 0)
        break;
      ++picks[position - 1];
      for (std::size_t index = position; index < length; ++index)
        picks[index] = picks[position - 1];
    }
  return disagreements;
}
} // namespace

TEST (LumexRangesTest,
      GivenRangesLessAndKeyLambda_WhenGetNearestTo_ThenComparesProjectedValues)
{
  std::vector<item> const items{ { 1, 100 }, { 5, 200 }, { 9, 300 } };
  auto const it
      = get_nearest_to (items, 6, std::ranges::less{},
                        [] (item const &element) { return element.key; });
  ASSERT_NE (it, items.end ());
  EXPECT_EQ (it->key, 5);
  EXPECT_EQ (it->tag, 200);
}

TEST (LumexRangesTest,
      GivenDescendingRangeAndRangesGreater_WhenGetNearestTo_ThenUsesTheOrder)
{
  std::vector<int> const values{ 100, 50, 0 };
  auto const it = get_nearest_to (values, 10, std::ranges::greater{});
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 0);
}

TEST (LumexRangesTest,
      GivenIotaView_WhenGetNearestTo_ThenReturnsNearestInteger)
{
  auto const view = std::views::iota (0, 11);
  auto const it = get_nearest_to (view, 7);
  ASSERT_NE (it, std::ranges::end (view));
  EXPECT_EQ (*it, 7);
}

TEST (LumexRangesTest,
      GivenBorrowedTemporaryRanges_WhenGetNearestTo_ThenTheyAreAccepted)
{
  // A temporary view or span does not own its elements, so the iterator
  // stays valid after the call.
  auto const from_iota = get_nearest_to (std::views::iota (0, 11), 7);
  EXPECT_EQ (*from_iota, 7);

  std::vector<int> const values{ 1, 3, 5, 7, 9 };
#if LUMEX_HAS_STD_SPAN
  auto const from_span = get_nearest_to (
      std::span<int const> (values.data (), values.size ()), 6);
  EXPECT_EQ (*from_span, 5);
#endif

  std::list<int> const list_values{ 10, 20, 30 };
  auto const from_subrange = get_nearest_to (
      std::ranges::subrange (list_values.begin (), list_values.end ()), 26);
  EXPECT_EQ (*from_subrange, 30);
}

TEST (LumexRangesTest, GivenReverseView_WhenGetNearestTo_ThenUsesGreater)
{
  std::vector<int> const values{ 1, 3, 5, 7, 9 };
  auto const view = values | std::views::reverse;
  // 9 7 5 3 1 read backwards from 1 3 5 7 9; 2.5 is nearer to 3 than to 1.
  auto const it = get_nearest_to (view, 2.5, std::ranges::greater{});
  ASSERT_NE (it, std::ranges::end (view));
  EXPECT_EQ (*it, 3);
}

TEST (LumexRangesTest, GivenFilterView_WhenGetNearestTo_ThenSkipsFiltered)
{
  std::vector<int> const values{ 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  auto view
      = values
        | std::views::filter ([] (int number) { return number % 2 == 0; });
  auto const it = get_nearest_to (view, 5);
  ASSERT_NE (it, std::ranges::end (view));
  EXPECT_EQ (*it, 4);
}

TEST (
    LumexRangesTest,
    GivenIteratorsWithWeakerCategory_WhenGetNearestTo_ThenTheConceptsAcceptThem)
{
  // The iterator of iota_view is a random access iterator by its
  // iterator_concept whatever its iterator_category says; the iterator
  // overload takes it as the standard algorithms do.
  auto const view = std::views::iota (0, 11);
  static_assert (std::bidirectional_iterator<decltype (view.begin ())>, "");
  auto const it = get_nearest_to (view.begin (), view.end (), 3);
  EXPECT_EQ (*it, 3);
}

TEST (
    LumexRangesTest,
    GivenCountedIteratorAndDefaultSentinel_WhenGetNearestTo_ThenWalksToTheEnd)
{
  int const values[] = { 2, 4, 6, 8, 10 };
  std::counted_iterator<int const *> const first (values, 4);
  auto const it = get_nearest_to (first, std::default_sentinel, 7);
  EXPECT_EQ (*it, 6);
  // The range is [2, 4, 6, 8]: 10 is not in it.
  auto const above = get_nearest_to (first, std::default_sentinel, 100);
  EXPECT_EQ (*above, 8);
}

TEST (LumexRangesTest, GivenTemporaryContainer_WhenGetNearestTo_ThenNoOverload)
{
  static_assert (!can_get_nearest<std::vector<int>, int>, "");
  static_assert (!can_get_nearest<std::vector<int> &&, int>, "");
  static_assert (can_get_nearest<std::vector<int> &, int>, "");
#if LUMEX_HAS_STD_SPAN
  static_assert (can_get_nearest<std::span<int>, int>, "");
#endif
  static_assert (can_get_nearest<decltype (std::views::iota (0, 3)), int>, "");
  SUCCEED ();
}

TEST (LumexRangesTest,
      GivenSortedVectorsUpToFive_WhenCompared_ThenSamePositionAsTheOriginal)
{
  std::vector<int> const values{ 0, 3, 6, 9, 12 };
  std::size_t checked = 0;
  std::size_t disagreements = 0;
  for (std::size_t length = 0; length <= 5; ++length)
    {
      disagreements += count_disagreements (
          values, length, -2, 14,
          [&] (std::vector<int> const &sorted, int q)
            {
              ++checked;
              auto const now = get_nearest_to (sorted, q);
              auto const before = original_get_nearest_to (sorted.begin (),
                                                           sorted.end (), q);
              return now - sorted.begin () == before - sorted.begin ();
            });
    }
  EXPECT_EQ (checked, 252U * 17U);
  EXPECT_EQ (disagreements, 0U);
}

TEST (
    LumexRangesTest,
    GivenProjectionAndRangesGreater_WhenCompared_ThenSamePositionAsTheOriginal)
{
  std::vector<int> const values{ 1, 2, 4, 8, 16 };
  std::size_t disagreements = 0;
  for (std::size_t length = 0; length <= 4; ++length)
    {
      disagreements += count_disagreements (
          values, length, -3, 20,
          [] (std::vector<int> const &ascending, int q)
            {
              std::vector<item> items;
              for (auto it = ascending.rbegin (); it != ascending.rend ();
                   ++it)
                items.push_back (item{ *it, 0 });
              auto const key
                  = [] (item const &element) { return element.key; };
              auto const now
                  = get_nearest_to (items, q, std::ranges::greater{}, key);
              auto const before
                  = original_get_nearest_to (items.begin (), items.end (), q,
                                             std::ranges::greater{}, key);
              return now - items.begin () == before - items.begin ();
            });
    }
  EXPECT_EQ (disagreements, 0U);
}

#else // the toolchain lacks the features of the module

TEST (LumexRangesTest, UnavailableOnThisToolchain)
{
  GTEST_SKIP () << "the toolchain has no C++20 <ranges>";
}

#endif
