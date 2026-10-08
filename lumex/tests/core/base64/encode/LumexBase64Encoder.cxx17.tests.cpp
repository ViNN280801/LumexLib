// Base64 encoder tests of the std::string_view wrappers (C++17), also with the
// lumex_string_view of the library, which converts to std::string_view. The
// C++17 and C++20 suites of this directory compile this file together with
// LumexBase64Encoder.cxx11.tests.cpp.

#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "lumex/core/base64/LumexBase64"
#include "lumex/core/string_view/LumexStringView"

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

TEST_F (Base64EncoderTest,
        GivenStringView_WhenEncode_ThenProducesCorrectOutput)
{
  std::string text = "Test string";
  std::string_view view (text);
  std::string result = encoder::encode (view);
  EXPECT_EQ (result, "VGVzdCBzdHJpbmc=");
}

TEST_F (Base64EncoderTest,
        GivenStringViewWithEmbeddedNul_WhenEncode_ThenEncodesTheWholeView)
{
  // The view is sized: its NUL is a byte, not the end of the input.
  std::string_view const view ("a\0b", 3);
  EXPECT_EQ (encoder::encode (view), "YQBi");
}

TEST_F (Base64EncoderTest,
        GivenStringViewSubRange_WhenEncode_ThenOnlyTheViewIsEncoded)
{
  std::string const longer = "xxHiyy";
  EXPECT_EQ (encoder::encode (std::string_view (longer).substr (2, 2)),
             "SGk=");
}

TEST_F (Base64EncoderTest,
        GivenDefaultConstructedStringView_WhenEncode_ThenReturnsEmptyString)
{
  EXPECT_TRUE (encoder::encode (std::string_view ()).empty ());
}

TEST_F (Base64EncoderTest,
        GivenLumexStringView_WhenEncode_ThenSameAsStdStringView)
{
  // The view of the library converts to std::string_view, so it is accepted
  // where the overload takes std::string_view, next to the std::string, char
  // const * and span overloads (none of them becomes ambiguous).
  std::string const text = "Test string";
  lumex_string_view const view (text);
  EXPECT_EQ (encoder::encode (view), "VGVzdCBzdHJpbmc=");
  EXPECT_EQ (encoder::encode (view),
             encoder::encode (std::string_view (text)));
  EXPECT_EQ (encoder::encode (view.substr (5)), "c3RyaW5n");
  std::string const zeros ("a\0b", 3);
  EXPECT_EQ (encoder::encode (lumex_string_view (zeros)), "YQBi");
  EXPECT_TRUE (encoder::encode (lumex_string_view ()).empty ());
  EXPECT_EQ (encoder::encode (text), "VGVzdCBzdHJpbmc=");
  EXPECT_EQ (encoder::encode ("Test string"), "VGVzdCBzdHJpbmc=");
}
