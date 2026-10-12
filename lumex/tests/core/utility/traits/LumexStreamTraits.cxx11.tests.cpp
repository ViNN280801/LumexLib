// LumexStreamTraits.cxx11.tests.cpp
// The SFINAE stream traits (is_streamable, all_streamable), which exist in
// every standard. LumexStreamTraits.cxx14.tests.cpp adds the variable
// templates, LumexStreamTraits.cxx20.tests.cpp the concepts.
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

#include "lumex/tests/core/utility/traits/LumexStreamTraitsTestTypes.hpp"
#include "lumex/tests/support/LumexOstreamProbe.hpp"

using lumex_stream_traits_test::custom_streamable_t;
using lumex_stream_traits_test::derived_streamable_t;
using lumex_stream_traits_test::not_streamable_t;
using lumex_stream_traits_test::plain_enum_e;
using lumex_stream_traits_test::scoped_enum_e;
using lumex_tests_support::ostream_accepts;
using lumex_tests_support::streamed;

// The SFINAE traits exist in every standard.
using lumex::core::utility::traits::stream::all_streamable;
using lumex::core::utility::traits::stream::is_streamable;

TEST (LumexStreamTraitsSfinaeTest, GivenFundamentalTypes_ThenStreamable)
{
  EXPECT_TRUE (is_streamable<bool>::value);
  EXPECT_TRUE (is_streamable<char>::value);
  EXPECT_TRUE (is_streamable<signed char>::value);
  EXPECT_TRUE (is_streamable<unsigned char>::value);
  EXPECT_TRUE (is_streamable<short>::value);
  EXPECT_TRUE (is_streamable<unsigned short>::value);
  EXPECT_TRUE (is_streamable<int>::value);
  EXPECT_TRUE (is_streamable<unsigned int>::value);
  EXPECT_TRUE (is_streamable<long>::value);
  EXPECT_TRUE (is_streamable<unsigned long>::value);
  EXPECT_TRUE (is_streamable<long long>::value);
  EXPECT_TRUE (is_streamable<unsigned long long>::value);
  EXPECT_TRUE (is_streamable<float>::value);
  EXPECT_TRUE (is_streamable<double>::value);
  EXPECT_TRUE (is_streamable<long double>::value);
}

// The character types other than char have no explicit specialization: a
// narrow stream takes them as numbers (or addresses, for the pointers) up to
// C++17 and the library deletes those operators from C++20 (P1423R3), so the
// answer is the library's, whatever the standard. The detector asks the
// compiler for the same expression, with no trait of the library.
TEST (LumexStreamTraitsSfinaeTest,
      GivenCharacterTypes_ThenTheTraitIsWhatTheStreamAccepts)
{
  EXPECT_EQ (is_streamable<wchar_t>::value, ostream_accepts<wchar_t>::value);
  EXPECT_EQ (is_streamable<char16_t>::value, ostream_accepts<char16_t>::value);
  EXPECT_EQ (is_streamable<char32_t>::value, ostream_accepts<char32_t>::value);
  EXPECT_EQ (is_streamable<wchar_t *>::value,
             ostream_accepts<wchar_t *>::value);
  EXPECT_EQ (is_streamable<wchar_t const *>::value,
             ostream_accepts<wchar_t const *>::value);
  EXPECT_EQ (is_streamable<char16_t const *>::value,
             ostream_accepts<char16_t const *>::value);
  EXPECT_EQ (is_streamable<char32_t const *>::value,
             ostream_accepts<char32_t const *>::value);
#if defined(__cpp_char8_t)
  EXPECT_EQ (is_streamable<char8_t>::value, ostream_accepts<char8_t>::value);
  EXPECT_EQ (is_streamable<char8_t const *>::value,
             ostream_accepts<char8_t const *>::value);
#endif
}

// Strings of the wide types are never written to a narrow stream.
TEST (LumexStreamTraitsSfinaeTest, GivenWideStrings_ThenNotStreamable)
{
  EXPECT_FALSE (is_streamable<std::wstring>::value);
  EXPECT_FALSE (is_streamable<std::u16string>::value);
  EXPECT_FALSE (is_streamable<std::u32string>::value);
}

#if __cplusplus < 202002L
// Up to C++17 the member and non-member inserters of a narrow stream take
// wchar_t, char16_t and char32_t by integral promotion: the number is written
// (the pointers are written as an address), and so the traits say true.
TEST (LumexStreamTraitsSfinaeTest,
      GivenCxx11To17_ThenWideCharactersAreStreamableAsNumbers)
{
  EXPECT_TRUE (is_streamable<wchar_t>::value);
  EXPECT_TRUE (is_streamable<char16_t>::value);
  EXPECT_TRUE (is_streamable<char32_t>::value);
  EXPECT_TRUE (is_streamable<wchar_t const *>::value);
  EXPECT_TRUE (is_streamable<char16_t const *>::value);
  EXPECT_TRUE (is_streamable<char32_t const *>::value);
  EXPECT_EQ (streamed (L'A'), "65");
  EXPECT_EQ (streamed (u'A'), "65");
  EXPECT_EQ (streamed (U'A'), "65");
#if defined(__cpp_char8_t)
  EXPECT_TRUE (is_streamable<char8_t>::value);
  EXPECT_TRUE (is_streamable<char8_t const *>::value);
  EXPECT_EQ (streamed (u8'A'), "65");
#endif
}
#endif

