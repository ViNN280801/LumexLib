// LumexSpanSubviews.cxx11.tests.cpp
//
// first / last / subspan with a count known at compile time and with one
// given at run time, the extents of the results, the borders (empty, whole,
// at the end) and the qualification of the element type.
#include <array>
#include <cstddef>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "LumexSpanTestSupport.hpp"
#include "lumex/core/span/LumexSpan"

using lumex::core::span::view::dynamic_extent;
using lumex::core::span::view::span;

namespace
{
template <typename Expected, typename Actual>
void
expect_type (Actual const &)
{
  static_assert (std::is_same<Expected, Actual>::value,
                 "the subview has the wrong type");
}
} // namespace

// -- first<Count>, last<Count>, subspan<Offset, Count> on a static extent --

TEST (LumexSpanSubviewsTest,
      GivenStaticSpan_WhenFirstCount_ThenStaticExtentCount)
{
  int values[5] = { 1, 2, 3, 4, 5 };
  span<int, 5> const view (values);
  expect_type<span<int, 2>> (view.first<2> ());
  expect_type<span<int, 0>> (view.first<0> ());
  expect_type<span<int, 5>> (view.first<5> ());
  EXPECT_EQ (view.first<2> ().size (), 2u);
  EXPECT_EQ (view.first<2> ().data (), values);
  EXPECT_EQ (view.first<2> ().back (), 2);
  EXPECT_TRUE (view.first<0> ().empty ());
  EXPECT_EQ (view.first<0> ().data (), values);
  EXPECT_EQ (view.first<5> ().data (), values);
}

TEST (LumexSpanSubviewsTest,
      GivenStaticSpan_WhenLastCount_ThenStaticExtentCount)
{
  int values[5] = { 1, 2, 3, 4, 5 };
  span<int, 5> const view (values);
  expect_type<span<int, 2>> (view.last<2> ());
  expect_type<span<int, 0>> (view.last<0> ());
  expect_type<span<int, 5>> (view.last<5> ());
  EXPECT_EQ (view.last<2> ().data (), values + 3);
  EXPECT_EQ (view.last<2> ().front (), 4);
  EXPECT_EQ (view.last<0> ().data (), values + 5);
  EXPECT_EQ (view.last<5> ().data (), values);
}

TEST (LumexSpanSubviewsTest,
      GivenStaticSpan_WhenSubspanOffsetOnly_ThenRestWithStaticExtent)
{
  int values[5] = { 1, 2, 3, 4, 5 };
  span<int, 5> const view (values);
  expect_type<span<int, 5>> (view.subspan<0> ());
  expect_type<span<int, 4>> (view.subspan<1> ());
  expect_type<span<int, 1>> (view.subspan<4> ());
  expect_type<span<int, 0>> (view.subspan<5> ());
  EXPECT_EQ (view.subspan<1> ().data (), values + 1);
  EXPECT_EQ (view.subspan<1> ().size (), 4u);
  EXPECT_EQ (view.subspan<5> ().data (), values + 5);
  EXPECT_TRUE (view.subspan<5> ().empty ());
}

TEST (LumexSpanSubviewsTest,
      GivenStaticSpan_WhenSubspanOffsetAndCount_ThenStaticExtentCount)
{
  int values[5] = { 1, 2, 3, 4, 5 };
  span<int, 5> const view (values);
  expect_type<span<int, 2>> (view.subspan<1, 2> ());
  expect_type<span<int, 0>> (view.subspan<1, 0> ());
  expect_type<span<int, 0>> (view.subspan<5, 0> ());
  expect_type<span<int, 5>> (view.subspan<0, 5> ());
  EXPECT_EQ ((view.subspan<1, 2> ().data ()), values + 1);
  EXPECT_EQ ((view.subspan<1, 2> ().back ()), 3);
  EXPECT_EQ ((view.subspan<3, 2> ().back ()), 5);
}

TEST (LumexSpanSubviewsTest,
      GivenStaticSpan_WhenSubspanCountIsDynamicExtent_ThenRest)
{
  int values[5] = { 1, 2, 3, 4, 5 };
  span<int, 5> const view (values);
  expect_type<span<int, 3>> (view.subspan<2, dynamic_extent> ());
  EXPECT_EQ ((view.subspan<2, dynamic_extent> ().size ()), 3u);
}

// -- The same on a dynamic extent --

