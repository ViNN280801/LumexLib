// lumex/tests/core/unicode/convert/LumexUnicodeConvert.cxx11.tests.cpp
//
// to_utf8 and to_wide of convert/LumexUnicodeConvert.hpp. Expected values
// come from the reference encoders of LumexUnicodeTestSupport.hpp. A wchar_t
// is 2 bytes wide on Windows and 4 on Linux; the tests that depend on it
// branch on sizeof (wchar_t) and hold on both.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/unicode/convert/LumexUnicodeConvert.hpp"

#include "lumex/tests/core/unicode/LumexUnicodeTestSupport.hpp"

using lumex::core::unicode::convert::to_utf8;
using lumex::core::unicode::convert::to_wide;
using unicode_test::encode_utf16;
using unicode_test::encode_utf8;
using unicode_test::sequence;

namespace
{
std::string
utf8_string (std::vector<std::uint32_t> const &points)
{
  std::vector<std::uint8_t> const encoded = encode_utf8 (points);
  return std::string (encoded.begin (), encoded.end ());
}

// The wchar_t string the platform should hold for the code points.
std::wstring
wide_string (std::vector<std::uint32_t> const &points)
{
  std::wstring out;
  if (sizeof (wchar_t) == 2)
    {
      std::vector<std::uint16_t> const units = encode_utf16 (points);
      for (std::size_t i = 0; i < units.size (); ++i)
        out.push_back (static_cast<wchar_t> (units[i]));
    }
  else
    {
      for (std::size_t i = 0; i < points.size (); ++i)
        out.push_back (static_cast<wchar_t> (points[i]));
    }
  return out;
}

std::vector<std::uint32_t>
sample_points ()
{
  std::vector<std::uint32_t> points;
  points.push_back ('c');
  points.push_back ('a');
  points.push_back ('f');
  points.push_back (0xE9);
  points.push_back (' ');
  points.push_back (0x20AC);
  points.push_back (' ');
  points.push_back (0x1F600);
  return points;
}
} // namespace

TEST (UnicodeConvertTest, GivenAscii_WhenToWideAndBack_ThenTheTextIsTheSame)
{
  EXPECT_EQ (to_wide ("Hello, world"), std::wstring (L"Hello, world"));
  EXPECT_EQ (to_utf8 (L"Hello, world"), std::string ("Hello, world"));
}

TEST (UnicodeConvertTest,
      GivenEachUtf8Length_WhenToWide_ThenTheCodePointIsOneCharacterOrAPair)
{
  EXPECT_EQ (to_wide ("\xC3\xA9"),
             std::wstring (1, static_cast<wchar_t> (0xE9)));
  EXPECT_EQ (to_wide ("\xE2\x82\xAC"),
             std::wstring (1, static_cast<wchar_t> (0x20AC)));
  std::wstring const emoji = to_wide ("\xF0\x9F\x98\x80");
  EXPECT_EQ (emoji.size (), sizeof (wchar_t) == 2 ? 2U : 1U);
  EXPECT_EQ (emoji, wide_string (std::vector<std::uint32_t> (1, 0x1F600)));
}

TEST (UnicodeConvertTest,
      GivenEachUtf8Length_WhenToUtf8_ThenTheBytesAreTheReferenceEncoding)
{
  EXPECT_EQ (to_utf8 (std::wstring (1, static_cast<wchar_t> (0xE9))),
             std::string ("\xC3\xA9"));
  EXPECT_EQ (to_utf8 (std::wstring (1, static_cast<wchar_t> (0x20AC))),
             std::string ("\xE2\x82\xAC"));
  EXPECT_EQ (to_utf8 (wide_string (std::vector<std::uint32_t> (1, 0x1F600))),
             std::string ("\xF0\x9F\x98\x80"));
  EXPECT_EQ (to_utf8 (wide_string (std::vector<std::uint32_t> (1, 0x10FFFF))),
             std::string ("\xF4\x8F\xBF\xBF"));
}

TEST (UnicodeConvertTest,
      GivenMixedText_WhenToWide_ThenItMatchesThePlatformEncoding)
{
  std::vector<std::uint32_t> const points = sample_points ();
  EXPECT_EQ (to_wide (utf8_string (points)), wide_string (points));
  EXPECT_EQ (to_wide (utf8_string (points)).size (),
             sizeof (wchar_t) == 2 ? 9U : 8U);
}

TEST (UnicodeConvertTest,
      GivenMixedText_WhenToUtf8_ThenItMatchesTheReferenceEncoding)
{
  std::vector<std::uint32_t> const points = sample_points ();
  EXPECT_EQ (to_utf8 (wide_string (points)), utf8_string (points));
}

