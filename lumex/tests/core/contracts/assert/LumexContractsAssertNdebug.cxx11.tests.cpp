// NDEBUG has no effect on a contract assertion (it is not assert): with it
// defined the semantic is still the one that was chosen.

#define NDEBUG 1
#define LUMEX_CONTRACTS_SEMANTIC observe

#include <cassert>

#include <gtest/gtest.h>

#include "lumex/core/contracts/LumexContracts"
#include "lumex/tests/core/contracts/LumexContractsTestSupport.hpp"

namespace
{
using namespace contracts_test;

TEST_F (fixture, NdebugDoesNotSwitchTheCheckOff)
{
  contracts::set_violation_handler (&recording_handler);
  assert (false); // does nothing with NDEBUG
  LUMEX_CONTRACT_ASSERT (false);
  EXPECT_EQ (record ().calls, 1);
  LUMEX_CONTRACT_ASSERT_ENFORCE (true);
  EXPECT_EQ (LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC, 2);
}
} // namespace
