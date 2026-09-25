// LumexFormatEdgeCases.tests.cpp
// Edge cases found by the adversarial pass of the cyclic verification
// (checked against MSVC std::format where it behaves correctly): grapheme
// cluster width, the range of `{:c}`, `L` with non-decimal bases, duration
// extremes, far dates, huge widths without allocation, exceptions from user
// formatters, malformed replacement fields.
#include <chrono>
#include <climits>
#include <limits>
#include <locale>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/fmt/LumexFormat"
#include "lumex/tests/core/fmt/LumexFormatTestHelpers.hpp"

namespace fmt = lumex::core::fmt;

using format_test_helpers::format_error;
using format_test_helpers::runtime_format;

namespace
{
/** Grouping by one digit with ',' (any base). */
class OneDigitGroups : public std::numpunct<char>
{
protected:
  char
  do_thousands_sep () const override
  {
    return ',';
  }

  std::string
  do_grouping () const override
  {
    return "\1";
  }
};

/** Indian style: 3 digits, then groups of 2. */
class IndianGroups : public std::numpunct<char>
{
protected:
  char
  do_thousands_sep () const override
  {
    return ',';
  }

  std::string
  do_grouping () const override
  {
    return "\3\2";
  }
};

struct throws_int_t
{
};

struct throws_std_t
{
};
} // namespace

namespace lumex
{
namespace core
{
namespace fmt
{
/** Throws something that is not a std::exception. */
template <> class Formatter<throws_int_t>
{
public:
  char const *
  parse (FormatParseContext &ctx)
  {
    return ctx.begin ();
  }

  BasicAppender<char>
  format (throws_int_t, FormatContext &) const
  {
    throw 42;
  }
};

template <> class Formatter<throws_std_t>
{
public:
  char const *
  parse (FormatParseContext &ctx)
  {
    return ctx.begin ();
  }

  BasicAppender<char>
  format (throws_std_t, FormatContext &) const
  {
    throw std::logic_error ("from the user formatter");
  }
};
} // namespace fmt
} // namespace core
} // namespace lumex

// ---------------------------------------------------------------------------
// Grapheme clusters
// ---------------------------------------------------------------------------

TEST (LumexFormatEdgeCasesTest, GivenCombiningMark_WhenPadded_ThenOneColumn)
{
  // "e" + U+0301 COMBINING ACUTE ACCENT is one cluster of width 1.
  EXPECT_EQ (fmt::format ("{:3}", "e\xCC\x81"), "e\xCC\x81  ");
  EXPECT_EQ (fmt::format ("{:.1}", "e\xCC\x81x"), "e\xCC\x81");
  // Devanagari KA + VIRAMA + SSA: marks join the base letter.
  EXPECT_EQ (fmt::format ("{:6}", "\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\xB7"),
             "\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\xB7    ");
}

TEST (LumexFormatEdgeCasesTest, GivenEmojiSequences_WhenPadded_ThenOneCluster)
{
  // MAN + ZWJ + WOMAN: one wide cluster.
  std::string const family = "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9";
  EXPECT_EQ (fmt::format ("{:6}", family), family + "    ");
  EXPECT_EQ (fmt::format ("{:.2}", family + "z"), family);
  // THUMBS UP + skin tone modifier; HEART + variation selector 16.
  EXPECT_EQ (fmt::format ("{:4}", "\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD"),
             "\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD  ");
  EXPECT_EQ (fmt::format ("{:4}", "\xE2\x9D\xA4\xEF\xB8\x8F"),
             "\xE2\x9D\xA4\xEF\xB8\x8F   ");
}

TEST (LumexFormatEdgeCasesTest,
      GivenRegionalIndicators_WhenPadded_ThenPairsJoin)
{
  std::string const flag = "\xF0\x9F\x87\xBA\xF0\x9F\x87\xA6";
  std::string const half = "\xF0\x9F\x87\xBA";
  EXPECT_EQ (fmt::format ("{:5}", flag), flag + "    ");
  // A third indicator starts a new cluster.
  EXPECT_EQ (fmt::format ("{:5}", flag + half), flag + half + "   ");
}

TEST (LumexFormatEdgeCasesTest,
      GivenCrLfAndInvalidUnits_WhenPadded_ThenCounted)
{
  EXPECT_EQ (fmt::format ("{:4}", "a\r\n"), "a\r\n  ");
  // Invalid UTF-8 units count one column each, and are never split.
  EXPECT_EQ (fmt::format ("{:5}", "\xFF\xFE"), "\xFF\xFE   ");
  EXPECT_EQ (fmt::format ("{:.1}", "\xFF\xFE"), "\xFF");
}

// ---------------------------------------------------------------------------
// Integers
// ---------------------------------------------------------------------------

