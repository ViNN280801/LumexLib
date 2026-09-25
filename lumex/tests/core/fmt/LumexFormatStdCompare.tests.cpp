// LumexFormatStdCompare.tests.cpp
// Cross-check against the standard library: where <format> is available
// (C++20), every specification in the tables below must produce the same
// text as std::format. Compiled into every LumexFormat suite; the tests
// exist only when the standard library provides std::format.
#if __cplusplus >= 202002L && defined(__has_include)
#if __has_include(<format>)
#include <format>
#endif
#endif

#include <climits>
#include <cstdint>
#include <limits>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/fmt/LumexFormat.hpp"

#if defined(__cpp_lib_format) && __cpp_lib_format >= 201907L

namespace fmt = lumex::core::fmt;

namespace
{
template <typename T, std::size_t N>
void
expect_same_as_std (char const *const (&specs)[N], T value)
{
  for (char const *spec : specs)
    {
      std::string const expected
          = std::vformat (spec, std::make_format_args (value));
      std::string const actual
          = fmt::vformat (spec, fmt::make_format_args (value));
      EXPECT_EQ (actual, expected) << "spec " << spec;
    }
}

char const *const integer_specs[]
    = { "{}",    "{:d}",  "{:+}",  "{: }",   "{:#x}",  "{:#X}",
        "{:#o}", "{:#b}", "{:#B}", "{:08}",  "{:+08}", "{:#010x}",
        "{:<8}", "{:>8}", "{:^8}", "{:*^9}", "{:x}",   "{:o}",
        "{:b}",  "{:-}",  "{:#}",  "{:1}",   "{:<08}" };

char const *const float_specs[]
    = { "{}",         "{:e}",   "{:E}",   "{:f}",   "{:F}",       "{:g}",
        "{:G}",       "{:a}",   "{:A}",   "{:.0}",  "{:.1}",      "{:.3}",
        "{:.17}",     "{:.0e}", "{:.3e}", "{:.0f}", "{:.3f}",     "{:.10f}",
        "{:.0g}",     "{:.3g}", "{:.3a}", "{:#}",   "{:#.0f}",    "{:#.0e}",
        "{:#g}",      "{:#.3}", "{:#a}",  "{:+}",   "{: }",       "{:012}",
        "{:+012.3f}", "{:<12}", "{:>12}", "{:^12}", "{:*^14.2e}", "{:#.3g}",
        "{:.20g}",    "{:+.0}" };

char const *const string_specs[]
    = { "{}",    "{:s}",  "{:.2}",  "{:.0}", "{:8}", "{:<8}",
        "{:>8}", "{:^8}", "{:*^9}", "{:.1}", "{:^1}" };

char const *const char_specs[]
    = { "{}",   "{:c}",  "{:d}",  "{:#x}", "{:b}",
        "{:5}", "{:>5}", "{:^5}", "{:+d}", "{:05d}" };

// The `?` presentation is C++23 (P2286): only where the library has it.
#if defined(__cpp_lib_format_ranges)
char const *const debug_specs[] = { "{:?}", "{:.2?}", "{:>10?}" };
#endif

char const *const bool_specs[]
    = { "{}", "{:s}", "{:d}", "{:#x}", "{:6}", "{:>6}", "{:^7}", "{:05d}" };

char const *const pointer_specs[]
    = { "{}", "{:p}", "{:10}", "{:<10}", "{:^10}" };
} // namespace

TEST (LumexFormatStdCompareTest, GivenIntegers_WhenFormat_ThenSameAsStd)
{
  expect_same_as_std (integer_specs, 0);
  expect_same_as_std (integer_specs, 42);
  expect_same_as_std (integer_specs, -42);
  expect_same_as_std (integer_specs, INT_MIN);
  expect_same_as_std (integer_specs, INT_MAX);
  expect_same_as_std (integer_specs, 42u);
  expect_same_as_std (integer_specs, UINT_MAX);
  expect_same_as_std (integer_specs, LLONG_MIN);
  expect_same_as_std (integer_specs, ULLONG_MAX);
  expect_same_as_std (integer_specs, static_cast<short> (-7));
  expect_same_as_std (integer_specs, static_cast<unsigned char> (200));
  expect_same_as_std (integer_specs, static_cast<signed char> (-100));
}

