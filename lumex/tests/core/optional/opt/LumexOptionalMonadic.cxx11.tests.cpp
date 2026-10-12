// LumexOptionalMonadic.cxx11.tests.cpp
//
// The C++23 monadic operations and_then, transform and or_else of
// optional<T> ([optional.monadic]) in C++11: return types for the four value
// categories, the constraints (a call that cannot be made, an and_then
// result that is not an optional, a transform result that cannot be held),
// and that transform does not flatten. The comments name the mutation of the
// pre-registered list that each test catches.
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/optional/opt/LumexOptional.hpp"

namespace
{
using lumex::core::optional::opt::in_place;
using lumex::core::optional::opt::in_place_t;
using lumex::core::optional::opt::nullopt;
using lumex::core::optional::opt::nullopt_t;
using lumex::core::optional::opt::optional;

optional<int>
half_if_even (int value)
{
  return value % 2 == 0 ? optional<int> (value / 2) : optional<int> ();
}

int
plus_one (int value)
{
  return value + 1;
}

optional<int>
make_five ()
{
  return optional<int> (5);
}

optional<int>
make_empty ()
{
  return optional<int> ();
}

// Records the value category the function was called with.
struct Categories
{
  int
  operator() (int &) const
  {
    return 1;
  }
  int
  operator() (int const &) const
  {
    return 2;
  }
  int
  operator() (int &&) const
  {
    return 3;
  }
  int
  operator() (int const &&) const
  {
    return 4;
  }
};

struct CategoriesOpt
{
  optional<int>
  operator() (int &) const
  {
    return optional<int> (1);
  }
  optional<int>
  operator() (int const &) const
  {
    return optional<int> (2);
  }
  optional<int>
  operator() (int &&) const
  {
    return optional<int> (3);
  }
  optional<int>
  operator() (int const &&) const
  {
    return optional<int> (4);
  }
};

struct Widget
{
  int value;
  explicit Widget (int v) : value (v) {}
  int
  get () const
  {
    return value;
  }
};

struct Holder
{
  optional<int> inner;
};

// Detection of viable calls (an overload removed by SFINAE gives false).
template <typename Opt, typename Fn, typename = void>
struct can_and_then : std::false_type
{
};
template <typename Opt, typename Fn>
struct can_and_then<Opt, Fn,
                    typename std::enable_if<
                        true, decltype ((void)std::declval<Opt> ().and_then (
                                  std::declval<Fn> ()))>::type>
    : std::true_type
{
};

template <typename Opt, typename Fn, typename = void>
struct can_transform : std::false_type
{
};
template <typename Opt, typename Fn>
struct can_transform<Opt, Fn,
                     typename std::enable_if<
                         true, decltype ((void)std::declval<Opt> ().transform (
                                   std::declval<Fn> ()))>::type>
    : std::true_type
{
};

template <typename Opt, typename Fn, typename = void>
struct can_or_else : std::false_type
{
};
template <typename Opt, typename Fn>
struct can_or_else<Opt, Fn,
                   typename std::enable_if<
                       true, decltype ((void)std::declval<Opt> ().or_else (
                                 std::declval<Fn> ()))>::type> : std::true_type
{
};

using int_fn = int (*) (int);
using opt_fn = optional<int> (*) (int);
using void_fn = void (*) (int);
using string_fn = std::string (*) (int);
using nullary_opt_fn = optional<int> (*) ();
using nullary_int_fn = int (*) ();

constexpr optional<int>
constexpr_half (int value)
{
  return value % 2 == 0 ? optional<int> (value / 2) : optional<int> ();
}

constexpr optional<int>
constexpr_five ()
{
  return optional<int> (5);
}
constexpr int
constexpr_plus_one (int value)
{
  return value + 1;
}
} // namespace

// ---- and_then ----

