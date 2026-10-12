// The safe comparator under -Wfloat-equal, built with -Wall -Wextra
// -Wfloat-equal -Werror by cmake.hygiene_compile_checks (GCC and Clang): a
// float or double compared for equality inside the header must not warn in the
// translation unit of the consumer. The header calls
// lumex::core::math::ops::exactly_equal there, so it needs no pragma of its
// own (GCC has no way to silence the warning from a header outside a system
// header except a pragma around every comparison).
//
// Every function below instantiates the comparisons of the header for the
// floating-point types: the identical floating-point types, two different
// ones, an integer against a floating-point type and back, the three-way
// compare, and the equality of the compare-and-set of a plain comparator. The
// functions are external so that nothing is optimized away before the
// instantiation.
//
// With -DLUMEX_HYGIENE_RAW_FLOAT_EQUAL the unit also compares two doubles with
// a raw `==` of its own, which must be rejected: that proves the flags make
// -Wfloat-equal an error on this compiler, so the good case proves something.

#include "lumex/core/utility/numeric/LumexSafeNumericComparator.hpp"

using namespace lumex::core::utility::numeric;

template <typename T, typename U>
bool
every_comparison (T lhs, U rhs)
{
  bool result = safe_equal (lhs, rhs);
  result = safe_not_equal (lhs, rhs) || result;
  result = safe_less (lhs, rhs) || result;
  result = safe_less_equal (lhs, rhs) || result;
  result = safe_greater (lhs, rhs) || result;
  result = safe_greater_equal (lhs, rhs) || result;
  result = safe_compare (lhs, rhs) || result;
  result = is_equal (safe_three_way_compare (lhs, rhs)) || result;
  result = is_less (safe_three_way_compare (lhs, rhs)) || result;

  safe_comparator<T, false> const plain (lhs);
  result = plain.safe_equal (rhs) || result;
  result = plain.safe_not_equal (rhs) || result;
  result = is_equal (plain.safe_three_way_compare (rhs)) || result;
  return result;
}

template <typename T>
bool
compare_and_set_plain (T expected, T desired)
{
  safe_comparator<T, false> plain (expected);
  return plain.compare_and_set (expected, desired);
}

bool
safe_equal_double (double lhs, double rhs)
{
  return safe_equal (lhs, rhs);
}

bool
safe_not_equal_double (double lhs, double rhs)
{
  return safe_not_equal (lhs, rhs);
}

bool
safe_three_way_float (float lhs, float rhs)
{
  return is_equal (safe_three_way_compare (lhs, rhs));
}

bool
every_float_pair (float f, double d, long double l, int i,
                  unsigned long long u)
{
  bool result = every_comparison (f, f) || every_comparison (d, d)
                || every_comparison (l, l);
  result = every_comparison (f, d) || every_comparison (d, f) || result;
  result = every_comparison (d, l) || every_comparison (l, d) || result;
  result = every_comparison (f, l) || every_comparison (l, f) || result;
  result = every_comparison (i, f) || every_comparison (f, i) || result;
  result = every_comparison (i, d) || every_comparison (d, i) || result;
  result = every_comparison (u, l) || every_comparison (l, u) || result;
  return result;
}

bool
every_compare_and_set (float f, double d, long double l)
{
  return compare_and_set_plain (f, f) || compare_and_set_plain (d, d)
         || compare_and_set_plain (l, l);
}

#if defined(LUMEX_HYGIENE_RAW_FLOAT_EQUAL)
bool
raw_float_equal (double lhs, double rhs)
{
  return lhs == rhs;
}
#endif
