// The comparisons and the swap of expected<T, E> and expected<void, E> are
// hidden friends, as the standard declares them ([expected.object.eq],
// [expected.void.eq], [expected.object.swap], [expected.void.swap]): found by
// argument-dependent lookup only, so a qualified call names nothing, while the
// expressions `a == b`, `a != b`, `value == a`, `a == unex` and `swap (a, b)`
// work as they always did, also through a class derived from expected. The
// source is compiled into every suite of the module (C++11, C++14, C++17,
// C++20, C++23): before C++20 the reversed `==` and every `!=` are written out
// as friends, from C++20 only `==` is and the compiler rewrites the rest, and
// no pair of operands may become ambiguous in either way.

#include <functional>
#include <string>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"
#include "lumex/tests/core/expected/ExpectedParitySupport.hpp"

// A name of the namespace of expected for the qualified calls below to look
// up: a qualified name that is not declared at all is an error where it is
// written, whatever the arguments are, so the probe gives `operator==`, `!=`
// and `swap` something to find. It takes a type of its own that no library
// type converts to, so a hit on a probe says nothing about the library.
namespace lumex
{
namespace core
{
namespace expected
{
namespace result
{
namespace
{
struct hidden_friend_probe_t
{
};
} // namespace

bool operator== (hidden_friend_probe_t, hidden_friend_probe_t);
bool operator!= (hidden_friend_probe_t, hidden_friend_probe_t);
void swap (hidden_friend_probe_t &, hidden_friend_probe_t &);
} // namespace result
} // namespace expected
} // namespace core
} // namespace lumex

