// LumexRangesConstraints.cxx11.tests.cpp
//
// Which calls of get_nearest_to exist. The constraints are SFINAE in every
// standard, so a call that does not fit finds no overload; the detector below
// asks the compiler instead of compiling a rejected call. The same
// expectations hold at C++11, 14, 17 and 20 (below C++20 the iterator and
// range requirements are the C++11 forms, from C++20 the standard concepts,
// and they agree on everything used here; what only the C++20 concepts accept
// is in LumexRanges.cxx20.tests.cpp). The iterator needs to be bidirectional,
// the sentinel has to compare with it, the value type has to be numeric, the
// projection has to be invocable with an element and to give a numeric type,
// and the comparison has to be invocable with the value and the projected
// element in both orders. A range has to outlive the call, so a temporary
// container finds no overload.
#include <array>
#include <cstddef>
#include <deque>
#include <forward_list>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/functional/LumexFunctional.hpp"
#include "lumex/core/utility/ranges/LumexRanges.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

namespace functional = lumex::core::utility::functional;
namespace traits = lumex::core::utility::traits;
using lumex::core::utility::ranges::Algorithm::get_nearest_to;

namespace
{
// get_nearest_to (declval<Args> ()...) is well-formed.
template <typename Void, typename... Args>
struct can_call_impl : std::false_type
{
};

template <typename... Args>
struct can_call_impl<
    traits::meta::void_t<decltype (get_nearest_to (std::declval<Args> ()...))>,
    Args...> : std::true_type
{
};

template <typename... Args> struct can_call : can_call_impl<void, Args...>
{
};

struct item
{
  int key;
};

// A sentinel that is not an iterator: the end of a zero-terminated array.
struct zero_sentinel
{
};

LUMEX_ATTRIBUTE_MAYBE_UNUSED inline bool
operator== (int const *position, zero_sentinel)
{
  return *position == 0;
}

LUMEX_ATTRIBUTE_MAYBE_UNUSED inline bool
operator!= (int const *position, zero_sentinel sentinel)
{
  return !(position == sentinel);
}

LUMEX_ATTRIBUTE_MAYBE_UNUSED inline bool
operator== (zero_sentinel sentinel, int const *position)
{
  return position == sentinel;
}

LUMEX_ATTRIBUTE_MAYBE_UNUSED inline bool
operator!= (zero_sentinel sentinel, int const *position)
{
  return !(position == sentinel);
}

struct not_a_bool
{
};

struct returns_not_a_bool
{
  not_a_bool
  operator() (int, int) const
  {
    return not_a_bool ();
  }
};

struct takes_strings
{
  bool
  operator() (std::string const &, std::string const &) const
  {
    return false;
  }
};

// Callable as (int, double), the order of (element, value), but not as
// (double, int): the comparison of a strict weak order has to take both.
struct element_first_only
{
  bool
  operator() (int, double) const
  {
    return false;
  }

  bool operator() (double, int) const = delete;
};

typedef std::vector<int> ints_t;
typedef std::vector<double> reals_t;
} // namespace

// --- ranges
// ---------------------------------------------------------------------

TEST (LumexRangesConstraintsTest,
      GivenLvalueRanges_WhenGetNearestTo_ThenTheRangeOverloadExists)
{
  EXPECT_TRUE ((can_call<ints_t &, int>::value));
  EXPECT_TRUE ((can_call<ints_t const &, int>::value));
  EXPECT_TRUE ((can_call<reals_t &, double>::value));
  EXPECT_TRUE ((can_call<std::list<int> &, int>::value));
  EXPECT_TRUE ((can_call<std::deque<int> &, int>::value));
  EXPECT_TRUE ((can_call<std::array<int, 3> &, int>::value));
  EXPECT_TRUE ((can_call<std::array<int, 3> const &, int>::value));
  EXPECT_TRUE ((can_call<int (&)[4], int>::value));
  EXPECT_TRUE ((can_call<int const (&)[4], int>::value));
  EXPECT_TRUE ((can_call<std::set<double> &, double>::value));
  EXPECT_TRUE ((can_call<std::multiset<int> const &, int>::value));
}

TEST (LumexRangesConstraintsTest,
      GivenMapRange_WhenProjectionNamesTheKey_ThenTheRangeOverloadExists)
{
  typedef std::map<int, std::string> table_t;
  // The elements are pairs, not numbers: a projection has to name the key.
  EXPECT_FALSE ((can_call<table_t &, int>::value));
  EXPECT_TRUE ((can_call<table_t &, int, functional::less,
                         decltype (&table_t::value_type::first)>::value));
}

