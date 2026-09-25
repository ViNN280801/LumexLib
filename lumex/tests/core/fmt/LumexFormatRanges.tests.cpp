// LumexFormatRanges.tests.cpp
// LumexFormatRanges.hpp: ranges, sets, maps, std::pair and std::tuple with
// the C++23 range specification ([[fill]align][width][n][m|s|?s][:element]).
// Every expected text below matches std::format of C++23 (checked with
// MSVC /std:c++latest); where the library provides range formatting the
// std comparison at the end runs too.
#include <array>
#include <deque>
#include <list>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#if __cplusplus >= 202002L && defined(__has_include)
#if __has_include(<format>)
#include <format>
#endif
#endif

#include <gtest/gtest.h>

#include "lumex/core/fmt/LumexFormatRanges.hpp"
#include "lumex/tests/core/fmt/LumexFormatTestHelpers.hpp"

namespace fmt = lumex::core::fmt;

using format_test_helpers::format_error;

namespace
{
struct opaque_t
{
};
} // namespace

TEST (LumexFormatRangesTest, GivenSequence_WhenFormat_ThenSquareBrackets)
{
  std::vector<int> const values = { 1, 2, 3 };
  EXPECT_EQ (fmt::format ("{}", values), "[1, 2, 3]");
  EXPECT_EQ (fmt::format ("{}", std::vector<int> ()), "[]");
  EXPECT_EQ (fmt::format ("{}", std::list<double>{ 0.5, 1e20 }),
             "[0.5, 1e+20]");
  EXPECT_EQ (fmt::format ("{}", std::deque<int>{ 7 }), "[7]");
  std::array<int, 2> const array = { { 4, 5 } };
  EXPECT_EQ (fmt::format ("{}", array), "[4, 5]");
}

TEST (LumexFormatRangesTest, GivenNoBracketsOption_WhenFormat_ThenBareList)
{
  std::vector<int> const values = { 1, 2, 3 };
  EXPECT_EQ (fmt::format ("{:n}", values), "1, 2, 3");
  EXPECT_EQ (fmt::format ("{:n}", std::set<int>{ 3, 1, 2 }), "1, 2, 3");
}

TEST (LumexFormatRangesTest, GivenElementSpec_WhenFormat_ThenAppliedToEach)
{
  std::vector<int> const values = { 1, 2, 3 };
  EXPECT_EQ (fmt::format ("{::#x}", values), "[0x1, 0x2, 0x3]");
  EXPECT_EQ (fmt::format ("{:>12n:02}", values), "  01, 02, 03");
  EXPECT_EQ (fmt::format ("{::.2f}", std::list<double>{ 0.5, 1e20 }),
             "[0.50, 100000000000000000000.00]");
  EXPECT_EQ (fmt::format ("{::{}}", values, 3), "[  1,   2,   3]");
}

TEST (LumexFormatRangesTest, GivenWidth_WhenFormat_ThenWholeRangePadded)
{
  std::vector<int> const values = { 1, 2, 3 };
  EXPECT_EQ (fmt::format ("{:*^15}", values), "***[1, 2, 3]***");
  EXPECT_EQ (fmt::format ("{:12}", values), "[1, 2, 3]   ");
  EXPECT_EQ (fmt::format ("{:>{}}", values, 11), "  [1, 2, 3]");
  EXPECT_EQ (fmt::format ("{:3}", values), "[1, 2, 3]");
}

TEST (LumexFormatRangesTest, GivenStrings_WhenFormat_ThenQuotedByDefault)
{
  std::vector<std::string> const strings = { "a", "b\tc" };
  EXPECT_EQ (fmt::format ("{}", strings), "[\"a\", \"b\\tc\"]");
  EXPECT_EQ (fmt::format ("{::}", strings), "[a, b\tc]");
  EXPECT_EQ (fmt::format ("{::>3}", strings), "[  a, b\tc]");
  std::vector<char const *> const c_strings = { "x", "y" };
  EXPECT_EQ (fmt::format ("{}", c_strings), "[\"x\", \"y\"]");
}

