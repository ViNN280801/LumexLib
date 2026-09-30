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
// The branches test ranges, not exact values: compilers report values
// between the published ones for unfinished standards (GCC 13 -std=c++23
// gives 202100L, MSVC /std:c++latest 202004L), and those must take the
// branch of the newest standard they complete.
#elif __cplusplus >= 202002L
// C++20: Virtual functions constexpr, but NOT virtual destructors
#define LUMEX_CONSTEXPR_FUNCTION constexpr
#define LUMEX_CONSTEXPR_VIRTUAL_FUNCTION constexpr
#define LUMEX_CONSTEXPR_CTOR constexpr
#define LUMEX_CONSTEXPR_DEFAULTED_CTOR
#define LUMEX_CONSTEXPR_DTOR constexpr
#define LUMEX_CONSTEXPR_DEFAULTED_DTOR
#define LUMEX_CONSTEXPR_VIRTUAL_DTOR
#define LUMEX_CONSTEXPR_DEFAULTED_VIRTUAL_DTOR
#elif __cplusplus >= 201703L
// C++17: Base constexpr capabilities. Destructors cannot be constexpr before
// C++20 (P0784R7).
#define LUMEX_CONSTEXPR_FUNCTION constexpr
#define LUMEX_CONSTEXPR_VIRTUAL_FUNCTION
#define LUMEX_CONSTEXPR_CTOR constexpr
#define LUMEX_CONSTEXPR_DEFAULTED_CTOR
#define LUMEX_CONSTEXPR_DTOR
#define LUMEX_CONSTEXPR_DEFAULTED_DTOR
#define LUMEX_CONSTEXPR_VIRTUAL_DTOR
#define LUMEX_CONSTEXPR_DEFAULTED_VIRTUAL_DTOR
#elif __cplusplus >= 201103L
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

// For functions whose body needs the relaxed constexpr rules of C++14
// (N3652): more than one statement, local variables, loops, a switch, or a
// void return type. C++11 allows none of these in a constexpr function, so
// there the specifier is dropped. A member function defined in its class
// stays implicitly inline; a namespace-scope function in a header needs an
// explicit `inline` next to the macro, because without `constexpr` it would
// be defined in every translation unit that includes the header.
#if __cplusplus >= 201402L
#define LUMEX_CONSTEXPR_CXX14 constexpr
#else
#define LUMEX_CONSTEXPR_CXX14
#endif

#endif // !LUMEX_CORE_UTILITY_MACROS_KEYWORDS_HPP
