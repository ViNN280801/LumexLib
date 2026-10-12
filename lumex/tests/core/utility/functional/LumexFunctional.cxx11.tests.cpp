// lumex/tests/core/utility/functional/LumexFunctional.cxx11.tests.cpp
// identity and less of LumexFunctional.hpp. They are classes of this library
// in every standard (never std::identity or std::less<void>; the conversions
// to and from those are checked in LumexFunctional.cxx14 and .cxx20); the
// tests describe their behavior (forwarding of the argument category,
// transparency, the result of `<`, noexcept, constant expressions, the order
// of pointers), so the same file runs in every suite.
#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/functional/LumexFunctional.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

namespace functional = lumex::core::utility::functional;
namespace traits = lumex::core::utility::traits;

namespace
{
typedef functional::identity identity_t;
typedef functional::less less_t;

/** Cannot be copied or moved: only a reference can be passed around. */
struct pinned_t
{
  pinned_t () : value (0) {}
  pinned_t (pinned_t const &) = delete;
  pinned_t (pinned_t &&) = delete;
  pinned_t &operator= (pinned_t const &) = delete;
  pinned_t &operator= (pinned_t &&) = delete;

  int value;
};

struct plain_t
{
};

/** The result of `<` on probe_t: which overload took the operands. */
enum class operands_t
{
  lvalues,
  rvalues,
  const_lvalues
};

struct probe_t
{
  int value;
};

operands_t
operator< (probe_t &, probe_t &)
{
  return operands_t::lvalues;
}

operands_t
operator< (probe_t &&, probe_t &&)
{
  return operands_t::rvalues;
}

operands_t
operator< (probe_t const &, probe_t const &)
{
  return operands_t::const_lvalues;
}

/** `<` that may throw. */
struct throwing_t
{
  int value;
};

bool
operator< (throwing_t const &left, throwing_t const &right) noexcept (false)
{
  return left.value < right.value;
}

/** `<` that does not throw. */
struct quiet_t
{
  int value;
};

bool
operator< (quiet_t const &left, quiet_t const &right) noexcept
{
  return left.value < right.value;
}

/** A class with `<` against a std::string, both ways round. */
struct person_t
{
  std::string name;
};

bool
operator< (person_t const &left, person_t const &right)
{
  return left.name < right.name;
}

bool
operator< (person_t const &left, std::string const &right)
{
  return left.name < right;
}

bool
operator< (std::string const &left, person_t const &right)
{
  return left < right.name;
}

struct base_t
{
  virtual ~base_t () {}
};

struct derived_t : base_t
{
};

template <typename T, typename Enable = void>
struct has_is_transparent : std::false_type
{
};

template <typename T>
struct has_is_transparent<T, traits::meta::void_t<typename T::is_transparent>>
    : std::true_type
{
};

template <typename Func, typename... Args>
struct is_callable_with : traits::invoke::is_callable<Func, Args...>
{
};
} // namespace

// ---------------------------------------------------------------------------
// identity
// ---------------------------------------------------------------------------

TEST (LumexFunctionalTest, GivenValueCategories_WhenIdentity_ThenSameCategory)
{
  static_assert (
      std::is_same<decltype (identity_t () (std::declval<int &> ())),
                   int &>::value,
      "an lvalue stays an lvalue");
  static_assert (
      std::is_same<decltype (identity_t () (std::declval<int const &> ())),
                   int const &>::value,
      "a const lvalue stays a const lvalue");
  static_assert (std::is_same<decltype (identity_t () (std::declval<int> ())),
                              int &&>::value,
                 "an rvalue stays an rvalue");
  static_assert (
      std::is_same<decltype (identity_t () (std::declval<int const> ())),
                   int const &&>::value,
      "a const rvalue stays a const rvalue");
  static_assert (
      std::is_same<decltype (identity_t () (std::declval<int volatile &> ())),
                   int volatile &>::value,
      "a volatile lvalue stays a volatile lvalue");
  static_assert (
      std::is_same<decltype (identity_t () (std::declval<int (&)[3]> ())),
                   int (&)[3]>::value,
      "an array is not decayed");
  static_assert (
      std::is_same<decltype (identity_t () (std::declval<void (&) ()> ())),
                   void (&) ()>::value,
      "a function is not decayed");
  SUCCEED ();
}

