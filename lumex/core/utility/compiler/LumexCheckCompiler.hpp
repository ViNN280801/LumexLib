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

#ifndef LUMEX_CORE_UTILITY_COMPILER_HPP
#define LUMEX_CORE_UTILITY_COMPILER_HPP

/**
 * @file LumexCheckCompiler.hpp
 * @brief Compiler identification and version macros.
 * @details
 *          - `LUMEX_COMPILER_MSVC`, `LUMEX_COMPILER_CLANG`,
 *            `LUMEX_COMPILER_GCC`: defined as `1` for the compiler in use,
 *            otherwise not defined. `LUMEX_COMPILER_VERSION` is that
 *            compiler's own number: `_MSC_VER` for MSVC, `MAJOR * 10000 +
 *            MINOR * 100 + PATCH` for Clang and GCC.
 *          - `LUMEX_COMPILER_IS_MSVC()`, `LUMEX_COMPILER_IS_CLANG()`,
 *            `LUMEX_COMPILER_IS_GCC()`: the constant `1` or `0`, safe in
 *            `#if` under `-Wundef` and in ordinary code.
 *          - `LUMEX_GCC_AT_LEAST (major, minor)`, `LUMEX_GCC_BEFORE (...)`,
 *            `LUMEX_CLANG_AT_LEAST (...)`, `LUMEX_CLANG_BEFORE (...)`: false
 *            for any other compiler. Meant for `#if`.
 *
 *          clang-cl defines `_MSC_VER` and counts as MSVC here, so the Clang
 *          version checks are false for it. Apple Clang numbers its releases
 *          differently from LLVM Clang.
 *
 *          Use the version checks only to work around a defect of a specific
 *          compiler release. To find out whether a language or library
 *          feature is available, use `LumexCheckFeatures.hpp` instead.
 *
 * @see Compiler macros: https://sourceforge.net/p/predef/wiki/Compilers/
 */

#if defined(_MSC_VER)
#define LUMEX_COMPILER_MSVC 1
#define LUMEX_COMPILER_VERSION _MSC_VER
#elif defined(__clang__)
#define LUMEX_COMPILER_CLANG 1
#define LUMEX_COMPILER_VERSION                                                \
  (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#elif defined(__GNUC__)
#define LUMEX_COMPILER_GCC 1
#define LUMEX_COMPILER_VERSION                                                \
  (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#endif

#if defined(LUMEX_COMPILER_MSVC)
#define LUMEX_COMPILER_IS_MSVC() 1
#else
#define LUMEX_COMPILER_IS_MSVC() 0
#endif

#if defined(LUMEX_COMPILER_CLANG)
#define LUMEX_COMPILER_IS_CLANG() 1
#else
#define LUMEX_COMPILER_IS_CLANG() 0
#endif

#if defined(LUMEX_COMPILER_GCC)
#define LUMEX_COMPILER_IS_GCC() 1
#else
#define LUMEX_COMPILER_IS_GCC() 0
#endif

/// True for GCC `major.minor` and later; false for any other compiler.
#define LUMEX_GCC_AT_LEAST(major, minor)                                      \
  (LUMEX_COMPILER_IS_GCC ()                                                   \
   && LUMEX_COMPILER_VERSION >= ((major) * 10000 + (minor) * 100))

/// True for GCC releases before `major.minor`; false for any other compiler.
#define LUMEX_GCC_BEFORE(major, minor)                                        \
  (LUMEX_COMPILER_IS_GCC ()                                                   \
   && LUMEX_COMPILER_VERSION < ((major) * 10000 + (minor) * 100))

/// True for Clang `major.minor` and later; false for any other compiler.
#define LUMEX_CLANG_AT_LEAST(major, minor)                                    \
  (LUMEX_COMPILER_IS_CLANG ()                                                 \
   && LUMEX_COMPILER_VERSION >= ((major) * 10000 + (minor) * 100))

/// True for Clang releases before `major.minor`; false for any other compiler.
#define LUMEX_CLANG_BEFORE(major, minor)                                      \
  (LUMEX_COMPILER_IS_CLANG ()                                                 \
   && LUMEX_COMPILER_VERSION < ((major) * 10000 + (minor) * 100))

#endif // LUMEX_CORE_UTILITY_COMPILER_HPP
