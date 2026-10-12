// The parts of the std::expected surface of expected<T, E> and expected<void,
// E> that need C++17: std::optional as a result type, and a result type that
// cannot be moved (guaranteed copy elision builds it in place). The C++17 and
// C++20 suites run this file; the C++11 suite cannot.

#include <atomic>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"
#include "lumex/tests/core/expected/ExpectedParitySupport.hpp"

using namespace lumex::core::expected::result;
using namespace lumex::core::expected::error;
using namespace expected_parity;

namespace
{

constexpr int kValue = 7;
constexpr int kError = 42;

/// Cannot be copied or moved.
struct pinned_t
{
  std::atomic<int> value;

  explicit pinned_t (int v) : value (v) {}
  pinned_t (pinned_t const &) = delete;
  pinned_t &operator= (pinned_t const &) = delete;
};

} // namespace

TEST (ExpectedStdParityTest, Transform_OptionalResult_IsAValue)
{
  expected<int, std::string> const present (kValue);
  expected<int, std::string> const absent (unexpect, std::string ("none"));

  auto engaged = present.transform ([] (int value)
                                      { return std::optional<int> (value); });
  auto empty = present.transform ([] (int) { return std::optional<int> (); });
  auto skipped = absent.transform ([] (int value)
                                     { return std::optional<int> (value); });

  static_assert (
      std::is_same<decltype (engaged),
                   expected<std::optional<int>, std::string>>::value,
      "an optional is a value type like any other");
  ASSERT_TRUE (engaged.has_value ());
  ASSERT_TRUE (engaged.value ().has_value ());
  EXPECT_EQ (*engaged.value (), kValue);
  ASSERT_TRUE (empty.has_value ()) << "the outer expected holds a value";
  EXPECT_FALSE (empty.value ().has_value ());
  ASSERT_FALSE (skipped.has_value ());
  EXPECT_EQ (skipped.error (), "none");
}

TEST (ExpectedStdParityTest,
      Transform_OptionalResultOfVoidSpecialization_IsAValue)
{
  expected<void, std::string> const uut;

  auto result = uut.transform ([] { return std::optional<int> (kValue); });

  static_assert (
      std::is_same<decltype (result),
                   expected<std::optional<int>, std::string>>::value,
      "an optional is a value type like any other");
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (*result.value (), kValue);
}

TEST (ExpectedStdParityTest, Transform_ResultThatCannotBeMoved_IsBuiltInPlace)
{
  expected<int, int> const uut (kValue);

  auto result = uut.transform ([] (int value) { return pinned_t (value); });

  static_assert (
      std::is_same<decltype (result), expected<pinned_t, int>>::value,
      "the result type is the type the function returns");
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (result.value ().value.load (), kValue);
}

TEST (ExpectedStdParityTest,
      TransformError_ResultThatCannotBeMoved_IsBuiltInPlace)
{
  expected<int, int> const uut (unexpect, kError);

  auto result
      = uut.transform_error ([] (int value) { return pinned_t (value); });

  static_assert (
      std::is_same<decltype (result), expected<int, pinned_t>>::value,
      "the error type is the type the function returns");
  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (result.error ().value.load (), kError);
}

TEST (ExpectedStdParityTest,
      Transform_VoidSpecializationResultThatCannotBeMoved_IsBuiltInPlace)
{
  expected<void, int> const uut;

  auto result = uut.transform ([] { return pinned_t (kValue); });

  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (result.value ().value.load (), kValue);
}