TEST (LumexFormatStdCompareTest, GivenDoubles_WhenFormat_ThenSameAsStd)
{
  double const values[] = { 0.0,
                            -0.0,
                            1.0,
                            -1.5,
                            0.1,
                            392.65,
                            1e-5,
                            1e-4,
                            1e15,
                            1e16,
                            1e21,
                            123456789.0,
                            0.000123456,
                            9.999e-5,
                            1.9156918820264798e-56,
                            5e-324,
                            1.7976931348623157e308,
                            (std::numeric_limits<double>::min) (),
                            std::numeric_limits<double>::infinity (),
                            -std::numeric_limits<double>::infinity (),
                            std::numeric_limits<double>::quiet_NaN (),
                            0.5,
                            2.5,
                            1e100,
                            4.35,
                            0.0005,
                            0.0015 };
  for (double value : values)
    expect_same_as_std (float_specs, value);
}

TEST (LumexFormatStdCompareTest, GivenFloats_WhenFormat_ThenSameAsStd)
{
  float const values[] = { 0.0f,          0.1f,          1.5f,
                           123456789.0f,  1019666432.0f, 1.35631564e-19f,
                           3.4028235e38f, 1e-45f,        16777217.0f };
  for (float value : values)
    expect_same_as_std (float_specs, value);
}

TEST (LumexFormatStdCompareTest, GivenLongDoubles_WhenFormat_ThenSameAsStd)
{
  long double const values[] = { 0.0l, 392.65l, -1.5l, 1e-5l, 0.1l };
  for (long double value : values)
    expect_same_as_std (float_specs, value);
}

TEST (LumexFormatStdCompareTest, GivenStrings_WhenFormat_ThenSameAsStd)
{
  expect_same_as_std (string_specs, "text");
  expect_same_as_std (string_specs, "");
  expect_same_as_std (string_specs, "a\tb\n\"q\"\\");
  expect_same_as_std (string_specs, std::string ("std string"));
  expect_same_as_std (string_specs, std::string_view ("view"));
  // Cyrillic, CJK and emoji: width and precision count display columns.
  expect_same_as_std (string_specs, "\xD1\x82\xD1\x83\xD0\xB4\xD0\xB0");
  expect_same_as_std (string_specs, "\xE4\xB8\xAD\xE5\xBF\x83");
  expect_same_as_std (string_specs, "\xF0\x9F\x90\xB1\xF0\x9F\x90\xB1");
}

TEST (LumexFormatStdCompareTest, GivenCharsAndBools_WhenFormat_ThenSameAsStd)
{
  expect_same_as_std (char_specs, 'x');
  expect_same_as_std (char_specs, '\n');
  expect_same_as_std (char_specs, '\'');
  expect_same_as_std (char_specs, '"');
  expect_same_as_std (bool_specs, true);
  expect_same_as_std (bool_specs, false);
}

#if defined(__cpp_lib_format_ranges)
TEST (LumexFormatStdCompareTest,
      GivenDebugPresentation_WhenFormat_ThenSameAsStd)
{
  expect_same_as_std (debug_specs, "a\tb\n\"q\"\\");
  expect_same_as_std (debug_specs, std::string ("\x01z"));
  expect_same_as_std (debug_specs, '\n');
  expect_same_as_std (debug_specs, '\'');
}
#endif

TEST (LumexFormatStdCompareTest, GivenPointers_WhenFormat_ThenSameAsStd)
{
  expect_same_as_std (pointer_specs, static_cast<void const *> (nullptr));
  expect_same_as_std (pointer_specs, reinterpret_cast<void const *> (
                                         std::uintptr_t{ 0xface }));
  expect_same_as_std (pointer_specs, nullptr);
}

TEST (LumexFormatStdCompareTest, GivenWideStrings_WhenFormat_ThenSameAsStd)
{
  EXPECT_EQ (fmt::format (L"{:>6}|{:#x}|{}", L"ab", 255, 1.5),
             std::format (L"{:>6}|{:#x}|{}", L"ab", 255, 1.5));
}

TEST (LumexFormatStdCompareTest, GivenDynamicSpecs_WhenFormat_ThenSameAsStd)
{
  EXPECT_EQ (fmt::format ("{:{}.{}f}", 3.14159, 10, 3),
             std::format ("{:{}.{}f}", 3.14159, 10, 3));
  EXPECT_EQ (fmt::format ("{0:*^{1}}", "x", 7),
             std::format ("{0:*^{1}}", "x", 7));
}

#endif
