// LumexFormatApi.tests.cpp
// Output API (format_to, format_to_n, formatted_size, vformat, vformat_to,
// try_format, print, println), wide strings, locales, and the extension
// points: reflected enums, user Formatter specializations, OstreamFormatter
// and streamed(). Cases partly ported from fmt's format-test.cc
// (format_to*, formatted_size, output_iterators, vformat_to, format_custom,
// format_to_custom, print, format_examples, format_locale).
#include <cstddef>
#include <iostream>
#include <iterator>
#include <limits>
#include <list>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/fmt/LumexFormat.hpp"
#include "lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp"
#include "lumex/tests/core/fmt/LumexFormatTestHelpers.hpp"

namespace fmt = lumex::core::fmt;

using format_test_helpers::format_error;
using format_test_helpers::runtime_format;
using format_test_helpers::runtime_wformat;

LUMEX_DEFINE_REFLECTED_ENUM (FormatTestColor, unsigned char, (red), (green),
                             (blue))

LUMEX_DEFINE_REFLECTED_ENUM (FormatTestLevel, int, (low, -1), (normal, 0),
                             (high, 10))

namespace
{
struct answer_t
{
};

struct point_t
{
  int x;
  int y;
};

std::ostream &
operator<< (std::ostream &stream, point_t const &point)
{
  return stream << '(' << point.x << ',' << point.y << ')';
}

struct date_t
{
  int year;
  int month;
  int day;
};

struct unformattable_t
{
};

/** Grouping by three with '\'' and a decimal comma. */
class ApostropheNumpunct : public std::numpunct<char>
{
protected:
  char
  do_decimal_point () const override
  {
    return ',';
  }

  char
  do_thousands_sep () const override
  {
    return '\'';
  }

  std::string
  do_grouping () const override
  {
    return "\3";
  }

  std::string
  do_truename () const override
  {
    return "yes";
  }

  std::string
  do_falsename () const override
  {
    return "no";
  }
};

std::locale
apostrophe_locale ()
{
  return std::locale (std::locale::classic (), new ApostropheNumpunct);
}

/** Output container whose growth always fails. */
struct nongrowing_container_t
{
  typedef char value_type;

  void
  push_back (char)
  {
    throw std::runtime_error ("can't take it any more");
  }
};
} // namespace

namespace lumex
{
namespace core
{
namespace fmt
{
/** Reuses the int specification: `{:04}` pads the number. */
template <> class Formatter<answer_t> : public Formatter<int>
{
public:
  BasicAppender<char>
  format (answer_t, FormatContext &ctx) const
  {
    return Formatter<int>::format (42, ctx);
  }
};

/** Own specification: empty or `iso`. */
template <> class Formatter<date_t>
{
public:
  char const *
  parse (FormatParseContext &ctx)
  {
    char const *it = ctx.begin ();
    _iso = false;
    if (it != ctx.end () && *it == 'i')
      {
        if (ctx.end () - it < 3 || it[1] != 's' || it[2] != 'o')
          throw FormatError ("unknown format specifier");
        _iso = true;
        it += 3;
      }
    if (it != ctx.end () && *it != '}')
      throw FormatError ("unknown format specifier");
    return it;
  }

  BasicAppender<char>
  format (date_t const &date, FormatContext &ctx) const
  {
    if (_iso)
      return fmt::format_to (ctx.out (), "{:04}-{:02}-{:02}", date.year,
                             date.month, date.day);
    return fmt::format_to (ctx.out (), "{}/{}/{}", date.day, date.month,
                           date.year);
  }

private:
  bool _iso = false;
};

template <> class Formatter<point_t> : public OstreamFormatter<char>
{
};
} // namespace fmt
} // namespace core
} // namespace lumex

// ---------------------------------------------------------------------------
// format / format_to
// ---------------------------------------------------------------------------

TEST (LumexFormatApiTest, GivenExamples_WhenFormat_ThenDocumentedOutput)
{
  EXPECT_EQ (fmt::format ("The answer is {}", 42), "The answer is 42");
  EXPECT_EQ (fmt::format ("First, thou shalt count to {0}", "three"),
             "First, thou shalt count to three");
  EXPECT_EQ (fmt::format ("Bring me a {}", "shrubbery"),
             "Bring me a shrubbery");
  EXPECT_EQ (fmt::format ("From {} to {}", 1, 3), "From 1 to 3");
  EXPECT_EQ (fmt::format ("{0}, {1}, {2}", 'a', 'b', 'c'), "a, b, c");
}

