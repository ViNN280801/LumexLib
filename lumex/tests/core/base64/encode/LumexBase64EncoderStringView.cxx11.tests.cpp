// Base64 encoder tests of the string overload (every standard): it takes
// string_type_t, which is std::string_view from C++17 and the
// lumex_string_view of lumex::string_view below it, so a C++11 program passes
// a literal, a char const * and a std::string the way a C++17 one does. The
// C++17 and C++20 suites compile this file too, so the overload set stays
// unambiguous next to the vector, pointer and span overloads
// (LumexBase64Encoder.cxx17.tests.cpp holds the tests that name
// std::string_view).

#include <array>
#include <cstddef>
#include <string>
#include <type_traits>
#include <vector>
#if __cplusplus >= 201703L
#include <string_view>
#endif

#include <gtest/gtest.h>

#include "lumex/core/base64/LumexBase64"
#include "lumex/core/span/LumexSpan"

#include "lumex/tests/core/base64/LumexBase64TestFixtures.hpp"

using lumex::core::base64::codec::Types::string_type_t;
using lumex::core::base64::encode::encoder;
using lumex::core::span::view::span;

namespace
{
struct rfc_vector_t
{
  char const *plain;
  char const *encoded;
};

rfc_vector_t const kRfc4648Vectors[] = { { "", "" },
                                         { "f", "Zg==" },
                                         { "fo", "Zm8=" },
                                         { "foo", "Zm9v" },
                                         { "foob", "Zm9vYg==" },
                                         { "fooba", "Zm9vYmE=" },
                                         { "foobar", "Zm9vYmFy" } };
} // namespace

#if __cplusplus >= 201703L
static_assert (std::is_same<string_type_t, std::string_view>::value,
               "from C++17 the string overloads take std::string_view");
#else
static_assert (
    std::is_same<string_type_t,
                 lumex::core::string_view::view::lumex_string_view>::value,
    "below C++17 the string overloads take the lumex_string_view");
#endif

TEST_F (Base64EncoderTest, GivenStringLiteral_WhenEncode_ThenRfc4648Vector)
{
  EXPECT_EQ (encoder::encode (""), "");
  EXPECT_EQ (encoder::encode ("f"), "Zg==");
  EXPECT_EQ (encoder::encode ("fo"), "Zm8=");
  EXPECT_EQ (encoder::encode ("foo"), "Zm9v");
  EXPECT_EQ (encoder::encode ("foob"), "Zm9vYg==");
  EXPECT_EQ (encoder::encode ("fooba"), "Zm9vYmE=");
  EXPECT_EQ (encoder::encode ("foobar"), "Zm9vYmFy");
}

TEST_F (Base64EncoderTest,
        GivenCharPointer_WhenEncode_ThenSameAsThePointerForm)
{
  for (rfc_vector_t const &item : kRfc4648Vectors)
    {
      char const *const text = item.plain;
      std::string const length_form
          = encoder::encode (text, std::string (text).size ());
      EXPECT_EQ (encoder::encode (text), length_form)
          << "input \"" << item.plain << "\"";
      EXPECT_EQ (encoder::encode (text), item.encoded)
          << "input \"" << item.plain << "\"";
    }
}

TEST_F (Base64EncoderTest, GivenStdString_WhenEncode_ThenSameAsThePointerForm)
{
  for (rfc_vector_t const &item : kRfc4648Vectors)
    {
      std::string const text (item.plain);
      EXPECT_EQ (encoder::encode (text), item.encoded)
          << "input \"" << item.plain << "\"";
      EXPECT_EQ (encoder::encode (text),
                 encoder::encode (text.data (), text.size ()));
    }
  // An rvalue string and a non-const string take the same overload.
  EXPECT_EQ (encoder::encode (std::string ("Test string")),
             "VGVzdCBzdHJpbmc=");
  std::string mutable_text = "Hi";
  EXPECT_EQ (encoder::encode (mutable_text), "SGk=");
}

TEST_F (Base64EncoderTest,
        GivenStdStringWithEmbeddedZeros_WhenEncode_ThenEncodesTheWholeString)
{
  // The std::string is sized: its NUL characters are bytes, not the end.
  std::string const text ("a\0b", 3);
  EXPECT_EQ (encoder::encode (text), "YQBi");
  EXPECT_EQ (encoder::encode (text), encoder::encode (text.data (), 3));
  std::string const zeros (5, '\0');
  EXPECT_EQ (encoder::encode (zeros), "AAAAAAA=");
  // A char const * is NUL-terminated: it ends at the first zero.
  EXPECT_EQ (encoder::encode ("a\0b"), "YQ==");
}

TEST_F (Base64EncoderTest, GivenStringView_WhenEncode_ThenSameAsThePointerForm)
{
  std::string const longer = "xxHiyy";
  string_type_t const view (longer.data () + 2, 2);
  EXPECT_EQ (encoder::encode (view), "SGk=");
  EXPECT_EQ (encoder::encode (view), encoder::encode (view.data (), 2));
  string_type_t const zeros ("a\0b\0", 4);
  EXPECT_EQ (encoder::encode (zeros), "YQBiAA==");
  // The sized view is what gets encoded, not the NUL-terminated text behind.
  string_type_t const part ("foobar", 3);
  EXPECT_EQ (encoder::encode (part), "Zm9v");
}

