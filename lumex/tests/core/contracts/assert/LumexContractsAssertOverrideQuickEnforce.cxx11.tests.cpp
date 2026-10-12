// LUMEX_CONTRACT_ASSERT with the semantic QuickEnforce, chosen by defining the
// macro LUMEX_CONTRACTS_SEMANTIC before the first include of the header. The
// definition wins over the semantic the build passes (the test targets link
// lumex::contracts, which defines LUMEX_CONTRACTS_BUILD_SEMANTIC).

#define LUMEX_CONTRACTS_SEMANTIC quick_enforce

#include <cstdlib>

#include <gtest/gtest.h>

#include "lumex/core/contracts/LumexContracts"
#include "lumex/tests/core/contracts/LumexContractsTestSupport.hpp"
#include "lumex/tests/support/LumexTestSubprocess.hpp"

namespace
{
using namespace contracts_test;

static_assert (LUMEX_CONTRACTS_REQUESTED_SEMANTIC == 4,
               "the translation unit asked for quick_enforce");
static_assert (LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC == 4,
               "the semantic in force");

class ContractsAssertQuickEnforce : public fixture
{
protected:
  void
  SetUp () override
  {
    fixture::SetUp ();
    contracts::set_violation_handler (&recording_handler);
  }
};

TEST_F (ContractsAssertQuickEnforce, HoldingPredicateIsQuiet)
{
  probe holds{ 0 };
  LUMEX_CONTRACT_ASSERT (holds (true));
  EXPECT_EQ (holds.evaluations, 1);
  EXPECT_EQ (record ().calls, 0);
}

TEST_F (ContractsAssertQuickEnforce, FalsePredicateTrapsWithoutTheHandler)
{
  if (!lumex_test::subprocess_available ())
    GTEST_SKIP () << "no fork () on this platform";
  lumex_test::ChildResult const result = lumex_test::run_in_child (
      []
        {
          contracts::set_violation_handler (
              +[] (contracts::contract_violation const &)
                { std::_Exit (77); });
          LUMEX_CONTRACT_ASSERT (false);
          return 0;
        });
  EXPECT_EQ (result.end, lumex_test::ChildEnd::signaled)
      << lumex_test::describe_child (result);
}
} // namespace
