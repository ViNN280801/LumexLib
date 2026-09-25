// LumexFormatTypes.tests.cpp
// Built-in argument types other than floating point: integers of every
// width (plus __int128), bool, characters, strings, pointers, and the `?`
// debug presentation. Cases ported from fmt's format-test.cc (format_bool,
// format_short, format_int, format_bin, format_dec, format_hex, format_oct,
// format_char, format_unsigned_char, format_cstring, format_pointer,
// format_string, format_string_view, debug_presentation).
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#if __cplusplus >= 201703L
#include <string_view>
#endif

#include <gtest/gtest.h>

#include "lumex/core/fmt/LumexFormat.hpp"
#include "lumex/core/string_view/view/LumexStringView.hpp"
#include "lumex/tests/core/fmt/LumexFormatTestHelpers.hpp"

namespace fmt = lumex::core::fmt;

using format_test_helpers::format_error;
using format_test_helpers::runtime_format;

namespace
{
template <typename T>
std::string
printf_text (char const *pattern, T value)
{
  char buffer[128];
  std::snprintf (buffer, sizeof (buffer), pattern, value);
  return buffer;
}

/**
 * Every printable ASCII type character that is neither in `types` nor a
 * character with another meaning in the specification must be rejected.
 */
template <typename T>
void
expect_unknown_types_rejected (T const &value, char const *types,
                               char const *message)
{
  char const *const special = ".0123456789L?}{";
  for (int code = 0x21; code < 0x7f; ++code)
    {
      char const type = static_cast<char> (code);
      if (std::strchr (types, type) != nullptr
          || std::strchr (special, type) != nullptr)
        continue;
      std::string const text = std::string ("{0:10") + type + "}";
      EXPECT_EQ (format_error (text, value), message) << text;
    }
}

void const *
fake_pointer (std::uintptr_t value)
{
  return reinterpret_cast<void const *> (value);
}
} // namespace

// ---------------------------------------------------------------------------
// bool
// ---------------------------------------------------------------------------

TEST (LumexFormatTypesTest, GivenBool_WhenFormat_ThenTextOrNumber)
{
  EXPECT_EQ (fmt::format ("{}", true), "true");
  EXPECT_EQ (fmt::format ("{}", false), "false");
  EXPECT_EQ (fmt::format ("{:d}", true), "1");
  EXPECT_EQ (fmt::format ("{:d}", false), "0");
  EXPECT_EQ (fmt::format ("{:5}", true), "true ");
  EXPECT_EQ (fmt::format ("{:s}", true), "true");
  EXPECT_EQ (fmt::format ("{:s}", false), "false");
  EXPECT_EQ (fmt::format ("{:6s}", false), "false ");
  EXPECT_EQ (fmt::format ("{:#x}", true), "0x1");
  EXPECT_EQ (fmt::format ("{:+d}", true), "+1");
  EXPECT_EQ (fmt::format ("{:03d}", true), "001");
}

TEST (LumexFormatTypesTest, GivenBoolWithBadType_WhenFormat_ThenFormatError)
{
  EXPECT_EQ (format_error ("{:c}", false), "invalid format specifier");
  EXPECT_EQ (format_error ("{:f}", true), "invalid format specifier");
  EXPECT_EQ (format_error ("{:05}", true),
             "format specifier requires numeric argument");
}

// ---------------------------------------------------------------------------
// Integers
// ---------------------------------------------------------------------------

TEST (LumexFormatTypesTest, GivenShortTypes_WhenFormat_ThenDecimal)
{
  short const s = 42;
  EXPECT_EQ (fmt::format ("{0:d}", s), "42");
  unsigned short const us = 42;
  EXPECT_EQ (fmt::format ("{0:d}", us), "42");
  EXPECT_EQ (fmt::format ("{}", static_cast<short> (-32768)), "-32768");
}

TEST (LumexFormatTypesTest, GivenSmallCharTypes_WhenFormat_ThenNumbers)
{
  EXPECT_EQ (fmt::format ("{}", static_cast<unsigned char> (42)), "42");
  EXPECT_EQ (fmt::format ("{}", static_cast<std::uint8_t> (200)), "200");
  EXPECT_EQ (fmt::format ("{}", static_cast<signed char> (-5)), "-5");
  EXPECT_EQ (fmt::format ("{}", static_cast<std::int8_t> (-128)), "-128");
}