TEST (LumexFormatApiTest, GivenStringInserter_WhenFormatTo_ThenAppended)
{
  std::string text;
  fmt::format_to (std::back_inserter (text), "part{0}", 1);
  EXPECT_EQ (text, "part1");
  fmt::format_to (std::back_inserter (text), "part{0}", 2);
  EXPECT_EQ (text, "part1part2");
  fmt::format_to (std::back_inserter (text), "test");
  EXPECT_EQ (text, "part1part2test");
}

TEST (LumexFormatApiTest, GivenOtherOutputIterators_WhenFormatTo_ThenWritten)
{
  std::list<char> list;
  fmt::format_to (std::back_inserter (list), "{}", 42);
  EXPECT_EQ (std::string (list.begin (), list.end ()), "42");

  std::vector<char> vector;
  fmt::format_to (std::back_inserter (vector), "{}", "foo");
  EXPECT_EQ (std::string (vector.begin (), vector.end ()), "foo");

  std::stringstream stream;
  fmt::format_to (std::ostream_iterator<char> (stream), "{}", 42);
  EXPECT_EQ (stream.str (), "42");

  std::stringstream stream2;
  fmt::format_to (std::ostreambuf_iterator<char> (stream2), "{}.{:06d}", 42,
                  43);
  EXPECT_EQ (stream2.str (), "42.000043");
}

TEST (LumexFormatApiTest, GivenCharArray_WhenFormatTo_ThenIteratorPastOutput)
{
  char buffer[10] = {};
  char *const end = fmt::format_to (buffer, "{}", answer_t ());
  EXPECT_EQ (end, buffer + 2);
  EXPECT_STREQ (buffer, "42");
}

TEST (LumexFormatApiTest, GivenThrowingContainer_WhenFormatTo_ThenPropagates)
{
  nongrowing_container_t container;
  EXPECT_THROW (fmt::format_to (std::back_inserter (container), "{}", 42),
                std::runtime_error);
}

// ---------------------------------------------------------------------------
// format_to_n / formatted_size
// ---------------------------------------------------------------------------

TEST (LumexFormatApiTest, GivenLimit_WhenFormatToN_ThenTruncatedAndSized)
{
  char buffer[4];
  buffer[3] = 'x';
  fmt::format_to_n_result_t<char *> result
      = fmt::format_to_n (buffer, 3, "{}", 12345);
  EXPECT_EQ (result.size, 5);
  EXPECT_EQ (result.out, buffer + 3);
  EXPECT_EQ (std::string (buffer, 4), "123x");

  result = fmt::format_to_n (buffer, 3, "{:s}", "foobar");
  EXPECT_EQ (result.size, 6);
  EXPECT_EQ (result.out, buffer + 3);
  EXPECT_EQ (std::string (buffer, 4), "foox");

  buffer[0] = 'x';
  buffer[1] = 'x';
  buffer[2] = 'x';
  result = fmt::format_to_n (buffer, 3, "{}", 'A');
  EXPECT_EQ (result.size, 1);
  EXPECT_EQ (result.out, buffer + 1);
  EXPECT_EQ (std::string (buffer, 4), "Axxx");

  result = fmt::format_to_n (buffer, 3, "{}{} ", 'B', 'C');
  EXPECT_EQ (result.size, 3);
  EXPECT_EQ (result.out, buffer + 3);
  EXPECT_EQ (std::string (buffer, 4), "BC x");

  result = fmt::format_to_n (buffer, 4, "{}", "ABCDE");
  EXPECT_EQ (result.size, 5);
  EXPECT_EQ (std::string (buffer, 4), "ABCD");

  buffer[3] = 'x';
  result = fmt::format_to_n (buffer, 3, "{}", std::string (1000, '*'));
  EXPECT_EQ (result.size, 1000);
  EXPECT_EQ (std::string (buffer, 4), "***x");
}

TEST (LumexFormatApiTest, GivenZeroOrNegativeLimit_WhenFormatToN_ThenNothing)
{
  char buffer[2] = { 'x', 'x' };
  fmt::format_to_n_result_t<char *> result
      = fmt::format_to_n (buffer, 0, "{}", 42);
  EXPECT_EQ (result.size, 2);
  EXPECT_EQ (result.out, buffer);
  result = fmt::format_to_n (buffer, -1, "{}", 42);
  EXPECT_EQ (result.size, 2);
  EXPECT_EQ (result.out, buffer);
  EXPECT_EQ (std::string (buffer, 2), "xx");
}

