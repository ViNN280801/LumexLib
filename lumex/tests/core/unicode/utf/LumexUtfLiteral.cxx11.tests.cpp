// LumexUtfLiteral.cxx11.tests.cpp
//
// The counting policies, the writing policies and the decoders of the module
// are stateless classes without a declared special member: at every standard
// of the build they are literal types, empty and trivially destructible, and
// a constexpr object of each exists (todo 108). The implicit special members
// must stay implicit: a user-declared one would make the policies a
// non-trivial type that the decoders copy by value.
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/unicode/LumexUnicode"

namespace
{
using namespace lumex::core::unicode::utf;

template <typename T> struct is_stateless_literal
{
  static constexpr bool value
      = std::is_empty<T>::value && std::is_trivially_destructible<T>::value
        && std::is_trivially_default_constructible<T>::value
        && std::is_trivially_copyable<T>::value;
};

constexpr utf8_counter k_utf8_counter{};
constexpr utf16_counter k_utf16_counter{};
constexpr utf32_counter k_utf32_counter{};
constexpr utf8_writer k_utf8_writer{};
constexpr utf16_writer k_utf16_writer{};
constexpr utf32_writer k_utf32_writer{};
constexpr latin1_writer k_latin1_writer{};
constexpr utf8_decoder k_utf8_decoder{};
constexpr utf16_decoder<false> k_utf16_decoder{};
constexpr utf16_decoder<true> k_utf16_swapped_decoder{};
constexpr utf32_decoder<false> k_utf32_decoder{};
constexpr utf32_decoder<true> k_utf32_swapped_decoder{};
constexpr latin1_decoder k_latin1_decoder{};
constexpr wchar_decoder k_wchar_decoder{};

static_assert (is_stateless_literal<utf8_counter>::value, "utf8_counter");
static_assert (is_stateless_literal<utf16_counter>::value, "utf16_counter");
static_assert (is_stateless_literal<utf32_counter>::value, "utf32_counter");
static_assert (is_stateless_literal<utf8_writer>::value, "utf8_writer");
static_assert (is_stateless_literal<utf16_writer>::value, "utf16_writer");
static_assert (is_stateless_literal<utf32_writer>::value, "utf32_writer");
static_assert (is_stateless_literal<latin1_writer>::value, "latin1_writer");
static_assert (is_stateless_literal<utf8_decoder>::value, "utf8_decoder");
static_assert (is_stateless_literal<utf16_decoder<false>>::value,
               "utf16_decoder");
static_assert (is_stateless_literal<utf16_decoder<true>>::value,
               "utf16_decoder, swapped");
static_assert (is_stateless_literal<utf32_decoder<false>>::value,
               "utf32_decoder");
static_assert (is_stateless_literal<utf32_decoder<true>>::value,
               "utf32_decoder, swapped");
static_assert (is_stateless_literal<latin1_decoder>::value, "latin1_decoder");
static_assert (is_stateless_literal<wchar_decoder>::value, "wchar_decoder");
} // namespace

TEST (LumexUtfLiteralTest, ConstantPolicies_AreUsableAtRunTime)
{
  EXPECT_EQ (utf8_counter::low (0, 0x41), 1U);
  EXPECT_EQ (utf8_counter::high (0, 0x10000), 4U);
  static_cast<void> (k_utf8_counter);
  static_cast<void> (k_utf16_counter);
  static_cast<void> (k_utf32_counter);
  static_cast<void> (k_utf8_writer);
  static_cast<void> (k_utf16_writer);
  static_cast<void> (k_utf32_writer);
  static_cast<void> (k_latin1_writer);
  static_cast<void> (k_utf8_decoder);
  static_cast<void> (k_utf16_decoder);
  static_cast<void> (k_utf16_swapped_decoder);
  static_cast<void> (k_utf32_decoder);
  static_cast<void> (k_utf32_swapped_decoder);
  static_cast<void> (k_latin1_decoder);
  static_cast<void> (k_wchar_decoder);
}
