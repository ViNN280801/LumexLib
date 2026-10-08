// LumexMemReadResultType.cxx11.tests.cpp
//
// The result type of as<T>: the optional of this library before C++17
// (std::optional from C++17 is checked in LumexMemRead.cxx17.tests.cpp). The
// optional_t alias is the one place that chooses; every overload returns it.
#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/span/LumexSpan"
#include "lumex/core/utility/mem/LumexMemRead.hpp"

#if __cplusplus < 201703L

namespace
{
using lumex::core::span::view::span;
using lumex::core::utility::mem::as;
using lumex::core::utility::mem::optional_t;

using lumex_optional = lumex::core::optional::opt::optional<std::uint32_t>;

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

TEST (LumexMemReadResultTypeTest,
      GivenBeforeCxx17_WhenAlias_ThenTheOptionalOfTheLibrary)
{
  static_assert (
      std::is_same<optional_t<std::uint32_t>, lumex_optional>::value,
      "optional_t is the optional of the library before C++17");
  static_assert (
      std::is_same<optional_t<double>,
                   lumex::core::optional::opt::optional<double>>::value,
      "for every value type");
  SUCCEED ();
}

TEST (LumexMemReadResultTypeTest,
      GivenBeforeCxx17_WhenEveryOverload_ThenReturnsTheOptionalOfTheLibrary)
{
  void const *pointer = nullptr;
  std::size_t size = 0;
  static_assert (std::is_same<decltype (as<std::uint32_t> (pointer, size)),
                              lumex_optional>::value,
                 "pointer and size");
  static_assert (
      std::is_same<decltype (as<std::uint32_t> (source_with_data ())),
                   lumex_optional>::value,
      "source object");
  static_assert (
      std::is_same<decltype (as<std::uint32_t> (span<unsigned char const> ())),
                   lumex_optional>::value,
      "span of this library");
  SUCCEED ();
}

TEST (LumexMemReadResultTypeTest,
      GivenBeforeCxx17_WhenValueAndEmpty_ThenOptionalOfTheLibraryBehaves)
{
  std::uint32_t const value = 77;
  lumex_optional const full = as<std::uint32_t> (&value, sizeof (value));
  lumex_optional const none = as<std::uint32_t> (&value, 1);
  ASSERT_TRUE (full.has_value ());
  EXPECT_EQ (*full, 77u);
  EXPECT_EQ (full.value (), 77u);
  EXPECT_FALSE (none.has_value ());
  EXPECT_TRUE (none == lumex::core::optional::opt::nullopt);
  EXPECT_EQ (none.value_or (5u), 5u);
}

#endif
