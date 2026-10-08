// LumexRanges.cxx11.tests.cpp
// get_nearest_to of LumexRanges.hpp from C++11: ranges and iterator pairs
// over standard containers, ties, the ends, a projection and a comparison.
// The projection forms are in LumexRangesProjection.cxx11.tests.cpp, the
// calls that must not compile in LumexRangesConstraints.cxx11.tests.cpp, the
// search against a linear oracle in LumexRangesOracle.cxx11.tests.cpp;
// LumexRanges.cxx20.tests.cpp adds what needs <ranges>, and
// LumexRanges.cxx23.tests.cpp a check through std::ranges::contains (C++23).
#include <array>
#include <deque>
#include <functional>
#include <list>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/functional/LumexFunctional.hpp"
#include "lumex/core/utility/ranges/LumexRanges.hpp"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

using namespace lumex::core::utility::ranges;

using lumex::core::utility::ranges::Algorithm::get_nearest_to;

TEST (LumexRangesTest, GivenEmptyRange_WhenGetNearestTo_ThenReturnsEnd)
{
  std::vector<int> const values;
  auto const it = get_nearest_to (values, 5);
  EXPECT_EQ (it, values.end ());
}

TEST (LumexRangesTest, GivenExactMatch_WhenGetNearestTo_ThenReturnsThatElement)
{
  std::vector<int> const values{ 1, 3, 5, 7, 9 };
  auto const it = get_nearest_to (values, 5);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 5);
}

TEST (LumexRangesTest,
      GivenTiedDistance_WhenGetNearestTo_ThenReturnsPreviousElement)
{
  // value=6 is equidistant from 5 and 7. get_nearest_to only switches to the
  // found element when it is strictly closer, so on a tie it keeps the
  // predecessor (5).
  std::vector<int> const values{ 1, 3, 5, 7, 9 };
  auto const it = get_nearest_to (values, 6);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 5);
}

TEST (
    LumexRangesTest,
    GivenValueStrictlyCloserToPrevious_WhenGetNearestTo_ThenReturnsPreviousElement)
{
  std::vector<int> const values{ 0, 10, 100 };
  auto const it = get_nearest_to (values, 90);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 100);

  auto const it2 = get_nearest_to (values, 15);
  ASSERT_NE (it2, values.end ());
  EXPECT_EQ (*it2, 10);
}

TEST (LumexRangesTest,
      GivenValueBeforeRange_WhenGetNearestTo_ThenReturnsFirstElement)
{
  std::vector<int> const values{ 10, 20, 30 };
  auto const it = get_nearest_to (values, -100);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 10);
}

TEST (LumexRangesTest,
      GivenValueAfterRange_WhenGetNearestTo_ThenReturnsLastElement)
{
  std::vector<int> const values{ 10, 20, 30 };
  auto const it = get_nearest_to (values, 1000);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 30);
}

TEST (LumexRangesTest,
      GivenSingleElementRange_WhenGetNearestTo_ThenReturnsThatElement)
{
  std::vector<int> const values{ 42 };
  auto const it = get_nearest_to (values, -5);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 42);
}

TEST (LumexRangesTest,
      GivenFloatingPointRange_WhenGetNearestTo_ThenReturnsNearestElement)
{
  std::vector<double> const values{ 1.0, 2.5, 4.0, 8.0 };
  auto const it = get_nearest_to (values, 3.0);
  ASSERT_NE (it, values.end ());
  EXPECT_DOUBLE_EQ (*it, 2.5);
}

TEST (LumexRangesTest,
      GivenIteratorPairOverload_WhenGetNearestTo_ThenBehavesLikeRangeOverload)
{
  std::vector<int> const values{ 1, 3, 5, 7, 9 };
  auto const it = get_nearest_to (values.begin (), values.end (), 5);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 5);
}

TEST (LumexRangesTest,
      GivenValueEqualToFirst_WhenGetNearestTo_ThenReturnsFirst)
{
  std::vector<int> const values{ 10, 20, 30 };
  auto const it = get_nearest_to (values, 10);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 10);
}

