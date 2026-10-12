// LumexRangesProjection.cxx11.tests.cpp
//
// The projections and comparisons get_nearest_to accepts, from C++11: any
// callable, and the pointers to a data member or to a member function that
// std::invoke accepts (a plain `proj (*it)` would lose them). The element may
// be the object, a pointer or smart pointer to it, or a reference_wrapper.
// Below C++17 the call goes through a private equivalent of std::invoke
// (Detail::invoke_projection); the same expectations hold from C++17, where
// it is std::invoke itself. The suites of every standard run this file.
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/functional/LumexFunctional.hpp"
#include "lumex/core/utility/ranges/LumexRanges.hpp"

namespace functional = lumex::core::utility::functional;
using lumex::core::utility::ranges::Algorithm::get_nearest_to;
namespace algorithm_detail = lumex::core::utility::ranges::Algorithm::Detail;

namespace
{
struct item
{
  int key;
  double weight;

  int
  get_key () const
  {
    return key;
  }

  int
  next_key ()
  {
    return key + 1;
  }
};

int
key_of (item const &element)
{
  return element.key;
}

struct key_functor
{
  int
  operator() (item const &element) const
  {
    return element.key;
  }
};

// A projection whose call operator is not const and that counts its calls
// outside itself (the algorithm may copy it).
struct counting_key
{
  std::size_t *calls;

  int
  operator() (item const &element)
  {
    ++*calls;
    return element.key;
  }
};

struct counting_less
{
  std::size_t *calls;

  bool
  operator() (int left, int right)
  {
    ++*calls;
    return left < right;
  }
};

std::vector<item>
make_items ()
{
  std::vector<item> items;
  int const keys[] = { 10, 20, 30, 40 };
  double const weights[] = { 1.5, 2.5, 3.5, 4.5 };
  for (std::size_t index = 0; index < 4; ++index)
    {
      item element = { keys[index], weights[index] };
      items.push_back (element);
    }
  return items;
}
} // namespace

// --- callables
// ----------------------------------------------------------------

TEST (LumexRangesProjectionTest,
      GivenFunctionPointer_WhenProjecting_ThenUsesIt)
{
  std::vector<item> const items = make_items ();
  auto const it = get_nearest_to (items, 26, functional::less (), &key_of);
  ASSERT_NE (it, items.end ());
  EXPECT_EQ (it->key, 30);
}

TEST (LumexRangesProjectionTest, GivenFunctor_WhenProjecting_ThenUsesIt)
{
  std::vector<item> const items = make_items ();
  auto const it
      = get_nearest_to (items, 21, functional::less (), key_functor ());
  ASSERT_NE (it, items.end ());
  EXPECT_EQ (it->key, 20);
}

TEST (LumexRangesProjectionTest, GivenStdFunction_WhenProjecting_ThenUsesIt)
{
  std::vector<item> const items = make_items ();
  std::function<int (item const &)> project
      = [] (item const &element) { return element.key * 2; };
  auto const it = get_nearest_to (items, 61, functional::less (), project);
  ASSERT_NE (it, items.end ());
  EXPECT_EQ (it->key, 30);
}

TEST (LumexRangesProjectionTest,
      GivenReferenceWrapperOfLambda_WhenProjecting_ThenUsesIt)
{
  std::vector<item> const items = make_items ();
  auto project = [] (item const &element) { return element.key; };
  auto const it
      = get_nearest_to (items, 39, functional::less (), std::ref (project));
  ASSERT_NE (it, items.end ());
  EXPECT_EQ (it->key, 40);
}

TEST (LumexRangesProjectionTest,
      GivenProjectionReturningReference_WhenProjecting_ThenWorks)
{
  std::vector<item> const items = make_items ();
  auto const it = get_nearest_to (items, 12, functional::less (),
                                  [] (item const &element) -> int const &
                                    { return element.key; });
  ASSERT_NE (it, items.end ());
  EXPECT_EQ (it->key, 10);
}

TEST (LumexRangesProjectionTest,
      GivenNonConstCallOperators_WhenProjectingAndComparing_ThenTheyAreCalled)
{
  std::vector<item> const items = make_items ();
  std::size_t projection_calls = 0;
  std::size_t comparison_calls = 0;
  counting_key project = { &projection_calls };
  counting_less less = { &comparison_calls };
  auto const it = get_nearest_to (items, 33, less, project);
  ASSERT_NE (it, items.end ());
  EXPECT_EQ (it->key, 30);
  EXPECT_GT (projection_calls, 0U);
  EXPECT_GT (comparison_calls, 0U);
}

// --- pointers to members
// --------------------------------------------------------

TEST (LumexRangesProjectionTest,
      GivenPointerToDataMember_WhenProjecting_ThenComparesTheMember)
{
  std::vector<item> const items = make_items ();
  auto const it = get_nearest_to (items, 26, functional::less (), &item::key);
  ASSERT_NE (it, items.end ());
  EXPECT_EQ (it->key, 30);
}

