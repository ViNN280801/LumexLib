// LumexMemReadSpan.cxx11.tests.cpp
//
// as<T> over the span of this library (lumex::core::span::view::span), from
// C++11: the element types char, unsigned char and the byte of the span module
// (std::byte from C++17, an own enumeration before it). A template deduces the
// element type from the exact span type, so each kind of span takes its own
// overload; the std::span one is in LumexMemReadStdSpan.cxx20.tests.cpp.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/span/LumexSpan"
#include "lumex/core/utility/mem/LumexMemRead.hpp"

using lumex::core::span::view::byte;
using lumex::core::span::view::span;
using lumex::core::utility::mem::as;

TEST (LumexMemReadSpanTest, GivenByteSpan_WhenAs_ThenDecodesValue)
{
  std::uint32_t const value = 0x11223344;
  std::array<byte, sizeof (value)> buffer = {};
  std::memcpy (buffer.data (), &value, sizeof (value));

  span<byte const> const view (buffer);
  auto const result = as<std::uint32_t> (view);
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (*result, value);
}

TEST (LumexMemReadSpanTest,
      GivenCharAndUnsignedCharSpans_WhenAs_ThenDecodesValue)
{
  std::uint16_t const value = 0xABCD;
  std::array<char, sizeof (value)> chars = {};
  std::array<unsigned char, sizeof (value)> bytes = {};
  std::memcpy (chars.data (), &value, sizeof (value));
  std::memcpy (bytes.data (), &value, sizeof (value));

  EXPECT_EQ (*as<std::uint16_t> (span<char const> (chars)), value);
  EXPECT_EQ (*as<std::uint16_t> (span<unsigned char const> (bytes)), value);
}

TEST (LumexMemReadSpanTest, GivenSpanTooSmall_WhenAs_ThenReturnsNullopt)
{
  std::array<char, 2> buffer = { { 'a', 'b' } };
  EXPECT_FALSE (as<std::uint32_t> (span<char const> (buffer)).has_value ());
  EXPECT_FALSE (as<std::uint32_t> (span<char const> ()).has_value ());
  EXPECT_FALSE (
      as<std::uint16_t> (span<char const> (buffer).first (1)).has_value ());
}

TEST (LumexMemReadSpanTest, GivenSubspan_WhenAs_ThenReadsFromItsStart)
{
  std::array<std::uint8_t, 8> const buffer = { { 1, 0, 0, 0, 2, 0, 0, 0 } };
  span<std::uint8_t const> const whole (buffer);
  EXPECT_EQ (*as<std::uint32_t> (whole.first (4)), 1u);
  EXPECT_EQ (*as<std::uint32_t> (whole.subspan (4)), 2u);
  EXPECT_EQ (*as<std::uint32_t> (whole.last (4)), 2u);
}

TEST (LumexMemReadSpanTest,
      GivenSpanOfWritableElements_WhenAsThroughConstSpan_ThenDecodes)
{
  // span<char> converts to span<char const>, the type the overload takes.
  std::vector<char> buffer (sizeof (std::uint32_t), 0);
  std::uint32_t const value = 0x01020304u;
  std::memcpy (buffer.data (), &value, sizeof (value));
  span<char> const mutable_view (buffer.data (), buffer.size ());
  EXPECT_EQ (*as<std::uint32_t> (span<char const> (mutable_view)), value);
}

TEST (LumexMemReadSpanTest, GivenStructType_WhenAsFromSpan_ThenDecodesStruct)
{
  struct Pair
  {
    std::int16_t low;
    std::int16_t high;
  };
  Pair const pair = { -5, 300 };
  std::array<unsigned char, sizeof (Pair)> buffer = {};
  std::memcpy (buffer.data (), &pair, sizeof (pair));

  auto const result = as<Pair> (span<unsigned char const> (buffer));
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (result->low, -5);
  EXPECT_EQ (result->high, 300);
}

TEST (LumexMemReadSpanTest,
      GivenStaticExtentSpan_WhenAs_ThenDeducesTheElementType)
{
  // A template deduces from the exact type: a static extent does not match
  // span<ByteType const>, so the call converts it first.
  std::array<byte, 4> const buffer = {};
  span<byte const, 4> const fixed (buffer);
  EXPECT_TRUE (as<std::uint32_t> (span<byte const> (fixed)).has_value ());
}
