/*
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
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
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

// Detection traits shared by the LumexTextConstraints test files of every
// standard: whether a text::join / quote call with a given range and
// separator is viable.

#ifndef LUMEX_TESTS_CORE_STRING_TEXT_HPP
#define LUMEX_TESTS_CORE_STRING_TEXT_HPP

#include <type_traits>
#include <utility>

#include "lumex/core/string/text/LumexJoin.hpp"
#include "lumex/core/string/text/LumexQuote.hpp"

namespace text_constraints_test_helpers
{
template <typename... T> struct make_void
{
  typedef void type;
};

template <typename Range, typename Separator, typename = void>
struct can_join : std::false_type
{
};

template <typename Range, typename Separator>
struct can_join<Range, Separator,
                typename make_void<decltype (lumex::core::string::text::join (
                    std::declval<Range const &> (),
                    std::declval<Separator const &> ()))>::type>
    : std::true_type
{
};

template <typename Range, typename Separator, typename = void>
struct can_quote : std::false_type
{
};

template <typename Range, typename Separator>
struct can_quote<
    Range, Separator,
    typename make_void<decltype (lumex::core::string::text::quote (
                           std::declval<Range const &> (),
                           std::declval<Separator const &> ())),
                       decltype (lumex::core::string::text::quote_double (
                           std::declval<Range const &> (),
                           std::declval<Separator const &> ())),
                       decltype (lumex::core::string::text::quote_single (
                           std::declval<Range const &> (),
                           std::declval<Separator const &> ()))>::type>
    : std::true_type
{
};
} // namespace text_constraints_test_helpers

#endif // !LUMEX_TESTS_CORE_STRING_TEXT_HPP