TEST (LumexFormatRangesTest, GivenCharacters_WhenFormat_ThenQuotedOrString)
{
  std::vector<char> const chars = { 'h', 'i', '\n' };
  EXPECT_EQ (fmt::format ("{}", chars), "['h', 'i', '\\n']");
  EXPECT_EQ (fmt::format ("{:s}", chars), "hi\n");
  EXPECT_EQ (fmt::format ("{:?s}", chars), "\"hi\\n\"");
  EXPECT_EQ (fmt::format ("{::d}", chars), "[104, 105, 10]");
  EXPECT_EQ (fmt::format ("{:>5s}", std::vector<char>{ 'o', 'k' }), "   ok");
}

TEST (LumexFormatRangesTest, GivenMap_WhenFormat_ThenBracesAndColons)
{
  std::map<std::string, int> const map = { { "one", 1 }, { "two", 2 } };
  EXPECT_EQ (fmt::format ("{}", map), "{\"one\": 1, \"two\": 2}");
  EXPECT_EQ (fmt::format ("{:n}", map), "\"one\": 1, \"two\": 2");
  EXPECT_EQ (fmt::format ("{::}", map), "{\"one\": 1, \"two\": 2}");
  std::map<int, std::vector<std::string>> const nested = { { 1, { "x" } } };
  EXPECT_EQ (fmt::format ("{}", nested), "{1: [\"x\"]}");
  EXPECT_EQ (fmt::format ("{}", std::map<int, int> ()), "{}");
}

TEST (LumexFormatRangesTest, GivenSet_WhenFormat_ThenBraces)
{
  EXPECT_EQ (fmt::format ("{}", std::set<int>{ 3, 1, 2 }), "{1, 2, 3}");
  EXPECT_EQ (fmt::format ("{}", std::set<std::string>{ "b", "a" }),
             "{\"a\", \"b\"}");
}

TEST (LumexFormatRangesTest, GivenPairsWithMapOption_WhenFormat_ThenMapStyle)
{
  std::vector<std::pair<int, int>> const pairs = { { 1, 2 }, { 3, 4 } };
  EXPECT_EQ (fmt::format ("{}", pairs), "[(1, 2), (3, 4)]");
  EXPECT_EQ (fmt::format ("{:m}", pairs), "{1: 2, 3: 4}");
  EXPECT_EQ (fmt::format ("{:nm}", pairs), "1: 2, 3: 4");
}

TEST (LumexFormatRangesTest, GivenNestedRanges_WhenFormat_ThenRecursive)
{
  std::vector<std::vector<int>> const nested = { { 1, 2 }, { 3 } };
  EXPECT_EQ (fmt::format ("{}", nested), "[[1, 2], [3]]");
  EXPECT_EQ (fmt::format ("{::n}", nested), "[1, 2, 3]");
  EXPECT_EQ (fmt::format ("{:::#x}", nested), "[[0x1, 0x2], [0x3]]");
}

TEST (LumexFormatRangesTest, GivenBoolVector_WhenFormat_ThenBoolNames)
{
  std::vector<bool> const bits = { true, false };
  EXPECT_EQ (fmt::format ("{}", bits), "[true, false]");
  EXPECT_EQ (fmt::format ("{::d}", bits), "[1, 0]");
}

TEST (LumexFormatRangesTest, GivenPair_WhenFormat_ThenParentheses)
{
  std::pair<int, std::string> const pair (1, "x");
  EXPECT_EQ (fmt::format ("{}", pair), "(1, \"x\")");
  EXPECT_EQ (fmt::format ("{:n}", pair), "1, \"x\"");
  EXPECT_EQ (fmt::format ("{:m}", pair), "1: \"x\"");
  EXPECT_EQ (fmt::format ("{:>10}", pair), "  (1, \"x\")");
  EXPECT_EQ (fmt::format ("{:*<10}", pair), "(1, \"x\")**");
}

TEST (LumexFormatRangesTest, GivenTuple_WhenFormat_ThenParentheses)
{
  std::tuple<int, char, double> const tuple (1, 'c', 2.5);
  EXPECT_EQ (fmt::format ("{}", tuple), "(1, 'c', 2.5)");
  EXPECT_EQ (fmt::format ("{:n}", tuple), "1, 'c', 2.5");
  EXPECT_EQ (fmt::format ("{}", std::tuple<> ()), "()");
  EXPECT_EQ (fmt::format ("{}", std::make_tuple (std::make_pair (1, 2))),
             "((1, 2))");
  std::deque<std::tuple<int, std::string>> const rows
      = { std::make_tuple (1, std::string ("a")) };
  EXPECT_EQ (fmt::format ("{}", rows), "[(1, \"a\")]");
}

