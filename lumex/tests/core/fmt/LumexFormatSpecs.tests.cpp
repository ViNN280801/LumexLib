// LumexFormatSpecs.tests.cpp
// The standard specification [[fill]align][sign][#][0][width][.precision]
// [L][type]: alignment, fill, sign, alternate form, zero padding, static
// and dynamic width / precision, and their errors. Cases ported from fmt's
// format-test.cc (left/right/center_align, fill, plus/minus/space_sign,
// hash_flag, zero_flag, zero_flag_and_align, width, runtime_width,
// precision, runtime_precision). Where fmt and std::format disagree the
// std::format behaviour is expected (sign on unsigned integers is valid).
#include <climits>
#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/fmt/LumexFormat.hpp"
#include "lumex/tests/core/fmt/LumexFormatTestHelpers.hpp"

namespace fmt = lumex::core::fmt;

using format_test_helpers::format_error;
using format_test_helpers::runtime_format;

namespace
{
void const *
fake_pointer (std::uintptr_t value)
{
  return reinterpret_cast<void const *> (value);
}
} // namespace

TEST (LumexFormatSpecsTest, GivenLeftAlign_WhenFormat_ThenPaddedRight)
{
  EXPECT_EQ (fmt::format ("{0:<4}", 42), "42  ");
  EXPECT_EQ (fmt::format ("{0:<4o}", 042), "42  ");
  EXPECT_EQ (fmt::format ("{0:<4x}", 0x42), "42  ");
  EXPECT_EQ (fmt::format ("{0:<5}", -42), "-42  ");
  EXPECT_EQ (fmt::format ("{0:<5}", 42u), "42   ");
  EXPECT_EQ (fmt::format ("{0:<5}", -42l), "-42  ");
  EXPECT_EQ (fmt::format ("{0:<5}", 42ul), "42   ");
  EXPECT_EQ (fmt::format ("{0:<5}", -42ll), "-42  ");
  EXPECT_EQ (fmt::format ("{0:<5}", 42ull), "42   ");
  EXPECT_EQ (fmt::format ("{0:<5}", -42.0), "-42  ");
  EXPECT_EQ (fmt::format ("{0:<5}", -42.0l), "-42  ");
  EXPECT_EQ (fmt::format ("{0:<5}", 'c'), "c    ");
  EXPECT_EQ (fmt::format ("{0:<5}", "abc"), "abc  ");
  EXPECT_EQ (fmt::format ("{0:<8}", fake_pointer (0xface)), "0xface  ");
  EXPECT_EQ (fmt::format ("{:<30}", "left aligned"),
             "left aligned                  ");
}

TEST (LumexFormatSpecsTest, GivenRightAlign_WhenFormat_ThenPaddedLeft)
{
  EXPECT_EQ (fmt::format ("{0:>4}", 42), "  42");
  EXPECT_EQ (fmt::format ("{0:>4o}", 042), "  42");
  EXPECT_EQ (fmt::format ("{0:>4x}", 0x42), "  42");
  EXPECT_EQ (fmt::format ("{0:>5}", -42), "  -42");
  EXPECT_EQ (fmt::format ("{0:>5}", 42u), "   42");
  EXPECT_EQ (fmt::format ("{0:>5}", -42l), "  -42");
  EXPECT_EQ (fmt::format ("{0:>5}", 42ul), "   42");
  EXPECT_EQ (fmt::format ("{0:>5}", -42ll), "  -42");
  EXPECT_EQ (fmt::format ("{0:>5}", 42ull), "   42");
  EXPECT_EQ (fmt::format ("{0:>5}", -42.0), "  -42");
  EXPECT_EQ (fmt::format ("{0:>5}", -42.0l), "  -42");
  EXPECT_EQ (fmt::format ("{0:>5}", 'c'), "    c");
  EXPECT_EQ (fmt::format ("{0:>5}", "abc"), "  abc");
  EXPECT_EQ (fmt::format ("{0:>8}", fake_pointer (0xface)), "  0xface");
  EXPECT_EQ (fmt::format ("{:>30}", "right aligned"),
             "                 right aligned");
}

