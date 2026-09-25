// LumexFormatParse.tests.cpp
// Format string grammar: escapes, argument ids (automatic / manual / named),
// and the errors of malformed strings. Cases ported from fmt's format-test.cc
// (escape, unmatched_braces, args_in_different_positions, arg_errors,
// many_args, named_arg, auto_arg_index, empty_specs,
// non_null_terminated_format_string).
#include <climits>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/fmt/LumexFormat.hpp"
#include "lumex/tests/core/fmt/LumexFormatTestHelpers.hpp"

namespace fmt = lumex::core::fmt;

using format_test_helpers::format_error;
using format_test_helpers::runtime_format;

TEST (LumexFormatParseTest, GivenNoFields_WhenFormat_ThenTextCopied)
{
  EXPECT_EQ (fmt::format ("test"), "test");
  EXPECT_EQ (fmt::format (""), "");
}

TEST (LumexFormatParseTest, GivenDoubledOpenBrace_WhenFormat_ThenSingleBrace)
{
  EXPECT_EQ (fmt::format ("{{"), "{");
  EXPECT_EQ (fmt::format ("before {{"), "before {");
  EXPECT_EQ (fmt::format ("{{ after"), "{ after");
  EXPECT_EQ (fmt::format ("before {{ after"), "before { after");
}

TEST (LumexFormatParseTest, GivenDoubledCloseBrace_WhenFormat_ThenSingleBrace)
{
  EXPECT_EQ (fmt::format ("}}"), "}");
  EXPECT_EQ (fmt::format ("before }}"), "before }");
  EXPECT_EQ (fmt::format ("}} after"), "} after");
  EXPECT_EQ (fmt::format ("before }} after"), "before } after");
}

TEST (LumexFormatParseTest, GivenEscapesAroundField_WhenFormat_ThenBothKept)
{
  EXPECT_EQ (fmt::format ("{{}}"), "{}");
  EXPECT_EQ (fmt::format ("{{{0}}}", 42), "{42}");
  EXPECT_EQ (fmt::format ("{{}} {}", 3), "{} 3");
}

TEST (LumexFormatParseTest, GivenUnmatchedBraces_WhenFormat_ThenFormatError)
{
  EXPECT_EQ (format_error ("{"), "invalid format string");
  EXPECT_EQ (format_error ("}"), "unmatched '}' in format string");
  EXPECT_EQ (format_error ("{0{}"), "invalid format string");
  EXPECT_EQ (format_error ("a}b", 1), "unmatched '}' in format string");
}

TEST (LumexFormatParseTest, GivenIndexedFields_WhenFormat_ThenArgsPlaced)
{
  EXPECT_EQ (fmt::format ("{0}", 42), "42");
  EXPECT_EQ (fmt::format ("before {0}", 42), "before 42");
  EXPECT_EQ (fmt::format ("{0} after", 42), "42 after");
  EXPECT_EQ (fmt::format ("before {0} after", 42), "before 42 after");
  EXPECT_EQ (fmt::format ("{0} = {1}", "answer", 42), "answer = 42");
  EXPECT_EQ (fmt::format ("{1} is the {0}", "answer", 42), "42 is the answer");
  EXPECT_EQ (fmt::format ("{0}{1}{0}", "abra", "cad"), "abracadabra");
  EXPECT_EQ (fmt::format ("{2}, {1}, {0}", 'a', 'b', 'c'), "c, b, a");
}

TEST (LumexFormatParseTest, GivenAutomaticFields_WhenFormat_ThenArgsInOrder)
{
  EXPECT_EQ (fmt::format ("{}{}{}", 'a', 'b', 'c'), "abc");
  EXPECT_EQ (fmt::format ("{}, {}, {}", 'a', 'b', 'c'), "a, b, c");
  EXPECT_EQ (fmt::format ("{}c{}", "ab", 1), "abc1");
  EXPECT_EQ (fmt::format ("From {} to {}", 1, 3), "From 1 to 3");
}