TEST (LumexFormatEdgeCasesTest,
      GivenLocalizedOtherBases_WhenFormat_ThenGrouped)
{
  std::locale const one (std::locale::classic (), new OneDigitGroups);
  EXPECT_EQ (fmt::format (one, "{:Lx}", 0x12345), "1,2,3,4,5");
  EXPECT_EQ (fmt::format (one, "{:#Lx}", 0x123), "0x1,2,3");
  EXPECT_EQ (fmt::format (one, "{:Lb}", 5), "1,0,1");
  EXPECT_EQ (fmt::format (one, "{:08L}", 1234), "01,2,3,4");
  EXPECT_EQ (fmt::format (one, "{:L}", -12345), "-1,2,3,4,5");
  std::locale const indian (std::locale::classic (), new IndianGroups);
  EXPECT_EQ (fmt::format (indian, "{:L}", 1234567890), "1,23,45,67,890");
}

TEST (LumexFormatEdgeCasesTest,
      GivenExtremeIntegers_WhenPrefixedAndPadded_ThenExact)
{
  EXPECT_EQ (fmt::format ("{:+#b}", INT_MIN),
             "-0b10000000000000000000000000000000");
  EXPECT_EQ (fmt::format ("{:#o}", LLONG_MIN), "-01000000000000000000000");
  EXPECT_EQ (fmt::format ("{:#X}", LLONG_MIN), "-0X8000000000000000");
  EXPECT_EQ (fmt::format ("{:020}", LLONG_MIN), "-9223372036854775808");
  EXPECT_EQ (fmt::format ("{:=^+#012x}", 255), "===+0xff====");
}

// ---------------------------------------------------------------------------
// Sizes: the width is counted, not allocated, where the output is bounded
// ---------------------------------------------------------------------------

TEST (LumexFormatEdgeCasesTest,
      GivenHugeWidth_WhenSizeOrBoundedOutput_ThenNoBuffer)
{
  EXPECT_EQ (fmt::formatted_size ("{:{}}", 1, 100000000), 100000000u);
  char small[4] = {};
  fmt::format_to_n_result_t<char *> const result
      = fmt::format_to_n (small, 4, "{:>{}}", 1, 100000000);
  EXPECT_EQ (result.size, 100000000);
  EXPECT_EQ (std::string (small, 4), "    ");
}

// ---------------------------------------------------------------------------
// Malformed fields
// ---------------------------------------------------------------------------

TEST (LumexFormatEdgeCasesTest, GivenSpacesOrJunkInFields_WhenFormat_ThenError)
{
  EXPECT_EQ (format_error ("{ 0}", 1), "invalid format string");
  EXPECT_EQ (format_error ("{0 }", 1), "invalid format string");
  EXPECT_EQ (format_error ("{1a}", 1), "invalid format string");
  EXPECT_NE (format_error ("{a b}", 1), "<no error>");
  EXPECT_EQ (format_error ("{:}}", 1), "unmatched '}' in format string");
  EXPECT_EQ (format_error ("{:{{}}", 1), "invalid format string");
  // A width never starts with 0 (after the 0 flag): std::format rejects it.
  EXPECT_EQ (format_error ("{:00}", 1), "invalid format specifier");
  EXPECT_EQ (format_error ("{:<005}", 1), "invalid format specifier");
  EXPECT_EQ (runtime_format ("{:010}", 1), "0000000001");
  EXPECT_EQ (runtime_format ("{0:{0}}", 5), "    5");
  EXPECT_EQ (runtime_format ("{::<5}", 1), "1::::");
  EXPECT_EQ (runtime_format ("{:\t>5}", 1), "\t\t\t\t1");
}

// ---------------------------------------------------------------------------
// Exceptions from user formatters
// ---------------------------------------------------------------------------

TEST (LumexFormatEdgeCasesTest,
      GivenUserFormatterThrows_WhenFormat_ThenPropagated)
{
  EXPECT_THROW (fmt::format ("{}", throws_int_t ()), int);
  EXPECT_THROW (fmt::format ("{}", throws_std_t ()), std::logic_error);
}

TEST (LumexFormatEdgeCasesTest,
      GivenUserFormatterThrows_WhenTryFormat_ThenCaught)
{
  fmt::try_format_result_t<char> const from_std
      = fmt::try_format ("{}", throws_std_t ());
  EXPECT_FALSE (from_std.success);
  EXPECT_EQ (from_std.error, "from the user formatter");
  fmt::try_format_result_t<char> const from_int
      = fmt::try_format ("{}", throws_int_t ());
  EXPECT_FALSE (from_int.success);
  EXPECT_FALSE (from_int.error.empty ());
}

// ---------------------------------------------------------------------------
// Chrono extremes
// ---------------------------------------------------------------------------

