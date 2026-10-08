// Base64 validator tests of the string overload (every standard): it takes
// string_type_t, which is std::string_view from C++17 and the
// lumex_string_view of lumex::string_view below it, so a C++11 program passes
// a literal, a char const * and a std::string the way a C++17 one does. The
// C++17 and C++20 suites compile this file too
// (LumexBase64Validator.cxx17.tests.cpp holds the tests that name
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
using lumex::core::base64::validate::validator;

TEST_F (Base64ValidatorTest, GivenStringLiteral_WhenValidate_ThenSyntaxDecides)
{
  EXPECT_TRUE (validator::is_valid_base64 (""));
  EXPECT_TRUE (validator::is_valid_base64 ("Zg=="));
  EXPECT_TRUE (validator::is_valid_base64 ("Zm9vYmFy"));
  EXPECT_TRUE (validator::is_valid_base64 ("SGk"));
  EXPECT_FALSE (validator::is_valid_base64 ("S"));
  EXPECT_FALSE (validator::is_valid_base64 ("SGk=="));
  EXPECT_FALSE (validator::is_valid_base64 ("S=k="));
  EXPECT_FALSE (validator::is_valid_base64 ("SG k"));
}

TEST_F (Base64ValidatorTest,
        GivenCharPointerAndStdString_WhenValidate_ThenSameAsThePointerForm)
{
  for (std::string const &text : valid_inputs)
    {
      EXPECT_TRUE (validator::is_valid_base64 (text.data (), text.size ()));
      EXPECT_TRUE (validator::is_valid_base64 (text)) << text;
      EXPECT_TRUE (validator::is_valid_base64 (text.c_str ())) << text;
    }
  for (std::vector<std::string> const *group :
       { &invalid_length, &invalid_characters, &invalid_padding })
    for (std::string const &text : *group)
      {
        bool const by_pointer
            = validator::is_valid_base64 (text.data (), text.size ());
        EXPECT_EQ (validator::is_valid_base64 (text), by_pointer) << text;
        EXPECT_EQ (validator::is_valid_base64 (text.c_str ()), by_pointer)
            << text;
      }
  EXPECT_TRUE (validator::is_valid_base64 (std::string ("SGk=")));
  std::string mutable_text = "SGk=";
  EXPECT_TRUE (validator::is_valid_base64 (mutable_text));
}

TEST_F (Base64ValidatorTest,
        GivenStdStringWithEmbeddedNul_WhenValidate_ThenInvalid)
{
  // The std::string is sized: a NUL inside it is an invalid character, not
  // the end of the input.
  EXPECT_FALSE (validator::is_valid_base64 (std::string ("Zm9v\0YmFy", 9)));
  EXPECT_FALSE (validator::is_valid_base64 (std::string (1, '\0')));
  // A char const * is NUL-terminated: it ends at the first zero.
  EXPECT_TRUE (validator::is_valid_base64 ("Zm9v\0YmFy"));
}

TEST_F (Base64ValidatorTest,
        GivenStringView_WhenValidate_ThenOnlyTheViewIsRead)
{
  std::string const longer = "xxSGk=yy";
  EXPECT_FALSE (validator::is_valid_base64 (longer));
  EXPECT_TRUE (
      validator::is_valid_base64 (string_type_t (longer.data () + 2, 4)));
  EXPECT_TRUE (
      validator::is_valid_base64 (string_type_t (longer.data () + 2, 3)));
  EXPECT_FALSE (
      validator::is_valid_base64 (string_type_t (longer.data () + 2, 5)));
  EXPECT_FALSE (
      validator::is_valid_base64 (string_type_t (longer.data () + 2, 1)));
}

TEST_F (Base64ValidatorTest, GivenEmptyInputs_WhenValidate_ThenTrue)
{
  EXPECT_TRUE (validator::is_valid_base64 (std::string ()));
  EXPECT_TRUE (validator::is_valid_base64 (string_type_t ("Zm9v", 0)));
  // A default-constructed view has no data pointer but is an empty input,
  // unlike the pointer and size core called with nullptr.
  string_type_t const empty_view;
  ASSERT_EQ (empty_view.data (), nullptr);
  EXPECT_TRUE (validator::is_valid_base64 (empty_view));
  EXPECT_FALSE (validator::is_valid_base64 (nullptr, 0));
}

TEST_F (Base64ValidatorTest,
        GivenOverloads_WhenCallWithPointerAndSizeOrString_ThenNoAmbiguity)
{
  EXPECT_TRUE (validator::is_valid_base64 ("SGk=", 4));
  EXPECT_TRUE (validator::is_valid_base64 ("SGk=", std::size_t (4)));
  // A size shorter than the text cuts the input.
  EXPECT_TRUE (validator::is_valid_base64 ("SGk=", 3));
  EXPECT_FALSE (validator::is_valid_base64 ("SGk=", 1));
  EXPECT_FALSE (
      validator::is_valid_base64 (static_cast<char const *> (nullptr), 0));
  static_assert (std::is_convertible<char const (&)[5], string_type_t>::value,
                 "a literal is accepted as text");
  static_assert (
      std::is_convertible<std::string const &, string_type_t>::value,
      "a std::string is accepted as text");
}

#if __cplusplus < 201703L
TEST_F (Base64ValidatorTest, GivenNullCharPointer_WhenValidate_ThenEmptyInput)
{
  // The view of the library treats a null char const * as an empty text
  // (std::string_view does not allow it, so this is a test of the library
  // view only).
  EXPECT_TRUE (
      validator::is_valid_base64 (static_cast<char const *> (nullptr)));
}
#endif