TEST (LumexFormatRangesTest, GivenBadRangeSpecs_WhenFormat_ThenFormatError)
{
  std::vector<int> const values = { 1 };
  EXPECT_EQ (format_error ("{:s}", values), "invalid format specifier");
  EXPECT_EQ (format_error ("{:?s}", values), "invalid format specifier");
  EXPECT_EQ (format_error ("{:m}", values), "invalid format specifier");
  EXPECT_EQ (format_error ("{:x}", values), "invalid format specifier");
  EXPECT_EQ (format_error ("{::s}", values), "invalid format specifier");
  EXPECT_EQ (format_error ("{:s:}", std::vector<char>{ 'a' }),
             "invalid format specifier");
  EXPECT_EQ (format_error ("{:{<5}", values), "invalid fill character '{'");
}

TEST (LumexFormatRangesTest, GivenBadTupleSpecs_WhenFormat_ThenFormatError)
{
  std::tuple<int, int, int> const triple (1, 2, 3);
  EXPECT_EQ (format_error ("{:m}", triple), "invalid format specifier");
  EXPECT_EQ (format_error ("{:x}", std::make_pair (1, 2)),
             "invalid format specifier");
  EXPECT_EQ (format_error ("{::d}", std::make_pair (1, 2)),
             "invalid format specifier");
}

TEST (LumexFormatRangesTest, GivenWideFormat_WhenRange_ThenWideOutput)
{
  std::vector<std::wstring> const strings = { L"a", L"b" };
  EXPECT_EQ (fmt::format (L"{}", strings), L"[\"a\", \"b\"]");
  std::map<int, wchar_t> const map = { { 1, L'x' } };
  EXPECT_EQ (fmt::format (L"{}", map), L"{1: 'x'}");
  EXPECT_EQ (fmt::format (L"{:>8}", std::make_pair (1, 2)), L"  (1, 2)");
}

TEST (LumexFormatRangesTest, GivenElementTypes_WhenFormattable_ThenTraitsAgree)
{
  EXPECT_TRUE ((
      std::is_default_constructible<fmt::Formatter<std::vector<int>>>::value));
  EXPECT_TRUE (
      (std::is_default_constructible<
          fmt::Formatter<std::map<std::string, std::vector<int>>>>::value));
  EXPECT_TRUE ((std::is_default_constructible<
                fmt::Formatter<std::pair<int, int>>>::value));
  // Unformattable elements make the container unformattable.
  EXPECT_FALSE ((std::is_default_constructible<
                 fmt::Formatter<std::vector<opaque_t>>>::value));
  EXPECT_FALSE ((std::is_default_constructible<
                 fmt::Formatter<std::pair<int, opaque_t>>>::value));
  // Strings stay strings.
  EXPECT_EQ (fmt::format ("{}", std::string ("text")), "text");
}

#if defined(__cpp_lib_format_ranges) && __cpp_lib_format_ranges >= 202207L
TEST (LumexFormatRangesTest, GivenRanges_WhenFormat_ThenSameAsStd)
{
  std::vector<int> const values = { 1, 2, 3 };
  std::map<std::string, int> const map = { { "a", 1 } };
  std::vector<char> const chars = { 'a', '\t' };
  std::tuple<int, std::string, char> const tuple (1, "s", 'c');
  EXPECT_EQ (fmt::format ("{:*^15}|{::#x}|{:n}", values, values, values),
             std::format ("{:*^15}|{::#x}|{:n}", values, values, values));
  EXPECT_EQ (fmt::format ("{}|{:n}", map, map),
             std::format ("{}|{:n}", map, map));
  EXPECT_EQ (fmt::format ("{}|{:s}|{:?s}", chars, chars, chars),
             std::format ("{}|{:s}|{:?s}", chars, chars, chars));
  EXPECT_EQ (fmt::format ("{}|{:n}|{:>12}", tuple, tuple, tuple),
             std::format ("{}|{:n}|{:>12}", tuple, tuple, tuple));
}
#endif
