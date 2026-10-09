// LUMEX_CONTRACTS_CATCH_EXCEPTIONS: an exception that leaves the predicate is
// a violation with the detection mode evaluation_exception
// ([basic.contract.eval]), reported from inside the handler of that exception,
// so a handler may rethrow it.

#define LUMEX_CONTRACTS_CATCH_EXCEPTIONS 1

#include <stdexcept>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/contracts/LumexContracts"
#include "lumex/tests/core/contracts/LumexContractsTestSupport.hpp"

namespace
{
using namespace contracts_test;

bool
throws_on_demand (bool should_throw, bool result)
{
  if (should_throw)
    throw std::runtime_error ("predicate failed");
  return result;
}

class ContractsAssertCatch : public fixture
{
protected:
  void
  SetUp () override
  {
    fixture::SetUp ();
    contracts::set_violation_handler (&recording_handler);
  }
};

void
rethrowing_handler (contracts::contract_violation const &violation)
{
  recording_handler (violation);
  throw;
}

TEST_F (ContractsAssertCatch, ObserveReportsAnExceptionAndContinues)
{
  bool reached = false;
  LUMEX_CONTRACT_ASSERT_OBSERVE (throws_on_demand (true, true));
  reached = true;
  EXPECT_TRUE (reached);
  ASSERT_EQ (record ().calls, 1);
  EXPECT_EQ (record ().mode, contracts::detection_mode::evaluation_exception);
  EXPECT_EQ (record ().comment, "throws_on_demand (true, true)");
}

TEST_F (ContractsAssertCatch, AFalsePredicateIsStillPredicateFalse)
{
  LUMEX_CONTRACT_ASSERT_OBSERVE (throws_on_demand (false, false));
  ASSERT_EQ (record ().calls, 1);
  EXPECT_EQ (record ().mode, contracts::detection_mode::predicate_false);
}

TEST_F (ContractsAssertCatch, AHoldingPredicateIsQuiet)
{
  probe holds{ 0 };
  LUMEX_CONTRACT_ASSERT_OBSERVE (holds (true));
  LUMEX_CONTRACT_ASSERT_ENFORCE (holds (true));
  LUMEX_CONTRACT_ASSERT_QUICK_ENFORCE (holds (true));
  EXPECT_EQ (holds.evaluations, 3);
  EXPECT_EQ (record ().calls, 0);
}

TEST_F (ContractsAssertCatch, TheHandlerMayRethrowTheOriginalException)
{
  contracts::set_violation_handler (&rethrowing_handler);
  try
    {
      LUMEX_CONTRACT_ASSERT_ENFORCE (throws_on_demand (true, true));
      FAIL () << "the exception was swallowed";
    }
  catch (std::runtime_error const &error)
    {
      EXPECT_STREQ (error.what (), "predicate failed");
    }
  EXPECT_EQ (record ().calls, 1);
  EXPECT_EQ (record ().mode, contracts::detection_mode::evaluation_exception);
}

TEST_F (ContractsAssertCatch, AHandlerExceptionLeavesTheAssertion)
{
  contracts::set_violation_handler (&throwing_handler);
  EXPECT_THROW (
      LUMEX_CONTRACT_ASSERT_OBSERVE (throws_on_demand (false, false)),
      violation_error);
  EXPECT_THROW (LUMEX_CONTRACT_ASSERT_OBSERVE (throws_on_demand (true, false)),
                violation_error);
  EXPECT_EQ (record ().calls, 2);
}

void
report_from_this_function ()
{
  LUMEX_CONTRACT_ASSERT_OBSERVE (false);
}

TEST_F (ContractsAssertCatch, LocationIsTheEnclosingFunctionNotTheLambda)
{
  report_from_this_function ();
  ASSERT_EQ (record ().calls, 1);
  EXPECT_TRUE (contains (record ().function, "report_from_this_function"))
      << record ().function;
  EXPECT_TRUE (
      ends_with (record ().file, "LumexContractsAssertCatch.cxx11.tests.cpp"));
  EXPECT_GT (record ().line, 0u);
}

TEST_F (ContractsAssertCatch, CommasOfTemplateArgumentsAreKept)
{
  LUMEX_CONTRACT_ASSERT_OBSERVE (std::is_same<int, long>::value);
  EXPECT_EQ (record ().comment, "std::is_same<int, long>::value");
}

TEST_F (ContractsAssertCatch, IgnoreStillDoesNotEvaluate)
{
  probe holds{ 0 };
  LUMEX_CONTRACT_ASSERT_IGNORE (holds (false));
  EXPECT_EQ (holds.evaluations, 0);
}
} // namespace
