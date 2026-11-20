// LumexSafeNumericComparatorLiteral.cxx11.tests.cpp
//
// safe_comparator<T, Atomic> and literal_zero_t are literal types at every
// standard of the build (todo 108): a constexpr object of each and a trivial
// destructor. The default, value and copy constructors of the plain
// comparator are constexpr from C++11; its move constructor and both
// assignments from C++14. The atomic comparator is built by its value
// constructor in a constant expression (std::atomic has a constexpr
// constructor) and is constant-initialized at C++20 (`constinit`), so a global
// has no dynamic initialization; its copy, move and assignments read and write
// the atomic and are not constant expressions. A static_assert here stops the
// build when a constructor loses its constexpr.
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/utility/numeric/LumexOrdering.hpp"
#include "lumex/core/utility/numeric/LumexSafeNumericComparator.hpp"

namespace
{
using lumex::core::utility::numeric::safe_comparator;
using lumex::core::utility::numeric::Detail::literal_zero_t;

constexpr literal_zero_t kZero (static_cast<literal_zero_t *> (nullptr));

constexpr safe_comparator<int> kDefault;
constexpr safe_comparator<int> kValue (5);
constexpr safe_comparator<int> kCopy (kValue);
constexpr safe_comparator<int, true> kAtomicValue (7);

static_assert (std::is_trivially_destructible<literal_zero_t>::value,
               "literal_zero_t");
static_assert (std::is_trivially_destructible<safe_comparator<int>>::value,
               "safe_comparator<int>");
static_assert (
    std::is_trivially_destructible<safe_comparator<int, true>>::value,
    "safe_comparator<int, true>");

#if __cplusplus >= 201402L
// The move constructor and the assignments have a body: constexpr from C++14.
constexpr bool
moves_and_assigns ()
{
  safe_comparator<int> first (4);
  safe_comparator<int> second (std::move (first));
  safe_comparator<int> third;
  third = second;
  second = std::move (third);
  safe_comparator<int> fourth (std::move (second));
  return true;
}
static_assert (moves_and_assigns (),
               "safe_comparator: move constructor and both assignments are "
               "constexpr from C++14");
#endif

#if __cplusplus >= 202002L && defined(__cpp_constinit)
// A global atomic comparator is constant-initialized: no static
// initialization order problem, no guard.
constinit safe_comparator<int, true> g_atomic_global (11);
constinit safe_comparator<int, true> g_atomic_default;
#endif
} // namespace

TEST (LumexSafeNumericComparatorLiteralTest,
      ConstantObjects_AgreeWithTheRunTimeOnes)
{
  EXPECT_TRUE (kValue == 5);
  EXPECT_TRUE (kCopy == 5);
  EXPECT_TRUE (kDefault == 0);
  EXPECT_TRUE (kAtomicValue == 7);
  static_cast<void> (kZero);
#if __cplusplus >= 202002L && defined(__cpp_constinit)
  EXPECT_TRUE (g_atomic_global == 11);
  EXPECT_TRUE (g_atomic_default == 0);
#endif
}
