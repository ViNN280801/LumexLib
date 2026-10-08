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
 * @file LumexPortableWStringView.hpp
 * @brief The wide string view type that is `std::wstring_view` where the
 * standard has one and the `lumex_wstring_view` of this module below it.
 * @details `portable_wstring_view_t` is `std::wstring_view` from C++17 and
 * `lumex_wstring_view` before it; see `LumexPortableStringView.hpp` for the
 * narrow twin and the reasons. The header includes `<string_view>` from C++17
 * and the wide view header before it, nothing else.
 */
#ifndef LUMEX_CORE_STRING_VIEW_VIEW_PORTABLE_WSTRING_VIEW_HPP
#define LUMEX_CORE_STRING_VIEW_VIEW_PORTABLE_WSTRING_VIEW_HPP

#if __cplusplus >= 201703L
#include <string_view>
#else
#include "lumex/core/string_view/view/LumexWStringView.hpp"
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
 * @brief `std::wstring_view` from C++17, `lumex_wstring_view` below it.
 */
#if __cplusplus >= 201703L
using portable_wstring_view_t = std::wstring_view;
#else
using portable_wstring_view_t = lumex_wstring_view;
#endif
} // namespace view
} // namespace string_view
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_STRING_VIEW_VIEW_PORTABLE_WSTRING_VIEW_HPP
