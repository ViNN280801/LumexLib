// The swap of unexpected<E> that is a hidden friend is constexpr from C++14 (a
// constexpr function may change an object there, N3652) when the swap of the
// error is: an error type with a constexpr swap of its own is exchanged in a
// constant expression, and the comparisons stay constant expressions. The
// source is compiled into every suite from C++14.

#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"

namespace
{
using lumex::core::expected::error::unexpected;

/// A literal type with a constexpr swap of its own (`std::swap` is constexpr
/// only from C++20).
struct box_t
{
  int value;

  constexpr explicit box_t (int v) : value (v) {}
};

constexpr bool
operator== (box_t const &lhs, box_t const &rhs)
{
  return lhs.value == rhs.value;
}

constexpr void
swap (box_t &lhs, box_t &rhs) noexcept
{
  int const kept = lhs.value;
  lhs.value = rhs.value;
  rhs.value = kept;
}

constexpr int
swapped_by_adl ()
{
  unexpected<box_t> first (1);
  unexpected<box_t> second (2);

  swap (first, second);
  return first.error ().value * 10 + second.error ().value;
}

constexpr int
swapped_by_member ()
{
  unexpected<box_t> first (3);
  unexpected<box_t> second (4);

  first.swap (second);
  return first.error ().value * 10 + second.error ().value;
}

static_assert (swapped_by_adl () == 21, "the friend swap in a constant "
                                        "expression");
static_assert (swapped_by_member () == 43, "the member swap in a constant "
                                           "expression");
static_assert (unexpected<box_t> (box_t (5)) == unexpected<box_t> (box_t (5)),
               "== of the friend in a constant expression");
static_assert (unexpected<box_t> (box_t (5)) != unexpected<box_t> (box_t (6)),
               "!= of the friend in a constant expression");

TEST (UnexpectedHiddenFriendsTest,
      Swap_ConstexprErrorSwap_RunsInAConstantExpression)
{
  EXPECT_EQ (swapped_by_adl (), 21);
  EXPECT_EQ (swapped_by_member (), 43);
}

} // namespace
