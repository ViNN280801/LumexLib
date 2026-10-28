#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "LumexStringViewTestSupport.hpp"
#include "lumex/core/string_view/LumexStringView"

// The order of the views: the six operators and every compare overload,
// against std::basic_string::compare over all short texts of an alphabet that
// has a NUL and a byte with the high bit set; plus the fixed values of the
// embedded NUL, the unsigned comparison and the size tie-break.

namespace
{
template <class T>
void
sink (T const &)
{
}
using lumex_sv_test::exact_copy;

int
sign (int value)
{
  return value < 0 ? -1 : (value > 0 ? 1 : 0);
}

template <class CharT>
void
order_of_all_pairs (std::size_t max_length)
{
  using text_t = std::basic_string<CharT>;
  using view_t = typename lumex_sv_test::char_set<CharT>::view_t;
  std::vector<text_t> const texts = lumex_sv_test::all_texts (
      lumex_sv_test::char_set<CharT>::alphabet (), max_length);
  int mismatches = 0;
  for (std::size_t i = 0; i < texts.size (); ++i)
    for (std::size_t j = 0; j < texts.size (); ++j)
      {
        exact_copy<CharT> left_copy (texts[i]);
        exact_copy<CharT> right_copy (texts[j]);
        view_t const left (left_copy.data (), left_copy.size ());
        view_t const right (right_copy.data (), right_copy.size ());
        int const expected = sign (texts[i].compare (texts[j]));
        bool const ok = sign (left.compare (right)) == expected
                        && (left == right) == (expected == 0)
                        && (left != right) == (expected != 0)
                        && (left < right) == (expected < 0)
                        && (left > right) == (expected > 0)
                        && (left <= right) == (expected <= 0)
                        && (left >= right) == (expected >= 0);
        if (!ok && ++mismatches < 10)
          ADD_FAILURE () << "pair " << i << ", " << j << ": expected "
                         << expected;
      }
  EXPECT_EQ (mismatches, 0);
}

/// The result of a compare of the string and of the view, or "threw".
struct outcome
{
  bool threw;
  int order;
};

template <class Call>
outcome
run (Call call)
{
  outcome result;
  result.threw = false;
  result.order = 0;
  try
    {
      result.order = sign (call ());
    }
  catch (std::out_of_range const &)
    {
      result.threw = true;
    }
  return result;
}

template <class CharT> struct overloads
{
  using text_t = std::basic_string<CharT>;
  using view_t = typename lumex_sv_test::char_set<CharT>::view_t;

  text_t text;
  text_t other;
  std::size_t pos1;
  std::size_t n1;
  std::size_t pos2;
  std::size_t n2;

  int
  view_3 () const
  {
    return view_t (text).compare (pos1, n1, view_t (other));
  }
  int
  string_3 () const
  {
    return text.compare (pos1, n1, other);
  }
  int
  view_5 () const
  {
    return view_t (text).compare (pos1, n1, view_t (other), pos2, n2);
  }
  int
  string_5 () const
  {
    return text.compare (pos1, n1, other, pos2, n2);
  }
  int
  view_cstr () const
  {
    return view_t (text).compare (other.c_str ());
  }
  int
  string_cstr () const
  {
    return text.compare (other.c_str ());
  }
  int
  view_pos_cstr () const
  {
    return view_t (text).compare (pos1, n1, other.c_str ());
  }
  int
  string_pos_cstr () const
  {
    return text.compare (pos1, n1, other.c_str ());
  }
  /// The count of a C string cannot exceed its length.
  std::size_t
  counted () const
  {
    return n2 < other.size () ? n2 : other.size ();
  }
  int
  view_pos_cstr_count () const
  {
    return view_t (text).compare (pos1, n1, other.c_str (), counted ());
  }
  int
  string_pos_cstr_count () const
  {
    return text.compare (pos1, n1, other.c_str (), counted ());
  }
};

template <class CharT>
void
substring_overloads ()
{
  using text_t = std::basic_string<CharT>;
  std::vector<text_t> const texts = lumex_sv_test::all_texts (
      lumex_sv_test::char_set<CharT>::alphabet (), 2);
  int mismatches = 0;
  std::size_t const bounds[]
      = { 0, 1, 2, 3, 4, static_cast<std::size_t> (-1) };
  for (std::size_t i = 0; i < texts.size (); ++i)
    for (std::size_t j = 0; j < texts.size (); ++j)
      for (std::size_t a = 0; a < 6; ++a)
        for (std::size_t b = 0; b < 6; ++b)
          for (std::size_t c = 0; c < 6; ++c)
            for (std::size_t d = 0; d < 6; ++d)
              {
                overloads<CharT> o;
                o.text = texts[i];
                o.other = texts[j];
                o.pos1 = bounds[a];
                o.n1 = bounds[b];
                o.pos2 = bounds[c];
                o.n2 = bounds[d];
                outcome const e3 = run ([&] { return o.string_3 (); });
                outcome const a3 = run ([&] { return o.view_3 (); });
                outcome const e5 = run ([&] { return o.string_5 (); });
                outcome const a5 = run ([&] { return o.view_5 (); });
                outcome const ec = run ([&] { return o.string_pos_cstr (); });
                outcome const ac = run ([&] { return o.view_pos_cstr (); });
                outcome const en
                    = run ([&] { return o.string_pos_cstr_count (); });
                outcome const an
                    = run ([&] { return o.view_pos_cstr_count (); });
                outcome const e1 = run ([&] { return o.string_cstr (); });
                outcome const a1 = run ([&] { return o.view_cstr (); });
                bool const ok = e3.threw == a3.threw && e3.order == a3.order
                                && e5.threw == a5.threw && e5.order == a5.order
                                && ec.threw == ac.threw && ec.order == ac.order
                                && en.threw == an.threw && en.order == an.order
                                && e1.order == a1.order;
                if (!ok && ++mismatches < 10)
                  ADD_FAILURE () << "texts " << i << ", " << j << ", bounds "
                                 << a << b << c << d;
              }
  EXPECT_EQ (mismatches, 0);
}
} // namespace