TEST (LumexFormatSpecsTest, GivenCenterAlign_WhenFormat_ThenExtraFillRight)
{
  EXPECT_EQ (fmt::format ("{0:^5}", 42), " 42  ");
  EXPECT_EQ (fmt::format ("{0:^5o}", 042), " 42  ");
  EXPECT_EQ (fmt::format ("{0:^5x}", 0x42), " 42  ");
  EXPECT_EQ (fmt::format ("{0:^5}", -42), " -42 ");
  EXPECT_EQ (fmt::format ("{0:^5}", 42u), " 42  ");
  EXPECT_EQ (fmt::format ("{0:^5}", -42l), " -42 ");
  EXPECT_EQ (fmt::format ("{0:^5}", 42ul), " 42  ");
  EXPECT_EQ (fmt::format ("{0:^5}", -42ll), " -42 ");
  EXPECT_EQ (fmt::format ("{0:^5}", 42ull), " 42  ");
  EXPECT_EQ (fmt::format ("{0:^5}", -42.0), " -42 ");
  EXPECT_EQ (fmt::format ("{0:^5}", -42.0l), " -42 ");
  EXPECT_EQ (fmt::format ("{0:^5}", 'c'), "  c  ");
  EXPECT_EQ (fmt::format ("{0:^6}", "abc"), " abc  ");
  EXPECT_EQ (fmt::format ("{0:^8}", fake_pointer (0xface)), " 0xface ");
  EXPECT_EQ (fmt::format ("{:^30}", "centered"),
             "           centered           ");
}

TEST (LumexFormatSpecsTest, GivenDefaultAlign_WhenFormat_ThenByKind)
{
  // Numbers align right, text and characters align left by default.
  EXPECT_EQ (fmt::format ("{:4}", 7), "   7");
  EXPECT_EQ (fmt::format ("{:4}", 1.5), " 1.5");
  EXPECT_EQ (fmt::format ("{:4}", "ab"), "ab  ");
  EXPECT_EQ (fmt::format ("{:4}", 'a'), "a   ");
  EXPECT_EQ (fmt::format ("{:6}", true), "true  ");
  EXPECT_EQ (fmt::format ("{:4d}", 'a'), "  97");
  EXPECT_EQ (fmt::format ("{:4d}", true), "   1");
}

TEST (LumexFormatSpecsTest, GivenFillCharacter_WhenFormat_ThenUsedForPadding)
{
  EXPECT_EQ (fmt::format ("{0:*>4}", 42), "**42");
  EXPECT_EQ (fmt::format ("{0:*>5}", -42), "**-42");
  EXPECT_EQ (fmt::format ("{0:*>5}", 42u), "***42");
  EXPECT_EQ (fmt::format ("{0:*>5}", -42l), "**-42");
  EXPECT_EQ (fmt::format ("{0:*>5}", 42ul), "***42");
  EXPECT_EQ (fmt::format ("{0:*>5}", -42ll), "**-42");
  EXPECT_EQ (fmt::format ("{0:*>5}", 42ull), "***42");
  EXPECT_EQ (fmt::format ("{0:*>5}", -42.0), "**-42");
  EXPECT_EQ (fmt::format ("{0:*>5}", -42.0l), "**-42");
  EXPECT_EQ (fmt::format ("{0:*<5}", 'c'), "c****");
  EXPECT_EQ (fmt::format ("{0:*<5}", "abc"), "abc**");
  EXPECT_EQ (fmt::format ("{0:*>8}", fake_pointer (0xface)), "**0xface");
  EXPECT_EQ (fmt::format ("{:*^30}", "centered"),
             "***********centered***********");
  EXPECT_EQ (fmt::format ("{:*^7}", "ab"), "**ab***");
}

TEST (LumexFormatSpecsTest, GivenAlignCharAsFill_WhenFormat_ThenUsedAsFill)
{
  EXPECT_EQ (fmt::format ("{:<<4}", 1), "1<<<");
  EXPECT_EQ (fmt::format ("{:>>4}", 1), ">>>1");
  EXPECT_EQ (fmt::format ("{:^^5}", 1), "^^1^^");
  EXPECT_EQ (fmt::format ("{:0<4}", 1), "1000");
  EXPECT_EQ (fmt::format ("{:+>4}", 1), "+++1");
}

TEST (LumexFormatSpecsTest,
      GivenMultiByteFill_WhenFormat_ThenCodePointRepeated)
{
  // U+0436 (Cyrillic zhe), two UTF-8 bytes.
  EXPECT_EQ (fmt::format ("{0:\xD0\xB6>4}", 42), "\xD0\xB6\xD0\xB6"
                                                 "42");
  // U+1F921 (clown face), four UTF-8 bytes.
  EXPECT_EQ (fmt::format ("{:\xF0\x9F\xA4\xA1^5}", 'x'),
             "\xF0\x9F\xA4\xA1\xF0\x9F\xA4\xA1x\xF0\x9F\xA4\xA1"
             "\xF0\x9F\xA4\xA1");
}

