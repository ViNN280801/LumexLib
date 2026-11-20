// SuccessFailureLiteral.cxx11.tests.cpp
//
// success_t<V> and failure_t<E> are literal types for a literal V and E at
// every standard (todo 108): their constructors are LUMEX_CONSTEXPR_CTOR and
// the destructor is trivial. A static_assert here stops the build when a
// constructor loses its constexpr; the run-time test shows that the object
// built in a constant expression holds the value that was given.
#include <string>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"
#include "lumex/core/expected/result/SuccessFailure.hpp"

namespace
{
using lumex::core::expected::result::expected;
using lumex::core::expected::result::failure_t;
using lumex::core::expected::result::success_t;

constexpr int kSeven = 7;
constexpr success_t<int> kMovedValue (7);       // the rvalue constructor
constexpr success_t<int> kCopiedValue (kSeven); // the const lvalue one
constexpr failure_t<int> kMovedError (8);
constexpr failure_t<int> kCopiedError (kSeven);
constexpr success_t<int> kCopyOfMarker (kMovedValue); // implicit copy

static_assert (std::is_trivially_destructible<success_t<int>>::value,
               "success_t<int> has a trivial destructor");
static_assert (std::is_trivially_destructible<failure_t<int>>::value,
               "failure_t<int> has a trivial destructor");
static_assert (std::is_trivially_copy_constructible<success_t<int>>::value,
               "success_t<int> is trivially copyable");
} // namespace

TEST (SuccessFailureLiteralTest, ConstantMarkers_HoldTheGivenValue)
{
  EXPECT_EQ (kMovedValue.value (), 7);
  EXPECT_EQ (kCopiedValue.value (), 7);
  EXPECT_EQ (kCopyOfMarker.value (), 7);
  EXPECT_EQ (kMovedError.error (), 8);
  EXPECT_EQ (kCopiedError.error (), 7);
  expected<int, int> const converted = kMovedValue;
  ASSERT_TRUE (converted.has_value ());
  EXPECT_EQ (*converted, 7);
}

TEST (SuccessFailureLiteralTest, MarkerOfNonLiteralType_StillWorks)
{
  // A std::string marker is not a constant expression; the constructors stay
  // usable at run time (the constexpr of a member of a class template does not
  // reject a specialization that cannot be constant).
  std::string text ("abc");
  success_t<std::string> const copied (text);
  EXPECT_EQ (copied.value (), "abc");
  success_t<std::string> moved (std::move (text));
  EXPECT_EQ (std::move (moved).value (), "abc");
  failure_t<std::string> const error ((std::string ("bad")));
  EXPECT_EQ (error.error (), "bad");
}
