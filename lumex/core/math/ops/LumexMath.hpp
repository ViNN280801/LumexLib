/**
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

/**
 * @file LumexMath.hpp
 * @brief Numeric helpers: NaN/Inf checks, checked narrowing casts, distance,
 *        squared difference, and range-based avg/rms/rmse.
 * @details Works from C++11 on. A "range" is anything `begin (r)` / `end (r)`
 *          accept (found through `std::begin` / `std::end` or ADL): standard
 *          containers, C arrays, `std::initializer_list`, and from C++20 the
 *          `std::views` adaptors, including views that are not const-iterable
 *          (`std::views::filter`) and ranges whose end is a sentinel of
 *          another type. Every range is traversed once, so the range only
 *          needs input iterators. Numeric types are the integral types except
 *          `bool`, and the floating-point types; overloads outside that set
 *          do not take part in overload resolution (SFINAE).
 */
#ifndef LUMEX_CORE_MATH_OPS_HPP
#define LUMEX_CORE_MATH_OPS_HPP

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexExceptionMacros.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace math
{
namespace ops
{
/// @brief Thrown by the two-range `rms` / `rmse` when the ranges have
/// different lengths, one of them empty included. Derives from
/// `std::invalid_argument`, so handlers of that type still catch it.
LUMEX_DEFINE_EXCEPTION (LumexMathSizeMismatchException, std::invalid_argument)

namespace traits
{
/// @brief A "real" numeric type: any integral type except `bool`, or any
/// floating-point type (cv-qualifiers ignored).
template <typename T>
struct is_numeric
    : std::integral_constant<
          bool,
          (std::is_integral<T>::value
           && !std::is_same<typename std::remove_cv<T>::type, bool>::value)
              || std::is_floating_point<T>::value>
{
};

#if __cplusplus >= 202002L
/// @brief C++20 spelling of @ref is_numeric.
template <typename T>
concept NumericConcept = is_numeric<T>::value;
#endif
} // namespace traits

namespace Detail
{
template <typename...> struct voider
{
  typedef void type;
};

using std::begin;
using std::end;

/// @brief `begin (range)` with `std::begin` and ADL, as a range-for does.
template <typename Range>
auto
adl_begin (Range &range) -> decltype (begin (range))
{
  return begin (range);
}

/// @brief `end (range)` with `std::end` and ADL, as a range-for does.
template <typename Range>
auto
adl_end (Range &range) -> decltype (end (range))
{
  return end (range);
}

/// @brief `Range` without reference, as the lvalue the functions iterate.
template <typename Range> struct range_object
{
  typedef typename std::remove_reference<Range>::type type;
};

/// @brief True when `Range` can be iterated as an lvalue and its elements
/// decay to a numeric type.
template <typename Range, typename = void>
struct is_numeric_range : std::false_type
{
};

template <typename Range>
struct is_numeric_range<
    Range,
    typename voider<
        decltype (adl_begin (
                      std::declval<typename range_object<Range>::type &> ())
                  != adl_end (
                      std::declval<typename range_object<Range>::type &> ())),
        decltype (++std::declval<decltype (adl_begin (
                      std::declval<typename range_object<Range>::type &> ()))
                                     &> ()),
        decltype (*adl_begin (
            std::declval<typename range_object<Range>::type &> ()))>::type>
    : traits::is_numeric<typename std::decay<decltype (*adl_begin (
          std::declval<typename range_object<Range>::type &> ()))>::type>
{
};

/// @brief The element type of a numeric range; no `type` otherwise, so it
/// can sit in a return type for SFINAE.
template <typename Range, bool = is_numeric_range<Range>::value>
struct numeric_range_value
{
};

template <typename Range> struct numeric_range_value<Range, true>
{
  typedef typename std::decay<decltype (*adl_begin (
      std::declval<typename range_object<Range>::type &> ()))>::type type;
};

/// @brief `std::sqrt`'s result type for `T` (double for an integral `T`).
template <typename T> struct sqrt_result
{
  typedef decltype (std::sqrt (std::declval<T> ())) type;
};

/// @brief `std::common_type` of two numeric types; no `type` otherwise.
template <typename T, typename U,
          bool = traits::is_numeric<T>::value && traits::is_numeric<U>::value>
struct numeric_common
{
};

template <typename T, typename U> struct numeric_common<T, U, true>
{
  typedef typename std::common_type<T, U>::type type;
};

/// @brief The type `a - b` has in the common type of T and U (promoted, so
/// `short - short` is `int`); no `type` unless both are numeric.
template <typename T, typename U,
          bool = traits::is_numeric<T>::value && traits::is_numeric<U>::value>
struct difference_type
{
};

template <typename T, typename U> struct difference_type<T, U, true>
{
  typedef typename std::common_type<T, U>::type common;
  typedef decltype (std::declval<common> () - std::declval<common> ()) type;
};

/// @brief Result type of `distance` for two integers; no `type` otherwise.
/// Every trait here stays SFINAE-friendly for any argument type, so an
/// unqualified `distance (it1, it2)` next to `using namespace ops` still
/// finds `std::distance`.
template <typename T, typename U,
          bool = traits::is_numeric<T>::value && traits::is_numeric<U>::value
                 && !std::is_floating_point<T>::value
                 && !std::is_floating_point<U>::value>
struct integral_distance
{
};

template <typename T, typename U> struct integral_distance<T, U, true>
{
  typedef typename difference_type<T, U>::type type;
};

/// @brief Result type of `distance` with a floating-point operand; no `type`
/// otherwise.
template <typename T, typename U,
          bool = traits::is_numeric<T>::value && traits::is_numeric<U>::value
                 && (std::is_floating_point<T>::value
                     || std::is_floating_point<U>::value)>
struct floating_distance
{
};

template <typename T, typename U> struct floating_distance<T, U, true>
{
  typedef typename std::common_type<T, U>::type type;
};

/// @brief `sqrt` result type of the common element type of two numeric
/// ranges; no `type` otherwise.
template <typename Range1, typename Range2,
          bool
          = is_numeric_range<Range1>::value && is_numeric_range<Range2>::value>
struct range_pair_result
{
};

template <typename Range1, typename Range2>
struct range_pair_result<Range1, Range2, true>
{
  typedef typename std::common_type<
      typename numeric_range_value<Range1>::type,
      typename numeric_range_value<Range2>::type>::type common;
  typedef typename sqrt_result<common>::type type;
};

/// @brief `sqrt` result type of a numeric range's elements combined with a
/// numeric scalar; no `type` otherwise.
template <typename Range, typename Scalar,
          bool = is_numeric_range<Range>::value
                 && traits::is_numeric<Scalar>::value>
struct range_scalar_result
{
};

template <typename Range, typename Scalar>
struct range_scalar_result<Range, Scalar, true>
{
  typedef typename std::common_type<typename numeric_range_value<Range>::type,
                                    Scalar>::type common;
  typedef typename sqrt_result<common>::type type;
};

// --- checked_narrow_cast helpers ------------------------------------------

/// @brief Prints a value as a number: the unary plus promotes character
/// types, so they print their code, not a glyph.
template <typename T>
void
print_number (std::ostream &os, T value)
{
  os << +value;
}

/// @brief `a < b` for two integers of any signedness, without the usual
/// arithmetic conversions turning a negative value into a huge unsigned one
/// (C++20 `std::cmp_less`).
template <typename A, typename B>
bool
cmp_less (A a, B b, std::integral_constant<int, 0>) // unsigned, unsigned
{
  return static_cast<std::uintmax_t> (a) < static_cast<std::uintmax_t> (b);
}

template <typename A, typename B>
bool
cmp_less (A a, B b, std::integral_constant<int, 1>) // unsigned, signed
{
  return b > 0
         && static_cast<std::uintmax_t> (a) < static_cast<std::uintmax_t> (b);
}

template <typename A, typename B>
bool
cmp_less (A a, B b, std::integral_constant<int, 2>) // signed, unsigned
{
  return a < 0
         || static_cast<std::uintmax_t> (a) < static_cast<std::uintmax_t> (b);
}

template <typename A, typename B>
bool
cmp_less (A a, B b, std::integral_constant<int, 3>) // signed, signed
{
  return static_cast<std::intmax_t> (a) < static_cast<std::intmax_t> (b);
}

template <typename A, typename B>
bool
cmp_less (A a, B b)
{
  return cmp_less (
      a, b,
      std::integral_constant<int,
                             (std::is_signed<A>::value ? 2 : 0)
                                 + (std::is_signed<B>::value ? 1 : 0)> ());
}

/// @brief Integral source, integral target: exact comparison.
template <typename Target, typename Source>
bool
fits (Source value, std::true_type, std::true_type)
{
  return !cmp_less (value, std::numeric_limits<Target>::lowest ())
         && !cmp_less ((std::numeric_limits<Target>::max) (), value);
}

/// @brief Floating-point source, integral target. The bounds are compared in
/// `long double`, which is only `double` on MSVC: the maximum of a 64-bit
/// target rounds up to 2^63 / 2^64 there, so the exclusive power-of-two
/// bound `2^digits` (exact in any binary floating type) is checked as well.
/// The lowest value (0 or -2^digits) is always exact.
template <typename Target, typename Source>
bool
fits (Source value, std::false_type, std::true_type)
{
  long double const v = static_cast<long double> (value);
  long double const lowest
      = static_cast<long double> (std::numeric_limits<Target>::lowest ());
  long double const highest
      = static_cast<long double> ((std::numeric_limits<Target>::max) ());
  long double const bound
      = std::ldexp (1.0L, std::numeric_limits<Target>::digits);
  return !(v < lowest) && !(v > highest) && v < bound;
}

/// @brief Floating-point target: compare against its finite range.
template <typename Target, typename Source, typename SourceIsIntegral>
bool
fits (Source value, SourceIsIntegral, std::false_type)
{
  long double const v = static_cast<long double> (value);
  return !(v
           < static_cast<long double> (std::numeric_limits<Target>::lowest ()))
         && !(v > static_cast<long double> (
                  (std::numeric_limits<Target>::max) ()));
}

template <typename Source, typename Name>
void
throw_if_not_finite (Source value, Name const &fieldName, std::true_type)
{
  if (!std::isfinite (value))
    {
      std::ostringstream oss;
      oss << "Field '" << fieldName << "' contains non-finite value " << value;
      throw std::out_of_range (oss.str ());
    }
}

template <typename Source, typename Name>
void
throw_if_not_finite (Source, Name const &, std::false_type)
{
}

/// @brief Called after a lockstep pass over two ranges that consumed `count`
/// elements of each: throws LumexMathSizeMismatchException unless both
/// ranges are exhausted. Counts what is left, so the message carries both
/// lengths.
template <typename It1, typename End1, typename It2, typename End2>
void
require_same_size (char const *function, It1 &it1, End1 const &last1, It2 &it2,
                   End2 const &last2, std::size_t count)
{
  if (!(it1 != last1) && !(it2 != last2))
    return;

  std::size_t size1 = count;
  std::size_t size2 = count;
  for (; it1 != last1; ++it1)
    ++size1;
  for (; it2 != last2; ++it2)
    ++size2;
  throw LumexMathSizeMismatchException (
      std::string (function) + ": size of both ranges must be equal ("
      + std::to_string (size1) + " vs " + std::to_string (size2) + ")");
}
} // namespace Detail

/**
 * @brief Checks whether a floating-point value is NaN or +-Infinity.
 * @param value The value to check.
 * @return true if `value` is NaN or infinite, false otherwise.
 */
template <typename T>
LUMEX_ATTRIBUTE_NODISCARD ("return value must be used")
LUMEX_CONSTEXPR
    typename std::enable_if<std::is_floating_point<T>::value, bool>::type
    is_nan_inf (T value) LUMEX_NOEXCEPT_IF (
        noexcept (std::isnan (value)) && noexcept (std::isinf (value)))
{
  return std::isnan (value) || std::isinf (value);
}

/**
 * @brief Casts `sourceValue` to `TargetType`, throwing if the value is
 *        non-finite (for a floating-point source) or does not fit in
 *        `TargetType`'s range.
 * @param sourceValue The value to cast.
 * @param fieldName A human-readable name for `sourceValue`, used only to
 *        build the exception message: anything `std::ostream` prints
 *        (a string literal, `std::string`, `std::string_view`, ...).
 * @throws std::out_of_range if `sourceValue` is non-finite, or does not fit
 *         into `TargetType`'s representable range. The check is exact for
 *         every pair of types, 64-bit integers included.
 */
template <typename SourceType, typename TargetType, typename Name>
LUMEX_ATTRIBUTE_NODISCARD ("return value must be used")
typename std::enable_if<traits::is_numeric<SourceType>::value
                            && traits::is_numeric<TargetType>::value,
                        TargetType>::type
    checked_narrow_cast (SourceType sourceValue, Name const &fieldName)
{
  Detail::throw_if_not_finite (
      sourceValue, fieldName,
      std::integral_constant<bool,
                             std::is_floating_point<SourceType>::value> ());

  bool const inRange = Detail::fits<TargetType> (
      sourceValue,
      std::integral_constant<bool, std::is_integral<SourceType>::value> (),
      std::integral_constant<bool, std::is_integral<TargetType>::value> ());
  if (!inRange)
    {
      std::ostringstream oss;
      oss << "Field '" << fieldName << "' value ";
      Detail::print_number (oss, sourceValue);
      oss << " is out of range [";
      Detail::print_number (oss, std::numeric_limits<TargetType>::lowest ());
      oss << "; ";
      Detail::print_number (oss, (std::numeric_limits<TargetType>::max) ());
      oss << "]";
      throw std::out_of_range (oss.str ());
    }

  return static_cast<TargetType> (sourceValue);
}

/// @brief Absolute difference |a - b| of two integers, computed without an
/// intermediate signed underflow for unsigned types.
template <typename T, typename U>
LUMEX_CONSTEXPR typename Detail::integral_distance<T, U>::type
distance (T a, U b)
{
  typedef typename Detail::numeric_common<T, U>::type CommonType;
  return (static_cast<CommonType> (a) > static_cast<CommonType> (b))
             ? (static_cast<CommonType> (a) - static_cast<CommonType> (b))
             : (static_cast<CommonType> (b) - static_cast<CommonType> (a));
}

/// @brief Overload for at least one floating-point operand: `std::abs` of the
/// difference in the common type.
template <typename T, typename U>
typename Detail::floating_distance<T, U>::type
distance (T a, U b)
{
  typedef typename Detail::numeric_common<T, U>::type CommonType;
  return std::abs (static_cast<CommonType> (a) - static_cast<CommonType> (b));
}

/// @brief (a - b)^2, computed in the common type of a and b (promoted, so two
/// `short` operands give an `int`).
template <typename T, typename U>
LUMEX_CONSTEXPR typename Detail::difference_type<T, U>::type
squared_difference (T a, U b)
{
  typedef typename Detail::numeric_common<T, U>::type CommonType;
  return (static_cast<CommonType> (a) - static_cast<CommonType> (b))
         * (static_cast<CommonType> (a) - static_cast<CommonType> (b));
}

/**
 * @brief Arithmetic mean of a range's elements, or `ValueType{0}` for an
 *        empty range.
 * @details The mean has the element type: for integers it is truncated
 *          (`avg ({1, 2})` is 1). `range` keeps the caller's constness, so
 *          views that can only be iterated when not const
 *          (`std::views::filter`) work as lvalues and as temporaries.
 */
template <typename Range>
typename Detail::numeric_range_value<Range>::type
avg (Range &&range)
{
  typedef typename Detail::numeric_range_value<Range>::type ValueType;

  ValueType sum = ValueType (0);
  std::size_t count = 0;
  auto it = Detail::adl_begin (range);
  auto const last = Detail::adl_end (range);
  for (; it != last; ++it)
    {
      sum += *it;
      ++count;
    }

  return (count == 0)
             ? ValueType (0)
             : static_cast<ValueType> (sum / static_cast<ValueType> (count));
}

/// @brief Arithmetic mean of only the elements matching `predicate`, or
/// `ValueType{0}` if none match (or the range is empty).
template <typename Range, typename Predicate>
typename Detail::numeric_range_value<Range>::type
avg (Range &&range, Predicate predicate)
{
  typedef typename Detail::numeric_range_value<Range>::type ValueType;

  ValueType sum = ValueType (0);
  std::size_t count = 0;
  auto it = Detail::adl_begin (range);
  auto const last = Detail::adl_end (range);
  for (; it != last; ++it)
    {
      ValueType const value = *it;
      if (predicate (value))
        {
          sum += value;
          ++count;
        }
    }

  return (count == 0)
             ? ValueType (0)
             : static_cast<ValueType> (sum / static_cast<ValueType> (count));
}

/// @brief Root Mean Square of a range's elements, or `ResultType{0}` for an
/// empty range.
/// @note Returns `decltype(std::sqrt(ValueType{}))` (always a floating-point
///       type, even for an integral `ValueType`), not `ValueType` itself:
///       `std::sqrt` has no integral result, and truncating it back to an
///       integer would lose the answer.
template <typename Range>
typename Detail::sqrt_result<
    typename Detail::numeric_range_value<Range>::type>::type
rms (Range &&range)
{
  typedef typename Detail::numeric_range_value<Range>::type ValueType;
  typedef typename Detail::sqrt_result<ValueType>::type ResultType;

  ValueType sumOfSquares = ValueType (0);
  std::size_t count = 0;
  auto it = Detail::adl_begin (range);
  auto const last = Detail::adl_end (range);
  for (; it != last; ++it)
    {
      ValueType const value = *it;
      sumOfSquares += value * value;
      ++count;
    }

  if (count == 0)
    return ResultType (0);
  return static_cast<ResultType> (
      std::sqrt (sumOfSquares / static_cast<ResultType> (count)));
}

/// @brief Root Mean Square of the element-wise product of two equally-sized
/// ranges: sqrt( 1/N * sum (x_i * y_i) ). `ResultType{0}` if both ranges are
/// empty.
/// @throws LumexMathSizeMismatchException if `first` and `second` have
///         different lengths (one of them empty included).
/// @note The sum is divided by N in `ResultType`, so integer ranges are not
///       truncated before the square root.
template <typename Range1, typename Range2>
typename Detail::range_pair_result<Range1, Range2>::type
rms (Range1 &&first, Range2 &&second)
{
  typedef
      typename Detail::range_pair_result<Range1, Range2>::common CommonType;
  typedef typename Detail::range_pair_result<Range1, Range2>::type ResultType;

  auto it1 = Detail::adl_begin (first);
  auto const last1 = Detail::adl_end (first);
  auto it2 = Detail::adl_begin (second);
  auto const last2 = Detail::adl_end (second);
  CommonType sumOfProducts = CommonType (0);
  std::size_t count = 0;
  for (; it1 != last1 && it2 != last2; ++it1, ++it2)
    {
      sumOfProducts
          += static_cast<CommonType> (*it1) * static_cast<CommonType> (*it2);
      ++count;
    }
  Detail::require_same_size ("rms", it1, last1, it2, last2, count);

  if (count == 0)
    return ResultType (0);
  return static_cast<ResultType> (
      std::sqrt (sumOfProducts / static_cast<ResultType> (count)));
}

/// @brief Root Mean Squared Error of a range against a fixed scalar:
/// sqrt( 1/N * sum (x_i - scalar)^2 ). `ResultType{0}` for an empty range.
/// @note Returns `decltype(std::sqrt(CommonType{}))` for the same reason
///       @ref rms does; the sum is divided by N in that type.
template <typename Range, typename Scalar>
typename Detail::range_scalar_result<Range, Scalar>::type
rmse (Range &&range, Scalar const &scalar)
{
  typedef
      typename Detail::range_scalar_result<Range, Scalar>::common CommonType;
  typedef typename Detail::range_scalar_result<Range, Scalar>::type ResultType;

  CommonType sumOfSquaredDifferences = CommonType (0);
  std::size_t count = 0;
  auto it = Detail::adl_begin (range);
  auto const last = Detail::adl_end (range);
  for (; it != last; ++it)
    {
      sumOfSquaredDifferences
          += static_cast<CommonType> (squared_difference (*it, scalar));
      ++count;
    }

  if (count == 0)
    return ResultType (0);
  return static_cast<ResultType> (
      std::sqrt (sumOfSquaredDifferences / static_cast<ResultType> (count)));
}

/// @brief Root Mean Squared Error between two equally-sized ranges:
/// sqrt( 1/N * sum (x_i - y_i)^2 ). `ResultType{0}` if both ranges are empty.
/// @throws LumexMathSizeMismatchException if `first` and `second` have
///         different lengths (one of them empty included).
/// @note Returns `decltype(std::sqrt(CommonType{}))` for the same reason
///       @ref rms does; the sum is divided by N in that type.
template <typename Range1, typename Range2>
typename Detail::range_pair_result<Range1, Range2>::type
rmse (Range1 &&first, Range2 &&second)
{
  typedef
      typename Detail::range_pair_result<Range1, Range2>::common CommonType;
  typedef typename Detail::range_pair_result<Range1, Range2>::type ResultType;

  auto it1 = Detail::adl_begin (first);
  auto const last1 = Detail::adl_end (first);
  auto it2 = Detail::adl_begin (second);
  auto const last2 = Detail::adl_end (second);
  CommonType sumOfSquaredDifferences = CommonType (0);
  std::size_t count = 0;
  for (; it1 != last1 && it2 != last2; ++it1, ++it2)
    {
      sumOfSquaredDifferences += static_cast<CommonType> (squared_difference (
          static_cast<CommonType> (*it1), static_cast<CommonType> (*it2)));
      ++count;
    }
  Detail::require_same_size ("rmse", it1, last1, it2, last2, count);

  if (count == 0)
    return ResultType (0);
  return static_cast<ResultType> (
      std::sqrt (sumOfSquaredDifferences / static_cast<ResultType> (count)));
}
} // namespace ops
} // namespace math
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_MATH_OPS_HPP