TEST (LumexRangesConstraintsTest,
      GivenValueOfAnotherNumericType_WhenGetNearestTo_ThenExists)
{
  EXPECT_TRUE ((can_call<ints_t &, double>::value));
  EXPECT_TRUE ((can_call<reals_t &, int>::value));
  EXPECT_TRUE ((can_call<ints_t &, long>::value));
  EXPECT_TRUE ((can_call<ints_t &, unsigned char>::value));
  EXPECT_TRUE ((can_call<ints_t &, char>::value));
  EXPECT_TRUE ((can_call<std::vector<float> &, float>::value));
  EXPECT_TRUE ((can_call<std::vector<long double> &, long double>::value));
  EXPECT_TRUE ((can_call<ints_t &, int const &>::value));
  EXPECT_TRUE ((can_call<ints_t &, int &>::value));
}

TEST (LumexRangesConstraintsTest,
      GivenTemporaryContainer_WhenGetNearestTo_ThenNoOverload)
{
  // The iterator of a temporary would dangle: below C++20 only an lvalue
  // range is accepted, and from C++20 a range that is not borrowed is not.
  EXPECT_FALSE ((can_call<ints_t, int>::value));
  EXPECT_FALSE ((can_call<ints_t &&, int>::value));
  EXPECT_FALSE ((can_call<std::list<int>, int>::value));
  EXPECT_FALSE ((can_call<std::array<int, 3>, int>::value));
  EXPECT_FALSE ((can_call<ints_t const, int>::value));
}

// --- iterator pairs
// ---------------------------------------------------------------

TEST (
    LumexRangesConstraintsTest,
    GivenBidirectionalIterators_WhenGetNearestTo_ThenTheIteratorOverloadExists)
{
  EXPECT_TRUE ((can_call<ints_t::iterator, ints_t::iterator, int>::value));
  EXPECT_TRUE (
      (can_call<ints_t::const_iterator, ints_t::const_iterator, int>::value));
  EXPECT_TRUE ((can_call<int *, int *, int>::value));
  EXPECT_TRUE ((can_call<int const *, int const *, double>::value));
  EXPECT_TRUE ((can_call<std::list<int>::iterator, std::list<int>::iterator,
                         int>::value));
  EXPECT_TRUE ((can_call<std::set<int>::const_iterator,
                         std::set<int>::const_iterator, int>::value));
  EXPECT_TRUE ((can_call<ints_t::reverse_iterator, ints_t::reverse_iterator,
                         int, std::greater<int>>::value));
}

TEST (LumexRangesConstraintsTest,
      GivenIteratorAndConstIterator_WhenGetNearestTo_ThenTheEndIsWalkedTo)
{
  // iterator and const_iterator compare with each other, so the second one
  // is a sentinel of the first.
  EXPECT_TRUE (
      (can_call<ints_t::iterator, ints_t::const_iterator, int>::value));
}

TEST (LumexRangesConstraintsTest,
      GivenSentinelOfAnotherType_WhenGetNearestTo_ThenExists)
{
  EXPECT_TRUE ((can_call<int const *, zero_sentinel, int>::value));
  EXPECT_FALSE ((can_call<zero_sentinel, int const *, int>::value));
}

TEST (LumexRangesConstraintsTest,
      GivenIteratorsThatAreNotBidirectional_WhenGetNearestTo_ThenNoOverload)
{
  typedef std::forward_list<int> forward_t;
  EXPECT_FALSE ((can_call<forward_t &, int>::value));
  EXPECT_FALSE (
      (can_call<forward_t::iterator, forward_t::iterator, int>::value));
  typedef std::istream_iterator<int> input_t;
  EXPECT_FALSE ((can_call<input_t, input_t, int>::value));
  typedef std::ostream_iterator<int> output_t;
  EXPECT_FALSE ((can_call<output_t, output_t, int>::value));
}

TEST (LumexRangesConstraintsTest,
      GivenArgumentsThatAreNotIterators_WhenGetNearestTo_ThenNoOverload)
{
  EXPECT_FALSE ((can_call<int, int, int>::value));
  EXPECT_FALSE ((can_call<ints_t, ints_t, int>::value));
  EXPECT_FALSE ((can_call<std::string, std::string, int>::value));
  EXPECT_FALSE ((can_call<void *, void *, int>::value));
  EXPECT_FALSE ((can_call<std::nullptr_t, std::nullptr_t, int>::value));
  // An iterator and a number; an iterator and a pointer of another type.
  EXPECT_FALSE ((can_call<ints_t::iterator, int, int>::value));
  EXPECT_FALSE ((can_call<int *, long *, int>::value));
  EXPECT_FALSE ((can_call<int *, std::string *, int>::value));
}

// --- the value
// ------------------------------------------------------------------------

