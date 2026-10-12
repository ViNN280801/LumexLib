// LumexRangesOracle.cxx11.tests.cpp
//
// get_nearest_to against an oracle that does not search: it scans the
// elements and keeps the nearest, and on equal distances the smaller. Every
// sorted vector of up to six elements over a small set of values is checked
// with every query between them, for integers and for floating-point values,
// and a seeded pseudo-random run covers long ranges and the containers that
// are not random access. The search must also be a bisection: a counting
// comparison bounds the number of comparisons.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <list>
#include <set>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/ranges/LumexRanges.hpp"

using lumex::core::utility::ranges::Algorithm::get_nearest_to;

namespace
{
// The nearest element of a sorted sequence by a scan, as a position. Equal
// distances go to the smaller value, which is the earlier one in an
// ascending sequence; among equal values the first one is taken, except that
// get_nearest_to takes the last of the equal elements below the value (the
// predecessor of the first element not below it), so the tests compare the
// value and, where the values are distinct, the position.
template <typename Sequence, typename Value>
std::size_t
nearest_position (Sequence const &sorted, Value value)
{
  std::size_t best = 0;
  double best_distance = std::fabs (static_cast<double> (sorted[0])
                                    - static_cast<double> (value));
  for (std::size_t index = 1; index < sorted.size (); ++index)
    {
      double const distance = std::fabs (static_cast<double> (sorted[index])
                                         - static_cast<double> (value));
      if (distance < best_distance)
        {
          best = index;
          best_distance = distance;
        }
    }
  return best;
}

// Calls the visitor with every non-decreasing sequence of `length` values
// taken from `values`, in lexicographic order.
template <typename Element, typename Visitor>
void
for_each_sorted (std::vector<Element> const &values, std::size_t length,
                 std::vector<Element> &current, std::size_t from,
                 Visitor &visit)
{
  if (current.size () == length)
    {
      visit (current);
      return;
    }
  for (std::size_t index = from; index < values.size (); ++index)
    {
      current.push_back (values[index]);
      for_each_sorted (values, length, current, index, visit);
      current.pop_back ();
    }
}

template <typename Element> struct check_every_query
{
  std::vector<Element> const &queries;
  std::size_t sequences;
  std::size_t queries_checked;
  std::size_t mismatches;

  void
  operator() (std::vector<Element> const &sorted)
  {
    ++sequences;
    for (std::size_t q = 0; q < queries.size (); ++q)
      {
        ++queries_checked;
        typename std::vector<Element>::const_iterator const found
            = get_nearest_to (sorted, queries[q]);
        if (sorted.empty ())
          {
            if (found != sorted.end ())
              ++mismatches;
            continue;
          }
        if (found == sorted.end ())
          {
            ++mismatches;
            continue;
          }
        std::size_t const expected = nearest_position (sorted, queries[q]);
        // The same value as the oracle's; the same position as well when no
        // value is repeated (with repeated values either of them is an
        // element of the nearest group).
        bool const same_value = *found == sorted[expected];
        if (!same_value)
          ++mismatches;
      }
  }
};
} // namespace

TEST (LumexRangesOracleTest,
      GivenEverySortedIntVectorUpToSix_WhenQueried_ThenMatchesTheScan)
{
  std::vector<int> values;
  for (int value = 0; value <= 4; ++value)
    values.push_back (value * 3);
  std::vector<int> queries;
  for (int query = -2; query <= 14; ++query)
    queries.push_back (query);

  check_every_query<int> check = { queries, 0, 0, 0 };
  for (std::size_t length = 0; length <= 6; ++length)
    {
      std::vector<int> current;
      for_each_sorted (values, length, current, 0, check);
    }
  // C(n + 4, 4) sequences of each length over five values: 1, 5, 15, 35,
  // 70, 126, 210.
  EXPECT_EQ (check.sequences, 462U);
  EXPECT_EQ (check.queries_checked, 462U * queries.size ());
  EXPECT_EQ (check.mismatches, 0U);
}