TEST (LumexSpanSubviewsTest,
      GivenDynamicSpan_WhenStaticSubviews_ThenResultExtents)
{
  int values[5] = { 1, 2, 3, 4, 5 };
  span<int> const view (values);
  expect_type<span<int, 2>> (view.first<2> ());
  expect_type<span<int, 2>> (view.last<2> ());
  expect_type<span<int, dynamic_extent>> (view.subspan<1> ());
  expect_type<span<int, 2>> (view.subspan<1, 2> ());
  EXPECT_EQ (view.first<2> ().back (), 2);
  EXPECT_EQ (view.last<2> ().front (), 4);
  EXPECT_EQ (view.subspan<1> ().size (), 4u);
  EXPECT_EQ (view.subspan<1> ().front (), 2);
  EXPECT_EQ ((view.subspan<1, 2> ().data ()), values + 1);
}

TEST (LumexSpanSubviewsTest,
      GivenDynamicSpan_WhenStaticCountEqualsSize_ThenWhole)
{
  int values[3] = { 1, 2, 3 };
  span<int> const view (values);
  EXPECT_EQ (view.first<3> ().data (), values);
  EXPECT_EQ (view.last<3> ().data (), values);
  EXPECT_EQ ((view.subspan<0, 3> ().data ()), values);
  EXPECT_EQ (view.subspan<3> ().data (), values + 3);
  EXPECT_TRUE (view.subspan<3> ().empty ());
}

// -- Count given at run time --

TEST (LumexSpanSubviewsTest,
      GivenSpan_WhenFirstCountAtRunTime_ThenDynamicExtent)
{
  int values[5] = { 1, 2, 3, 4, 5 };
  span<int, 5> const fixed (values);
  span<int> const dynamic (values);
  expect_type<span<int, dynamic_extent>> (fixed.first (2));
  expect_type<span<int, dynamic_extent>> (dynamic.first (2));
  EXPECT_EQ (fixed.first (2).size (), 2u);
  EXPECT_EQ (dynamic.first (2).data (), values);
  EXPECT_EQ (dynamic.first (2).back (), 2);
  EXPECT_TRUE (dynamic.first (0).empty ());
  EXPECT_EQ (dynamic.first (0).data (), values);
  EXPECT_EQ (dynamic.first (5).size (), 5u);
  EXPECT_EQ (dynamic.first (5).data (), values);
}

TEST (LumexSpanSubviewsTest,
      GivenSpan_WhenLastCountAtRunTime_ThenDynamicExtent)
{
  int values[5] = { 1, 2, 3, 4, 5 };
  span<int, 5> const fixed (values);
  span<int> const dynamic (values);
  expect_type<span<int, dynamic_extent>> (fixed.last (2));
  expect_type<span<int, dynamic_extent>> (dynamic.last (2));
  EXPECT_EQ (dynamic.last (2).size (), 2u);
  EXPECT_EQ (dynamic.last (2).data (), values + 3);
  EXPECT_EQ (dynamic.last (2).front (), 4);
  EXPECT_TRUE (dynamic.last (0).empty ());
  EXPECT_EQ (dynamic.last (0).data (), values + 5);
  EXPECT_EQ (dynamic.last (5).data (), values);
}

TEST (LumexSpanSubviewsTest, GivenSpan_WhenSubspanAtRunTime_ThenDynamicExtent)
{
  int values[5] = { 1, 2, 3, 4, 5 };
  span<int, 5> const fixed (values);
  span<int> const dynamic (values);
  expect_type<span<int, dynamic_extent>> (fixed.subspan (1));
  expect_type<span<int, dynamic_extent>> (fixed.subspan (1, 2));
  expect_type<span<int, dynamic_extent>> (dynamic.subspan (1, 2));
  EXPECT_EQ (dynamic.subspan (1).size (), 4u);
  EXPECT_EQ (dynamic.subspan (1).data (), values + 1);
  EXPECT_EQ (dynamic.subspan (1, 2).size (), 2u);
  EXPECT_EQ (dynamic.subspan (1, 2).back (), 3);
  EXPECT_EQ (dynamic.subspan (2, dynamic_extent).size (), 3u);
}

TEST (LumexSpanSubviewsTest,
      GivenSpan_WhenSubspanAtTheBorders_ThenEmptyOrWhole)
{
  int values[5] = { 1, 2, 3, 4, 5 };
  span<int> const view (values);
  EXPECT_EQ (view.subspan (0).size (), 5u);
  EXPECT_EQ (view.subspan (0).data (), values);
  EXPECT_EQ (view.subspan (0, 5).size (), 5u);
  EXPECT_EQ (view.subspan (0, 0).size (), 0u);
  EXPECT_EQ (view.subspan (5).size (), 0u);
  EXPECT_EQ (view.subspan (5).data (), values + 5);
  EXPECT_EQ (view.subspan (5, 0).size (), 0u);
  EXPECT_EQ (view.subspan (4, 1).front (), 5);
  EXPECT_EQ (view.subspan (4).size (), 1u);
  EXPECT_EQ (view.subspan (2, 3).back (), 5);
}