TEST (LumexRangesProjectionTest,
      GivenPointerToDoubleMember_WhenProjecting_ThenComparesTheMember)
{
  std::vector<item> const items = make_items ();
  auto const it
      = get_nearest_to (items, 2.9, functional::less (), &item::weight);
  ASSERT_NE (it, items.end ());
  EXPECT_DOUBLE_EQ (it->weight, 2.5);
}

TEST (LumexRangesProjectionTest,
      GivenPointerToConstMemberFunction_WhenProjecting_ThenCallsIt)
{
  std::vector<item> const items = make_items ();
  auto const it
      = get_nearest_to (items, 18, functional::less (), &item::get_key);
  ASSERT_NE (it, items.end ());
  EXPECT_EQ (it->key, 20);
}

TEST (LumexRangesProjectionTest,
      GivenPointerToNonConstMemberFunction_WhenProjecting_ThenCallsIt)
{
  // next_key () is not const: a non-const range gives non-const elements.
  std::vector<item> items = make_items ();
  auto const it
      = get_nearest_to (items, 32, functional::less (), &item::next_key);
  ASSERT_NE (it, items.end ());
  EXPECT_EQ (it->key, 30);
}

TEST (LumexRangesProjectionTest,
      GivenPointersToElements_WhenProjectingDataMember_ThenDereferences)
{
  std::vector<item> const objects = make_items ();
  std::vector<item const *> pointers;
  for (std::size_t index = 0; index < objects.size (); ++index)
    pointers.push_back (&objects[index]);
  auto const it
      = get_nearest_to (pointers, 38, functional::less (), &item::key);
  ASSERT_NE (it, pointers.end ());
  EXPECT_EQ ((*it)->key, 40);
}

TEST (LumexRangesProjectionTest,
      GivenSmartPointers_WhenProjectingDataMember_ThenDereferences)
{
  std::vector<std::unique_ptr<item>> owned;
  std::vector<std::shared_ptr<item>> shared;
  std::vector<item> const objects = make_items ();
  for (std::size_t index = 0; index < objects.size (); ++index)
    {
      owned.push_back (std::unique_ptr<item> (new item (objects[index])));
      shared.push_back (std::make_shared<item> (objects[index]));
    }
  auto const from_unique
      = get_nearest_to (owned, 22, functional::less (), &item::key);
  ASSERT_NE (from_unique, owned.end ());
  EXPECT_EQ ((*from_unique)->key, 20);
  auto const from_shared
      = get_nearest_to (shared, 31, functional::less (), &item::get_key);
  ASSERT_NE (from_shared, shared.end ());
  EXPECT_EQ ((*from_shared)->key, 30);
}

TEST (LumexRangesProjectionTest,
      GivenReferenceWrappers_WhenProjectingDataMember_ThenUnwraps)
{
  std::vector<item> objects = make_items ();
  std::vector<std::reference_wrapper<item>> wrapped;
  for (std::size_t index = 0; index < objects.size (); ++index)
    wrapped.push_back (std::ref (objects[index]));
  auto const it
      = get_nearest_to (wrapped, 14, functional::less (), &item::key);
  ASSERT_NE (it, wrapped.end ());
  EXPECT_EQ (it->get ().key, 10);
}

TEST (LumexRangesProjectionTest,
      GivenMapWithKeyMember_WhenProjectingFirst_ThenFindsNearestKey)
{
  typedef std::map<int, std::string> table_t;
  table_t table;
  table[10] = "ten";
  table[20] = "twenty";
  table[40] = "forty";
  auto const it = get_nearest_to (table, 33, functional::less (),
                                  &table_t::value_type::first);
  ASSERT_NE (it, table.end ());
  EXPECT_EQ (it->second, "forty");
  auto const lower = get_nearest_to (table, 29, functional::less (),
                                     &table_t::value_type::first);
  ASSERT_NE (lower, table.end ());
  EXPECT_EQ (lower->second, "twenty");
}

TEST (LumexRangesProjectionTest,
      GivenPairsWithMember_WhenProjectingFirst_ThenFindsNearest)
{
  typedef std::pair<double, std::string> entry_t;
  std::vector<entry_t> entries;
  entries.push_back (entry_t (0.5, "a"));
  entries.push_back (entry_t (1.5, "b"));
  entries.push_back (entry_t (3.0, "c"));
  auto const it
      = get_nearest_to (entries, 1.9, functional::less (), &entry_t::first);
  ASSERT_NE (it, entries.end ());
  EXPECT_EQ (it->second, "b");
}

// --- comparisons
// -------------------------------------------------------------------

TEST (LumexRangesProjectionTest,
      GivenStdGreaterAndPointerToMember_WhenDescending_ThenUsesBoth)
{
  std::vector<item> const ascending = make_items ();
  std::vector<item> const items (ascending.rbegin (), ascending.rend ());
  auto const it = get_nearest_to (items, 24, std::greater<int> (), &item::key);
  ASSERT_NE (it, items.end ());
  EXPECT_EQ (it->key, 20);
}