TEST (LumexStringViewOrderingTest,
      GivenEveryNarrowPair_WhenCompare_ThenSameAsString)
{
  order_of_all_pairs<char> (4);
}

TEST (LumexStringViewOrderingTest,
      GivenEveryWidePair_WhenCompare_ThenSameAsString)
{
  order_of_all_pairs<wchar_t> (3);
}

TEST (LumexStringViewOrderingTest,
      GivenSubstringOverloads_WhenCompareNarrow_ThenSameAsString)
{
  substring_overloads<char> ();
}

TEST (LumexStringViewOrderingTest,
      GivenSubstringOverloads_WhenCompareWide_ThenSameAsString)
{
  substring_overloads<wchar_t> ();
}

TEST (LumexStringViewOrderingTest,
      GivenHighBitByte_WhenCompare_ThenGreaterThanAscii)
{
  // 0x80 and 0xFF are negative as a char, and still sort after every ASCII
  // character (char_traits compares as unsigned char).
  lumex_string_view const high ("\x80");
  lumex_string_view const higher ("\xff");
  EXPECT_GT (high.compare ("a"), 0);
  EXPECT_GT (higher.compare ("a"), 0);
  EXPECT_LT (lumex_string_view ("a").compare (higher), 0);
  EXPECT_TRUE (high > lumex_string_view ("a"));
  EXPECT_TRUE (lumex_string_view ("a") < high);
  EXPECT_TRUE (high < higher);
  EXPECT_TRUE (high >= high);
  EXPECT_TRUE (lumex_string_view ("z") < high);
  EXPECT_TRUE (lumex_string_view ("\x7f") < high);
}

TEST (LumexStringViewOrderingTest,
      GivenEmbeddedNul_WhenCompare_ThenNulDoesNotEndTheText)
{
  lumex_string_view const first ("a\0b", 3);
  lumex_string_view const second ("a\0c", 3);
  lumex_string_view const shorter ("a", 1);
  EXPECT_TRUE (first < second);
  EXPECT_TRUE (second > first);
  EXPECT_TRUE (first != second);
  EXPECT_FALSE (first == second);
  EXPECT_TRUE (first != shorter);
  EXPECT_TRUE (shorter < first);
  EXPECT_LT (first.compare (second), 0);
  EXPECT_GT (first.compare (shorter), 0);
  EXPECT_EQ (first.compare (lumex_string_view ("a\0b", 3)), 0);
  EXPECT_TRUE (first == lumex_string_view ("a\0b", 3));
  EXPECT_GT (first.compare ("a"), 0) << "the C string stops at the NUL";
}