TEST (LumexFormatSpecsTest, GivenNulFill_WhenFormat_ThenNulPadding)
{
  std::string const text ("{:\0>4}", 6);
  EXPECT_EQ (fmt::vformat (text, fmt::make_format_args ('*')),
             std::string ("\0\0\0*", 4));
}

TEST (LumexFormatSpecsTest, GivenBadFill_WhenFormat_ThenFormatError)
{
  EXPECT_EQ (format_error ("{0:{<5}", 'c'), "invalid fill character '{'");
  EXPECT_EQ (format_error ("{0:{<5}}", 'c'), "invalid fill character '{'");
  EXPECT_EQ (format_error ("{:\x80\x80\x80\x80\x80>}", 0),
             "invalid format specifier");
}

TEST (LumexFormatSpecsTest, GivenPlusSign_WhenFormat_ThenSignAlwaysShown)
{
  EXPECT_EQ (fmt::format ("{0:+}", 42), "+42");
  EXPECT_EQ (fmt::format ("{0:+}", -42), "-42");
  EXPECT_EQ (fmt::format ("{0:+}", 42l), "+42");
  EXPECT_EQ (fmt::format ("{0:+}", 42ll), "+42");
  EXPECT_EQ (fmt::format ("{0:+}", 42.0), "+42");
  EXPECT_EQ (fmt::format ("{0:+}", 42.0l), "+42");
  EXPECT_EQ (fmt::format ("{0:+}", 0), "+0");
  EXPECT_EQ (fmt::format ("{0:+}", -0.0), "-0");
  EXPECT_EQ (fmt::format ("{:+f}; {:+f}", 3.14, -3.14),
             "+3.140000; -3.140000");
}

TEST (LumexFormatSpecsTest, GivenSignOnUnsigned_WhenFormat_ThenAcceptedAsStd)
{
  // fmt rejects a sign for unsigned integers; std::format accepts it.
  EXPECT_EQ (fmt::format ("{0:+}", 42u), "+42");
  EXPECT_EQ (fmt::format ("{0:+}", 42ul), "+42");
  EXPECT_EQ (fmt::format ("{0:+}", 42ull), "+42");
  EXPECT_EQ (fmt::format ("{0:-}", 42u), "42");
  EXPECT_EQ (fmt::format ("{0: }", 42u), " 42");
  EXPECT_EQ (fmt::format ("{0:+x}", 255u), "+ff");
}

TEST (LumexFormatSpecsTest, GivenMinusSign_WhenFormat_ThenOnlyNegativeSigned)
{
  EXPECT_EQ (fmt::format ("{0:-}", 42), "42");
  EXPECT_EQ (fmt::format ("{0:-}", -42), "-42");
  EXPECT_EQ (fmt::format ("{0:-}", 42l), "42");
  EXPECT_EQ (fmt::format ("{0:-}", 42ll), "42");
  EXPECT_EQ (fmt::format ("{0:-}", 42.0), "42");
  EXPECT_EQ (fmt::format ("{0:-}", 42.0l), "42");
  EXPECT_EQ (fmt::format ("{:-f}; {:-f}", 3.14, -3.14), "3.140000; -3.140000");
}

TEST (LumexFormatSpecsTest, GivenSpaceSign_WhenFormat_ThenSpaceForPositive)
{
  EXPECT_EQ (fmt::format ("{0: }", 42), " 42");
  EXPECT_EQ (fmt::format ("{0: }", -42), "-42");
  EXPECT_EQ (fmt::format ("{0: }", 42l), " 42");
  EXPECT_EQ (fmt::format ("{0: }", 42ll), " 42");
  EXPECT_EQ (fmt::format ("{0: }", 42.0), " 42");
  EXPECT_EQ (fmt::format ("{0: }", 42.0l), " 42");
  EXPECT_EQ (fmt::format ("{: f}; {: f}", 3.14, -3.14),
             " 3.140000; -3.140000");
}

TEST (LumexFormatSpecsTest, GivenSignOnNonNumeric_WhenFormat_ThenFormatError)
{
  char const *const specs[] = { "{0:+}", "{0:-}", "{0: }" };
  for (char const *spec : specs)
    {
      EXPECT_EQ (format_error (spec, 'c'), "invalid format specifier for char")
          << spec;
      EXPECT_EQ (format_error (spec, "abc"), "invalid format specifier")
          << spec;
      EXPECT_EQ (format_error (spec, fake_pointer (0x42)),
                 "invalid format specifier")
          << spec;
      EXPECT_EQ (format_error (spec, true), "invalid format specifier")
          << spec;
    }
}

