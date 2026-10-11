#include <cstddef>
#include <iomanip>
#include <ios>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "LumexStringViewTestSupport.hpp"
#include "lumex/core/string_view/LumexStringView"

// Differential tests against std::basic_string_view: every query of the view
// on every short text of an alphabet with a NUL and a high-bit byte, and on
// random texts, must give what the standard view gives. (The searches are
// also checked against std::basic_string in LumexStringViewSearch.cxx11; here
// the oracle is the standard view itself, with the conversions both ways.)

namespace
{
template <class T>
void
sink (T const &)
{
}
using lumex_sv_test::exact_copy;
using lumex_sv_test::random_source;

int
sign (int value)
{
  return value < 0 ? -1 : (value > 0 ? 1 : 0);
}

template <class CharT>
bool
same_query_results (std::basic_string<CharT> const &text,
                    std::basic_string<CharT> const &other)
{
  typedef typename lumex_sv_test::char_set<CharT>::view_t view_t;
  typedef std::basic_string_view<CharT> std_view_t;
  exact_copy<CharT> text_copy (text);
  exact_copy<CharT> other_copy (other);
  view_t const view (text_copy.data (), text_copy.size ());
  view_t const needle (other_copy.data (), other_copy.size ());
  std_view_t const std_view (text_copy.data (), text_copy.size ());
  std_view_t const std_needle (other_copy.data (), other_copy.size ());
  bool ok = true;
  std::vector<std::size_t> const where
      = lumex_sv_test::positions (text.size ());
  for (std::size_t i = 0; i < where.size (); ++i)
    {
      std::size_t const pos = where[i];
      ok = ok && view.find (needle, pos) == std_view.find (std_needle, pos);
      ok = ok && view.rfind (needle, pos) == std_view.rfind (std_needle, pos);
      ok = ok
           && view.find_first_of (needle, pos)
                  == std_view.find_first_of (std_needle, pos);
      ok = ok
           && view.find_last_of (needle, pos)
                  == std_view.find_last_of (std_needle, pos);
      ok = ok
           && view.find_first_not_of (needle, pos)
                  == std_view.find_first_not_of (std_needle, pos);
      ok = ok
           && view.find_last_not_of (needle, pos)
                  == std_view.find_last_not_of (std_needle, pos);
      if (!other.empty ())
        {
          CharT const chr = other[0];
          ok = ok && view.find (chr, pos) == std_view.find (chr, pos);
          ok = ok && view.rfind (chr, pos) == std_view.rfind (chr, pos);
          ok = ok
               && view.find_first_not_of (chr, pos)
                      == std_view.find_first_not_of (chr, pos);
          ok = ok
               && view.find_last_not_of (chr, pos)
                      == std_view.find_last_not_of (chr, pos);
        }
      std::size_t const length = pos < text.size () + 2 ? pos : 2;
      if (pos <= text.size ())
        {
          ok = ok
               && std_view_t (view.substr (pos, length))
                      == std_view.substr (pos, length);
        }
      else
        {
          bool view_threw = false;
          bool std_threw = false;
          try
            {
              sink (view.substr (pos, length));
            }
          catch (std::out_of_range const &)
            {
              view_threw = true;
            }
          try
            {
              sink (std_view.substr (pos, length));
            }
          catch (std::out_of_range const &)
            {
              std_threw = true;
            }
          ok = ok && view_threw && std_threw;
        }
    }
  ok = ok
       && sign (view.compare (needle)) == sign (std_view.compare (std_needle));
  ok = ok && (view == needle) == (std_view == std_needle);
  ok = ok && (view < needle) == (std_view < std_needle);
  ok = ok && (view >= needle) == (std_view >= std_needle);
  // the mixed comparisons, both orders
  ok = ok && (view == std_needle) == (std_view == std_needle);
  ok = ok && (std_view == needle) == (std_view == std_needle);
  ok = ok && (view != std_needle) == (std_view != std_needle);
  ok = ok && (view < std_needle) == (std_view < std_needle);
  ok = ok && (std_view < needle) == (std_view < std_needle);
  ok = ok && (view > std_needle) == (std_view > std_needle);
  ok = ok && (std_view <= needle) == (std_view <= std_needle);
  ok = ok && (view >= std_needle) == (std_view >= std_needle);
  // starts_with and ends_with are C++20 in the standard: the reference is
  // the standard view's own substr and compare.
  bool const std_starts
      = std_view.size () >= std_needle.size ()
        && std_view.substr (0, std_needle.size ()) == std_needle;
  bool const std_ends
      = std_view.size () >= std_needle.size ()
        && std_view.substr (std_view.size () - std_needle.size ())
               == std_needle;
  ok = ok && view.starts_with (needle) == std_starts;
  ok = ok && view.ends_with (needle) == std_ends;
  ok = ok
       && view.contains (needle)
              == (std_view.find (std_needle) != std_view_t::npos);
  // the conversions
  std_view_t const converted = view;
  ok = ok && converted.data () == std_view.data ()
       && converted.size () == std_view.size ();
  view_t const back = converted;
  ok = ok && back.data () == view.data () && back.size () == view.size ();
  return ok;
}

template <class CharT>
void
exhaustive (std::size_t text_length, std::size_t needle_length)
{
  std::vector<CharT> const alphabet
      = lumex_sv_test::char_set<CharT>::alphabet ();
  std::vector<std::basic_string<CharT>> const texts
      = lumex_sv_test::all_texts (alphabet, text_length);
  std::vector<std::basic_string<CharT>> const needles
      = lumex_sv_test::all_texts (alphabet, needle_length);
  int mismatches = 0;
  for (std::size_t t = 0; t < texts.size (); ++t)
    for (std::size_t n = 0; n < needles.size (); ++n)
      if (!same_query_results<CharT> (texts[t], needles[n])
          && ++mismatches < 10)
        ADD_FAILURE () << "text " << t << ", needle " << n;
  EXPECT_EQ (mismatches, 0);
}

template <class CharT>
void
formatting_probe ()
{
  typedef typename lumex_sv_test::char_set<CharT>::view_t view_t;
  typedef std::basic_ostringstream<CharT> stream_t;
  std::vector<CharT> const alphabet
      = lumex_sv_test::char_set<CharT>::alphabet ();
  std::vector<std::basic_string<CharT>> const texts
      = lumex_sv_test::all_texts (alphabet, 2);
  int mismatches = 0;
  for (std::size_t t = 0; t < texts.size (); ++t)
    for (int width = 0; width <= 4; ++width)
      for (int adjust = 0; adjust < 4; ++adjust)
        {
          std::ios_base::fmtflags const adjusts[]
              = { std::ios_base::left, std::ios_base::right,
                  std::ios_base::internal, std::ios_base::fmtflags () };
          stream_t viewed;
          stream_t reference;
          viewed.setf (adjusts[adjust], std::ios_base::adjustfield);
          reference.setf (adjusts[adjust], std::ios_base::adjustfield);
          viewed << CharT ('[') << std::setw (width)
                 << std::setfill (CharT ('*')) << view_t (texts[t])
                 << CharT (']') << view_t (texts[t]);
          reference << CharT ('[') << std::setw (width)
                    << std::setfill (CharT ('*'))
                    << std::basic_string_view<CharT> (texts[t].data (),
                                                      texts[t].size ())
                    << CharT (']')
                    << std::basic_string_view<CharT> (texts[t].data (),
                                                      texts[t].size ());
          if (viewed.str () != reference.str () && ++mismatches < 10)
            ADD_FAILURE () << "text " << t << ", width " << width
                           << ", adjust " << adjust;
        }
  EXPECT_EQ (mismatches, 0);
}
} // namespace

