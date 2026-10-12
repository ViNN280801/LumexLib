// The violation path is not noexcept (a handler may throw); in a noexcept
// function a throwing handler ends the program ([basic.contract.eval]); a
// holding assertion does not change the noexcept-ness of its function.

#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/contracts/LumexContracts"
#include "lumex/tests/core/contracts/LumexContractsTestSupport.hpp"
#include "lumex/tests/support/LumexTestSubprocess.hpp"

namespace
{
using namespace contracts_test;

int
checked_in_noexcept (int value) LUMEX_NOEXCEPT
{
  LUMEX_CONTRACT_ASSERT_OBSERVE (value >= 0);
  return value + 1;
}

TEST_F (fixture, AHoldingAssertionLeavesNoexceptAlone)
{
  EXPECT_TRUE (LUMEX_NOEXCEPT_IF (checked_in_noexcept (1)));
  EXPECT_EQ (checked_in_noexcept (1), 2);
}

TEST_F (fixture, AThrowingHandlerInANoexceptFunctionTerminates)
{
  if (!lumex_test::subprocess_available ())
    GTEST_SKIP () << "no fork () on this platform";
  lumex_test::ChildResult const result = lumex_test::run_in_child (
      []
        {
          contracts::set_violation_handler (&throwing_handler);
          return checked_in_noexcept (-1);
        });
  EXPECT_EQ (result.end, lumex_test::ChildEnd::signaled)
      << lumex_test::describe_child (result);
}

TEST_F (fixture, TheSameHandlerDoesNotTerminateInAFunctionThatMayThrow)
{
  contracts::set_violation_handler (&throwing_handler);
  auto may_throw = [] (int value)
    {
      LUMEX_CONTRACT_ASSERT_OBSERVE (value >= 0);
      return value;
    };
  EXPECT_THROW (may_throw (-1), violation_error);
  EXPECT_EQ (may_throw (1), 1);
}
} // namespace
