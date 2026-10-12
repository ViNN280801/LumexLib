// The same with NDEBUG explicitly undefined: the semantic is again the one
// that was chosen, so debug and release builds report alike.

#undef NDEBUG
#define LUMEX_CONTRACTS_SEMANTIC observe

#include <gtest/gtest.h>

#include "lumex/core/contracts/LumexContracts"
#include "lumex/tests/core/contracts/LumexContractsTestSupport.hpp"

namespace
{
using namespace contracts_test;

TEST_F (fixture, WithoutNdebugTheSameSemanticApplies)
{
  contracts::set_violation_handler (&recording_handler);
  LUMEX_CONTRACT_ASSERT (false);
  EXPECT_EQ (record ().calls, 1);
  EXPECT_EQ (LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC, 2);
}
} // namespace
