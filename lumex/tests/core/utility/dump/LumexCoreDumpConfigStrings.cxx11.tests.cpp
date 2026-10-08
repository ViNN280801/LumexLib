// LumexCoreDumpConfigStrings.cxx11.tests.cpp
//
// The string-taking members of dump_configuration and core_dump_generator
// that accept more than std::string: set_filename, set_directory,
// add_memory_filter and the two generate_instance_dump overloads. Before the
// C++11 port they were constrained by the concept StringLike and existed only
// from C++20; now they are SFINAE members of every standard, constrained by
// traits::string::is_string_convertible (an implicit conversion to
// std::string, and from C++17 to std::string_view). The detectors below ask
// the compiler instead of compiling a rejected call. What each member does
// with the accepted types is run on a dump_configuration; the instance
// overloads are run on a generator in LumexCoreDumpInstance. std::string_view
// is in LumexCoreDumpConfigStrings.cxx17, the agreement with the concept in
// LumexCoreDumpConfigStrings.cxx20.
#include <cstddef>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/dump/LumexCoreDumpGenerator.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"
#include "lumex/tests/core/utility/dump/LumexCoreDumpTestTypes.hpp"

namespace traits = lumex::core::utility::traits;
using lumex::core::utility::dump::core_dump_generator;
using lumex::core::utility::dump::dump_configuration;
using lumex_dump_test::accepted_by_all;
using lumex_dump_test::derived_text;
using lumex_dump_test::explicit_text;
using lumex_dump_test::follows_the_trait;
using lumex_dump_test::implicit_text;
using lumex_dump_test::rejected_by_all;

namespace
{
/// Each setter stores `good`.
template <typename Good>
void
expect_setters_store (Good const &good, std::string const &expected)
{
  dump_configuration config;
  EXPECT_TRUE (config.set_filename (good));
  EXPECT_EQ (config.get_filename (), expected);
  EXPECT_TRUE (config.set_directory (good));
  EXPECT_EQ (config.get_directory (), expected);
  EXPECT_TRUE (config.add_memory_filter (good));
  ASSERT_EQ (config.get_memory_filters ().size (), 1u);
  EXPECT_EQ (config.get_memory_filters ().front (), expected);
}

template <typename Bad>
void
expect_setters_reject (Bad const &bad)
{
  dump_configuration config;
  ASSERT_TRUE (config.set_filename (std::string ("keep.dump")));
  ASSERT_TRUE (config.set_directory (std::string ("keep_dir")));
  EXPECT_FALSE (config.set_filename (bad));
  EXPECT_EQ (config.get_filename (), "keep.dump");
  EXPECT_FALSE (config.set_directory (bad));
  EXPECT_EQ (config.get_directory (), "keep_dir");
  EXPECT_FALSE (config.add_memory_filter (bad));
  EXPECT_TRUE (config.get_memory_filters ().empty ());
}
} // namespace

// --- Which argument types the members take ---

TEST (LumexCoreDumpConfigStringsTest,
      GivenStdStringInEveryForm_WhenCalled_ThenTaken)
{
  static_assert (accepted_by_all<std::string>::value, "std::string");
  static_assert (accepted_by_all<std::string &>::value, "std::string &");
  static_assert (accepted_by_all<std::string const &>::value,
                 "std::string const &");
  static_assert (accepted_by_all<std::string &&>::value, "std::string &&");
  SUCCEED ();
}

TEST (LumexCoreDumpConfigStringsTest,
      GivenCharacterPointersAndLiterals_WhenCalled_ThenTaken)
{
  static_assert (accepted_by_all<char const *>::value, "char const *");
  static_assert (accepted_by_all<char *>::value, "char *");
  static_assert (accepted_by_all<char const *const &>::value,
                 "char const * const &");
  static_assert (accepted_by_all<char const (&)[12]>::value,
                 "a string literal");
  static_assert (accepted_by_all<char (&)[12]>::value, "a char array");
  SUCCEED ();
}

