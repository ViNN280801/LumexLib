// Base64 decoder tests of the string overloads (every standard): they take
// string_type_t, which is std::string_view from C++17 and the
// lumex_string_view of lumex::string_view below it, so a C++11 program passes
// a literal, a char const * and a std::string the way a C++17 one does. The
// C++17 and C++20 suites compile this file too
// (LumexBase64Decoder.cxx17.tests.cpp holds the tests that name
// std::string_view).

#include <cstddef>
#include <string>
#include <type_traits>
#include <vector>
#if __cplusplus >= 201703L
#include <string_view>
#endif

#include <gtest/gtest.h>

#include "lumex/core/base64/LumexBase64"

#include "lumex/tests/core/base64/LumexBase64TestFixtures.hpp"

using lumex::core::base64::codec::Types::string_type_t;
using lumex::core::base64::decode::decoder;
using lumex::core::base64::encode::encoder;

namespace
{
std::string
as_text (std::vector<unsigned char> const &bytes)
{
  return std::string (bytes.begin (), bytes.end ());
}
} // namespace

TEST_F (Base64DecoderTest, GivenStringLiteral_WhenDecode_ThenRfc4648Vector)
{
  EXPECT_EQ (as_text (decoder::decode ("")), "");
  EXPECT_EQ (as_text (decoder::decode ("Zg==")), "f");
  EXPECT_EQ (as_text (decoder::decode ("Zm8=")), "fo");
  EXPECT_EQ (as_text (decoder::decode ("Zm9v")), "foo");
  EXPECT_EQ (as_text (decoder::decode ("Zm9vYg==")), "foob");
  EXPECT_EQ (as_text (decoder::decode ("Zm9vYmE=")), "fooba");
  EXPECT_EQ (as_text (decoder::decode ("Zm9vYmFy")), "foobar");
  std::vector<byte_type> out (4, 0x7F);
  EXPECT_TRUE (decoder::decode ("Zm9vYmFy", out));
  EXPECT_EQ (as_text (out), "foobar");
}

TEST_F (Base64DecoderTest,
        GivenCharPointer_WhenDecode_ThenSameAsThePointerForm)
{
  for (auto const &item : rfc_vectors)
    {
      char const *const text = item.first.c_str ();
      std::vector<byte_type> expected;
      ASSERT_TRUE (decoder::decode (text, item.first.size (), expected));
      std::vector<byte_type> out (2, 0x55);
      EXPECT_TRUE (decoder::decode (text, out)) << item.first;
      EXPECT_EQ (out, expected) << item.first;
      EXPECT_EQ (decoder::decode (text), expected) << item.first;
      EXPECT_EQ (as_text (expected), item.second) << item.first;
    }
}

TEST_F (Base64DecoderTest, GivenStdString_WhenDecode_ThenSameAsThePointerForm)
{
  for (auto const &item : rfc_vectors)
    {
      std::vector<byte_type> expected;
      ASSERT_TRUE (
          decoder::decode (item.first.data (), item.first.size (), expected));
      std::vector<byte_type> out (2, 0x55);
      EXPECT_TRUE (decoder::decode (item.first, out)) << item.first;
      EXPECT_EQ (out, expected) << item.first;
      EXPECT_EQ (decoder::decode (item.first), expected) << item.first;
    }
  // An rvalue string and a non-const string take the same overload.
  EXPECT_EQ (as_text (decoder::decode (std::string ("SGk="))), "Hi");
  std::string mutable_text = "SGk=";
  EXPECT_EQ (as_text (decoder::decode (mutable_text)), "Hi");
}

TEST_F (Base64DecoderTest,
        GivenStdStringWithEmbeddedNul_WhenDecode_ThenInvalidAndOutCleared)
{
  // The std::string is sized: a NUL inside it is an invalid character, not
  // the end of the input.
  std::string const text ("Zm9v\0YmFy", 9);
  std::vector<byte_type> out (3, 0x7F);
  EXPECT_FALSE (decoder::decode (text, out));
  EXPECT_TRUE (out.empty ());
  EXPECT_TRUE (decoder::decode (text).empty ());
  // A char const * is NUL-terminated: it ends at the first zero.
  EXPECT_EQ (as_text (decoder::decode ("Zm9v\0YmFy")), "foo");
  std::vector<byte_type> again;
  EXPECT_TRUE (decoder::decode ("Zm9v\0YmFy", again));
  EXPECT_EQ (as_text (again), "foo");
}

