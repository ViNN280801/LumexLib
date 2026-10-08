// LumexSafeEqualExactly.cxx11.tests.cpp
// safe_equal and safe_not_equal agree with lumex::core::math::ops::
// exactly_equal, the one place of the library that spells a floating-point
// `==` (so that -Wfloat-equal stays quiet in a consumer): over every pair of
// the values 0, -0, 1, -1, 1.5, +inf, -inf and two different NaNs (81 pairs)
// for float, double and long double; for two different floating-point types
// (the narrower value widens to the wider one without loss); and for an
// integer against a floating-point type, both ways. The test passed with the
// raw `==` that the header had before and must pass with the call, which is
// the point: the call changes the spelling and not one answer.

#include <cstddef>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/math/ops/LumexMath.hpp"
#include "lumex/core/utility/numeric/LumexSafeNumericComparator.hpp"
#include "lumex/tests/core/utility/numeric/LumexNumericSamples.hpp"

using namespace lumex::core::utility::numeric;
using namespace lumex_numeric_samples;
using lumex::core::math::ops::exactly_equal;

namespace
{
// 0, -0, 1, -1, 1.5, +inf, -inf, a NaN and a NaN of the other sign.
template <typename F>
std::vector<F>
the_nine_values ()
{
  std::vector<F> values;
  values.push_back (static_cast<F> (0.0));
  values.push_back (static_cast<F> (-0.0));
  values.push_back (static_cast<F> (1.0));
  values.push_back (static_cast<F> (-1.0));
  values.push_back (static_cast<F> (1.5));
  values.push_back (std::numeric_limits<F>::infinity ());
  values.push_back (-std::numeric_limits<F>::infinity ());
  values.push_back (std::numeric_limits<F>::quiet_NaN ());
  values.push_back (-std::numeric_limits<F>::quiet_NaN ());
  return values;
}

template <typename T, typename U>
void
expect_agreement (mismatch_report &report, T lhs, U rhs, bool exact)
{
  if (safe_equal (lhs, rhs) != exact)
    report.add (std::string ("safe_equal: ") + describe (lhs) + " "
                + describe (rhs) + " expected " + (exact ? "true" : "false"));
  if (safe_not_equal (lhs, rhs) != !exact)
    report.add (std::string ("safe_not_equal: ") + describe (lhs) + " "
                + describe (rhs) + " expected " + (exact ? "false" : "true"));
  if (safe_comparator<T, false> (lhs).safe_equal (rhs) != exact)
    report.add (std::string ("member safe_equal: ") + describe (lhs) + " "
                + describe (rhs));
  if (safe_comparator<T, false> (lhs).safe_not_equal (rhs) != !exact)
    report.add (std::string ("member safe_not_equal: ") + describe (lhs) + " "
                + describe (rhs));
}

// Two values of one floating-point type: exactly_equal itself.
template <typename F> struct same_type_check
{
  static void
  run (mismatch_report &report)
  {
    std::vector<F> const values = the_nine_values<F> ();
    for (std::size_t i = 0; i < values.size (); ++i)
      for (std::size_t j = 0; j < values.size (); ++j)
        expect_agreement (report, values[i], values[j],
                          exactly_equal (values[i], values[j]));
  }
};

// Two floating-point types: both widen to long double without rounding, so
// exactly_equal on the widened values is the reference.
template <typename F, typename G> struct two_types_check
{
  static void
  run (mismatch_report &report)
  {
    std::vector<F> const lhs = the_nine_values<F> ();
    std::vector<G> const rhs = the_nine_values<G> ();
    for (std::size_t i = 0; i < lhs.size (); ++i)
      for (std::size_t j = 0; j < rhs.size (); ++j)
        expect_agreement (report, lhs[i], rhs[j],
                          exactly_equal (static_cast<long double> (lhs[i]),
                                         static_cast<long double> (rhs[j])));
  }
};

// An integer against a floating-point type, both ways: 0, 1 and -1 are exact
// in every floating-point type, so the integer converted to it is the
// reference (an infinity and a NaN never equal an integer, and exactly_equal
// says so).
template <typename I, typename F> struct integer_float_check
{
  static void
  run (mismatch_report &report)
  {
    std::vector<I> integers;
    integers.push_back (static_cast<I> (0));
    integers.push_back (static_cast<I> (1));
    if (std::is_signed<I>::value)
      integers.push_back (static_cast<I> (-1));
    std::vector<F> const floats = the_nine_values<F> ();
    for (std::size_t i = 0; i < integers.size (); ++i)
      for (std::size_t j = 0; j < floats.size (); ++j)
        {
          bool const exact
              = exactly_equal (static_cast<F> (integers[i]), floats[j]);
          expect_agreement (report, integers[i], floats[j], exact);
          expect_agreement (report, floats[j], integers[i], exact);
        }
  }
};

template <template <typename> class Check, typename List> struct run_each;

template <template <typename> class Check> struct run_each<Check, type_list<>>
{
  static void
  run (mismatch_report &)
  {
  }
};

template <template <typename> class Check, typename T, typename... Rest>
struct run_each<Check, type_list<T, Rest...>>
{
  static void
  run (mismatch_report &report)
  {
    Check<T>::run (report);
    run_each<Check, type_list<Rest...>>::run (report);
  }
};
} // namespace