TEST (LumexCoreDumpConfigStringsTest,
      GivenUserTypesWithAConversionToStdString_WhenCalled_ThenTaken)
{
  static_assert (accepted_by_all<implicit_text>::value,
                 "an implicit conversion to std::string");
  static_assert (accepted_by_all<implicit_text const &>::value,
                 "the same by const reference");
  static_assert (accepted_by_all<derived_text>::value,
                 "a class derived from std::string");
  SUCCEED ();
}

TEST (LumexCoreDumpConfigStringsTest,
      GivenTypesWithoutAnImplicitConversionToStdString_WhenCalled_ThenRejected)
{
  static_assert (rejected_by_all<int>::value, "int");
  static_assert (rejected_by_all<unsigned long>::value, "unsigned long");
  static_assert (rejected_by_all<char>::value, "char");
  static_assert (rejected_by_all<bool>::value, "bool");
  static_assert (rejected_by_all<double>::value, "double");
  static_assert (rejected_by_all<void *>::value, "void *");
  static_assert (rejected_by_all<std::string *>::value, "std::string *");
  static_assert (rejected_by_all<std::wstring>::value, "std::wstring");
  static_assert (rejected_by_all<wchar_t const *>::value, "wchar_t const *");
  static_assert (rejected_by_all<std::vector<char>>::value,
                 "std::vector<char>");
  static_assert (rejected_by_all<explicit_text>::value,
                 "a conversion to std::string that is explicit");
  SUCCEED ();
}

TEST (
    LumexCoreDumpConfigStringsTest,
    GivenTheTrait_WhenComparedWithTheMembers_ThenTheyTakeExactlyWhatItAccepts)
{
  // The members are constrained by the trait: every type it accepts is taken
  // by all five members, every type it rejects by none.
  static_assert (follows_the_trait<std::string>::value, "std::string");
  static_assert (follows_the_trait<char const *>::value, "char const *");
  static_assert (follows_the_trait<char const (&)[8]>::value, "a literal");
  static_assert (follows_the_trait<implicit_text>::value, "implicit_text");
  static_assert (follows_the_trait<derived_text>::value, "derived_text");
  static_assert (follows_the_trait<explicit_text>::value, "explicit_text");
  static_assert (follows_the_trait<int>::value, "int");
  static_assert (follows_the_trait<std::wstring>::value, "std::wstring");
  static_assert (follows_the_trait<std::vector<char>>::value,
                 "std::vector<char>");
  // The trait itself is true for the accepted and false for the rejected,
  // so the check above is not empty.
  static_assert (traits::string::is_string_convertible<char const *>::value,
                 "the trait accepts char const *");
  static_assert (!traits::string::is_string_convertible<int>::value,
                 "the trait rejects int");
  SUCCEED ();
}

TEST (LumexCoreDumpConfigStringsTest,
      GivenTheMembers_WhenInspected_ThenResultAndNoexceptAreAsDocumented)
{
  // The non-template overloads stay as they were; the template ones return
  // bool and are noexcept like them (generate_instance_dump with one argument
  // is not noexcept, as before).
  static_assert (
      std::is_same<
          decltype (std::declval<dump_configuration &> ().set_filename (
              std::declval<char const *> ())),
          bool>::value,
      "set_filename returns bool for a char pointer");
  static_assert (
      std::is_same<
          decltype (std::declval<dump_configuration &> ().add_memory_filter (
              std::declval<implicit_text> ())),
          bool>::value,
      "add_memory_filter returns bool for a user type");
  static_assert (noexcept (std::declval<dump_configuration &> ().set_filename (
                     std::declval<char const *> ())),
                 "set_filename is noexcept");
  static_assert (
      noexcept (std::declval<dump_configuration &> ().set_directory (
          std::declval<implicit_text const &> ())),
      "set_directory is noexcept");
  static_assert (
      noexcept (std::declval<dump_configuration &> ().add_memory_filter (
          std::declval<char const *> ())),
      "add_memory_filter is noexcept");
  static_assert (
      noexcept (std::declval<core_dump_generator &> ().generate_instance_dump (
          std::declval<char const *> (), std::declval<std::error_code &> ())),
      "generate_instance_dump with an error code is noexcept");
  static_assert (
      !noexcept (
          std::declval<core_dump_generator &> ().generate_instance_dump (
              std::declval<char const *> ())),
      "generate_instance_dump with one argument is not noexcept");
  SUCCEED ();
}

