// lumex/tests/core/utility/functional/LumexFunctional.cxx14.tests.cpp
// From C++14 `less` still is the class of this library, never std::less<void>:
// it converts implicitly to and from std::less<void>, and, being transparent
// like it, makes the associative containers look up a key by a value of
// another type.
#include <functional>
#include <set>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/functional/LumexFunctional.hpp"

namespace functional = lumex::core::utility::functional;

namespace
{
/** A key that cannot be built from the type the lookup passes. */
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
} // namespace

TEST (LumexFunctionalStdTest, GivenCxx14_WhenLess_ThenNotStdLessVoid)
{
  static_assert (!std::is_same<functional::less, std::less<void>>::value,
                 "less is not std::less<void>");
  static_assert (
      std::is_empty<functional::less>::value
          && std::is_trivially_default_constructible<functional::less>::value,
      "still an empty trivial class");
  SUCCEED ();
}

TEST (LumexFunctionalStdTest, GivenStdLessVoid_WhenConverted_ThenBothWays)
{
  static_assert (std::is_convertible<functional::less, std::less<void>>::value,
                 "own to std");
  static_assert (std::is_convertible<std::less<void>, functional::less>::value,
                 "std to own");
  static_assert (!std::is_convertible<functional::less, std::less<int>>::value,
                 "std::less<int> is another class");
  static_assert (
      std::is_nothrow_constructible<std::less<void>, functional::less>::value
          && std::is_nothrow_constructible<functional::less,
                                           std::less<void>>::value,
      "the conversions are noexcept");
  constexpr std::less<void> toStd = functional::less ();
  constexpr functional::less fromStd = std::less<void> ();
  static_assert (toStd (1, 2) && fromStd (1, 2),
                 "usable in constant expressions");
  // A container whose comparator is the standard one takes the own object.
  std::set<std::string, std::less<void>> words ((functional::less ()));
  words.insert ("b");
  words.insert ("a");
  EXPECT_EQ (*words.begin (), "a");
  EXPECT_TRUE (words.find ("b") != words.end ());
  std::set<std::string, functional::less> back ((std::less<void> ()));
  back.insert ("x");
  EXPECT_EQ (back.size (), 1U);
}

TEST (LumexFunctionalStdTest, GivenTransparentLess_WhenFind_ThenKeyOfOtherType)
{
  // find (std::string) takes no person_t: only a transparent comparator
  // enables the template overload, and the lookup builds no key.
  std::set<person_t, functional::less> people;
  people.insert (person_t{ "alice" });
  people.insert (person_t{ "bob" });
  people.insert (person_t{ "carol" });
  EXPECT_TRUE (people.find (std::string ("bob")) != people.end ());
  EXPECT_TRUE (people.find (std::string ("dave")) == people.end ());
  EXPECT_EQ (people.count (std::string ("alice")), 1U);
  EXPECT_EQ (people.lower_bound (std::string ("b"))->name, "bob");
  EXPECT_EQ (people.upper_bound (std::string ("bob"))->name, "carol");
}

TEST (LumexFunctionalStdTest,
      GivenTransparentLess_WhenFindCString_ThenNoTemporary)
{
  std::set<std::string, functional::less> words;
  words.insert ("pear");
  words.insert ("apple");
  EXPECT_TRUE (words.find ("apple") != words.end ());
  EXPECT_TRUE (words.find ("plum") == words.end ());
}