TEST (LumexFormatEdgeCasesTest,
      GivenDurationMin_WhenTimeSpecs_ThenOneMinusNoOverflow)
{
  // Negating duration::min () overflows; the fields are taken from the
  // signed parts instead (MSVC's std::format prints "--2562047:-47:...").
  EXPECT_EQ (fmt::format ("{:%T}", std::chrono::nanoseconds::min ()),
             "-2562047:47:16.854775808");
  EXPECT_EQ (fmt::format ("{:%T}", std::chrono::nanoseconds::max ()),
             "2562047:47:16.854775807");
  EXPECT_EQ (fmt::format ("{:%T}", std::chrono::duration<int> (INT_MIN)),
             "-596523:14:08");
  EXPECT_EQ (fmt::format ("{:%Q}", std::chrono::duration<int> (INT_MIN)),
             "-2147483648");
  EXPECT_EQ (fmt::format ("{}", std::chrono::nanoseconds::min ()),
             "-9223372036854775808ns");
}

TEST (LumexFormatEdgeCasesTest,
      GivenHugeFloatingDuration_WhenTimeSpecs_ThenError)
{
  EXPECT_EQ (format_error ("{:%T}", std::chrono::duration<double> (1e300)),
             "duration is out of range for chrono-specs");
  EXPECT_EQ (
      format_error ("{:%T}", std::chrono::duration<double> (
                                 std::numeric_limits<double>::quiet_NaN ())),
      "duration is out of range for chrono-specs");
  // Without chrono-specs the count itself prints fine.
  EXPECT_EQ (fmt::format ("{}", std::chrono::duration<double> (
                                    std::numeric_limits<double>::infinity ())),
             "infs");
  EXPECT_EQ (fmt::format ("{:%T}", std::chrono::duration<unsigned> (3725u)),
             "01:02:05");
}

TEST (LumexFormatEdgeCasesTest,
      GivenFarTimePoints_WhenDate_ThenProlepticGregorian)
{
  // Checked against an independent 400-year-cycle computation; MSVC's
  // std::format prints garbage for both.
  typedef std::chrono::time_point<std::chrono::system_clock,
                                  std::chrono::seconds>
      sys_seconds_t;
  typedef std::chrono::duration<long long, std::ratio<86400>> days_t;
  typedef std::chrono::time_point<std::chrono::system_clock, days_t>
      sys_days_t;
  EXPECT_EQ (fmt::format ("{:%F %T}", sys_seconds_t (std::chrono::seconds (
                                          LLONG_MAX / 2))),
             "146138514283-06-19 07:45:03");
  EXPECT_EQ (fmt::format ("{:%F}", sys_days_t (days_t (INT_MIN / 2))),
             "-2937836-09-26");
}

// ---------------------------------------------------------------------------
// Ranges and tuples
// ---------------------------------------------------------------------------

TEST (LumexFormatEdgeCasesTest,
      GivenReferenceTuplesAndEmptyNests_WhenFormat_ThenOk)
{
  int number = 3;
  std::string const text = "x";
  EXPECT_EQ (fmt::format ("{}", std::tie (number, text)), "(3, \"x\")");
  std::vector<std::vector<std::string>> const empty_nested (2);
  EXPECT_EQ (fmt::format ("{}", empty_nested), "[[], []]");
  std::map<int, std::map<int, int>> const nested_map = { { 1, {} } };
  EXPECT_EQ (fmt::format ("{}", nested_map), "{1: {}}");
  EXPECT_EQ (fmt::format ("{:?s}", std::vector<char> ()), "\"\"");
  std::vector<void const *> const pointers (1, nullptr);
  EXPECT_EQ (fmt::format ("{}", pointers), "[0x0]");
}

// ---------------------------------------------------------------------------
// Re-parsing a formatter (the underlying () customization pattern)
// ---------------------------------------------------------------------------

TEST (LumexFormatEdgeCasesTest,
      GivenFormatterParsedTwice_WhenFormat_ThenOnlyLastSpec)
{
  // A range formatter parses its element formatter with the default
  // specification first; a user formatter may then give the element
  // formatter its own specification. Nothing of the first parse may leak.
  fmt::Formatter<std::vector<int>> range;
  fmt::FormatParseContext first (">8}");
  range.parse (first);
  fmt::FormatParseContext element_first ("*>9}");
  range.underlying ().parse (element_first);
  fmt::FormatParseContext element_second ("#x}");
  range.underlying ().parse (element_second);

  std::string text;
  fmt::Detail::StringBuffer<char> buffer (text);
  fmt::FormatArgs const no_args;
  fmt::FormatContext ctx (buffer, no_args, nullptr);
  range.format (std::vector<int>{ 10, 11 }, ctx);
  EXPECT_EQ (text, "[0xa, 0xb]");
}
