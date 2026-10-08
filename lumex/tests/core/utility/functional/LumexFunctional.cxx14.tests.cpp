// lumex/tests/core/utility/functional/LumexFunctional.cxx14.tests.cpp
// From C++14 `less` is std::less<void>, and a transparent comparator makes the
// associative containers look up a key by a value of another type. The header
// decides by LUMEX_HAS_STD_TRANSPARENT_OPERATORS; a toolchain that reports
// C++14 without them uses the own class and the identity test skips (the
// lookup tests still hold: the own class is transparent too).
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

TEST (LumexFunctionalStdTest, GivenCxx14_WhenLess_ThenStdLessVoid)
{
#if LUMEX_HAS_STD_TRANSPARENT_OPERATORS
  static_assert (std::is_same<functional::less, std::less<void>>::value,
                 "less is std::less<void>");
  SUCCEED ();
#else
  GTEST_SKIP () << "the standard library has no transparent operators";
#endif
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
