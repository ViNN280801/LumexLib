// One file that includes both value-holding modules. Each of them once gave
// its own `in_place` the global name `in_place`, so this file did not compile
// on any standard. Both tags are written in full: neither module has a global
// alias. The tests compile from C++11, so every expected suite (C++11, C++17,
// C++20) runs them.

#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"
#include "lumex/core/optional/LumexOptional"

namespace
{
using ResultType = lumex::core::expected::result::expected<int, std::string>;
using lumex::core::expected::result::unexpect;
} // namespace

TEST (ExpectedWithOptional, EachModuleBuildsItsValueInPlace)
{
  lumex::core::optional::opt::optional<int> const present (
      lumex::core::optional::opt::in_place, 7);
  ResultType const succeeded (lumex::core::expected::result::in_place, 7);

  ASSERT_TRUE (present.has_value ());
  EXPECT_EQ (*present, 7);
  ASSERT_TRUE (succeeded.has_value ());
  EXPECT_EQ (*succeeded, 7);
}

TEST (ExpectedWithOptional, AnExpectedLivesInAnOptional)
{
  lumex::core::optional::opt::optional<ResultType> const holder (
      lumex::core::optional::opt::in_place,
      lumex::core::expected::result::in_place, 5);
  ASSERT_TRUE (holder.has_value ());
  ASSERT_TRUE (holder->has_value ());
  EXPECT_EQ (**holder, 5);

  lumex::core::optional::opt::optional<ResultType> const failed (
      lumex::core::optional::opt::in_place, unexpect, std::string ("bad"));
  ASSERT_TRUE (failed.has_value ());
  EXPECT_FALSE (failed->has_value ());
  EXPECT_EQ (failed->error (), "bad");
}

TEST (ExpectedWithOptional, AnOptionalLivesInAnExpected)
{
  using Inner = lumex::core::optional::opt::optional<int>;
  lumex::core::expected::result::expected<Inner, std::string> const result (
      lumex::core::expected::result::in_place,
      lumex::core::optional::opt::in_place, 3);
  ASSERT_TRUE (result.has_value ());
  ASSERT_TRUE (result->has_value ());
  EXPECT_EQ (**result, 3);
}
