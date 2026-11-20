// LumexFunctionalLiteral.cxx11.tests.cpp
//
// identity and less are literal types at every standard (todo 108): constexpr
// objects and trivial destructors; their constructors from the standard
// function objects (C++14 and C++20) are LUMEX_CONSTEXPR_CTOR.
#include <functional>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/utility/functional/LumexFunctional.hpp"

namespace
{
using lumex::core::utility::functional::identity;
using lumex::core::utility::functional::less;

constexpr identity kIdentity{};
constexpr less kLess{};

static_assert (std::is_trivially_destructible<identity>::value, "identity");
static_assert (std::is_trivially_destructible<less>::value, "less");

#if LUMEX_HAS_STD_TRANSPARENT_OPERATORS
constexpr less kLessFromStd (std::less<void>{});
#endif
} // namespace

TEST (LumexFunctionalLiteralTest, ConstantObjects_AgreeWithTheRunTimeOnes)
{
  EXPECT_TRUE (kLess (1, 2));
  EXPECT_EQ (kIdentity (7), 7);
#if LUMEX_HAS_STD_TRANSPARENT_OPERATORS
  EXPECT_FALSE (kLessFromStd (2, 1));
#endif
}