TEST_F (Base64EncoderTest, GivenEmptyInputs_WhenEncode_ThenEmptyString)
{
  EXPECT_TRUE (encoder::encode ("").empty ());
  EXPECT_TRUE (encoder::encode (std::string ()).empty ());
  EXPECT_TRUE (encoder::encode (string_type_t ()).empty ());
  EXPECT_TRUE (encoder::encode (string_type_t ("abc", 0)).empty ());
  EXPECT_TRUE (
      encoder::encode (static_cast<void const *> (nullptr), 0).empty ());
}

TEST_F (Base64EncoderTest,
        GivenEveryByteValue_WhenEncodeString_ThenSameAsBytes)
{
  std::string text;
  for (std::size_t i = 0; i < 256; ++i)
    text.push_back (static_cast<char> (i));
  EXPECT_EQ (encoder::encode (text), encoder::encode (all_bytes));
  EXPECT_EQ (encoder::encode (string_type_t (text.data (), text.size ())),
             encoder::encode (all_bytes));
}

TEST_F (Base64EncoderTest, GivenEveryLength_WhenEncodeString_ThenSameAsBytes)
{
  // Every size around the three-byte groups, content from the whole alphabet.
  for (std::size_t length = 0; length < 70; ++length)
    {
      std::string text;
      for (std::size_t i = 0; i < length; ++i)
        text.push_back (static_cast<char> ((i * 37 + 11) & 0xFF));
      std::vector<byte_type> const bytes (text.begin (), text.end ());
      EXPECT_EQ (encoder::encode (text), encoder::encode (bytes))
          << "length " << length;
      EXPECT_EQ (encoder::encode (text).size (), ((length + 2) / 3) * 4);
    }
}

TEST_F (
    Base64EncoderTest,
    GivenOverloads_WhenCallWithLiteralStringVectorSpanPointer_ThenNoAmbiguity)
{
  // Each argument kind picks its overload; none of the calls is ambiguous.
  char const array[] = "abc";
  char mutable_array[] = "abc";
  std::string const text = "abc";
  std::vector<byte_type> const bytes = { 'a', 'b', 'c' };
  std::array<byte_type, 3> const fixed = { { 'a', 'b', 'c' } };
  byte_type const raw[3] = { 'a', 'b', 'c' };
  EXPECT_EQ (encoder::encode ("abc"), "YWJj");
  EXPECT_EQ (encoder::encode (array), "YWJj");
  EXPECT_EQ (encoder::encode (mutable_array), "YWJj");
  EXPECT_EQ (encoder::encode (static_cast<char const *> (array)), "YWJj");
  EXPECT_EQ (encoder::encode (text), "YWJj");
  EXPECT_EQ (encoder::encode (bytes), "YWJj");
  EXPECT_EQ (encoder::encode (fixed), "YWJj");
  EXPECT_EQ (encoder::encode (raw), "YWJj");
  EXPECT_EQ (encoder::encode (span<byte_type const> (bytes)), "YWJj");
  EXPECT_EQ (encoder::encode (bytes.data (), bytes.size ()), "YWJj");
  EXPECT_EQ (encoder::encode (static_cast<void const *> (nullptr), 0), "");
  static_assert (
      !std::is_convertible<char const (&)[4], span<byte_type const>>::value,
      "a literal is text, not an array of bytes");
  static_assert (
      !std::is_convertible<std::string &, span<byte_type const>>::value,
      "a std::string is text, not a range of bytes");
  static_assert (std::is_convertible<char const (&)[4], string_type_t>::value,
                 "a literal is accepted as text");
  static_assert (
      std::is_convertible<std::string const &, string_type_t>::value,
      "a std::string is accepted as text");
  static_assert (std::is_convertible<char const *, string_type_t>::value,
                 "a char const * is accepted as text");
  static_assert (!std::is_convertible<std::vector<byte_type> const &,
                                      string_type_t>::value,
                 "bytes are not text");
}

#if __cplusplus < 201703L
TEST_F (Base64EncoderTest, GivenNullCharPointer_WhenEncode_ThenEmptyString)
{
  // The view of the library treats a null char const * as an empty text
  // (std::string_view does not allow it, so this is a test of the library
  // view only).
  EXPECT_TRUE (encoder::encode (static_cast<char const *> (nullptr)).empty ());
}

TEST_F (Base64EncoderTest, GivenLumexStringViewApi_WhenEncode_ThenSubviewsWork)
{
  lumex_string_view view ("xxHiyy");
  view.remove_prefix (2);
  view.remove_suffix (2);
  EXPECT_EQ (encoder::encode (view), "SGk=");
  EXPECT_EQ (encoder::encode (view.substr (1)), "aQ==");
  EXPECT_EQ (encoder::encode (lumex_string_view ()), "");
}
#endif
