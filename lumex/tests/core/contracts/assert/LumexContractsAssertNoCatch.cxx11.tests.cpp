// By default an exception that leaves the predicate is not a violation: it
// propagates as any exception would, and no handler is called.

#include <stdexcept>

#include <gtest/gtest.h>

#include "lumex/core/contracts/LumexContracts"
#include "lumex/tests/core/contracts/LumexContractsTestSupport.hpp"

#if LUMEX_CONTRACTS_CATCH_EXCEPTIONS
#error "exceptions are caught only on request"
#endif

namespace
{
using namespace contracts_test;

bool
always_throws ()
{
  throw std::runtime_error ("predicate failed");
}

TEST_F (fixture, AnExceptionOfThePredicatePropagates)
{
  contracts::set_violation_handler (&recording_handler);
  EXPECT_THROW (LUMEX_CONTRACT_ASSERT_OBSERVE (always_throws ()),
                std::runtime_error);
  EXPECT_THROW (LUMEX_CONTRACT_ASSERT_ENFORCE (always_throws ()),
                std::runtime_error);
  EXPECT_THROW (LUMEX_CONTRACT_ASSERT_QUICK_ENFORCE (always_throws ()),
                std::runtime_error);
  EXPECT_EQ (record ().calls, 0);
}

TEST_F (fixture, IgnoreDoesNotEvaluateAThrowingPredicate)
{
  EXPECT_NO_THROW (LUMEX_CONTRACT_ASSERT_IGNORE (always_throws ()));
}
} // namespace
