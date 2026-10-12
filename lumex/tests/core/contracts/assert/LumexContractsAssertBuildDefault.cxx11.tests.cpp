// No macro in this translation unit: the semantic is the one the build passes
// through the target lumex::contracts (the CMake option
// LUMEX_CONTRACTS_SEMANTIC, ENFORCE by default), and enforce without it.

#include <gtest/gtest.h>

#include "lumex/core/contracts/LumexContracts"
#include "lumex/tests/core/contracts/LumexContractsTestSupport.hpp"

#if defined(LUMEX_CONTRACTS_SEMANTIC)
#error "this file tests the build default: it must not define the semantic"
#endif

namespace
{
using namespace contracts_test;

#define CONTRACTS_TEST_PASTE_(a, b) a##b
#define CONTRACTS_TEST_PASTE(a, b) CONTRACTS_TEST_PASTE_ (a, b)

TEST (ContractsAssertBuildDefault, TheSemanticIsTheOneOfTheBuild)
{
#if defined(LUMEX_CONTRACTS_BUILD_SEMANTIC)
  int const expected = CONTRACTS_TEST_PASTE (LUMEX_CONTRACTS_SEMANTIC_ID_,
                                             LUMEX_CONTRACTS_BUILD_SEMANTIC);
  EXPECT_EQ (LUMEX_CONTRACTS_REQUESTED_SEMANTIC, expected);
#else
  EXPECT_EQ (LUMEX_CONTRACTS_REQUESTED_SEMANTIC, 3);
#endif
}

TEST (ContractsAssertBuildDefault, TheDefaultBuildIsEnforce)
{
#if defined(LUMEX_CONTRACTS_BUILD_SEMANTIC)
  // The test tree is configured with the default option; a tree configured
  // with another one runs the other checks of the semantics table.
  if (LUMEX_CONTRACTS_REQUESTED_SEMANTIC != 3)
    GTEST_SKIP () << "the build chose another semantic";
#endif
  EXPECT_EQ (LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC, 3);
}

TEST_F (fixture, PlainMacroFollowsTheBuild)
{
  contracts::set_violation_handler (&throwing_handler);
  probe holds{ 0 };
  switch (LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC)
    {
    case 1:
      LUMEX_CONTRACT_ASSERT (holds (false));
      EXPECT_EQ (holds.evaluations, 0);
      break;
    case 2:
      LUMEX_CONTRACT_ASSERT (holds (false));
      EXPECT_EQ (holds.evaluations, 1);
      EXPECT_EQ (record ().calls, 1);
      break;
    case 3:
      EXPECT_THROW (LUMEX_CONTRACT_ASSERT (holds (false)), violation_error);
      EXPECT_EQ (holds.evaluations, 1);
      break;
    default:
      GTEST_SKIP ()
          << "quick_enforce or the native keyword: see the death tests";
    }
  contracts::set_violation_handler (nullptr);
}
} // namespace