TEST (LumexFormatApiTest, GivenFormat_WhenFormattedSize_ThenOutputLength)
{
  EXPECT_EQ (fmt::formatted_size ("{}", 42), 2u);
  EXPECT_EQ (fmt::formatted_size ("{:10}", 1), 10u);
  EXPECT_EQ (fmt::formatted_size (""), 0u);
  EXPECT_EQ (fmt::formatted_size ("{:.3f}", 3.14159), 5u);
  EXPECT_EQ (fmt::formatted_size ("{}", std::string (1000, '*')), 1000u);
}

// ---------------------------------------------------------------------------
// vformat / vformat_to / runtime strings
// ---------------------------------------------------------------------------

TEST (LumexFormatApiTest, GivenStoredArgs_WhenVformatTwice_ThenSameOutput)
{
  int const n = 42;
  std::string text;
  fmt::vformat_to (std::back_inserter (text), "{}", fmt::make_format_args (n));
  EXPECT_EQ (text, "42");
  text.clear ();
  fmt::vformat_to (std::back_inserter (text), "{:>4}",
                   fmt::make_format_args (n));
  EXPECT_EQ (text, "  42");
  EXPECT_EQ (fmt::vformat ("{} {}", fmt::make_format_args (n, "x")), "42 x");
}

TEST (LumexFormatApiTest, GivenFormatArgs_WhenQueried_ThenSizeAndLookup)
{
  auto const store = fmt::make_format_args (1, fmt::arg ("name", 2));
  fmt::FormatArgs const args = store;
  EXPECT_EQ (args.size (), 2);
}

// ---------------------------------------------------------------------------
// try_format
// ---------------------------------------------------------------------------

TEST (LumexFormatApiTest, GivenValidString_WhenTryFormat_ThenSuccess)
{
  fmt::try_format_result_t<char> const result
      = fmt::try_format ("{} {}", 1, "two");
  EXPECT_TRUE (result.success);
  EXPECT_EQ (result.text, "1 two");
  EXPECT_TRUE (result.error.empty ());
}

TEST (LumexFormatApiTest, GivenBadString_WhenTryFormat_ThenErrorNoThrow)
{
  fmt::try_format_result_t<char> const missing
      = fmt::try_format (fmt::runtime ("{} {}"), 1);
  EXPECT_FALSE (missing.success);
  EXPECT_EQ (missing.error, "argument not found");

  fmt::try_format_result_t<char> const bad_spec
      = fmt::try_format (fmt::runtime ("{:d}"), "text");
  EXPECT_FALSE (bad_spec.success);
  EXPECT_EQ (bad_spec.error, "invalid format specifier");
}

TEST (LumexFormatApiTest, GivenThrowingFormatter_WhenTryFormat_ThenCaught)
{
  // A null C string throws from inside the value formatting.
  char const *const null_string = nullptr;
  fmt::try_format_result_t<char> const result
      = fmt::try_format ("{}", null_string);
  EXPECT_FALSE (result.success);
  EXPECT_EQ (result.error, "string pointer is null");
}

TEST (LumexFormatApiTest, GivenTryFormat_WhenCheckedStatically_ThenNoexcept)
{
  EXPECT_TRUE (noexcept (fmt::try_format (fmt::runtime ("{}"), 1)));
}

// ---------------------------------------------------------------------------
// print / println
// ---------------------------------------------------------------------------

TEST (LumexFormatApiTest, GivenStream_WhenPrint_ThenFormattedTextWritten)
{
  std::ostringstream stream;
  fmt::print (stream, "Don't {}!", "panic");
  EXPECT_EQ (stream.str (), "Don't panic!");
  fmt::println (stream, " {}", 42);
  EXPECT_EQ (stream.str (), "Don't panic! 42\n");
}

TEST (LumexFormatApiTest, GivenWideStream_WhenPrintln_ThenWideTextWritten)
{
  std::wostringstream stream;
  fmt::println (stream, L"{}-{}", 1, L"two");
  EXPECT_EQ (stream.str (), L"1-two\n");
}

TEST (LumexFormatApiTest, GivenStdout_WhenPrint_ThenWrittenToCout)
{
  std::ostringstream captured;
  std::streambuf *const previous = std::cout.rdbuf (captured.rdbuf ());
  fmt::print ("{}", std::numeric_limits<double>::infinity ());
  fmt::println ("");
  std::cout.rdbuf (previous);
  EXPECT_EQ (captured.str (), "inf\n");
}

// ---------------------------------------------------------------------------
// Wide strings
// ---------------------------------------------------------------------------