TEST (LumexFormatTypesTest, GivenIntWithUnknownType_WhenFormat_ThenFormatError)
{
  EXPECT_EQ (format_error ("{0:v", 42), "invalid format specifier");
  expect_unknown_types_rejected (42, "bBdoxXc", "invalid format specifier");
  expect_unknown_types_rejected (42u, "bBdoxXc", "invalid format specifier");
  expect_unknown_types_rejected (42ll, "bBdoxXc", "invalid format specifier");
}

TEST (LumexFormatTypesTest, GivenBinaryType_WhenFormat_ThenBaseTwo)
{
  EXPECT_EQ (fmt::format ("{0:b}", 0), "0");
  EXPECT_EQ (fmt::format ("{0:b}", 42), "101010");
  EXPECT_EQ (fmt::format ("{0:b}", 42u), "101010");
  EXPECT_EQ (fmt::format ("{0:b}", -42), "-101010");
  EXPECT_EQ (fmt::format ("{0:b}", 12345), "11000000111001");
  EXPECT_EQ (fmt::format ("{0:b}", 0x12345678),
             "10010001101000101011001111000");
  EXPECT_EQ (fmt::format ("{0:b}", 0x90ABCDEF),
             "10010000101010111100110111101111");
  EXPECT_EQ (
      fmt::format ("{0:b}", (std::numeric_limits<std::uint32_t>::max) ()),
      "11111111111111111111111111111111");
  EXPECT_EQ (fmt::format ("{0:B}", 5), "101");
}

TEST (LumexFormatTypesTest, GivenDecimalType_WhenFormat_ThenBaseTen)
{
  EXPECT_EQ (fmt::format ("{0}", 0), "0");
  EXPECT_EQ (fmt::format ("{0}", 42), "42");
  EXPECT_EQ (fmt::format ("{0:d}", 42), "42");
  EXPECT_EQ (fmt::format ("{0}", 42u), "42");
  EXPECT_EQ (fmt::format ("{0}", -42), "-42");
  EXPECT_EQ (fmt::format ("{0}", 12345), "12345");
  EXPECT_EQ (fmt::format ("{0}", 67890), "67890");
}

TEST (LumexFormatTypesTest, GivenIntegerLimits_WhenFormat_ThenSameAsPrintf)
{
  EXPECT_EQ (fmt::format ("{0}", INT_MIN), printf_text ("%d", INT_MIN));
  EXPECT_EQ (fmt::format ("{0}", INT_MAX), printf_text ("%d", INT_MAX));
  EXPECT_EQ (fmt::format ("{0}", UINT_MAX), printf_text ("%u", UINT_MAX));
  EXPECT_EQ (fmt::format ("{0}", LONG_MIN), printf_text ("%ld", LONG_MIN));
  EXPECT_EQ (fmt::format ("{0}", LONG_MAX), printf_text ("%ld", LONG_MAX));
  EXPECT_EQ (fmt::format ("{0}", ULONG_MAX), printf_text ("%lu", ULONG_MAX));
  EXPECT_EQ (fmt::format ("{0}", LLONG_MIN), printf_text ("%lld", LLONG_MIN));
  EXPECT_EQ (fmt::format ("{0}", LLONG_MAX), printf_text ("%lld", LLONG_MAX));
  EXPECT_EQ (fmt::format ("{0}", ULLONG_MAX),
             printf_text ("%llu", ULLONG_MAX));
}