#if LUMEX_TEST_OSTREAM_DELETES_WIDE_CHARACTERS
// From C++20 (libstdc++ 13, libc++ 23) the inserters are deleted: no overload.
TEST (LumexStreamTraitsSfinaeTest,
      GivenCxx20Library_ThenWideCharactersAreNotStreamable)
{
  EXPECT_FALSE (is_streamable<wchar_t>::value);
  EXPECT_FALSE (is_streamable<char16_t>::value);
  EXPECT_FALSE (is_streamable<char32_t>::value);
  EXPECT_FALSE (is_streamable<wchar_t *>::value);
  EXPECT_FALSE (is_streamable<wchar_t const *>::value);
  EXPECT_FALSE (is_streamable<char16_t const *>::value);
  EXPECT_FALSE (is_streamable<char32_t const *>::value);
  EXPECT_FALSE (is_streamable<wchar_t const (&)[3]>::value);
#if defined(__cpp_char8_t)
  EXPECT_FALSE (is_streamable<char8_t>::value);
  EXPECT_FALSE (is_streamable<char8_t const *>::value);
#endif
  // The neighbours are not touched.
  EXPECT_TRUE (is_streamable<char>::value);
  EXPECT_TRUE (is_streamable<signed char>::value);
  EXPECT_TRUE (is_streamable<unsigned char>::value);
  EXPECT_TRUE (is_streamable<char const *>::value);
}
#endif

// std::nullptr_t has an inserter from C++17 (LWG 2221), in the libraries that
// implement it (libstdc++ 12 and libc++ do, libstdc++ 8 does not).
TEST (LumexStreamTraitsSfinaeTest, GivenNullptrType_ThenWhatTheStreamAccepts)
{
  EXPECT_EQ (is_streamable<std::nullptr_t>::value,
             ostream_accepts<std::nullptr_t>::value);
#if __cplusplus < 201703L
  EXPECT_FALSE (is_streamable<std::nullptr_t>::value);
#endif
}

TEST (LumexStreamTraitsSfinaeTest, GivenStringLikeTypes_ThenStreamable)
{
  EXPECT_TRUE (is_streamable<char const *>::value);
  EXPECT_TRUE (is_streamable<char *>::value);
  EXPECT_TRUE (is_streamable<std::string>::value);
  EXPECT_TRUE (is_streamable<void const *>::value);
}

#if __cplusplus < 202002L
// Below C++20 utility::stringify streams smart pointers as addresses.
TEST (LumexStreamTraitsSfinaeTest, GivenSmartPointersPreCxx20_ThenStreamable)
{
  EXPECT_TRUE ((is_streamable<std::unique_ptr<int>>::value));
  EXPECT_TRUE (
      (is_streamable<std::unique_ptr<int, std::default_delete<int>>>::value));
  EXPECT_TRUE ((is_streamable<std::shared_ptr<not_streamable_t>>::value));
}
#endif

TEST (LumexStreamTraitsSfinaeTest, GivenPointerToNonStreamable_ThenAddress)
{
  // A raw pointer streams as void const *, whatever it points to.
  EXPECT_TRUE (is_streamable<not_streamable_t *>::value);
}

TEST (LumexStreamTraitsSfinaeTest, GivenUserTypes_ThenMatchesOperatorPresence)
{
  EXPECT_TRUE (is_streamable<custom_streamable_t>::value);
  EXPECT_TRUE (is_streamable<derived_streamable_t>::value);
  EXPECT_FALSE (is_streamable<not_streamable_t>::value);
  EXPECT_TRUE (is_streamable<plain_enum_e>::value);
  EXPECT_FALSE (is_streamable<scoped_enum_e>::value);
  EXPECT_FALSE (is_streamable<std::vector<int>>::value);
  EXPECT_FALSE ((is_streamable<std::map<int, int>>::value));
}

TEST (LumexStreamTraitsSfinaeTest, GivenTraits_ThenDeriveFromBoolConstants)
{
  EXPECT_TRUE ((std::is_base_of<std::true_type, is_streamable<int>>::value));
  EXPECT_TRUE ((std::is_base_of<std::false_type,
                                is_streamable<not_streamable_t>>::value));
}

TEST (LumexStreamTraitsSfinaeTest, GivenPacks_ThenAllStreamableRecursesDecay)
{
  EXPECT_TRUE (all_streamable<>::value);
  EXPECT_TRUE ((all_streamable<int>::value));
  EXPECT_TRUE ((all_streamable<int, std::string, char const *>::value));
  EXPECT_TRUE ((all_streamable<custom_streamable_t const &, int &&>::value));
  EXPECT_FALSE ((all_streamable<not_streamable_t>::value));
  EXPECT_FALSE ((all_streamable<int, not_streamable_t>::value));
  EXPECT_FALSE ((all_streamable<not_streamable_t, int>::value));
  EXPECT_FALSE ((all_streamable<int, std::string, std::vector<int>>::value));
}