TEST (LumexStringViewStdDifferentialTest,
      GivenEveryNarrowText_WhenQuery_ThenSameAsStdStringView)
{
  exhaustive<char> (4, 3);
}

TEST (LumexStringViewStdDifferentialTest,
      GivenEveryWideText_WhenQuery_ThenSameAsStdWstringView)
{
  exhaustive<wchar_t> (3, 3);
}

TEST (LumexStringViewStdDifferentialTest,
      GivenRandomTexts_WhenQuery_ThenSameAsStdStringView)
{
  random_source random (0x77aa55ccU);
  int mismatches = 0;
  std::vector<char> const alphabet
      = lumex_sv_test::char_set<char>::alphabet ();
  for (int round = 0; round < 600; ++round)
    {
      std::string text;
      std::size_t const length = random.below (120);
      for (std::size_t i = 0; i < length; ++i)
        text.push_back (alphabet[random.below (3)]);
      std::string other;
      if (!text.empty () && random.below (3) != 0)
        {
          std::size_t const from = random.below (text.size ());
          other = text.substr (from, random.below (6));
        }
      else
        other.push_back (alphabet[random.below (4)]);
      if (!same_query_results<char> (text, other) && ++mismatches < 10)
        ADD_FAILURE () << "round " << round;
    }
  EXPECT_EQ (mismatches, 0);
}