TEST (LumexFormatSpecsTest, GivenHashOnIntegers_WhenFormat_ThenBasePrefix)
{
  EXPECT_EQ (fmt::format ("{0:#}", 42), "42");
  EXPECT_EQ (fmt::format ("{0:#}", -42), "-42");
  EXPECT_EQ (fmt::format ("{0:#b}", 42), "0b101010");
  EXPECT_EQ (fmt::format ("{0:#B}", 42), "0B101010");
  EXPECT_EQ (fmt::format ("{0:#b}", -42), "-0b101010");
  EXPECT_EQ (fmt::format ("{0:#x}", 0x42), "0x42");
  EXPECT_EQ (fmt::format ("{0:#X}", 0x42), "0X42");
  EXPECT_EQ (fmt::format ("{0:#x}", -0x42), "-0x42");
  EXPECT_EQ (fmt::format ("{0:#o}", 0), "0");
  EXPECT_EQ (fmt::format ("{0:#o}", 042), "042");
  EXPECT_EQ (fmt::format ("{0:#o}", -042), "-042");
  EXPECT_EQ (fmt::format ("{0:#}", 42u), "42");
  EXPECT_EQ (fmt::format ("{0:#x}", 0x42u), "0x42");
  EXPECT_EQ (fmt::format ("{0:#o}", 042u), "042");
  EXPECT_EQ (fmt::format ("{0:#}", -42l), "-42");
  EXPECT_EQ (fmt::format ("{0:#x}", 0x42l), "0x42");
  EXPECT_EQ (fmt::format ("{0:#x}", -0x42l), "-0x42");
  EXPECT_EQ (fmt::format ("{0:#o}", 042l), "042");
  EXPECT_EQ (fmt::format ("{0:#o}", -042l), "-042");
  EXPECT_EQ (fmt::format ("{0:#}", 42ul), "42");
  EXPECT_EQ (fmt::format ("{0:#x}", 0x42ul), "0x42");
  EXPECT_EQ (fmt::format ("{0:#o}", 042ul), "042");
  EXPECT_EQ (fmt::format ("{0:#}", -42ll), "-42");
  EXPECT_EQ (fmt::format ("{0:#x}", 0x42ll), "0x42");
  EXPECT_EQ (fmt::format ("{0:#x}", -0x42ll), "-0x42");
  EXPECT_EQ (fmt::format ("{0:#o}", 042ll), "042");
  EXPECT_EQ (fmt::format ("{0:#o}", -042ll), "-042");
  EXPECT_EQ (fmt::format ("{0:#}", 42ull), "42");
  EXPECT_EQ (fmt::format ("{0:#x}", 0x42ull), "0x42");
  EXPECT_EQ (fmt::format ("{0:#o}", 042ull), "042");
  EXPECT_EQ (fmt::format ("int: {0:d};  hex: {0:#x};  oct: {0:#o}", 42),
             "int: 42;  hex: 0x2a;  oct: 052");
}

TEST (LumexFormatSpecsTest, GivenHashOnFloats_WhenFormat_ThenPointKept)
{
  EXPECT_EQ (fmt::format ("{0:#}", -42.0), "-42.");
  EXPECT_EQ (fmt::format ("{0:#}", -42.0l), "-42.");
  EXPECT_EQ (fmt::format ("{:#.0e}", 42.0), "4.e+01");
  EXPECT_EQ (fmt::format ("{:#.0f}", 0.01), "0.");
  EXPECT_EQ (fmt::format ("{:#.2g}", 0.5), "0.50");
  EXPECT_EQ (fmt::format ("{:#.0f}", 0.5), "0.");
  EXPECT_EQ (fmt::format ("{:#.0f}", 123.0), "123.");
  EXPECT_EQ (fmt::format ("{:#g}", 1.0), "1.00000");
  EXPECT_EQ (fmt::format ("{:#}", 1e20), "1.e+20");
  // Without a type `#` keeps the decimal point but not trailing zeros.
  EXPECT_EQ (fmt::format ("{:#.3}", 1.0), "1.");
}

