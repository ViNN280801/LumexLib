// LUMEX_CONTRACT_ASSERT with the semantic Ignore, chosen by defining the
// macro LUMEX_CONTRACTS_SEMANTIC before the first include of the header. The
// definition wins over the semantic the build passes (the test targets link
// lumex::contracts, which defines LUMEX_CONTRACTS_BUILD_SEMANTIC).

#define LUMEX_CONTRACTS_SEMANTIC ignore

#include <gtest/gtest.h>

#include "lumex/core/contracts/LumexContracts"
#include "lumex/tests/core/contracts/LumexContractsTestSupport.hpp"

namespace
{
using namespace contracts_test;

static_assert (LUMEX_CONTRACTS_REQUESTED_SEMANTIC == 1,
               "the translation unit asked for ignore");
static_assert (LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC == 1,
               "the semantic in force");

class ContractsAssertIgnore : public fixture
{
protected:
  void
  SetUp () override
  {
    fixture::SetUp ();
    contracts::set_violation_handler (&recording_handler);
  }
};

TEST_F (ContractsAssertIgnore, HoldingPredicateIsQuiet)
{
  probe holds{ 0 };
  LUMEX_CONTRACT_ASSERT (holds (true));
  EXPECT_EQ (holds.evaluations, 0);
  EXPECT_EQ (record ().calls, 0);
}

TEST_F (ContractsAssertIgnore, FalsePredicateIsNeverEvaluatedNorReported)
{
  probe holds{ 0 };
  bool reached = false;
  LUMEX_CONTRACT_ASSERT (holds (false));
  reached = true;
  EXPECT_TRUE (reached);
  EXPECT_EQ (holds.evaluations, 0);
  EXPECT_EQ (record ().calls, 0);
}

TEST_F (ContractsAssertIgnore, ThePredicateIsStillCompiled)
{
  // Names in an ignored predicate must exist and have the right types: this
  // line compiles only because `holds` is callable with a bool.
  probe holds{ 0 };
  LUMEX_CONTRACT_ASSERT (holds (false) && std::is_same<int, int>::value);
  EXPECT_EQ (holds.evaluations, 0);
}
} // namespace
