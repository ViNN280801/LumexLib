// LumexMemRead.cxx17.tests.cpp
//
// From C++17 the result of as<T> is std::optional<T>, as before the header
// worked from C++11, and std::byte is a byte element of a span. The same
// checks for the optional of this library are in
// LumexMemReadResultType.cxx11.tests.cpp.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/span/LumexSpan"
#include "lumex/core/utility/mem/LumexMemRead.hpp"

using lumex::core::span::view::span;
using lumex::core::utility::mem::as;
using lumex::core::utility::mem::optional_t;

namespace
{
struct source_with_data
{
  void const *
  get_data () const
  {
    return nullptr;
  }
  int
  get_data_size () const
  {
    return 0;
  }
};
} // namespace

TEST (LumexMemReadCxx17Test, GivenCxx17_WhenAlias_ThenStdOptional)
{
  static_assert (std::is_same<optional_t<std::uint32_t>,
                              std::optional<std::uint32_t>>::value,
                 "optional_t is std::optional from C++17");
  static_assert (
      std::is_same<optional_t<double>, std::optional<double>>::value,
      "for every value type");
  SUCCEED ();
}

TEST (LumexMemReadCxx17Test,
      GivenCxx17_WhenEveryOverload_ThenReturnsStdOptional)
{
  void const *pointer = nullptr;
  std::size_t size = 0;
  static_assert (std::is_same<decltype (as<std::uint32_t> (pointer, size)),
                              std::optional<std::uint32_t>>::value,
                 "pointer and size");
  static_assert (
      std::is_same<decltype (as<std::uint32_t> (source_with_data ())),
                   std::optional<std::uint32_t>>::value,
      "source object");
  static_assert (
      std::is_same<decltype (as<std::uint32_t> (span<unsigned char const> ())),
                   std::optional<std::uint32_t>>::value,
      "span of this library");
  SUCCEED ();
}

TEST (LumexMemReadCxx17Test, GivenCxx17_WhenEmptyResult_ThenEqualsStdNullopt)
{
  std::uint8_t const one = 1;
  EXPECT_EQ (as<std::uint32_t> (&one, sizeof (one)), std::nullopt);
  EXPECT_EQ (as<std::uint32_t> (nullptr, 4), std::nullopt);
  std::uint32_t const value = 9;
  EXPECT_EQ (as<std::uint32_t> (&value, sizeof (value)),
             std::optional<std::uint32_t> (9u));
}

TEST (LumexMemReadCxx17Test, GivenStdByteBuffer_WhenAsFromPointer_ThenDecodes)
{
  std::uint32_t const value = 0x0A0B0C0Du;
  std::array<std::byte, sizeof (value)> buffer{};
  std::memcpy (buffer.data (), &value, sizeof (value));
  auto const result = as<std::uint32_t> (buffer.data (), buffer.size ());
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (*result, value);
}

TEST (LumexMemReadCxx17Test, GivenSpanOfStdByte_WhenAs_ThenDecodes)
{
  // The byte of the span module is std::byte from C++17.
  static_assert (std::is_same<lumex::core::span::view::byte, std::byte>::value,
                 "byte is std::byte from C++17");
  std::uint16_t const value = 0x1234;
  std::array<std::byte, sizeof (value)> buffer{};
  std::memcpy (buffer.data (), &value, sizeof (value));
  auto const result = as<std::uint16_t> (span<std::byte const> (buffer));
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (*result, value);
}
