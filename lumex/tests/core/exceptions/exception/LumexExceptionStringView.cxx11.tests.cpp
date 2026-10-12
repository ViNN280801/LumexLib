//
// lumex_base_exception tests of the string view constructor (every standard):
// it takes the lumex_string_view of lumex::string_view in every standard
// (never std::string_view; a std::string_view converts to it, see the C++17
// file), as an inline wrapper over the exported std::string one. The C++17 and
// C++20 suites compile this file too, so the constructor set stays unambiguous
// next to the char const * and std::string constructors
// (LumexException.cxx17.tests.cpp holds the tests that name std::string_view).

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#if __cplusplus >= 201703L
#include <string_view>
#endif

#include <gtest/gtest.h>

#include "lumex/core/exceptions/LumexException"
#include "lumex/core/string_view/view/LumexWStringView.hpp"

#include "lumex/tests/core/exceptions/exception/LumexExceptionTestFixtures.hpp"

using lumex::core::exceptions::exception::lumex_base_exception;

namespace
{
// The type the string view constructor takes.
using message_view_t = lumex::core::string_view::view::lumex_string_view;

// Converts to lumex_string_view and to nothing else (in particular not to
// std::string_view). A constructor that took std::string_view would not accept
// it at C++17 and later (that would need two user-defined conversions), so
// the probe tells the parameter type apart from the std one in every
// standard.
struct only_lumex_view_t
{
  operator lumex_string_view () const; // NOLINT(google-explicit-constructor)
};
// The mirror image: converts to the standard view only, where there is one.
// The constructor does not take it (two user-defined conversions).
#if __cplusplus >= 201703L
struct only_std_view_t
{
  operator std::string_view () const; // NOLINT(google-explicit-constructor)
};
#endif
} // namespace

static_assert (
    std::is_constructible<lumex_base_exception, message_view_t>::value,
    "a message view is accepted");
static_assert (
    std::is_constructible<lumex_base_exception, char const *>::value,
    "a char const * is accepted");
static_assert (
    std::is_constructible<lumex_base_exception, char const (&)[6]>::value,
    "a literal is accepted");
static_assert (
    std::is_constructible<lumex_base_exception, std::string const &>::value,
    "a std::string is accepted");
static_assert (std::is_constructible<lumex_base_exception, std::string>::value,
               "a temporary std::string is accepted");
static_assert (!std::is_constructible<lumex_base_exception, int>::value,
               "a number is not a message");
static_assert (
    std::is_constructible<lumex_base_exception, lumex_string_view>::value,
    "the view of the library is accepted in every standard");
static_assert (std::is_constructible<lumex_base_exception,
                                     lumex_string_view const &>::value,
               "a const lvalue view is accepted");
static_assert (
    std::is_constructible<lumex_base_exception, only_lumex_view_t>::value,
    "the parameter is lumex_string_view in every standard");
static_assert (std::is_constructible<lumex_base_exception, char *>::value,
               "a char * is accepted");
static_assert (
    std::is_constructible<lumex_base_exception, std::nullptr_t>::value,
    "nullptr is accepted by the existing constructors");
static_assert (
    !std::is_constructible<lumex_base_exception, wchar_t const *>::value,
    "a wide text is not a message");
static_assert (
    !std::is_constructible<lumex_base_exception, lumex_wstring_view>::value,
    "a wide view is not a message");
static_assert (
    !std::is_constructible<lumex_base_exception, std::wstring>::value,
    "a wide string is not a message");
#if __cplusplus >= 201703L
static_assert (
    !std::is_constructible<lumex_base_exception, only_std_view_t>::value,
    "the parameter is not std::string_view");
#endif

TEST_F (LumexExceptionTest, LumexBaseException_MessageViewCtor_CopiesTheView)
{
  std::string const text = "prefix:message:suffix";
  message_view_t const view (text.data () + 7, 7);
  lumex_base_exception const ex (view);
  EXPECT_STREQ (ex.what (), "message");
  EXPECT_EQ (std::string (ex.what ()).size (), 7U);
}

TEST_F (LumexExceptionTest,
        LumexBaseException_MessageViewCtor_EmptyAndEmbeddedNulKept)
{
  message_view_t const empty_view;
  lumex_base_exception const empty (empty_view);
  EXPECT_STREQ (empty.what (), "");

  message_view_t const none ("message", 0);
  lumex_base_exception const none_ex (none);
  EXPECT_STREQ (none_ex.what (), "");

  message_view_t const with_nul ("a\0b", 3);
  lumex_base_exception const ex (with_nul);
  EXPECT_EQ (std::string (ex.what (), 3), std::string ("a\0b", 3));
  EXPECT_STREQ (ex.what (), "a");
}

TEST_F (LumexExceptionTest,
        LumexBaseException_EveryConstructor_GivesTheSameMessage)
{
  std::string const text = "Failed to open file";
  message_view_t const view (text.data (), text.size ());
  char const *const literal = "Failed to open file";
  lumex_base_exception const from_view (view);
  lumex_base_exception const from_literal ("Failed to open file");
  lumex_base_exception const from_pointer (literal);
  lumex_base_exception const from_string (text);
  lumex_base_exception const from_temporary (
      std::string ("Failed to open file"));
  std::string movable = text;
  lumex_base_exception const from_moved (std::move (movable));
  EXPECT_STREQ (from_view.what (), text.c_str ());
  EXPECT_STREQ (from_literal.what (), text.c_str ());
  EXPECT_STREQ (from_pointer.what (), text.c_str ());
  EXPECT_STREQ (from_string.what (), text.c_str ());
  EXPECT_STREQ (from_temporary.what (), text.c_str ());
  EXPECT_STREQ (from_moved.what (), text.c_str ());
}

TEST_F (LumexExceptionTest,
        LumexBaseException_MessageViewCtor_ThrowsAndCatches)
{
  std::string const text = "xx boom yy";
  try
    {
      throw lumex_base_exception (message_view_t (text.data () + 3, 4));
    }
  catch (std::exception const &caught)
    {
      EXPECT_STREQ (caught.what (), "boom");
      return;
    }
  FAIL () << "the exception was not thrown";
}