TEST_F (Base64DecoderTest, GivenStringView_WhenDecode_ThenOnlyTheViewIsRead)
{
  std::string const longer = "xxSGk=yy";
  string_type_t const view (longer.data () + 2, 4);
  EXPECT_EQ (as_text (decoder::decode (view)), "Hi");
  std::vector<byte_type> out;
  EXPECT_TRUE (decoder::decode (view, out));
  EXPECT_EQ (as_text (out), "Hi");
  // Without the padding the view is still valid unpadded Base64.
  EXPECT_EQ (as_text (decoder::decode (string_type_t (longer.data () + 2, 3))),
             "Hi");
  // A view that cuts a group is invalid.
  EXPECT_TRUE (
      decoder::decode (string_type_t (longer.data () + 2, 1)).empty ());
  EXPECT_FALSE (decoder::decode (string_type_t (longer.data () + 2, 1), out));
  EXPECT_TRUE (out.empty ());
}

TEST_F (Base64DecoderTest, GivenEmptyInputs_WhenDecode_ThenTrueAndEmpty)
{
  std::vector<byte_type> out (3, 0x7F);
  EXPECT_TRUE (decoder::decode ("", out));
  EXPECT_TRUE (out.empty ());
  out.assign (3, 0x7F);
  EXPECT_TRUE (decoder::decode (std::string (), out));
  EXPECT_TRUE (out.empty ());
  // A default-constructed view has no data pointer but is an empty input,
  // unlike the pointer and size core called with nullptr.
  string_type_t const empty_view;
  ASSERT_EQ (empty_view.data (), nullptr);
  out.assign (3, 0x7F);
  EXPECT_TRUE (decoder::decode (empty_view, out));
  EXPECT_TRUE (out.empty ());
  EXPECT_TRUE (decoder::decode (empty_view).empty ());
  out.assign (3, 0x7F);
  EXPECT_TRUE (decoder::decode (string_type_t ("Zm9v", 0), out));
  EXPECT_TRUE (out.empty ());
}

TEST_F (Base64DecoderTest, GivenInvalidInputs_WhenDecodeString_ThenFalse)
{
  std::vector<std::string> const invalid_texts
      = { "Q",    "Q===", "QQ=Q", "Q=QQ",  "=QQQ",
          "====", "QQ@Q", "QQ Q", "QQ\nQ", "QQQ=Q" };
  for (std::string const &text : invalid_texts)
    {
      std::vector<byte_type> out (3, 0x7F);
      EXPECT_FALSE (decoder::decode (text, out)) << text;
      EXPECT_TRUE (out.empty ()) << text;
      EXPECT_TRUE (decoder::decode (text).empty ()) << text;
      EXPECT_FALSE (decoder::decode (text.c_str (), out)) << text;
    }
}

TEST_F (Base64DecoderTest, GivenEveryLength_WhenRoundTrip_ThenTextRestored)
{
  for (std::size_t length = 0; length < 70; ++length)
    {
      std::string text;
      for (std::size_t i = 0; i < length; ++i)
        text.push_back (static_cast<char> ((i * 37 + 11) & 0xFF));
      std::string const encoded = encoder::encode (text);
      EXPECT_EQ (as_text (decoder::decode (encoded)), text)
          << "length " << length;
      EXPECT_EQ (as_text (decoder::decode (encoded.c_str ())), text)
          << "length " << length;
    }
}

TEST_F (Base64DecoderTest,
        GivenOverloads_WhenCallWithPointerAndSizeOrString_ThenNoAmbiguity)
{
  std::vector<byte_type> out;
  // Two arguments: a string and the output vector, or the pointer and size.
  EXPECT_TRUE (decoder::decode ("SGk=", out));
  EXPECT_EQ (as_text (out), "Hi");
  EXPECT_EQ (as_text (decoder::decode ("SGk=", 4)), "Hi");
  EXPECT_EQ (as_text (decoder::decode ("SGk=", std::size_t (4))), "Hi");
  EXPECT_TRUE (decoder::decode ("SGk=", 4, out));
  EXPECT_EQ (as_text (out), "Hi");
  // A size shorter than the text cuts the input: "SGk" is unpadded Base64.
  EXPECT_EQ (as_text (decoder::decode ("SGk=", 3)), "Hi");
  EXPECT_FALSE (decoder::decode (static_cast<char const *> (nullptr), 0, out));
  static_assert (std::is_convertible<char const (&)[5], string_type_t>::value,
                 "a literal is accepted as text");
  static_assert (
      std::is_convertible<std::string const &, string_type_t>::value,
      "a std::string is accepted as text");
}

#if __cplusplus < 201703L
TEST_F (Base64DecoderTest, GivenNullCharPointer_WhenDecode_ThenEmptyInput)
{
  // The view of the library treats a null char const * as an empty text
  // (std::string_view does not allow it, so this is a test of the library
  // view only).
  std::vector<byte_type> out (3, 0x7F);
  EXPECT_TRUE (decoder::decode (static_cast<char const *> (nullptr), out));
  EXPECT_TRUE (out.empty ());
  EXPECT_TRUE (decoder::decode (static_cast<char const *> (nullptr)).empty ());
}
#endif
