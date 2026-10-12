// LumexTextWideChars.cxx11.tests.cpp
//
// join and the character types other than char. The constraint on the
// elements is the stream trait (below C++20) or the concept Streamable (C++20
// ranges), and a narrow stream takes wchar_t, char16_t and char32_t as numbers
// up to C++17 and rejects them from C++20 in the libraries that delete those
// inserters (P1423R3). Every suite of the module compiles this file.
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/string/text/LumexJoin.hpp"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"

#include "lumex/tests/core/string/text/LumexTextConstraintsTestHelpers.hpp"
#include "lumex/tests/support/LumexOstreamProbe.hpp"

using lumex::core::string::text::join;
using lumex_tests_support::ostream_accepts;
using text_constraints_test_helpers::can_join;
using text_constraints_test_helpers::can_quote;

namespace
{
typedef char const separator_literal_t[3];
} // namespace

TEST (LumexTextWideCharsTest, GivenWideElements_ThenJoinFollowsTheStream)
{
  EXPECT_EQ ((can_join<std::vector<wchar_t>, separator_literal_t>::value),
             (ostream_accepts<wchar_t>::value));
  EXPECT_EQ ((can_join<std::vector<char16_t>, separator_literal_t>::value),
             (ostream_accepts<char16_t>::value));
  EXPECT_EQ ((can_join<std::vector<char32_t>, separator_literal_t>::value),
             (ostream_accepts<char32_t>::value));
  EXPECT_EQ ((can_join<wchar_t[3], separator_literal_t>::value),
             (ostream_accepts<wchar_t>::value));
  EXPECT_EQ (
      (can_join<std::vector<wchar_t const *>, separator_literal_t>::value),
      (ostream_accepts<wchar_t const *>::value));
}

TEST (LumexTextWideCharsTest, GivenWideStrings_ThenJoinRejected)
{
  EXPECT_FALSE (
      (can_join<std::vector<std::wstring>, separator_literal_t>::value));
  EXPECT_FALSE (
      (can_join<std::vector<std::u16string>, separator_literal_t>::value));
  EXPECT_FALSE (
      (can_join<std::vector<std::u32string>, separator_literal_t>::value));
}

TEST (LumexTextWideCharsTest, GivenWideElements_ThenQuoteRejected)
{
  // Quote takes elements that convert to std::string; a number is not one.
  EXPECT_FALSE ((can_quote<std::vector<wchar_t>, separator_literal_t>::value));
  EXPECT_FALSE (
      (can_quote<std::vector<std::wstring>, separator_literal_t>::value));
}

#if !LUMEX_HAS_STD_RANGES
// Below the ranges overloads the separator is any streamable type.
TEST (LumexTextWideCharsTest, GivenWideSeparator_ThenJoinFollowsTheStream)
{
  EXPECT_EQ ((can_join<std::vector<int>, wchar_t>::value),
             (ostream_accepts<wchar_t>::value));
  EXPECT_EQ ((can_join<std::vector<int>, char16_t>::value),
             (ostream_accepts<char16_t>::value));
  EXPECT_EQ ((can_join<std::vector<int>, std::wstring>::value), false);
}
#endif

#if __cplusplus < 202002L
TEST (LumexTextWideCharsTest, GivenCxx11To17_ThenWideCharactersJoinAsNumbers)
{
  std::vector<wchar_t> const wide = { L'A', L'B' };
  std::vector<char16_t> const utf16 = { u'A', u'B' };
  std::vector<char32_t> const utf32 = { U'A', U'B' };
  EXPECT_EQ (join (wide, ", "), "65, 66");
  EXPECT_EQ (join (utf16, ", "), "65, 66");
  EXPECT_EQ (join (utf32, "|"), "65|66");
}
#endif

#if LUMEX_TEST_OSTREAM_DELETES_WIDE_CHARACTERS
TEST (LumexTextWideCharsTest, GivenCxx20Library_ThenWideElementsAreRejected)
{
  EXPECT_FALSE ((can_join<std::vector<wchar_t>, separator_literal_t>::value));
  EXPECT_FALSE ((can_join<std::vector<char16_t>, separator_literal_t>::value));
  EXPECT_FALSE ((can_join<std::vector<char32_t>, separator_literal_t>::value));
  EXPECT_TRUE ((can_join<std::vector<char>, separator_literal_t>::value));
}
#endif
