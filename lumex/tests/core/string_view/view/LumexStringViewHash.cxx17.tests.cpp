#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "LumexStringViewTestSupport.hpp"
#include "lumex/core/string_view/LumexStringView"

// From C++17 the hash of a view is the hash of the standard view of the same
// characters (which the standard requires to equal the hash of the string).

namespace
{
using lumex::core::string_view::view::lumex_u16string_view;
using lumex::core::string_view::view::lumex_u32string_view;
using lumex_sv_test::exact_copy;
using lumex_sv_test::random_source;

template <class CharT>
void
equals_standard_hash (std::size_t max_length)
{
  typedef typename lumex_sv_test::char_set<CharT>::view_t view_t;
  std::vector<std::basic_string<CharT>> const texts
      = lumex_sv_test::all_texts (lumex_sv_test::char_set<CharT>::alphabet (),
                                  max_length);
  int mismatches = 0;
  for (std::size_t i = 0; i < texts.size (); ++i)
    {
      exact_copy<CharT> copy (texts[i]);
      view_t const view (copy.data (), copy.size ());
      std::basic_string_view<CharT> const standard (copy.data (),
                                                    copy.size ());
      bool const ok
          = std::hash<view_t> () (view)
                == std::hash<std::basic_string_view<CharT>> () (standard)
            && std::hash<view_t> () (view)
                   == std::hash<std::basic_string<CharT>> () (texts[i]);
      if (!ok && ++mismatches < 10)
        ADD_FAILURE () << "text " << i;
    }
  EXPECT_EQ (mismatches, 0);
}
} // namespace

TEST (LumexStringViewHashCxx17Test,
      GivenNarrowTexts_WhenHash_ThenEqualsStdHash)
{
  equals_standard_hash<char> (5);
}

TEST (LumexStringViewHashCxx17Test, GivenWideTexts_WhenHash_ThenEqualsStdHash)
{
  equals_standard_hash<wchar_t> (4);
}

TEST (LumexStringViewHashCxx17Test,
      GivenRandomTexts_WhenHash_ThenEqualsStdHash)
{
  random_source random (0x1234abcdU);
  int mismatches = 0;
  for (int i = 0; i < 2000; ++i)
    {
      std::string text;
      std::size_t const length = random.below (200);
      for (std::size_t c = 0; c < length; ++c)
        text.push_back (static_cast<char> (random.below (256)));
      if (std::hash<lumex_string_view> () (lumex_string_view (text))
          != std::hash<std::string_view> () (std::string_view (text)))
        ++mismatches;
    }
  EXPECT_EQ (mismatches, 0);
}

TEST (LumexStringViewHashCxx17Test,
      GivenOtherCharacterTypes_WhenHash_ThenEqualsStdHash)
{
  EXPECT_EQ (
      std::hash<lumex_u16string_view> () (lumex_u16string_view (u"text")),
      std::hash<std::u16string_view> () (std::u16string_view (u"text")));
  EXPECT_EQ (
      std::hash<lumex_u32string_view> () (lumex_u32string_view (U"text")),
      std::hash<std::u32string_view> () (std::u32string_view (U"text")));
}

TEST (LumexStringViewHashCxx17Test,
      GivenNullPointerAndEmptyViews_WhenHash_ThenEqualsStdHashOfEmpty)
{
  std::size_t const empty
      = std::hash<std::string_view> () (std::string_view ());
  EXPECT_EQ (std::hash<lumex_string_view> () (lumex_string_view ()), empty);
  EXPECT_EQ (std::hash<lumex_string_view> () (lumex_string_view ("")), empty);
  EXPECT_EQ (std::hash<lumex_string_view> () (lumex_string_view (nullptr, 0)),
             empty);
}

TEST (LumexStringViewHashCxx17Test,
      GivenEmbeddedNul_WhenHash_ThenEqualsStdHash)
{
  std::string const text ("a\0b\0", 4);
  EXPECT_EQ (std::hash<lumex_string_view> () (lumex_string_view (text)),
             std::hash<std::string> () (text));
}
