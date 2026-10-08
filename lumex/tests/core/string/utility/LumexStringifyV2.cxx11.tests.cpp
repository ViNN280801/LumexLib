// LumexStringifyV2.cxx11.tests.cpp
//
// stringify_v2 from C++11: the same text as stringify for every argument list,
// and a constraint that is std::enable_if in every standard, so that a call
// with an argument that cannot be streamed finds no overload (stringify
// itself stops with a static_assert below C++20). The detector below asks the
// compiler instead of compiling a rejected call. The C++20 file ties the
// constraint to the concept AllStringifiable.
#include <cstddef>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/string/LumexString"

#include "lumex/tests/core/string/LumexStringTestFixtures.hpp"

namespace utility_traits = lumex::core::utility::traits;
using lumex::core::string::utility::stringify;
using lumex::core::string::utility::stringify_v2;

namespace
{
// stringify_v2 (declval<Args> ()...) is well-formed.
template <typename Void, typename... Args>
struct can_stringify_v2_impl : std::false_type
{
};

template <typename... Args>
struct can_stringify_v2_impl<
    utility_traits::meta::void_t<decltype (stringify_v2 (
        std::declval<Args> ()...))>,
    Args...> : std::true_type
{
};

template <typename... Args>
struct can_stringify_v2 : can_stringify_v2_impl<void, Args...>
{
};

struct adl_streamable
{
  int value;
};

std::ostream &
operator<< (std::ostream &stream, adl_streamable const &object)
{
  return stream << "only(" << object.value << ")";
}
} // namespace

// --- the text
// --------------------------------------------------------------------

TEST (LumexStringifyV2Test, GivenNoArguments_WhenCalled_ThenEmptyString)
{
  EXPECT_EQ (stringify_v2 (), "");
  EXPECT_EQ (stringify_v2 (), stringify ());
}

TEST (LumexStringifyV2Test, GivenOneArgument_WhenCalled_ThenItsText)
{
  EXPECT_EQ (stringify_v2 (42), "42");
  EXPECT_EQ (stringify_v2 (-7), "-7");
  EXPECT_EQ (stringify_v2 (1.5), "1.5");
  EXPECT_EQ (stringify_v2 ('x'), "x");
  EXPECT_EQ (stringify_v2 (true), "1");
  EXPECT_EQ (stringify_v2 ("text"), "text");
  EXPECT_EQ (stringify_v2 (std::string ("string")), "string");
  EXPECT_EQ (stringify_v2 (3U), "3");
  EXPECT_EQ (stringify_v2 (4L), "4");
  EXPECT_EQ (stringify_v2 (5ULL), "5");
  EXPECT_EQ (stringify_v2 (2.5F), "2.5");
}

TEST (LumexStringifyV2Test, GivenSeveralArguments_WhenCalled_ThenConcatenated)
{
  EXPECT_EQ (stringify_v2 ("channel=", 2, " flow=", 1.5),
             "channel=2 flow=1.5");
  EXPECT_EQ (stringify_v2 ("a", "b", "c", "d", "e", "f", "g", "h"),
             "abcdefgh");
  EXPECT_EQ (stringify_v2 (1, 2, 3, 4, 5, 6, 7, 8, 9, 10), "12345678910");
}

TEST (LumexStringifyV2Test, GivenAnyArguments_WhenCalled_ThenSameAsStringify)
{
  CustomStreamable const custom (7);
  std::string const text = "t";
  int const number = 12;
  EXPECT_EQ (stringify_v2 (custom), stringify (custom));
  EXPECT_EQ (stringify_v2 (text, number, custom, 0.25),
             stringify (text, number, custom, 0.25));
  EXPECT_EQ (stringify_v2 (text, number, custom, 0.25),
             "t12CustomStreamable(7)0.25");
  EXPECT_EQ (stringify_v2 (std::string (), std::string ()), "");
  EXPECT_EQ (stringify_v2 ("", ""), stringify ("", ""));
}

TEST (LumexStringifyV2Test, GivenLvaluesAndRvalues_WhenCalled_ThenSameText)
{
  std::string lvalue = "left";
  std::string const constant = "const";
  EXPECT_EQ (stringify_v2 (lvalue, constant, std::string ("right")),
             "leftconstright");
  EXPECT_EQ (lvalue, "left");
}

TEST (LumexStringifyV2Test, GivenArrays_WhenCalled_ThenTheyDecayToText)
{
  char const literal[] = "array";
  char mutable_text[] = "mutable";
  EXPECT_EQ (stringify_v2 (literal), "array");
  EXPECT_EQ (stringify_v2 (mutable_text), "mutable");
  char const *const pointer = "pointer";
  EXPECT_EQ (stringify_v2 (pointer), "pointer");
}

TEST (LumexStringifyV2Test, GivenSmartPointers_WhenCalled_ThenTheRawAddress)
{
  // Both smart pointers stream the address they hold, as with stringify.
  std::unique_ptr<int> unique (new int (1));
  std::shared_ptr<int> shared = std::make_shared<int> (2);
  std::ostringstream expected_unique;
  expected_unique << static_cast<void const *> (unique.get ());
  std::ostringstream expected_shared;
  expected_shared << static_cast<void const *> (shared.get ());
  EXPECT_EQ (stringify_v2 (unique), expected_unique.str ());
  EXPECT_EQ (stringify_v2 (shared), expected_shared.str ());
  EXPECT_EQ (stringify_v2 ("[", unique, "|", shared, "]"),
             "[" + expected_unique.str () + "|" + expected_shared.str ()
                 + "]");
  std::unique_ptr<int> const empty;
  EXPECT_EQ (stringify_v2 (empty), stringify (empty));
}