TEST (LumexFormatApiTest, GivenWideFormatString_WhenFormat_ThenWideString)
{
  EXPECT_EQ (fmt::format (L"{} {:>3}", 1, L"ab"), L"1  ab");
  EXPECT_EQ (fmt::format (L"{:*^7}", L"mid"), L"**mid**");
  EXPECT_EQ (fmt::format (L"{:#x}", 255), L"0xff");
  EXPECT_EQ (fmt::format (L"{}", 1.5), L"1.5");
  EXPECT_EQ (fmt::format (L"{}", true), L"true");
  EXPECT_EQ (fmt::format (L"{}", 'a'), L"a");
  EXPECT_EQ (fmt::format (L"{}", L'b'), L"b");
  EXPECT_EQ (fmt::format (L"{}", std::wstring (L"str")), L"str");
  EXPECT_EQ (fmt::format (L"{name}", fmt::arg (L"name", 7)), L"7");
  EXPECT_EQ (fmt::format (L"{:?}", L"a\tb"), L"\"a\\tb\"");
}

TEST (LumexFormatApiTest, GivenWideErrors_WhenFormat_ThenFormatError)
{
  EXPECT_THROW (runtime_wformat (L"{"), fmt::FormatError);
  EXPECT_THROW (runtime_wformat (L"{:d}", L"x"), fmt::FormatError);
  EXPECT_EQ (runtime_wformat (L"{0}{0}", 5), L"55");
}

TEST (LumexFormatApiTest, GivenWideOutput_WhenFormatToAndSize_ThenConsistent)
{
  std::wstring text;
  fmt::format_to (std::back_inserter (text), L"{}+{}", 1, 2);
  EXPECT_EQ (text, L"1+2");
  EXPECT_EQ (fmt::formatted_size (L"{:5}", 1), 5u);
  wchar_t buffer[3] = {};
  fmt::format_to_n_result_t<wchar_t *> const result
      = fmt::format_to_n (buffer, 2, L"{}", 12345);
  EXPECT_EQ (result.size, 5);
  EXPECT_EQ (std::wstring (buffer, 2), L"12");
}

// ---------------------------------------------------------------------------
// Locales
// ---------------------------------------------------------------------------

TEST (LumexFormatApiTest, GivenLocaleWithGrouping_WhenLocalized_ThenGrouped)
{
  std::locale const locale = apostrophe_locale ();
  EXPECT_EQ (fmt::format (locale, "{:L}", 1234567), "1'234'567");
  EXPECT_EQ (fmt::format (locale, "{:L}", -1234567), "-1'234'567");
  EXPECT_EQ (fmt::format (locale, "{:L}", 123), "123");
  EXPECT_EQ (fmt::format (locale, "{:L}", 1234567.25), "1'234'567,25");
  EXPECT_EQ (fmt::format (locale, "{:.2Lf}", 1234.5), "1'234,50");
  EXPECT_EQ (fmt::format (locale, "{:L}", true), "yes");
  EXPECT_EQ (fmt::format (locale, "{:L}", false), "no");
  EXPECT_EQ (fmt::format (locale, "{:>12L}", 1234567), "   1'234'567");
}

TEST (LumexFormatApiTest, GivenLocaleWithoutL_WhenFormat_ThenLocaleIgnored)
{
  std::locale const locale = apostrophe_locale ();
  EXPECT_EQ (fmt::format (locale, "{}", 1234567), "1234567");
  EXPECT_EQ (fmt::format (locale, "{}", 1.5), "1.5");
  EXPECT_EQ (fmt::format (locale, "{}", true), "true");
}

TEST (LumexFormatApiTest, GivenLocale_WhenFormatToAndNamedArgs_ThenApplied)
{
  std::locale const locale = apostrophe_locale ();
  std::string text;
  fmt::format_to (std::back_inserter (text), locale, "{:L}", 1000);
  EXPECT_EQ (text, "1'000");
  EXPECT_EQ (fmt::format (locale, "{n:L}", fmt::arg ("n", 1000)), "1'000");
  EXPECT_EQ (fmt::vformat (locale, "{:L}", fmt::make_format_args (2000)),
             "2'000");
}

// ---------------------------------------------------------------------------
// Reflected enums
// ---------------------------------------------------------------------------

TEST (LumexFormatApiTest, GivenReflectedEnum_WhenFormat_ThenName)
{
  EXPECT_EQ (fmt::format ("{}", FormatTestColor::green), "green");
  EXPECT_EQ (fmt::format ("{:s}", FormatTestColor::blue), "blue");
  EXPECT_EQ (fmt::format ("{:>6}", FormatTestColor::red), "   red");
  EXPECT_EQ (fmt::format ("{:*<7}", FormatTestColor::red), "red****");
  EXPECT_EQ (fmt::format ("{:.2}", FormatTestColor::green), "gr");
}