TEST (LumexRangesProjectionTest,
      GivenFunctionPointerComparison_WhenSearching_ThenUsesIt)
{
  std::vector<int> const values = { 1, 4, 9, 16 };
  bool (*const before) (int, int)
      = [] (int left, int right) { return left < right; };
  auto const it = get_nearest_to (values, 10, before);
  ASSERT_NE (it, values.end ());
  EXPECT_EQ (*it, 9);
}

TEST (LumexRangesProjectionTest,
      GivenMixedValueAndElementTypes_WhenSearching_ThenComparesByValue)
{
  std::vector<int> const integers = { 1, 4, 9, 16 };
  auto const from_double = get_nearest_to (integers, 6.4);
  ASSERT_NE (from_double, integers.end ());
  EXPECT_EQ (*from_double, 4);

  std::vector<double> const reals = { 0.5, 1.5, 2.5 };
  auto const from_int = get_nearest_to (reals, 2);
  ASSERT_NE (from_int, reals.end ());
  EXPECT_DOUBLE_EQ (*from_int, 1.5);

  std::vector<unsigned> const naturals = { 2U, 4U, 8U };
  auto const from_long = get_nearest_to (naturals, 7L);
  ASSERT_NE (from_long, naturals.end ());
  EXPECT_EQ (*from_long, 8U);
}

TEST (LumexRangesProjectionTest,
      GivenProjectionToFloat_WhenValueIsDouble_ThenWorks)
{
  std::vector<item> const items = make_items ();
  auto const it = get_nearest_to (
      items, 3.1, functional::less (), [] (item const &element)
        { return static_cast<float> (element.weight); });
  ASSERT_NE (it, items.end ());
  EXPECT_DOUBLE_EQ (it->weight, 3.5);
}

// --- Detail::invoke_projection
// ------------------------------------------------------------

TEST (LumexRangesInvokeProjectionTest, GivenCallables_WhenInvoked_ThenCalled)
{
  item element = { 7, 1.5 };
  key_functor functor;
  EXPECT_EQ (algorithm_detail::invoke_projection (functor, element), 7);
  int (*function) (item const &) = &key_of;
  EXPECT_EQ (algorithm_detail::invoke_projection (function, element), 7);
  auto lambda = [] (item const &object) { return object.key + 1; };
  EXPECT_EQ (algorithm_detail::invoke_projection (lambda, element), 8);
  functional::identity identity;
  EXPECT_EQ (&algorithm_detail::invoke_projection (identity, element),
             &element);
}

TEST (LumexRangesInvokeProjectionTest,
      GivenDataMemberPointer_WhenInvoked_ThenIsTheMemberOfTheObject)
{
  item element = { 7, 1.5 };
  item const constant = { 9, 2.5 };
  item *pointer = &element;
  int item::*const member = &item::key;

  // The result is the member itself, not a copy.
  EXPECT_EQ (&algorithm_detail::invoke_projection (member, element),
             &element.key);
  EXPECT_EQ (&algorithm_detail::invoke_projection (member, pointer),
             &element.key);
  EXPECT_EQ (algorithm_detail::invoke_projection (member, constant), 9);

  static_assert (std::is_same<decltype (algorithm_detail::invoke_projection (
                                  member, element)),
                              int &>::value,
                 "an lvalue object gives an lvalue member");
  static_assert (std::is_same<decltype (algorithm_detail::invoke_projection (
                                  member, constant)),
                              int const &>::value,
                 "a const object gives a const member");
  static_assert (std::is_same<decltype (algorithm_detail::invoke_projection (
                                  member, std::move (element))),
                              int &&>::value,
                 "an rvalue object gives an rvalue member");
  static_assert (std::is_same<decltype (algorithm_detail::invoke_projection (
                                  member, pointer)),
                              int &>::value,
                 "a pointer gives an lvalue member");
  SUCCEED ();
}

TEST (LumexRangesInvokeProjectionTest,
      GivenMemberFunctionPointer_WhenInvoked_ThenCallsItOnTheObject)
{
  item element = { 7, 1.5 };
  item const constant = { 9, 2.5 };
  int (item::*const get_key) () const = &item::get_key;
  int (item::*next_key) () = &item::next_key;
  EXPECT_EQ (algorithm_detail::invoke_projection (get_key, element), 7);
  EXPECT_EQ (algorithm_detail::invoke_projection (get_key, constant), 9);
  EXPECT_EQ (algorithm_detail::invoke_projection (next_key, element), 8);
  item *pointer = &element;
  EXPECT_EQ (algorithm_detail::invoke_projection (next_key, pointer), 8);
  std::reference_wrapper<item> wrapper = std::ref (element);
  EXPECT_EQ (algorithm_detail::invoke_projection (get_key, wrapper), 7);
  std::unique_ptr<item> owner (new item (element));
  EXPECT_EQ (algorithm_detail::invoke_projection (get_key, owner), 7);
}

TEST (LumexRangesInvokeProjectionTest,
      GivenConstMemberPointer_WhenInvoked_ThenStillAMemberCall)
{
  // The projection object itself may be const (a `T Class::* const`).
  item element = { 7, 1.5 };
  double item::*const weight = &item::weight;
  EXPECT_DOUBLE_EQ (algorithm_detail::invoke_projection (weight, element),
                    1.5);
}