TEST (UnicodeConvertTest, GivenEmptyInput_WhenConverted_ThenTheResultIsEmpty)
{
  EXPECT_TRUE (to_utf8 (std::wstring ()).empty ());
  EXPECT_TRUE (to_utf8 (L"").empty ());
  EXPECT_TRUE (to_utf8 (static_cast<wchar_t const *> (nullptr), 0).empty ());
  EXPECT_TRUE (to_wide (std::string ()).empty ());
  EXPECT_TRUE (to_wide ("").empty ());
  EXPECT_TRUE (to_wide (static_cast<char const *> (nullptr), 0).empty ());
}

TEST (UnicodeConvertTest,
      GivenAPointerAndALength_WhenConverted_ThenOnlyThatManyUnitsAreRead)
{
  EXPECT_EQ (to_utf8 (L"abcdef", 3), std::string ("abc"));
  EXPECT_EQ (to_wide ("abcdef", 3), std::wstring (L"abc"));
  // The length cuts a multi-byte sequence: the incomplete tail is dropped.
  EXPECT_EQ (to_wide ("a\xE2\x82\xAC", 3), std::wstring (L"a"));
}

TEST (UnicodeConvertTest,
      GivenEmbeddedZeros_WhenConvertedWithALength_ThenTheyAreKept)
{
  std::string const bytes ("a\0b", 3);
  std::wstring const wide = to_wide (bytes);
  ASSERT_EQ (wide.size (), 3U);
  EXPECT_EQ (wide[0], L'a');
  EXPECT_EQ (wide[1], L'\0');
  EXPECT_EQ (wide[2], L'b');
  EXPECT_EQ (to_wide (bytes.c_str (), bytes.size ()), wide);
  EXPECT_EQ (to_utf8 (wide), bytes);
  EXPECT_EQ (to_utf8 (wide.c_str (), wide.size ()), bytes);
}

TEST (
    UnicodeConvertTest,
    GivenEmbeddedZeros_WhenConvertedAsAZeroTerminatedString_ThenTheTextStopsAtTheZero)
{
  std::string const bytes ("a\0b", 3);
  EXPECT_EQ (to_wide (bytes.c_str ()), std::wstring (L"a"));
  std::wstring const wide (L"a\0b", 3);
  EXPECT_EQ (to_utf8 (wide.c_str ()), std::string ("a"));
}

TEST (UnicodeConvertTest,
      GivenInvalidUtf8_WhenToWide_ThenWhatCanNotBeDecodedIsLeftOut)
{
  EXPECT_EQ (to_wide ("a\x80z"), std::wstring (L"az"));
  EXPECT_EQ (to_wide ("ab\xE2\x82"), std::wstring (L"ab"));
  EXPECT_EQ (to_wide ("\xFF\xFE"), std::wstring ());
  EXPECT_EQ (to_wide ("x\xF0\x9F\x98y"), std::wstring (L"xy"));
}

TEST (UnicodeConvertTest,
      GivenAnUnpairedHighSurrogate_WhenToUtf8_ThenItFollowsTheWidthOfWchar)
{
  std::wstring input (L"a");
  input.push_back (static_cast<wchar_t> (0xD800));
  input.push_back (L'b');
  if (sizeof (wchar_t) == 2)
    // UTF-16: the unpaired surrogate is skipped.
    EXPECT_EQ (to_utf8 (input), std::string ("ab"));
  else
    // UTF-32 units are not validated: U+D800 is written as three bytes.
    EXPECT_EQ (to_utf8 (input), std::string ("a\xED\xA0\x80"
                                             "b"));
}

TEST (UnicodeConvertTest, GivenRandomText_WhenRoundTripped_ThenTheTextSurvives)
{
  sequence generator (0x5EED);
  for (int round = 0; round < 100; ++round)
    {
      std::vector<std::uint32_t> const points = generator.code_points (120);
      std::string const utf8 = utf8_string (points);
      std::wstring const wide = to_wide (utf8);
      ASSERT_EQ (wide, wide_string (points)) << "round " << round;
      ASSERT_EQ (to_utf8 (wide), utf8) << "round " << round;
    }
}

TEST (UnicodeConvertTest,
      GivenAMillionCodePoints_WhenRoundTripped_ThenTheTextSurvives)
{
  sequence generator (0xB16);
  std::vector<std::uint32_t> const points = generator.code_points (1000000);
  std::string const utf8 = utf8_string (points);
  std::wstring const wide = to_wide (utf8);
  EXPECT_EQ (wide.size (), wide_string (points).size ());
  EXPECT_TRUE (to_utf8 (wide) == utf8);
}

TEST (UnicodeConvertTest,
      GivenAStringLiteral_WhenOverloadsResolved_ThenThePointerFormIsChosen)
{
  // A literal could also build a std::string: the pointer overload wins and
  // takes the length from the terminator.
  EXPECT_EQ (to_utf8 (L"abc").size (), 3U);
  EXPECT_EQ (to_wide ("abc").size (), 3U);
}