TEST (LumexFormatSpecsTest, GivenHashOnNonNumeric_WhenFormat_ThenFormatError)
{
  EXPECT_EQ (format_error ("{0:#", 'c'), "invalid format specifier for char");
  EXPECT_EQ (format_error ("{0:#}", 'c'), "invalid format specifier for char");
  EXPECT_EQ (format_error ("{0:#}", "abc"), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:#}", fake_pointer (0x42)),
             "invalid format specifier");
}

TEST (LumexFormatSpecsTest, GivenZeroFlag_WhenFormat_ThenZerosAfterSign)
{
  EXPECT_EQ (fmt::format ("{0:0}", 42), "42");
  EXPECT_EQ (fmt::format ("{0:05}", -42), "-0042");
  EXPECT_EQ (fmt::format ("{0:05}", 42u), "00042");
  EXPECT_EQ (fmt::format ("{0:05}", -42l), "-0042");
  EXPECT_EQ (fmt::format ("{0:05}", 42ul), "00042");
  EXPECT_EQ (fmt::format ("{0:05}", -42ll), "-0042");
  EXPECT_EQ (fmt::format ("{0:05}", 42ull), "00042");
  EXPECT_EQ (fmt::format ("{0:07}", -42.0), "-000042");
  EXPECT_EQ (fmt::format ("{0:07}", -42.0l), "-000042");
  EXPECT_EQ (fmt::format ("{:#06x}", 0x2a), "0x002a");
  EXPECT_EQ (fmt::format ("{:+06d}", 42), "+00042");
  EXPECT_EQ (fmt::format ("{:08.3f}", -3.14159), "-003.142");
  EXPECT_EQ (fmt::format ("{:05}", -1.5), "-01.5");
}

TEST (LumexFormatSpecsTest,
      GivenZeroFlagOnNonNumeric_WhenFormat_ThenFormatError)
{
  EXPECT_EQ (format_error ("{0:0", 'c'), "invalid format specifier for char");
  EXPECT_EQ (format_error ("{0:05}", 'c'),
             "invalid format specifier for char");
  EXPECT_EQ (format_error ("{0:05}", "abc"),
             "format specifier requires numeric argument");
  // Pointers take zero padding after the prefix (P2510, as std::format).
  EXPECT_EQ (runtime_format ("{0:06}", fake_pointer (0x42)), "0x0042");
  EXPECT_EQ (runtime_format ("{0:06}", nullptr), "0x0000");
  EXPECT_EQ (runtime_format ("{0:<06}", fake_pointer (0x42)), "0x42  ");
}

TEST (LumexFormatSpecsTest, GivenZeroFlagAndAlign_WhenFormat_ThenZeroIgnored)
{
  EXPECT_EQ (fmt::format ("{:<05}", 42), "42   ");
  EXPECT_EQ (fmt::format ("{:<05}", -42), "-42  ");
  EXPECT_EQ (fmt::format ("{:^05}", 42), " 42  ");
  EXPECT_EQ (fmt::format ("{:^05}", -42), " -42 ");
  EXPECT_EQ (fmt::format ("{:>05}", 42), "   42");
  EXPECT_EQ (fmt::format ("{:>05}", -42), "  -42");
}

TEST (LumexFormatSpecsTest, GivenWidth_WhenFormat_ThenMinimumColumns)
{
  EXPECT_EQ (fmt::format ("{:4}", -42), " -42");
  EXPECT_EQ (fmt::format ("{:5}", 42u), "   42");
  EXPECT_EQ (fmt::format ("{:6}", -42l), "   -42");
  EXPECT_EQ (fmt::format ("{:7}", 42ul), "     42");
  EXPECT_EQ (fmt::format ("{:6}", -42ll), "   -42");
  EXPECT_EQ (fmt::format ("{:7}", 42ull), "     42");
  EXPECT_EQ (fmt::format ("{:8}", -1.23), "   -1.23");
  EXPECT_EQ (fmt::format ("{:9}", -1.23l), "    -1.23");
  EXPECT_EQ (fmt::format ("{:10}", fake_pointer (0xcafe)), "    0xcafe");
  EXPECT_EQ (fmt::format ("{:11}", 'x'), "x          ");
  EXPECT_EQ (fmt::format ("{:12}", "str"), "str         ");
  EXPECT_EQ (fmt::format ("{:#6}", 42.0), "   42.");
  EXPECT_EQ (fmt::format ("{:6c}", static_cast<int> ('x')), "x     ");
  EXPECT_EQ (fmt::format ("{:>06.0f}", 0.00884311), "     0");
  EXPECT_EQ (fmt::format ("{:2}", 12345), "12345");
}

