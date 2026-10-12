// With the native contract_assert available, a semantic other than p2900 keeps
// the emulation: the choice of the build is honoured, and the library handler
// is the one that is called.

#define __cpp_contracts 202606L
#define LUMEX_CONTRACTS_SEMANTIC observe

#include <gtest/gtest.h>

#define contract_assert(...)                                                  \
  static_assert (false, "the keyword must not be used")

#include "lumex/core/contracts/LumexContracts"
#include "lumex/tests/core/contracts/LumexContractsTestSupport.hpp"

namespace
{
using namespace contracts_test;

static_assert (LUMEX_CONTRACTS_HAS_NATIVE == 1, "202606L counts as P2900");
static_assert (LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC == 2,
               "observe stays observe");

TEST_F (fixture, ObserveIsEmulatedEvenWithNativeContracts)
{
  contracts::set_violation_handler (&recording_handler);
  LUMEX_CONTRACT_ASSERT (false);
  EXPECT_EQ (record ().calls, 1);
  EXPECT_EQ (record ().semantic, contracts::evaluation_semantic::observe);
}
} // namespace
