// Base64 decoder tests of the std::string_view wrappers (C++17). The C++17
// and C++20 suites of this directory compile this file together with
// LumexBase64Decoder.cxx11.tests.cpp.

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

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

// A std::string_view argument converts to the lumex_string_view the overloads
// take; the conversion is implicit, so the calls are unambiguous and return
// the same types as every other form.
static_assert (std::is_convertible<std::string_view, string_type_t>::value,
               "std::string_view converts to the text type");
static_assert (std::is_same<decltype (decoder::decode (
                                std::declval<std::string_view> ())),
                            std::vector<unsigned char>>::value,
               "decode (std::string_view)");
static_assert (std::is_same<decltype (decoder::decode (
                                std::declval<std::string_view const &> ())),
                            std::vector<unsigned char>>::value,
               "decode (std::string_view const &)");
static_assert (
    std::is_same<decltype (decoder::decode (
                     std::declval<std::string_view> (),
                     std::declval<std::vector<unsigned char> &> ())),
                 bool>::value,
    "decode (std::string_view, out)");

TEST_F (Base64DecoderTest,
        GivenDefaultConstructedStringView_WhenDecode_ThenTrueAndEmpty)
{
  // A default-constructed view has no data pointer but is an empty input,
  // unlike the pointer and size core called with nullptr.
  std::string_view const empty_view;
  ASSERT_EQ (empty_view.data (), nullptr);
  std::vector<byte_type> out (3, 0x7F);
  EXPECT_TRUE (decoder::decode (empty_view, out));
  EXPECT_TRUE (out.empty ());
  EXPECT_TRUE (decoder::decode (empty_view).empty ());
}

TEST_F (Base64DecoderTest,
        GivenStringViewSubRange_WhenDecode_ThenOnlyTheViewIsRead)
{
  std::string const longer = "xxSGk=yy";
  std::string_view const view = std::string_view (longer).substr (2, 4);
  std::vector<byte_type> const bytes = decoder::decode (view);
  EXPECT_EQ (std::string (bytes.begin (), bytes.end ()), "Hi");
  // Without the padding the view is still valid unpadded Base64.
  std::vector<byte_type> const unpadded = decoder::decode (view.substr (0, 3));
  EXPECT_EQ (std::string (unpadded.begin (), unpadded.end ()), "Hi");
}

TEST_F (Base64DecoderTest,
        GivenLumexStringView_WhenDecode_ThenSameAsStdStringView)
{
  // The view of the library converts to std::string_view.
  std::string const longer = "xxSGk=yy";
  lumex_string_view const view = lumex_string_view (longer).substr (2, 4);
  std::vector<byte_type> const bytes = decoder::decode (view);
  EXPECT_EQ (std::string (bytes.begin (), bytes.end ()), "Hi");
  std::vector<byte_type> out (3, 0x7F);
  EXPECT_TRUE (decoder::decode (view, out));
  EXPECT_EQ (out, bytes);
  EXPECT_FALSE (decoder::decode (lumex_string_view (longer), out));
  EXPECT_TRUE (out.empty ());
  std::string const with_nul ("Zm9v\0YmFy", 9);
  EXPECT_FALSE (decoder::decode (lumex_string_view (with_nul), out));
  EXPECT_TRUE (decoder::decode (lumex_string_view (), out));
  EXPECT_TRUE (out.empty ());
  EXPECT_EQ (decoder::decode ("SGk="), bytes);
  EXPECT_EQ (decoder::decode (std::string ("SGk=")), bytes);
}