TEST (LumexOptionalMonadicTest, GivenEngaged_WhenAndThen_ThenResultOfFunction)
{
  optional<int> value (10);
  optional<int> const cvalue (10);
  EXPECT_EQ (*value.and_then (half_if_even), 5);
  EXPECT_EQ (*cvalue.and_then (half_if_even), 5);
  EXPECT_EQ (*optional<int> (10).and_then (half_if_even), 5);
  EXPECT_FALSE (optional<int> (3).and_then (half_if_even).has_value ())
      << "the function can return empty";
}

// M26: the empty case calls the function or returns something engaged.
TEST (LumexOptionalMonadicTest, GivenEmpty_WhenAndThen_ThenEmptyAndNoCall)
{
  int calls = 0;
  optional<int> empty;
  auto result = empty.and_then (
      [&calls] (int v) -> optional<int>
        {
          ++calls;
          return optional<int> (v);
        });
  static_assert (std::is_same<decltype (result), optional<int>>::value, "");
  EXPECT_FALSE (result.has_value ());
  EXPECT_EQ (calls, 0);
}

// M23: the rvalue overloads pass an lvalue; the four categories.
TEST (LumexOptionalMonadicTest,
      GivenCategories_WhenAndThen_ThenFunctionSeesThem)
{
  optional<int> value (1);
  optional<int> const cvalue (1);
  EXPECT_EQ (*value.and_then (CategoriesOpt ()), 1);
  EXPECT_EQ (*cvalue.and_then (CategoriesOpt ()), 2);
  EXPECT_EQ (*std::move (value).and_then (CategoriesOpt ()), 3);
  EXPECT_EQ (*std::move (cvalue).and_then (CategoriesOpt ()), 4);
}

TEST (LumexOptionalMonadicTest, GivenMoveOnly_WhenAndThenOnRvalue_ThenMoved)
{
  optional<std::unique_ptr<int>> owner (std::unique_ptr<int> (new int (3)));
  auto result = std::move (owner).and_then (
      [] (std::unique_ptr<int> p) -> optional<int>
        { return optional<int> (*p + 1); });
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (*result, 4);
  ASSERT_TRUE (owner.has_value ());
  EXPECT_EQ (owner->get (), nullptr) << "the value was moved into the call";
}

// M20: a function that does not return an optional is accepted.
TEST (LumexOptionalMonadicTest, GivenTraits_WhenAndThen_ThenConstrained)
{
  static_assert (can_and_then<optional<int> &, opt_fn>::value, "");
  static_assert (can_and_then<optional<int> const &, opt_fn>::value, "");
  static_assert (can_and_then<optional<int>, opt_fn>::value, "");
  static_assert (can_and_then<optional<int> const, opt_fn>::value, "");
  static_assert (!can_and_then<optional<int> &, int_fn>::value,
                 "M20: the result is not an optional");
  static_assert (!can_and_then<optional<int>, void_fn>::value,
                 "M20: void result");
  static_assert (!can_and_then<optional<int> &, nullary_opt_fn>::value,
                 "the function must take the value");
  static_assert (!can_and_then<optional<int> &, string_fn>::value, "");
  static_assert (
      std::is_same<decltype (std::declval<optional<int> &> ().and_then (
                       std::declval<opt_fn> ())),
                   optional<int>>::value,
      "the result type is the optional the function returns");
  static_assert (
      std::is_same<decltype (std::declval<optional<int> &> ().and_then (
                       std::declval<optional<long> (*) (int)> ())),
                   optional<long>>::value,
      "");
  static_assert (
      std::is_same<decltype (std::declval<optional<int> &> ().and_then (
                       std::declval<optional<int> const &(*)(int)> ())),
                   optional<int>>::value,
      "the result is remove_cvref_t of the call result");
  SUCCEED ();
}

