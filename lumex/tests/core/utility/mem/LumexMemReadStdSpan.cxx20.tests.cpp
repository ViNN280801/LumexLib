// LumexMemReadStdSpan.cxx20.tests.cpp
//
// as<T> over std::span (C++20) next to the span of this library: a template
// deduces the element type from the exact span type, so a std::span takes its
// own overload. A toolchain that accepts -std=c++2a without <span> (GCC 8)
// skips the tests.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#if __has_include(<span>)
#include <span>
#endif

#include <gtest/gtest.h>

#include "lumex/core/span/LumexSpan"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/mem/LumexMemRead.hpp"

#if LUMEX_HAS_STD_SPAN

using lumex::core::utility::mem::as;

TEST (LumexMemReadStdSpanTest, GivenByteSpan_WhenAs_ThenDecodesValue)
{
  std::uint32_t const value = 0x11223344;
  std::array<std::byte, sizeof (value)> buffer{};
  std::memcpy (buffer.data (), &value, sizeof (value));

  std::span<std::byte const> const view (buffer);
  auto const result = as<std::uint32_t> (view);
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (*result, value);
}

TEST (LumexMemReadStdSpanTest, GivenCharSpanTooSmall_WhenAs_ThenReturnsNullopt)
{
  std::array<char, 2> buffer{ 'a', 'b' };
  std::span<char const> const view (buffer);

  auto const result = as<std::uint32_t> (view);
  EXPECT_FALSE (result.has_value ());
}

TEST (LumexMemReadStdSpanTest, GivenUnsignedCharSpan_WhenAs_ThenDecodesValue)
{
  std::uint16_t const value = 0xBEEF;
  std::array<unsigned char, sizeof (value)> buffer{};
  std::memcpy (buffer.data (), &value, sizeof (value));
  std::span<unsigned char const> const view (buffer);
  auto const result = as<std::uint16_t> (view);
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (*result, value);
}

TEST (LumexMemReadStdSpanTest, GivenCharSpanExactSize_WhenAs_ThenDecodesValue)
{
  std::uint16_t const value = 0x3344;
  std::array<char, sizeof (value)> buffer{};
  std::memcpy (buffer.data (), &value, sizeof (value));
  std::span<char const> const view (buffer);
  auto const result = as<std::uint16_t> (view);
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (*result, value);
}

TEST (LumexMemReadStdSpanTest, GivenEmptyByteSpan_WhenAs_ThenReturnsNullopt)
{
  std::span<std::byte const> const view;
  EXPECT_FALSE (as<std::uint32_t> (view).has_value ());
}

TEST (LumexMemReadStdSpanTest,
      GivenStdSpanAndLumexSpan_WhenAs_ThenBothOverloadsAgree)
{
  std::array<std::byte, 4> const buffer
      = { { std::byte{ 1 }, std::byte{ 0 }, std::byte{ 0 }, std::byte{ 0 } } };
  EXPECT_EQ (*as<std::uint32_t> (std::span<std::byte const> (buffer)),
             *as<std::uint32_t> (
                 lumex::core::span::view::span<std::byte const> (buffer)));
  EXPECT_EQ (*as<std::uint32_t> (std::span<std::byte const> (buffer)), 1u);
}

#else

TEST (LumexMemReadStdSpanTest, GivenNoStdSpan_WhenCompiled_ThenSkipped)
{
  GTEST_SKIP () << "the standard library has no <span>";
}

#endif
