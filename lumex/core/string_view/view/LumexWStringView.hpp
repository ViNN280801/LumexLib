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
 * @file LumexWStringView.hpp
 * @brief `lumex_wstring_view`: the view over `wchar_t`, the
 * `std::wstring_view` of the library from C++11.
 * @details A header-only alias of `lumex_basic_string_view<wchar_t>` (see
 * `LumexBasicStringView.hpp` for the class). The global alias
 * `::lumex_wstring_view` is the wide view type of the whole library in every
 * C++ standard.
 */
#ifndef LUMEX_CORE_STRING_VIEW_VIEW_WSTRING_VIEW_HPP
#define LUMEX_CORE_STRING_VIEW_VIEW_WSTRING_VIEW_HPP

#include "lumex/core/string_view/view/LumexBasicStringView.hpp"

/**
 * @brief Global alias of `lumex::core::string_view::view::lumex_wstring_view`.
 */
using lumex_wstring_view = lumex::core::string_view::view::lumex_wstring_view;

#endif // LUMEX_CORE_STRING_VIEW_VIEW_WSTRING_VIEW_HPP
