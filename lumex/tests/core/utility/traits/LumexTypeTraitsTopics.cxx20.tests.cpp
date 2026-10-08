// LumexTypeTraitsTopics.cxx20.tests.cpp
// The concepts of the meta, string and numeric topics of LumexTypeTraits.hpp.
// The header declares them only where the compiler and the standard library
// have them (LUMEX_HAS_STD_CONCEPTS for <concepts>, LUMEX_HAS_CONCEPTS for
// the language feature): GCC 8 accepts -std=c++2a without either, so there
// the tests skip. The suites from C++20 up compile this file together with
// LumexTypeTraitsTopics.cxx11.tests.cpp.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

namespace traits = lumex::core::utility::traits;

namespace
{
struct incomplete_t;

struct base_t
{
  virtual ~base_t () = default;
};
} // namespace

// ---------------------------------------------------------------------------
// meta
// ---------------------------------------------------------------------------

TEST (LumexTypeTraitsTopicsTest,
      GivenClassForms_WhenPointerOrRefToClass_ThenMatch)
{
#if LUMEX_HAS_STD_CONCEPTS
  EXPECT_TRUE (traits::meta::PointerToClass<base_t *>);
  EXPECT_TRUE (traits::meta::PointerToClass<base_t const *>);
  EXPECT_FALSE (traits::meta::PointerToClass<int *>);
  EXPECT_FALSE (traits::meta::PointerToClass<base_t>);
  EXPECT_FALSE (traits::meta::PointerToClass<base_t &>);

  EXPECT_TRUE (traits::meta::LvalueRefToClass<base_t &>);
  EXPECT_TRUE (traits::meta::LvalueRefToClass<base_t const &>);
  EXPECT_FALSE (traits::meta::LvalueRefToClass<base_t &&>);
  EXPECT_FALSE (traits::meta::LvalueRefToClass<int &>);
  EXPECT_FALSE (traits::meta::LvalueRefToClass<base_t *>);
#else
  GTEST_SKIP () << "the standard library has no <concepts>";
#endif
}

TEST (LumexTypeTraitsTopicsTest, GivenTypes_WhenCompleteType_ThenOnlyComplete)
{
#if LUMEX_HAS_STD_CONCEPTS
  EXPECT_TRUE (traits::meta::CompleteType<int>);
  EXPECT_TRUE (traits::meta::CompleteType<base_t>);
  EXPECT_FALSE (traits::meta::CompleteType<incomplete_t>);
  EXPECT_FALSE (traits::meta::CompleteType<void>);
#else
  GTEST_SKIP () << "the standard library has no <concepts>";
#endif
}

TEST (LumexTypeTraitsTopicsTest, GivenCvPairs_WhenPreserveCV_ThenNoCvDropped)
{
#if LUMEX_HAS_STD_CONCEPTS
  EXPECT_TRUE ((traits::meta::PreserveCV<int, int>));
  EXPECT_TRUE ((traits::meta::PreserveCV<int, int const>));
  EXPECT_TRUE ((traits::meta::PreserveCV<int const, int const volatile>));
  EXPECT_FALSE ((traits::meta::PreserveCV<int const, int>));
  EXPECT_FALSE ((traits::meta::PreserveCV<int volatile, int const>));
#else
  GTEST_SKIP () << "the standard library has no <concepts>";
#endif
}

TEST (LumexTypeTraitsTopicsTest, GivenTypes_WhenExtractible_ThenPodValuesOnly)
{
#if LUMEX_HAS_STD_CONCEPTS
  EXPECT_TRUE (traits::meta::Extractible<int>);
  EXPECT_TRUE (traits::meta::Extractible<double>);
  EXPECT_TRUE ((traits::meta::Extractible<std::array<char, 4>>));
  EXPECT_FALSE (traits::meta::Extractible<int *>);
  EXPECT_FALSE (traits::meta::Extractible<int &>);
  EXPECT_FALSE (traits::meta::Extractible<std::string>);
  EXPECT_FALSE (traits::meta::Extractible<base_t>);
#else
  GTEST_SKIP () << "the standard library has no <concepts>";
#endif
}

namespace
{
struct concept_pair_t
{
  std::int16_t low;
  std::int16_t high;
};

using concept_array_t = std::array<char, 4>;
} // namespace