// --- What the setters do with the accepted types ---

TEST (LumexCoreDumpConfigStringsTest,
      GivenStdString_WhenSetters_ThenStoreTheValue)
{
  expect_setters_store (std::string ("heap.dump"), "heap.dump");
}

TEST (LumexCoreDumpConfigStringsTest,
      GivenCharPointer_WhenSetters_ThenStoreTheValue)
{
  char const *const pointer = "heap.dump";
  expect_setters_store (pointer, "heap.dump");
}

TEST (LumexCoreDumpConfigStringsTest,
      GivenStringLiteral_WhenSetters_ThenStoreTheValue)
{
  expect_setters_store ("heap.dump", "heap.dump");
}

TEST (LumexCoreDumpConfigStringsTest,
      GivenImplicitConversionType_WhenSetters_ThenStoreTheConvertedValue)
{
  expect_setters_store (implicit_text ("heap.dump"), "heap.dump");
}

TEST (LumexCoreDumpConfigStringsTest,
      GivenDerivedFromStdString_WhenSetters_ThenStoreTheValue)
{
  expect_setters_store (derived_text ("heap.dump"), "heap.dump");
}

TEST (
    LumexCoreDumpConfigStringsTest,
    GivenInvalidContent_WhenSettersWithEveryAcceptedType_ThenRejectedAndUnchanged)
{
  // "a|b" is rejected by all three (the pipe is forbidden in a file name, a
  // directory and a memory filter), and the earlier value stays.
  expect_setters_reject ("a|b");
  expect_setters_reject (std::string ("a|b"));
  expect_setters_reject (implicit_text ("a|b"));
  expect_setters_reject (derived_text ("a|b"));
  char const *const pointer = "a|b";
  expect_setters_reject (pointer);
}

TEST (
    LumexCoreDumpConfigStringsTest,
    GivenEmptyText_WhenSettersWithEveryAcceptedType_ThenFollowsTheStdStringRules)
{
  // An empty file name and directory are valid, an empty memory filter is not.
  dump_configuration config;
  EXPECT_TRUE (config.set_filename (""));
  EXPECT_TRUE (config.set_directory (""));
  EXPECT_FALSE (config.add_memory_filter (""));
  EXPECT_TRUE (config.set_filename (implicit_text ("")));
  EXPECT_FALSE (config.add_memory_filter (implicit_text ("")));
  EXPECT_TRUE (config.get_memory_filters ().empty ());
}

TEST (LumexCoreDumpConfigStringsTest,
      GivenSeveralFilters_WhenAddedWithDifferentTypes_ThenAllKeptInOrder)
{
  dump_configuration config;
  char const *const pointer = "stack";
  EXPECT_TRUE (config.add_memory_filter ("heap"));
  EXPECT_TRUE (config.add_memory_filter (pointer));
  EXPECT_TRUE (config.add_memory_filter (std::string ("anon")));
  EXPECT_TRUE (config.add_memory_filter (implicit_text ("shared")));
  std::vector<std::string> const expected
      = { "heap", "stack", "anon", "shared" };
  EXPECT_EQ (config.get_memory_filters (), expected);
  EXPECT_TRUE (config.is_valid ());
}

TEST (LumexCoreDumpConfigStringsTest,
      GivenSameValueThroughEveryType_WhenCompared_ThenConfigurationsAreEqual)
{
  dump_configuration from_string;
  dump_configuration from_pointer;
  dump_configuration from_user_type;
  EXPECT_TRUE (from_string.set_filename (std::string ("x.dump")));
  EXPECT_TRUE (from_pointer.set_filename ("x.dump"));
  EXPECT_TRUE (from_user_type.set_filename (implicit_text ("x.dump")));
  EXPECT_EQ (from_string, from_pointer);
  EXPECT_EQ (from_string, from_user_type);
}
