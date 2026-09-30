// LumexQuote.tests.cpp
#include <array>
#include <list>
#include <set>
#include <string>
#include <vector>
#if __cplusplus >= 202002L
#include <ranges>
#include <string_view>
#endif

#include <gtest/gtest.h>

#include "lumex/core/string/text/LumexQuote.hpp"
#include "lumex/core/string_view/view/LumexStringView.hpp"

using lumex::core::string::text::quote;
using lumex::core::string::text::quote_double;
using lumex::core::string::text::quote_single;

TEST (LumexQuoteTest, GivenEmptyRange_WhenQuoteAny_ThenEmptyString)
{
  std::vector<std::string> const empty;
  EXPECT_EQ (quote (empty, ", "), "");
  EXPECT_EQ (quote_double (empty, ", "), "");
  EXPECT_EQ (quote_single (empty, ", "), "");
}

TEST (LumexQuoteTest, GivenSingleElement_WhenQuote_ThenWrappedNoSeparator)
{
  std::vector<std::string> const single (1, "COM1");
  EXPECT_EQ (quote (single, ", "), "\"COM1\"");
  EXPECT_EQ (quote_single (single, ", "), "'COM1'");
}

TEST (LumexQuoteTest, GivenSeveralElements_WhenQuote_ThenEachWrapped)
{
  std::vector<std::string> const ports = { "COM1", "COM2", "COM3", "COM4" };
  EXPECT_EQ (quote (ports, ", "), "\"COM1\", \"COM2\", \"COM3\", \"COM4\"");
}

TEST (LumexQuoteTest, GivenSeveralElements_WhenQuoteSingle_ThenEachWrapped)
{
  std::vector<std::string> const ports = { "COM1", "COM2" };
  EXPECT_EQ (quote_single (ports, ", "), "'COM1', 'COM2'");
}

TEST (LumexQuoteTest, GivenAnyInput_WhenQuoteDouble_ThenSameAsQuote)
{
  std::vector<std::string> const parts = { "a", "b" };
  EXPECT_EQ (quote_double (parts, "|"), quote (parts, "|"));
  EXPECT_EQ (quote_double (parts, "|"), "\"a\"|\"b\"");
}

TEST (LumexQuoteTest, GivenEmptyStringElements_WhenQuote_ThenEmptyQuotePairs)
{
  std::vector<std::string> const parts = { "", "" };
  EXPECT_EQ (quote (parts, ","), "\"\",\"\"");
  EXPECT_EQ (quote_single (parts, ","), "'',''");
}

TEST (LumexQuoteTest, GivenEmbeddedQuotes_WhenQuote_ThenNotEscaped)
{
  std::vector<std::string> const parts = { "say \"hi\"", "it's" };
  EXPECT_EQ (quote (parts, " "), "\"say \"hi\"\" \"it's\"");
  EXPECT_EQ (quote_single (parts, " "), "'say \"hi\"' 'it's'");
}

TEST (LumexQuoteTest, GivenEmptySeparator_WhenQuote_ThenAdjacentPairs)
{
  std::vector<std::string> const parts = { "a", "b" };
  EXPECT_EQ (quote (parts, ""), "\"a\"\"b\"");
}

TEST (LumexQuoteTest, GivenCharPointerElements_WhenQuote_ThenConvertedToText)
{
  std::vector<char const *> const words = { "alpha", "beta" };
  EXPECT_EQ (quote (words, " "), "\"alpha\" \"beta\"");
  EXPECT_EQ (quote_single (words, " "), "'alpha' 'beta'");
}

TEST (LumexQuoteTest, GivenOtherContainers_WhenQuote_ThenSameText)
{
  std::set<std::string> const as_set = { "b", "a" };
  std::list<std::string> const as_list = { "a", "b" };
  std::array<std::string, 2> const as_array = { { "a", "b" } };
  std::string const expected ("\"a\",\"b\"");
  EXPECT_EQ (quote (as_set, ","), expected);
  EXPECT_EQ (quote (as_list, ","), expected);
  EXPECT_EQ (quote (as_array, ","), expected);
}

TEST (LumexQuoteTest, GivenCArrayOfStrings_WhenQuote_ThenJoined)
{
  std::string const parts[] = { "x", "y", "z" };
  EXPECT_EQ (quote_single (parts, "-"), "'x'-'y'-'z'");
}

TEST (LumexQuoteTest, GivenStdStringSeparator_WhenQuote_ThenUsed)
{
  std::vector<std::string> const parts = { "a", "b" };
  std::string const separator (" | ");
  EXPECT_EQ (quote (parts, separator), "\"a\" | \"b\"");
}

#if __cplusplus < 202002L

TEST (LumexQuoteTest, GivenCharSeparator_WhenQuotePreCxx20_ThenUsed)
{
  std::vector<std::string> const parts = { "a", "b" };
  EXPECT_EQ (quote (parts, ','), "\"a\",\"b\"");
  EXPECT_EQ (quote_single (parts, ','), "'a','b'");
}

TEST (LumexQuoteTest, GivenLumexStringViewSeparator_WhenQuotePreCxx20_ThenUsed)
{
  std::vector<std::string> const parts = { "a", "b" };
  lumex::core::string_view::view::LumexStringView const separator ("; ", 2);
  EXPECT_EQ (quote_double (parts, separator), "\"a\"; \"b\"");
}

#else

TEST (LumexQuoteTest, GivenStringViewSeparator_WhenQuote_ThenUsed)
{
  std::vector<std::string> const parts = { "a", "b" };
  std::string_view const separator ("; ");
  EXPECT_EQ (quote (parts, separator), "\"a\"; \"b\"");
}

TEST (LumexQuoteTest, GivenConstIterableView_WhenQuoteSingle_ThenWalksIt)
{
  std::vector<std::string> const parts = { "a", "b" };
  auto shouted = parts
                 | std::views::transform ([] (std::string const &text)
                                            { return text + "!"; });
  EXPECT_EQ (quote_single (shouted, ","), "'a!','b!'");
}

#endif
