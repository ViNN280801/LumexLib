// LumexIteratorRangeLiteral.cxx11.tests.cpp
//
// iterator_range<Iterator> is a literal type for a pointer or another literal
// iterator at every standard (todo 108): its constructor is
// LUMEX_CONSTEXPR_CTOR and the destructor is trivial. So is
// element_before_value, the predicate adapter that exists before C++20.
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/functional/LumexFunctional.hpp"
#include "lumex/core/utility/ranges/LumexIteratorRange.hpp"
#include "lumex/core/utility/ranges/LumexRanges.hpp"

namespace
{
using lumex::core::utility::ranges::iterator_range;

constexpr int kData[] = { 1, 2, 3 };
constexpr iterator_range<int const *> kRange (kData, kData + 3);

static_assert (
    std::is_trivially_destructible<iterator_range<int const *>>::value,
    "iterator_range");

#if !LUMEX_HAS_STD_RANGES
using lumex::core::utility::functional::identity;
using lumex::core::utility::functional::less;
using lumex::core::utility::ranges::Algorithm::Detail::element_before_value;

constexpr less kLess{};
constexpr identity kIdentity{};
constexpr element_before_value<less, identity> kBefore (kLess, kIdentity);
static_assert (std::is_trivially_destructible<
                   element_before_value<less, identity>>::value,
               "element_before_value");
#endif
} // namespace

TEST (LumexIteratorRangeLiteralTest, ConstantRange_AgreeWithTheRunTimeOne)
{
  EXPECT_EQ (kRange.size (), 3u);
  EXPECT_EQ (*kRange.begin (), 1);
  EXPECT_FALSE (kRange.empty ());
#if !LUMEX_HAS_STD_RANGES
  static_cast<void> (kBefore);
#endif
}
