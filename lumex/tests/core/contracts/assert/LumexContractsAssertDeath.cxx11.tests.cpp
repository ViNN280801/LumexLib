// The terminating semantics end the process: enforce aborts after the handler
// returned, quick_enforce traps without calling the handler. Each case runs in
// a forked child (lumex/tests/support/LumexTestSubprocess.hpp).

#include <csignal>
#include <cstdlib>

#include <gtest/gtest.h>

#include "lumex/core/contracts/LumexContracts"
#include "lumex/tests/core/contracts/LumexContractsTestSupport.hpp"
#include "lumex/tests/support/LumexTestSubprocess.hpp"

namespace
{
using namespace contracts_test;
using lumex_test::ChildEnd;
using lumex_test::ChildResult;
using lumex_test::run_in_child;

int const k_handler_ran = 77;

void
exiting_handler (contracts::contract_violation const &)
{
  // A distinct exit code proves the handler ran.
  std::_Exit (k_handler_ran);
}

void
returning_handler (contracts::contract_violation const &)
{
}

class ContractsAssertDeath : public fixture
{
protected:
  void
  SetUp () override
  {
    fixture::SetUp ();
    if (!lumex_test::subprocess_available ())
      GTEST_SKIP () << "no fork () on this platform";
  }
};

TEST_F (ContractsAssertDeath, EnforceCallsTheHandlerFirst)
{
  ChildResult const result = run_in_child (
      []
        {
          contracts::set_violation_handler (&exiting_handler);
          LUMEX_CONTRACT_ASSERT_ENFORCE (false);
          return 0;
        });
  EXPECT_EQ (result.end, ChildEnd::failed_exit) << describe_child (result);
  EXPECT_EQ (result.code, k_handler_ran);
}

TEST_F (ContractsAssertDeath, EnforceAbortsWhenTheHandlerReturns)
{
  ChildResult const result = run_in_child (
      []
        {
          contracts::set_violation_handler (&returning_handler);
          LUMEX_CONTRACT_ASSERT_ENFORCE (false);
          return 0;
        });
  EXPECT_EQ (result.end, ChildEnd::signaled) << describe_child (result);
  EXPECT_EQ (result.code, SIGABRT);
}

TEST_F (ContractsAssertDeath, EnforceWithTheDefaultHandlerAborts)
{
  ChildResult const result = run_in_child (
      []
        {
          LUMEX_CONTRACT_ASSERT_ENFORCE (false);
          return 0;
        });
  EXPECT_EQ (result.end, ChildEnd::signaled) << describe_child (result);
  EXPECT_EQ (result.code, SIGABRT);
}

TEST_F (ContractsAssertDeath, EnforceAbortsOnlyAfterTheReportedViolation)
{
  // Two assertions: the first holds, the second fails; the process ends there.
  ChildResult const result = run_in_child (
      []
        {
          contracts::set_violation_handler (&returning_handler);
          LUMEX_CONTRACT_ASSERT_ENFORCE (true);
          LUMEX_CONTRACT_ASSERT_ENFORCE (1 == 2);
          return 0;
        });
  EXPECT_EQ (result.end, ChildEnd::signaled) << describe_child (result);
}

TEST_F (ContractsAssertDeath, QuickEnforceTrapsWithoutTheHandler)
{
  ChildResult const result = run_in_child (
      []
        {
          contracts::set_violation_handler (&exiting_handler);
          LUMEX_CONTRACT_ASSERT_QUICK_ENFORCE (false);
          return 0;
        });
  // The handler would end the child with k_handler_ran; a trap is a signal
  // (SIGILL or SIGTRAP, by compiler and target).
  EXPECT_EQ (result.end, ChildEnd::signaled) << describe_child (result);
  EXPECT_TRUE (result.code == SIGILL || result.code == SIGTRAP
               || result.code == SIGABRT || result.code == SIGSEGV)
      << result.code;
}

TEST_F (ContractsAssertDeath, TrueAssertionsDoNotEndTheProcess)
{
  ChildResult const result = run_in_child (
      []
        {
          LUMEX_CONTRACT_ASSERT_ENFORCE (1 + 1 == 2);
          LUMEX_CONTRACT_ASSERT_QUICK_ENFORCE (true);
          LUMEX_CONTRACT_ASSERT_OBSERVE (true);
          LUMEX_CONTRACT_ASSERT_IGNORE (false);
          return 0;
        });
  EXPECT_EQ (result.end, ChildEnd::clean) << describe_child (result);
}

TEST_F (ContractsAssertDeath, ObserveAndIgnoreDoNotEndTheProcess)
{
  ChildResult const result = run_in_child (
      []
        {
          contracts::set_violation_handler (&returning_handler);
          LUMEX_CONTRACT_ASSERT_OBSERVE (false);
          LUMEX_CONTRACT_ASSERT_IGNORE (false);
          return 0;
        });
  EXPECT_EQ (result.end, ChildEnd::clean) << describe_child (result);
}
} // namespace