TEST (LumexSafeEqualExactly,
      GivenTheNineValues_WhenOneFloatingType_ThenSafeEqualIsExactlyEqual)
{
  mismatch_report report;
  run_each<same_type_check, float_types>::run (report);
  EXPECT_EQ (report.count (), 0U) << report.text ();
}

TEST (LumexSafeEqualExactly,
      GivenTheNineValues_WhenTwoFloatingTypes_ThenSafeEqualIsExactlyEqual)
{
  mismatch_report report;
  for_each_pair<two_types_check, float_types, float_types,
                mismatch_report>::run (report);
  EXPECT_EQ (report.count (), 0U) << report.text ();
}

TEST (LumexSafeEqualExactly,
      GivenIntegerAndFloat_WhenComparedBothWays_ThenSafeEqualIsExactlyEqual)
{
  mismatch_report report;
  for_each_pair<integer_float_check, integer_types, float_types,
                mismatch_report>::run (report);
  EXPECT_EQ (report.count (), 0U) << report.text ();
}

TEST (LumexSafeEqualExactly, GivenTheNineValues_WhenCounted_ThenTheTableIs81)
{
  // The table is the 81 pairs of the nine values: 9 x 9.
  std::vector<double> const values = the_nine_values<double> ();
  EXPECT_EQ (values.size () * values.size (), 81U);
  std::size_t equal_pairs = 0;
  for (std::size_t i = 0; i < values.size (); ++i)
    for (std::size_t j = 0; j < values.size (); ++j)
      if (safe_equal (values[i], values[j]))
        ++equal_pairs;
  // 0 and -0 equal each other and themselves (4 pairs); 1, -1, 1.5, +inf and
  // -inf equal themselves (5 pairs); a NaN equals nothing, itself included.
  EXPECT_EQ (equal_pairs, 9U);
}

// The plain comparator's compare_and_set tests the stored value with the same
// exact equality.
template <typename F>
void
expect_plain_compare_and_set ()
{
  F const nan_value = std::numeric_limits<F>::quiet_NaN ();
  F const infinity = std::numeric_limits<F>::infinity ();

  // -0 equals 0, so the swap happens.
  safe_comparator<F, false> zero (static_cast<F> (0.0));
  EXPECT_TRUE (
      zero.compare_and_set (static_cast<F> (-0.0), static_cast<F> (2.0)));
  EXPECT_TRUE (exactly_equal (zero.get (), static_cast<F> (2.0)));

  // A different value does not swap.
  EXPECT_FALSE (
      zero.compare_and_set (static_cast<F> (1.5), static_cast<F> (3.0)));
  EXPECT_TRUE (exactly_equal (zero.get (), static_cast<F> (2.0)));

  // A NaN equals nothing, a stored NaN included.
  safe_comparator<F, false> stored_nan (nan_value);
  EXPECT_FALSE (stored_nan.compare_and_set (nan_value, static_cast<F> (1.0)));
  EXPECT_FALSE (zero.compare_and_set (nan_value, static_cast<F> (1.0)));

  // An infinity equals the same infinity only.
  safe_comparator<F, false> stored_inf (infinity);
  EXPECT_FALSE (stored_inf.compare_and_set (-infinity, static_cast<F> (1.0)));
  EXPECT_TRUE (stored_inf.compare_and_set (infinity, static_cast<F> (1.0)));
  EXPECT_TRUE (exactly_equal (stored_inf.get (), static_cast<F> (1.0)));
}

TEST (LumexSafeEqualExactly,
      GivenPlainFloatingComparator_WhenCompareAndSet_ThenExactEquality)
{
  expect_plain_compare_and_set<float> ();
  expect_plain_compare_and_set<double> ();
  expect_plain_compare_and_set<long double> ();
}