TEST (LumexFormatSpecsTest, GivenWideCharacters_WhenPadded_ThenTwoColumnsEach)
{
  // U+1F921 and CJK ideographs take two columns.
  EXPECT_EQ (fmt::format ("{:*^5}", "\xF0\x9F\xA4\xA1"),
             "*\xF0\x9F\xA4\xA1**");
  EXPECT_EQ (fmt::format ("{:*^6}", "\xF0\x9F\xA4\xA1"),
             "**\xF0\x9F\xA4\xA1**");
  EXPECT_EQ (fmt::format ("{:*^8}", "\xE4\xBD\xA0\xE5\xA5\xBD"),
             "**\xE4\xBD\xA0\xE5\xA5\xBD**");
  // Two-byte Cyrillic letters take one column each.
  EXPECT_EQ (fmt::format ("{:*<6}", "\xD1\x82\xD1\x83"),
             "\xD1\x82\xD1\x83****");
}

TEST (LumexFormatSpecsTest, GivenHugeWidth_WhenFormat_ThenNumberTooBig)
{
  std::string const int_maxer = std::to_string (INT_MAX + 1u);
  EXPECT_EQ (format_error ("{:" + int_maxer, 0), "number is too big");
  EXPECT_EQ (format_error ("{:" + int_maxer + "}", 0), "number is too big");
}

TEST (LumexFormatSpecsTest, GivenDynamicWidth_WhenFormat_ThenArgumentUsed)
{
  EXPECT_EQ (fmt::format ("{0:{1}}", -42, 4), " -42");
  EXPECT_EQ (fmt::format ("{0:{1}}", 42u, 5), "   42");
  EXPECT_EQ (fmt::format ("{0:{1}}", -42l, 6), "   -42");
  EXPECT_EQ (fmt::format ("{0:{1}}", 42ul, 7), "     42");
  EXPECT_EQ (fmt::format ("{0:{1}}", -42ll, 6), "   -42");
  EXPECT_EQ (fmt::format ("{0:{1}}", 42ull, 7), "     42");
  EXPECT_EQ (fmt::format ("{0:{1}}", -1.23, 8), "   -1.23");
  EXPECT_EQ (fmt::format ("{0:{1}}", -1.23l, 9), "    -1.23");
  EXPECT_EQ (fmt::format ("{0:{1}}", fake_pointer (0xcafe), 10), "    0xcafe");
  EXPECT_EQ (fmt::format ("{0:{1}}", 'x', 11), "x          ");
  EXPECT_EQ (fmt::format ("{0:{1}}", "str", 12), "str         ");
  EXPECT_EQ (fmt::format ("{:{}}", 42, static_cast<short> (4)), "  42");
  EXPECT_EQ (fmt::format ("{:{}}", 7, 4), "   7");
  EXPECT_EQ (fmt::format ("{:*^{}}", "a", 5u), "**a**");
}

TEST (LumexFormatSpecsTest, GivenBadDynamicWidth_WhenFormat_ThenFormatError)
{
  std::string const int_maxer = std::to_string (INT_MAX + 1u);
  EXPECT_EQ (format_error ("{0:{" + int_maxer, 0), "invalid format string");
  EXPECT_EQ (format_error ("{0:{" + int_maxer + "}", 0), "argument not found");
  EXPECT_EQ (format_error ("{0:{" + int_maxer + "}}", 0),
             "argument not found");
  EXPECT_EQ (format_error ("{0:{", 0), "invalid format string");
  EXPECT_EQ (format_error ("{0:{}", 0),
             "cannot switch from manual to automatic argument indexing");
  EXPECT_EQ (format_error ("{0:{?}}", 0), "invalid format string");
  EXPECT_EQ (format_error ("{0:{1}}", 0), "argument not found");
  EXPECT_EQ (format_error ("{0:{0:}}", 0), "invalid format string");
}

TEST (LumexFormatSpecsTest, GivenOutOfRangeDynamicWidth_WhenFormat_ThenError)
{
  EXPECT_EQ (format_error ("{0:{1}}", 0, -1),
             "width/precision is out of range");
  EXPECT_EQ (format_error ("{0:{1}}", 0, INT_MAX + 1u),
             "width/precision is out of range");
  EXPECT_EQ (format_error ("{0:{1}}", 0, -1l),
             "width/precision is out of range");
  EXPECT_EQ (format_error ("{0:{1}}", 0, static_cast<long long> (INT_MAX) + 1),
             "width/precision is out of range");
  EXPECT_EQ (format_error ("{0:{1}}", 0, INT_MAX + 1ul),
             "width/precision is out of range");
}

