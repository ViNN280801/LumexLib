// LumexSpanByteTwin.cxx17.tests.cpp
//
// The byte of the span module is an own enumeration in every standard. From
// C++17 a span of it and a span of std::byte convert to each other, with the
// rules of the other span conversions: implicit unless a static extent is made
// from a dynamic one, only by a qualification conversion after the exchange of
// the byte type (std::byte const to byte const, never to byte), and never from
// an unrelated byte-sized type. Each check is falsified by removing the
// constructor template it names.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/span/LumexSpan"

using lumex::core::span::view::as_bytes;
using lumex::core::span::view::as_writable_bytes;
using lumex::core::span::view::byte;
using lumex::core::span::view::dynamic_extent;
using lumex::core::span::view::span;

namespace
{
int
overload (span<byte const>)
{
  return 1;
}

int
overload (span<std::uint16_t const>)
{
  return 2;
}
} // namespace

TEST (LumexSpanByteTwinTest,
      GivenTwoByteTypes_WhenSpansConvertible_ThenInBothDirections)
{
  static_assert (
      std::is_convertible<span<std::byte const>, span<byte const>>::value,
      "std::byte const to byte const");
  static_assert (std::is_convertible<span<std::byte>, span<byte const>>::value,
                 "std::byte to byte const");
  static_assert (std::is_convertible<span<std::byte>, span<byte>>::value,
                 "std::byte to byte");
  static_assert (
      std::is_convertible<span<byte const>, span<std::byte const>>::value,
      "byte const to std::byte const");
  static_assert (std::is_convertible<span<byte>, span<std::byte const>>::value,
                 "byte to std::byte const");
  static_assert (std::is_convertible<span<byte>, span<std::byte>>::value,
                 "byte to std::byte");
  SUCCEED ();
}

TEST (LumexSpanByteTwinTest,
      GivenConstness_WhenSpansConvertible_ThenNeverDropsConst)
{
  static_assert (
      !std::is_convertible<span<std::byte const>, span<byte>>::value,
      "const cannot be dropped");
  static_assert (
      !std::is_convertible<span<byte const>, span<std::byte>>::value,
      "const cannot be dropped");
  static_assert (
      !std::is_constructible<span<byte>, span<std::byte const>>::value,
      "not even explicitly");
  SUCCEED ();
}

TEST (LumexSpanByteTwinTest,
      GivenExtents_WhenSpansConvertible_ThenLikeOtherSpans)
{
  static_assert (
      std::is_convertible<span<std::byte, 4>, span<byte const, 4>>::value,
      "static to the same static extent");
  static_assert (
      std::is_convertible<span<std::byte, 4>, span<byte const>>::value,
      "static to dynamic");
  static_assert (
      !std::is_convertible<span<std::byte>, span<byte const, 4>>::value,
      "dynamic to static is explicit");
  static_assert (
      std::is_constructible<span<byte const, 4>, span<std::byte>>::value,
      "dynamic to static is allowed explicitly");
  static_assert (
      !std::is_constructible<span<byte const, 4>, span<std::byte, 5>>::value,
      "different static extents never");
  static_assert (std::is_convertible<span<byte, 4>, span<std::byte, 4>>::value,
                 "the reverse direction keeps the rule");
  static_assert (!std::is_convertible<span<byte>, span<std::byte, 4>>::value,
                 "the reverse direction keeps the rule");
  SUCCEED ();
}

TEST (LumexSpanByteTwinTest,
      GivenOtherElementTypes_WhenSpansConvertible_ThenNot)
{
  static_assert (
      !std::is_convertible<span<unsigned char const>, span<byte const>>::value,
      "unsigned char is not a twin of byte");
  static_assert (
      !std::is_convertible<span<char const>, span<byte const>>::value,
      "char is not a twin of byte");
  static_assert (!std::is_convertible<span<std::int8_t const>,
                                      span<std::byte const>>::value,
                 "int8_t is not a twin of std::byte");
  static_assert (
      !std::is_convertible<span<byte const>, span<unsigned char const>>::value,
      "byte does not become unsigned char");
  SUCCEED ();
}

