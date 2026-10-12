// bad_expected_access<void>, the base of every bad_expected_access<E>, as in
// std::expected: one catch of it takes the failed value () of any expected.
// The source is compiled into every suite of the module (C++11, C++17 and
// C++20).

#include <exception>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/tests/core/expected/ExpectedParitySupport.hpp"

using namespace lumex::core::expected::result;
using namespace lumex::core::expected::error;
using namespace expected_parity;

namespace
{

constexpr int kError = 42;

/// Reads the value of `uut` and reports how the failure was caught.
template <typename Uut>
std::string
catch_through_the_base (Uut &uut)
{
  try
    {
      LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (uut.value ());
    }
  catch (bad_expected_access<void> const &exception)
    {
      return exception.what ();
    }
  return "no exception";
}

} // namespace

TEST (BadExpectedAccessBaseTest,
      Hierarchy_EveryErrorType_DerivesFromTheVoidBase)
{
  static_assert (std::is_base_of<bad_expected_access<void>,
                                 bad_expected_access<int>>::value,
                 "int");
  static_assert (std::is_base_of<bad_expected_access<void>,
                                 bad_expected_access<std::string>>::value,
                 "string");
  static_assert (std::is_base_of<bad_expected_access<void>,
                                 bad_expected_access<tracked_t>>::value,
                 "tracked_t");
  static_assert (
      std::is_base_of<std::exception, bad_expected_access<void>>::value,
      "the void base is a std::exception");
  static_assert (
      std::is_base_of<std::exception, bad_expected_access<int>>::value,
      "so is every other one");
  SUCCEED ();
}

TEST (BadExpectedAccessBaseTest, ValueOfAnError_CaughtThroughTheVoidBase)
{
  expected<int, int> int_error (unexpect, kError);
  expected<std::string, std::string> string_error (unexpect,
                                                   std::string ("bad"));
  expected<void, tracked_t> void_error (unexpect, kError);

  EXPECT_EQ (catch_through_the_base (int_error), "Bad expected access");
  EXPECT_EQ (catch_through_the_base (string_error), "Bad expected access");
  EXPECT_EQ (catch_through_the_base (void_error), "Bad expected access");
}

TEST (BadExpectedAccessBaseTest,
      ValueOfAnError_StillCarriesTheErrorWhenCaughtByItsOwnType)
{
  expected<int, std::string> uut (unexpect, std::string ("bad"));

  try
    {
      LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (uut.value ());
      FAIL () << "value () must throw";
    }
  catch (bad_expected_access<std::string> const &exception)
    {
      EXPECT_EQ (exception.error (), "bad");
    }
}

TEST (BadExpectedAccessBaseTest,
      ValueOfAnError_CaughtAsStdExceptionKeepsTheText)
{
  expected<int, int> uut (unexpect, kError);

  try
    {
      LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (uut.value ());
      FAIL () << "value () must throw";
    }
  catch (std::exception const &exception)
    {
      EXPECT_STREQ (exception.what (), "Bad expected access");
    }
}

TEST (BadExpectedAccessBaseTest, VoidBase_CannotBeMadeOrCopiedByAUser)
{
  static_assert (
      !std::is_default_constructible<bad_expected_access<void>>::value,
      "only a derived class can construct the base");
  static_assert (!std::is_copy_constructible<bad_expected_access<void>>::value,
                 "only a derived class can copy the base");
  static_assert (std::is_copy_constructible<bad_expected_access<int>>::value,
                 "the derived class is copyable, like every exception");
  SUCCEED ();
}

TEST (BadExpectedAccessBaseTest,
      Exception_CopiedThroughAnExceptionPtr_StillCaughtThroughTheBase)
{
  std::exception_ptr pointer;
  expected<int, int> uut (unexpect, kError);

  try
    {
      LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (uut.value ());
    }
  catch (...)
    {
      pointer = std::current_exception ();
    }

  ASSERT_TRUE (static_cast<bool> (pointer));
  try
    {
      std::rethrow_exception (pointer);
      FAIL () << "the exception must be rethrown";
    }
  catch (bad_expected_access<void> const &exception)
    {
      EXPECT_STREQ (exception.what (), "Bad expected access");
    }
}

TEST (BadExpectedAccessBaseTest, Error_EveryValueCategory_IsNoexcept)
{
  using uut_t = bad_expected_access<std::string>;

  static_assert (noexcept (std::declval<uut_t &> ().error ()), "&");
  static_assert (noexcept (std::declval<uut_t const &> ().error ()),
                 "const &");
  static_assert (noexcept (std::declval<uut_t &&> ().error ()), "&&");
  static_assert (noexcept (std::declval<uut_t const &&> ().error ()),
                 "const &&");
  static_assert (
      noexcept (std::declval<bad_expected_access<void> const &> ().what ()),
      "what () is noexcept");
  SUCCEED ();
}
