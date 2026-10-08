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
 * @file LumexPortableStringView.hpp
 * @brief The string view type that is `std::string_view` where the standard
 * has one and the `lumex_string_view` of this module below it.
 * @details `portable_string_view_t` is `std::string_view` from C++17 and
 * `lumex_string_view` before it. A module with a string overload takes this
 * type, so the overload exists in every C++ standard and a literal, a `char
 * const *` and a `std::string` convert to it in all of them. From C++17 the
 * view of this module also converts to and from `std::string_view`, so a
 * `lumex_string_view` argument works there too. The header includes
 * `<string_view>` from C++17 and the view header before it, nothing else.
 */
#ifndef LUMEX_CORE_STRING_VIEW_VIEW_PORTABLE_STRING_VIEW_HPP
#define LUMEX_CORE_STRING_VIEW_VIEW_PORTABLE_STRING_VIEW_HPP

#if __cplusplus >= 201703L
#include <string_view>
#else
#include "lumex/core/string_view/view/LumexStringView.hpp"
#endif

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace string_view
{
namespace view
{
/**
 * @brief `std::string_view` from C++17, `lumex_string_view` below it.
 */
#if __cplusplus >= 201703L
using portable_string_view_t = std::string_view;
#else
using portable_string_view_t = lumex_string_view;
#endif
} // namespace view
} // namespace string_view
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_STRING_VIEW_VIEW_PORTABLE_STRING_VIEW_HPP
