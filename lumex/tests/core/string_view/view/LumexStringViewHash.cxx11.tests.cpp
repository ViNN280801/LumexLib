#include <cstddef>
#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <gtest/gtest.h>

#include "LumexStringViewTestSupport.hpp"
#include "lumex/core/string_view/LumexStringView"

// std::hash of the views: one 64-bit block hash on every standard (it is not
// the hash of std::string_view, which depends on the standard library), so a
// library built at C++11 and a program built at C++17 hash a key alike (the
// cross-standard case is the consumer fixture
// string_view_hash_standard_mismatch). The pinned values below are the same on
// every platform, standard and compiler.

namespace
{
using lumex::core::string_view::view::lumex_u16string_view;
using lumex::core::string_view::view::lumex_u32string_view;
using lumex_sv_test::random_source;

template <class View>
std::size_t
hash_of (View const &view)
{
  return std::hash<View> () (view);
}

std::uint64_t
wide_hash (std::size_t value)
{
  return static_cast<std::uint64_t> (value);
}

int
bits_set (std::uint64_t value)
{
  int count = 0;
  for (; value != 0; value >>= 1)
    count += static_cast<int> (value & 1U);
  return count;
}
} // namespace

TEST (LumexStringViewHashTest,
      GivenPinnedVectors_WhenHash_ThenTheSameOnEveryPlatform)
{
  if (sizeof (std::size_t) < 8)
    GTEST_SKIP () << "the vectors are 64-bit";
  EXPECT_EQ (hash_of (lumex_string_view ("")), 0x9353dfc8a195f3e2ULL);
  EXPECT_EQ (hash_of (lumex_string_view ("key")), 0x56f946003910f923ULL);
  EXPECT_EQ (hash_of (lumex_string_view ("foobar")), 0x3b329c7d65de484aULL);
  EXPECT_EQ (hash_of (lumex_string_view ("a\0", 2)), 0x6d0f239e9a44cd2aULL);
  EXPECT_EQ (hash_of (lumex_string_view (
                 "The quick brown fox jumps over the lazy dog")),
             0xf49a5f6e50ef91d6ULL);
  EXPECT_EQ (hash_of (lumex_string_view ("\x80\xff")), 0x0f5cb501e7329b0cULL);
  EXPECT_EQ (hash_of (lumex_wstring_view (L"key")), 0x323f66e13681609fULL);
  EXPECT_EQ (hash_of (lumex_wstring_view (L"The quick brown fox")),
             0xd4c81d87116fd6f6ULL);
}

TEST (LumexStringViewHashTest, GivenSameCodePoints_WhenHashWideTypes_ThenEqual)
{
  EXPECT_EQ (hash_of (lumex_wstring_view (L"key")),
             hash_of (lumex_u16string_view (u"key")));
  EXPECT_EQ (hash_of (lumex_wstring_view (L"key")),
             hash_of (lumex_u32string_view (U"key")));
  EXPECT_EQ (hash_of (lumex_u16string_view (u"€")),
             hash_of (lumex_u32string_view (U"€")));
  EXPECT_NE (hash_of (lumex_u16string_view (u"ab")),
             hash_of (lumex_u16string_view (u"ba")));
  EXPECT_NE (hash_of (lumex_u32string_view (U"\x10000")),
             hash_of (lumex_u32string_view (U"\x20000")));
}

TEST (LumexStringViewHashTest,
      GivenEmbeddedNulAndLength_WhenHash_ThenAllCharactersCount)
{
  EXPECT_NE (hash_of (lumex_string_view ("a\0b", 3)),
             hash_of (lumex_string_view ("a", 1)));
  EXPECT_NE (hash_of (lumex_string_view ("a\0b", 3)),
             hash_of (lumex_string_view ("a\0c", 3)));
  EXPECT_NE (hash_of (lumex_string_view ("\0", 1)),
             hash_of (lumex_string_view ()));
  EXPECT_NE (hash_of (lumex_string_view ("\0\0", 2)),
             hash_of (lumex_string_view ("\0", 1)));
  EXPECT_NE (hash_of (lumex_string_view ("12345678", 8)),
             hash_of (lumex_string_view ("12345678\0", 9)));
  EXPECT_NE (hash_of (lumex_wstring_view (L"a\0", 2)),
             hash_of (lumex_wstring_view (L"a", 1)));
}

TEST (LumexStringViewHashTest,
      GivenEqualText_WhenHash_ThenEqualWhateverTheAddress)
{
  std::string const first ("some text here");
  std::string second ("xxsome text herexx");
  EXPECT_EQ (hash_of (lumex_string_view (first)),
             hash_of (lumex_string_view (second).substr (2, 14)));
  EXPECT_EQ (hash_of (lumex_string_view (first)),
             hash_of (lumex_string_view ("some text here")));
  EXPECT_EQ (hash_of (lumex_wstring_view (L"wide")),
             hash_of (lumex_wstring_view (std::wstring (L"wide"))));
}

TEST (LumexStringViewHashTest, GivenNullPointerView_WhenHash_ThenSameAsEmpty)
{
  std::size_t const empty = hash_of (lumex_string_view (""));
  EXPECT_EQ (hash_of (lumex_string_view ()), empty);
  EXPECT_EQ (hash_of (lumex_string_view (nullptr, 0)), empty);
  EXPECT_EQ (hash_of (lumex_string_view (static_cast<char const *> (nullptr))),
             empty);
  EXPECT_EQ (hash_of (lumex_wstring_view ()),
             hash_of (lumex_wstring_view (L"")));
}

