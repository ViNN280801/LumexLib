// LumexOptionalConstexpr.cxx11.tests.cpp
//
// optional<T> is a literal type for a trivially destructible T: constexpr
// construction and const observers from C++11, the relaxed constexpr
// operations (reset, assignment of nullopt, non-const access) from C++14, and
// the operations that change the active member of the union (emplace, value
// assignment, swap, non-trivial members) from C++20. Each static_assert names
// the mutation of the pre-registered list it catches (M13: a constexpr
// constructor lost, M14: a constexpr observer lost, M31: the constexpr
// std::optional conversion lost).
#include <string>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/optional/opt/LumexOptional.hpp"

namespace
{
using lumex::core::optional::opt::in_place;
using lumex::core::optional::opt::make_optional;
using lumex::core::optional::opt::nullopt;
using lumex::core::optional::opt::optional;

struct Pair
{
  int first;
  double second;
  constexpr Pair (int a, double b) : first (a), second (b) {}
};

constexpr optional<int> empty_int;
constexpr optional<int> null_int (nullopt);
constexpr optional<int> three (3);
constexpr optional<int> copied = three;
constexpr optional<Pair> paired (in_place, 4, 2.5);
constexpr optional<long> widened = 5; // converting constructor from a value

static_assert (!empty_int.has_value () && !null_int.has_value (),
               "M13: constexpr default and nullopt constructors");
static_assert (three.has_value () && *three == 3,
               "M13/M14: constexpr value constructor and operator*");
static_assert (copied.has_value () && *copied == 3,
               "constexpr trivial copy constructor");
static_assert (paired->first == 4 && paired.value ().second == 2.5,
               "M13/M14: in-place constructor, operator->, value ()");
static_assert (three.value () == 3 && three.value_or (9) == 3
                   && empty_int.value_or (9) == 9,
               "M14: constexpr value () and value_or");
static_assert (static_cast<bool> (three) && !static_cast<bool> (empty_int),
               "M14: constexpr explicit operator bool");
static_assert (widened.has_value () && *widened == 5L,
               "M13: constexpr converting constructor from a value");
static_assert (three == 3 && 3 == three && three != empty_int
                   && three == copied && empty_int == nullopt
                   && three != nullopt && empty_int < three && three > nullopt
                   && three <= copied,
               "M14: constexpr comparisons");
static_assert (make_optional (7).has_value (),
               "constexpr make_optional (value)");
static_assert (
    std::is_same<decltype (make_optional (7)), optional<int>>::value, "");

constexpr int
read_both (optional<int> const &first, optional<int> const &second)
{
  return first.value_or (0) + second.value_or (0);
}
static_assert (read_both (three, empty_int) == 3, "");
} // namespace

// A type with a non-trivial destructor is not a literal type before C++20;
// its optional is then not a constant: the constexpr members stay templates
// that are simply not constant expressions.
TEST (LumexOptionalConstexprTest, GivenNonLiteralT_WhenRuntimeUse_ThenWorks)
{
  optional<std::string> value (std::string ("runtime"));
  EXPECT_EQ (value.value (), "runtime");
  EXPECT_EQ (value.value_or ("x"), "runtime");
}

TEST (LumexOptionalConstexprTest, GivenConstants_WhenRead_ThenAsDeclared)
{
  EXPECT_EQ (*three, 3);
  EXPECT_EQ (paired->first, 4);
  EXPECT_EQ (*widened, 5L);
  EXPECT_FALSE (empty_int.has_value ());
}

#if __cplusplus >= 201402L
// ---- C++14: reset, nullopt assignment and non-const access are constexpr
// for a trivially destructible T ----

constexpr bool
reset_works ()
{
  optional<int> value (3);
  value.reset ();
  return !value.has_value ();
}
static_assert (reset_works (), "M14: constexpr reset (C++14, trivial T)");

constexpr bool
nullopt_assign_works ()
{
  optional<int> value (3);
  value = nullopt;
  return !value.has_value ();
}
static_assert (nullopt_assign_works (), "M14: constexpr operator= (nullopt)");

constexpr int
write_through_reference ()
{
  optional<int> value (3);
  *value = 4;
  value.value () += 1;
  return *value;
}
static_assert (write_through_reference () == 5,
               "M14: constexpr non-const operator* and value ()");

constexpr int
move_out ()
{
  optional<int> value (6);
  return std::move (value).value_or (0) + *std::move (value);
}
static_assert (move_out () == 12, "M14: constexpr rvalue observers");

constexpr int
trivial_copy_and_move ()
{
  optional<int> first (1);
  optional<int> second = first;
  optional<int> third = std::move (second);
  return *third;
}
static_assert (trivial_copy_and_move () == 1, "constexpr trivial copy/move");
#endif

#if __cplusplus >= 202002L && defined(__cpp_lib_constexpr_dynamic_alloc)      \
    && __cpp_lib_constexpr_dynamic_alloc >= 201907L
// ---- C++20: the operations that change the active member ----

constexpr int
emplace_works ()
{
  optional<Pair> value;
  value.emplace (2, 3.0);
  value.emplace (5, 1.0);
  return value->first;
}
static_assert (emplace_works () == 5, "M14: constexpr emplace (C++20)");

constexpr int
assign_value_works ()
{
  optional<Pair> value;
  value = Pair (1, 1.0);
  value = Pair (2, 1.0);
  return value->first;
}
static_assert (assign_value_works () == 2, "constexpr value assignment");

constexpr int
copy_assign_works ()
{
  optional<int> first (1);
  optional<int> second;
  second = first;
  first = optional<int> (4);
  return *first + *second;
}
static_assert (copy_assign_works () == 5, "constexpr assignment");

constexpr int
swap_works ()
{
  optional<int> first (1);
  optional<int> second;
  first.swap (second);
  return second.has_value () && !first.has_value () ? *second : -1;
}
static_assert (swap_works () == 1, "constexpr swap (C++20)");

constexpr long
converting_works ()
{
  optional<int> source (3);
  optional<long> target (source);
  target = optional<int> (4);
  return *target;
}
static_assert (converting_works () == 4,
               "constexpr converting constructor and assignment (C++20)");

constexpr int
emplace_const_works ()
{
  optional<const int> value;
  value.emplace (3);
  value.emplace (4);
  return *value;
}
static_assert (emplace_const_works () == 4,
               "F4: constexpr emplace of an optional<const int> (C++20)");
#endif

TEST (LumexOptionalConstexprTest,
      GivenStandard_WhenCompiled_ThenStaticChecksHold)
{
  SUCCEED ();
}
