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

#ifndef LUMEX_CORE_UTILITY_MACROS_KEYWORDS_HPP
#define LUMEX_CORE_UTILITY_MACROS_KEYWORDS_HPP

#if __cplusplus >= 202002L
#define LUMEX_CONSTINIT constinit
#elif __cplusplus >= 201103L
#define LUMEX_CONSTINIT constexpr
#else
#define LUMEX_CONSTINIT
#endif

#if __cplusplus >= 202002L
#define LUMEX_CONSTEVAL consteval
#elif __cplusplus >= 201103L
#define LUMEX_CONSTEVAL constexpr
#else
#define LUMEX_CONSTEVAL
#endif

#if __cplusplus >= 201703L
#define LUMEX_INLINE_VARIABLE inline
#else
#define LUMEX_INLINE_VARIABLE
#endif

#if __cplusplus >= 201103L
#define LUMEX_NOEXCEPT noexcept
#define LUMEX_NOEXCEPT_IF(...) noexcept (__VA_ARGS__)
#else
#define LUMEX_NOEXCEPT
#define LUMEX_NOEXCEPT_IF(...)
#endif

#if __cplusplus >= 201703L
#define LUMEX_CONSTEXPR_IF if constexpr
#else
#define LUMEX_CONSTEXPR_IF if
#endif

#if defined(_MSC_VER)
#define LUMEX_RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
#define LUMEX_RESTRICT __restrict__
#else
#define LUMEX_RESTRICT
#endif

#if __cplusplus >= 202303L
// C++23 and newer has relaxed constraints for constexpr for all of these
#define LUMEX_CONSTEXPR_FUNCTION constexpr
#define LUMEX_CONSTEXPR_VIRTUAL_FUNCTION constexpr
#define LUMEX_CONSTEXPR_CTOR constexpr
#define LUMEX_CONSTEXPR_DEFAULTED_CTOR constexpr
#define LUMEX_CONSTEXPR_DTOR constexpr
#define LUMEX_CONSTEXPR_DEFAULTED_DTOR constexpr
#define LUMEX_CONSTEXPR_VIRTUAL_DTOR constexpr
#define LUMEX_CONSTEXPR_DEFAULTED_VIRTUAL_DTOR constexpr
#elif __cplusplus == 202002L
// C++20: Virtual functions constexpr, but NOT virtual destructors
#define LUMEX_CONSTEXPR_FUNCTION constexpr
#define LUMEX_CONSTEXPR_VIRTUAL_FUNCTION constexpr
#define LUMEX_CONSTEXPR_CTOR constexpr
#define LUMEX_CONSTEXPR_DEFAULTED_CTOR
#define LUMEX_CONSTEXPR_DTOR constexpr
#define LUMEX_CONSTEXPR_DEFAULTED_DTOR
#define LUMEX_CONSTEXPR_VIRTUAL_DTOR
#define LUMEX_CONSTEXPR_DEFAULTED_VIRTUAL_DTOR
#elif __cplusplus == 201703L
// C++17: Base constexpr capabilities
#define LUMEX_CONSTEXPR_FUNCTION constexpr
#define LUMEX_CONSTEXPR_VIRTUAL_FUNCTION
#define LUMEX_CONSTEXPR_CTOR constexpr
#define LUMEX_CONSTEXPR_DEFAULTED_CTOR
#define LUMEX_CONSTEXPR_DTOR constexpr
#define LUMEX_CONSTEXPR_DEFAULTED_DTOR
#define LUMEX_CONSTEXPR_VIRTUAL_DTOR
#define LUMEX_CONSTEXPR_DEFAULTED_VIRTUAL_DTOR
#elif __cplusplus == 201103L || __cplusplus == 201402L
// C++11 and C++14: Very limited constexpr (only simple functions, no defaulted
// special members)
#define LUMEX_CONSTEXPR_FUNCTION constexpr
#define LUMEX_CONSTEXPR_VIRTUAL_FUNCTION
#define LUMEX_CONSTEXPR_CTOR constexpr
#define LUMEX_CONSTEXPR_DEFAULTED_CTOR
#define LUMEX_CONSTEXPR_DTOR
#define LUMEX_CONSTEXPR_DEFAULTED_DTOR
#define LUMEX_CONSTEXPR_VIRTUAL_DTOR
#define LUMEX_CONSTEXPR_DEFAULTED_VIRTUAL_DTOR
#else
// All the lower standards are not supported `constexpr` for these features,
// because `constexpr` was introduced in C++11
#define LUMEX_CONSTEXPR_FUNCTION
#define LUMEX_CONSTEXPR_VIRTUAL_FUNCTION
#define LUMEX_CONSTEXPR_CTOR
#define LUMEX_CONSTEXPR_DEFAULTED_CTOR
#define LUMEX_CONSTEXPR_DTOR
#define LUMEX_CONSTEXPR_DEFAULTED_DTOR
#define LUMEX_CONSTEXPR_VIRTUAL_DTOR
#define LUMEX_CONSTEXPR_DEFAULTED_VIRTUAL_DTOR
#endif

#define LUMEX_CONSTEXPR LUMEX_CONSTEXPR_FUNCTION

#endif // !LUMEX_CORE_UTILITY_MACROS_KEYWORDS_HPP