TEST (LumexFormatTypesTest, GivenHexType_WhenFormat_ThenBaseSixteen)
{
  EXPECT_EQ (fmt::format ("{0:x}", 0), "0");
  EXPECT_EQ (fmt::format ("{0:x}", 0x42), "42");
  EXPECT_EQ (fmt::format ("{0:x}", 0x42u), "42");
  EXPECT_EQ (fmt::format ("{0:x}", -0x42), "-42");
  EXPECT_EQ (fmt::format ("{0:x}", 0x12345678), "12345678");
  EXPECT_EQ (fmt::format ("{0:x}", 0x90abcdef), "90abcdef");
  EXPECT_EQ (fmt::format ("{0:X}", 0x12345678), "12345678");
  EXPECT_EQ (fmt::format ("{0:X}", 0x90ABCDEF), "90ABCDEF");
  EXPECT_EQ (fmt::format ("{0:x}", INT_MIN),
             "-" + printf_text ("%x", 0 - static_cast<unsigned> (INT_MIN)));
  EXPECT_EQ (fmt::format ("{0:x}", INT_MAX), printf_text ("%x", INT_MAX));
  EXPECT_EQ (fmt::format ("{0:x}", UINT_MAX), printf_text ("%x", UINT_MAX));
  EXPECT_EQ (
      fmt::format ("{0:x}", LONG_MIN),
      "-" + printf_text ("%lx", 0 - static_cast<unsigned long> (LONG_MIN)));
  EXPECT_EQ (fmt::format ("{0:x}", LONG_MAX), printf_text ("%lx", LONG_MAX));
  EXPECT_EQ (fmt::format ("{0:x}", ULONG_MAX), printf_text ("%lx", ULONG_MAX));
}

TEST (LumexFormatTypesTest, GivenOctalType_WhenFormat_ThenBaseEight)
{
  EXPECT_EQ (fmt::format ("{0:o}", 0), "0");
  EXPECT_EQ (fmt::format ("{0:o}", 042), "42");
  EXPECT_EQ (fmt::format ("{0:o}", 042u), "42");
  EXPECT_EQ (fmt::format ("{0:o}", -042), "-42");
  EXPECT_EQ (fmt::format ("{0:o}", 012345670), "12345670");
  EXPECT_EQ (fmt::format ("{0:o}", INT_MIN),
             "-" + printf_text ("%o", 0 - static_cast<unsigned> (INT_MIN)));
  EXPECT_EQ (fmt::format ("{0:o}", INT_MAX), printf_text ("%o", INT_MAX));
  EXPECT_EQ (fmt::format ("{0:o}", UINT_MAX), printf_text ("%o", UINT_MAX));
  EXPECT_EQ (fmt::format ("{0:o}", LONG_MAX), printf_text ("%lo", LONG_MAX));
  EXPECT_EQ (fmt::format ("{0:o}", ULONG_MAX), printf_text ("%lo", ULONG_MAX));
}

TEST (LumexFormatTypesTest, GivenCharacterType_WhenFormatInteger_ThenCodeUnit)
{
  EXPECT_EQ (fmt::format ("{:c}", static_cast<int> ('x')), "x");
  EXPECT_EQ (fmt::format ("{:c}", 65), "A");
  EXPECT_EQ (fmt::format ("{:>3c}", 65u), "  A");
  // The value must fit the character type (signed or unsigned char per
  // platform, as std::format checks it).
  EXPECT_EQ (format_error ("{:c}", 256), "character out of range");
  if (std::numeric_limits<char>::is_signed)
    {
      EXPECT_EQ (runtime_format ("{:c}", -1), std::string (1, '\xff'));
      EXPECT_EQ (format_error ("{:c}", 128), "character out of range");
      EXPECT_EQ (format_error ("{:c}", -129), "character out of range");
    }
  else
    {
      EXPECT_EQ (format_error ("{:c}", -1), "character out of range");
      EXPECT_EQ (runtime_format ("{:c}", 255), std::string (1, '\xff'));
    }
}

TEST (LumexFormatTypesTest, GivenLocalizedInteger_WhenClassicLocale_ThenPlain)
{
  EXPECT_EQ (fmt::format ("{:L}", 1234), "1234");
  EXPECT_EQ (fmt::format ("{:L}", 1234567), "1234567");
  EXPECT_EQ (fmt::format ("{:Lx}", 255), "ff");
}