TEST (LumexFormatParseTest, GivenUnusedArguments_WhenFormat_ThenIgnored)
{
  EXPECT_EQ (fmt::format ("{0}", 1, 2, 3), "1");
  EXPECT_EQ (fmt::format ("{}", 1, "unused"), "1");
  EXPECT_EQ (fmt::format ("none", 1), "none");
}

TEST (LumexFormatParseTest, GivenEmptySpecs_WhenFormat_ThenDefaultOutput)
{
  EXPECT_EQ (fmt::format ("{0:}", 42), "42");
  EXPECT_EQ (fmt::format ("{:}=", "foo"), "foo=");
  EXPECT_EQ (fmt::format ("{:}>", 42), "42>");
}

TEST (LumexFormatParseTest, GivenBadArgumentIds_WhenFormat_ThenFormatError)
{
  EXPECT_EQ (format_error ("{?}"), "invalid format string");
  EXPECT_EQ (format_error ("{0"), "invalid format string");
  EXPECT_EQ (format_error ("{0}"), "argument not found");
  EXPECT_EQ (format_error ("{00}", 42), "invalid format string");
  EXPECT_EQ (format_error ("{}"), "argument not found");
  EXPECT_EQ (format_error ("{1}", 42), "argument not found");
  EXPECT_EQ (format_error ("{} {}", 42), "argument not found");
}

TEST (LumexFormatParseTest, GivenHugeArgumentIds_WhenFormat_ThenFormatError)
{
  std::string const int_max = std::to_string (INT_MAX);
  EXPECT_EQ (format_error ("{" + int_max), "invalid format string");
  EXPECT_EQ (format_error ("{" + int_max + "}"), "argument not found");

  std::string const int_maxer = std::to_string (INT_MAX + 1u);
  EXPECT_EQ (format_error ("{" + int_maxer), "invalid format string");
  EXPECT_EQ (format_error ("{" + int_maxer + "}"), "argument not found");
}

TEST (LumexFormatParseTest, GivenManyArgs_WhenIndexedPastEnd_ThenFormatError)
{
  EXPECT_EQ (runtime_format ("{19}", 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                             13, 14, 15, 16, 17, 18, 19),
             "19");
  EXPECT_EQ (format_error ("{20}", 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                           13, 14, 15, 16, 17, 18, 19),
             "argument not found");
}

TEST (LumexFormatParseTest, GivenMixedIndexing_WhenFormat_ThenFormatError)
{
  EXPECT_EQ (format_error ("{0}{}", 'a', 'b'),
             "cannot switch from manual to automatic argument indexing");
  EXPECT_EQ (format_error ("{}{0}", 'a', 'b'),
             "cannot switch from automatic to manual argument indexing");
  EXPECT_EQ (format_error ("{0}:.{}", 1.2345, 2),
             "cannot switch from manual to automatic argument indexing");
  EXPECT_EQ (format_error ("{:.{0}}", 1.2345, 2),
             "cannot switch from automatic to manual argument indexing");
}

TEST (LumexFormatParseTest, GivenNamedArgs_WhenFormat_ThenLookedUpByName)
{
  EXPECT_EQ (fmt::format ("{_1}/{a_}/{A_}", fmt::arg ("a_", 'a'),
                          fmt::arg ("A_", "A"), fmt::arg ("_1", 1)),
             "1/a/A");
  EXPECT_EQ (fmt::format ("{name}={value}", fmt::arg ("name", "x"),
                          fmt::arg ("value", 42)),
             "x=42");
  EXPECT_EQ (fmt::format ("{a}{a}", fmt::arg ("a", 7)), "77");
}

TEST (LumexFormatParseTest, GivenNamedDynamicSpecs_WhenFormat_ThenApplied)
{
  EXPECT_EQ (fmt::format ("{0:{width}}", -42, fmt::arg ("width", 4)), " -42");
  EXPECT_EQ (fmt::format ("{value:{width}}", fmt::arg ("value", -42),
                          fmt::arg ("width", 4)),
             " -42");
  EXPECT_EQ (
      fmt::format ("{0:.{precision}}", "str", fmt::arg ("precision", 2)),
      "st");
}

