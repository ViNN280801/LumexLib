// LumexRanges.tests.cpp
#include <array>
#include <deque>
#include <functional>
#include <list>
#include <ranges>
#include <vector>
#include <version>

#include <gtest/gtest.h>

#include "lumex/core/utility/ranges/LumexRanges.hpp"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

using namespace lumex::core::utility::ranges;

using lumex::core::utility::ranges::Algorithm::GetNearestTo;

TEST (LumexRangesTest, GivenEmptyRange_WhenGetNearestTo_ThenReturnsEnd)
{
  std::vector<int> const values;
  auto const it = GetNearestTo (values, 5);
  EXPECT_EQ (it, values.end ());
}

TEST (LumexRangesTest, GivenExactMatch_WhenGetNearestTo_ThenReturnsThatElement)
{
  std::vector<int> const values{ 1, 3, 5, 7, 9 };
  auto const it = GetNearestTo (values, 5);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 5);
}

TEST (LumexRangesTest,
      GivenTiedDistance_WhenGetNearestTo_ThenReturnsPreviousElement)
{
  // value=6 is equidistant from 5 and 7. GetNearestTo only switches to the
  // found element when it is strictly closer, so on a tie it keeps the
  // predecessor (5).
  std::vector<int> const values{ 1, 3, 5, 7, 9 };
  auto const it = GetNearestTo (values, 6);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 5);
}

TEST (
    LumexRangesTest,
    GivenValueStrictlyCloserToPrevious_WhenGetNearestTo_ThenReturnsPreviousElement)
{
  std::vector<int> const values{ 0, 10, 100 };
  auto const it = GetNearestTo (values, 90);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 100);

  auto const it2 = GetNearestTo (values, 15);
  ASSERT_NE (it2, values.end ());
  EXPECT_EQ (*it2, 10);
}

TEST (LumexRangesTest,
      GivenValueBeforeRange_WhenGetNearestTo_ThenReturnsFirstElement)
{
  std::vector<int> const values{ 10, 20, 30 };
  auto const it = GetNearestTo (values, -100);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 10);
}

TEST (LumexRangesTest,
      GivenValueAfterRange_WhenGetNearestTo_ThenReturnsLastElement)
{
  std::vector<int> const values{ 10, 20, 30 };
  auto const it = GetNearestTo (values, 1000);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 30);
}

TEST (LumexRangesTest,
      GivenSingleElementRange_WhenGetNearestTo_ThenReturnsThatElement)
{
  std::vector<int> const values{ 42 };
  auto const it = GetNearestTo (values, -5);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 42);
}

TEST (LumexRangesTest,
      GivenFloatingPointRange_WhenGetNearestTo_ThenReturnsNearestElement)
{
  std::vector<double> const values{ 1.0, 2.5, 4.0, 8.0 };
  auto const it = GetNearestTo (values, 3.0);
  ASSERT_NE (it, values.end ());
  EXPECT_DOUBLE_EQ (*it, 2.5);
}

TEST (LumexRangesTest,
      GivenIteratorPairOverload_WhenGetNearestTo_ThenBehavesLikeRangeOverload)
{
  std::vector<int> const values{ 1, 3, 5, 7, 9 };
  auto const it = GetNearestTo (values.begin (), values.end (), 5);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 5);
}

TEST (LumexRangesTest,
      GivenValueEqualToFirst_WhenGetNearestTo_ThenReturnsFirst)
{
  std::vector<int> const values{ 10, 20, 30 };
  auto const it = GetNearestTo (values, 10);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 10);
}

TEST (LumexRangesTest, GivenValueEqualToLast_WhenGetNearestTo_ThenReturnsLast)
{
  std::vector<int> const values{ 10, 20, 30 };
  auto const it = GetNearestTo (values, 30);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 30);
}