TEST (LumexStringViewHashTest, GivenEveryShortString_WhenHash_ThenNoCollision)
{
  // All strings of up to 3 characters over 16 characters (4369 strings), the
  // NUL and a high-bit byte among them, and the same over wchar_t.
  std::vector<char> alphabet;
  for (char c = 'a'; c < 'a' + 14; ++c)
    alphabet.push_back (c);
  alphabet.push_back ('\0');
  alphabet.push_back (static_cast<char> (0x80));
  std::vector<std::string> const texts
      = lumex_sv_test::all_texts (alphabet, 3);
  std::set<std::size_t> hashes;
  for (std::size_t i = 0; i < texts.size (); ++i)
    hashes.insert (hash_of (lumex_string_view (texts[i])));
  EXPECT_EQ (hashes.size (), texts.size ());
  std::vector<wchar_t> wide_alphabet;
  for (wchar_t c = L'a'; c < L'a' + 15; ++c)
    wide_alphabet.push_back (c);
  wide_alphabet.push_back (static_cast<wchar_t> (0x20AC));
  std::vector<std::wstring> const wide_texts
      = lumex_sv_test::all_texts (wide_alphabet, 3);
  std::set<std::size_t> wide_hashes;
  for (std::size_t i = 0; i < wide_texts.size (); ++i)
    wide_hashes.insert (hash_of (lumex_wstring_view (wide_texts[i])));
  EXPECT_EQ (wide_hashes.size (), wide_texts.size ());
}

TEST (LumexStringViewHashTest,
      GivenOneFlippedBit_WhenHash_ThenAboutHalfTheBitsChange)
{
  if (sizeof (std::size_t) < 8)
    GTEST_SKIP () << "the avalanche is measured on 64 bits";
  random_source random (0x4a5b6c7dU);
  long total = 0;
  int minimum = 64;
  int samples = 0;
  for (int round = 0; round < 3000; ++round)
    {
      std::string text;
      std::size_t const length = 1 + random.below (40);
      for (std::size_t i = 0; i < length; ++i)
        text.push_back (static_cast<char> (random.below (256)));
      std::string flipped = text;
      std::size_t const at = random.below (length);
      flipped[at] = static_cast<char> (static_cast<unsigned char> (flipped[at])
                                       ^ (1U << random.below (8)));
      int const changed
          = bits_set (wide_hash (hash_of (lumex_string_view (text)))
                      ^ wide_hash (hash_of (lumex_string_view (flipped))));
      total += changed;
      minimum = changed < minimum ? changed : minimum;
      ++samples;
    }
  double const mean = static_cast<double> (total) / samples;
  EXPECT_GT (mean, 30.0);
  EXPECT_LT (mean, 34.0);
  EXPECT_GE (minimum, 10);
}

TEST (LumexStringViewHashTest, GivenManyRandomTexts_WhenHash_ThenTheyDiffer)
{
  random_source random (0x1234abcdU);
  std::set<std::string> texts;
  std::set<std::size_t> hashes;
  for (int i = 0; i < 20000; ++i)
    {
      std::string text;
      std::size_t const length = random.below (40);
      for (std::size_t c = 0; c < length; ++c)
        text.push_back (static_cast<char> ('a' + random.below (26)));
      if (texts.insert (text).second)
        hashes.insert (hash_of (lumex_string_view (text)));
    }
  EXPECT_EQ (hashes.size (), texts.size ());
}

TEST (LumexStringViewHashTest,
      GivenViewsAsKeys_WhenLookUp_ThenFoundByEqualText)
{
  std::string const one ("one");
  std::string const two ("two");
  std::unordered_map<lumex_string_view, int> map;
  map[lumex_string_view (one)] = 1;
  map[lumex_string_view (two)] = 2;
  std::string const probe ("xxtwoxx");
  std::unordered_map<lumex_string_view, int>::const_iterator const found
      = map.find (lumex_string_view (probe).substr (2, 3));
  ASSERT_TRUE (found != map.end ());
  EXPECT_EQ (found->second, 2);
  EXPECT_EQ (map.count (lumex_string_view ("one")), 1U);
  EXPECT_EQ (map.count (lumex_string_view ("three")), 0U);
  std::unordered_set<lumex_wstring_view> wide;
  wide.insert (lumex_wstring_view (L"a"));
  wide.insert (lumex_wstring_view (L"a"));
  EXPECT_EQ (wide.size (), 1U);
}

namespace
{
template <class T, class = void> struct has_std_hash : std::false_type
{
};
template <class T>
struct has_std_hash<T, typename std::enable_if<std::is_default_constructible<
                           std::hash<T>>::value>::type> : std::true_type
{
};
} // namespace

static_assert (has_std_hash<lumex_string_view>::value,
               "char views are hashable");
static_assert (has_std_hash<lumex_wstring_view>::value,
               "wide views are hashable");
static_assert (has_std_hash<lumex_u16string_view>::value, "");
static_assert (has_std_hash<lumex_u32string_view>::value, "");
static_assert (!has_std_hash<lumex_sv_test::caseless_view>::value,
               "a view with other traits has no std::hash: it compares through"
               " its traits, a hash of the raw characters would disagree");
static_assert (
    !has_std_hash<lumex::core::string_view::view::lumex_basic_string_view<
        char, lumex_sv_test::counting_traits>>::value,
    "no std::hash for any traits but std::char_traits");

TEST (LumexStringViewHashTest,
      GivenCaselessTraits_WhenEqualViews_ThenTheyAreEqualButNotHashable)
{
  lumex_sv_test::caseless_view const upper ("HELLO");
  lumex_sv_test::caseless_view const lower ("hello");
  EXPECT_TRUE (upper == lower);
}