TEST (LumexOptionalMonadicTest, GivenMemberPointers_WhenCalled_ThenInvoked)
{
  optional<Widget> widget (in_place, 4);
  auto from_member = widget.transform (&Widget::get);
  static_assert (std::is_same<decltype (from_member), optional<int>>::value,
                 "");
  ASSERT_TRUE (from_member.has_value ());
  EXPECT_EQ (*from_member, 4);
  // A pointer to a data member that is itself an optional: and_then returns a
  // copy of it (the result is remove_cvref_t of the call).
  optional<Holder> holder (in_place);
  holder->inner = 9;
  auto inner = holder.and_then (&Holder::inner);
  static_assert (std::is_same<decltype (inner), optional<int>>::value, "");
  ASSERT_TRUE (inner.has_value ());
  EXPECT_EQ (*inner, 9);
  // A member pointer applied through a std::reference_wrapper and a pointer.
  Widget object (6);
  optional<std::reference_wrapper<Widget>> wrapped (object);
  EXPECT_EQ (*wrapped.transform (&Widget::get), 6);
  optional<Widget *> pointer (&object);
  EXPECT_EQ (*pointer.transform (&Widget::get), 6);
}

// ---- transform ----

TEST (LumexOptionalMonadicTest, GivenEngaged_WhenTransform_ThenWrappedResult)
{
  optional<int> value (4);
  auto plus = value.transform (plus_one);
  static_assert (std::is_same<decltype (plus), optional<int>>::value, "");
  ASSERT_TRUE (plus.has_value ());
  EXPECT_EQ (*plus, 5);
  auto text = value.transform (
      [] (int v) { return std::string (static_cast<std::size_t> (v), 'x'); });
  static_assert (std::is_same<decltype (text), optional<std::string>>::value,
                 "");
  EXPECT_EQ (*text, "xxxx");
  optional<int> empty;
  EXPECT_FALSE (empty.transform (plus_one).has_value ());
}

// M21: transform flattens an optional result.
TEST (LumexOptionalMonadicTest,
      GivenOptionalResult_WhenTransform_ThenNotFlattened)
{
  optional<int> value (10);
  auto nested = value.transform (half_if_even);
  static_assert (
      std::is_same<decltype (nested), optional<optional<int>>>::value,
      "M21: optional<optional<int>>");
  ASSERT_TRUE (nested.has_value ());
  EXPECT_TRUE (nested->has_value ());
  EXPECT_EQ (**nested, 5);

  optional<int> odd (3);
  auto inner_empty = odd.transform (half_if_even);
  ASSERT_TRUE (inner_empty.has_value ())
      << "M21: the outer optional is engaged although the inner is empty";
  EXPECT_FALSE (inner_empty->has_value ());

  optional<int> none;
  auto outer_empty = none.transform (half_if_even);
  EXPECT_FALSE (outer_empty.has_value ());
}

// M22: the cv of the result is kept (remove_cv missing).
TEST (LumexOptionalMonadicTest, GivenCvResult_WhenTransform_ThenCvRemoved)
{
  static_assert (
      std::is_same<decltype (std::declval<optional<int> &> ().transform (
                       std::declval<std::string const (*) (int)> ())),
                   optional<std::string>>::value,
      "M22: remove_cv_t of the result");
  SUCCEED ();
}

// M24b: the value category of the argument.
TEST (LumexOptionalMonadicTest,
      GivenCategories_WhenTransform_ThenFunctionSeesThem)
{
  optional<int> value (1);
  optional<int> const cvalue (1);
  EXPECT_EQ (*value.transform (Categories ()), 1);
  EXPECT_EQ (*cvalue.transform (Categories ()), 2);
  EXPECT_EQ (*std::move (value).transform (Categories ()), 3);
  EXPECT_EQ (*std::move (cvalue).transform (Categories ()), 4);
}

TEST (LumexOptionalMonadicTest, GivenTraits_WhenTransform_ThenConstrained)
{
  static_assert (can_transform<optional<int> &, int_fn>::value, "");
  static_assert (can_transform<optional<int> const, string_fn>::value, "");
  static_assert (!can_transform<optional<int> &, void_fn>::value,
                 "a void result cannot be held");
  static_assert (!can_transform<optional<int> &, nullary_int_fn>::value,
                 "the function must take the value");
  static_assert (!can_transform<optional<int> &, int (*) (int, int)>::value,
                 "");
  static_assert (!can_transform<optional<int> &, in_place_t (*) (int)>::value,
                 "in_place_t cannot be held");
  static_assert (!can_transform<optional<int> &, nullopt_t (*) (int)>::value,
                 "nullopt_t cannot be held");
  static_assert (!can_transform<optional<int> &, int (&(*)(int))[2]>::value,
                 "an array result cannot be held");
  static_assert (
      std::is_same<decltype (std::declval<optional<int> const &> ().transform (
                       std::declval<string_fn> ())),
                   optional<std::string>>::value,
      "");
  SUCCEED ();
}