TEST (LumexStringViewStdDifferentialTest,
      GivenSetwSetfillProbe_WhenInsertNarrow_ThenSameAsStdStringView)
{
  formatting_probe<char> ();
  std::ostringstream viewed;
  viewed << '[' << std::setw (8) << std::setfill ('*')
         << lumex_string_view ("ab") << ']';
  std::ostringstream reference;
  reference << '[' << std::setw (8) << std::setfill ('*')
            << std::string_view ("ab") << ']';
  EXPECT_EQ (viewed.str (), reference.str ());
  EXPECT_EQ (viewed.str (), "[******ab]");
}

TEST (LumexStringViewStdDifferentialTest,
      GivenSetwSetfillProbe_WhenInsertWide_ThenSameAsStdWstringView)
{
  formatting_probe<wchar_t> ();
  std::wostringstream viewed;
  viewed << L'[' << std::setw (8) << std::setfill (L'*')
         << lumex_wstring_view (L"ab") << L']';
  std::wostringstream reference;
  reference << L'[' << std::setw (8) << std::setfill (L'*')
            << std::wstring_view (L"ab") << L']';
  EXPECT_EQ (viewed.str (), reference.str ());
  EXPECT_EQ (viewed.str (), L"[******ab]");
}

TEST (LumexStringViewStdDifferentialTest,
      GivenCompareOverloads_WhenCall_ThenSameAsStdStringView)
{
  std::string const text = "hello world";
  std::string const other = "xworld!";
  lumex_string_view const view (text);
  std::string_view const std_view (text);
  std::size_t const bounds[]
      = { 0, 1, 3, 6, 11, 12, static_cast<std::size_t> (-1) };
  int mismatches = 0;
  for (std::size_t a = 0; a < 7; ++a)
    for (std::size_t b = 0; b < 7; ++b)
      for (std::size_t c = 0; c < 7; ++c)
        for (std::size_t d = 0; d < 7; ++d)
          {
            bool view_threw = false;
            bool std_threw = false;
            int view_result = 0;
            int std_result = 0;
            try
              {
                view_result = view.compare (bounds[a], bounds[b],
                                            lumex_string_view (other),
                                            bounds[c], bounds[d]);
              }
            catch (std::out_of_range const &)
              {
                view_threw = true;
              }
            try
              {
                std_result = std_view.compare (bounds[a], bounds[b],
                                               std::string_view (other),
                                               bounds[c], bounds[d]);
              }
            catch (std::out_of_range const &)
              {
                std_threw = true;
              }
            if ((view_threw != std_threw
                 || sign (view_result) != sign (std_result))
                && ++mismatches < 10)
              ADD_FAILURE () << a << b << c << d;
          }
  EXPECT_EQ (mismatches, 0);
}

TEST (LumexStringViewStdDifferentialTest,
      GivenMixedArguments_WhenOverloaded_ThenNoAmbiguity)
{
  lumex_string_view const view ("abc");
  std::string_view const standard ("abc");
  std::string const string ("abc");
  EXPECT_TRUE (view == standard);
  EXPECT_TRUE (standard == view);
  EXPECT_TRUE (view == string);
  EXPECT_TRUE (string == view);
  EXPECT_TRUE (view == "abc");
  EXPECT_TRUE ("abc" == view);
  EXPECT_TRUE (standard == "abc");
  EXPECT_FALSE (view != standard);
  EXPECT_TRUE (view <= standard && standard <= view);
  EXPECT_TRUE (view >= standard && standard >= view);
  EXPECT_FALSE (view < standard || standard < view);
}
