// LumexCircularBufferLiteral: CircularBufferLiteral.cxx11.tests.cpp
//
// The iterators of circular_buffer are literal types at every standard of the
// build (todo 108): trivially destructible, with constructors that are
// constexpr from C++11 (an empty body, two scalar members). A constexpr
// iterator and the conversion from iterator to const_iterator exist; a
// static_assert here stops the build when one of the constructors loses its
// constexpr. The private constructor from a buffer and an index takes the
// macro for consistency; it is only called at run time (begin and end of a
// buffer, which is not a literal type: the allocator and the destructor), so
// no test can pin it. circular_buffer itself
// cannot be literal: its destructor releases storage and std::allocator is
// constexpr only from C++20.
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/circular_buffer/CircularBuffer"

namespace
{
using buffer_type = lumex::core::circular_buffer::circular_buffer<int>;
using iterator = buffer_type::iterator;
using const_iterator = buffer_type::const_iterator;

constexpr iterator k_iterator;
constexpr const_iterator k_const_iterator;
constexpr const_iterator k_converted (k_iterator);
constexpr const_iterator k_copy (k_const_iterator);

static_assert (std::is_trivially_destructible<iterator>::value,
               "iterator is trivially destructible");
static_assert (std::is_trivially_destructible<const_iterator>::value,
               "const_iterator is trivially destructible");
static_assert (std::is_trivially_copyable<iterator>::value,
               "iterator stays trivially copyable");
static_assert (!std::is_constructible<iterator, const_iterator>::value,
               "the conversion goes from iterator to const_iterator only");
} // namespace

TEST (LumexCircularBufferLiteralTest,
      GivenConstantIterators_WhenCompared_ThenTheyAreSingularAndEqual)
{
  EXPECT_TRUE (k_converted == k_const_iterator);
  EXPECT_TRUE (k_copy == k_const_iterator);
  EXPECT_TRUE (k_iterator == k_const_iterator);
}
