// lumex/tests/core/unicode/convert/LumexUnicodeConvertWideWidths.cxx11.tests.cpp
//
// to_utf8 and to_wide pick their policies by sizeof (wchar_t): UTF-16 where
// wchar_t has 2 bytes (Windows) and UTF-32 where it has 4 (Linux). One test
// binary only ever runs one of the two, and the code that calls the functions
// (the path conversions of filesystem, the Windows sources of serial,
// hardware and resource_monitor) runs the 2-byte path only on Windows. These
// tests run both pipelines on every platform: they repeat the two passes of
// the functions (count, then write) with the policies of wchar_selector<2>
// and wchar_selector<4> over buffers of 16 and 32 bit units, so the UTF-16
// path that Windows takes is exercised on Linux too.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/unicode/convert/LumexUnicodeConvert.hpp"

#include "lumex/tests/core/unicode/LumexUnicodeTestSupport.hpp"

using lumex::core::unicode::utf::utf8_counter;
using lumex::core::unicode::utf::utf8_decoder;
using lumex::core::unicode::utf::utf8_writer;
using lumex::core::unicode::utf::wchar_selector;
using unicode_test::encode_utf16;
using unicode_test::encode_utf8;
using unicode_test::sequence;

namespace
{
// The two passes of to_utf8 for a wide unit of Size bytes.
template <std::size_t Size>
std::string
wide_to_utf8 (std::vector<typename wchar_selector<Size>::type> const &units)
{
  using decoder = typename wchar_selector<Size>::decoder;
  typename wchar_selector<Size>::type const *const data
      = units.empty () ? nullptr : &units[0];
  std::size_t const size
      = decoder::process (data, units.size (), 0, utf8_counter ());
  std::string out (size, '\0');
  if (size > 0)
    {
      std::uint8_t *const begin = reinterpret_cast<std::uint8_t *> (&out[0]);
      std::uint8_t *const end
          = decoder::process (data, units.size (), begin, utf8_writer ());
      EXPECT_EQ (begin + size, end);
    }
  return out;
}

// The two passes of to_wide for a wide unit of Size bytes.
template <std::size_t Size>
std::vector<typename wchar_selector<Size>::type>
utf8_to_wide (std::string const &text)
{
  using selector = wchar_selector<Size>;
  std::uint8_t const *const data
      = reinterpret_cast<std::uint8_t const *> (text.data ());
  std::size_t const count = utf8_decoder::process (
      data, text.size (), 0, typename selector::counter ());
  std::vector<typename selector::type> out (count);
  if (count > 0)
    {
      typename selector::type *const begin = &out[0];
      typename selector::type *const end = utf8_decoder::process (
          data, text.size (), begin, typename selector::writer ());
      EXPECT_EQ (begin + count, end);
    }
  return out;
}

std::string
utf8_of (std::vector<std::uint32_t> const &points)
{
  std::vector<std::uint8_t> const bytes = encode_utf8 (points);
  return std::string (bytes.begin (), bytes.end ());
}

std::vector<std::uint32_t>
sample_points ()
{
  std::vector<std::uint32_t> points;
  // Latin, Cyrillic, a 3-byte code point, and two 4-byte ones.
  points.push_back ('p');
  points.push_back (0x41F);
  points.push_back (0x440);
  points.push_back (0x20AC);
  points.push_back (0x1F600);
  points.push_back (0x10FFFF);
  points.push_back ('/');
  return points;
}
} // namespace

TEST (UnicodeConvertWideWidthsTest,
      GivenTwoByteUnits_WhenToUtf8_ThenSurrogatePairsBecomeFourBytes)
{
  std::vector<std::uint32_t> const points = sample_points ();
  std::vector<std::uint16_t> const units = encode_utf16 (points);
  // The two 4-byte code points take two units each.
  EXPECT_EQ (units.size (), points.size () + 2U);
  EXPECT_EQ (wide_to_utf8<2> (units), utf8_of (points));
}

