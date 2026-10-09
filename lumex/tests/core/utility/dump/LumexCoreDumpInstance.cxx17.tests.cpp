// LumexCoreDumpInstance.cxx17.tests.cpp
//
// The result of core_dump_generator::get_optional_dump_directory is the
// optional of this library in every standard, never std::optional; from C++17
// it converts to and from std::optional<std::string> (the type itself and the
// behavior are checked in LumexCoreDumpInstance.cxx11 in every suite).
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/utility/dump/LumexCoreDumpGenerator.hpp"

using lumex::core::utility::dump::core_dump_generator;

TEST (LumexCoreDumpInstanceCxx17Test,
      GivenCxx17_WhenOptionalDumpDirectoryAlias_ThenNotStdOptional)
{
  static_assert (
      !std::is_same<core_dump_generator::optional_dump_directory_t,
                    std::optional<std::string>>::value,
      "the optional of the library, not std::optional, from C++17 too");
  static_assert (
      std::is_same<core_dump_generator::optional_dump_directory_t,
                   lumex::core::optional::opt::optional<std::string>>::value,
      "the optional of the library");
  SUCCEED ();
}

TEST (LumexCoreDumpInstanceCxx17Test,
      GivenCxx17_WhenGetOptionalDumpDirectory_ThenConvertsToAndFromStd)
{
  using result_t = decltype (std::declval<core_dump_generator const &> ()
                                 .get_optional_dump_directory ());
  static_assert (
      std::is_same<result_t,
                   core_dump_generator::optional_dump_directory_t>::value,
      "the result is the alias");
  static_assert (
      std::is_convertible<result_t, std::optional<std::string>>::value,
      "converts to std::optional<std::string>");
  static_assert (
      std::is_convertible<std::optional<std::string>, result_t>::value,
      "std::optional<std::string> converts to the result");
  SUCCEED ();
}

TEST (LumexCoreDumpInstanceCxx17Test,
      GivenAliasValue_WhenConvertedToStdOptional_ThenSameState)
{
  core_dump_generator::optional_dump_directory_t empty;
  core_dump_generator::optional_dump_directory_t full = std::string ("/d");
  std::optional<std::string> const stdEmpty = empty;
  std::optional<std::string> const stdFull = full;
  EXPECT_FALSE (stdEmpty.has_value ());
  ASSERT_TRUE (stdFull.has_value ());
  EXPECT_EQ (*stdFull, "/d");
  EXPECT_TRUE (full == stdFull);
  EXPECT_TRUE (empty == stdEmpty);
  core_dump_generator::optional_dump_directory_t const back = stdFull;
  ASSERT_TRUE (back.has_value ());
  EXPECT_EQ (*back, "/d");
}
