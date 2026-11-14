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
 * @file LumexKeywords.hpp
 * @brief Macros that spell C++ keywords and specifiers only where the compiled
 * standard supports them, so one source compiles from C++11 to C++26.
 * @details `LUMEX_NOEXCEPT` and `LUMEX_NOEXCEPT_IF(...)` are `noexcept` and
 * its conditional form. `LUMEX_CONSTEXPR` and the `LUMEX_CONSTEXPR_*` family
 * for functions, constructors, destructors and virtual functions expand to
 * `constexpr` only from the standard the header picks for that kind of
 * declaration: C++11 for functions and constructors, C++20 for virtual
 * functions and destructors, C++23 for defaulted special members and virtual
 * destructors. `LUMEX_CONSTEXPR_CXX14` marks a function body that needs the
 * relaxed rules of C++14. `LUMEX_CONSTEXPR_IF` is `if constexpr` from C++17
 * and a plain `if` before, so both branches must compile.
 * `LUMEX_INLINE_VARIABLE` is `inline` from C++17 and empty before;
 * `LUMEX_CONSTINIT` and `LUMEX_CONSTEVAL` are the C++20 keywords and
 * `constexpr` before; `LUMEX_RESTRICT` is the compiler's `__restrict`
 * extension. `LUMEX_COMPILER_HAS_EXCEPTIONS` and `LUMEX_HAS_EXCEPTIONS` are
 * the exception mode of the translation unit and the switch of the throwing
 * functions of the header-only modules. The header includes nothing.
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

/**
 * @def LUMEX_COMPILER_HAS_EXCEPTIONS
 * @brief 1 when the compiler compiles `try`, `catch` and `throw` (GCC and
 * Clang: `__cpp_exceptions` or `__EXCEPTIONS`; MSVC: `_CPPUNWIND`), otherwise
 * 0 (`-fno-exceptions`, MSVC without `/EHsc`).
 * @details A fact of the translation unit that a program cannot override: code
 * that contains a `try` block (the reference counters of `smart_ptr`, the
 * inserter of the string views, the paths of `expected` that put the old
 * contents back) is compiled by it.
 */
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#define LUMEX_COMPILER_HAS_EXCEPTIONS 1
#else
#define LUMEX_COMPILER_HAS_EXCEPTIONS 0
#endif

/**
 * @def LUMEX_HAS_EXCEPTIONS
 * @brief 1 when the throwing functions of the library throw, 0 when they call
 * `std::terminate`: `expected::value ()`, `optional::value ()`, `span::at ()`,
 * `lumex_string_view::at ()`, `substr ()`, `copy ()` and the `compare`
 * overloads with a position.
 * @details The default is `LUMEX_COMPILER_HAS_EXCEPTIONS`. A program may
 * define the macro as 0 before the first include of the library to switch
 * every module to the terminating form with exceptions on. Each module has a
 * macro of its own (`LUMEX_EXPECTED_HAS_EXCEPTIONS`,
 * `LUMEX_OPTIONAL_HAS_EXCEPTIONS`, `LUMEX_SPAN_HAS_EXCEPTIONS`,
 * `LUMEX_STRING_VIEW_HAS_EXCEPTIONS`) that defaults to this one and overrides
 * it for that module. Defined as 1 when the compiler has no exceptions it does
 * not compile. `smart_ptr` follows `LUMEX_COMPILER_HAS_EXCEPTIONS`, not this
 * macro: its `try` blocks must not disappear while exceptions are on.
 */
#ifndef LUMEX_HAS_EXCEPTIONS
#define LUMEX_HAS_EXCEPTIONS LUMEX_COMPILER_HAS_EXCEPTIONS
#endif
#if LUMEX_HAS_EXCEPTIONS && !LUMEX_COMPILER_HAS_EXCEPTIONS
#error                                                                        \
    "LUMEX_HAS_EXCEPTIONS is 1 but the compiler has exceptions off: a program built without exceptions cannot throw"
#endif

#endif // !LUMEX_CORE_UTILITY_MACROS_KEYWORDS_HPP