TEST (LumexFormatParseTest, GivenPositionalThenNamed_WhenFormat_ThenBothWork)
{
  EXPECT_EQ (fmt::format ("{} {two}", 1, fmt::arg ("two", 2)), "1 2");
  EXPECT_EQ (fmt::format ("{0} {two} {0}", 1, fmt::arg ("two", 2)), "1 2 1");
}

TEST (LumexFormatParseTest, GivenNamedArgAddressedByIndex_WhenFormat_ThenValue)
{
  EXPECT_EQ (fmt::format ("{0} {1}", fmt::arg ("a", 1), fmt::arg ("b", 2)),
             "1 2");
}

TEST (LumexFormatParseTest, GivenManyNamedArgs_WhenFormat_ThenRightOnePicked)
{
  EXPECT_EQ (
      fmt::format ("{c}", fmt::arg ("a", 0), fmt::arg ("b", 0),
                   fmt::arg ("c", 42), fmt::arg ("d", 0), fmt::arg ("e", 0),
                   fmt::arg ("f", 0), fmt::arg ("g", 0), fmt::arg ("h", 0),
                   fmt::arg ("i", 0), fmt::arg ("j", 0), fmt::arg ("k", 0),
                   fmt::arg ("l", 0), fmt::arg ("m", 0), fmt::arg ("n", 0),
                   fmt::arg ("o", 0), fmt::arg ("p", 0)),
      "42");
}

TEST (LumexFormatParseTest, GivenBadNamedArgs_WhenFormat_ThenFormatError)
{
  EXPECT_EQ (format_error ("{a}"), "argument not found");
  EXPECT_EQ (format_error ("{a}", 42), "argument not found");
  EXPECT_EQ (format_error ("{b}", fmt::arg ("a", 1)), "argument not found");
  EXPECT_EQ (format_error ("{a} {}", fmt::arg ("a", 2), 42),
             "cannot switch from manual to automatic argument indexing");
  EXPECT_EQ (format_error ("{a}", fmt::arg ("a", 1), fmt::arg ("a", 10)),
             "duplicate named arg");
  EXPECT_EQ (format_error ("{ab}", fmt::arg ("a", 1), fmt::arg ("abc", 10)),
             "argument not found");
}

TEST (LumexFormatParseTest,
      GivenNonTerminatedString_WhenFormat_ThenOnlyViewUsed)
{
  std::string const text = "{}foo";
  EXPECT_EQ (fmt::vformat (text.substr (0, 2), fmt::make_format_args (42)),
             "42");
#if __cplusplus >= 201703L
  EXPECT_EQ (fmt::format (fmt::runtime (std::string_view ("{}foo", 2)), 42),
             "42");
#endif
}

TEST (LumexFormatParseTest, GivenRuntimeString_WhenFormat_ThenSameAsLiteral)
{
  std::string const text = "{} + {} = {}";
  EXPECT_EQ (fmt::format (fmt::runtime (text), 1, 2, 3), "1 + 2 = 3");
  EXPECT_EQ (fmt::format (fmt::runtime ("{:>4}"), 7), "   7");
}

TEST (LumexFormatParseTest, GivenErrorInLaterField_WhenFormat_ThenPositionSet)
{
  try
    {
      (void)runtime_format ("ab {} {:d}", 1, "x");
      FAIL () << "no exception";
    }
  catch (fmt::FormatError const &error)
    {
      EXPECT_STREQ (error.what (), "invalid format specifier");
      EXPECT_EQ (error.position (), 6u);
    }
}

TEST (LumexFormatParseTest, GivenFormatError_WhenCaughtAsBase_ThenRuntimeError)
{
  EXPECT_THROW ((void)runtime_format ("{"), std::runtime_error);
  EXPECT_THROW ((void)runtime_format ("{"), fmt::FormatError);
}