TEST (LumexFormatSpecsTest, GivenNonIntegerDynamicWidth_WhenFormat_ThenError)
{
  EXPECT_EQ (format_error ("{0:{1}}", 0, '0'),
             "width/precision is not integer");
  EXPECT_EQ (format_error ("{0:{1}}", 0, 0.0),
             "width/precision is not integer");
  EXPECT_EQ (format_error ("{0:{1}}", 0, "4"),
             "width/precision is not integer");
  EXPECT_EQ (format_error ("{0:{1}}", 0, true),
             "width/precision is not integer");
}

TEST (LumexFormatSpecsTest, GivenPrecisionOnFloat_WhenFormat_ThenDigitsLimited)
{
  EXPECT_EQ (fmt::format ("{0:.2}", 1.2345), "1.2");
  EXPECT_EQ (fmt::format ("{0:.2}", 1.2345l), "1.2");
  EXPECT_EQ (fmt::format ("{:.2}", 1.234e56), "1.2e+56");
  EXPECT_EQ (fmt::format ("{0:.3}", 1.1), "1.1");
  EXPECT_EQ (fmt::format ("{:.0e}", 1.0L), "1e+00");
  EXPECT_EQ (fmt::format ("{:9.1e}", 0.0), "  0.0e+00");
  EXPECT_EQ (fmt::format ("{:.7f}", 0.0000000000000071054273576010018587L),
             "0.0000000");
  EXPECT_EQ (fmt::format ("{:.02f}", 1.234), "1.23");
  EXPECT_EQ (fmt::format ("{:.1g}", 0.001), "0.001");
  EXPECT_EQ (fmt::format ("{:.0e}", 9.5), "1e+01");
  EXPECT_EQ (fmt::format ("{:.1e}", 1e-34), "1.0e-34");
}

TEST (LumexFormatSpecsTest, GivenPrecisionOnString_WhenFormat_ThenTruncated)
{
  EXPECT_EQ (fmt::format ("{0:.2}", "str"), "st");
  EXPECT_EQ (fmt::format ("{0:.0}", "str"), "");
  EXPECT_EQ (fmt::format ("{0:.10}", "str"), "str");
  EXPECT_EQ (fmt::format ("{:.2}", "abcdef"), "ab");
  EXPECT_EQ (fmt::format ("{:>5.2}", "abcdef"), "   ab");
  // Precision counts display columns, never splits a code point.
  EXPECT_EQ (fmt::format ("{0:.5}", "\xD0\xB2\xD0\xBE\xD0\xB6\xD1\x8B\xD0\xBA"
                                    "\xD1\x96"),
             "\xD0\xB2\xD0\xBE\xD0\xB6\xD1\x8B\xD0\xBA");
  EXPECT_EQ (fmt::format ("{:.4}", "caf\xC3\xA9s"), "caf\xC3\xA9");
  EXPECT_EQ (fmt::format ("{0:.6}", "123456\xad"), "123456");
  // A wide character that does not fit is dropped entirely.
  EXPECT_EQ (fmt::format ("{:.5}", "\xF0\x9F\x90\xB1\xF0\x9F\x90\xB1"
                                   "\xF0\x9F\x90\xB1"),
             "\xF0\x9F\x90\xB1\xF0\x9F\x90\xB1");
}

TEST (LumexFormatSpecsTest, GivenBadPrecision_WhenFormat_ThenFormatError)
{
  std::string const int_maxer = std::to_string (INT_MAX + 1u);
  EXPECT_EQ (format_error ("{0:." + int_maxer, 0.0), "number is too big");
  EXPECT_EQ (format_error ("{0:." + int_maxer + "}", 0.0),
             "number is too big");
  EXPECT_EQ (format_error ("{0:.99999999999999999999}", 0.0),
             "number is too big");
  EXPECT_EQ (format_error ("{0:.", 0.0), "invalid precision");
  EXPECT_EQ (format_error ("{0:.}", 0.0), "invalid format string");
  EXPECT_EQ (format_error ("{:.f}", 42.0), "invalid format string");
  EXPECT_EQ (format_error ("{:.2147483646f}", -2.2121295195081227E+304),
             "number is too big");
}

