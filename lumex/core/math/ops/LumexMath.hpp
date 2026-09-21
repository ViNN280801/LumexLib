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
 * @details Requires C++20 (`<concepts>`, `<ranges>`). Signatures are kept as
 *          close as practical to their original source, only renamed into
 *          the `lumex::core::Math` namespace and adapted to this library's
 *          error-reporting style (message building via `std::ostringstream`
 *          rather than `std::format`, since full `<format>` support is not
 *          guaranteed on every C++20 toolchain this library targets).
 */
#ifndef LUMEX_CORE_MATH_OPS_HPP
#define LUMEX_CORE_MATH_OPS_HPP

#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#if __cplusplus < 202002L
#error "LumexMath.hpp requires C++20 (concepts, ranges)."
#endif

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <limits>
#include <numeric>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace lumex
{
namespace core
{
namespace math
{
namespace ops
{
namespace traits
{
/// @brief A "real" numeric type: any integral type except `bool`, or any
/// floating-point type.
template <typename T>
concept NumericConcept
    = (std::integral<T> && !std::same_as<T, bool>) || std::floating_point<T>;
} // namespace traits

/**
 * @brief Checks whether a floating-point value is NaN or +-Infinity.
 * @param value The value to check.
 * @return true if `value` is NaN or infinite, false otherwise.
 */
template <std::floating_point T>
LUMEX_ATTRIBUTE_NODISCARD ("return value must be used")
LUMEX_CONSTEXPR bool is_nan_inf (T value) LUMEX_NOEXCEPT_IF (
    noexcept (std::isnan (value)) && noexcept (std::isinf (value)))
{
  return std::isnan (value) || std::isinf (value);
}

/**
 * @brief Casts `sourceValue` to `TargetType`, throwing if the value is
 *        non-finite (for a floating-point source) or does not fit in
 *        `TargetType`'s range.
 * @param sourceValue The value to cast.
 * @param fieldName A human-readable name for `sourceValue`, used only
 *        to build the exception message.
 * @throws std::out_of_range if `sourceValue` is non-finite, or does
 *         not fit into `TargetType`'s representable range.
 */
template <typename SourceType, typename TargetType>
  requires (traits::NumericConcept<SourceType>
            && traits::NumericConcept<TargetType>)
LUMEX_ATTRIBUTE_NODISCARD ("return value must be used")
TargetType
    checked_narrow_cast (SourceType sourceValue, std::string_view fieldName)
{
  LUMEX_CONSTEXPR_IF (std::floating_point<SourceType>)
  {
    if (!std::isfinite (sourceValue))
      {
        std::ostringstream oss;
        oss << "Field '" << fieldName << "' contains non-finite value "
            << sourceValue;
        throw std::out_of_range (oss.str ());
      }
  }

  auto const value = static_cast<long double> (sourceValue);
  auto const minValue
      = static_cast<long double> (std::numeric_limits<TargetType>::lowest ());
  auto const maxValue
      = static_cast<long double> (std::numeric_limits<TargetType>::max ());
  if ((value < minValue) || (value > maxValue))
    {
      std::ostringstream oss;
      oss << "Field '" << fieldName << "' value " << sourceValue
          << " is out of range [" << minValue << "; " << maxValue << "]";
      throw std::out_of_range (oss.str ());
    }

  return static_cast<TargetType> (sourceValue);
}

/// @brief Unsigned distance between two numeric values (|a - b|, computed
/// without an intermediate signed underflow for unsigned types).
template <typename T, typename U>
  requires (traits::NumericConcept<T> && traits::NumericConcept<U>)
auto
distance (T a, U b)
{
  using CommonType = std::common_type_t<T, U>;
  auto const ca = static_cast<CommonType> (a);
  auto const cb = static_cast<CommonType> (b);
  return (ca > cb) ? (ca - cb) : (cb - ca);
}

/// @brief Overload for at least one floating-point operand - uses `std::abs`
/// directly instead of a manual comparison.
template <typename T, typename U>
  requires ((traits::NumericConcept<T> && traits::NumericConcept<U>)
            && (std::floating_point<T> || std::floating_point<U>))
auto
distance (T a, U b)
{
  using CommonType = std::common_type_t<T, U>;
  return std::abs (static_cast<CommonType> (a) - static_cast<CommonType> (b));
}

/// @brief (a - b)^2, computed in the common type of a and b.
template <typename T, typename U>
  requires (traits::NumericConcept<T> && traits::NumericConcept<U>)
auto
squared_difference (T a, U b)
{
  using CommonType = std::common_type_t<T, U>;
  auto const diff = static_cast<CommonType> (a) - static_cast<CommonType> (b);
  return diff * diff;
}

namespace Detail
{
/// @brief Shared implementation for both `avg` overloads below - avoids
/// the two overloads calling each other recursively.
template <std::ranges::input_range Range>
  requires (traits::NumericConcept<std::ranges::range_value_t<Range>>)
auto
_avg_impl (Range const &range)
{
  using ValueType = std::ranges::range_value_t<Range>;

  if (std::ranges::begin (range) == std::ranges::end (range))
    return ValueType{ 0 };

  ValueType sum = ValueType{ 0 };
  std::size_t size = std::size_t{ 0 };

  for (auto const &elem : range)
    {
      sum += elem;
      ++size;
    }

  // static_cast to ValueType: without it, `sum / size` promotes to
  // the common type of ValueType and std::size_t (the usual
  // arithmetic conversions) whenever ValueType is a narrower
  // integral type - e.g. int / std::size_t promotes to std::size_t - which
  // does not match the `ValueType{0}` early-return above and fails
  // `auto` return-type deduction (both return statements in one
  // function must deduce to the same type).
  return (size == 0)
             ? ValueType{ 0 }
             : static_cast<ValueType> (sum / static_cast<ValueType> (size));
}
} // namespace Detail

/// @brief Arithmetic mean of a range's elements, or `ValueType{0}` for an
/// empty range.
template <std::ranges::input_range Range>
  requires (traits::NumericConcept<std::ranges::range_value_t<Range>>)
auto
avg (Range &range)
{
  return Detail::_avg_impl (range);
}

/// @brief `const`-range overload of avg(Range&).
template <std::ranges::input_range Range>
  requires (traits::NumericConcept<std::ranges::range_value_t<Range>>)
auto
avg (Range const &range)
{
  return Detail::_avg_impl (range);
}

/// @brief Arithmetic mean of only the elements matching `predicate`, or
/// `ValueType{0}` if none match (or the range is empty).
template <std::ranges::input_range Range, typename Predicate>
  requires (traits::NumericConcept<std::ranges::range_value_t<Range>>
            && std::predicate<Predicate, std::ranges::range_value_t<Range>>)
auto
avg (Range const &range, Predicate predicate)
{
  using ValueType = std::ranges::range_value_t<Range>;

  if (std::ranges::begin (range) == std::ranges::end (range))
    return ValueType{ 0 };

  auto init
      = std::make_pair (ValueType{ 0 }, std::ranges::range_size_t<Range>{ 0 });
  auto [sum, count] = std::accumulate (
      std::ranges::begin (range), std::ranges::end (range), init,
      [&predicate] (auto accumulator, auto const &elem) {
        if (predicate (elem))
          {
            accumulator.first += elem;
            accumulator.second += 1;
          }
        return accumulator;
      });
  if (count == std::ranges::range_size_t<Range>{ 0 })
    return ValueType{ 0 };

  return sum / static_cast<ValueType> (count);
}

/// @brief Root Mean Square of a range's elements, or `ResultType{0}` for an
/// empty range.
/// @note Returns `decltype(std::sqrt(ValueType{}))` (always a floating-point
/// type,
///       even for an integral `ValueType`), not `ValueType` itself -
///       `std::sqrt` has no integral overload, so forcing the result back to
///       an integral `ValueType` would both lose precision and make the
///       empty-range early-return (`ValueType{0}`) a different type than the
///       computed result, which fails `auto` return-type deduction.
template <std::ranges::sized_range Range>
  requires (traits::NumericConcept<std::ranges::range_value_t<Range>>)
auto
rms (Range const &range)
{
  using ValueType = std::ranges::range_value_t<Range>;
  using ResultType = decltype (std::sqrt (ValueType{}));

  if (std::ranges::empty (range))
    return ResultType{ 0 };

  ValueType const sumOfSquares = std::inner_product (
      std::ranges::begin (range), std::ranges::end (range),
      std::ranges::begin (range), ValueType{ 0 });
  return static_cast<ResultType> (std::sqrt (
      sumOfSquares / static_cast<ResultType> (std::ranges::size (range))));
}

/// @brief Root Mean Square of the element-wise product of two equally-sized
/// ranges.
/// @throws std::invalid_argument if `first` and `second` have different sizes.
/// @note See @ref rms(Range const&) for why this returns
/// `decltype(std::sqrt(ValueType{}))`.
template <std::ranges::sized_range Range>
  requires (traits::NumericConcept<std::ranges::range_value_t<Range>>)
auto
rms (Range const &first, Range const &second)
{
  using ValueType = std::ranges::range_value_t<Range>;
  using ResultType = decltype (std::sqrt (ValueType{}));

  if (std::ranges::empty (first) || std::ranges::empty (second))
    return ResultType{ 0 };

  auto const firstSize = std::ranges::size (first);
  auto const secondSize = std::ranges::size (second);
  if (firstSize != secondSize)
    throw std::invalid_argument ("Size of both ranges must be equal.");

  ValueType const sumOfSquares = std::inner_product (
      std::ranges::begin (first), std::ranges::end (first),
      std::ranges::begin (second), ValueType{ 0 });
  return static_cast<ResultType> (
      std::sqrt (sumOfSquares / static_cast<ValueType> (firstSize)));
}

/// @brief Root Mean Squared Error of a range against a fixed scalar: sqrt( 1/N
/// * sum (x_i - scalar)^2 ).
/// @note Returns `decltype(std::sqrt(CommonType{}))` for the same reason
///       @ref rms(Range const&) does - see that function's own note.
template <std::ranges::sized_range Range, typename Scalar>
  requires (traits::NumericConcept<std::ranges::range_value_t<Range>>
            && traits::NumericConcept<Scalar>)
auto
rmse (Range const &range, Scalar const &scalar)
{
  using ValueType = std::ranges::range_value_t<Range>;
  using CommonType = std::common_type_t<ValueType, Scalar>;
  using ResultType = decltype (std::sqrt (CommonType{}));

  if (std::ranges::empty (range))
    return ResultType{ 0 };

  CommonType sumOfSquaredDifferences{};

  std::ranges::for_each (
      range, [&sumOfSquaredDifferences, scalar] (auto const &value) {
        return sumOfSquaredDifferences += squared_difference (value, scalar);
      });
  return static_cast<ResultType> (
      std::sqrt (sumOfSquaredDifferences
                 / static_cast<CommonType> (std::ranges::size (range))));
}

/// @brief Root Mean Squared Error between two equally-sized ranges: sqrt( 1/N
/// * sum (x_i - y_i)^2 ).
/// @throws std::invalid_argument if `first` and `second` have different sizes.
/// @note Returns `decltype(std::sqrt(CommonType{}))` for the same reason
///       @ref rms(Range const&) does - see that function's own note.
template <std::ranges::sized_range Range1, std::ranges::sized_range Range2>
  requires (traits::NumericConcept<std::ranges::range_value_t<Range1>>
            && traits::NumericConcept<std::ranges::range_value_t<Range2>>)
auto
rmse (Range1 const &first, Range2 const &second)
{
  using ValueType1 = std::ranges::range_value_t<Range1>;
  using ValueType2 = std::ranges::range_value_t<Range2>;
  using CommonType = std::common_type_t<ValueType1, ValueType2>;
  using ResultType = decltype (std::sqrt (CommonType{}));

  if (std::ranges::empty (first) || std::ranges::empty (second))
    return ResultType{ 0 };

  auto const firstSize = std::ranges::size (first);
  auto const secondSize = std::ranges::size (second);
  if (firstSize != secondSize)
    throw std::invalid_argument ("Size of both ranges must be equal.");

  CommonType sumOfSquaredDifferences{};

  auto const indices = std::views::iota (
      std::ptrdiff_t{ 0 }, static_cast<std::ptrdiff_t> (firstSize));
  auto it1 = std::ranges::begin (first);
  auto it2 = std::ranges::begin (second);

  std::ranges::for_each (indices, [&sumOfSquaredDifferences, it1,
                                   it2] (std::ptrdiff_t idx) {
    sumOfSquaredDifferences += squared_difference (*(it1 + idx), *(it2 + idx));
  });

  return static_cast<ResultType> (std::sqrt (
      sumOfSquaredDifferences / static_cast<CommonType> (firstSize)));
}
} // namespace ops
} // namespace math
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_MATH_OPS_HPP
