// lumex/tests/core/unicode/utf/LumexUtf8Validate.cxx20.tests.cpp
//
// The char8_t overloads of utf8_valid_prefix_length and is_valid_utf8
// (utf/LumexUtf8Validate.hpp), which exist where the compiler has char8_t
// (C++20). They read the code units as bytes and give what the overloads for
// std::uint8_t and char give; the tests compare with both, on u8 literals and
// on arrays of char8_t that hold ill-formed sequences. The rest of the
// validation is tested in LumexUtf8Validate.cxx11.tests.cpp.
#include <cstddef>
#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/unicode/utf/LumexUtf8Validate.hpp"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"

using lumex::core::unicode::utf::is_valid_utf8;
using lumex::core::unicode::utf::utf8_valid_prefix_length;

#if LUMEX_HAS_CHAR8_T
TEST (Utf8ValidateChar8Test, GivenAU8Literal_WhenChecked_ThenItIsValid)
{
  EXPECT_TRUE (is_valid_utf8 (u8"x", 1));
  EXPECT_TRUE (is_valid_utf8 (u8"café € \U0001F600", 14));
  EXPECT_EQ (utf8_valid_prefix_length (u8"x", 1), 1U);
  EXPECT_EQ (utf8_valid_prefix_length (u8"é", 2), 2U);
}

TEST (Utf8ValidateChar8Test, GivenAnEmptyBuffer_WhenChecked_ThenItIsValid)
{
  EXPECT_TRUE (is_valid_utf8 (static_cast<char8_t const *> (nullptr), 0));
  EXPECT_EQ (
      utf8_valid_prefix_length (static_cast<char8_t const *> (nullptr), 0),
      0U);
  EXPECT_TRUE (is_valid_utf8 (u8"", 0));
}

TEST (Utf8ValidateChar8Test,
      GivenIllFormedCodeUnits_WhenChecked_ThenThePrefixEndsBeforeThem)
{
  char8_t const overlong[] = { 0x61, 0x62, 0xC0, 0xBC, 0x63 };
  char8_t const surrogate[] = { 0x61, 0x62, 0xED, 0xA0, 0x80, 0x63 };
  char8_t const above[] = { 0x61, 0x62, 0xF4, 0x90, 0x80, 0x80 };
  char8_t const truncated[] = { 0x61, 0x62, 0xE2, 0x82 };
  char8_t const stray[] = { 0x61, 0x62, 0x80 };
  EXPECT_EQ (utf8_valid_prefix_length (overlong, sizeof (overlong)), 2U);
  EXPECT_EQ (utf8_valid_prefix_length (surrogate, sizeof (surrogate)), 2U);
  EXPECT_EQ (utf8_valid_prefix_length (above, sizeof (above)), 2U);
  EXPECT_EQ (utf8_valid_prefix_length (truncated, sizeof (truncated)), 2U);
  EXPECT_EQ (utf8_valid_prefix_length (stray, sizeof (stray)), 2U);
  EXPECT_FALSE (is_valid_utf8 (overlong, sizeof (overlong)));
  EXPECT_FALSE (is_valid_utf8 (surrogate, sizeof (surrogate)));
  EXPECT_FALSE (is_valid_utf8 (above, sizeof (above)));
  EXPECT_FALSE (is_valid_utf8 (truncated, sizeof (truncated)));
  EXPECT_FALSE (is_valid_utf8 (stray, sizeof (stray)));
}

TEST (Utf8ValidateChar8Test,
      GivenTheSameBytes_WhenCheckedAsChar8CharAndBytes_ThenTheResultIsTheSame)
{
  // Every byte value alone and in the pairs that decide the ranges of the
  // table, through the three overloads.
  for (unsigned int first = 0; first < 256; ++first)
    for (unsigned int second = 0; second < 256; second += 7)
      {
        std::uint8_t const bytes[3]
            = { static_cast<std::uint8_t> (first),
                static_cast<std::uint8_t> (second), 0x80 };
        char8_t const units[3] = { static_cast<char8_t> (first),
                                   static_cast<char8_t> (second), 0x80 };
        char const chars[3]
            = { static_cast<char> (first), static_cast<char> (second),
                static_cast<char> (0x80) };
        for (std::size_t size = 0; size <= 3; ++size)
          {
            std::size_t const expected
                = utf8_valid_prefix_length (bytes, size);
            EXPECT_EQ (utf8_valid_prefix_length (units, size), expected)
                << first << " " << second << " size " << size;
            EXPECT_EQ (utf8_valid_prefix_length (chars, size), expected)
                << first << " " << second << " size " << size;
            EXPECT_EQ (is_valid_utf8 (units, size),
                       is_valid_utf8 (bytes, size))
                << first << " " << second << " size " << size;
          }
      }
}
#else
TEST (Utf8ValidateChar8Test, GivenNoChar8T_WhenBuilt_ThenTheOverloadsAreAbsent)
{
  GTEST_SKIP () << "the compiler has no char8_t";
}
#endif