TEST (LumexRangesConstraintsTest,
      GivenValueThatIsNotNumeric_WhenGetNearestTo_ThenNoOverload)
{
  EXPECT_FALSE ((can_call<ints_t &, std::string>::value));
  EXPECT_FALSE ((can_call<ints_t &, char const *>::value));
  EXPECT_FALSE ((can_call<ints_t &, int *>::value));
  EXPECT_FALSE ((can_call<ints_t &, bool>::value));
  EXPECT_FALSE ((can_call<ints_t &, std::nullptr_t>::value));
  EXPECT_FALSE ((can_call<ints_t &, item>::value));
  EXPECT_FALSE (
      (can_call<ints_t::iterator, ints_t::iterator, std::string>::value));
}

TEST (LumexRangesConstraintsTest,
      GivenElementsThatAreNotNumeric_WhenGetNearestTo_ThenNoOverload)
{
  EXPECT_FALSE ((can_call<std::vector<std::string> &, int>::value));
  EXPECT_FALSE ((can_call<std::vector<item> &, int>::value));
  EXPECT_FALSE ((can_call<std::vector<bool> &, int>::value));
  EXPECT_FALSE ((can_call<std::vector<int *> &, int>::value));
}

TEST (LumexRangesConstraintsTest,
      GivenTooFewOrTooManyArguments_WhenGetNearestTo_ThenNoOverload)
{
  EXPECT_FALSE ((can_call<ints_t &>::value));
  EXPECT_FALSE ((can_call<>::value));
  EXPECT_FALSE ((can_call<ints_t::iterator, ints_t::iterator>::value));
  EXPECT_FALSE ((can_call<ints_t &, int, functional::less,
                          functional::identity, int>::value));
}

// --- the comparison
// ------------------------------------------------------------------

TEST (LumexRangesConstraintsTest,
      GivenComparisons_WhenGetNearestTo_ThenTheyMustBeCallableBothWays)
{
  EXPECT_TRUE ((can_call<ints_t &, int, functional::less>::value));
  EXPECT_TRUE ((can_call<ints_t &, int, std::less<int>>::value));
  EXPECT_TRUE ((can_call<ints_t &, int, std::greater<int>>::value));
  EXPECT_TRUE ((can_call<ints_t &, int, bool (*) (int, int)>::value));
  EXPECT_TRUE ((can_call<ints_t &, double, bool (*) (double, double)>::value));
  EXPECT_TRUE (
      (can_call<ints_t &, int, std::function<bool (int, int)>>::value));
}

TEST (LumexRangesConstraintsTest,
      GivenCallablesThatDoNotFit_WhenGetNearestTo_ThenNoOverload)
{
  EXPECT_FALSE ((can_call<ints_t &, int, int>::value));
  EXPECT_FALSE ((can_call<ints_t &, int, std::string>::value));
  EXPECT_FALSE ((can_call<ints_t &, int, takes_strings>::value));
  EXPECT_FALSE ((can_call<ints_t &, int, returns_not_a_bool>::value));
  EXPECT_FALSE ((can_call<ints_t &, int, bool (*) (int)>::value));
  EXPECT_FALSE ((can_call<ints_t &, int, bool (*) (int, int, int)>::value));
  EXPECT_FALSE ((can_call<ints_t &, int, void (*) (int, int)>::value));
  EXPECT_FALSE ((can_call<ints_t &, int, std::nullptr_t>::value));
}

TEST (LumexRangesConstraintsTest,
      GivenComparisonCallableOneWayOnly_WhenGetNearestTo_ThenNoOverload)
{
  EXPECT_FALSE ((can_call<ints_t &, double, element_first_only>::value));
  EXPECT_FALSE ((can_call<ints_t::iterator, ints_t::iterator, double,
                          element_first_only>::value));
}

// --- the projection
// -----------------------------------------------------------------------

TEST (LumexRangesConstraintsTest,
      GivenProjections_WhenGetNearestTo_ThenTheyMustGiveANumber)
{
  EXPECT_TRUE ((can_call<std::vector<item> &, int, functional::less,
                         int item::*>::value));
  EXPECT_TRUE ((can_call<std::vector<item> const &, int, functional::less,
                         int item::*>::value));
  EXPECT_TRUE ((can_call<std::vector<item *> &, int, functional::less,
                         int item::*>::value));
  EXPECT_TRUE ((can_call<std::vector<item> &, int, functional::less,
                         int (*) (item const &)>::value));
  EXPECT_TRUE ((can_call<std::vector<item> &, int, functional::less,
                         std::function<int (item const &)>>::value));
  EXPECT_TRUE ((can_call<std::vector<std::pair<double, std::string>> &, int,
                         functional::less,
                         double std::pair<double, std::string>::*>::value));
}

