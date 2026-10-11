#include <cstddef>
#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <gtest/gtest.h>

#include "LumexStringViewTestSupport.hpp"
#include "lumex/core/string_view/LumexStringView"

// std::hash of the views. Below C++17 (and for traits other than
// std::char_traits) it is a deterministic FNV-1a over the code units; from
// C++17 it is the hash of the standard view (LumexStringViewHash.cxx17).

namespace
{
using lumex::core::string_view::view::detail::fnv1a;
using lumex_sv_test::random_source;

template <class View>
std::size_t
hash_of (View const &view)
{
  return std::hash<View> () (view);
}
} // namespace

TEST (LumexStringViewHashTest, GivenKnownVectors_WhenFnv1a_ThenPublishedValues)
{
  // The 64-bit FNV-1a test vectors (draft-eastlake-fnv).
  if (sizeof (std::size_t) < 8)
    GTEST_SKIP () << "the vectors are 64-bit";
  EXPECT_EQ (fnv1a ("", 0), static_cast<std::size_t> (0xcbf29ce484222325ULL));
  EXPECT_EQ (fnv1a ("a", 1), static_cast<std::size_t> (0xaf63dc4c8601ec8cULL));
  EXPECT_EQ (fnv1a ("foobar", 6),
             static_cast<std::size_t> (0x85944171f73967e8ULL));
  EXPECT_NE (fnv1a ("foobar", 5), fnv1a ("foobar", 6))
      << "the size is the length of the hashed text";
}

TEST (LumexStringViewHashTest,
      GivenCodeUnits_WhenFnv1aOfWide_ThenEachUnitIsOneStep)
{
  if (sizeof (std::size_t) < 8)
    GTEST_SKIP () << "the vectors are 64-bit";
  EXPECT_EQ (fnv1a (L"foobar", 6),
             static_cast<std::size_t> (0x85944171f73967e8ULL));
  EXPECT_EQ (fnv1a (u"foobar", 6), fnv1a (L"foobar", 6));
  EXPECT_EQ (fnv1a (U"a", 1),
             static_cast<std::size_t> (0xaf63dc4c8601ec8cULL));
  EXPECT_NE (fnv1a (L"\x20AC", 1), fnv1a (L"\x20", 1))
      << "the whole code unit takes part";
}

TEST (LumexStringViewHashTest, GivenHighBitChar_WhenFnv1a_ThenHashedAsUnsigned)
{
  if (sizeof (std::size_t) < 8)
    GTEST_SKIP () << "the vectors are 64-bit";
  // 0x80 is negative as a char; the hash is over the unsigned unit, so that
  // it does not depend on the signedness of char.
  std::size_t const expected = static_cast<std::size_t> (
      (0xcbf29ce484222325ULL ^ 0x80ULL) * 1099511628211ULL);
  EXPECT_EQ (fnv1a ("\x80", 1), expected);
}

TEST (LumexStringViewHashTest,
      GivenEmbeddedNul_WhenHash_ThenAllCharactersCount)
{
  EXPECT_NE (hash_of (lumex_string_view ("a\0b", 3)),
             hash_of (lumex_string_view ("a", 1)));
  EXPECT_NE (hash_of (lumex_string_view ("a\0b", 3)),
             hash_of (lumex_string_view ("a\0c", 3)));
  EXPECT_NE (hash_of (lumex_string_view ("\0", 1)),
             hash_of (lumex_string_view ()));
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

TEST (LumexStringViewHashTest, GivenManyTexts_WhenHash_ThenTheyDiffer)
{
  random_source random (0x4a5b6c7dU);
  std::set<std::string> texts;
  std::set<std::size_t> hashes;
  for (int i = 0; i < 2000; ++i)
    {
      std::string text;
      std::size_t const length = random.below (24);
      for (std::size_t c = 0; c < length; ++c)
        text.push_back (static_cast<char> ('a' + random.below (26)));
      if (texts.insert (text).second)
        hashes.insert (hash_of (lumex_string_view (text)));
    }
  EXPECT_EQ (hashes.size (), texts.size ()) << "no collision among 2000 texts";
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

#if __cplusplus < 201703L
TEST (LumexStringViewHashTest,
      GivenBeforeCxx17_WhenStdHash_ThenFnv1aOfTheUnits)
{
  if (sizeof (std::size_t) < 8)
    GTEST_SKIP () << "the vectors are 64-bit";
  EXPECT_EQ (hash_of (lumex_string_view ("foobar")),
             static_cast<std::size_t> (0x85944171f73967e8ULL));
  EXPECT_EQ (hash_of (lumex_wstring_view (L"foobar")),
             static_cast<std::size_t> (0x85944171f73967e8ULL));
  EXPECT_EQ (hash_of (lumex_string_view ("")),
             static_cast<std::size_t> (0xcbf29ce484222325ULL));
  EXPECT_EQ (hash_of (lumex_string_view ("foobar").substr (1, 4)),
             fnv1a ("ooba", 4));
}
#endif
