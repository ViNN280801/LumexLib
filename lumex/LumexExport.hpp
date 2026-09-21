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

#ifndef LUMEX_EXPORT_HPP
#define LUMEX_EXPORT_HPP

#ifdef __cplusplus
#define LUMEX_EXTERN_C_BEGIN                                                  \
  extern "C"                                                                  \
  {
#define LUMEX_EXTERN_C_END }
#else
#define LUMEX_EXTERN_C_BEGIN
#define LUMEX_EXTERN_C_END
#endif

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#ifdef _MSC_VER // MSVC compiler
#ifdef LUMEX_EXPORTS
#define LUMEX_API __declspec (dllexport)
#else
#define LUMEX_API __declspec (dllimport)
#endif
#else
// MinGW or other Windows compilers
#ifdef LUMEX_EXPORTS
#define LUMEX_API __attribute__ ((dllexport))
#else
#define LUMEX_API __attribute__ ((dllimport))
#endif
#endif
#else
// UNIX
#ifdef LUMEX_EXPORTS
#define LUMEX_API __attribute__ ((visibility ("default")))
#else
#define LUMEX_API
#endif
#endif

// Per-module export for LumexCore_utility. CMake defines <target>_EXPORTS when
// building a shared library, so the utility DLL exports these symbols while
// every other DLL/executable imports them. Unlike the generic LUMEX_API (which
// keys off LUMEX_EXPORTS, defined for ALL shared targets), this pins symbols
// to exactly one module so header-only classes with out-of-line static data
// members are not re-exported from every DLL that happens to include the
// header.
#if defined(LumexCore_utility_EXPORTS)
#define LUMEX_UTILITY_API __declspec (dllexport)
#elif defined(_WIN32) || defined(__CYGWIN__)
#define LUMEX_UTILITY_API __declspec (dllimport)
#else
#define LUMEX_UTILITY_API
#endif

#ifdef LUMEX_IMPLEMENTATION
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)                   \
    || defined(__NT__) && defined(_MSC_VER)
// Force C linkage for compatibility with legacy applications
#define LUMEX_PUBLIC_C_API extern "C" __declspec (dllexport)
#define LUMEX_PUBLIC_API __declspec (dllexport)
#elif _WIN32
#define LUMEX_PUBLIC_C_API extern "C" __attribute__ ((dllexport))
#define LUMEX_PUBLIC_API __attribute__ ((dllexport))
#else
#define LUMEX_PUBLIC_C_API extern "C" __attribute__ ((visibility ("default")))
#define LUMEX_PUBLIC_API __attribute__ ((visibility ("default")))
#endif
#else
/// Macros for marking functions that should be available from outside.
#define LUMEX_PUBLIC_C_API extern "C" LUMEX_API
#define LUMEX_PUBLIC_API LUMEX_API
#endif

#endif // !LUMEX_EXPORT_HPP
