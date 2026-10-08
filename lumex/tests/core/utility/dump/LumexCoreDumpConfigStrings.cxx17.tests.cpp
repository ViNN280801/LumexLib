// LumexCoreDumpConfigStrings.cxx17.tests.cpp
//
// From C++17 the members that take a string also take std::string_view and
// every type that converts implicitly to it: traits::string::
// is_string_convertible adds that conversion at C++17, so the setters of
// dump_configuration and the instance overloads of core_dump_generator accept
// a view without a std::string at the call. Before the C++11 port this held
// only at C++20 (the concept StringLike). The members that take the other
// types are in LumexCoreDumpConfigStrings.cxx11.
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/utility/dump/LumexCoreDumpGenerator.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"
#include "lumex/tests/core/utility/dump/LumexCoreDumpTestTypes.hpp"

namespace traits = lumex::core::utility::traits;
using lumex::core::utility::dump::dump_configuration;
using lumex_dump_test::accepted_by_all;
using lumex_dump_test::rejected_by_all;

namespace
{
/// A user type whose only conversion is to std::string_view.
struct view_text
{
  explicit view_text (char const *chars) : text (chars) {}

  operator std::string_view () const // NOLINT(google-explicit-constructor)
  {
    return text;
  }

  std::string_view text;
};
} // namespace

TEST (LumexCoreDumpConfigStringsCxx17Test,
      GivenStringView_WhenCalled_ThenTakenInEveryForm)
{
  static_assert (accepted_by_all<std::string_view>::value, "string_view");
  static_assert (accepted_by_all<std::string_view &>::value, "string_view &");
  static_assert (accepted_by_all<std::string_view const &>::value,
                 "string_view const &");
  static_assert (accepted_by_all<std::string_view &&>::value,
                 "string_view &&");
  SUCCEED ();
}

TEST (LumexCoreDumpConfigStringsCxx17Test,
      GivenUserTypeConvertingToStringView_WhenCalled_ThenTaken)
{
  static_assert (accepted_by_all<view_text>::value,
                 "a conversion to std::string_view");
  SUCCEED ();
}

TEST (LumexCoreDumpConfigStringsCxx17Test,
      GivenWideViewAndNumbers_WhenCalled_ThenStillRejected)
{
  static_assert (rejected_by_all<std::wstring_view>::value,
                 "std::wstring_view");
  static_assert (rejected_by_all<std::u16string_view>::value,
                 "std::u16string_view");
  static_assert (rejected_by_all<int>::value, "int");
  SUCCEED ();
}

TEST (LumexCoreDumpConfigStringsCxx17Test,
      GivenTheTrait_WhenStringView_ThenAcceptsItFromCxx17)
{
  static_assert (
      traits::string::is_string_convertible<std::string_view>::value,
      "the trait counts std::string_view from C++17");
  static_assert (
      !std::is_convertible<std::string_view, std::string>::value,
      "std::string_view converts to std::string only explicitly, so the "
      "trait's second term is what makes the members take a view");
  SUCCEED ();
}

TEST (LumexCoreDumpConfigStringsCxx17Test,
      GivenStringView_WhenSetters_ThenStoreTheValue)
{
  std::string_view const view = "heap.dump";
  dump_configuration config;
  EXPECT_TRUE (config.set_filename (view));
  EXPECT_EQ (config.get_filename (), "heap.dump");
  EXPECT_TRUE (config.set_directory (view));
  EXPECT_EQ (config.get_directory (), "heap.dump");
  EXPECT_TRUE (config.add_memory_filter (view));
  ASSERT_EQ (config.get_memory_filters ().size (), 1u);
  EXPECT_EQ (config.get_memory_filters ().front (), "heap.dump");
}

TEST (LumexCoreDumpConfigStringsCxx17Test,
      GivenPartOfAString_WhenSettersWithStringView_ThenStoreOnlyThatPart)
{
  std::string const whole = "prefix/name.dump";
  std::string_view const tail = std::string_view (whole).substr (7);
  dump_configuration config;
  EXPECT_TRUE (config.set_filename (tail));
  EXPECT_EQ (config.get_filename (), "name.dump");
  EXPECT_TRUE (config.add_memory_filter (tail));
  EXPECT_EQ (config.get_memory_filters ().front (), "name.dump");
}

TEST (LumexCoreDumpConfigStringsCxx17Test,
      GivenInvalidView_WhenSetters_ThenRejectedAndUnchanged)
{
  std::string_view const bad = "a|b";
  dump_configuration config;
  ASSERT_TRUE (config.set_filename (std::string ("keep.dump")));
  EXPECT_FALSE (config.set_filename (bad));
  EXPECT_EQ (config.get_filename (), "keep.dump");
  EXPECT_FALSE (config.set_directory (bad));
  EXPECT_TRUE (config.get_directory ().empty ());
  EXPECT_FALSE (config.add_memory_filter (bad));
  EXPECT_TRUE (config.get_memory_filters ().empty ());
  EXPECT_FALSE (config.add_memory_filter (std::string_view ()));
}

TEST (LumexCoreDumpConfigStringsCxx17Test,
      GivenUserTypeConvertingToStringView_WhenSetters_ThenStoreTheValue)
{
  dump_configuration config;
  EXPECT_TRUE (config.set_filename (view_text ("view.dump")));
  EXPECT_EQ (config.get_filename (), "view.dump");
  EXPECT_TRUE (config.add_memory_filter (view_text ("stack")));
  EXPECT_EQ (config.get_memory_filters ().front (), "stack");
}