TEST (LumexFunctionalTest, GivenLvalue_WhenIdentity_ThenSameObject)
{
  int number = 5;
  EXPECT_EQ (&identity_t () (number), &number);
  int const constant = 6;
  EXPECT_EQ (&identity_t () (constant), &constant);

  std::string text = "abc";
  std::string &same = identity_t () (text);
  EXPECT_EQ (&same, &text);
  identity_t () (text) += "d";
  EXPECT_EQ (text, "abcd");

  int array[3] = { 1, 2, 3 };
  EXPECT_EQ (&identity_t () (array), &array);
}

TEST (LumexFunctionalTest, GivenPinnedObject_WhenIdentity_ThenNoCopyNoMove)
{
  pinned_t pinned;
  pinned.value = 7;
  EXPECT_EQ (&identity_t () (pinned), &pinned);
  EXPECT_EQ (identity_t () (pinned).value, 7);
  pinned_t &&moved = identity_t () (std::move (pinned));
  EXPECT_EQ (&moved, &pinned);
}

TEST (LumexFunctionalTest, GivenMoveOnlyValue_WhenIdentity_ThenNothingMoved)
{
  std::unique_ptr<int> pointer (new int (42));
  int const *raw = pointer.get ();
  std::unique_ptr<int> &&forwarded = identity_t () (std::move (pointer));
  // Forwarding does not move: the object still owns the integer.
  EXPECT_EQ (pointer.get (), raw);
  EXPECT_EQ (forwarded.get (), raw);
  std::unique_ptr<int> taken (std::move (forwarded));
  EXPECT_EQ (taken.get (), raw);
  EXPECT_EQ (pointer.get (), nullptr);
}

TEST (LumexFunctionalTest, GivenRvalueInteger_WhenIdentity_ThenValueKept)
{
  EXPECT_EQ (identity_t () (5), 5);
  EXPECT_EQ (identity_t () (std::string ("moved")), "moved");
  static_assert (identity_t () (5) == 5, "usable in a constant expression");
  static_assert (identity_t () (7) + identity_t () (1) == 8,
                 "usable twice in one constant expression");
}

TEST (LumexFunctionalTest, GivenIdentity_WhenInspected_ThenTransparentAndEmpty)
{
  static_assert (has_is_transparent<identity_t>::value,
                 "identity is transparent");
  static_assert (!has_is_transparent<std::less<int>>::value,
                 "the detection does tell");
  static_assert (std::is_empty<identity_t>::value, "no state");
  static_assert (std::is_default_constructible<identity_t>::value, "default");
  static_assert (std::is_copy_constructible<identity_t>::value, "copy");
  static_assert (noexcept (identity_t () (std::declval<int &> ())),
                 "the call does not throw");
  static_assert (noexcept (identity_t () (std::declval<std::string> ())),
                 "even for a class rvalue");
  SUCCEED ();
}

TEST (LumexFunctionalTest, GivenIdentity_WhenUsedAsProjection_ThenElementsSeen)
{
  std::vector<int> const source{ 3, 1, 2 };
  std::vector<int> copy (source.size ());
  std::transform (source.begin (), source.end (), copy.begin (),
                  identity_t ());
  EXPECT_EQ (copy, source);
}

// ---------------------------------------------------------------------------
// less
// ---------------------------------------------------------------------------

TEST (LumexFunctionalTest, GivenNumbers_WhenLess_ThenStrictOrder)
{
  EXPECT_TRUE (less_t () (1, 2));
  EXPECT_FALSE (less_t () (2, 1));
  EXPECT_FALSE (less_t () (1, 1));
  EXPECT_TRUE (less_t () (-3, -2));
  EXPECT_TRUE (less_t () (1.5, 2.5));
  EXPECT_FALSE (less_t () (2.5, 1.5));
  EXPECT_FALSE (less_t () (2.5, 2.5));
  EXPECT_TRUE (less_t () ('a', 'b'));
  EXPECT_TRUE (less_t () (1u, 2u));
}

TEST (LumexFunctionalTest, GivenMixedTypes_WhenLess_ThenComparedWithLess)
{
  EXPECT_TRUE (less_t () (1, 1.5));
  EXPECT_FALSE (less_t () (2, 1.5));
  EXPECT_TRUE (less_t () (1.5f, 2.0));
  EXPECT_TRUE (less_t () (1L, 2LL));
  EXPECT_TRUE (less_t () (std::string ("abc"), "abd"));
  EXPECT_FALSE (less_t () ("abd", std::string ("abc")));
  EXPECT_FALSE (less_t () (std::string ("abc"), "abc"));
  EXPECT_TRUE (less_t () (std::string ("ab"), std::string ("abc")));
}

