// LumexIndexSequenceLiteral.cxx11.tests.cpp
//
// integer_sequence is a literal type at every standard (todo 108): a constexpr
// object of it, a trivial destructor, and (C++14) the constexpr constructor
// from the standard integer_sequence of the same list.
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/utility/sequence/LumexIndexSequence.hpp"

namespace
{
using lumex::core::utility::sequence::integer_sequence;

constexpr integer_sequence<int, 1, 2, 3> kSequence{};

static_assert (std::is_trivially_destructible<integer_sequence<int, 1>>::value,
               "integer_sequence");
static_assert (integer_sequence<int, 1, 2, 3>::size () == 3, "size");

#if __cplusplus >= 201402L
constexpr integer_sequence<int, 1, 2, 3>
    kFromStd (std::integer_sequence<int, 1, 2, 3>{});
#endif
} // namespace

TEST (LumexIndexSequenceLiteralTest, ConstantObject_HasTheListSize)
{
  EXPECT_EQ (kSequence.size (), 3u);
#if __cplusplus >= 201402L
  EXPECT_EQ (kFromStd.size (), 3u);
#endif
}
