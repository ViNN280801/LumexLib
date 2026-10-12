// Base64 encoder tests of std::span arguments (C++20): a std::span converts
// to the span the encoder takes, so the calls that compiled before the
// overload took the span of this library still compile. The tests skip where
// the standard library has no std::span (libstdc++ 8 has no <span> even with
// -std=c++2a).

#include <cstddef>
#include <string>
#include <type_traits>
#include <vector>
#if defined(__has_include)
#if __has_include(<span>)
#include <span>
#endif
#endif

#include <gtest/gtest.h>

#include "lumex/core/base64/LumexBase64"
#include "lumex/core/span/LumexSpan"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"

#include "lumex/tests/core/base64/LumexBase64TestFixtures.hpp"

using lumex::core::base64::encode::encoder;

TEST_F (Base64EncoderTest,
        GivenMutableStdSpan_WhenEncode_ThenProducesCorrectOutput)
{
#if LUMEX_HAS_STD_SPAN
  std::vector<byte_type> data = { 'T', 'e', 's', 't' };
  std::span<byte_type> const view (data);
  EXPECT_EQ (encoder::encode (view), "VGVzdA==");
  EXPECT_EQ (encoder::encode (std::span<byte_type> (data).first (2)), "VGU=");
#else
  GTEST_SKIP () << "the standard library has no std::span";
#endif
}

TEST_F (Base64EncoderTest,
        GivenStaticStdSpan_WhenEncode_ThenProducesCorrectOutput)
{
#if LUMEX_HAS_STD_SPAN
  byte_type const data[3] = { 'f', 'o', 'o' };
  std::span<byte_type const, 3> const view (data);
  EXPECT_EQ (encoder::encode (view), "Zm9v");
  EXPECT_EQ (encoder::encode (view.subspan<1> ()), "b28=");
#else
  GTEST_SKIP () << "the standard library has no std::span";
#endif
}

TEST_F (Base64EncoderTest,
        GivenRvalueStdSpan_WhenEncode_ThenProducesCorrectOutput)
{
#if LUMEX_HAS_STD_SPAN
  std::vector<byte_type> const data = { 'f', 'o', 'o', 'b', 'a', 'r' };
  EXPECT_EQ (encoder::encode (std::span<byte_type const> (data)), "Zm9vYmFy");
  EXPECT_EQ (encoder::encode (std::span<byte_type const> (data).last (3)),
             "YmFy");
  EXPECT_TRUE (encoder::encode (std::span<byte_type const> ()).empty ());
#else
  GTEST_SKIP () << "the standard library has no std::span";
#endif
}

TEST_F (Base64EncoderTest,
        GivenStdSpanOfOtherElements_WhenEncode_ThenNotAccepted)
{
#if LUMEX_HAS_STD_SPAN
  using lumex::core::span::view::span;
  static_assert (std::is_convertible<std::span<byte_type const>,
                                     span<byte_type const>>::value,
                 "a std::span of bytes converts");
  static_assert (!std::is_convertible<std::span<char const>,
                                      span<byte_type const>>::value,
                 "a std::span of char does not");
  static_assert (!std::is_convertible<std::span<std::byte const>,
                                      span<byte_type const>>::value,
                 "neither does a std::span of std::byte");
  SUCCEED ();
#else
  GTEST_SKIP () << "the standard library has no std::span";
#endif
}