TEST (UnicodeConvertWideWidthsTest,
      GivenFourByteUnits_WhenToUtf8_ThenOneUnitPerCodePoint)
{
  std::vector<std::uint32_t> const points = sample_points ();
  EXPECT_EQ (wide_to_utf8<4> (points), utf8_of (points));
}

TEST (UnicodeConvertWideWidthsTest,
      GivenUtf8_WhenToTwoByteUnits_ThenTheUtf16Encoding)
{
  std::vector<std::uint32_t> const points = sample_points ();
  EXPECT_EQ (utf8_to_wide<2> (utf8_of (points)), encode_utf16 (points));
}

TEST (UnicodeConvertWideWidthsTest,
      GivenUtf8_WhenToFourByteUnits_ThenTheCodePoints)
{
  std::vector<std::uint32_t> const points = sample_points ();
  EXPECT_EQ (utf8_to_wide<4> (utf8_of (points)), points);
}

TEST (UnicodeConvertWideWidthsTest,
      GivenUnpairedSurrogatesInUtf16_WhenToUtf8_ThenSkipped)
{
  // The case WideCharToMultiByte answers with U+FFFD: a lone high surrogate
  // (here before an ASCII letter and at the end), a lone low surrogate, and
  // a low surrogate before a high one (a reversed pair).
  std::vector<std::uint16_t> units;
  units.push_back ('a');
  units.push_back (0xD83D); // lone high
  units.push_back ('b');
  units.push_back (0xDE00); // lone low
  units.push_back ('c');
  units.push_back (0xDE00); // reversed pair
  units.push_back (0xD83D);
  units.push_back ('d');
  units.push_back (0xD83D); // a pair that is whole: kept
  units.push_back (0xDE00);
  units.push_back (0xD83D); // lone high at the end
  EXPECT_EQ (wide_to_utf8<2> (units), std::string ("abcd"
                                                   "\xF0\x9F\x98\x80"));
}

TEST (UnicodeConvertWideWidthsTest,
      GivenInvalidUtf8_WhenToEitherWidth_ThenTheSameUnitsAreKept)
{
  // The same input is skipped the same way for both widths: only the text
  // that survives is encoded differently.
  std::string const text ("a\x80z\xE2\x82"
                          "b\xFF"
                          "\xF0\x9F\x98\x80");
  std::vector<std::uint16_t> const narrow_units = utf8_to_wide<2> (text);
  std::vector<std::uint32_t> const wide_units = utf8_to_wide<4> (text);
  std::vector<std::uint32_t> expected;
  expected.push_back ('a');
  expected.push_back ('z');
  expected.push_back ('b');
  expected.push_back (0x1F600);
  EXPECT_EQ (wide_units, expected);
  EXPECT_EQ (narrow_units, encode_utf16 (expected));
}

TEST (UnicodeConvertWideWidthsTest,
      GivenRandomText_WhenRoundTrippedThroughEitherWidth_ThenTheTextSurvives)
{
  sequence generator (0xC0DE);
  for (int round = 0; round < 50; ++round)
    {
      std::vector<std::uint32_t> const points = generator.code_points (80);
      std::string const utf8 = utf8_of (points);
      ASSERT_EQ (wide_to_utf8<2> (utf8_to_wide<2> (utf8)), utf8)
          << "round " << round;
      ASSERT_EQ (wide_to_utf8<4> (utf8_to_wide<4> (utf8)), utf8)
          << "round " << round;
      ASSERT_EQ (utf8_to_wide<2> (utf8), encode_utf16 (points))
          << "round " << round;
    }
}

TEST (UnicodeConvertWideWidthsTest, GivenEmptyInput_WhenConverted_ThenEmpty)
{
  EXPECT_TRUE (wide_to_utf8<2> (std::vector<std::uint16_t> ()).empty ());
  EXPECT_TRUE (wide_to_utf8<4> (std::vector<std::uint32_t> ()).empty ());
  EXPECT_TRUE (utf8_to_wide<2> (std::string ()).empty ());
  EXPECT_TRUE (utf8_to_wide<4> (std::string ()).empty ());
}