TEST (LumexFunctionalTest,
      GivenOperandCategories_WhenLess_ThenForwardedAsGiven)
{
  probe_t left = { 1 };
  probe_t right = { 2 };
  probe_t const constant = { 3 };
  EXPECT_EQ (less_t () (left, right), operands_t::lvalues);
  EXPECT_EQ (less_t () (std::move (left), std::move (right)),
             operands_t::rvalues);
  EXPECT_EQ (less_t () (constant, constant), operands_t::const_lvalues);
  EXPECT_EQ (less_t () (left, constant), operands_t::const_lvalues);
  // An lvalue with an rvalue: only the const-reference overload takes both.
  EXPECT_EQ (less_t () (left, std::move (right)), operands_t::const_lvalues);
}

TEST (LumexFunctionalTest,
      GivenOverloadedLess_WhenLess_ThenResultOfLessOperator)
{
  static_assert (
      std::is_same<decltype (less_t () (std::declval<probe_t &> (),
                                        std::declval<probe_t &> ())),
                   operands_t>::value,
      "the result is what `<` returns, not bool");
  static_assert (std::is_same<decltype (less_t () (1, 2)), bool>::value,
                 "bool for numbers");
  static_assert (
      std::is_same<decltype (less_t () (std::declval<std::string> (),
                                        std::declval<char const *> ())),
                   bool>::value,
      "bool for strings");
  SUCCEED ();
}

TEST (LumexFunctionalTest, GivenThrowingOperator_WhenLess_ThenNoexceptFollows)
{
  static_assert (noexcept (less_t () (1, 2)), "numbers do not throw");
  static_assert (noexcept (less_t () (std::declval<quiet_t &> (),
                                      std::declval<quiet_t &> ())),
                 "a noexcept `<` gives a noexcept call");
  static_assert (!noexcept (less_t () (std::declval<throwing_t &> (),
                                       std::declval<throwing_t &> ())),
                 "a throwing `<` gives a throwing call");
  // The operators also run: the declarations above are not only unevaluated.
  quiet_t quiet_low = { 1 };
  quiet_t quiet_high = { 2 };
  EXPECT_TRUE (less_t () (quiet_low, quiet_high));
  EXPECT_FALSE (less_t () (quiet_high, quiet_low));
  throwing_t throwing_low = { 1 };
  throwing_t throwing_high = { 2 };
  EXPECT_TRUE (less_t () (throwing_low, throwing_high));
  EXPECT_FALSE (less_t () (throwing_high, throwing_low));
}

TEST (LumexFunctionalTest, GivenIncomparableTypes_WhenLess_ThenNotCallable)
{
  static_assert (is_callable_with<less_t, int, int>::value, "numbers");
  static_assert (is_callable_with<less_t, int, double>::value, "mixed");
  static_assert (is_callable_with<less_t, std::string, char const *>::value,
                 "strings");
  static_assert (is_callable_with<less_t, probe_t &, probe_t &>::value,
                 "overloaded `<`");
  static_assert (!is_callable_with<less_t, plain_t, plain_t>::value,
                 "no `<` on plain_t");
  static_assert (!is_callable_with<less_t, int, plain_t>::value,
                 "no `<` between int and plain_t");
  static_assert (!is_callable_with<less_t, std::string, int>::value,
                 "no `<` between std::string and int");
  static_assert (!is_callable_with<less_t, int>::value, "one operand");
  static_assert (!is_callable_with<less_t, int, int, int>::value,
                 "three operands");
  SUCCEED ();
}

TEST (LumexFunctionalTest, GivenLess_WhenInspected_ThenTransparentAndEmpty)
{
  static_assert (has_is_transparent<less_t>::value, "less is transparent");
  static_assert (std::is_empty<less_t>::value, "no state");
  static_assert (std::is_default_constructible<less_t>::value, "default");
  static_assert (std::is_copy_constructible<less_t>::value, "copy");
  SUCCEED ();
}

TEST (LumexFunctionalTest,
      GivenNumbers_WhenLessInConstantExpression_ThenUsable)
{
  static_assert (less_t () (1, 2), "true in a constant expression");
  static_assert (!less_t () (2, 1), "false in a constant expression");
  static_assert (!less_t () (2, 2), "equal is not less");
  static_assert (less_t () (1, 2.5), "mixed in a constant expression");
  static_assert (less_t () ('a', 'b'), "characters");
  SUCCEED ();
}