TEST (LumexStringifyV2Test, GivenOperatorFoundByAdl_WhenCalled_ThenUsed)
{
  adl_streamable const object = { 3 };
  EXPECT_EQ (stringify_v2 (object), "only(3)");
  EXPECT_EQ (stringify_v2 ("<", object, ">"), "<only(3)>");
}

TEST (LumexStringifyV2Test, GivenLongText_WhenCalled_ThenNothingIsCut)
{
  std::string const part (1000, 'x');
  std::string const result = stringify_v2 (part, part, "!");
  EXPECT_EQ (result.size (), 2001U);
  EXPECT_EQ (result.back (), '!');
}

// --- the constraint
// -----------------------------------------------------------------

TEST (LumexStringifyV2Test, GivenStreamableArguments_WhenDetecting_ThenExists)
{
  EXPECT_TRUE ((can_stringify_v2<>::value));
  EXPECT_TRUE ((can_stringify_v2<int>::value));
  EXPECT_TRUE ((can_stringify_v2<int &>::value));
  EXPECT_TRUE ((can_stringify_v2<int const &>::value));
  EXPECT_TRUE ((can_stringify_v2<std::string>::value));
  EXPECT_TRUE ((can_stringify_v2<std::string const &>::value));
  EXPECT_TRUE ((can_stringify_v2<char const (&)[4]>::value));
  EXPECT_TRUE ((can_stringify_v2<char const *>::value));
  EXPECT_TRUE ((can_stringify_v2<CustomStreamable>::value));
  EXPECT_TRUE (
      (can_stringify_v2<int, std::string, CustomStreamable, double>::value));
  EXPECT_TRUE ((can_stringify_v2<std::unique_ptr<int>>::value));
  EXPECT_TRUE ((can_stringify_v2<std::unique_ptr<int> const &>::value));
  EXPECT_TRUE ((can_stringify_v2<std::shared_ptr<double>>::value));
  EXPECT_TRUE ((can_stringify_v2<adl_streamable>::value));
}

TEST (LumexStringifyV2Test,
      GivenAnArgumentThatCannotBeStreamed_WhenDetecting_ThenNoOverload)
{
  EXPECT_FALSE ((can_stringify_v2<NonStreamable>::value));
  EXPECT_FALSE ((can_stringify_v2<NonStreamable &>::value));
  EXPECT_FALSE ((can_stringify_v2<NonStreamable const &>::value));
  EXPECT_FALSE ((can_stringify_v2<int, NonStreamable>::value));
  EXPECT_FALSE ((can_stringify_v2<NonStreamable, int>::value));
  EXPECT_FALSE (
      (can_stringify_v2<int, std::string, NonStreamable, double>::value));
  EXPECT_FALSE ((can_stringify_v2<std::vector<int>>::value));
  EXPECT_FALSE ((can_stringify_v2<std::vector<int> &>::value));
  EXPECT_FALSE ((can_stringify_v2<std::pair<int, int>>::value));
}

TEST (LumexStringifyV2Test,
      GivenWideChar_WhenDetecting_ThenFollowsTheStandardLibrary)
{
  // Streamed as its number before C++20; from C++20 the library deletes
  // `operator<<` of a narrow stream for wchar_t and so the call has no
  // overload.
  EXPECT_EQ ((can_stringify_v2<wchar_t>::value),
             (utility_traits::stream::is_ostreamable<wchar_t>::value));
  EXPECT_EQ ((can_stringify_v2<char16_t>::value),
             (utility_traits::stream::is_ostreamable<char16_t>::value));
  EXPECT_EQ ((can_stringify_v2<char32_t>::value),
             (utility_traits::stream::is_ostreamable<char32_t>::value));
}

TEST (LumexStringifyV2Test, GivenResult_WhenDecltype_ThenIsAString)
{
  static_assert (
      std::is_same<decltype (stringify_v2 (1, "a")), std::string>::value, "");
  static_assert (std::is_same<decltype (stringify_v2 ()), std::string>::value,
                 "");
  SUCCEED ();
}

TEST (LumexStringifyV2Test,
      GivenSomeArgumentLists_WhenCompared_ThenTheConstraintIsAllStreamable)
{
  // Below C++20 the condition of stringify_v2 is traits::stream::
  // all_streamable, which the tests of the traits cover; from C++20 it is
  // the concept AllStringifiable, tied to the trait in
  // LumexString.cxx20.tests.cpp. On the types used above the two agree.
  EXPECT_EQ (
      (can_stringify_v2<int, NonStreamable>::value),
      (utility_traits::stream::all_streamable<int, NonStreamable>::value));
  EXPECT_EQ (
      (can_stringify_v2<int, std::string>::value),
      (utility_traits::stream::all_streamable<int, std::string>::value));
  EXPECT_EQ (
      (can_stringify_v2<std::unique_ptr<int>>::value),
      (utility_traits::stream::all_streamable<std::unique_ptr<int>>::value));
}