TEST (LumexFormatApiTest, GivenReflectedEnum_WhenIntegerType_ThenValue)
{
  EXPECT_EQ (fmt::format ("{:d}", FormatTestColor::blue), "2");
  EXPECT_EQ (fmt::format ("{:#x}", FormatTestLevel::high), "0xa");
  EXPECT_EQ (fmt::format ("{:d}", FormatTestLevel::low), "-1");
  EXPECT_EQ (fmt::format ("{:+d}", FormatTestLevel::normal), "+0");
  EXPECT_EQ (fmt::format ("{:03d}", FormatTestColor::green), "001");
}

TEST (LumexFormatApiTest, GivenUnknownEnumValue_WhenFormat_ThenUnknownText)
{
  FormatTestColor const unknown = static_cast<FormatTestColor> (42);
  EXPECT_EQ (fmt::format ("{}", unknown), "<Unknown>");
  EXPECT_EQ (fmt::format ("{:d}", unknown), "42");
}

TEST (LumexFormatApiTest, GivenReflectedEnumBadSpec_WhenFormat_ThenFormatError)
{
  EXPECT_EQ (format_error ("{:f}", FormatTestColor::red),
             "invalid format specifier");
  EXPECT_EQ (format_error ("{:+}", FormatTestColor::red),
             "invalid format specifier");
  EXPECT_EQ (format_error ("{:.2d}", FormatTestColor::red),
             "invalid format specifier");
}

// ---------------------------------------------------------------------------
// User formatters
// ---------------------------------------------------------------------------

TEST (LumexFormatApiTest, GivenFormatterInheritingInt_WhenFormat_ThenIntSpecs)
{
  EXPECT_EQ (fmt::format ("{0}", answer_t ()), "42");
  EXPECT_EQ (fmt::format ("{:04}", answer_t ()), "0042");
  EXPECT_EQ (fmt::format ("{:#x}", answer_t ()), "0x2a");
  EXPECT_EQ (format_error ("{:s}", answer_t ()), "invalid format specifier");
}

TEST (LumexFormatApiTest, GivenFormatterWithOwnParse_WhenFormat_ThenItsSpecs)
{
  date_t const date = { 2012, 12, 9 };
  EXPECT_EQ (fmt::format ("{}", date), "9/12/2012");
  EXPECT_EQ (fmt::format ("{:iso}", date), "2012-12-09");
  EXPECT_EQ (format_error ("{:s}", date), "unknown format specifier");
  EXPECT_EQ (format_error ("{:isox}", date), "unknown format specifier");
}

TEST (LumexFormatApiTest, GivenOstreamFormatter_WhenFormat_ThenStreamedText)
{
  point_t const point = { 1, 2 };
  EXPECT_EQ (fmt::format ("{}", point), "(1,2)");
  EXPECT_EQ (fmt::format ("{:>7}", point), "  (1,2)");
  EXPECT_EQ (fmt::format ("{:.3}", point), "(1,");
}

TEST (LumexFormatApiTest, GivenStreamedWrapper_WhenFormat_ThenOperatorUsed)
{
  EXPECT_EQ (fmt::format ("{:*^9}", fmt::streamed (point_t{ 3, 4 })),
             "**(3,4)**");
  EXPECT_EQ (fmt::format ("{}", fmt::streamed (std::string ("s"))), "s");
}

TEST (LumexFormatApiTest, GivenTypeTraits_WhenFormattable_ThenFormatterUsable)
{
  EXPECT_TRUE ((std::is_default_constructible<fmt::Formatter<int>>::value));
  EXPECT_TRUE (
      (std::is_default_constructible<fmt::Formatter<answer_t>>::value));
  EXPECT_FALSE (
      (std::is_default_constructible<fmt::Formatter<unformattable_t>>::value));
}

// ---------------------------------------------------------------------------
// Exceptions
// ---------------------------------------------------------------------------

TEST (LumexFormatApiTest, GivenFormatError_WhenConstructed_ThenMessageAndPos)
{
  fmt::FormatError const plain ("plain");
  EXPECT_STREQ (plain.what (), "plain");
  EXPECT_EQ (plain.position (), fmt::FormatError::no_position ());

  fmt::FormatError const positioned (std::string ("at"), 3u);
  EXPECT_STREQ (positioned.what (), "at");
  EXPECT_EQ (positioned.position (), 3u);

  std::string moved = "moved";
  fmt::FormatError const from_rvalue (std::move (moved));
  EXPECT_STREQ (from_rvalue.what (), "moved");
}
