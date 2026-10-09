// A holding assertion is a constant expression: LUMEX_CONTRACT_ASSERT works in
// a constexpr function from C++11 (one return statement, the comma form). A
// failing one in constant evaluation does not compile (negative case in
// cmake.contracts_compile_checks); ignore never evaluates its predicate, so
// even a failing one is fine.

#include <gtest/gtest.h>

#include "lumex/core/contracts/LumexContracts"
#include "lumex/tests/core/contracts/LumexContractsTestSupport.hpp"

namespace
{
using namespace contracts_test;

constexpr int
checked_square (int value)
{
  return (LUMEX_CONTRACT_ASSERT_ENFORCE (value >= 0), value * value);
}

constexpr int
observed_square (int value)
{
  return (LUMEX_CONTRACT_ASSERT_OBSERVE (value >= 0), value * value);
}

constexpr int
quick_square (int value)
{
  return (LUMEX_CONTRACT_ASSERT_QUICK_ENFORCE (value >= 0), value * value);
}

constexpr int
ignored_square (int value)
{
  return (LUMEX_CONTRACT_ASSERT_IGNORE (value >= 0), value * value);
}

constexpr int
plain_square (int value)
{
  return (LUMEX_CONTRACT_ASSERT (value >= 0), value * value);
}

static_assert (checked_square (4) == 16, "enforce in a constant expression");
static_assert (observed_square (4) == 16, "observe in a constant expression");
static_assert (quick_square (4) == 16,
               "quick_enforce in a constant expression");
static_assert (ignored_square (4) == 16, "ignore in a constant expression");
static_assert (ignored_square (-4) == 16, "ignore does not evaluate");
static_assert (plain_square (4) == 16, "the plain macro");

template <int Value> struct square_t
{
  static constexpr int value = checked_square (Value);
};
static_assert (square_t<5>::value == 25, "usable as a template argument");

TEST_F (fixture, ConstexprFunctionsRunAtRunTimeToo)
{
  contracts::set_violation_handler (&recording_handler);
  EXPECT_EQ (checked_square (3), 9);
  EXPECT_EQ (observed_square (-3), 9);
  EXPECT_EQ (record ().calls, 1);
  EXPECT_EQ (ignored_square (-3), 9);
  EXPECT_EQ (record ().calls, 1);
}
} // namespace