namespace
{
using expected_parity::can_compare;
using lumex::core::expected::error::unexpected;
using lumex::core::expected::result::expected;
using lumex::core::expected::result::unexpect;
using lumex::core::expected::result::unexpect_t;
namespace traits = lumex::core::utility::traits;
namespace result_ns = lumex::core::expected::result;

constexpr int kValue = 7;
constexpr int kError = 42;

using ex_t = expected<int, int>;
using exl_t = expected<long, long>;
using ev_t = expected<void, int>;
using evl_t = expected<void, long>;

/// `result::operator== (a, b)` names a function that accepts the arguments.
template <typename A, typename B, typename = void>
struct qualified_equal_t : std::false_type
{
};

template <typename A, typename B>
struct qualified_equal_t<A, B,
                         traits::meta::void_t<decltype (result_ns::operator== (
                             std::declval<A> (), std::declval<B> ()))>>
    : std::true_type
{
};

/// `result::operator!= (a, b)` names a function that accepts the arguments.
template <typename A, typename B, typename = void>
struct qualified_not_equal_t : std::false_type
{
};

template <typename A, typename B>
struct qualified_not_equal_t<
    A, B,
    traits::meta::void_t<decltype (result_ns::operator!= (
        std::declval<A> (), std::declval<B> ()))>> : std::true_type
{
};

/// `result::swap (a, b)` names a function that accepts two lvalues of `T`.
template <typename T, typename = void>
struct qualified_swap_t : std::false_type
{
};

template <typename T>
struct qualified_swap_t<T, traits::meta::void_t<decltype (result_ns::swap (
                               std::declval<T &> (), std::declval<T &> ()))>>
    : std::true_type
{
};

namespace adl_only
{
// No `using std::swap` here: only the swap a type brings with it counts, so a
// type that `std::swap` could move but the friend refuses is told apart.
template <typename T, typename = void> struct swappable_t : std::false_type
{
};

template <typename T>
struct swappable_t<T, traits::meta::void_t<decltype (swap (
                          std::declval<T &> (), std::declval<T &> ()))>>
    : std::true_type
{
};

template <typename T, bool = swappable_t<T>::value>
struct nothrow_swappable_t : std::false_type
{
};

template <typename T>
struct nothrow_swappable_t<T, true>
    : std::integral_constant<bool, noexcept (swap (std::declval<T &> (),
                                                   std::declval<T &> ()))>
{
};
} // namespace adl_only

/// Cannot be exchanged by any means: no swap, no move.
struct unswappable_t
{
  explicit unswappable_t (int v) : value (v) {}
  unswappable_t (unswappable_t const &) = delete;
  unswappable_t (unswappable_t &&) = delete;
  unswappable_t &operator= (unswappable_t const &) = delete;
  unswappable_t &operator= (unswappable_t &&) = delete;
  int value;
};

/// Exchanged by a move that may throw.
struct throwing_move_t
{
  throwing_move_t () {}
  throwing_move_t (throwing_move_t const &) {}
  throwing_move_t (throwing_move_t &&) noexcept (false) {}
  throwing_move_t &
  operator= (throwing_move_t const &)
  {
    return *this;
  }
  throwing_move_t &
  operator= (throwing_move_t &&) noexcept (false)
  {
    return *this;
  }
};

/// The result of `==` converts to `bool` explicitly only.
struct explicit_answer_t
{
  bool answer;
  explicit
  operator bool () const
  {
    return answer;
  }
};

struct explicit_comparable_t
{
  int value;
};

explicit_answer_t
operator== (explicit_comparable_t const &lhs, explicit_comparable_t const &rhs)
{
  return explicit_answer_t{ lhs.value == rhs.value };
}

/// A class that derives from expected: the friends are found through the base.
struct derived_t : ex_t
{
  explicit derived_t (int v) : ex_t (v) {}
  derived_t (unexpect_t tag, int e) : ex_t (tag, e) {}
};

struct derived_void_t : ev_t
{
  derived_void_t () : ev_t () {}
  derived_void_t (unexpect_t tag, int e) : ev_t (tag, e) {}
};

/// Not an expected, but converts to one.
struct converts_to_expected_t
{
  operator ex_t () const { return ex_t (kValue); }
};

/// A class that derives privately from expected, with its own comparison with
/// an int: `ex == pd` compares through that operator (a private base is never
/// seen as an expected, as with std::expected).
struct private_derived_t : private ex_t
{
  explicit private_derived_t (int v) : ex_t (v) {}
};

bool
operator== (int lhs, private_derived_t const &)
{
  return lhs == kValue;
}

struct ambiguous_left_t : ex_t
{
};

struct ambiguous_right_t : ex_t
{
};

/// The same expected twice among the bases: no unique base to convert to.
struct ambiguous_base_t : ambiguous_left_t, ambiguous_right_t
{
};

/// Two different expected bases: the deduction of the pointer fails.
struct two_bases_t : ex_t, exl_t
{
};

struct incomplete_t;

/// A class that converts to `unexpected<int>` only.
struct converts_to_unexpected_t
{
  operator unexpected<int> () const { return unexpected<int> (kError); }
};

/// 1 or 0 for the answer of `a == b`, -1 where it is not well-formed.
template <typename A, typename B>
auto
answer_or_unavailable (A const &a, B const &b, int)
    -> decltype (static_cast<bool> (a == b), 0)
{
  return (a == b) ? 1 : 0;
}

template <typename A, typename B>
int
answer_or_unavailable (A const &, B const &, long)
{
  return -1;
}

// === the qualified call names nothing ====================================

TEST (ExpectedHiddenFriendsTest, Probe_QualifiedLookup_FindsOnlyTheProbe)
{
  using probe_t = lumex::core::expected::result::hidden_friend_probe_t;

  static_assert (qualified_equal_t<probe_t, probe_t>::value,
                 "the detector sees a function declared in the namespace");
  static_assert (qualified_not_equal_t<probe_t, probe_t>::value,
                 "the detector sees a function declared in the namespace");
  static_assert (qualified_swap_t<probe_t>::value,
                 "the detector sees a function declared in the namespace");
  SUCCEED ();
}

TEST (ExpectedHiddenFriendsTest, Equal_QualifiedCall_NamesNoFunction)
{
  static_assert (!qualified_equal_t<ex_t, ex_t>::value,
                 "expected == expected");
  static_assert (!qualified_equal_t<ex_t, exl_t>::value,
                 "expected == expected of other types");
  static_assert (!qualified_equal_t<ex_t const &, ex_t const &>::value,
                 "expected == expected, const lvalues");
  static_assert (!qualified_equal_t<ex_t, int>::value, "expected == value");
  static_assert (!qualified_equal_t<int, ex_t>::value, "value == expected");
  static_assert (!qualified_equal_t<ex_t, unexpected<int>>::value,
                 "expected == unexpected");
  static_assert (!qualified_equal_t<unexpected<int>, ex_t>::value,
                 "unexpected == expected");
  static_assert (!qualified_equal_t<ev_t, ev_t>::value,
                 "expected<void> == expected<void>");
  static_assert (!qualified_equal_t<ev_t, evl_t>::value,
                 "expected<void> == expected<void> of another error");
  static_assert (!qualified_equal_t<ev_t, unexpected<int>>::value,
                 "expected<void> == unexpected");
  static_assert (!qualified_equal_t<unexpected<int>, ev_t>::value,
                 "unexpected == expected<void>");
  SUCCEED ();
}

TEST (ExpectedHiddenFriendsTest, NotEqual_QualifiedCall_NamesNoFunction)
{
  static_assert (!qualified_not_equal_t<ex_t, ex_t>::value,
                 "expected != expected");
  static_assert (!qualified_not_equal_t<ex_t, exl_t>::value,
                 "expected != expected of other types");
  static_assert (!qualified_not_equal_t<ex_t, int>::value,
                 "expected != value");
  static_assert (!qualified_not_equal_t<int, ex_t>::value,
                 "value != expected");
  static_assert (!qualified_not_equal_t<ex_t, unexpected<int>>::value,
                 "expected != unexpected");
  static_assert (!qualified_not_equal_t<unexpected<int>, ex_t>::value,
                 "unexpected != expected");
  static_assert (!qualified_not_equal_t<ev_t, ev_t>::value,
                 "expected<void> != expected<void>");
  static_assert (!qualified_not_equal_t<ev_t, unexpected<int>>::value,
                 "expected<void> != unexpected");
  static_assert (!qualified_not_equal_t<unexpected<int>, ev_t>::value,
                 "unexpected != expected<void>");
  SUCCEED ();
}

TEST (ExpectedHiddenFriendsTest, Swap_QualifiedCall_NamesNoFunction)
{
  static_assert (!qualified_swap_t<ex_t>::value, "swap of expected");
  static_assert (!qualified_swap_t<exl_t>::value, "swap of expected");
  static_assert (!qualified_swap_t<ev_t>::value, "swap of expected<void>");
  static_assert (!qualified_swap_t<expected<std::string, std::string>>::value,
                 "swap of expected");
  SUCCEED ();
}

// === the expressions still work ==========================================

TEST (ExpectedHiddenFriendsTest, Equal_Expression_FindsEveryFriendByAdl)
{
  ex_t const value (kValue);
  ex_t const error (unexpect, kError);
  exl_t const long_value (kValue);
  exl_t const long_error (unexpect, kError);
  ev_t const success;
  ev_t const failure (unexpect, kError);
  evl_t const long_success;
  evl_t const long_failure (unexpect, kError);

  EXPECT_TRUE (value == ex_t (kValue));
  EXPECT_TRUE (value == long_value);
  EXPECT_TRUE (long_value == value);
  EXPECT_TRUE (error == long_error);
  EXPECT_FALSE (value == error);
  EXPECT_TRUE (value == kValue);
  EXPECT_TRUE (kValue == value);
  EXPECT_FALSE (error == kError);
  EXPECT_FALSE (kError == error);
  EXPECT_TRUE (error == unexpected<int> (kError));
  EXPECT_TRUE (unexpected<int> (kError) == error);
  EXPECT_FALSE (value == unexpected<int> (kError));
  EXPECT_TRUE (success == long_success);
  EXPECT_TRUE (failure == long_failure);
  EXPECT_FALSE (success == long_failure);
  EXPECT_TRUE (failure == unexpected<long> (kError));
  EXPECT_TRUE (unexpected<long> (kError) == failure);
  EXPECT_FALSE (success == unexpected<int> (kError));
}

TEST (ExpectedHiddenFriendsTest, NotEqual_Expression_FindsEveryFriendByAdl)
{
  ex_t const value (kValue);
  ex_t const error (unexpect, kError);
  exl_t const long_value (kValue + 1);
  ev_t const success;
  evl_t const long_failure (unexpect, kError);

  EXPECT_TRUE (value != long_value);
  EXPECT_TRUE (long_value != value);
  EXPECT_TRUE (value != error);
  EXPECT_TRUE (value != kValue + 1);
  EXPECT_TRUE (kValue + 1 != value);
  EXPECT_TRUE (error != kError);
  EXPECT_TRUE (kError != error);
  EXPECT_TRUE (value != unexpected<int> (kError));
  EXPECT_TRUE (unexpected<int> (kError) != value);
  EXPECT_FALSE (error != unexpected<int> (kError));
  EXPECT_FALSE (unexpected<int> (kError) != error);
  EXPECT_TRUE (success != long_failure);
  EXPECT_TRUE (success != unexpected<int> (kError));
  EXPECT_TRUE (unexpected<int> (kError) != success);
}

TEST (ExpectedHiddenFriendsTest, Swap_Expression_FindsTheFriendByAdl)
{
  static_assert (adl_only::swappable_t<ex_t>::value,
                 "the friend is found without a using-declaration");
  static_assert (adl_only::swappable_t<ev_t>::value,
                 "the friend is found without a using-declaration");

  ex_t first (kValue);
  ex_t second (unexpect, kError);

  swap (first, second);
  EXPECT_FALSE (first.has_value ());
  EXPECT_EQ (first.error (), kError);
  EXPECT_EQ (*second, kValue);

  using std::swap;
  swap (first, second);
  EXPECT_EQ (*first, kValue);
  EXPECT_EQ (second.error (), kError);

  ev_t success;
  ev_t failure (unexpect, kError);
  swap (success, failure);
  EXPECT_FALSE (success.has_value ());
  EXPECT_EQ (success.error (), kError);
  EXPECT_TRUE (failure.has_value ());
}

TEST (ExpectedHiddenFriendsTest, Swap_Noexcept_IsTheNoexceptOfTheMember)
{
  static_assert (adl_only::nothrow_swappable_t<ex_t>::value,
                 "ints swap without throwing");
  static_assert (adl_only::nothrow_swappable_t<ev_t>::value,
                 "an int error swaps without throwing");
  static_assert (
      !adl_only::nothrow_swappable_t<expected<throwing_move_t, int>>::value,
      "a throwing move of the value makes the swap throwing");
  static_assert (
      !adl_only::nothrow_swappable_t<expected<int, throwing_move_t>>::value,
      "a throwing move of the error makes the swap throwing");
  static_assert (
      !adl_only::nothrow_swappable_t<expected<void, throwing_move_t>>::value,
      "a throwing move of the error makes the swap throwing");
  static_assert (adl_only::swappable_t<expected<throwing_move_t, int>>::value,
                 "but the swap is offered");
  SUCCEED ();
}

// === a class derived from expected =======================================

TEST (ExpectedHiddenFriendsTest, DerivedClass_ComparesThroughTheBase)
{
  static_assert (can_compare<derived_t, derived_t>::value,
                 "a derived class compares with itself");
  static_assert (can_compare<derived_t, ex_t>::value,
                 "a derived class compares with its base");
  static_assert (can_compare<exl_t, derived_t>::value,
                 "an expected of another type compares with a derived class");
  static_assert (can_compare<derived_t, int>::value,
                 "a derived class compares with a value");
  static_assert (can_compare<int, derived_t>::value,
                 "a value compares with a derived class");
  static_assert (can_compare<derived_t, unexpected<int>>::value,
                 "a derived class compares with an unexpected");
  static_assert (can_compare<unexpected<long>, derived_t>::value,
                 "an unexpected compares with a derived class");
  static_assert (can_compare<derived_void_t, derived_void_t>::value,
                 "a derived class of expected<void> compares with itself");
  static_assert (can_compare<derived_void_t, ev_t>::value,
                 "a derived class of expected<void> compares with its base");
  static_assert (can_compare<derived_void_t, unexpected<int>>::value,
                 "a derived class of expected<void> compares with an "
                 "unexpected");

  derived_t const value (kValue);
  derived_t const error (unexpect, kError);
  derived_void_t const success;
  derived_void_t const failure (unexpect, kError);

  EXPECT_TRUE (value == derived_t (kValue));
  EXPECT_TRUE (value != error);
  EXPECT_TRUE (value == kValue);
  EXPECT_TRUE (kValue == value);
  EXPECT_TRUE (error == unexpected<int> (kError));
  EXPECT_TRUE (unexpected<int> (kError) == error);
  EXPECT_TRUE (value == exl_t (kValue));
  EXPECT_TRUE (exl_t (kValue) == value);
  EXPECT_TRUE (success == derived_void_t ());
  EXPECT_TRUE (success != failure);
  EXPECT_TRUE (failure == unexpected<int> (kError));
  EXPECT_TRUE (unexpected<int> (kError) == failure);
  EXPECT_TRUE (failure == evl_t (unexpect, kError));
}

TEST (ExpectedHiddenFriendsTest, DerivedClass_IsSwappedThroughTheBase)
{
  static_assert (adl_only::swappable_t<derived_t>::value,
                 "a derived class is swapped through the base");
  static_assert (adl_only::swappable_t<derived_void_t>::value,
                 "a derived class of expected<void> is swapped");

  derived_t first (kValue);
  derived_t second (unexpect, kError);

  swap (first, second);
  EXPECT_EQ (first.error (), kError);
  EXPECT_EQ (*second, kValue);
}

// === the first operand converts to expected ==============================

TEST (ExpectedHiddenFriendsTest,
      Equal_FirstOperandThatConvertsToExpected_IsAccepted)
{
  // The first parameter of a friend is not deduced, so the operand may
  // convert to it, as with std::expected: a class found by lookup through the
  // other operand is enough.
  static_assert (can_compare<converts_to_expected_t, ex_t>::value,
                 "the first parameter of the friend is not deduced");

  converts_to_expected_t const converting{};

  EXPECT_TRUE (converting == ex_t (kValue));
  EXPECT_FALSE (converting == ex_t (kValue + 1));
  EXPECT_FALSE (converting == ex_t (unexpect, kValue));
}

TEST (ExpectedHiddenFriendsTest,
      Equal_ExpectedAgainstConvertingOperand_NeedsTheRewrittenCandidate)
{
  // The converting operand on the right is not deduced as expected<T2, E2>:
  // below C++20 `ex == converting` is ill-formed, from C++20 the compiler
  // rewrites it as the reversed `converting == ex`. std::expected is the same.
#if LUMEX_HAS_IMPL_THREE_WAY_COMPARISON
  static_assert (can_compare<ex_t, converts_to_expected_t>::value,
                 "C++20 rewrites it as the reversed comparison");

  EXPECT_TRUE (ex_t (kValue) == converts_to_expected_t{});
  EXPECT_FALSE (ex_t (kValue + 1) == converts_to_expected_t{});
#else
  static_assert (!can_compare<ex_t, converts_to_expected_t>::value,
                 "below C++20 there is no rewritten candidate");
  SUCCEED ();
#endif
}

// === a class derived from expected is compared as an expected ============

TEST (ExpectedHiddenFriendsTest,
      Equal_ExpectedOfExpectedAgainstDerived_ComparesAsAnExpected)
{
  // `derived_t` is an expected<int, int>: against an expected<expected<int,
  // int>, int> it is compared as an expected (state and contents), not as a
  // value of the inner type as std::expected does.
  expected<ex_t, int> const outer_error (unexpect, kError);
  expected<ex_t, int> const outer_value{ ex_t (kValue) };
  derived_t const error (unexpect, kError);
  derived_t const value (kValue);

  EXPECT_TRUE (outer_error == error);
  EXPECT_TRUE (error == outer_error);
  EXPECT_FALSE (outer_error == value);
  EXPECT_FALSE (value == outer_error);
  EXPECT_TRUE (outer_value == value);
  EXPECT_TRUE (value == outer_value);
  EXPECT_TRUE (outer_error != value);
}

// === derives_from_expected ===============================================

TEST (ExpectedHiddenFriendsTest,
      DerivesFromExpected_EveryKindOfType_IsAnswered)
{
  using lumex::core::expected::result::detail::derives_from_expected;

  static_assert (derives_from_expected<ex_t>::value, "expected itself");
  static_assert (derives_from_expected<ev_t>::value, "expected<void> itself");
  static_assert (derives_from_expected<derived_t>::value, "public base");
  static_assert (derives_from_expected<derived_void_t>::value,
                 "public base, void");
  static_assert (derives_from_expected<derived_t const>::value, "const");
  static_assert (derives_from_expected<derived_t volatile>::value, "volatile");
  static_assert (derives_from_expected<derived_t const volatile>::value,
                 "const volatile");
  static_assert (!derives_from_expected<private_derived_t>::value,
                 "a private base is not seen, and is not a hard error");
  static_assert (!derives_from_expected<ambiguous_base_t>::value,
                 "an ambiguous base is not seen, and is not a hard error");
  static_assert (!derives_from_expected<two_bases_t>::value,
                 "two different expected bases: the deduction fails");
  static_assert (!derives_from_expected<incomplete_t>::value,
                 "an incomplete type is not derived from anything");
  static_assert (!derives_from_expected<int>::value, "int");
  static_assert (!derives_from_expected<int[3]>::value, "array");
  static_assert (!derives_from_expected<void ()>::value, "function");
  static_assert (!derives_from_expected<unexpected<int>>::value,
                 "an unexpected is not an expected");
  SUCCEED ();
}

TEST (ExpectedHiddenFriendsTest, Equal_PrivateBase_UsesTheOperatorOfTheClass)
{
  static_assert (can_compare<ex_t, private_derived_t>::value,
                 "a private base is a value, as for std::expected");

  private_derived_t const pd (kError);

  EXPECT_TRUE (ex_t (kValue) == pd);
  EXPECT_FALSE (ex_t (kValue + 1) == pd);
  EXPECT_FALSE (ex_t (unexpect, kValue) == pd);
}

// === operands that wrap an expected ======================================

TEST (ExpectedHiddenFriendsTest, Equal_ReferenceWrapper_NeverRecursesNorLies)
{
  // A reference_wrapper converts to the expected it refers to. Whether the
  // comparison is well-formed depends on the standard and the compiler (the
  // left operand of a friend is only reached by a conversion, and the
  // reversed candidate needs C++20), but it never recurses through that
  // conversion and, where it compiles, never calls two equal values unequal
  // or two different values equal. A value friend whose operand is not
  // deduced did recurse without end (Clang, C++20).
  ex_t value (kValue);
  ex_t same (kValue);
  ex_t other (kValue + 1);

  EXPECT_NE (answer_or_unavailable (std::ref (value), same, 0), 0);
  EXPECT_NE (answer_or_unavailable (same, std::ref (value), 0), 0);
  EXPECT_NE (answer_or_unavailable (kValue, std::ref (value), 0), 0);
  EXPECT_NE (answer_or_unavailable (std::ref (value), other, 0), 1);
  EXPECT_NE (answer_or_unavailable (other, std::ref (value), 0), 1);
  EXPECT_NE (answer_or_unavailable (kValue + 1, std::ref (value), 0), 1);
}

// === an unexpected next to a converting operand ==========================

TEST (ExpectedHiddenFriendsTest,
      Equal_UnexpectedOperandsThatConvert_BehaveLikeStdUnexpected)
{
  // Like std::unexpected: the first parameter of the friend is not deduced, so
  // an operand that converts to `unexpected<int>` on the left is accepted, and
  // so is a reference_wrapper of an unexpected there. The converting operand
  // on the right needs the rewritten candidate of C++20.
  static_assert (can_compare<converts_to_unexpected_t, unexpected<int>>::value,
                 "the first parameter is not deduced");

  unexpected<int> const unex (kError);
  unexpected<int> const other (kError + 1);

  EXPECT_TRUE (converts_to_unexpected_t{} == unex);
  EXPECT_FALSE (converts_to_unexpected_t{} == other);
  EXPECT_EQ (answer_or_unavailable (std::ref (unex), unex, 0), 1);
  EXPECT_EQ (answer_or_unavailable (std::ref (unex), other, 0), 0);
  EXPECT_NE (answer_or_unavailable (unex, std::ref (unex), 0), 0);
#if LUMEX_HAS_IMPL_THREE_WAY_COMPARISON
  static_assert (can_compare<unexpected<int>, converts_to_unexpected_t>::value,
                 "C++20 rewrites it as the reversed comparison");
#else
  static_assert (
      !can_compare<unexpected<int>, converts_to_unexpected_t>::value,
      "below C++20 there is no rewritten candidate");
#endif
}

// === no pair becomes ambiguous ===========================================

TEST (ExpectedHiddenFriendsTest,
      Equal_EveryKindOfPair_GivesABoolWithoutAmbiguity)
{
  ex_t const value (kValue);
  exl_t const long_value (kValue);
  ev_t const success;
  evl_t const long_success;
  ev_t const failure (unexpect, kError);
  expected<int, long> const long_error (unexpect, kError);
  unexpected<int> const unex (kError);
  unexpected<long> const long_unex (kError);

  static_assert (std::is_same<decltype (value == long_value), bool>::value,
                 "expected<int, E> == expected<long, E>");
  static_assert (std::is_same<decltype (long_value == value), bool>::value,
                 "expected<long, E> == expected<int, E>");
  static_assert (std::is_same<decltype (value == unex), bool>::value,
                 "expected == unexpected");
  static_assert (std::is_same<decltype (unex == value), bool>::value,
                 "unexpected == expected");
  static_assert (std::is_same<decltype (value == kValue), bool>::value,
                 "expected == value");
  static_assert (std::is_same<decltype (kValue == value), bool>::value,
                 "value == expected");
  static_assert (std::is_same<decltype (success == long_success), bool>::value,
                 "expected<void, E> == expected<void, E2>");
  static_assert (std::is_same<decltype (failure == long_unex), bool>::value,
                 "expected<void, E> == unexpected<E2>");
  static_assert (std::is_same<decltype (long_unex == failure), bool>::value,
                 "unexpected<E2> == expected<void, E>");
  static_assert (std::is_same<decltype (long_error == unex), bool>::value,
                 "expected<T, E2> == unexpected<E>");
  static_assert (std::is_same<decltype (unex == long_error), bool>::value,
                 "unexpected<E> == expected<T, E2>");
  static_assert (std::is_same<decltype (value != long_value), bool>::value,
                 "expected<int, E> != expected<long, E>");
  static_assert (std::is_same<decltype (value != kValue), bool>::value,
                 "expected != value");
  static_assert (std::is_same<decltype (kValue != value), bool>::value,
                 "value != expected");
  static_assert (std::is_same<decltype (unex != value), bool>::value,
                 "unexpected != expected");

  EXPECT_TRUE (value == long_value);
  EXPECT_TRUE (long_value == value);
  EXPECT_TRUE (value == kValue);
  EXPECT_TRUE (kValue == value);
  EXPECT_FALSE (value == unex);
  EXPECT_FALSE (unex == value);
  EXPECT_TRUE (success == long_success);
  EXPECT_TRUE (long_success == success);
  EXPECT_TRUE (failure == long_unex);
  EXPECT_TRUE (long_unex == failure);
  EXPECT_TRUE (long_error == unex);
  EXPECT_TRUE (unex == long_error);
  EXPECT_TRUE (long_error == long_unex);
  EXPECT_FALSE (value != long_value);
  EXPECT_FALSE (value != kValue);
  EXPECT_FALSE (kValue != value);
  EXPECT_TRUE (unex != value);
}

TEST (ExpectedHiddenFriendsTest,
      Equal_ExpectedOfExpected_OperandsAreNotTakenForValues)
{
  // An expected or an unexpected on the right is never taken for a value of
  // the left one: an expected<expected<int, int>, int> against an unexpected
  // compares the error, against an expected<int, int> the inner value.
  expected<ex_t, int> const outer{ ex_t (kValue) };
  expected<ex_t, int> const outer_error (unexpect, kError);

  EXPECT_TRUE (outer == ex_t (kValue));
  EXPECT_TRUE (ex_t (kValue) == outer);
  EXPECT_FALSE (outer == unexpected<int> (kError));
  EXPECT_TRUE (outer_error == unexpected<int> (kError));
  EXPECT_TRUE (unexpected<int> (kError) == outer_error);
  EXPECT_FALSE (outer_error == ex_t (kError));
}

// === the constraints =====================================================

TEST (ExpectedHiddenFriendsTest, Swap_ContentsThatCannotBeSwapped_IsNotOffered)
{
  // Mutation note: with the decltype constraint of the friend swap replaced
  // by `void`, GCC and Clang fail the first three assertions, cl.exe does not:
  // it also takes the noexcept-specifier into account for SFINAE, so on MSVC
  // the mutant is equivalent and the constraint is only checked on GCC and
  // Clang.
  static_assert (!adl_only::swappable_t<expected<unswappable_t, int>>::value,
                 "a value that cannot be swapped");
  static_assert (!adl_only::swappable_t<expected<int, unswappable_t>>::value,
                 "an error that cannot be swapped");
  static_assert (!adl_only::swappable_t<expected<void, unswappable_t>>::value,
                 "an error that cannot be swapped, void value");
  static_assert (!adl_only::swappable_t<expected<int const, int>>::value,
                 "a const value is not swappable");
  static_assert (
      adl_only::swappable_t<expected<std::string, std::string>>::value,
      "movable contents are swapped");
  SUCCEED ();
}

TEST (ExpectedHiddenFriendsTest, Equal_ContentsThatDoNotCompare_AreRejected)
{
  using bad_value_t = expected<explicit_comparable_t, int>;
  using bad_error_t = expected<int, explicit_comparable_t>;
  using bad_void_t = expected<void, explicit_comparable_t>;

  static_assert (!can_compare<bad_value_t, bad_value_t>::value,
                 "the answer must convert to bool implicitly (values)");
  static_assert (!can_compare<bad_error_t, bad_error_t>::value,
                 "the answer must convert to bool implicitly (errors)");
  static_assert (!can_compare<bad_void_t, bad_void_t>::value,
                 "the answer must convert to bool implicitly (void)");
  static_assert (!can_compare<bad_value_t, explicit_comparable_t>::value,
                 "the answer must convert to bool implicitly (value)");
  static_assert (
      !can_compare<bad_error_t, unexpected<explicit_comparable_t>>::value,
      "the answer must convert to bool implicitly (unexpected)");
  static_assert (
      !can_compare<unexpected<explicit_comparable_t>, bad_error_t>::value,
      "the answer must convert to bool implicitly (reversed)");
  static_assert (
      !can_compare<bad_void_t, unexpected<explicit_comparable_t>>::value,
      "the answer must convert to bool implicitly (void, unexpected)");
  static_assert (!can_compare<ex_t, std::string>::value,
                 "an int and a string do not compare");
  static_assert (!can_compare<std::string, ex_t>::value,
                 "a string and an int do not compare");
  static_assert (!can_compare<ex_t, expected<std::string, int>>::value,
                 "the values cannot be compared");
  static_assert (!can_compare<ev_t, ex_t>::value,
                 "expected<void> does not compare with a value type");
  static_assert (!can_compare<ex_t, ev_t>::value,
                 "a value type does not compare with expected<void>");
  static_assert (!can_compare<ev_t, int>::value,
                 "expected<void> has no value to compare");
  static_assert (!can_compare<int, ev_t>::value,
                 "expected<void> has no value to compare");
  SUCCEED ();
}

// === constant expressions ================================================

constexpr ex_t kValueEx (kValue);
constexpr ex_t kErrorEx (unexpect, kError);
constexpr exl_t kLongValueEx (kValue);
constexpr ev_t kSuccessEx{};
constexpr evl_t kLongSuccessEx{};
constexpr ev_t kFailureEx (unexpect, kError);

static_assert (kValueEx == kLongValueEx,
               "expected == expected of another type");
static_assert (kLongValueEx == kValueEx, "and the other way");
static_assert (kValueEx != kErrorEx, "!= between a value and an error");
static_assert (kValueEx == kValue && kValue == kValueEx, "value, both sides");
static_assert (kValueEx != kError && kError != kValueEx, "!= value");
static_assert (unexpected<int> (kError) == kErrorEx, "unexpected == expected");
static_assert (kErrorEx == unexpected<long> (kError),
               "expected == unexpected");
static_assert (unexpected<int> (kValue) != kErrorEx, "unexpected != expected");
static_assert (kSuccessEx == kLongSuccessEx, "void == void of another error");
static_assert (kSuccessEx != kFailureEx, "void != void");
static_assert (unexpected<long> (kError) == kFailureEx,
               "unexpected == expected<void>");
static_assert (kFailureEx != unexpected<int> (kValue),
               "expected<void> != unexpected");

TEST (ExpectedHiddenFriendsTest, ConstantExpressions_AgreeWithRunTimeAnswers)
{
  EXPECT_TRUE (kValueEx == kLongValueEx);
  EXPECT_TRUE (kLongValueEx == kValueEx);
  EXPECT_TRUE (kValueEx != kErrorEx);
  EXPECT_TRUE (unexpected<int> (kError) == kErrorEx);
  EXPECT_TRUE (kSuccessEx == kLongSuccessEx);
  EXPECT_TRUE (kFailureEx != unexpected<int> (kValue));
}

} // namespace
