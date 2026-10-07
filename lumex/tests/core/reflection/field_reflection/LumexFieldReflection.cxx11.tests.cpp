// LumexFieldReflection.cxx11.tests.cpp
//
// Built when LUMEX_WITH_FIELD_REFLECTION is ON and
// nlohmann_json::nlohmann_json exists. The same macro gates the body
// so clangd on the C++11 sibling flags does not report missing headers.
// The field count (tuple_size) from C++11; every suite of the module
// compiles this file. Indexed get is in LumexFieldReflection.cxx14.tests.cpp,
// names and to_json in LumexFieldReflection.cxx20.tests.cpp.
#if defined(LUMEX_WITH_FIELD_REFLECTION)

#include <cstddef>

#include <gtest/gtest.h>

#include "lumex/core/reflection/LumexReflection"

#include "lumex/tests/core/reflection/LumexFieldReflectionTestFixtures.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

#if defined(__clang__)
#endif

using namespace lumex::core::reflection::field_reflection;
using namespace lumex_field_reflection_tests;

TEST (LumexAggregateFieldsTest, GivenEmptyAggregate_WhenSized_ThenZero)
{
  EXPECT_EQ (tuple_size<Empty>::value, 0u);
}

TEST (LumexAggregateFieldsTest, GivenOneField_WhenSized_ThenOne)
{
  EXPECT_EQ (tuple_size<OneField>::value, 1u);
}

TEST (LumexAggregateFieldsTest, GivenPlainAggregate_WhenSized_ThenTwo)
{
  EXPECT_EQ (tuple_size<Plain>::value, 2u);
}

TEST (LumexAggregateFieldsTest, GivenPaddedAggregate_WhenSized_ThenThree)
{
  EXPECT_EQ (tuple_size<Padded>::value, 3u);
}

TEST (LumexAggregateFieldsTest, GivenEightFields_WhenSized_ThenEight)
{
  EXPECT_EQ (tuple_size<EightFields>::value, 8u);
}

TEST (LumexAggregateFieldsTest, GivenTupleSizeValue_WhenAddressTaken_ThenLinks)
{
  // A volatile pointer keeps the reference to the `value` symbol in an
  // optimized build too, so a member without a definition fails to link
  // here in every build type, not only without optimization.
  std::size_t const *volatile address = &tuple_size<Plain>::value;
  EXPECT_EQ (*address, 2u);
  address = &tuple_size<Empty>::value;
  EXPECT_EQ (*address, 0u);
}

TYPED_TEST (LumexFieldArityTest, GivenArity_WhenTupleSize_ThenMatches)
{
  typedef TypeParam tag_t;
  typedef typename fields_of<tag_t::value>::type agg_t;
  EXPECT_EQ (tuple_size<agg_t>::value, tag_t::value);
}

#endif // defined(LUMEX_WITH_FIELD_REFLECTION)