TEST (LumexRangesTest,
      GivenTwoElementRange_WhenGetNearestTo_ThenPicksCloserSide)
{
  std::vector<int> const values{ 0, 10 };
  auto const left = GetNearestTo (values, 1);
  auto const right = GetNearestTo (values, 9);
  ASSERT_NE (left, values.end ());
  ASSERT_NE (right, values.end ());
  EXPECT_EQ (*left, 0);
  EXPECT_EQ (*right, 10);
}

TEST (LumexRangesTest,
      GivenDuplicateSortedValues_WhenGetNearestTo_ThenReturnsAMatchingElement)
{
  std::vector<int> const values{ 1, 5, 5, 5, 9 };
  auto const it = GetNearestTo (values, 5);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 5);
}

TEST (LumexRangesTest,
      GivenBidirectionalList_WhenGetNearestToTies_ThenKeepsPredecessor)
{
  std::list<int> const values{ 1, 4, 16, 64 };
  auto const it = GetNearestTo (values, 10);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 4);
}

TEST (LumexRangesTest, GivenDeque_WhenGetNearestTo_ThenReturnsNearest)
{
  std::deque<int> const values{ 2, 8, 32 };
  auto const it = GetNearestTo (values, 9);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 8);
}

TEST (LumexRangesTest, GivenStdArray_WhenGetNearestTo_ThenReturnsNearest)
{
  std::array<int, 4> const values{ 1, 10, 100, 1000 };
  auto const it = GetNearestTo (values, 70);
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
  auto const it = GetNearestTo (items, 6, std::ranges::less{},
                                [] (Item const &item) { return item.key; });
  ASSERT_NE (it, items.end ());
  EXPECT_EQ (it->key, 5);
  EXPECT_EQ (it->tag, 200);
}

TEST (LumexRangesTest,
      GivenDescendingRangeAndGreater_WhenGetNearestTo_ThenUsesCustomOrder)
{
  std::vector<int> const values{ 100, 50, 0 };
  auto const it = GetNearestTo (values, 10, std::ranges::greater{});
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 0);
}

TEST (LumexRangesTest, GivenEmptyIteratorPair_WhenGetNearestTo_ThenReturnsLast)
{
  std::vector<int> const values;
  auto const it = GetNearestTo (values.begin (), values.end (), 1);
  EXPECT_EQ (it, values.end ());
}

TEST (LumexRangesTest,
      GivenIotaView_WhenGetNearestTo_ThenReturnsNearestInteger)
{
  auto const view = std::views::iota (0, 11);
  auto const it = GetNearestTo (view, 7);
  ASSERT_NE (it, std::ranges::end (view));
  EXPECT_EQ (*it, 7);
}

TEST (LumexRangesTest, GivenStdArray_WhenGetNearestToTies_ThenKeepsPredecessor)
{
  std::array<int, 5> const values{ 1, 3, 5, 7, 9 };
  auto const it = GetNearestTo (values, 6);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 5);
}

TEST (LumexRangesTest,
      GivenNegativeIntegers_WhenGetNearestTo_ThenUsesAbsoluteDistance)
{
  std::vector<int> const values{ -40, -10, 0, 10 };
  auto const it = GetNearestTo (values, -12);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, -10);
}

TEST (LumexRangesTest,
      GivenFloatExactMatch_WhenGetNearestTo_ThenReturnsThatValue)
{
  std::vector<float> const values{ 0.25f, 0.5f, 0.75f };
  auto const it = GetNearestTo (values, 0.5f);
  ASSERT_NE (it, values.end ());
  EXPECT_FLOAT_EQ (*it, 0.5f);
}

#if defined(__cpp_lib_ranges_contains) && __cpp_lib_ranges_contains >= 202207L
TEST (LumexRangesTest,
      GivenCpp23Contains_WhenCheckingSortedSource_ThenConfirmsMembership)
{
  std::vector<int> const values{ 1, 3, 5, 7, 9 };
  auto const it = GetNearestTo (values, 7);
  ASSERT_NE (it, values.end ());
  EXPECT_TRUE (std::ranges::contains (values, *it));
}
#endif