#if LUMEX_FORMAT_HAS_INT128
TEST (LumexFormatTypesTest, GivenInt128_WhenFormat_ThenAllDigits)
{
  __int128 const int128_max = static_cast<__int128> (
      (static_cast<unsigned __int128> (1) << 127) - 1);
  __int128 const int128_min = -int128_max - 1;
  unsigned __int128 const uint128_max = ~static_cast<unsigned __int128> (0);

  EXPECT_EQ (fmt::format ("{0}", static_cast<__int128> (0)), "0");
  EXPECT_EQ (fmt::format ("{0}", static_cast<unsigned __int128> (0)), "0");
  EXPECT_EQ (fmt::format ("{0}", static_cast<__int128> (INT64_MAX) + 1),
             "9223372036854775808");
  EXPECT_EQ (fmt::format ("{0}", static_cast<__int128> (INT64_MIN) - 1),
             "-9223372036854775809");
  EXPECT_EQ (fmt::format ("{0}", static_cast<__int128> (UINT64_MAX) + 1),
             "18446744073709551616");
  EXPECT_EQ (fmt::format ("{0}", int128_max),
             "170141183460469231731687303715884105727");
  EXPECT_EQ (fmt::format ("{0}", int128_min),
             "-170141183460469231731687303715884105728");
  EXPECT_EQ (fmt::format ("{0}", uint128_max),
             "340282366920938463463374607431768211455");
  EXPECT_EQ (fmt::format ("{0:x}", int128_max),
             "7fffffffffffffffffffffffffffffff");
  EXPECT_EQ (fmt::format ("{0:x}", int128_min),
             "-80000000000000000000000000000000");
  EXPECT_EQ (fmt::format ("{0:x}", uint128_max),
             "ffffffffffffffffffffffffffffffff");
  EXPECT_EQ (fmt::format ("{0:o}", int128_max),
             "1777777777777777777777777777777777777777777");
  EXPECT_EQ (fmt::format ("{0:o}", int128_min),
             "-2000000000000000000000000000000000000000000");
  EXPECT_EQ (fmt::format ("{0:+}", static_cast<__int128> (42)), "+42");
}
#endif

// ---------------------------------------------------------------------------
// Characters
// ---------------------------------------------------------------------------

TEST (LumexFormatTypesTest, GivenChar_WhenFormat_ThenCharacterByDefault)
{
  EXPECT_EQ (fmt::format ("{0}", 'a'), "a");
  EXPECT_EQ (fmt::format ("{0:c}", 'z'), "z");
  EXPECT_EQ (fmt::format ("{}", '\n'), "\n");
  EXPECT_EQ (fmt::format ("{:x}", '\xff'), "ff");
  EXPECT_EQ (fmt::format ("{:d}", 'A'), "65");
}

TEST (LumexFormatTypesTest, GivenCharWithIntegerTypes_WhenFormat_ThenAsInt)
{
  int const n = 'x';
  char const *const types[] = { "{:b}", "{:B}",  "{:d}",  "{:o}",  "{:x}",
                                "{:X}", "{:#x}", "{:+d}", "{:04d}" };
  for (char const *type : types)
    EXPECT_EQ (runtime_format (type, n), runtime_format (type, 'x')) << type;
  EXPECT_EQ (fmt::format ("{:02X}", n), fmt::format ("{:02X}", 'x'));
}

TEST (LumexFormatTypesTest, GivenCharWithUnknownType_WhenFormat_ThenError)
{
  expect_unknown_types_rejected ('a', "cbBdoxX", "invalid format specifier");
}

TEST (LumexFormatTypesTest, GivenCharDebug_WhenFormat_ThenQuotedEscaped)
{
  EXPECT_EQ (fmt::format ("{:?}", 'a'), "'a'");
  EXPECT_EQ (fmt::format ("{:?}", '\n'), "'\\n'");
  EXPECT_EQ (fmt::format ("{:?}", '\t'), "'\\t'");
  EXPECT_EQ (fmt::format ("{:?}", '\''), "'\\''");
  EXPECT_EQ (fmt::format ("{:?}", '"'), "'\"'");
  EXPECT_EQ (fmt::format ("{:?}", '\\'), "'\\\\'");
  EXPECT_EQ (fmt::format ("{:?}", '\x01'), "'\\u{1}'");
}

// ---------------------------------------------------------------------------
// Strings
// ---------------------------------------------------------------------------

