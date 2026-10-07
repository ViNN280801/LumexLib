// LumexTypeTraits.cxx17.tests.cpp
// is_optional on std::optional (C++17). The suites from C++17 up compile
// this file together with LumexTypeTraits.cxx11.tests.cpp.
#include <optional>

#include <gtest/gtest.h>

#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

using namespace lumex::core::utility::traits::value;

TEST (LumexTypeTraitsTest, GivenStdOptional_WhenIsOptional_ThenTrue)
{
  LUMEX_STATIC_ASSERT_MSG ((is_optional<std::optional<int>>::value),
                           "std::optional<int> must be recognized");
  EXPECT_TRUE (is_optional_v<std::optional<int>>);
  EXPECT_TRUE (is_optional_v<std::optional<int> const>);
  EXPECT_FALSE (is_optional_v<std::optional<int> *>);
}
