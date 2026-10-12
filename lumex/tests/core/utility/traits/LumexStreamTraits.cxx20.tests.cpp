// LumexStreamTraits.cxx20.tests.cpp
// The stream concepts (Streamable, AllStreamable) and the C++20 answer of
// is_streamable for smart pointers. The suites from C++20 up compile this
// file together with the .cxx11 and .cxx14 files. The concepts exist only
// where the compiler has them (LUMEX_HAS_CONCEPTS): GCC 8 accepts
// -std=c++2a without them, so there the concept tests skip.
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

#include "lumex/tests/core/utility/traits/LumexStreamTraitsTestTypes.hpp"
#include "lumex/tests/support/LumexOstreamProbe.hpp"

using lumex_stream_traits_test::custom_streamable_t;
using lumex_stream_traits_test::derived_streamable_t;
using lumex_stream_traits_test::not_streamable_t;
using lumex_stream_traits_test::plain_enum_e;
using lumex_stream_traits_test::scoped_enum_e;

#if LUMEX_HAS_CONCEPTS
using lumex::core::utility::traits::stream::AllStreamable;
using lumex::core::utility::traits::stream::Streamable;
#endif

TEST (LumexStreamTraitsConceptTest, GivenFundamentalTypes_ThenStreamable)
{
#if LUMEX_HAS_CONCEPTS
  EXPECT_TRUE (Streamable<bool>);
  EXPECT_TRUE (Streamable<char>);
  EXPECT_TRUE (Streamable<signed char>);
  EXPECT_TRUE (Streamable<unsigned char>);
  EXPECT_TRUE (Streamable<short>);
  EXPECT_TRUE (Streamable<unsigned short>);
  EXPECT_TRUE (Streamable<int>);
  EXPECT_TRUE (Streamable<unsigned int>);
  EXPECT_TRUE (Streamable<long>);
  EXPECT_TRUE (Streamable<unsigned long>);
  EXPECT_TRUE (Streamable<long long>);
  EXPECT_TRUE (Streamable<unsigned long long>);
  EXPECT_TRUE (Streamable<float>);
  EXPECT_TRUE (Streamable<double>);
  EXPECT_TRUE (Streamable<long double>);
#else
  GTEST_SKIP () << "the compiler has no concepts";
#endif
}

TEST (LumexStreamTraitsConceptTest, GivenStringLikeTypes_ThenStreamable)
{
#if LUMEX_HAS_CONCEPTS
  EXPECT_TRUE (Streamable<char const *>);
  EXPECT_TRUE (Streamable<char *>);
  EXPECT_TRUE (Streamable<std::string>);
  EXPECT_TRUE (Streamable<std::string const &>);
  EXPECT_TRUE (Streamable<char const (&)[4]>);
  EXPECT_TRUE (Streamable<void const *>);
#else
  GTEST_SKIP () << "the compiler has no concepts";
#endif
}

TEST (LumexStreamTraitsConceptTest, GivenUserTypes_ThenMatchesOperatorPresence)
{
#if LUMEX_HAS_CONCEPTS
  EXPECT_TRUE (Streamable<custom_streamable_t>);
  EXPECT_TRUE (Streamable<custom_streamable_t const &>);
  EXPECT_TRUE (Streamable<derived_streamable_t>);
  EXPECT_FALSE (Streamable<not_streamable_t>);
  EXPECT_TRUE (Streamable<plain_enum_e>);
  EXPECT_FALSE (Streamable<scoped_enum_e>);
  EXPECT_FALSE (Streamable<std::vector<int>>);
  EXPECT_FALSE ((Streamable<std::map<int, int>>));
#else
  GTEST_SKIP () << "the compiler has no concepts";
#endif
}

