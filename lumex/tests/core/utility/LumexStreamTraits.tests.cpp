// LumexStreamTraits.tests.cpp
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

namespace
{
struct custom_streamable_t
{
  int value;
};

std::ostream &
operator<< (std::ostream &os, custom_streamable_t const &item)
{
  return os << item.value;
}

struct not_streamable_t
{
  int value;
};

enum plain_enum_e
{
  plain_value = 1
};

enum class scoped_enum_e
{
  scoped_value = 1
};

struct derived_streamable_t : custom_streamable_t
{
};
} // namespace

#if __cplusplus >= 202002L

using lumex::core::utility::traits::AllStreamable;
using lumex::core::utility::traits::Streamable;

TEST (LumexStreamTraitsConceptTest, GivenFundamentalTypes_ThenStreamable)
{
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
}

TEST (LumexStreamTraitsConceptTest, GivenStringLikeTypes_ThenStreamable)
{
  EXPECT_TRUE (Streamable<char const *>);
  EXPECT_TRUE (Streamable<char *>);
  EXPECT_TRUE (Streamable<std::string>);
  EXPECT_TRUE (Streamable<std::string const &>);
  EXPECT_TRUE (Streamable<char const (&)[4]>);
  EXPECT_TRUE (Streamable<void const *>);
}

TEST (LumexStreamTraitsConceptTest, GivenUserTypes_ThenMatchesOperatorPresence)
{
  EXPECT_TRUE (Streamable<custom_streamable_t>);
  EXPECT_TRUE (Streamable<custom_streamable_t const &>);
  EXPECT_TRUE (Streamable<derived_streamable_t>);
  EXPECT_FALSE (Streamable<not_streamable_t>);
  EXPECT_TRUE (Streamable<plain_enum_e>);
  EXPECT_FALSE (Streamable<scoped_enum_e>);
  EXPECT_FALSE (Streamable<std::vector<int>>);
  EXPECT_FALSE ((Streamable<std::map<int, int>>));
}

TEST (LumexStreamTraitsConceptTest, GivenPacks_ThenAllStreamableFoldsDecay)
{
  EXPECT_TRUE ((AllStreamable<>));
  EXPECT_TRUE ((AllStreamable<int>));
  EXPECT_TRUE ((AllStreamable<int, std::string, char const *, double>));
  EXPECT_TRUE ((AllStreamable<custom_streamable_t const &, int &&>));
  EXPECT_FALSE ((AllStreamable<not_streamable_t>));
  EXPECT_FALSE ((AllStreamable<int, not_streamable_t>));
  EXPECT_FALSE ((AllStreamable<not_streamable_t, int>));
  EXPECT_FALSE ((AllStreamable<int, std::string, std::vector<int>>));
}

#endif // __cplusplus >= 202002L

// The SFINAE traits exist in every standard.
using lumex::core::utility::traits::all_streamable;
using lumex::core::utility::traits::is_streamable;

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
// Below C++20 format::stringify streams smart pointers as addresses.
TEST (LumexStreamTraitsSfinaeTest, GivenSmartPointersPreCxx20_ThenStreamable)
{
  EXPECT_TRUE ((is_streamable<std::unique_ptr<int>>::value));
  EXPECT_TRUE (
      (is_streamable<std::unique_ptr<int, std::default_delete<int>>>::value));
  EXPECT_TRUE ((is_streamable<std::shared_ptr<not_streamable_t>>::value));
}
#else
// C++20 declares operator<< for std::unique_ptr (and std::shared_ptr has one
// since C++11), so the expression check itself finds them.
TEST (LumexStreamTraitsSfinaeTest, GivenSmartPointersCxx20_ThenStreamableByStd)
{
  EXPECT_TRUE ((is_streamable<std::unique_ptr<int>>::value));
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

#if __cplusplus >= 201402L
using lumex::core::utility::traits::all_streamable_v;
using lumex::core::utility::traits::is_streamable_v;

TEST (LumexStreamTraitsSfinaeTest, GivenVariableTemplates_ThenMatchTraits)
{
  EXPECT_TRUE (is_streamable_v<int>);
  EXPECT_TRUE (is_streamable_v<std::string const &>);
  EXPECT_TRUE (is_streamable_v<custom_streamable_t &&>);
  EXPECT_FALSE (is_streamable_v<not_streamable_t const &>);
  EXPECT_TRUE ((all_streamable_v<>));
  EXPECT_TRUE ((all_streamable_v<int, std::string const &, double>));
  EXPECT_FALSE ((all_streamable_v<int, not_streamable_t>));
}
#endif