TEST (LumexSpanSubviewsTest, GivenEmptySpan_WhenSubviews_ThenEmpty)
{
  span<int> const none;
  EXPECT_TRUE (none.first (0).empty ());
  EXPECT_TRUE (none.last (0).empty ());
  EXPECT_TRUE (none.subspan (0).empty ());
  EXPECT_TRUE (none.subspan (0, 0).empty ());
  EXPECT_TRUE (none.first<0> ().empty ());
  EXPECT_TRUE (none.last<0> ().empty ());
  EXPECT_TRUE (none.subspan<0> ().empty ());
  EXPECT_TRUE ((span<int, 0> ()).subspan<0> ().empty ());
  EXPECT_EQ (none.first (0).data (), nullptr);
}

TEST (LumexSpanSubviewsTest, GivenSubspans_WhenChained_ThenComposeOffsets)
{
  std::vector<int> values = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  span<int> const view (values);
  span<int> const middle = view.subspan (2, 6).subspan (1, 4);
  EXPECT_EQ (middle.size (), 4u);
  EXPECT_EQ (middle.front (), 3);
  EXPECT_EQ (middle.back (), 6);
  EXPECT_EQ (middle.data (), values.data () + 3);
  EXPECT_EQ (view.first (6).last (3).front (), 3);
  EXPECT_EQ (view.last (6).first (2).back (), 5);
  EXPECT_EQ (view.first<8> ().last<3> ().front (), 5);
  EXPECT_EQ ((view.subspan<2> ().subspan<1, 3> ().back ()), 5);
}

TEST (LumexSpanSubviewsTest, GivenSubview_WhenWritten_ThenViewedStorageChanges)
{
  int values[6] = { 0, 0, 0, 0, 0, 0 };
  span<int> const view (values);
  view.subspan (2, 3)[1] = 7;
  view.last (1).front () = 9;
  view.first<1> ().front () = 5;
  EXPECT_EQ (values[0], 5);
  EXPECT_EQ (values[3], 7);
  EXPECT_EQ (values[5], 9);
}

TEST (LumexSpanSubviewsTest, GivenSpanOfConst_WhenSubviews_ThenKeepConst)
{
  int values[4] = { 1, 2, 3, 4 };
  span<int const, 4> const fixed (values);
  span<int const> const dynamic (values);
  expect_type<span<int const, 2>> (fixed.first<2> ());
  expect_type<span<int const, dynamic_extent>> (dynamic.last (2));
  expect_type<span<int const, 3>> (fixed.subspan<1> ());
  expect_type<span<int const, 2>> (dynamic.first<2> ());
}

TEST (LumexSpanSubviewsTest,
      GivenSubviewResult_WhenConvertedOrAssigned_ThenUsable)
{
  int values[5] = { 1, 2, 3, 4, 5 };
  span<int, 5> const view (values);
  span<int> const dynamic = view.first<3> ();
  EXPECT_EQ (dynamic.size (), 3u);
  span<int const> const read_only = view.subspan<1, 2> ();
  EXPECT_EQ (read_only.front (), 2);
  span<int, 2> const fixed (view.subspan (3));
  EXPECT_EQ (fixed.front (), 4);
  span<int> assigned;
  assigned = view.last (2);
  EXPECT_EQ (assigned.back (), 5);
}

TEST (LumexSpanSubviewsTest, GivenSubviews_WhenNoexcept_ThenAllNoexcept)
{
  span<int, 5> fixed (static_cast<int *> (nullptr), 5);
  span<int> dynamic;
  static_assert (noexcept (fixed.first<2> ()), "");
  static_assert (noexcept (fixed.last<2> ()), "");
  static_assert (noexcept (fixed.subspan<1> ()), "");
  static_assert (noexcept (fixed.subspan<1, 2> ()), "");
  static_assert (noexcept (dynamic.first (0)), "");
  static_assert (noexcept (dynamic.last (0)), "");
  static_assert (noexcept (dynamic.subspan (0)), "");
  static_assert (noexcept (dynamic.subspan (0, 0)), "");
  SUCCEED ();
}

TEST (LumexSpanSubviewsTest,
      GivenStaticSpanOfStdArray_WhenSubviews_ThenSameElements)
{
  std::array<int, 6> values = { { 10, 20, 30, 40, 50, 60 } };
  span<int, 6> const view (values);
  EXPECT_EQ (view.first<3> ().back (), 30);
  EXPECT_EQ (view.last<3> ().front (), 40);
  EXPECT_EQ ((view.subspan<2, 2> ().front ()), 30);
  EXPECT_EQ (view.subspan (4, 2).back (), 60);
}
