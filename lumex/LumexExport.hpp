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
 * @file LumexExport.hpp
 * @brief Export and linkage macros of LumexLib: `LUMEX_API`,
 * `LUMEX_PUBLIC_API`, `LUMEX_PUBLIC_C_API`, `LUMEX_UTILITY_API`,
 * `LUMEX_STRING_VIEW_API`, `LUMEX_HAZARD_POINTER_API`, `LUMEX_CONTRACTS_API`,
 * `LUMEX_EXTERN_C_BEGIN` and `LUMEX_EXTERN_C_END`.
 * @details `LUMEX_API` marks a class or function of a shared library: on
 * Windows it is `dllexport` while `LUMEX_EXPORTS` is defined and `dllimport`
 * otherwise, elsewhere it gives default visibility while `LUMEX_EXPORTS` is
 * defined and is empty otherwise. `LUMEX_PUBLIC_API` and `LUMEX_PUBLIC_C_API`
 * (the latter with C linkage) export unconditionally in a source file that
 * defines `LUMEX_IMPLEMENTATION` before its includes, and equal `LUMEX_API`
 * everywhere else. `LUMEX_UTILITY_API` exports only from the utility library
 * itself (`LumexCore_utility_EXPORTS` defined), so classes with out-of-line
 * static data members are not exported again by every library that includes
 * their header. `LUMEX_STRING_VIEW_API` does the same for the string_view
 * library (`LumexCore_string_view_EXPORTS`): its two view classes and stream
 * inserters are exported only from there. `LUMEX_HAZARD_POINTER_API` marks the
 * free functions of the hazard pointer library
 * (`LumexCore_hazard_pointer_EXPORTS`) the same way, and `LUMEX_CONTRACTS_API`
 * the free functions of the contracts library (`LumexCore_contracts_EXPORTS`).
 * `LUMEX_EXTERN_C_BEGIN` and `LUMEX_EXTERN_C_END` open and close an `extern
 * "C"` block when compiled as C++.
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
// header. __declspec exists only for Windows targets; ELF builds of the
// utility library give the same symbols default visibility instead.
#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(LumexCore_utility_EXPORTS)
#define LUMEX_UTILITY_API __declspec (dllexport)
#else
#define LUMEX_UTILITY_API __declspec (dllimport)
#endif
#elif defined(LumexCore_utility_EXPORTS)
#define LUMEX_UTILITY_API __attribute__ ((visibility ("default")))
#else
#define LUMEX_UTILITY_API
#endif

// Per-module export for LumexCore_string_view, like LUMEX_UTILITY_API: the
// string_view library exports its two view classes and their stream inserters,
// every other DLL that includes the headers imports them. With the generic
// LUMEX_API (dllexport in every DLL built with LUMEX_EXPORTS) each of those
// DLLs would export the inline members and operators of the view again.
#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(LumexCore_string_view_EXPORTS)
#define LUMEX_STRING_VIEW_API __declspec (dllexport)
#else
#define LUMEX_STRING_VIEW_API __declspec (dllimport)
#endif
#elif defined(LumexCore_string_view_EXPORTS)
#define LUMEX_STRING_VIEW_API __attribute__ ((visibility ("default")))
#else
#define LUMEX_STRING_VIEW_API
#endif

// Per-module export for LumexCore_hazard_pointer, like LUMEX_STRING_VIEW_API:
// the library exports its engine functions (free functions only), every other
// DLL that includes the headers imports them. One engine per process is a
// safety property of hazard pointers: a reader in one DLL and a reclaimer in
// another must see the same slots, so the engine state lives in this library
// only.
#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(LumexCore_hazard_pointer_EXPORTS)
#define LUMEX_HAZARD_POINTER_API __declspec (dllexport)
#else
#define LUMEX_HAZARD_POINTER_API __declspec (dllimport)
#endif
#elif defined(LumexCore_hazard_pointer_EXPORTS)
#define LUMEX_HAZARD_POINTER_API __attribute__ ((visibility ("default")))
#else
#define LUMEX_HAZARD_POINTER_API
#endif

// Per-module export for LumexCore_contracts, like LUMEX_HAZARD_POINTER_API:
// the library exports the free functions of the violation handler (the handler
// slot and the default handler), every other DLL that includes the headers
// imports them. The handler is one per process, so its state lives in this
// library only.
#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(LumexCore_contracts_EXPORTS)
#define LUMEX_CONTRACTS_API __declspec (dllexport)
#else
#define LUMEX_CONTRACTS_API __declspec (dllimport)
#endif
#elif defined(LumexCore_contracts_EXPORTS)
#define LUMEX_CONTRACTS_API __attribute__ ((visibility ("default")))
#else
#define LUMEX_CONTRACTS_API
#endif

#ifdef LUMEX_IMPLEMENTATION
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)                   \
    || defined(__NT__) && defined(_MSC_VER)
// Force C linkage for compatibility with legacy applications
#define LUMEX_PUBLIC_C_API extern "C" __declspec (dllexport)
#define LUMEX_PUBLIC_API __declspec (dllexport)
#elif defined(_WIN32)
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