TEST (LumexTypeTraitsTopicsTest, GivenTypes_WhenTraitAndConcept_ThenTheyAgree)
{
#if LUMEX_HAS_STD_CONCEPTS
  // The C++11 traits are the same sets as the concepts (the concepts stay for
  // their users, LumexMemRead.hpp uses the traits in every standard).
#define LUMEX_TRAITS_AGREE(TRAIT, CONCEPT, TYPE)                              \
  EXPECT_EQ (traits::meta::TRAIT<TYPE>::value, traits::meta::CONCEPT<TYPE>)
  LUMEX_TRAITS_AGREE (is_extractible, Extractible, int);
  LUMEX_TRAITS_AGREE (is_extractible, Extractible, double);
  LUMEX_TRAITS_AGREE (is_extractible, Extractible, concept_pair_t);
  LUMEX_TRAITS_AGREE (is_extractible, Extractible, int *);
  LUMEX_TRAITS_AGREE (is_extractible, Extractible, int &);
  LUMEX_TRAITS_AGREE (is_extractible, Extractible, std::string);
  LUMEX_TRAITS_AGREE (is_extractible, Extractible, base_t);
  LUMEX_TRAITS_AGREE (is_extractible, Extractible, concept_array_t);
  LUMEX_TRAITS_AGREE (is_byte_like, ByteLike, std::byte);
  LUMEX_TRAITS_AGREE (is_byte_like, ByteLike, char);
  LUMEX_TRAITS_AGREE (is_byte_like, ByteLike, unsigned char);
  LUMEX_TRAITS_AGREE (is_byte_like, ByteLike, signed char);
  LUMEX_TRAITS_AGREE (is_byte_like, ByteLike, std::uint16_t);
  LUMEX_TRAITS_AGREE (is_byte_like, ByteLike, char const);
#undef LUMEX_TRAITS_AGREE
#else
  GTEST_SKIP () << "the standard library has no <concepts>";
#endif
}

TEST (LumexTypeTraitsTopicsTest, GivenTypes_WhenByteLike_ThenOnlyByteTypes)
{
#if LUMEX_HAS_STD_CONCEPTS
  EXPECT_TRUE (traits::meta::ByteLike<std::byte>);
  EXPECT_TRUE (traits::meta::ByteLike<char>);
  EXPECT_TRUE (traits::meta::ByteLike<unsigned char>);
  EXPECT_FALSE (traits::meta::ByteLike<signed char>);
  EXPECT_FALSE (traits::meta::ByteLike<std::uint16_t>);
  EXPECT_FALSE (traits::meta::ByteLike<char const>);
#else
  GTEST_SKIP () << "the standard library has no <concepts>";
#endif
}

// ---------------------------------------------------------------------------
// string
// ---------------------------------------------------------------------------

TEST (LumexTypeTraitsTopicsTest,
      GivenTypes_WhenStringLikeConcept_ThenConvertible)
{
#if LUMEX_HAS_STD_CONCEPTS
  EXPECT_TRUE (traits::string::StringLike<std::string>);
  EXPECT_TRUE (traits::string::StringLike<char const *>);
  EXPECT_TRUE (traits::string::StringLike<std::string_view>);
  EXPECT_TRUE ((traits::string::StringLike<char const (&)[4]>));
  EXPECT_FALSE (traits::string::StringLike<int>);
  EXPECT_FALSE (traits::string::StringLike<std::wstring>);
#else
  GTEST_SKIP () << "the standard library has no <concepts>";
#endif
}

// ---------------------------------------------------------------------------
// numeric
// ---------------------------------------------------------------------------

TEST (LumexTypeTraitsTopicsTest,
      GivenTypes_WhenArithmeticConcepts_ThenMatchTrait)
{
#if LUMEX_HAS_CONCEPTS
  EXPECT_TRUE (traits::numeric::ArithmeticType<int>);
  EXPECT_TRUE (traits::numeric::ArithmeticType<long double>);
  EXPECT_FALSE (traits::numeric::ArithmeticType<int *>);
  EXPECT_FALSE (traits::numeric::ArithmeticType<int const &>);
  EXPECT_TRUE ((traits::numeric::SafeComparable<int, double>));
  EXPECT_FALSE ((traits::numeric::SafeComparable<int, std::string>));
#else
  GTEST_SKIP () << "the compiler has no concepts";
#endif
}
