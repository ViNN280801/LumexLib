// LUMEX_CONTRACT_ASSERT with the semantic Observe, chosen by defining the
// macro LUMEX_CONTRACTS_SEMANTIC before the first include of the header. The
// definition wins over the semantic the build passes (the test targets link
// lumex::contracts, which defines LUMEX_CONTRACTS_BUILD_SEMANTIC).

#define LUMEX_CONTRACTS_SEMANTIC observe

#include <gtest/gtest.h>

#include "lumex/core/contracts/LumexContracts"
#include "lumex/tests/core/contracts/LumexContractsTestSupport.hpp"

namespace
{
using namespace contracts_test;

static_assert (LUMEX_CONTRACTS_REQUESTED_SEMANTIC == 2,
               "the translation unit asked for observe");
static_assert (LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC == 2,
               "the semantic in force");

class ContractsAssertObserve : public fixture
{
protected:
  void
  SetUp () override
  {
    fixture::SetUp ();
    contracts::set_violation_handler (&recording_handler);
  }
};

TEST_F (ContractsAssertObserve, HoldingPredicateIsQuiet)
{
  probe holds{ 0 };
  LUMEX_CONTRACT_ASSERT (holds (true));
  EXPECT_EQ (holds.evaluations, 1);
  EXPECT_EQ (record ().calls, 0);
}

TEST_F (ContractsAssertObserve,
        FalsePredicateIsReportedOnceAndTheProgramContinues)
{
  probe holds{ 0 };
  bool reached = false;
  LUMEX_CONTRACT_ASSERT (holds (false));
  reached = true;
  EXPECT_TRUE (reached);
  EXPECT_EQ (holds.evaluations, 1);
  ASSERT_EQ (record ().calls, 1);
  EXPECT_EQ (record ().semantic, contracts::evaluation_semantic::observe);
  EXPECT_EQ (record ().comment, "holds (false)");
}
} // namespace
