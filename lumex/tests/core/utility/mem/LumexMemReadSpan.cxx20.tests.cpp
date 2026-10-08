// LumexMemReadSpan.cxx20.tests.cpp
//
// as<T> over the span of this library (lumex::core::span::view::span): the
// overload next to the one for std::span. A template deduces the element type
// from the exact span type, so each kind of span takes its own overload.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#if __has_include(<span>)
#include <span>
#endif
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/span/LumexSpan"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/mem/LumexMemRead.hpp"

#if LUMEX_HAS_STD_CONCEPTS && LUMEX_HAS_STD_SPAN

using lumex::core::span::view::span;
using lumex::core::utility::mem::as;

TEST (LumexMemReadSpanTest, GivenByteSpan_WhenAs_ThenDecodesValue)
{
  std::uint32_t const value = 0x11223344;
  std::array<std::byte, sizeof (value)> buffer{};
  std::memcpy (buffer.data (), &value, sizeof (value));

  span<std::byte const> const view (buffer);
  auto const result = as<std::uint32_t> (view);
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (*result, value);
}

TEST (LumexMemReadSpanTest,
      GivenCharAndUnsignedCharSpans_WhenAs_ThenDecodesValue)
{
  std::uint16_t const value = 0xABCD;
  std::array<char, sizeof (value)> chars{};
  std::array<unsigned char, sizeof (value)> bytes{};
  std::memcpy (chars.data (), &value, sizeof (value));
  std::memcpy (bytes.data (), &value, sizeof (value));

  EXPECT_EQ (*as<std::uint16_t> (span<char const> (chars)), value);
  EXPECT_EQ (*as<std::uint16_t> (span<unsigned char const> (bytes)), value);
}

TEST (LumexMemReadSpanTest, GivenSpanTooSmall_WhenAs_ThenReturnsNullopt)
{
  std::array<char, 2> buffer{ 'a', 'b' };
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
      GivenStdSpanAndLumexSpan_WhenAs_ThenBothOverloadsAgree)
{
  std::array<std::byte, 4> const buffer
      = { { std::byte{ 1 }, std::byte{ 0 }, std::byte{ 0 }, std::byte{ 0 } } };
  EXPECT_EQ (*as<std::uint32_t> (std::span<std::byte const> (buffer)),
             *as<std::uint32_t> (span<std::byte const> (buffer)));
  EXPECT_EQ (*as<std::uint32_t> (std::span<std::byte const> (buffer)), 1u);
}

TEST (LumexMemReadSpanTest,
      GivenStaticExtentSpan_WhenAs_ThenDeducesTheElementType)
{
  // A template deduces from the exact type: a static extent does not match
  // span<ByteType const>, so the call converts it first.
  std::array<std::byte, 4> const buffer{};
  span<std::byte const, 4> const fixed (buffer);
  EXPECT_TRUE (as<std::uint32_t> (span<std::byte const> (fixed)).has_value ());
}

#else

TEST (LumexMemReadSpanTest, GivenNoConceptsOrSpan_WhenCompiled_ThenSkipped)
{
  GTEST_SKIP () << "LumexMemRead.hpp needs C++20 <concepts> and <span>";
}

#endif
