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
 * @file LumexQuote.hpp
 * @brief Joins the elements of a range with each element wrapped in quotes.
 *
 * @details `quote` / `quote_double` wrap in `"`, `quote_single` in `'`.
 * Elements must convert to `std::string const &` (`std::string`,
 * `char const *`, ...). Quote characters inside an element are not escaped.
 * C++20 builds on `std::ranges` and `join`; C++11 to C++17 use one explicit
 * loop with the same result.
 *
 * C++20 takes the range as `input_range auto const &`: a view that is not
 * iterable through const (`std::views::filter`) must be materialized
 * first.
 */
#ifndef LUMEX_CORE_STRING_TEXT_QUOTE_HPP
#define LUMEX_CORE_STRING_TEXT_QUOTE_HPP

#include <sstream>
#include <string>
#include <type_traits>

#if __cplusplus >= 202002L
#include <ranges>
#include <string_view>
#endif

#include "lumex/core/string/text/LumexJoin.hpp"
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
 * @brief `join` with every element wrapped in double quotes (`"element"`).
 * @param range Elements convertible to `std::string const &`.
 * @param separator Written between (not after) elements.
 */
std::string
quote (std::ranges::input_range auto const &range, std::string_view separator)
  requires std::is_convertible_v<
      std::ranges::range_reference_t<decltype (range)>, std::string const &>
{
  return join (
      range | std::ranges::views::transform ([] (std::string const &element) {
        return "\"" + element + "\"";
      }),
      separator);
}

/** @brief Same as `quote`; named for symmetry with `quote_single`. */
std::string
quote_double (std::ranges::input_range auto const &range,
              std::string_view separator)
  requires std::is_convertible_v<
      std::ranges::range_reference_t<decltype (range)>, std::string const &>
{
  return quote (range, separator);
}

/**
 * @brief `join` with every element wrapped in single quotes (`'element'`).
 * @param range Elements convertible to `std::string const &`.
 * @param separator Written between (not after) elements.
 */
std::string
quote_single (std::ranges::input_range auto const &range,
              std::string_view separator)
  requires std::is_convertible_v<
      std::ranges::range_reference_t<decltype (range)>, std::string const &>
{
  return join (
      range | std::ranges::views::transform ([] (std::string const &element) {
        return "'" + element + "'";
      }),
      separator);
}

#else

namespace Detail
{
/**
 * @brief Joins `range`, wrapping each element (converted to
 * `std::string const &`) in `mark`.
 */
template <typename Range, typename Separator>
typename std::enable_if<
    lumex::core::utility::traits::range::has_elements_convertible_to<
        Range, std::string const &>::value
        && lumex::core::utility::traits::stream::is_streamable<
            typename std::decay<Separator>::type>::value,
    std::string>::type
quote_each (Range const &range, Separator const &separator, char mark)
{
  std::ostringstream oss;
  bool is_first = true;
  for (auto const &element : range)
    {
      std::string const &text = element;
      if (!is_first)
        oss << separator;
      is_first = false;
      oss << mark << text << mark;
    }
  return oss.str ();
}
} // namespace Detail

/**
 * @brief `join` with every element wrapped in double quotes (`"element"`),
 * C++11 to C++17.
 * @tparam Range Anything a range-based `for` accepts.
 * @tparam Separator Anything `operator<<` can write.
 * @param range Elements convertible to `std::string const &`.
 * @param separator Written between (not after) elements.
 */
template <typename Range, typename Separator>
typename std::enable_if<
    lumex::core::utility::traits::range::has_elements_convertible_to<
        Range, std::string const &>::value
        && lumex::core::utility::traits::stream::is_streamable<
            typename std::decay<Separator>::type>::value,
    std::string>::type
quote (Range const &range, Separator const &separator)
{
  return Detail::quote_each (range, separator, '"');
}

/** @brief Same as `quote`; named for symmetry with `quote_single`. */
template <typename Range, typename Separator>
typename std::enable_if<
    lumex::core::utility::traits::range::has_elements_convertible_to<
        Range, std::string const &>::value
        && lumex::core::utility::traits::stream::is_streamable<
            typename std::decay<Separator>::type>::value,
    std::string>::type
quote_double (Range const &range, Separator const &separator)
{
  return quote (range, separator);
}

/**
 * @brief `join` with every element wrapped in single quotes (`'element'`),
 * C++11 to C++17.
 */
template <typename Range, typename Separator>
typename std::enable_if<
    lumex::core::utility::traits::range::has_elements_convertible_to<
        Range, std::string const &>::value
        && lumex::core::utility::traits::stream::is_streamable<
            typename std::decay<Separator>::type>::value,
    std::string>::type
quote_single (Range const &range, Separator const &separator)
{
  return Detail::quote_each (range, separator, '\'');
}

#endif // __cplusplus >= 202002L
} // namespace text
} // namespace string
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_STRING_TEXT_QUOTE_HPP
