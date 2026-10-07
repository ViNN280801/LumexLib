// LumexStreamTraits.cxx11.tests.cpp
// The SFINAE stream traits (is_streamable, all_streamable), which exist in
// every standard. LumexStreamTraits.cxx14.tests.cpp adds the variable
// templates, LumexStreamTraits.cxx20.tests.cpp the concepts.
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

#include "lumex/tests/core/utility/LumexStreamTraitsTestTypes.hpp"

using lumex_stream_traits_test::custom_streamable_t;
using lumex_stream_traits_test::derived_streamable_t;
using lumex_stream_traits_test::not_streamable_t;
using lumex_stream_traits_test::plain_enum_e;
using lumex_stream_traits_test::scoped_enum_e;

// The SFINAE traits exist in every standard.
using lumex::core::utility::traits::stream::all_streamable;
using lumex::core::utility::traits::stream::is_streamable;

TEST (LumexStreamTraitsSfinaeTest, GivenFundamentalTypes_ThenStreamable)
{
  EXPECT_TRUE (is_streamable<bool>::value);
  EXPECT_TRUE (is_streamable<char>::value);
  EXPECT_TRUE (is_streamable<signed char>::value);
  EXPECT_TRUE (is_streamable<unsigned char>::value);
  EXPECT_TRUE (is_streamable<wchar_t>::value);
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