TEST (LumexFormatSpecsTest, GivenPrecisionOnInteger_WhenFormat_ThenFormatError)
{
  EXPECT_EQ (format_error ("{0:.2", 0), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.2}", 42), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.2f}", 42), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.2}", 42u), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.2f}", 42u), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.2}", 42l), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.2f}", 42l), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.2}", 42ul), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.2f}", 42ul), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.2}", 42ll), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.2f}", 42ll), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.2}", 42ull), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.2f}", 42ull), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:3.0}", 'x'), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.2}", fake_pointer (0xcafe)),
             "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.2f}", fake_pointer (0xcafe)),
             "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.2}", true), "invalid format specifier");
}

TEST (LumexFormatSpecsTest, GivenDynamicPrecision_WhenFormat_ThenArgumentUsed)
{
  EXPECT_EQ (fmt::format ("{0:.{1}}", 1.2345, 2), "1.2");
  EXPECT_EQ (fmt::format ("{1:.{0}}", 2, 1.2345l), "1.2");
  EXPECT_EQ (fmt::format ("{0:.{1}}", "str", 2), "st");
  EXPECT_EQ (fmt::format ("{:.{}}", 1.2345, 2), "1.2");
  EXPECT_EQ (fmt::format ("{:.{}f}", 3.14159, 2), "3.14");
  EXPECT_EQ (fmt::format ("{:{}.{}f}", 3.14159, 7, 2), "   3.14");
}

TEST (LumexFormatSpecsTest, GivenBadDynamicPrecision_WhenFormat_ThenError)
{
  std::string const int_maxer = std::to_string (INT_MAX + 1u);
  EXPECT_EQ (format_error ("{0:.{" + int_maxer, 0.0), "invalid format string");
  EXPECT_EQ (format_error ("{0:.{" + int_maxer + "}", 0.0),
             "argument not found");
  EXPECT_EQ (format_error ("{0:.{" + int_maxer + "}}", 0.0),
             "argument not found");
  EXPECT_EQ (format_error ("{0:.{", 0.0), "invalid format string");
  EXPECT_EQ (format_error ("{0:.{}", 0.0),
             "cannot switch from manual to automatic argument indexing");
  EXPECT_EQ (format_error ("{0:.{?}}", 0.0), "invalid format string");
  EXPECT_EQ (format_error ("{0:.{1}", 0, 0), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.{1}}", 0.0), "argument not found");
  EXPECT_EQ (format_error ("{0:.{0:}}", 0.0), "invalid format string");
  EXPECT_EQ (format_error ("{0:.{1}}", 0.0, -1),
             "width/precision is out of range");
  EXPECT_EQ (format_error ("{0:.{1}}", 0.0, INT_MAX + 1u),
             "width/precision is out of range");
  EXPECT_EQ (format_error ("{0:.{1}}", 0.0, -1l),
             "width/precision is out of range");
  EXPECT_EQ (format_error ("{0:.{1}}", 0.0, INT_MAX + 1ul),
             "width/precision is out of range");
  EXPECT_EQ (format_error ("{0:.{1}}", 0.0, '0'),
             "width/precision is not integer");
  EXPECT_EQ (format_error ("{0:.{1}}", 0.0, 0.0),
             "width/precision is not integer");
}

TEST (LumexFormatSpecsTest,
      GivenDynamicPrecisionOnInteger_WhenFormat_ThenError)
{
  EXPECT_EQ (format_error ("{0:.{1}}", 42, 2), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.{1}f}", 42, 2), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.{1}}", 42u, 2), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.{1}f}", 42u, 2), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.{1}}", 42l, 2), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.{1}f}", 42l, 2), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.{1}}", 42ul, 2), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.{1}f}", 42ul, 2), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.{1}}", 42ll, 2), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.{1}f}", 42ll, 2), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.{1}}", 42ull, 2), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.{1}f}", 42ull, 2), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:3.{1}}", 'x', 0), "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.{1}}", fake_pointer (0xcafe), 2),
             "invalid format specifier");
  EXPECT_EQ (format_error ("{0:.{1}f}", fake_pointer (0xcafe), 2),
             "invalid format specifier");
}

TEST (LumexFormatSpecsTest, GivenTrailingGarbage_WhenFormat_ThenFormatError)
{
  EXPECT_EQ (format_error ("{0:v", 42), "invalid format specifier");
  EXPECT_EQ (format_error ("{:dd}", 42), "invalid format specifier");
  EXPECT_EQ (format_error ("{:5 }", 42), "invalid format specifier");
  EXPECT_EQ (format_error ("{:d", 42), "missing '}' in format string");
}
