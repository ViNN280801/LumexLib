// LumexStreamTraits.cxx20.tests.cpp
// The stream concepts (Streamable, AllStreamable) and the C++20 answer of
// is_streamable for smart pointers. The suites from C++20 up compile this
// file together with the .cxx11 and .cxx14 files. The concepts exist only
// where the compiler has them (LUMEX_HAS_CONCEPTS): GCC 8 accepts
// -std=c++2a without them, so there the concept tests skip.
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

#include "lumex/tests/core/utility/LumexStreamTraitsTestTypes.hpp"

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