TEST (LumexRangesConstraintsTest,
      GivenProjectionsThatDoNotFit_WhenGetNearestTo_ThenNoOverload)
{
  // Not invocable with an element.
  EXPECT_FALSE ((can_call<ints_t &, int, functional::less, int>::value));
  EXPECT_FALSE (
      (can_call<ints_t &, int, functional::less, std::nullptr_t>::value));
  EXPECT_FALSE (
      (can_call<ints_t &, int, functional::less, int (*) (int, int)>::value));
  EXPECT_FALSE ((can_call<ints_t &, int, functional::less,
                          int (*) (std::string const &)>::value));
  // A member of another class, or a member that is not there for the element.
  EXPECT_FALSE (
      (can_call<ints_t &, int, functional::less, int item::*>::value));
  // Invocable, but the result is not a number.
  EXPECT_FALSE ((can_call<std::vector<item> &, int, functional::less,
                          std::string (*) (item const &)>::value));
  EXPECT_FALSE ((can_call<std::vector<item> &, int, functional::less,
                          item (*) (item const &)>::value));
  EXPECT_FALSE ((can_call<std::vector<item> &, int, functional::less,
                          bool (*) (item const &)>::value));
  EXPECT_FALSE ((can_call<std::vector<item> &, int, functional::less,
                          void (*) (item const &)>::value));
}

TEST (LumexRangesConstraintsTest,
      GivenStringsAndMemberFunctionProjection_WhenGetNearestTo_ThenExists)
{
  // The strings are not numbers, their length is.
  EXPECT_FALSE ((can_call<std::vector<std::string> &, int>::value));
  EXPECT_TRUE ((can_call<std::vector<std::string> &, int, functional::less,
                         std::size_t (std::string::*) () const>::value));
}

TEST (LumexRangesConstraintsTest,
      GivenIteratorAndConstIterator_WhenSearching_ThenFindsTheNearestElement)
{
  ints_t values;
  for (int value = 0; value < 8; ++value)
    values.push_back (value * 5);
  ints_t::iterator const found
      = get_nearest_to (values.begin (), values.cend (), 21);
  ASSERT_NE (found, values.end ());
  EXPECT_EQ (*found, 20);
  EXPECT_EQ (get_nearest_to (values.begin (), values.cend (), 1000),
             values.end () - 1);
  ints_t empty;
  EXPECT_EQ (get_nearest_to (empty.begin (), empty.cend (), 3),
             empty.begin ());
}

TEST (LumexRangesConstraintsTest,
      GivenSentinelOfAnotherType_WhenSearching_ThenFindsTheNearestElement)
{
  int const terminated[] = { 1, 4, 9, 16, 0, 25 };
  int const *const found = get_nearest_to (terminated, zero_sentinel (), 10);
  ASSERT_NE (found, nullptr);
  EXPECT_EQ (*found, 9);
  // Past the end of the terminated range, not of the array.
  int const *const last = get_nearest_to (terminated, zero_sentinel (), 100);
  EXPECT_EQ (*last, 16);
  int const empty[] = { 0, 5 };
  EXPECT_EQ (get_nearest_to (empty, zero_sentinel (), 3), &empty[0]);
}

// --- results
// ---------------------------------------------------------------------------------

TEST (LumexRangesConstraintsTest,
      GivenCalls_WhenDecltype_ThenResultIsTheIterator)
{
  static_assert (
      std::is_same<decltype (get_nearest_to (std::declval<ints_t &> (), 1)),
                   ints_t::iterator>::value,
      "");
  static_assert (std::is_same<decltype (get_nearest_to (
                                  std::declval<ints_t const &> (), 1)),
                              ints_t::const_iterator>::value,
                 "");
  static_assert (std::is_same<decltype (get_nearest_to (
                                  std::declval<std::list<double> &> (), 1)),
                              std::list<double>::iterator>::value,
                 "");
  static_assert (
      std::is_same<decltype (get_nearest_to (std::declval<int (&)[3]> (), 1)),
                   int *>::value,
      "");
  static_assert (std::is_same<decltype (get_nearest_to (
                                  std::declval<int const (&)[3]> (), 1)),
                              int const *>::value,
                 "");
  static_assert (std::is_same<decltype (get_nearest_to (
                                  std::declval<ints_t::iterator> (),
                                  std::declval<ints_t::const_iterator> (), 1)),
                              ints_t::iterator>::value,
                 "the result has the type of the first iterator");
  static_assert (std::is_same<decltype (get_nearest_to (
                                  std::declval<int const *> (),
                                  std::declval<zero_sentinel> (), 1)),
                              int const *>::value,
                 "");
  SUCCEED ();
}
