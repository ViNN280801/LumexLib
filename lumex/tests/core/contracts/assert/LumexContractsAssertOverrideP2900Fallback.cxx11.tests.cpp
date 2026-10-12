// LUMEX_CONTRACT_ASSERT with the semantic P2900Fallback, chosen by defining
// the macro LUMEX_CONTRACTS_SEMANTIC before the first include of the header.
// The definition wins over the semantic the build passes (the test targets
// link lumex::contracts, which defines LUMEX_CONTRACTS_BUILD_SEMANTIC).

#define LUMEX_CONTRACTS_SEMANTIC p2900

#include <gtest/gtest.h>

#include "lumex/core/contracts/LumexContracts"
#include "lumex/tests/core/contracts/LumexContractsTestSupport.hpp"

namespace
{
using namespace contracts_test;

static_assert (LUMEX_CONTRACTS_REQUESTED_SEMANTIC == 5,
               "the translation unit asked for p2900");
static_assert (LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC
                   == (LUMEX_CONTRACTS_HAS_NATIVE ? 5 : 3),
               "the semantic in force");

class ContractsAssertP2900Fallback : public fixture
{
protected:
  void
  SetUp () override
  {
    fixture::SetUp ();
    contracts::set_violation_handler (&recording_handler);
  }
};

TEST_F (ContractsAssertP2900Fallback, HoldingPredicateIsQuiet)
{
  probe holds{ 0 };
  LUMEX_CONTRACT_ASSERT (holds (true));
#if !LUMEX_CONTRACTS_HAS_NATIVE
  // With the compiler's own contract_assert the number of evaluations is
  // unspecified ([basic.contract.eval]).
  EXPECT_EQ (holds.evaluations, 1);
#endif
  EXPECT_EQ (record ().calls, 0);
}

// No installed compiler implements P2900 (contract_assert): p2900 falls back
// to enforce there. A compiler that does have it takes its own keyword.
TEST_F (ContractsAssertP2900Fallback, WithoutNativeContractsItIsEnforce)
{
#if LUMEX_CONTRACTS_HAS_NATIVE
  GTEST_SKIP () << "the compiler has contract_assert: its own flags decide";
#else
  contracts::set_violation_handler (&throwing_handler);
  probe holds{ 0 };
  EXPECT_THROW (LUMEX_CONTRACT_ASSERT (holds (false)), violation_error);
  EXPECT_EQ (holds.evaluations, 1);
  EXPECT_EQ (record ().semantic, contracts::evaluation_semantic::enforce);
#endif
}
} // namespace
