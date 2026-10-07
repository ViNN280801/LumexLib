// Base64 tests of the std::span wrappers (C++20). The C++20 suite compiles
// this file together with LumexBase64.cxx11.tests.cpp and
// LumexBase64.cxx17.tests.cpp. Encoder.hpp declares the std::span overload
// only when the standard library has std::span (LUMEX_HAS_STD_SPAN):
// libstdc++ 8 has no <span> even with -std=c++2a, so there the tests skip.

#include <string>
#include <vector>
#if defined(__has_include)
#if __has_include(<span>)
#include <span>
#endif
#endif

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

// ==========================================================================
// encoder
// ==========================================================================

TEST_F (Base64EncoderTest, GivenSpan_WhenEncode_ThenProducesCorrectOutput)
{
#if LUMEX_HAS_STD_SPAN
  std::vector<byte_type> data = { 'T', 'e', 's', 't' };
  std::span<byte_type const> span_data (data);
  std::string result = encoder::encode (span_data);
  EXPECT_EQ (result, "VGVzdA==");
#else
  GTEST_SKIP () << "the standard library has no std::span";
#endif
}

TEST_F (Base64EncoderTest, GivenSubSpan_WhenEncode_ThenOnlyTheSpanIsEncoded)
{
#if LUMEX_HAS_STD_SPAN
  std::vector<byte_type> const data = { 'x', 'H', 'i', 'y' };
  std::span<byte_type const> const whole (data);
  EXPECT_EQ (encoder::encode (whole.subspan (1, 2)), "SGk=");
  EXPECT_TRUE (encoder::encode (std::span<byte_type const> ()).empty ());
#else
  GTEST_SKIP () << "the standard library has no std::span";
#endif
}