TEST (LumexRangesTest, GivenValueEqualToLast_WhenGetNearestTo_ThenReturnsLast)
{
  std::vector<int> const values{ 10, 20, 30 };
  auto const it = get_nearest_to (values, 30);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 30);
}

TEST (LumexRangesTest,
      GivenTwoElementRange_WhenGetNearestTo_ThenPicksCloserSide)
{
  std::vector<int> const values{ 0, 10 };
  auto const left = get_nearest_to (values, 1);
  auto const right = get_nearest_to (values, 9);
  ASSERT_NE (left, values.end ());
  ASSERT_NE (right, values.end ());
  EXPECT_EQ (*left, 0);
  EXPECT_EQ (*right, 10);
}

TEST (LumexRangesTest,
      GivenDuplicateSortedValues_WhenGetNearestTo_ThenReturnsAMatchingElement)
{
  std::vector<int> const values{ 1, 5, 5, 5, 9 };
  auto const it = get_nearest_to (values, 5);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 5);
}

TEST (LumexRangesTest,
      GivenBidirectionalList_WhenGetNearestToTies_ThenKeepsPredecessor)
{
  std::list<int> const values{ 1, 4, 16, 64 };
  auto const it = get_nearest_to (values, 10);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 4);
}

TEST (LumexRangesTest, GivenDeque_WhenGetNearestTo_ThenReturnsNearest)
{
  std::deque<int> const values{ 2, 8, 32 };
  auto const it = get_nearest_to (values, 9);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 8);
}

TEST (LumexRangesTest, GivenStdArray_WhenGetNearestTo_ThenReturnsNearest)
{
  std::array<int, 4> const values{ 1, 10, 100, 1000 };
  auto const it = get_nearest_to (values, 70);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 100);
}

TEST (LumexRangesTest,
      GivenProjectionOnKey_WhenGetNearestTo_ThenComparesProjectedValues)
{
  struct Item
  {
    int key;
    int tag;
  };

  std::vector<Item> const items{ { 1, 100 }, { 5, 200 }, { 9, 300 } };
  auto const it
      = get_nearest_to (items, 6, lumex::core::utility::functional::less (),
                        [] (Item const &item) { return item.key; });
  ASSERT_NE (it, items.end ());
  EXPECT_EQ (it->key, 5);
  EXPECT_EQ (it->tag, 200);
}

TEST (LumexRangesTest,
      GivenDescendingRangeAndGreater_WhenGetNearestTo_ThenUsesCustomOrder)
{
  std::vector<int> const values{ 100, 50, 0 };
  auto const it = get_nearest_to (values, 10, std::greater<int> ());
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 0);
}

TEST (LumexRangesTest, GivenEmptyIteratorPair_WhenGetNearestTo_ThenReturnsLast)
{
  std::vector<int> const values;
  auto const it = get_nearest_to (values.begin (), values.end (), 1);
  EXPECT_EQ (it, values.end ());
}

TEST (LumexRangesTest, GivenStdArray_WhenGetNearestToTies_ThenKeepsPredecessor)
{
  std::array<int, 5> const values{ 1, 3, 5, 7, 9 };
  auto const it = get_nearest_to (values, 6);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 5);
}

TEST (LumexRangesTest,
      GivenNegativeIntegers_WhenGetNearestTo_ThenUsesAbsoluteDistance)
{
  std::vector<int> const values{ -40, -10, 0, 10 };
  auto const it = get_nearest_to (values, -12);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, -10);
}

TEST (LumexRangesTest,
      GivenFloatExactMatch_WhenGetNearestTo_ThenReturnsThatValue)
{
  std::vector<float> const values{ 0.25f, 0.5f, 0.75f };
  auto const it = get_nearest_to (values, 0.5f);
  ASSERT_NE (it, values.end ());
  EXPECT_FLOAT_EQ (*it, 0.5f);
}