TEST (LumexRangesOracleTest,
      GivenEverySortedDoubleVectorUpToFive_WhenQueried_ThenMatchesTheScan)
{
  std::vector<double> values;
  values.push_back (-1.5);
  values.push_back (0.0);
  values.push_back (0.25);
  values.push_back (2.0);
  values.push_back (3.75);
  std::vector<double> queries;
  for (int step = -12; step <= 36; ++step)
    queries.push_back (step * 0.125);

  check_every_query<double> check = { queries, 0, 0, 0 };
  for (std::size_t length = 0; length <= 5; ++length)
    {
      std::vector<double> current;
      for_each_sorted (values, length, current, 0, check);
    }
  EXPECT_EQ (check.sequences, 252U);
  EXPECT_EQ (check.mismatches, 0U);
}

TEST (LumexRangesOracleTest,
      GivenDistinctValues_WhenQueried_ThenTheIteratorIsTheScanPosition)
{
  std::vector<int> const sorted = { -9, -4, 0, 5, 11, 12, 30 };
  for (int query = -15; query <= 40; ++query)
    {
      std::vector<int>::const_iterator const found
          = get_nearest_to (sorted, query);
      ASSERT_NE (found, sorted.end ());
      EXPECT_EQ (static_cast<std::size_t> (found - sorted.begin ()),
                 nearest_position (sorted, query))
          << "query " << query;
    }
}

namespace
{
// A small deterministic generator, so that the run is the same everywhere.
struct lcg
{
  std::uint64_t state;

  std::uint32_t
  next ()
  {
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    return static_cast<std::uint32_t> (state >> 33);
  }
};

std::vector<int>
random_sorted (lcg &random, std::size_t count, std::uint32_t spread)
{
  std::vector<int> values;
  int current = static_cast<int> (random.next () % 1000U) - 500;
  for (std::size_t index = 0; index < count; ++index)
    {
      values.push_back (current);
      current += static_cast<int> (random.next () % spread);
    }
  return values;
}
} // namespace

TEST (
    LumexRangesOracleTest,
    GivenRandomSortedRanges_WhenQueriedOnEveryContainer_ThenAllAgreeWithTheScan)
{
  lcg random = { 12345 };
  std::size_t mismatches = 0;
  std::size_t checked = 0;
  for (int round = 0; round < 60; ++round)
    {
      std::size_t const count = 1 + random.next () % 400U;
      std::vector<int> const sorted
          = random_sorted (random, count, 1U + random.next () % 20U);
      std::list<int> const as_list (sorted.begin (), sorted.end ());
      std::deque<int> const as_deque (sorted.begin (), sorted.end ());
      std::set<int> const as_set (sorted.begin (), sorted.end ());
      std::vector<int> const unique_sorted (as_set.begin (), as_set.end ());
      for (int query = 0; query < 40; ++query)
        {
          int const value = static_cast<int> (random.next () % 1200U) - 600;
          std::size_t const expected = nearest_position (sorted, value);
          ++checked;
          if (*get_nearest_to (sorted, value) != sorted[expected])
            ++mismatches;
          if (*get_nearest_to (as_list, value) != sorted[expected])
            ++mismatches;
          if (*get_nearest_to (as_deque, value) != sorted[expected])
            ++mismatches;
          // A set holds the distinct values: its nearest is the scan's.
          if (*get_nearest_to (as_set, value)
              != unique_sorted[nearest_position (unique_sorted, value)])
            ++mismatches;
          if (*get_nearest_to (sorted.begin (), sorted.end (), value)
              != sorted[expected])
            ++mismatches;
        }
    }
  EXPECT_EQ (checked, 2400U);
  EXPECT_EQ (mismatches, 0U);
}

TEST (
    LumexRangesOracleTest,
    GivenRepeatedValues_WhenValueIsPresent_ThenTheFirstEqualElementIsReturned)
{
  std::vector<int> const sorted = { 1, 5, 5, 5, 9 };
  std::vector<int>::const_iterator const found = get_nearest_to (sorted, 5);
  ASSERT_NE (found, sorted.end ());
  EXPECT_EQ (found - sorted.begin (), 1);
}

