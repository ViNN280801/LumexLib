/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef LUMEX_TESTS_CORE_UTILITY_NUMERIC_HPP
#define LUMEX_TESTS_CORE_UTILITY_NUMERIC_HPP

#include <cstddef>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

// Value tables and an exact reference for the three-way comparison tests. The
// suites of several standards (LumexSafeThreeWayCompare.cxx11.tests.cpp and
// .cxx20) check the same values, so they live here. The reference never
// converts one integer to the type of the other: an integer is a sign and a
// magnitude, so every width and signedness compares exactly.

namespace lumex_numeric_samples
{
/// A list of types, for running a check over every type or pair of types.
template <typename... Ts> struct type_list
{
};

/// The integer types the safe comparator accepts in a mixed comparison (not
/// `bool`, whose `std::make_unsigned` does not exist, and not `char`, whose
/// signedness depends on the target).
using integer_types = type_list<signed char, unsigned char, short,
                                unsigned short, int, unsigned int, long,
                                unsigned long, long long, unsigned long long>;

/// The floating-point types.
using float_types = type_list<float, double, long double>;

/// An integer as a sign and a magnitude.
struct exact_int
{
  bool negative;
  unsigned long long magnitude;
};

template <typename T>
exact_int
make_exact_impl (T value, std::true_type /* signed */)
{
  exact_int result;
  result.negative = value < 0;
  // The conversion to unsigned long long is modular, so the negation is exact
  // for the most negative value too.
  result.magnitude = result.negative
                         ? 0ULL - static_cast<unsigned long long> (value)
                         : static_cast<unsigned long long> (value);
  return result;
}

template <typename T>
exact_int
make_exact_impl (T value, std::false_type /* unsigned */)
{
  exact_int result;
  result.negative = false;
  result.magnitude = static_cast<unsigned long long> (value);
  return result;
}

/// The sign and magnitude of an integer value.
template <typename T>
exact_int
make_exact (T value)
{
  return make_exact_impl (value, typename std::is_signed<T>::type ());
}

/// -1, 0 or 1: the exact order of two integers.
inline int
compare_exact (exact_int lhs, exact_int rhs)
{
  if (lhs.negative != rhs.negative)
    return lhs.negative ? -1 : 1;
  int const by_magnitude = lhs.magnitude < rhs.magnitude   ? -1
                           : lhs.magnitude > rhs.magnitude ? 1
                                                           : 0;
  return lhs.negative ? -by_magnitude : by_magnitude;
}

/// The code of the exact order of two integers of any types.
template <typename T, typename U>
int
integer_order (T lhs, U rhs)
{
  return compare_exact (make_exact (lhs), make_exact (rhs));
}

/// The order of two floating-point values by the language operators: -1, 0, 1,
/// or 2 when either is NaN. The operands widen to `long double` without loss.
inline int
float_order (long double lhs, long double rhs)
{
  if (lhs < rhs)
    return -1;
  if (lhs > rhs)
    return 1;
  if (lhs == rhs)
    return 0;
  return 2;
}

/// The code of an ordering: -1 less, 0 equivalent, 1 greater, 2 anything else
/// (unordered). It reads the ordering by comparing with its own named values.
template <typename Ordering>
int
ordering_code (Ordering const &value)
{
  if (value == Ordering::less)
    return -1;
  if (value == Ordering::greater)
    return 1;
  if (value == Ordering::equivalent)
    return 0;
  return 2;
}

/// Appends `candidate` to `values` if the integer type `T` holds it.
template <typename T>
void
add_positive (std::vector<T> &values, unsigned long long candidate)
{
  if (candidate
      <= static_cast<unsigned long long> ((std::numeric_limits<T>::max) ()))
    values.push_back (static_cast<T> (candidate));
}

/// Appends the negative `candidate` to `values` if `T` is signed and holds it.
template <typename T>
void
add_negative (std::vector<T> &values, long long candidate)
{
  if (std::is_signed<T>::value
      && candidate
             >= static_cast<long long> ((std::numeric_limits<T>::min) ()))
    values.push_back (static_cast<T> (candidate));
}

/// Values of an integer type: the limits and their neighbours, zero, the
/// small numbers around the limits of the narrower types, and the halves.
template <typename T>
std::vector<T>
integer_samples ()
{
  std::vector<T> values;
  T const lowest = (std::numeric_limits<T>::min) ();
  T const highest = (std::numeric_limits<T>::max) ();
  values.push_back (lowest);
  values.push_back (static_cast<T> (lowest + 1));
  values.push_back (static_cast<T> (highest - 1));
  values.push_back (highest);
  values.push_back (static_cast<T> (highest / 2));
  values.push_back (static_cast<T> (highest / 2 + 1));
  values.push_back (static_cast<T> (lowest / 2));
  values.push_back (static_cast<T> (lowest / 2 - 1));

  unsigned long long const positives[] = { 0ULL,
                                           1ULL,
                                           2ULL,
                                           100ULL,
                                           126ULL,
                                           127ULL,
                                           128ULL,
                                           129ULL,
                                           254ULL,
                                           255ULL,
                                           256ULL,
                                           257ULL,
                                           32766ULL,
                                           32767ULL,
                                           32768ULL,
                                           32769ULL,
                                           65534ULL,
                                           65535ULL,
                                           65536ULL,
                                           65537ULL,
                                           2147483646ULL,
                                           2147483647ULL,
                                           2147483648ULL,
                                           2147483649ULL,
                                           4294967294ULL,
                                           4294967295ULL,
                                           4294967296ULL,
                                           4294967297ULL,
                                           9223372036854775806ULL,
                                           9223372036854775807ULL,
                                           9223372036854775808ULL,
                                           9223372036854775809ULL,
                                           18446744073709551614ULL,
                                           18446744073709551615ULL };
  long long const negatives[] = { -1LL,
                                  -2LL,
                                  -100LL,
                                  -127LL,
                                  -128LL,
                                  -129LL,
                                  -32767LL,
                                  -32768LL,
                                  -32769LL,
                                  -2147483647LL,
                                  -2147483648LL,
                                  -2147483649LL,
                                  -4294967296LL,
                                  -9223372036854775807LL,
                                  -9223372036854775807LL - 1LL };
  for (std::size_t i = 0; i < sizeof (positives) / sizeof (positives[0]); ++i)
    add_positive (values, positives[i]);
  for (std::size_t i = 0; i < sizeof (negatives) / sizeof (negatives[0]); ++i)
    add_negative (values, negatives[i]);
  return values;
}

/// Values of a floating-point type: both infinities, NaN, both zeros, the
/// limits of the type, a denormal, and a few ordinary numbers.
template <typename F>
std::vector<F>
float_samples ()
{
  std::vector<F> values;
  values.push_back (-std::numeric_limits<F>::infinity ());
  values.push_back (-(std::numeric_limits<F>::max) ());
  values.push_back (static_cast<F> (-1.5));
  values.push_back (static_cast<F> (-1.0));
  values.push_back (static_cast<F> (-0.5));
  values.push_back (-(std::numeric_limits<F>::min) ());
  values.push_back (-std::numeric_limits<F>::denorm_min ());
  values.push_back (static_cast<F> (-0.0));
  values.push_back (static_cast<F> (0.0));
  values.push_back (std::numeric_limits<F>::denorm_min ());
  values.push_back ((std::numeric_limits<F>::min) ());
  values.push_back (static_cast<F> (0.5));
  values.push_back (static_cast<F> (1.0));
  values.push_back (static_cast<F> (1.5));
  values.push_back ((std::numeric_limits<F>::max) ());
  values.push_back (std::numeric_limits<F>::infinity ());
  values.push_back (std::numeric_limits<F>::quiet_NaN ());
  return values;
}

/// Finite values of a floating-point type (the infinities and NaN left out).
template <typename F>
std::vector<F>
finite_float_samples ()
{
  std::vector<F> values;
  std::vector<F> const all = float_samples<F> ();
  for (std::size_t i = 0; i < all.size (); ++i)
    if (all[i] == all[i] && all[i] != std::numeric_limits<F>::infinity ()
        && all[i] != -std::numeric_limits<F>::infinity ())
      values.push_back (all[i]);
  return values;
}

/// Integers of type `T` a floating-point type `F` holds exactly (no more than
/// 2^24 in magnitude for `float`, 2^53 for the others), so the exact order of
/// an integer and a floating-point value is the order of the two as `long
/// double`.
template <typename T, typename F>
std::vector<T>
exactly_representable_integers ()
{
  unsigned long long const limit
      = std::numeric_limits<F>::digits >= 53 ? (1ULL << 53) : (1ULL << 24);
  std::vector<T> values;
  std::vector<T> const all = integer_samples<T> ();
  for (std::size_t i = 0; i < all.size (); ++i)
    {
      exact_int const exact = make_exact (all[i]);
      if (exact.magnitude <= limit)
        values.push_back (all[i]);
    }
  // Numbers near the limit of the floating-point type and a few small ones,
  // wherever the integer type has them.
  add_positive (values, limit);
  add_positive (values, limit - 1ULL);
  add_positive (values, 3ULL);
  add_positive (values, 41ULL);
  add_positive (values, 42ULL);
  add_negative (values, -3LL);
  add_negative (values, -42LL);
  add_negative (values, -static_cast<long long> (limit));
  add_negative (values, -static_cast<long long> (limit) + 1LL);
  return values;
}

/// Floating-point values used against integers: ordinary numbers around the
/// integer samples, the infinities and NaN.
template <typename F>
std::vector<F>
mixed_float_samples ()
{
  std::vector<F> values = float_samples<F> ();
  values.push_back (static_cast<F> (2.5));
  values.push_back (static_cast<F> (41.5));
  values.push_back (static_cast<F> (42.0));
  values.push_back (static_cast<F> (-42.0));
  values.push_back (static_cast<F> (127.5));
  values.push_back (static_cast<F> (128.0));
  values.push_back (static_cast<F> (255.5));
  values.push_back (static_cast<F> (256.0));
  values.push_back (static_cast<F> (-128.5));
  values.push_back (static_cast<F> (32767.5));
  values.push_back (static_cast<F> (65535.5));
  values.push_back (static_cast<F> (16777216.0));
  values.push_back (static_cast<F> (-16777216.0));
  values.push_back (static_cast<F> (16777215.0));
  values.push_back (static_cast<F> (9007199254740992.0));
  values.push_back (static_cast<F> (-9007199254740992.0));
  return values;
}

/// Runs `Check<T, U>::run (report)` for every `U` of a list, `T` fixed.
template <template <typename, typename> class Check, typename T, typename List,
          typename Report>
struct for_each_second;

template <template <typename, typename> class Check, typename T,
          typename Report>
struct for_each_second<Check, T, type_list<>, Report>
{
  static void
  run (Report &)
  {
  }
};

template <template <typename, typename> class Check, typename T, typename U,
          typename... Rest, typename Report>
struct for_each_second<Check, T, type_list<U, Rest...>, Report>
{
  static void
  run (Report &report)
  {
    Check<T, U>::run (report);
    for_each_second<Check, T, type_list<Rest...>, Report>::run (report);
  }
};

/// Runs `Check<T, U>::run (report)` for every `T` of the first list and every
/// `U` of the second.
template <template <typename, typename> class Check, typename FirstList,
          typename SecondList, typename Report>
struct for_each_pair;

template <template <typename, typename> class Check, typename SecondList,
          typename Report>
struct for_each_pair<Check, type_list<>, SecondList, Report>
{
  static void
  run (Report &)
  {
  }
};

template <template <typename, typename> class Check, typename SecondList,
          typename Report, typename T, typename... Ts>
struct for_each_pair<Check, type_list<T, Ts...>, SecondList, Report>
{
  static void
  run (Report &report)
  {
    for_each_second<Check, T, SecondList, Report>::run (report);
    for_each_pair<Check, type_list<Ts...>, SecondList, Report>::run (report);
  }
};

/// A readable text of a value for a failure message.
template <typename T>
std::string
describe (T value)
{
  std::ostringstream stream;
  stream << std::setprecision (21) << +value;
  return stream.str ();
}

/// Counts the mismatches of a check that runs over many values and keeps the
/// text of the first ones, so a failure names the values and not only a count.
class mismatch_report
{
public:
  mismatch_report () : m_count (0) {}

  void
  add (std::string const &message)
  {
    ++m_count;
    if (m_messages.size () < 6)
      m_messages.push_back (message);
  }

  /// The number of mismatches.
  std::size_t
  count () const
  {
    return m_count;
  }

  /// The first mismatches, one per line.
  std::string
  text () const
  {
    std::string result;
    for (std::size_t i = 0; i < m_messages.size (); ++i)
      result += m_messages[i] + "\n";
    return result;
  }

private:
  std::size_t m_count;
  std::vector<std::string> m_messages;
};

} // namespace lumex_numeric_samples

#endif // !LUMEX_TESTS_CORE_UTILITY_NUMERIC_HPP