TEST (LumexStreamTraitsConceptTest, GivenPacks_ThenAllStreamableFoldsDecay)
{
#if LUMEX_HAS_CONCEPTS
  EXPECT_TRUE ((AllStreamable<>));
  EXPECT_TRUE ((AllStreamable<int>));
  EXPECT_TRUE ((AllStreamable<int, std::string, char const *, double>));
  EXPECT_TRUE ((AllStreamable<custom_streamable_t const &, int &&>));
  EXPECT_FALSE ((AllStreamable<not_streamable_t>));
  EXPECT_FALSE ((AllStreamable<int, not_streamable_t>));
  EXPECT_FALSE ((AllStreamable<not_streamable_t, int>));
  EXPECT_FALSE ((AllStreamable<int, std::string, std::vector<int>>));
#else
  GTEST_SKIP () << "the compiler has no concepts";
#endif
}

using lumex::core::utility::traits::stream::is_streamable;

// C++20 declares operator<< for std::unique_ptr (and std::shared_ptr has one
// since C++11), so the expression check itself finds them.
TEST (LumexStreamTraitsSfinaeTest, GivenSmartPointersCxx20_ThenStreamableByStd)
{
  EXPECT_TRUE ((is_streamable<std::unique_ptr<int>>::value));
  EXPECT_TRUE ((is_streamable<std::shared_ptr<not_streamable_t>>::value));
}

// The trait is the concept's set of types, in the way of the other twins of
// LumexTypeTraitsTopics.cxx20.tests.cpp: `is_streamable` is not decayed, as
// the concept is not. The set is every fundamental type, the character types
// whose answer changes with the standard (wchar_t, char8_t, char16_t,
// char32_t and the pointers to them), the string types and some types that are
// not streamable. The smart pointers are not here on purpose: the trait names
// them streamable for the stringify overloads in every library, the concept
// follows the library (AllStringifiable adds them, see LumexString.cxx20).
namespace
{
using wide_array_t = wchar_t const (&)[4];
using narrow_array_t = char const (&)[4];
using function_pointer_t = int (*) ();
} // namespace

