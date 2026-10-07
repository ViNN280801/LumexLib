// Base64 validator tests of the std::string_view wrappers (C++17). The C++17
// and C++20 suites of this directory compile this file together with
// LumexBase64Validator.cxx11.tests.cpp.

#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "lumex/core/base64/LumexBase64"

#include "lumex/tests/core/base64/LumexBase64TestFixtures.hpp"

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

using namespace lumex::core::base64::codec;
using namespace lumex::core::base64::decode;
using namespace lumex::core::base64::encode;
using namespace lumex::core::base64::validate;
using namespace lumex::core::base64::codec::Types;

TEST_F (Base64ValidatorTest,
        GivenDefaultConstructedStringView_WhenValidate_ThenReturnsTrue)
{
  // A default-constructed view has no data pointer but is an empty input,
  // unlike the pointer and size core called with nullptr.
  std::string_view const empty_view;
  ASSERT_EQ (empty_view.data (), nullptr);
  EXPECT_TRUE (validator::is_valid_base64 (empty_view));
}

TEST_F (Base64ValidatorTest, GivenStringView_WhenValidate_ThenWorksCorrectly)
{
  // Test C++17 string_view interface
  std::string base_string = "VGVzdERhdGE=";
  std::string_view view (base_string);

  bool result = validator::is_valid_base64 (view);
  EXPECT_TRUE (result);

  // Test substring view
  std::string longer = "PrefixVGVzdERhdGE=Suffix";
  std::string_view sub_view (longer.data () + 6, 12); // Extract "VGVzdERhdGE="

  bool result2 = validator::is_valid_base64 (sub_view);
  EXPECT_TRUE (result2);
}
