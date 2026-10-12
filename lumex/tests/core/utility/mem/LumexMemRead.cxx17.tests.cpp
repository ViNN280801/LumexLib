// LumexMemRead.cxx17.tests.cpp
//
// From C++17 the result of as<T> (the optional of this library, in every
// standard) converts to and compares with std::optional<T>, and std::byte is a
// byte element of a span. The type itself is pinned in
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

TEST (LumexMemReadCxx17Test, GivenCxx17_WhenAlias_ThenOwnOptionalNotStd)
{
  static_assert (!std::is_same<optional_t<std::uint32_t>,
                               std::optional<std::uint32_t>>::value,
                 "optional_t is not std::optional in any standard");
  static_assert (
      std::is_same<optional_t<std::uint32_t>,
                   lumex::core::optional::opt::optional<std::uint32_t>>::value,
      "optional_t is the optional of the library");
  SUCCEED ();
}

TEST (LumexMemReadCxx17Test,
      GivenCxx17_WhenEveryOverload_ThenReturnsOwnOptionalConvertibleToStd)
{
  void const *pointer = nullptr;
  std::size_t size = 0;
  using own = optional_t<std::uint32_t>;
  static_assert (
      std::is_same<decltype (as<std::uint32_t> (pointer, size)), own>::value,
      "pointer and size");
  static_assert (
      std::is_same<decltype (as<std::uint32_t> (source_with_data ())),
                   own>::value,
      "source object");
  static_assert (
      std::is_same<decltype (as<std::uint32_t> (span<unsigned char const> ())),
                   own>::value,
      "span of this library");
  static_assert (std::is_convertible<own, std::optional<std::uint32_t>>::value,
                 "the result converts to std::optional");
  static_assert (std::is_convertible<std::optional<std::uint32_t>, own>::value,
                 "and std::optional converts to the result");
  SUCCEED ();
}

TEST (LumexMemReadCxx17Test, GivenCxx17_WhenResultToStdOptional_ThenSameValue)
{
  std::uint8_t const one = 1;
  std::uint32_t const value = 9;
  std::optional<std::uint32_t> const none
      = as<std::uint32_t> (&one, sizeof (one));
  std::optional<std::uint32_t> const full
      = as<std::uint32_t> (&value, sizeof (value));
  EXPECT_FALSE (none.has_value ());
  EXPECT_EQ (full, std::optional<std::uint32_t> (9u));
  EXPECT_EQ (as<std::uint32_t> (&value, sizeof (value)),
             std::optional<std::uint32_t> (9u));
  EXPECT_NE (as<std::uint32_t> (nullptr, 4),
             std::optional<std::uint32_t> (9u));
  EXPECT_EQ (as<std::uint32_t> (nullptr, 4), std::optional<std::uint32_t> ());
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
  std::uint16_t const value = 0x1234;
  std::array<std::byte, sizeof (value)> buffer{};
  std::memcpy (buffer.data (), &value, sizeof (value));
  auto const result = as<std::uint16_t> (span<std::byte const> (buffer));
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (*result, value);
}
