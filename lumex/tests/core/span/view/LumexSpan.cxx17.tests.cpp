// LumexSpan.cxx17.tests.cpp
//
// What C++17 adds: the deduction guides, the conversions between the byte of
// the span module and std::byte, constexpr std::array and the relaxed constexpr rules used with a span.
#include <array>
#include <cstddef>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/span/LumexSpan"

using lumex::core::span::view::as_bytes;
using lumex::core::span::view::byte;
using lumex::core::span::view::dynamic_extent;
using lumex::core::span::view::span;

namespace
{
constexpr int
sum_with_loop (span<int const> view)
{
  int total = 0;
  for (int element : view)
    {
      total += element;
    }
  return total;
}

constexpr int
fill_and_sum ()
{
  std::array<int, 4> values{};
  span<int> view (values);
  for (std::size_t index = 0; index < view.size (); ++index)
    {
      view[index] = static_cast<int> (index) + 1;
    }
  return sum_with_loop (view);
}
} // namespace

// -- Deduction guides --

TEST (LumexSpanDeductionTest, GivenBuiltInArray_WhenDeduced_ThenStaticExtent)
{
  int values[3] = { 1, 2, 3 };
  span deduced (values);
  static_assert (std::is_same<decltype (deduced), span<int, 3>>::value, "");
  int const read_only[2] = { 1, 2 };
  span deduced_const (read_only);
  static_assert (
      std::is_same<decltype (deduced_const), span<int const, 2>>::value, "");
  EXPECT_EQ (deduced.size (), 3u);
}

TEST (LumexSpanDeductionTest, GivenStdArray_WhenDeduced_ThenStaticExtent)
{
  std::array<int, 4> values = { { 1, 2, 3, 4 } };
  span deduced (values);
  static_assert (std::is_same<decltype (deduced), span<int, 4>>::value, "");
  std::array<int, 4> const read_only = { { 1, 2, 3, 4 } };
  span deduced_const (read_only);
  static_assert (
      std::is_same<decltype (deduced_const), span<int const, 4>>::value, "");
  EXPECT_EQ (deduced_const.size (), 4u);
}

TEST (LumexSpanDeductionTest,
      GivenPointerAndCount_WhenDeduced_ThenDynamicExtent)
{
  int values[4] = { 1, 2, 3, 4 };
  span deduced (values + 1, 2);
  static_assert (std::is_same<decltype (deduced), span<int>>::value, "");
  EXPECT_EQ (deduced.size (), 2u);
  int const *const first = values;
  span deduced_const (first, std::size_t{ 3 });
  static_assert (
      std::is_same<decltype (deduced_const), span<int const>>::value, "");
  EXPECT_EQ (deduced_const.size (), 3u);
}

TEST (LumexSpanDeductionTest, GivenPointerPair_WhenDeduced_ThenDynamicExtent)
{
  int values[4] = { 1, 2, 3, 4 };
  span deduced (values, values + 3);
  static_assert (std::is_same<decltype (deduced), span<int>>::value, "");
  EXPECT_EQ (deduced.size (), 3u);
}

TEST (LumexSpanDeductionTest,
      GivenContainer_WhenDeduced_ThenDynamicExtentOfItsElement)
{
  std::vector<int> values = { 1, 2, 3 };
  span deduced (values);
  static_assert (std::is_same<decltype (deduced), span<int>>::value, "");
  std::vector<int> const read_only = { 1, 2, 3 };
  span deduced_const (read_only);
  static_assert (
      std::is_same<decltype (deduced_const), span<int const>>::value, "");
  std::string text = "abc";
  span deduced_text (text);
  static_assert (std::is_same<decltype (deduced_text), span<char>>::value, "");
  EXPECT_EQ (deduced_text.size (), 3u);
}

TEST (LumexSpanDeductionTest, GivenSpan_WhenDeduced_ThenSameSpanType)
{
  int values[3] = { 1, 2, 3 };
  span<int, 3> const fixed (values);
  span copy_fixed (fixed);
  static_assert (std::is_same<decltype (copy_fixed), span<int, 3>>::value,
                 "a copy of a static span stays static");
  span<int> const dynamic (values);
  span copy_dynamic (dynamic);
  static_assert (std::is_same<decltype (copy_dynamic), span<int>>::value, "");
  span<int const> const read_only (values);
  span copy_read_only (read_only);
  static_assert (
      std::is_same<decltype (copy_read_only), span<int const>>::value, "");
  EXPECT_EQ (copy_fixed.size (), 3u);
}

TEST (LumexSpanDeductionTest,
      GivenBracedInitialization_WhenDeduced_ThenSameAsParentheses)
{
  int values[3] = { 1, 2, 3 };
  span braced{ values };
  static_assert (std::is_same<decltype (braced), span<int, 3>>::value, "");
  span braced_pair{ values, values + 2 };
  static_assert (std::is_same<decltype (braced_pair), span<int>>::value, "");
  EXPECT_EQ (braced_pair.size (), 2u);
}

// -- std::byte --

TEST (LumexSpanCxx17Test, GivenCxx17_WhenByte_ThenOwnByteNotStdByte)
{
  static_assert (!std::is_same<byte, std::byte>::value, "");
  int values[2] = { 1, 2 };
  auto bytes = as_bytes (span<int> (values));
  static_assert (std::is_same<decltype (bytes),
                              span<byte const, dynamic_extent>>::value,
                 "");
  EXPECT_EQ (bytes.size (), 2 * sizeof (int));
  EXPECT_EQ (lumex::core::span::view::to_integer<int> (
                 as_bytes (span<int, 2> (values))[0]),
             static_cast<int> (reinterpret_cast<unsigned char *> (values)[0]));
  // The byte view converts to a view of std::byte (LumexSpanByteTwin has the
  // whole matrix).
  span<std::byte const> const standard = bytes;
  EXPECT_EQ (standard.size (), bytes.size ());
  EXPECT_EQ (static_cast<void const *> (standard.data ()),
             static_cast<void const *> (bytes.data ()));
}

// -- Relaxed constexpr --

TEST (LumexSpanCxx17Test,
      GivenStdArray_WhenConstexpr_ThenSpanWorksInConstantExpression)
{
  static constexpr std::array<int, 4> numbers = { { 2, 4, 6, 8 } };
  constexpr span<int const, 4> view (numbers);
  static_assert (view.size () == 4, "");
  static_assert (view[3] == 8, "");
  static_assert (view.subspan (1, 2).front () == 4, "");
  static_assert (sum_with_loop (view) == 20, "a loop over a span");
  EXPECT_EQ (sum_with_loop (view), 20);
}

TEST (LumexSpanCxx17Test,
      GivenLocalArrayInConstexprFunction_WhenWrittenThroughSpan_ThenResult)
{
  static_assert (fill_and_sum () == 10,
                 "writes through a span in a constant expression");
  EXPECT_EQ (fill_and_sum (), 10);
}