TEST (LumexStringViewOrderingTest,
      GivenCommonPrefix_WhenCompare_ThenShorterIsLess)
{
  EXPECT_LT (lumex_string_view ("ab").compare ("abc"), 0);
  EXPECT_GT (lumex_string_view ("abc").compare ("ab"), 0);
  EXPECT_EQ (lumex_string_view ("abc").compare ("abc"), 0);
  EXPECT_TRUE (lumex_string_view ("ab") < lumex_string_view ("abc"));
  EXPECT_TRUE (lumex_string_view ("ab") <= lumex_string_view ("abc"));
  EXPECT_FALSE (lumex_string_view ("ab") >= lumex_string_view ("abc"));
  EXPECT_FALSE (lumex_string_view ("ab") > lumex_string_view ("abc"));
  EXPECT_TRUE (lumex_string_view () < lumex_string_view ("a"));
  EXPECT_TRUE (lumex_string_view () <= lumex_string_view ());
  EXPECT_TRUE (lumex_string_view () == lumex_string_view (""));
}

TEST (LumexStringViewOrderingTest,
      GivenEqualSizeDifferentText_WhenEqual_ThenFalse)
{
  EXPECT_FALSE (lumex_string_view ("abc") == lumex_string_view ("abd"));
  EXPECT_TRUE (lumex_string_view ("abc") != lumex_string_view ("abd"));
  EXPECT_FALSE (lumex_string_view ("abc") == lumex_string_view ("abcd"));
  char const first[] = "same";
  char const second[] = "same";
  EXPECT_TRUE (lumex_string_view (first) == lumex_string_view (second))
      << "equal text at different addresses";
}

TEST (LumexStringViewOrderingTest,
      GivenSixOperators_WhenSetsAreOrdered_ThenEachSenseIsRight)
{
  lumex_string_view const less ("a");
  lumex_string_view const more ("b");
  EXPECT_TRUE (less < more);
  EXPECT_FALSE (more < less);
  EXPECT_FALSE (less < less);
  EXPECT_TRUE (more > less);
  EXPECT_FALSE (less > more);
  EXPECT_FALSE (less > less);
  EXPECT_TRUE (less <= more);
  EXPECT_FALSE (more <= less);
  EXPECT_TRUE (less <= less);
  EXPECT_TRUE (more >= less);
  EXPECT_FALSE (less >= more);
  EXPECT_TRUE (less >= less);
  EXPECT_TRUE (less == less);
  EXPECT_FALSE (less == more);
  EXPECT_TRUE (less != more);
  EXPECT_FALSE (less != less);
}

TEST (LumexStringViewOrderingTest,
      GivenStringAndLiteral_WhenCompare_ThenConvertsToView)
{
  lumex_string_view const view ("abc");
  EXPECT_TRUE (view == "abc");
  EXPECT_TRUE ("abc" == view);
  EXPECT_TRUE (view == std::string ("abc"));
  EXPECT_TRUE (std::string ("abc") == view);
  EXPECT_TRUE (view < "abd");
  EXPECT_TRUE ("abb" < view);
  EXPECT_TRUE (view != "abcd");
}

TEST (LumexStringViewOrderingTest,
      GivenSubstringCompare_WhenPositionPastTheEnd_ThenOutOfRange)
{
  lumex_string_view const view ("abcdef");
  EXPECT_THROW (sink (view.compare (7, 1, "x")), std::out_of_range);
  EXPECT_THROW (sink (view.compare (7, 1, lumex_string_view ("x"))),
                std::out_of_range);
  EXPECT_THROW (sink (view.compare (0, 1, lumex_string_view ("x"), 2, 1)),
                std::out_of_range);
  EXPECT_THROW (sink (view.compare (7, 1, lumex_string_view ("x"), 0, 1)),
                std::out_of_range);
  EXPECT_THROW (sink (view.compare (7, 1, "xyz", 2)), std::out_of_range);
  EXPECT_NO_THROW (sink (view.compare (6, 1, "x")));
}

TEST (LumexStringViewOrderingTest,
      GivenSubstringCompare_WhenWholeOrPartAgrees_ThenZero)
{
  lumex_string_view const view ("abcdef");
  EXPECT_EQ (view.compare (1, 3, "bcd"), 0);
  EXPECT_EQ (view.compare (1, 3, lumex_string_view ("bcd")), 0);
  EXPECT_EQ (view.compare (1, 3, lumex_string_view ("xbcdx"), 1, 3), 0);
  EXPECT_EQ (view.compare (1, 3, "bcdXY", 3), 0);
  EXPECT_LT (view.compare (1, 3, "bcdXY", 5), 0);
  EXPECT_GT (view.compare (1, 3, lumex_string_view ("xbcdx"), 1, 2), 0);
  EXPECT_NE (view.compare (0, 3, "bcd"), 0) << "the substring is used";
  EXPECT_EQ (view.compare (4, std::size_t (-1), "ef"), 0);
}