TEST (LumexFormatTypesTest, GivenCString_WhenFormat_ThenText)
{
  EXPECT_EQ (fmt::format ("{0}", "test"), "test");
  EXPECT_EQ (fmt::format ("{0:s}", "test"), "test");
  char nonconst[] = "nonconst";
  EXPECT_EQ (fmt::format ("{0}", nonconst), "nonconst");
  char const *const pointer = "pointer";
  EXPECT_EQ (fmt::format ("{0}", pointer), "pointer");
}

TEST (LumexFormatTypesTest, GivenNullCString_WhenFormat_ThenFormatError)
{
  char const *const null_string = nullptr;
  EXPECT_EQ (format_error ("{}", null_string), "string pointer is null");
  EXPECT_EQ (format_error ("{:s}", null_string), "string pointer is null");
}

TEST (LumexFormatTypesTest, GivenStringWithUnknownType_WhenFormat_ThenError)
{
  expect_unknown_types_rejected ("test", "s", "invalid format specifier");
  EXPECT_EQ (format_error ("{:x}", std::string ("test")),
             "invalid format specifier");
  EXPECT_EQ (format_error ("{:d}", "forty-two"), "invalid format specifier");
}

TEST (LumexFormatTypesTest, GivenStdString_WhenFormat_ThenText)
{
  EXPECT_EQ (fmt::format ("{0}", std::string ("test")), "test");
  EXPECT_EQ (fmt::format ("{}", std::string ()), "");
  EXPECT_EQ (fmt::format ("{}", std::string ("a\0b", 3)),
             std::string ("a\0b", 3));
}

TEST (LumexFormatTypesTest, GivenLumexStringView_WhenFormat_ThenText)
{
  using lumex::core::string_view::view::LumexStringView;
  EXPECT_EQ (fmt::format ("{}", LumexStringView ("test")), "test");
  EXPECT_EQ (fmt::format ("{:>6}", LumexStringView ("test")), "  test");
  EXPECT_EQ (fmt::format ("{}", LumexStringView ("test", 2)), "te");
  EXPECT_EQ (fmt::format ("{}", LumexStringView ()), "");
  EXPECT_EQ (fmt::format ("{:?}", LumexStringView ("t\nst")), "\"t\\nst\"");
}

#if __cplusplus >= 201703L
TEST (LumexFormatTypesTest, GivenStdStringView_WhenFormat_ThenText)
{
  EXPECT_EQ (fmt::format ("{}", std::string_view ("test")), "test");
  EXPECT_EQ (fmt::format ("{:?}", std::string_view ("t\nst")), "\"t\\nst\"");
  EXPECT_EQ (fmt::format ("{}", std::string_view ()), "");
  EXPECT_EQ (fmt::format ("{:*^8}", std::string_view ("mid")), "**mid***");
}
#endif

TEST (LumexFormatTypesTest, GivenStringDebug_WhenFormat_ThenQuotedEscaped)
{
  EXPECT_EQ (fmt::format ("{:?}", ""), "\"\"");
  EXPECT_EQ (fmt::format ("{:?}", std::string ("test")), "\"test\"");
  EXPECT_EQ (fmt::format ("{:*^10?}", std::string ("test")), "**\"test\"**");
  EXPECT_EQ (fmt::format ("{:?}", std::string ("\test")), "\"\\test\"");
  EXPECT_EQ (fmt::format ("{:?}", std::string ("a\tb")), "\"a\\tb\"");
  EXPECT_EQ (fmt::format ("{:?}", "q\"'\\"), "\"q\\\"'\\\\\"");
  EXPECT_EQ (fmt::format ("{:?}", "\r\n"), "\"\\r\\n\"");
}

