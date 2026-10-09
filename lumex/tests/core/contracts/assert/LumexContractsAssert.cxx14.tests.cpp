// Statements in a constexpr function (C++14): the assertions sit in the body
// of loops and branches.

#include <gtest/gtest.h>

#include "lumex/core/contracts/LumexContracts"
#include "lumex/tests/core/contracts/LumexContractsTestSupport.hpp"

namespace
{
using namespace contracts_test;

constexpr int
sum_to (int count)
{
  LUMEX_CONTRACT_ASSERT_ENFORCE (count >= 0);
  int total = 0;
  for (int index = 1; index <= count; ++index)
    {
      LUMEX_CONTRACT_ASSERT_ENFORCE (total >= 0);
      total += index;
    }
  LUMEX_CONTRACT_ASSERT_OBSERVE (total == count * (count + 1) / 2);
  return total;
}

static_assert (sum_to (0) == 0, "empty sum");
static_assert (sum_to (10) == 55, "assertions in a loop");

constexpr int
ignored_sum (int count)
{
  LUMEX_CONTRACT_ASSERT_IGNORE (count >= 0);
  return count;
}

static_assert (ignored_sum (-1) == -1, "an ignored failing assertion");

TEST_F (fixture, AssertionsInAConstexprBodyWorkAtRunTime)
{
  contracts::set_violation_handler (&throwing_handler);
  EXPECT_EQ (sum_to (4), 10);
  EXPECT_THROW (sum_to (-1), violation_error);
  EXPECT_EQ (record ().comment, "count >= 0");
}
} // namespace
