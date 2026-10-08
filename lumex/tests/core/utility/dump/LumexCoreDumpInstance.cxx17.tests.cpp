// LumexCoreDumpInstance.cxx17.tests.cpp
//
// From C++17 the result of core_dump_generator::get_optional_dump_directory is
// std::optional<std::string>; before it the optional of this library (checked
// in LumexCoreDumpInstance.cxx11, where the behavior of both is run). The
// alias optional_dump_directory_t is the one place that chooses.
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/utility/dump/LumexCoreDumpGenerator.hpp"

using lumex::core::utility::dump::core_dump_generator;

TEST (LumexCoreDumpInstanceCxx17Test,
      GivenCxx17_WhenOptionalDumpDirectoryAlias_ThenStdOptional)
{
  static_assert (std::is_same<core_dump_generator::optional_dump_directory_t,
                              std::optional<std::string>>::value,
                 "std::optional<std::string> from C++17");
  SUCCEED ();
}

TEST (LumexCoreDumpInstanceCxx17Test,
      GivenCxx17_WhenGetOptionalDumpDirectory_ThenReturnsStdOptional)
{
  static_assert (
      std::is_same<decltype (std::declval<core_dump_generator const &> ()
                                 .get_optional_dump_directory ()),
                   std::optional<std::string>>::value,
      "the result is std::optional<std::string>");
  SUCCEED ();
}

TEST (LumexCoreDumpInstanceCxx17Test,
      GivenAliasValue_WhenUsedAsStdOptional_ThenStdOptionalApiWorks)
{
  core_dump_generator::optional_dump_directory_t empty;
  EXPECT_FALSE (empty.has_value ());
  EXPECT_TRUE (empty == std::nullopt);
  core_dump_generator::optional_dump_directory_t full = std::string ("/d");
  ASSERT_TRUE (full.has_value ());
  EXPECT_EQ (*full, "/d");
  EXPECT_EQ (full.value_or (std::string ("x")), "/d");
}