TEST (LumexFormatTypesTest,
      GivenDebugWithPrecision_WhenFormat_ThenCutByColumns)
{
  EXPECT_EQ (fmt::format ("{:*<5.0?}", "\n"), "*****");
  EXPECT_EQ (fmt::format ("{:*<5.1?}", "\n"), "\"****");
  EXPECT_EQ (fmt::format ("{:*<5.2?}", "\n"), "\"\\***");
  EXPECT_EQ (fmt::format ("{:*<5.3?}", "\n"), "\"\\n**");
  EXPECT_EQ (fmt::format ("{:*<5.4?}", "\n"), "\"\\n\"*");

  // U+03A3 (one column).
  EXPECT_EQ (fmt::format ("{:*<5.1?}", "\xCE\xA3"), "\"****");
  EXPECT_EQ (fmt::format ("{:*<5.2?}", "\xCE\xA3"), "\"\xCE\xA3***");
  EXPECT_EQ (fmt::format ("{:*<5.3?}", "\xCE\xA3"), "\"\xCE\xA3\"**");

  // U+7B11 (two columns).
  EXPECT_EQ (fmt::format ("{:*<5.1?}", "\xE7\xAC\x91"), "\"****");
  EXPECT_EQ (fmt::format ("{:*<5.2?}", "\xE7\xAC\x91"), "\"****");
  EXPECT_EQ (fmt::format ("{:*<5.3?}", "\xE7\xAC\x91"), "\"\xE7\xAC\x91**");
  EXPECT_EQ (fmt::format ("{:*<5.4?}", "\xE7\xAC\x91"), "\"\xE7\xAC\x91\"*");
}

TEST (LumexFormatTypesTest, GivenDebugUnicode_WhenPadded_ThenColumnsCounted)
{
  // Two Cyrillic words, one column per letter.
  EXPECT_EQ (fmt::format ("{:*<8?}", "\xD1\x82\xD1\x83\xD0\xB4\xD0\xB0"),
             "\"\xD1\x82\xD1\x83\xD0\xB4\xD0\xB0\"**");
  EXPECT_EQ (fmt::format ("{:*>8?}", "\xD1\x81\xD1\x8E\xD0\xB4\xD0\xB0"),
             "**\"\xD1\x81\xD1\x8E\xD0\xB4\xD0\xB0\"");
  // U+4E2D U+5FC3: two columns each.
  EXPECT_EQ (fmt::format ("{:*^8?}", "\xE4\xB8\xAD\xE5\xBF\x83"),
             "*\"\xE4\xB8\xAD\xE5\xBF\x83\"*");
}

TEST (LumexFormatTypesTest, GivenDebugInvalidUtf8_WhenFormat_ThenHexEscaped)
{
  EXPECT_EQ (fmt::format ("{:?}", "a\xff"), "\"a\\x{ff}\"");
}

// ---------------------------------------------------------------------------
// Pointers
// ---------------------------------------------------------------------------

TEST (LumexFormatTypesTest, GivenPointer_WhenFormat_ThenHexAddress)
{
  EXPECT_EQ (fmt::format ("{0}", static_cast<void *> (nullptr)), "0x0");
  EXPECT_EQ (fmt::format ("{0}", fake_pointer (0x1234)), "0x1234");
  EXPECT_EQ (fmt::format ("{0:p}", fake_pointer (0x1234)), "0x1234");
  EXPECT_EQ (fmt::format ("{0:P}", fake_pointer (0xabcd)), "0XABCD");
  EXPECT_EQ (
      fmt::format ("{0}", reinterpret_cast<void *> (~std::uintptr_t ())),
      "0x" + std::string (sizeof (void *) * CHAR_BIT / 4, 'f'));
  EXPECT_EQ (fmt::format ("{}", nullptr), "0x0");
  EXPECT_EQ (fmt::format ("{:>5}", nullptr), "  0x0");
}

TEST (LumexFormatTypesTest, GivenPointerWithUnknownType_WhenFormat_ThenError)
{
  expect_unknown_types_rejected (fake_pointer (0x1234), "pP",
                                 "invalid format specifier");
}

// ---------------------------------------------------------------------------
// Mixed arguments
// ---------------------------------------------------------------------------

TEST (LumexFormatTypesTest, GivenSpeedTestString_WhenFormat_ThenAllFieldsRight)
{
  EXPECT_EQ (fmt::format ("{0:0.10f}:{1:04}:{2:+g}:{3}:{4}:{5}:%", 1.234, 42,
                          3.13, "str", fake_pointer (1000), 'X'),
             "1.2340000000:0042:+3.13:str:0x3e8:X:%");
}