TEST (LumexSpanByteTwinTest, GivenSpans_WhenConverted_ThenNoexcept)
{
  static_assert (std::is_nothrow_constructible<span<byte const>,
                                               span<std::byte const>>::value,
                 "noexcept like the other span conversions");
  static_assert (std::is_nothrow_constructible<span<std::byte const>,
                                               span<byte const>>::value,
                 "noexcept like the other span conversions");
  static_assert (std::is_nothrow_constructible<span<byte const, 4>,
                                               span<std::byte>>::value,
                 "the explicit one is noexcept too");
  SUCCEED ();
}

TEST (LumexSpanByteTwinTest,
      GivenStdByteSpan_WhenConvertedToOwn_ThenSameMemory)
{
  std::array<std::byte, 4> storage{ { std::byte{ 1 }, std::byte{ 2 },
                                      std::byte{ 3 }, std::byte{ 4 } } };
  span<std::byte> const stdView (storage);
  span<byte> const own = stdView;
  EXPECT_EQ (static_cast<void *> (own.data ()),
             static_cast<void *> (storage.data ()));
  EXPECT_EQ (own.size (), 4u);
  EXPECT_EQ (static_cast<unsigned> (own[2]), 3u);
  own[0] = static_cast<byte> (9);
  EXPECT_EQ (storage[0], std::byte{ 9 })
      << "a write through the own view is seen";
  span<byte const, 4> const fixed (stdView);
  EXPECT_EQ (fixed.extent, 4u);
  EXPECT_EQ (static_cast<unsigned> (fixed[3]), 4u);
}

TEST (LumexSpanByteTwinTest,
      GivenOwnByteSpan_WhenConvertedToStd_ThenSameMemory)
{
  std::array<std::uint8_t, 3> storage{ { 7, 8, 9 } };
  span<byte> const own = as_writable_bytes (span<std::uint8_t> (storage));
  span<std::byte> const stdView = own;
  EXPECT_EQ (static_cast<void *> (stdView.data ()),
             static_cast<void *> (storage.data ()));
  EXPECT_EQ (stdView.size (), 3u);
  EXPECT_EQ (stdView[1], std::byte{ 8 });
  stdView[2] = std::byte{ 0x55 };
  EXPECT_EQ (storage[2], 0x55) << "a write through the std::byte view is seen";
}

TEST (LumexSpanByteTwinTest,
      GivenAsBytes_WhenResultUsedAsStdByteSpan_ThenWorks)
{
  // as_bytes gives the own byte; a program that holds std::byte views takes
  // it.
  std::array<std::uint16_t, 2> const values{ { 0x0102, 0x0304 } };
  span<std::byte const> const stdView
      = as_bytes (span<std::uint16_t const> (values));
  EXPECT_EQ (stdView.size (), 4u);
  std::uint16_t back = 0;
  std::memcpy (&back, stdView.data () + 2, sizeof (back));
  EXPECT_EQ (back, 0x0304);
}

TEST (
    LumexSpanByteTwinTest,
    GivenOverloads_WhenCalledWithStdByteSpan_ThenOwnByteOverloadIsTheOneThatFits)
{
  std::array<std::byte, 2> const storage{ { std::byte{ 1 }, std::byte{ 2 } } };
  EXPECT_EQ (
      overload (span<byte const> (as_bytes (span<std::byte const> (storage)))),
      1);
  EXPECT_EQ (overload (span<std::byte const> (storage)), 1)
      << "the span of std::byte takes the byte overload by conversion";
  std::array<std::uint16_t, 1> const words{ { 1 } };
  EXPECT_EQ (overload (span<std::uint16_t const> (words)), 2);
}
