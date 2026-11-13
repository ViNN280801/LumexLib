// The comparison and the swap of unexpected<E> are hidden friends, as the
// standard declares them ([expected.un.eq], [expected.un.swap]): found by
// argument-dependent lookup only, so a qualified call names nothing, while the
// expressions `a == b`, `a != b` and `swap (a, b)` work as they always did.
// The source is compiled into every suite of the module (C++11, C++14, C++17,
// C++20), so before C++20 the written-out `!=` and from C++20 the rewritten
// one are both checked.

#include <string>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

// A name of the namespace of unexpected for the qualified calls below to look
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
namespace error
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
} // namespace error
} // namespace expected
} // namespace core
} // namespace lumex

namespace
{
using lumex::core::expected::error::unexpected;
namespace traits = lumex::core::utility::traits;
namespace error_ns = lumex::core::expected::error;

/// `error::operator== (a, b)` names a function that accepts the arguments.
template <typename A, typename B, typename = void>
struct qualified_equal_t : std::false_type
{
};

template <typename A, typename B>
struct qualified_equal_t<A, B,
                         traits::meta::void_t<decltype (error_ns::operator== (
                             std::declval<A> (), std::declval<B> ()))>>
    : std::true_type
{
};

/// `error::operator!= (a, b)` names a function that accepts the arguments.
template <typename A, typename B, typename = void>
struct qualified_not_equal_t : std::false_type
{
};

template <typename A, typename B>
struct qualified_not_equal_t<
    A, B,
    traits::meta::void_t<decltype (error_ns::operator!= (
        std::declval<A> (), std::declval<B> ()))>> : std::true_type
{
};

/// `error::swap (a, b)` names a function that accepts two lvalues of `T`.
template <typename T, typename = void>
struct qualified_swap_t : std::false_type
{
};

template <typename T>
struct qualified_swap_t<T, traits::meta::void_t<decltype (error_ns::swap (
                               std::declval<T &> (), std::declval<T &> ()))>>
    : std::true_type
{
};

/// `a == b` is well-formed (ADL only).
template <typename A, typename B, typename = void>
struct adl_equal_t : std::false_type
{
};

template <typename A, typename B>
struct adl_equal_t<
    A, B,
    traits::meta::void_t<decltype (std::declval<A> () == std::declval<B> ())>>
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

/// Cannot be moved or copied, but brings a swap of its own.
struct pinned_t
{
  explicit pinned_t (int v) : value (v) {}
  pinned_t (pinned_t const &) = delete;
  pinned_t (pinned_t &&) = delete;
  pinned_t &operator= (pinned_t const &) = delete;
  pinned_t &operator= (pinned_t &&) = delete;
  int value;
};

void
swap (pinned_t &lhs, pinned_t &rhs) noexcept
{
  int const kept = lhs.value;
  lhs.value = rhs.value;
  rhs.value = kept;
}

/// The result of `==` is a class that converts to `bool` implicitly.
struct implicit_answer_t
{
  bool answer;
  operator bool () const { return answer; }
};

/// The result of `==` is a class that converts to `bool` explicitly only.
struct explicit_answer_t
{
  bool answer;
  explicit
  operator bool () const
  {
    return answer;
  }
};

struct implicit_comparable_t
{
  int value;
};

struct explicit_comparable_t
{
  int value;
};

implicit_answer_t
operator== (implicit_comparable_t const &lhs, implicit_comparable_t const &rhs)
{
  return implicit_answer_t{ lhs.value == rhs.value };
}

explicit_answer_t
operator== (explicit_comparable_t const &lhs, explicit_comparable_t const &rhs)
{
  return explicit_answer_t{ lhs.value == rhs.value };
}

/// A class that derives from unexpected: the friends are found through the
/// base class.
struct derived_t : unexpected<int>
{
  explicit derived_t (int v) : unexpected<int> (v) {}
};

// === the qualified call names nothing ====================================

TEST (UnexpectedHiddenFriendsTest, Probe_QualifiedLookup_FindsOnlyTheProbe)
{
  using probe_t = lumex::core::expected::error::hidden_friend_probe_t;

  static_assert (qualified_equal_t<probe_t, probe_t>::value,
                 "the detector sees a function declared in the namespace");
  static_assert (qualified_not_equal_t<probe_t, probe_t>::value,
                 "the detector sees a function declared in the namespace");
  static_assert (qualified_swap_t<probe_t>::value,
                 "the detector sees a function declared in the namespace");
  SUCCEED ();
}

TEST (UnexpectedHiddenFriendsTest, Equal_QualifiedCall_NamesNoFunction)
{
  static_assert (!qualified_equal_t<unexpected<int>, unexpected<int>>::value,
                 "== of unexpected is not a member of the namespace");
  static_assert (!qualified_equal_t<unexpected<int>, unexpected<long>>::value,
                 "== of unexpected is not a member of the namespace");
  static_assert (!qualified_equal_t<unexpected<int> const &,
                                    unexpected<int> const &>::value,
                 "== of unexpected is not a member of the namespace");
  SUCCEED ();
}

TEST (UnexpectedHiddenFriendsTest, NotEqual_QualifiedCall_NamesNoFunction)
{
  static_assert (
      !qualified_not_equal_t<unexpected<int>, unexpected<int>>::value,
      "!= of unexpected is not a member of the namespace");
  static_assert (
      !qualified_not_equal_t<unexpected<int>, unexpected<long>>::value,
      "!= of unexpected is not a member of the namespace");
  SUCCEED ();
}

TEST (UnexpectedHiddenFriendsTest, Swap_QualifiedCall_NamesNoFunction)
{
  static_assert (!qualified_swap_t<unexpected<int>>::value,
                 "swap of unexpected is not a member of the namespace");
  static_assert (!qualified_swap_t<unexpected<std::string>>::value,
                 "swap of unexpected is not a member of the namespace");
  SUCCEED ();
}

// === the expressions still work ==========================================

TEST (UnexpectedHiddenFriendsTest, Equal_Expression_FindsTheFriendByAdl)
{
  static_assert (adl_equal_t<unexpected<int>, unexpected<int>>::value,
                 "same error types compare");
  static_assert (adl_equal_t<unexpected<int>, unexpected<long>>::value,
                 "other error types compare");
  static_assert (
      std::is_same<decltype (std::declval<unexpected<int> &> ()
                             == std::declval<unexpected<long> &> ()),
                   bool>::value,
      "the answer is a bool");

  unexpected<int> const one (1);
  unexpected<long> const long_one (1);
  unexpected<int> const two (2);

  EXPECT_TRUE (one == one);
  EXPECT_TRUE (one == long_one);
  EXPECT_TRUE (long_one == one);
  EXPECT_FALSE (one == two);
  EXPECT_TRUE (one != two);
  EXPECT_TRUE (long_one != two);
  EXPECT_FALSE (one != long_one);
}

TEST (UnexpectedHiddenFriendsTest, Swap_Expression_FindsTheFriendByAdl)
{
  static_assert (adl_only::swappable_t<unexpected<int>>::value,
                 "the friend is found without a using-declaration");

  unexpected<std::string> first ("first");
  unexpected<std::string> second ("second");

  swap (first, second);
  EXPECT_EQ (first.error (), "second");
  EXPECT_EQ (second.error (), "first");

  using std::swap;
  swap (first, second);
  EXPECT_EQ (first.error (), "first");
  EXPECT_EQ (second.error (), "second");
}

TEST (UnexpectedHiddenFriendsTest, Swap_Noexcept_IsTheNoexceptOfTheMember)
{
  static_assert (noexcept (swap (std::declval<unexpected<int> &> (),
                                 std::declval<unexpected<int> &> ())),
                 "ints swap without throwing");
  static_assert (noexcept (swap (std::declval<unexpected<pinned_t> &> (),
                                 std::declval<unexpected<pinned_t> &> ())),
                 "the swap of the error is noexcept");
  SUCCEED ();
}

TEST (UnexpectedHiddenFriendsTest, DerivedClass_ComparesAndSwapsThroughTheBase)
{
  static_assert (adl_equal_t<derived_t, derived_t>::value,
                 "a derived class compares with itself");
  static_assert (adl_equal_t<derived_t, unexpected<int>>::value,
                 "a derived class compares with its base");
  static_assert (adl_equal_t<unexpected<long>, derived_t>::value,
                 "a base of another error type compares with a derived class");
  static_assert (adl_only::swappable_t<derived_t>::value,
                 "a derived class is swapped through the base");

  derived_t first (1);
  derived_t second (2);

  EXPECT_FALSE (first == second);
  EXPECT_TRUE (first != second);
  EXPECT_TRUE (first == unexpected<int> (1));
  EXPECT_TRUE (unexpected<long> (2) == second);
  swap (first, second);
  EXPECT_EQ (first.error (), 2);
  EXPECT_EQ (second.error (), 1);
}

// === the constraints =====================================================

TEST (UnexpectedHiddenFriendsTest, Swap_ErrorThatCannotBeSwapped_IsNotOffered)
{
  static_assert (!adl_only::swappable_t<unexpected<unswappable_t>>::value,
                 "no swap and no move: the friend is not a candidate");
  static_assert (adl_only::swappable_t<unexpected<std::string>>::value,
                 "a movable error is swapped");
  SUCCEED ();
}

TEST (UnexpectedHiddenFriendsTest, Swap_ErrorWithASwapOfItsOwn_IsExchanged)
{
  static_assert (adl_only::swappable_t<unexpected<pinned_t>>::value,
                 "the swap of the error is enough");

  unexpected<pinned_t> first (1);
  unexpected<pinned_t> second (2);

  swap (first, second);
  EXPECT_EQ (first.error ().value, 2);
  EXPECT_EQ (second.error ().value, 1);
}

TEST (UnexpectedHiddenFriendsTest, Equal_AnswerThatConvertsToBool_IsAccepted)
{
  using error_t = implicit_comparable_t;

  static_assert (adl_equal_t<unexpected<error_t>, unexpected<error_t>>::value,
                 "an implicit conversion to bool is enough");

  EXPECT_TRUE (unexpected<error_t> (error_t{ 3 })
               == unexpected<error_t> (error_t{ 3 }));
  EXPECT_FALSE (unexpected<error_t> (error_t{ 3 })
                == unexpected<error_t> (error_t{ 4 }));
}

TEST (UnexpectedHiddenFriendsTest, Equal_AnswerThatDoesNotConvert_IsRejected)
{
  using error_t = explicit_comparable_t;

  static_assert (!adl_equal_t<unexpected<error_t>, unexpected<error_t>>::value,
                 "the answer must convert to bool implicitly");
  SUCCEED ();
}

TEST (UnexpectedHiddenFriendsTest, Equal_ErrorsThatDoNotCompare_AreRejected)
{
  static_assert (!adl_equal_t<unexpected<int>, unexpected<std::string>>::value,
                 "an int and a string do not compare");
  static_assert (!adl_equal_t<unexpected<unswappable_t>,
                              unexpected<unswappable_t>>::value,
                 "an error without == does not compare");
  SUCCEED ();
}

} // namespace
