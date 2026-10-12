#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "LumexStringViewTestSupport.hpp"
#include "lumex/core/string_view/LumexStringView"

// The searches of the view against the members of std::basic_string, which
// have the same semantics: every text of a small alphabet that includes a NUL
// and a byte with the high bit set, then random long texts. Plus the fixed
// values of the edge cases (empty needle, needle longer than the text, npos
// and positions past the end), which do not depend on any oracle.

namespace
{
using lumex_sv_test::exact_copy;
using lumex_sv_test::random_source;

std::size_t const npos = static_cast<std::size_t> (-1);

template <class CharT> struct checker
{
  typedef std::basic_string<CharT> text_t;
  typedef typename lumex_sv_test::char_set<CharT>::view_t view_t;

  int mismatches;
  checker () : mismatches (0) {}

  void
  expect (std::size_t actual, std::size_t expected, char const *what,
          text_t const &text, text_t const &other, std::size_t pos,
          std::size_t count)
  {
    if (actual == expected)
      return;
    ++mismatches;
    if (mismatches > 20)
      return;
    std::ostringstream message;
    message << what << ": got " << actual << ", expected " << expected
            << "; text size " << text.size () << ", needle size "
            << other.size () << ", pos " << pos << ", count " << count
            << ", text codes";
    for (std::size_t i = 0; i < text.size (); ++i)
      message << ' ' << static_cast<long> (text[i]);
    message << ", needle codes";
    for (std::size_t i = 0; i < other.size (); ++i)
      message << ' ' << static_cast<long> (other[i]);
    ADD_FAILURE () << message.str ();
  }

#define LUMEX_SV_EXPECT(call, oracle)                                         \
  expect (view.call, text.oracle, #call, text, other, pos, count)

  /// Every search at `pos`, with the needle `other` (first `count`
  /// characters for the overloads that take a count).
  void
  at (view_t view, text_t const &text, view_t needle, text_t const &other,
      std::size_t pos, std::size_t count, CharT const *cstr)
  {
    LUMEX_SV_EXPECT (find (needle, pos), find (other, pos));
    LUMEX_SV_EXPECT (find (cstr, pos, count),
                     find (other.data (), pos, count));
    LUMEX_SV_EXPECT (find (cstr, pos), find (other.c_str (), pos));
    LUMEX_SV_EXPECT (rfind (needle, pos), rfind (other, pos));
    LUMEX_SV_EXPECT (rfind (cstr, pos, count),
                     rfind (other.data (), pos, count));
    LUMEX_SV_EXPECT (rfind (cstr, pos), rfind (other.c_str (), pos));
    LUMEX_SV_EXPECT (find_first_of (needle, pos), find_first_of (other, pos));
    LUMEX_SV_EXPECT (find_first_of (cstr, pos, count),
                     find_first_of (other.data (), pos, count));
    LUMEX_SV_EXPECT (find_first_of (cstr, pos),
                     find_first_of (other.c_str (), pos));
    LUMEX_SV_EXPECT (find_last_of (needle, pos), find_last_of (other, pos));
    LUMEX_SV_EXPECT (find_last_of (cstr, pos, count),
                     find_last_of (other.data (), pos, count));
    LUMEX_SV_EXPECT (find_last_of (cstr, pos),
                     find_last_of (other.c_str (), pos));
    LUMEX_SV_EXPECT (find_first_not_of (needle, pos),
                     find_first_not_of (other, pos));
    LUMEX_SV_EXPECT (find_first_not_of (cstr, pos, count),
                     find_first_not_of (other.data (), pos, count));
    LUMEX_SV_EXPECT (find_first_not_of (cstr, pos),
                     find_first_not_of (other.c_str (), pos));
    LUMEX_SV_EXPECT (find_last_not_of (needle, pos),
                     find_last_not_of (other, pos));
    LUMEX_SV_EXPECT (find_last_not_of (cstr, pos, count),
                     find_last_not_of (other.data (), pos, count));
    LUMEX_SV_EXPECT (find_last_not_of (cstr, pos),
                     find_last_not_of (other.c_str (), pos));
    if (!other.empty ())
      {
        CharT const chr = other[0];
        LUMEX_SV_EXPECT (find (chr, pos), find (chr, pos));
        LUMEX_SV_EXPECT (rfind (chr, pos), rfind (chr, pos));
        LUMEX_SV_EXPECT (find_first_of (chr, pos), find_first_of (chr, pos));
        LUMEX_SV_EXPECT (find_last_of (chr, pos), find_last_of (chr, pos));
        LUMEX_SV_EXPECT (find_first_not_of (chr, pos),
                         find_first_not_of (chr, pos));
        LUMEX_SV_EXPECT (find_last_not_of (chr, pos),
                         find_last_not_of (chr, pos));
      }
  }
#undef LUMEX_SV_EXPECT

  /// One text against one needle: all positions, two counts.
  void
  pair (text_t const &text, text_t const &other)
  {
    exact_copy<CharT> text_copy (text);
    exact_copy<CharT> other_copy (other);
    view_t const view (text_copy.data (), text_copy.size ());
    view_t const needle (other_copy.data (), other_copy.size ());
    std::vector<std::size_t> const where
        = lumex_sv_test::positions (text.size ());
    for (std::size_t i = 0; i < where.size (); ++i)
      {
        std::size_t counts[2] = { other.size (), other.size () / 2 };
        for (std::size_t k = 0; k < 2; ++k)
          at (view, text, needle, other, where[i], counts[k], other.c_str ());
      }
  }
};

template <class CharT>
void
exhaustive (std::size_t text_length, std::size_t needle_length)
{
  checker<CharT> check;
  std::vector<CharT> const alphabet
      = lumex_sv_test::char_set<CharT>::alphabet ();
  std::vector<std::basic_string<CharT>> const texts
      = lumex_sv_test::all_texts (alphabet, text_length);
  std::vector<std::basic_string<CharT>> const needles
      = lumex_sv_test::all_texts (alphabet, needle_length);
  for (std::size_t t = 0; t < texts.size (); ++t)
    for (std::size_t n = 0; n < needles.size (); ++n)
      check.pair (texts[t], needles[n]);
  EXPECT_EQ (check.mismatches, 0);
}

template <class CharT>
void
random_texts (std::uint64_t seed)
{
  checker<CharT> check;
  std::vector<CharT> const alphabet
      = lumex_sv_test::char_set<CharT>::alphabet ();
  random_source random (seed);
  for (int round = 0; round < 1500; ++round)
    {
      std::size_t const letters = 2 + random.below (3);
      std::basic_string<CharT> text;
      std::size_t const length = random.below (160);
      for (std::size_t i = 0; i < length; ++i)
        text.push_back (alphabet[random.below (letters)]);
      std::basic_string<CharT> other;
      if (!text.empty () && random.below (4) != 0)
        {
          std::size_t const from = random.below (text.size ());
          other = text.substr (from, random.below (text.size () - from + 1));
          if (other.size () > 12)
            other.resize (12);
        }
      else
        {
          std::size_t const other_length = random.below (5);
          for (std::size_t i = 0; i < other_length; ++i)
            other.push_back (alphabet[random.below (alphabet.size ())]);
        }
      check.pair (text, other);
    }
  EXPECT_EQ (check.mismatches, 0);
}
} // namespace

TEST (LumexStringViewSearchTest,
      GivenEveryNarrowText_WhenSearch_ThenSameAsString)
{
  exhaustive<char> (5, 3);
}

TEST (LumexStringViewSearchTest,
      GivenEveryWideText_WhenSearch_ThenSameAsString)
{
  exhaustive<wchar_t> (4, 3);
}

TEST (LumexStringViewSearchTest,
      GivenRandomNarrowTexts_WhenSearch_ThenSameAsString)
{
  random_texts<char> (0x5eed0001U);
}

TEST (LumexStringViewSearchTest,
      GivenRandomWideTexts_WhenSearch_ThenSameAsString)
{
  random_texts<wchar_t> (0x5eed0002U);
}

TEST (LumexStringViewSearchTest, GivenEmptyNeedle_WhenFind_ThenPositionOrNpos)
{
  lumex_string_view const view ("abc");
  EXPECT_EQ (view.find (""), 0U);
  EXPECT_EQ (view.find ("", 2), 2U);
  EXPECT_EQ (view.find ("", 3), 3U);
  EXPECT_EQ (view.find ("", 4), npos);
  EXPECT_EQ (view.find ("", npos), npos);
  EXPECT_EQ (lumex_string_view ().find (""), 0U);
  EXPECT_EQ (lumex_string_view ().find ("", 1), npos);
}

TEST (LumexStringViewSearchTest, GivenEmptyNeedle_WhenRfind_ThenClampedToSize)
{
  lumex_string_view const view ("abc");
  EXPECT_EQ (view.rfind (""), 3U);
  EXPECT_EQ (view.rfind ("", 1), 1U);
  EXPECT_EQ (view.rfind ("", 3), 3U);
  EXPECT_EQ (view.rfind ("", 7), 3U);
  EXPECT_EQ (lumex_string_view ().rfind (""), 0U);
}

TEST (LumexStringViewSearchTest, GivenNeedleLongerThanText_WhenSearch_ThenNpos)
{
  lumex_string_view const view ("abc");
  EXPECT_EQ (view.find ("abcd"), npos);
  EXPECT_EQ (view.rfind ("abcd"), npos);
  EXPECT_EQ (view.find ("abcdefghijklmnopqrstuvwxyz", 0), npos);
  EXPECT_EQ (lumex_string_view ().find ("a"), npos);
  EXPECT_EQ (lumex_string_view ().rfind ("a"), npos);
  EXPECT_EQ (view.find ("abc"), 0U);
  EXPECT_EQ (view.rfind ("abc"), 0U);
}

TEST (LumexStringViewSearchTest, GivenPositionPastTheEnd_WhenSearch_ThenNpos)
{
  lumex_string_view const view ("abcabc");
  EXPECT_EQ (view.find ("c", 6), npos);
  EXPECT_EQ (view.find ("c", 7), npos);
  EXPECT_EQ (view.find ("c", npos), npos);
  EXPECT_EQ (view.find ('c', 6), npos);
  EXPECT_EQ (view.find ('c', npos), npos);
  EXPECT_EQ (view.find_first_of ("c", 6), npos);
  EXPECT_EQ (view.find_first_of ("c", npos), npos);
  EXPECT_EQ (view.find_first_not_of ("x", 6), npos);
  EXPECT_EQ (view.find_first_not_of ("x", npos), npos);
  EXPECT_EQ (view.find_first_not_of ('x', 6), npos);
}

TEST (LumexStringViewSearchTest,
      GivenPositionPastTheEnd_WhenSearchBackward_ThenClamped)
{
  lumex_string_view const view ("abcabc");
  EXPECT_EQ (view.rfind ("c", npos), 5U);
  EXPECT_EQ (view.rfind ("c", 6), 5U);
  EXPECT_EQ (view.rfind ('c', npos), 5U);
  EXPECT_EQ (view.rfind ('c', 100), 5U);
  EXPECT_EQ (view.rfind ("abc", 100), 3U);
  EXPECT_EQ (view.rfind ("abc", 3), 3U);
  EXPECT_EQ (view.rfind ("abc", 2), 0U);
  EXPECT_EQ (view.find_last_of ("c", npos), 5U);
  EXPECT_EQ (view.find_last_of ("a", 100), 3U);
  EXPECT_EQ (view.find_last_not_of ("c", npos), 4U);
  EXPECT_EQ (view.find_last_not_of ("xyz", npos), 5U);
  EXPECT_EQ (view.find_last_not_of ('c', 100), 4U);
}

TEST (LumexStringViewSearchTest, GivenPositionInside_WhenFind_ThenStartsThere)
{
  lumex_string_view const view ("abcabc");
  EXPECT_EQ (view.find ("abc", 1), 3U);
  EXPECT_EQ (view.find ("abc", 3), 3U);
  EXPECT_EQ (view.find ("abc", 4), npos);
  EXPECT_EQ (view.find ('a', 1), 3U);
  EXPECT_EQ (view.find ("c", 2), 2U);
  EXPECT_EQ (view.rfind ("abc", 2), 0U);
  EXPECT_EQ (view.rfind ('a', 2), 0U);
  EXPECT_EQ (view.rfind ('b', 0), npos);
}

TEST (LumexStringViewSearchTest, GivenMatchAtLastValidStart_WhenFind_ThenFound)
{
  lumex_string_view const view ("xxxxabc");
  EXPECT_EQ (view.find ("abc"), 4U);
  EXPECT_EQ (view.find ("c"), 6U);
  EXPECT_EQ (view.find ("bc", 5), 5U);
  EXPECT_EQ (view.find ("bc", 6), npos);
  EXPECT_EQ (view.find ("xabc", 3), 3U);
  EXPECT_EQ (view.find ("xabc", 4), npos);
}

TEST (LumexStringViewSearchTest, GivenMatchAtFirstPosition_WhenFind_ThenZero)
{
  lumex_string_view const view ("abcxyz");
  EXPECT_EQ (view.find ("abc"), 0U);
  EXPECT_EQ (view.find ("a"), 0U);
  EXPECT_EQ (view.find ('a'), 0U);
  EXPECT_EQ (view.find ("abd"), npos);
  EXPECT_EQ (view.find ("aab"), npos);
}

TEST (LumexStringViewSearchTest,
      GivenFirstLetterRepeats_WhenFind_ThenCompareRest)
{
  lumex_string_view const view ("aaaaaaaaab");
  EXPECT_EQ (view.find ("aab"), 7U);
  EXPECT_EQ (view.find ("ab"), 8U);
  EXPECT_EQ (view.find ("aaab", 5), 6U);
  EXPECT_EQ (view.find ("abb"), npos);
}

TEST (LumexStringViewSearchTest, GivenFirstLetterAbsent_WhenFind_ThenNpos)
{
  std::string const text (100000, 'x');
  lumex_string_view const view (text);
  EXPECT_EQ (view.find ("yz"), npos);
  EXPECT_EQ (view.find ("xy"), npos);
  EXPECT_EQ (view.find (std::string (50, 'x')), 0U);
  EXPECT_EQ (view.find (std::string (50, 'x'), 99950), 99950U);
  EXPECT_EQ (view.find (std::string (50, 'x'), 99951), npos);
}

TEST (LumexStringViewSearchTest, GivenSets_WhenSearchFirst_ThenFixedValues)
{
  lumex_string_view const view ("hello world");
  EXPECT_EQ (view.find_first_of ("ow"), 4U);
  EXPECT_EQ (view.find_first_of ("wo"), 4U);
  EXPECT_EQ (view.find_first_of ("o", 5), 7U);
  EXPECT_EQ (view.find_first_of (""), npos);
  EXPECT_EQ (view.find_first_of ("xyz"), npos);
  EXPECT_EQ (view.find_first_of ('l'), 2U);
  EXPECT_EQ (view.find_first_of ("zzzd"), 10U);
  EXPECT_EQ (view.find_first_not_of ("hel"), 4U);
  EXPECT_EQ (view.find_first_not_of (""), 0U);
  EXPECT_EQ (view.find_first_not_of ("", 3), 3U);
  EXPECT_EQ (view.find_first_not_of ("helo wrd"), npos);
  EXPECT_EQ (view.find_first_not_of ('h'), 1U);
  EXPECT_EQ (view.find_first_not_of ('x'), 0U);
}

TEST (LumexStringViewSearchTest, GivenSets_WhenSearchLast_ThenFixedValues)
{
  lumex_string_view const view ("hello world");
  EXPECT_EQ (view.find_last_of ("ol"), 9U);
  EXPECT_EQ (view.find_last_of ("lo", 8), 7U);
  EXPECT_EQ (view.find_last_of ("h"), 0U);
  EXPECT_EQ (view.find_last_of (""), npos);
  EXPECT_EQ (view.find_last_of ("xyz"), npos);
  EXPECT_EQ (view.find_last_of ("zzzh"), 0U);
  EXPECT_EQ (view.find_last_of ('o'), 7U);
  EXPECT_EQ (lumex_string_view ().find_last_of ("a"), npos);
  EXPECT_EQ (view.find_last_not_of ("dl"), 8U);
  EXPECT_EQ (view.find_last_not_of (""), 10U);
  EXPECT_EQ (view.find_last_not_of ("", 4), 4U);
  EXPECT_EQ (view.find_last_not_of ("helo wrd"), npos);
  EXPECT_EQ (view.find_last_not_of ("d"), 9U);
  EXPECT_EQ (view.find_last_not_of ('d'), 9U);
  EXPECT_EQ (view.find_last_not_of ('h', 0), npos);
  EXPECT_EQ (view.find_last_not_of ('x', 0), 0U);
  EXPECT_EQ (lumex_string_view ().find_last_not_of ("a"), npos);
}

TEST (LumexStringViewSearchTest,
      GivenCountOverloads_WhenSearch_ThenCountLimitsTheNeedle)
{
  lumex_string_view const view ("abcdef");
  EXPECT_EQ (view.find ("cdXY", 0, 2), 2U);
  EXPECT_EQ (view.find ("cdXY", 0, 4), npos);
  EXPECT_EQ (view.rfind ("cdXY", npos, 2), 2U);
  EXPECT_EQ (view.find_first_of ("zzcX", 0, 3), 2U);
  EXPECT_EQ (view.find_first_of ("zzcX", 0, 2), npos);
  EXPECT_EQ (view.find_last_of ("azzzX", npos, 2), 0U);
  EXPECT_EQ (view.find_first_not_of ("abXX", 0, 2), 2U);
  EXPECT_EQ (view.find_first_not_of ("abXX", 0, 4), 2U);
  EXPECT_EQ (view.find_last_not_of ("efXX", npos, 1), 5U);
  EXPECT_EQ (view.find_last_not_of ("efXX", npos, 2), 3U);
}

TEST (LumexStringViewSearchTest,
      GivenNullPointerNeedle_WhenSearch_ThenEmptyNeedle)
{
  lumex_string_view const view ("abc");
  char const *const none = nullptr;
  EXPECT_EQ (view.find (none), 0U);
  EXPECT_EQ (view.rfind (none), 3U);
  EXPECT_EQ (view.find_first_of (none), npos);
  EXPECT_EQ (view.find_last_of (none), npos);
  EXPECT_EQ (view.find_first_not_of (none), 0U);
  EXPECT_EQ (view.find_last_not_of (none), 2U);
  EXPECT_EQ (view.find (none, 1, 0), 1U);
}

TEST (LumexStringViewSearchTest,
      GivenNullPointerView_WhenSearch_ThenNothingFound)
{
  lumex_string_view const none;
  EXPECT_EQ (none.find ('a'), npos);
  EXPECT_EQ (none.find ("a"), npos);
  EXPECT_EQ (none.rfind ('a'), npos);
  EXPECT_EQ (none.find_first_of ("ab"), npos);
  EXPECT_EQ (none.find_last_of ("ab"), npos);
  EXPECT_EQ (none.find_first_not_of ("ab"), npos);
  EXPECT_EQ (none.find_last_not_of ("ab"), npos);
  EXPECT_EQ (none.find_first_not_of ('a'), npos);
  EXPECT_EQ (none.find_last_not_of ('a'), npos);
  EXPECT_FALSE (none.contains ("a"));
  EXPECT_FALSE (none.contains ('a'));
  EXPECT_TRUE (none.contains (""));
  lumex_string_view const empty_view (nullptr, 0);
  EXPECT_EQ (empty_view.find (""), 0U);
  EXPECT_EQ (empty_view.find ("a"), npos);
}

TEST (LumexStringViewSearchTest,
      GivenHighBitCharacters_WhenSearch_ThenComparedByValue)
{
  std::string const text ("a\x80z\xff", 4);
  lumex_string_view const view (text);
  EXPECT_EQ (view.find ('\x80'), 1U);
  EXPECT_EQ (view.find (std::string ("\xff")), 3U);
  EXPECT_EQ (view.find_first_of (std::string ("\xff\x80")), 1U);
  EXPECT_EQ (view.find_last_of (std::string ("\x80", 1)), 1U);
  EXPECT_EQ (view.find_first_not_of (std::string ("a\x80", 2)), 2U);
  EXPECT_EQ (view.find_last_not_of ("\xff"), 2U);
}

TEST (LumexStringViewSearchTest,
      GivenEmbeddedNul_WhenSearch_ThenNulIsACharacter)
{
  std::string const text ("ab\0cd\0", 6);
  lumex_string_view const view (text);
  EXPECT_EQ (view.find ('\0'), 2U);
  EXPECT_EQ (view.rfind ('\0'), 5U);
  EXPECT_EQ (view.find (std::string ("\0c", 2)), 2U);
  EXPECT_EQ (view.find_first_of (std::string ("\0", 1)), 2U);
  EXPECT_EQ (view.find_last_of (std::string ("\0", 1)), 5U);
  EXPECT_EQ (view.find_first_not_of (std::string ("ab", 2)), 2U);
  EXPECT_EQ (view.find_last_not_of (std::string ("\0", 1)), 4U);
  EXPECT_EQ (view.find ("c"), 3U);
}
