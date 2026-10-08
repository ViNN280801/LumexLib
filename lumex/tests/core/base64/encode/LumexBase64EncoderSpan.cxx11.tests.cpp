// Base64 encoder tests of the span overload (every standard): the overload
// takes lumex::core::span::view::span<byte_type const>, so a C++11 or C++14
// program has the same way to pass contiguous bytes as a C++20 one has with
// std::span (LumexBase64Encoder.cxx20.tests.cpp and
// LumexBase64EncoderStdSpan.cxx20.tests.cpp). The suites of C++17 and C++20
// compile this file too, so the overload set stays unambiguous next to the
// std::string_view overload.

#include <array>
#include <cstddef>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/base64/LumexBase64"
#include "lumex/core/span/LumexSpan"

#include "lumex/tests/core/base64/LumexBase64TestFixtures.hpp"

using lumex::core::base64::encode::encoder;
using lumex::core::span::view::span;

TEST_F (Base64EncoderTest,
        GivenStdArray_WhenEncodeThroughSpan_ThenProducesCorrectOutput)
{
  std::array<byte_type, 6> const data = { { 'f', 'o', 'o', 'b', 'a', 'r' } };
  EXPECT_EQ (encoder::encode (data), "Zm9vYmFy");
  std::array<byte_type, 4> mutable_data = { { 'T', 'e', 's', 't' } };
  EXPECT_EQ (encoder::encode (mutable_data), "VGVzdA==");
}

TEST_F (Base64EncoderTest,
        GivenBuiltInArray_WhenEncodeThroughSpan_ThenProducesCorrectOutput)
{
  byte_type const data[3] = { 'f', 'o', 'o' };
  EXPECT_EQ (encoder::encode (data), "Zm9v");
  byte_type mutable_data[2] = { 'f', 'o' };
  EXPECT_EQ (encoder::encode (mutable_data), "Zm8=");
}

TEST_F (Base64EncoderTest, GivenLumexSpan_WhenEncode_ThenSameAsThePointerForm)
{
  std::vector<byte_type> const data = { 0x00, 0x01, 0x02, 0xFF, 0xFE, 0xFD };
  span<byte_type const> const view (data);
  EXPECT_EQ (encoder::encode (view),
             encoder::encode (data.data (), data.size ()));
  EXPECT_EQ (encoder::encode (view), encoder::encode (data));
  span<byte_type const, 6> const fixed (data.data (), data.size ());
  EXPECT_EQ (encoder::encode (fixed), encoder::encode (view));
  span<byte_type> const mutable_view (const_cast<byte_type *> (data.data ()),
                                      data.size ());
  EXPECT_EQ (encoder::encode (mutable_view), encoder::encode (view));
}

TEST_F (Base64EncoderTest,
        GivenSubviews_WhenEncode_ThenOnlyTheSubviewIsEncoded)
{
  std::array<byte_type, 4> const data = { { 'x', 'H', 'i', 'y' } };
  span<byte_type const> const whole (data);
  EXPECT_EQ (encoder::encode (whole.subspan (1, 2)), "SGk=");
  EXPECT_EQ (encoder::encode (whole.first (1)), "eA==");
  EXPECT_EQ (encoder::encode (whole.last (1)), "eQ==");
  EXPECT_EQ (encoder::encode (whole.first<2> ()), "eEg=");
  EXPECT_EQ (encoder::encode (whole.subspan<1, 2> ()), "SGk=");
}

TEST_F (Base64EncoderTest, GivenEmptySpan_WhenEncode_ThenEmptyString)
{
  EXPECT_TRUE (encoder::encode (span<byte_type const> ()).empty ());
  EXPECT_TRUE (encoder::encode (span<byte_type const, 0> ()).empty ());
  std::vector<byte_type> const data = { 1, 2, 3 };
  EXPECT_TRUE (
      encoder::encode (span<byte_type const> (data).first (0)).empty ());
  EXPECT_TRUE (encoder::encode (std::array<byte_type, 0> ()).empty ());
}

TEST_F (Base64EncoderTest, GivenRfc4648Vectors_WhenEncodeThroughSpan_ThenMatch)
{
  struct vector_t
  {
    char const *plain;
    char const *encoded;
  };
  vector_t const vectors[] = { { "", "" },
                               { "f", "Zg==" },
                               { "fo", "Zm8=" },
                               { "foo", "Zm9v" },
                               { "foob", "Zm9vYg==" },
                               { "fooba", "Zm9vYmE=" },
                               { "foobar", "Zm9vYmFy" } };
  for (vector_t const &item : vectors)
    {
      std::string const text (item.plain);
      std::vector<byte_type> const bytes (text.begin (), text.end ());
      EXPECT_EQ (encoder::encode (span<byte_type const> (bytes)), item.encoded)
          << "input \"" << item.plain << "\"";
    }
}

TEST_F (Base64EncoderTest,
        GivenLargeSpan_WhenEncode_ThenSameAsTheVectorOverload)
{
  std::vector<byte_type> const data (10000, 0xAA);
  EXPECT_EQ (encoder::encode (span<byte_type const> (data)),
             encoder::encode (data));
  EXPECT_EQ (encoder::encode (span<byte_type const> (data)).size (),
             ((data.size () + 2) / 3) * 4);
}

TEST_F (Base64EncoderTest,
        GivenOverloads_WhenCallWithVectorAndPointer_ThenNoAmbiguity)
{
  std::vector<byte_type> const data = { 'a', 'b', 'c' };
  // The vector overload and the pointer and size overload stay the better
  // match than the span overload.
  EXPECT_EQ (encoder::encode (data), "YWJj");
  EXPECT_EQ (encoder::encode (data.data (), data.size ()), "YWJj");
  EXPECT_EQ (encoder::encode (static_cast<void const *> (nullptr), 0), "");
  static_assert (std::is_convertible<std::array<byte_type, 3> &,
                                     span<byte_type const>>::value,
                 "a std::array is accepted through the span");
  static_assert (!std::is_convertible<std::array<char, 3> &,
                                      span<byte_type const>>::value,
                 "an array of char is not an array of bytes");
}