TEST (LumexStreamTraitsConceptTest,
      GivenFundamentalAndStringTypes_ThenTraitAndConceptAgree)
{
#if LUMEX_HAS_CONCEPTS
  namespace stream = lumex::core::utility::traits::stream;
#define LUMEX_STREAM_TRAITS_AGREE(TYPE)                                       \
  EXPECT_EQ (stream::is_streamable<TYPE>::value, stream::Streamable<TYPE>)    \
      << #TYPE
  LUMEX_STREAM_TRAITS_AGREE (bool);
  LUMEX_STREAM_TRAITS_AGREE (char);
  LUMEX_STREAM_TRAITS_AGREE (signed char);
  LUMEX_STREAM_TRAITS_AGREE (unsigned char);
  LUMEX_STREAM_TRAITS_AGREE (wchar_t);
#if defined(__cpp_char8_t)
  LUMEX_STREAM_TRAITS_AGREE (char8_t);
  LUMEX_STREAM_TRAITS_AGREE (char8_t const *);
#endif
  LUMEX_STREAM_TRAITS_AGREE (char16_t);
  LUMEX_STREAM_TRAITS_AGREE (char32_t);
  LUMEX_STREAM_TRAITS_AGREE (short);
  LUMEX_STREAM_TRAITS_AGREE (unsigned short);
  LUMEX_STREAM_TRAITS_AGREE (int);
  LUMEX_STREAM_TRAITS_AGREE (unsigned int);
  LUMEX_STREAM_TRAITS_AGREE (long);
  LUMEX_STREAM_TRAITS_AGREE (unsigned long);
  LUMEX_STREAM_TRAITS_AGREE (long long);
  LUMEX_STREAM_TRAITS_AGREE (unsigned long long);
  LUMEX_STREAM_TRAITS_AGREE (float);
  LUMEX_STREAM_TRAITS_AGREE (double);
  LUMEX_STREAM_TRAITS_AGREE (long double);
  LUMEX_STREAM_TRAITS_AGREE (std::nullptr_t);
  LUMEX_STREAM_TRAITS_AGREE (void);
  LUMEX_STREAM_TRAITS_AGREE (void *);
  LUMEX_STREAM_TRAITS_AGREE (void const *);
  LUMEX_STREAM_TRAITS_AGREE (int *);
  LUMEX_STREAM_TRAITS_AGREE (int const *);
  LUMEX_STREAM_TRAITS_AGREE (function_pointer_t);
  LUMEX_STREAM_TRAITS_AGREE (char *);
  LUMEX_STREAM_TRAITS_AGREE (char const *);
  LUMEX_STREAM_TRAITS_AGREE (signed char const *);
  LUMEX_STREAM_TRAITS_AGREE (unsigned char const *);
  LUMEX_STREAM_TRAITS_AGREE (wchar_t *);
  LUMEX_STREAM_TRAITS_AGREE (wchar_t const *);
  LUMEX_STREAM_TRAITS_AGREE (char16_t const *);
  LUMEX_STREAM_TRAITS_AGREE (char32_t const *);
  LUMEX_STREAM_TRAITS_AGREE (narrow_array_t);
  LUMEX_STREAM_TRAITS_AGREE (wide_array_t);
  LUMEX_STREAM_TRAITS_AGREE (int &);
  LUMEX_STREAM_TRAITS_AGREE (int const &);
  LUMEX_STREAM_TRAITS_AGREE (int &&);
  LUMEX_STREAM_TRAITS_AGREE (wchar_t const &);
  LUMEX_STREAM_TRAITS_AGREE (std::string);
  LUMEX_STREAM_TRAITS_AGREE (std::string const &);
  LUMEX_STREAM_TRAITS_AGREE (std::string_view);
  LUMEX_STREAM_TRAITS_AGREE (std::wstring);
  LUMEX_STREAM_TRAITS_AGREE (std::u16string);
  LUMEX_STREAM_TRAITS_AGREE (std::u32string);
  LUMEX_STREAM_TRAITS_AGREE (std::wstring_view);
  LUMEX_STREAM_TRAITS_AGREE (std::byte);
  LUMEX_STREAM_TRAITS_AGREE (plain_enum_e);
  LUMEX_STREAM_TRAITS_AGREE (scoped_enum_e);
  LUMEX_STREAM_TRAITS_AGREE (custom_streamable_t);
  LUMEX_STREAM_TRAITS_AGREE (derived_streamable_t);
  LUMEX_STREAM_TRAITS_AGREE (not_streamable_t);
#undef LUMEX_STREAM_TRAITS_AGREE
#else
  GTEST_SKIP () << "the compiler has no concepts";
#endif
}

TEST (LumexStreamTraitsConceptTest,
      GivenPacks_ThenAllStreamableAgreesWithConcept)
{
#if LUMEX_HAS_CONCEPTS
  namespace stream = lumex::core::utility::traits::stream;
#define LUMEX_STREAM_PACK_AGREE(...)                                          \
  EXPECT_EQ ((stream::all_streamable<__VA_ARGS__>::value),                    \
             (stream::AllStreamable<__VA_ARGS__>))                            \
      << #__VA_ARGS__
  LUMEX_STREAM_PACK_AGREE ();
  LUMEX_STREAM_PACK_AGREE (int);
  LUMEX_STREAM_PACK_AGREE (wchar_t);
  LUMEX_STREAM_PACK_AGREE (int, wchar_t);
  LUMEX_STREAM_PACK_AGREE (wchar_t, int);
  LUMEX_STREAM_PACK_AGREE (int, char16_t, double);
  LUMEX_STREAM_PACK_AGREE (char32_t const &, std::string);
  LUMEX_STREAM_PACK_AGREE (wchar_t const *, char const *);
  LUMEX_STREAM_PACK_AGREE (int, std::string, char const *, double);
  LUMEX_STREAM_PACK_AGREE (custom_streamable_t const &, int &&);
  LUMEX_STREAM_PACK_AGREE (int, not_streamable_t);
#undef LUMEX_STREAM_PACK_AGREE
#else
  GTEST_SKIP () << "the compiler has no concepts";
#endif
}
