// make_unexpected<E> (args...) is the only factory of that name: it returns an
// unexpected<E>. The two overloads of 1.x that returned an expected
// (make_unexpected<T> (error) and make_unexpected (error)) made
// make_unexpected<int> (1) ambiguous with this one and were removed. The tests
// compile from C++11, so every expected suite runs them.

#include <string>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"

namespace
{
using lumex::core::expected::error::unexpected;
using lumex::core::expected::result::expected;
using lumex::core::expected::result::make_unexpected;

constexpr int kError = 7;
} // namespace

TEST (MakeUnexpectedTest, ScalarErrorType_IsNotAmbiguous)
{
  auto failed = make_unexpected<int> (kError);

  static_assert (std::is_same<decltype (failed), unexpected<int>>::value,
                 "make_unexpected<E> returns an unexpected<E>");
  EXPECT_EQ (failed.error (), kError);
}

TEST (MakeUnexpectedTest, ErrorConstructedFromSeveralArguments)
{
  auto failed = make_unexpected<std::string> (3u, 'z');

  static_assert (
      std::is_same<decltype (failed), unexpected<std::string>>::value,
      "make_unexpected<E> returns an unexpected<E>");
  EXPECT_EQ (failed.error (), "zzz");
}

TEST (MakeUnexpectedTest, NoArguments_ValueInitializesTheError)
{
  auto failed = make_unexpected<int> ();

  EXPECT_EQ (failed.error (), 0);
}

TEST (MakeUnexpectedTest, ErrorTypeIsDecayed)
{
  static_assert (std::is_same<decltype (make_unexpected<int const &> (kError)),
                              unexpected<int>>::value,
                 "the error type is the decayed E");
}

TEST (MakeUnexpectedTest, ResultConvertsToAnExpectedInTheErrorState)
{
  expected<int, int> const result = make_unexpected<int> (kError);
  expected<void, std::string> const voided
      = make_unexpected<std::string> (2u, 'q');

  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (result.error (), kError);
  ASSERT_FALSE (voided.has_value ());
  EXPECT_EQ (voided.error (), "qq");
}

TEST (MakeUnexpectedTest, ReturnedFromAFunction)
{
  struct local
  {
    static expected<int, std::string>
    fail ()
    {
      return make_unexpected<std::string> ("bad");
    }
  };

  expected<int, std::string> const result = local::fail ();

  ASSERT_FALSE (result.has_value ());
  EXPECT_EQ (result.error (), "bad");
}