// ---- or_else ----

// M25: the function is called although a value is held.
TEST (LumexOptionalMonadicTest, GivenEngaged_WhenOrElse_ThenNoCallAndSameValue)
{
  int calls = 0;
  optional<int> value (7);
  auto result = value.or_else (
      [&calls] () -> optional<int>
        {
          ++calls;
          return optional<int> (-1);
        });
  static_assert (std::is_same<decltype (result), optional<int>>::value, "");
  EXPECT_EQ (calls, 0);
  EXPECT_EQ (*result, 7);
  optional<int> moved = std::move (value).or_else (
      [&calls] () -> optional<int>
        {
          ++calls;
          return optional<int> ();
        });
  EXPECT_EQ (calls, 0);
  EXPECT_EQ (*moved, 7);
}

TEST (LumexOptionalMonadicTest, GivenEmpty_WhenOrElse_ThenFunctionResult)
{
  optional<int> empty;
  optional<int> const cempty;
  EXPECT_EQ (*empty.or_else (make_five), 5);
  EXPECT_EQ (*cempty.or_else (make_five), 5);
  EXPECT_EQ (*optional<int> ().or_else (make_five), 5);
  EXPECT_FALSE (empty.or_else (make_empty).has_value ());
}

// M24: the copy constructibility constraint of or_else const & is dropped.
TEST (LumexOptionalMonadicTest, GivenTraits_WhenOrElse_ThenConstrained)
{
  using move_only = optional<std::unique_ptr<int>>;
  using move_only_fn = move_only (*) ();
  static_assert (can_or_else<optional<int> const &, nullary_opt_fn>::value,
                 "");
  static_assert (can_or_else<optional<int>, nullary_opt_fn>::value, "");
  static_assert (!can_or_else<optional<int> &, nullary_int_fn>::value,
                 "the result must be an optional<T>");
  static_assert (!can_or_else<optional<int> &, opt_fn>::value,
                 "the function takes no argument");
  static_assert (!can_or_else<optional<long> &, nullary_opt_fn>::value,
                 "optional<int> is not optional<long>");
  static_assert (!can_or_else<move_only const &, move_only_fn>::value,
                 "M24: a const lvalue needs a copyable T");
  static_assert (!can_or_else<move_only &, move_only_fn>::value,
                 "an lvalue needs a copyable T");
  static_assert (can_or_else<move_only, move_only_fn>::value,
                 "an rvalue needs a movable T");
  SUCCEED ();
}

// ---- constexpr in C++11 ----

TEST (LumexOptionalMonadicTest, GivenConstexprObjects_WhenMonadic_ThenConstant)
{
  constexpr optional<int> five (5);
  constexpr optional<int> none;
  constexpr optional<int> plus = five.transform (constexpr_plus_one);
  static_assert (plus.has_value () && *plus == 6,
                 "transform is constexpr from C++11");
  constexpr optional<int> plus_none = none.transform (constexpr_plus_one);
  static_assert (!plus_none.has_value (), "");
  constexpr optional<int> eight (8);
  constexpr optional<int> halved = eight.and_then (constexpr_half);
  static_assert (halved.has_value () && *halved == 4,
                 "and_then is constexpr from C++11");
  constexpr optional<int> kept = five.or_else (constexpr_five);
  static_assert (*kept == 5, "or_else keeps the value");
  constexpr optional<int> filled = none.or_else (constexpr_five);
  static_assert (*filled == 5, "or_else is constexpr from C++11");
  EXPECT_EQ (*plus, 6);
}