TEST (LumexFunctionalTest, GivenPointersIntoOneArray_WhenLess_ThenAddressOrder)
{
  int values[4] = { 0, 0, 0, 0 };
  EXPECT_TRUE (less_t () (&values[0], &values[3]));
  EXPECT_FALSE (less_t () (&values[3], &values[0]));
  EXPECT_FALSE (less_t () (&values[2], &values[2]));
  EXPECT_TRUE (less_t () (values, values + 1));
  EXPECT_TRUE (less_t () (values + 1, values + 4));
  int const *const first = values;
  int const *const last = values + 4;
  EXPECT_TRUE (less_t () (first, last));
  EXPECT_FALSE (less_t () (last, first));
}

TEST (LumexFunctionalTest, GivenPointersIntoTwoObjects_WhenLess_ThenTotalOrder)
{
  // Pointers into different arrays: the order is that of
  // std::less<void const volatile *>, a strict total order, and the same
  // whichever way round the operands are written.
  int first[3] = { 0, 0, 0 };
  int second[3] = { 0, 0, 0 };
  std::less<void const volatile *> const total;
  EXPECT_EQ (less_t () (&first[0], &second[0]), total (&first[0], &second[0]));
  EXPECT_EQ (less_t () (&second[0], &first[0]), total (&second[0], &first[0]));
  EXPECT_NE (less_t () (&first[0], &second[0]),
             less_t () (&second[0], &first[0]));
  EXPECT_EQ (less_t () (first, second), total (first, second));
  EXPECT_FALSE (less_t () (first, first));
  // The standard containers need that order for a pointer key.
  std::set<int *, less_t> ordered;
  ordered.insert (&second[2]);
  ordered.insert (&first[1]);
  ordered.insert (&second[0]);
  ordered.insert (&first[1]);
  EXPECT_EQ (ordered.size (), 3U);
  EXPECT_TRUE (std::is_sorted (ordered.begin (), ordered.end (),
                               std::less<void const volatile *> ()));
}

TEST (LumexFunctionalTest, GivenDifferentPointerTypes_WhenLess_ThenCompared)
{
  int values[2] = { 0, 0 };
  EXPECT_TRUE (less_t () (&values[0], static_cast<void const *> (&values[1])));
  EXPECT_FALSE (less_t () (static_cast<void *> (&values[1]), &values[0]));
  int const volatile *const qualified = &values[1];
  EXPECT_TRUE (less_t () (&values[0], qualified));

  derived_t derived;
  base_t *const as_base = &derived;
  EXPECT_FALSE (less_t () (&derived, as_base));
  EXPECT_FALSE (less_t () (as_base, &derived));
}

TEST (LumexFunctionalTest, GivenNullPointer_WhenLess_ThenCompilesAndIsStrict)
{
  int value = 0;
  int *const none = nullptr;
  int *const some = &value;
  EXPECT_FALSE (less_t () (none, none));
  EXPECT_FALSE (less_t () (some, some));
  EXPECT_NE (less_t () (none, some), less_t () (some, none));
}

TEST (LumexFunctionalTest, GivenContainers_WhenLessIsComparator_ThenSorted)
{
  std::vector<int> numbers{ 5, 2, 9, 1, 5, 6 };
  std::sort (numbers.begin (), numbers.end (), less_t ());
  EXPECT_EQ (numbers, (std::vector<int>{ 1, 2, 5, 5, 6, 9 }));
  EXPECT_TRUE (
      std::binary_search (numbers.begin (), numbers.end (), 6, less_t ()));
  EXPECT_FALSE (
      std::binary_search (numbers.begin (), numbers.end (), 7, less_t ()));
  std::vector<int>::const_iterator const bound
      = std::lower_bound (numbers.begin (), numbers.end (), 5, less_t ());
  EXPECT_EQ (bound - numbers.begin (), 2);

  std::set<std::string, less_t> words;
  words.insert ("pear");
  words.insert ("apple");
  words.insert ("fig");
  words.insert ("apple");
  ASSERT_EQ (words.size (), 3U);
  EXPECT_EQ (*words.begin (), "apple");
  EXPECT_EQ (*words.rbegin (), "pear");

  std::map<std::string, int, less_t> counts;
  counts["b"] = 2;
  counts["a"] = 1;
  EXPECT_EQ (counts.begin ()->first, "a");
}

TEST (LumexFunctionalTest, GivenSortedPeople_WhenLessIsComparator_ThenOrdered)
{
  person_t const alice = { "alice" };
  person_t const bob = { "bob" };
  EXPECT_TRUE (less_t () (alice, bob));
  EXPECT_TRUE (less_t () (alice, std::string ("bob")));
  EXPECT_TRUE (less_t () (std::string ("alice"), bob));
  EXPECT_FALSE (less_t () (bob, std::string ("alice")));
}
