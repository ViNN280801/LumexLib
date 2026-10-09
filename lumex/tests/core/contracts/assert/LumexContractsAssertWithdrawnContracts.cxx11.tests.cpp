// __cpp_contracts of 201906L is the contracts of the C++20 draft (GCC with
// -fcontracts): they have no contract_assert. That value must not switch
// LUMEX_CONTRACT_ASSERT to the keyword, even for the semantic p2900.

#define __cpp_contracts 201906L
#define LUMEX_CONTRACTS_SEMANTIC p2900

#include <gtest/gtest.h>

#define contract_assert(...)                                                  \
  static_assert (false, "the keyword must not be used")

#include "lumex/core/contracts/LumexContracts"
#include "lumex/tests/core/contracts/LumexContractsTestSupport.hpp"

namespace
{
using namespace contracts_test;

static_assert (LUMEX_CONTRACTS_HAS_NATIVE == 0, "201906L is not P2900");
static_assert (LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC == 3,
               "p2900 falls back to enforce");

TEST_F (fixture, WithdrawnContractsDoNotCount)
{
  contracts::set_violation_handler (&throwing_handler);
  EXPECT_THROW (LUMEX_CONTRACT_ASSERT (false), violation_error);
  EXPECT_EQ (record ().semantic, contracts::evaluation_semantic::enforce);
  contracts::set_violation_handler (nullptr);
}
} // namespace
