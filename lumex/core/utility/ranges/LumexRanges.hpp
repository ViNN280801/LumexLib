/**
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy
 * of this software and associated
 * documentation files (the "Software"), to deal
 * in the Software without
 * restriction, including without limitation the rights
 * to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the
 * Software, and to permit persons to whom the Software is
 * furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice
 * and this permission notice shall be included in
 * all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT
 * WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO
 * THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file LumexRanges.hpp
 * @brief `GetNearestTo()`, which finds the element of a sorted range whose
 * value is numerically nearest to a given value.
 * @details It searches with `std::ranges::lower_bound` and then compares the
 * element found with its predecessor, so the range must be sorted by the same
 * comparison and projection. It accepts an iterator and sentinel pair or a
 * range, plus an optional comparison and projection as the standard range
 * algorithms do, and returns the end iterator only for an empty range. The
 * distance and the numeric concept come from the `lumex::math` module.
 * @warning Requires C++20 (concepts and `<ranges>`); with an older standard
 * the header declares nothing.
 */
#ifndef LUMEX_CORE_UTILITY_RANGES_HPP
#define LUMEX_CORE_UTILITY_RANGES_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#if __has_warning("-Wnrvo")
#pragma clang diagnostic ignored "-Wnrvo"
#endif
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#if __has_warning("-Wvariadic-macro-arguments-omitted")
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#endif
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#include <algorithm>
#include <functional>
#include <iterator>
#if __cplusplus > 201703L && defined(__has_include)
#if __has_include(<ranges>)
#include <ranges>
#endif
#endif
#include <type_traits>

#if __cplusplus > 201703L
#include "lumex/core/math/LumexMath"
#endif
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

// Needs C++20 <ranges>; without it the header declares nothing.
#if LUMEX_HAS_STD_RANGES

namespace lumex
{
namespace core
{
namespace utility
{
namespace ranges
{
namespace Algorithm
{
namespace Detail
{
// NumericConcept/distance now come from the real lumex::core::Math module (see
// lumex/core/math/LumexMath) - this used to be a self-contained duplicate
// here because that module did not exist yet at the time GetNearestTo() below
// was written.
using lumex::core::math::ops::distance;
using lumex::core::math::ops::traits::NumericConcept;
} // namespace Detail

/**
 * @brief Finds the iterator whose (projected) value is numerically nearest to
 * `value`.
 * @details Requires the range to be sorted according to `pred`/`proj` (uses
 *          `std::ranges::lower_bound`), then compares the found element and
 * its predecessor to pick whichever is closer to `value`.
 * @tparam IteratorType Bidirectional iterator over the range.
 * @tparam Sentinel Sentinel type for `IteratorType`.
 * @tparam ValueType Numeric type of `value` (see Detail::NumericConcept).
 * @tparam Projection Projection applied to elements before comparison
 * (defaults to identity).
 * @tparam Predicate Strict-weak-order predicate used for the search (defaults
 * to `std::ranges::less`).
 * @return Iterator to the nearest element, or `last` if `first == last`.
 */
template <std::bidirectional_iterator IteratorType,
          std::sentinel_for<IteratorType> Sentinel, typename ValueType,
          typename Projection = std::identity,
          std::indirect_strict_weak_order<
              ValueType const *, std::projected<IteratorType, Projection>>
              Predicate
          = std::ranges::less>
  requires Detail::NumericConcept<ValueType>
LUMEX_ATTRIBUTE_NODISCARD ("return value must be used")
LUMEX_CONSTEXPR IteratorType
    GetNearestTo (IteratorType first, Sentinel last, ValueType const &value,
                  Predicate pred = {}, Projection proj = {})
{
  if (first == last)
    return last;

  auto iter = std::ranges::lower_bound (first, last, value, pred, proj);
  if (iter == last)
    return std::ranges::prev (iter);
  if (iter == first)
    return iter;

  auto const prev = std::ranges::prev (iter);

  auto const distBetweenFoundAndSpecified
      = Detail::distance (std::invoke (proj, *iter), value);
  auto const distBetweenPrevAndSpecified
      = Detail::distance (std::invoke (proj, *prev), value);

  return (distBetweenFoundAndSpecified < distBetweenPrevAndSpecified) ? iter
                                                                      : prev;
}

/**
 * @brief Range-based overload of GetNearestTo().
 */
template <std::ranges::bidirectional_range RangeType, typename ValueType,
          typename Projection = std::identity,
          std::indirect_strict_weak_order<
              ValueType const *,
              std::projected<std::ranges::iterator_t<RangeType>, Projection>>
              Predicate
          = std::ranges::less>
  requires Detail::NumericConcept<ValueType>
LUMEX_ATTRIBUTE_NODISCARD ("return value must be used")
LUMEX_CONSTEXPR std::ranges::borrowed_iterator_t<RangeType> GetNearestTo (
    RangeType &&range, ValueType const &value, Predicate pred = {},
    Projection proj = {})
{
  return GetNearestTo (std::ranges::begin (std::forward<RangeType> (range)),
                       std::ranges::end (std::forward<RangeType> (range)),
                       value, pred, proj);
}
} // namespace Algorithm
} // namespace ranges
} // namespace utility
} // namespace core
} // namespace lumex

#endif // LUMEX_HAS_STD_RANGES

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_UTILITY_RANGES_HPP
