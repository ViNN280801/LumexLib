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
 * @file LumexJoin.hpp
 * @brief Joins the elements of a range into one string.
 *
 * @details C++20: `join` takes any `std::ranges::input_range` (views
 * included) and a `std::string_view` separator. C++11 to C++17: `join` takes
 * anything a range-based `for` can walk (containers, C arrays,
 * `std::initializer_list`) and any separator `operator<<` can write
 * (`"literal"`, `std::string`, `LumexStringView`, C++17 `std::string_view`,
 * `char`). Both produce the same text for the same elements.
 *
 * C++20 takes the range as `input_range auto const &`: a view that is not
 * iterable through const (`std::views::filter`) must be materialized
 * first.
 */
#ifndef LUMEX_CORE_STRING_TEXT_JOIN_HPP
#define LUMEX_CORE_STRING_TEXT_JOIN_HPP

#include <iterator>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#if __cplusplus >= 202002L
#include <ranges>
#include <string_view>
#endif

#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace string
{
namespace text
{
#if __cplusplus >= 202002L

/**
 * @brief Streams every element of `range` with `operator<<`, separated by
 * `separator`.
 * @details One pass: the separator is written before the second and later
 * elements, never after the last one, so an `input_range` whose end is not
 * known in advance works too.
 * @param range Any `std::ranges::input_range` whose elements support
 * `operator<<`.
 * @param separator Written between (not after) elements.
 * @return The joined text; empty for an empty range.
 * @note O(n) time, one pass.
 */
std::string
join (std::ranges::input_range auto const &range, std::string_view separator)
  requires lumex::core::utility::traits::stream::Streamable<
      std::ranges::range_reference_t<decltype (range)>>
{
  std::ostringstream oss;
  bool is_first = true;
  for (auto const &element : range)
    {
      if (!std::exchange (is_first, false))
        oss << separator;
      oss << element;
    }
  return oss.str ();
}

#else

/**
 * @brief Streams every element of `range` with `operator<<`, separated by
 * `separator` (C++11 to C++17).
 * @details Same contract as the C++20 overload: one pass, no trailing
 * separator, empty text for an empty range.
 * @tparam Range Anything a range-based `for` accepts: a container, a C array,
 * a `std::initializer_list`.
 * @tparam Separator Anything `operator<<` can write.
 * @param range The elements; each must support `operator<<`.
 * @param separator Written between (not after) elements.
 * @note O(n) time, one pass. Takes part in overload resolution only when
 * `Range` is iterable with streamable elements and `Separator` is
 * streamable (`lumex::core::utility::traits`).
 */
template <typename Range, typename Separator>
typename std::enable_if<
    lumex::core::utility::traits::range::has_streamable_elements<Range>::value
        && lumex::core::utility::traits::stream::is_streamable<
            typename std::decay<Separator>::type>::value,
    std::string>::type
join (Range const &range, Separator const &separator)
{
  std::ostringstream oss;
  bool is_first = true;
  for (auto const &element : range)
    {
      if (!is_first)
        oss << separator;
      is_first = false;
      oss << element;
    }
  return oss.str ();
}

#endif // __cplusplus >= 202002L
} // namespace text
} // namespace string
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_STRING_TEXT_JOIN_HPP
