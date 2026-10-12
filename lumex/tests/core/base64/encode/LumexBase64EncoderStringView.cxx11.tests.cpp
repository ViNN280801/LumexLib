// Base64 encoder tests of the string overload (every standard): it takes
// string_type_t, which is the lumex_string_view of lumex::string_view in
// every standard (never std::string_view), so a program passes a literal, a
// char const *, a std::string and a lumex_string_view the same way at C++11
// and at C++23; a std::string_view converts to it (the C++17 file). The
// C++17 and C++20 suites compile this file too, so the overload set stays
// unambiguous next to the vector, pointer and span overloads
// (LumexBase64Encoder.cxx17.tests.cpp holds the tests that name
// std::string_view).

#include <array>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/base64/LumexBase64"
#include "lumex/core/span/LumexSpan"
#include "lumex/core/string_view/view/LumexWStringView.hpp"

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

static_assert (
    std::is_same<string_type_t,
                 lumex::core::string_view::view::lumex_string_view>::value,
    "the string overload takes the lumex_string_view in every standard");

namespace
{
// Taking the address picks the one overload whose parameter is exactly
// lumex_string_view: the line does not compile when the parameter is another
// type (std::string_view, std::string const &) in any standard.
std::string (*const kEncodeText) (lumex_string_view) = &encoder::encode;

template <class T, class = void> struct accepts_text : std::false_type
{
};
template <class T>
struct accepts_text<T, decltype (void (encoder::encode (std::declval<T> ())))>
    : std::true_type
{
};
} // namespace

// Every argument form is accepted without ambiguity next to the vector,
// pointer and span overloads, in every standard.
static_assert (accepts_text<char const (&)[4]>::value, "a literal");
static_assert (accepts_text<char (&)[4]>::value, "a char array");
static_assert (accepts_text<char const *>::value, "a char const *");
static_assert (accepts_text<char *>::value, "a char *");
static_assert (accepts_text<std::string>::value, "a std::string");
static_assert (accepts_text<std::string const &>::value, "a std::string &");
static_assert (accepts_text<lumex_string_view>::value, "the view");
static_assert (accepts_text<lumex_string_view const &>::value, "the view &");
static_assert (accepts_text<std::nullptr_t>::value, "nullptr: an empty text");
static_assert (!accepts_text<int>::value, "a number is not text");
static_assert (!accepts_text<wchar_t const *>::value, "a wide text");
static_assert (!accepts_text<lumex_wstring_view>::value, "a wide view");
static_assert (!accepts_text<std::wstring>::value, "a wide string");

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

TEST_F (Base64EncoderTest, GivenTextParameter_WhenCallThroughPointer_ThenWorks)
{
  EXPECT_EQ (kEncodeText ("foobar"), "Zm9vYmFy");
  EXPECT_EQ (kEncodeText (lumex_string_view ("a\0b", 3)), "YQBi");
  EXPECT_EQ (kEncodeText (std::string ("fo")), "Zm8=");
}

TEST_F (Base64EncoderTest, GivenNullCharPointer_WhenEncode_ThenEmptyString)
{
  // The view of the library treats a null char const * as an empty text
  // (std::string_view does not allow it); the nullptr literal does the same.
  EXPECT_TRUE (encoder::encode (static_cast<char const *> (nullptr)).empty ());
  EXPECT_TRUE (encoder::encode (nullptr).empty ());
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