TEST (LumexRangesOracleTest,
      GivenRepeatedValuesBelowTheQuery_WhenTied_ThenTheLastOfThemIsReturned)
{
  // 5 is as far from 4 as from 6; the predecessor of the first element not
  // below 5 wins the tie, and that is the second 4.
  std::vector<int> const sorted = { 4, 4, 6 };
  std::vector<int>::const_iterator const found = get_nearest_to (sorted, 5);
  ASSERT_NE (found, sorted.end ());
  EXPECT_EQ (found - sorted.begin (), 1);
}

TEST (LumexRangesOracleTest,
      GivenQueryOutsideTheRange_WhenQueried_ThenTheNearEndIsReturned)
{
  std::vector<int> const sorted = { 3, 6, 9 };
  EXPECT_EQ (get_nearest_to (sorted, -1000), sorted.begin ());
  EXPECT_EQ (get_nearest_to (sorted, 3), sorted.begin ());
  EXPECT_EQ (get_nearest_to (sorted, 9), sorted.begin () + 2);
  EXPECT_EQ (get_nearest_to (sorted, 1000), sorted.begin () + 2);
}

TEST (LumexRangesOracleTest,
      GivenValuesNearTheLimits_WhenQueried_ThenTheNearestIsFound)
{
  std::vector<int> const sorted = { INT32_MIN, 0, INT32_MAX };
  EXPECT_EQ (*get_nearest_to (sorted, -1), 0);
  EXPECT_EQ (*get_nearest_to (sorted, INT32_MAX - 5), INT32_MAX);
  EXPECT_EQ (*get_nearest_to (sorted, INT32_MIN + 5), INT32_MIN);
  std::vector<unsigned> const unsigned_sorted = { 0U, 4000000000U };
  EXPECT_EQ (*get_nearest_to (unsigned_sorted, 3000000000U), 4000000000U);
  EXPECT_EQ (*get_nearest_to (unsigned_sorted, 1999999999U), 0U);
}

// --- the search is a bisection
// --------------------------------------------------

namespace
{
struct counting_less
{
  std::size_t *comparisons;

  bool
  operator() (int left, int right) const
  {
    ++*comparisons;
    return left < right;
  }
};
} // namespace

TEST (LumexRangesOracleTest,
      GivenLongRange_WhenQueried_ThenTheComparisonsAreLogarithmic)
{
  std::vector<int> sorted;
  for (int value = 0; value < 4096; ++value)
    sorted.push_back (value * 2);
  std::size_t comparisons = 0;
  counting_less const less = { &comparisons };
  std::vector<int>::const_iterator const found
      = get_nearest_to (sorted, 5001, less);
  ASSERT_NE (found, sorted.end ());
  EXPECT_EQ (*found, 5000);
  // 4096 elements: a bisection needs 12 comparisons; a scan 2500.
  EXPECT_GT (comparisons, 0U);
  EXPECT_LE (comparisons, 14U);
}

TEST (LumexRangesOracleTest,
      GivenList_WhenQueried_ThenTheComparisonsAreStillLogarithmic)
{
  std::list<int> sorted;
  for (int value = 0; value < 1024; ++value)
    sorted.push_back (value * 2);
  std::size_t comparisons = 0;
  counting_less const less = { &comparisons };
  std::list<int>::const_iterator const found
      = get_nearest_to (sorted, 1001, less);
  ASSERT_NE (found, sorted.end ());
  EXPECT_EQ (*found, 1000);
  EXPECT_LE (comparisons, 12U);
}

TEST (LumexRangesOracleTest,
      GivenTwoElementRange_WhenQueriedAtTheMiddle_ThenTheSmallerWins)
{
  std::vector<int> const sorted = { 10, 20 };
  EXPECT_EQ (*get_nearest_to (sorted, 15), 10);
  std::vector<double> const reals = { -0.5, 0.5 };
  EXPECT_DOUBLE_EQ (*get_nearest_to (reals, 0.0), -0.5);
}
