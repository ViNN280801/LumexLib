// LumexSpanByteTwin.cxx20.tests.cpp
//
// With C++20 and <span> a span of the byte of the span module and a
// std::span<std::byte> convert to each other, with the rules of the other span
// conversions (implicit unless a static extent is made from a dynamic one,
// only by a qualification conversion after the exchange of the byte type).
// The conversion of a span of std::byte to std::span<std::byte> is the one the
// two classes already had and is not touched. GCC 8 accepts -std=c++2a without
// <span>, so there the tests skip.
#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/span/LumexSpan"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"

#if LUMEX_SPAN_HAS_STD_SPAN

using lumex::core::span::view::as_bytes;
using lumex::core::span::view::byte;
using lumex::core::span::view::span;

namespace
{
int
overload (std::span<std::byte const>)
{
  return 1;
}

int
overload (span<byte const>)
{
  return 2;
}
} // namespace

TEST (LumexSpanByteTwinStdSpanTest,
      GivenStdSpan_WhenConvertibleToOwn_ThenByQualificationRules)
{
  static_assert (
      std::is_convertible_v<std::span<std::byte const>, span<byte const>>);
  static_assert (
      std::is_convertible_v<std::span<std::byte>, span<byte const>>);
  static_assert (std::is_convertible_v<std::span<std::byte>, span<byte>>);
  static_assert (
      !std::is_convertible_v<std::span<std::byte const>, span<byte>>);
  static_assert (
      std::is_convertible_v<std::span<std::byte, 4>, span<byte const, 4>>);
  static_assert (
      std::is_convertible_v<std::span<std::byte, 4>, span<byte const>>);
  static_assert (
      !std::is_convertible_v<std::span<std::byte>, span<byte const, 4>>);
  static_assert (
      std::is_constructible_v<span<byte const, 4>, std::span<std::byte>>);
  static_assert (
      !std::is_constructible_v<span<byte const, 4>, std::span<std::byte, 5>>);
  static_assert (!std::is_convertible_v<std::span<unsigned char const>,
                                        span<byte const>>);
  static_assert (std::is_nothrow_constructible_v<span<byte const>,
                                                 std::span<std::byte const>>);
  SUCCEED ();
}

TEST (LumexSpanByteTwinStdSpanTest,
      GivenOwnSpan_WhenConvertibleToStdSpan_ThenByQualificationRules)
{
  static_assert (
      std::is_convertible_v<span<byte const>, std::span<std::byte const>>);
  static_assert (
      std::is_convertible_v<span<byte>, std::span<std::byte const>>);
  static_assert (std::is_convertible_v<span<byte>, std::span<std::byte>>);
  static_assert (
      !std::is_convertible_v<span<byte const>, std::span<std::byte>>);
  static_assert (
      std::is_convertible_v<span<byte, 4>, std::span<std::byte, 4>>);
  static_assert (std::is_convertible_v<span<byte, 4>, std::span<std::byte>>);
  static_assert (!std::is_convertible_v<span<byte>, std::span<std::byte, 4>>);
  static_assert (
      !std::is_convertible_v<span<byte, 4>, std::span<std::byte, 5>>);
  static_assert (!std::is_convertible_v<span<byte const>,
                                        std::span<unsigned char const>>);
  static_assert (std::is_nothrow_convertible_v<span<byte const>,
                                               std::span<std::byte const>>);
  SUCCEED ();
}

TEST (LumexSpanByteTwinStdSpanTest,
      GivenSpanOfStdByte_WhenToStdSpan_ThenTheOldConversionStillWorks)
{
  static_assert (std::is_convertible_v<span<std::byte>, std::span<std::byte>>);
  static_assert (std::is_convertible_v<std::span<std::byte>, span<std::byte>>);
  static_assert (std::is_convertible_v<span<byte>, span<byte>>);
  static_assert (std::is_convertible_v<std::span<byte>, span<byte>>);
  SUCCEED ();
}

TEST (LumexSpanByteTwinStdSpanTest,
      GivenStdSpan_WhenConvertedToOwn_ThenSameMemory)
{
  std::array<std::byte, 4> storage{ { std::byte{ 1 }, std::byte{ 2 },
                                      std::byte{ 3 }, std::byte{ 4 } } };
  std::span<std::byte> const stdView (storage);
  span<byte> const own = stdView;
  EXPECT_EQ (static_cast<void *> (own.data ()),
             static_cast<void *> (storage.data ()));
  EXPECT_EQ (own.size (), 4u);
  own[1] = static_cast<byte> (0x77);
  EXPECT_EQ (storage[1], std::byte{ 0x77 });
  span<byte const, 4> const fixed (stdView);
  EXPECT_EQ (static_cast<unsigned> (fixed[2]), 3u);
  span<byte const, 4> const fromFixed = std::span<std::byte, 4> (storage);
  EXPECT_EQ (fromFixed.size (), 4u);
}

TEST (LumexSpanByteTwinStdSpanTest,
      GivenOwnSpan_WhenConvertedToStdSpan_ThenSameMemory)
{
  std::array<std::uint8_t, 3> storage{ { 7, 8, 9 } };
  auto const own = as_bytes (span<std::uint8_t const> (storage));
  std::span<std::byte const> const stdView = own;
  EXPECT_EQ (static_cast<void const *> (stdView.data ()),
             static_cast<void const *> (storage.data ()));
  EXPECT_EQ (stdView.size (), 3u);
  EXPECT_EQ (stdView[2], std::byte{ 9 });
  // A static extent stays static through the conversion.
  span<byte const, 3> const fixed
      = as_bytes (span<std::uint8_t const, 3> (storage));
  std::span<std::byte const, 3> const stdFixed = fixed;
  EXPECT_EQ (stdFixed.size (), 3u);
  std::span<std::byte const> const stdDynamic = fixed;
  EXPECT_EQ (stdDynamic.size (), 3u);
}

TEST (LumexSpanByteTwinStdSpanTest,
      GivenOverloads_WhenCalled_ThenTheExactTypeWins)
{
  std::array<std::byte, 2> const storage{ { std::byte{ 1 }, std::byte{ 2 } } };
  EXPECT_EQ (overload (std::span<std::byte const> (storage)), 1);
  EXPECT_EQ (overload (as_bytes (span<std::byte const> (storage))), 2)
      << "a span of the own byte takes the overload of its own type";
}

#else

TEST (LumexSpanByteTwinStdSpanTest, GivenNoStdSpan_ThenNothingToCheck)
{
  GTEST_SKIP () << "this toolchain has no <span>";
}

#endif
