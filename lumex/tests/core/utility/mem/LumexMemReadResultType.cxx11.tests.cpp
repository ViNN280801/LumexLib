// LumexMemReadResultType.cxx11.tests.cpp
//
// The result type of as<T>: the optional of this library in every standard,
// never std::optional (the conversions to and from std::optional<T> from
// C++17 are checked in LumexMemRead.cxx17.tests.cpp). optional_t is a plain
// alias of the own class; every overload returns it. The static_asserts below
// are compiled by the suites of C++11, 17 and 20 and pin the same type in
// each.
#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#if __cplusplus >= 201703L
#include <optional>
#endif

#include <gtest/gtest.h>

#include "lumex/core/span/LumexSpan"
#include "lumex/core/utility/mem/LumexMemRead.hpp"

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
      GivenAnyStandard_WhenAlias_ThenTheOptionalOfTheLibrary)
{
  static_assert (
      std::is_same<optional_t<std::uint32_t>, lumex_optional>::value,
      "optional_t is the optional of the library in every standard");
  static_assert (
      std::is_same<optional_t<double>,
                   lumex::core::optional::opt::optional<double>>::value,
      "for every value type");
#if __cplusplus >= 201703L
  static_assert (!std::is_same<optional_t<std::uint32_t>,
                               std::optional<std::uint32_t>>::value,
                 "never std::optional, whatever the standard");
#endif
  SUCCEED ();
}

TEST (LumexMemReadResultTypeTest,
      GivenAnyStandard_WhenEveryOverload_ThenReturnsTheOptionalOfTheLibrary)
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
      GivenAnyStandard_WhenValueAndEmpty_ThenOptionalOfTheLibraryBehaves)
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
