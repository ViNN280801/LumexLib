// LumexCoreDumpConfigStrings.cxx20.tests.cpp
//
// The members that take a string used to be constrained by the concept
// traits::string::StringLike and to exist only from C++20. They are SFINAE
// members of every standard now (traits::string::is_string_convertible, the
// C++11 form of the concept); this file keeps the C++20 spelling honest: for
// every argument type the five members take it exactly when the concept holds
// for the type they deduce, so the overload set at C++20 is the one the
// concept gave. A toolchain without concepts (GCC 8 accepts -std=c++2a
// without them) reports GTEST_SKIP.
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/dump/LumexCoreDumpGenerator.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"
#include "lumex/tests/core/utility/dump/LumexCoreDumpTestTypes.hpp"

#if LUMEX_HAS_STD_CONCEPTS

namespace traits = lumex::core::utility::traits;
using lumex_dump_test::accepted_by_all;
using lumex_dump_test::derived_text;
using lumex_dump_test::explicit_text;
using lumex_dump_test::follows_the_trait;
using lumex_dump_test::implicit_text;
using lumex_dump_test::rejected_by_all;

namespace
{
/// A user type whose only conversion is to std::string_view.
struct view_text
{
  operator std::string_view () const // NOLINT(google-explicit-constructor)
  {
    return {};
  }
};

/// The members deduce the decayed type of a by-const-reference argument.
template <typename T>
using deduced_t = std::remove_cv_t<std::remove_reference_t<T>>;

/// All five members take `Argument` when the concept holds for the type they
/// deduce from it, and none takes it when the concept does not hold.
template <typename Argument>
constexpr bool agrees_with_the_concept
    = traits::string::StringLike<deduced_t<Argument>>
          ? accepted_by_all<Argument>::value
          : rejected_by_all<Argument>::value;
} // namespace

TEST (LumexCoreDumpConfigStringsCxx20Test,
      GivenTypesTheConceptAccepts_WhenMembersCalled_ThenTheyTakeThem)
{
  static_assert (traits::string::StringLike<std::string>, "std::string");
  static_assert (traits::string::StringLike<char const *>, "char const *");
  static_assert (traits::string::StringLike<implicit_text>, "implicit_text");
  static_assert (traits::string::StringLike<std::string_view>, "string_view");
  static_assert (traits::string::StringLike<view_text>, "view_text");
  static_assert (traits::string::StringLike<derived_text>, "derived_text");
  static_assert (accepted_by_all<std::string>::value, "std::string");
  static_assert (accepted_by_all<char const *>::value, "char const *");
  static_assert (accepted_by_all<implicit_text>::value, "implicit_text");
  static_assert (accepted_by_all<std::string_view>::value, "string_view");
  static_assert (accepted_by_all<view_text>::value, "view_text");
  static_assert (accepted_by_all<derived_text>::value, "derived_text");
  SUCCEED ();
}

TEST (LumexCoreDumpConfigStringsCxx20Test,
      GivenTypesTheConceptRejects_WhenMembersCalled_ThenTheyDoNotTakeThem)
{
  static_assert (!traits::string::StringLike<int>, "int");
  static_assert (!traits::string::StringLike<std::wstring>, "std::wstring");
  static_assert (!traits::string::StringLike<std::vector<char>>,
                 "std::vector<char>");
  static_assert (!traits::string::StringLike<explicit_text>, "explicit_text");
  static_assert (rejected_by_all<int>::value, "int");
  static_assert (rejected_by_all<std::wstring>::value, "std::wstring");
  static_assert (rejected_by_all<std::vector<char>>::value,
                 "std::vector<char>");
  static_assert (rejected_by_all<explicit_text>::value, "explicit_text");
  SUCCEED ();
}

TEST (LumexCoreDumpConfigStringsCxx20Test,
      GivenEveryArgumentForm_WhenComparedWithTheConcept_ThenTheyAgree)
{
  static_assert (agrees_with_the_concept<std::string>, "std::string");
  static_assert (agrees_with_the_concept<std::string &>, "std::string &");
  static_assert (agrees_with_the_concept<std::string const &>,
                 "std::string const &");
  static_assert (agrees_with_the_concept<std::string &&>, "std::string &&");
  static_assert (agrees_with_the_concept<char const *>, "char const *");
  static_assert (agrees_with_the_concept<char *>, "char *");
  static_assert (agrees_with_the_concept<char const (&)[8]>, "a literal");
  static_assert (agrees_with_the_concept<char (&)[8]>, "a char array");
  static_assert (agrees_with_the_concept<implicit_text>, "implicit_text");
  static_assert (agrees_with_the_concept<implicit_text const &>,
                 "implicit_text const &");
  static_assert (agrees_with_the_concept<derived_text>, "derived_text");
  static_assert (agrees_with_the_concept<explicit_text>, "explicit_text");
  static_assert (agrees_with_the_concept<std::string_view>, "string_view");
  static_assert (agrees_with_the_concept<std::string_view const &>,
                 "string_view const &");
  static_assert (agrees_with_the_concept<view_text>, "view_text");
  static_assert (agrees_with_the_concept<std::wstring_view>, "wstring_view");
  static_assert (agrees_with_the_concept<std::wstring>, "std::wstring");
  static_assert (agrees_with_the_concept<wchar_t const *>, "wchar_t const *");
  static_assert (agrees_with_the_concept<std::vector<char>>,
                 "std::vector<char>");
  static_assert (agrees_with_the_concept<void *>, "void *");
  static_assert (agrees_with_the_concept<int>, "int");
  static_assert (agrees_with_the_concept<unsigned long>, "unsigned long");
  static_assert (agrees_with_the_concept<char>, "char");
  static_assert (agrees_with_the_concept<double>, "double");
  SUCCEED ();
}

TEST (LumexCoreDumpConfigStringsCxx20Test,
      GivenTheConceptAndTheTrait_WhenCompared_ThenTheyAgreeOnEveryType)
{
  // The members are constrained by the trait, so their agreement with the
  // concept is the agreement of the trait with the concept, which the trait
  // tests check on more types; here on the types of this module.
  static_assert (follows_the_trait<std::string_view>::value, "string_view");
  static_assert (follows_the_trait<view_text>::value, "view_text");
  static_assert (traits::string::is_string_convertible<std::string>::value
                     == traits::string::StringLike<std::string>,
                 "std::string");
  static_assert (traits::string::is_string_convertible<std::string_view>::value
                     == traits::string::StringLike<std::string_view>,
                 "string_view");
  static_assert (traits::string::is_string_convertible<explicit_text>::value
                     == traits::string::StringLike<explicit_text>,
                 "explicit_text");
  SUCCEED ();
}

#else

TEST (LumexCoreDumpConfigStringsCxx20Test, GivenNoConcepts_ThenNothingToCheck)
{
  GTEST_SKIP () << "this toolchain has no concepts";
}

#endif
