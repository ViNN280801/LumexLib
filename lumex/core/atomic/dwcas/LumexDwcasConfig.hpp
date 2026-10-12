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
 * @file LumexDwcasConfig.hpp
 * @brief Detection macros of the 128-bit compare-and-swap layer: whether the
 * layer exists on this target and which backend implements it.
 * @details The layer (`dwcas_word` in `LumexDwcasWord.hpp`) exists on x86-64
 * only: 64-bit pointers, no x32 ABI, with GCC, Clang (clang-cl included) or
 * MSVC. AArch64, the 32-bit targets and every other architecture are not
 * supported: there `LUMEX_ATOMIC_HAS_DWCAS` is 0 and the class is not
 * declared, so naming it is a compile error and code can branch on the
 * macro. `LUMEX_ATOMIC_DISABLE_DWCAS`, defined before the first include,
 * switches the layer off on a supported target as well.
 *
 * The backend is chosen at compile time:
 *
 * | Value of `LUMEX_DWCAS_BACKEND` | Backend | Chosen by default for |
 * | --- | --- | --- |
 * | `LUMEX_DWCAS_BACKEND_ASM` (1) | `lock cmpxchg16b` in GNU inline assembly |
 * GCC, Clang, clang-cl, MinGW | | `LUMEX_DWCAS_BACKEND_MSVC` (2) |
 * `_InterlockedCompareExchange128` | MSVC (`cl.exe`) | |
 * `LUMEX_DWCAS_BACKEND_BUILTIN` (3) | `__atomic_*` on `unsigned __int128` |
 * nothing (opt in) |
 *
 * No compiler flag is needed for the first two: `cmpxchg16b` is written out
 * by hand, so neither `-mcx16` nor libatomic is involved. The built-in
 * backend exists for ThreadSanitizer, which does not see inline assembly:
 * GCC sends every 16-byte `__atomic_*` operation to libatomic (link
 * `-latomic`), Clang does the same unless `-mcx16` is given. Define
 * `LUMEX_DWCAS_BACKEND` to 3 (or to 2 to exercise the MSVC wrapper on
 * another compiler) before the first include to select a backend by hand;
 * a value the compiler cannot implement is an `#error`. Each backend lives in
 * its own inline namespace, so translation units that disagree on the backend
 * do not share function definitions; the layout of the word is the same in
 * all of them.
 *
 * Everything in this header is a macro, so it includes nothing.
 */
#ifndef LUMEX_CORE_ATOMIC_DWCAS_DWCAS_CONFIG_HPP
#define LUMEX_CORE_ATOMIC_DWCAS_DWCAS_CONFIG_HPP

/// Value of `LUMEX_DWCAS_BACKEND` when the layer does not exist.
#define LUMEX_DWCAS_BACKEND_NONE 0
/// `lock cmpxchg16b` in GNU inline assembly.
#define LUMEX_DWCAS_BACKEND_ASM 1
/// `_InterlockedCompareExchange128` of MSVC.
#define LUMEX_DWCAS_BACKEND_MSVC 2
/// `__atomic_*` builtins on `unsigned __int128` (libatomic with GCC).
#define LUMEX_DWCAS_BACKEND_BUILTIN 3

#if defined(LUMEX_ATOMIC_DISABLE_DWCAS)
#define LUMEX_ATOMIC_DWCAS_TARGET_OK 0
#elif (defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)           \
       || defined(_M_AMD64))                                                  \
    && !defined(_M_ARM64EC) && !defined(__ILP32__)
#if defined(__GNUC__) || defined(__clang__)
#define LUMEX_ATOMIC_DWCAS_TARGET_OK 1
#define LUMEX_ATOMIC_DWCAS_DEFAULT_BACKEND LUMEX_DWCAS_BACKEND_ASM
#elif defined(_MSC_VER)
#define LUMEX_ATOMIC_DWCAS_TARGET_OK 1
#define LUMEX_ATOMIC_DWCAS_DEFAULT_BACKEND LUMEX_DWCAS_BACKEND_MSVC
#else
#define LUMEX_ATOMIC_DWCAS_TARGET_OK 0
#endif
#else
#define LUMEX_ATOMIC_DWCAS_TARGET_OK 0
#endif

/**
 * @brief 1 when `dwcas_word` is declared, 0 when it is not (AArch64, 32-bit
 * and other targets, an unknown compiler, or `LUMEX_ATOMIC_DISABLE_DWCAS`).
 */
#define LUMEX_ATOMIC_HAS_DWCAS LUMEX_ATOMIC_DWCAS_TARGET_OK

#if LUMEX_ATOMIC_HAS_DWCAS

#if !defined(LUMEX_DWCAS_BACKEND)
#define LUMEX_DWCAS_BACKEND LUMEX_ATOMIC_DWCAS_DEFAULT_BACKEND
#elif LUMEX_DWCAS_BACKEND == LUMEX_DWCAS_BACKEND_ASM
#if !(defined(__GNUC__) || defined(__clang__))
#error "LUMEX_DWCAS_BACKEND_ASM needs GNU inline assembly (GCC or Clang)"
#endif
#elif LUMEX_DWCAS_BACKEND == LUMEX_DWCAS_BACKEND_MSVC
// The intrinsic comes from <intrin.h>; whether it exists is the compiler's
// business.
#elif LUMEX_DWCAS_BACKEND == LUMEX_DWCAS_BACKEND_BUILTIN
#if !((defined(__GNUC__) || defined(__clang__)) && defined(__SIZEOF_INT128__))
#error "LUMEX_DWCAS_BACKEND_BUILTIN needs GCC or Clang with unsigned __int128"
#endif
#else
#error "LUMEX_DWCAS_BACKEND must be 1 (asm), 2 (msvc) or 3 (builtin)"
#endif

#else // !LUMEX_ATOMIC_HAS_DWCAS

#if defined(LUMEX_DWCAS_BACKEND)
#undef LUMEX_DWCAS_BACKEND
#endif
#define LUMEX_DWCAS_BACKEND LUMEX_DWCAS_BACKEND_NONE

#endif // LUMEX_ATOMIC_HAS_DWCAS

#endif // !LUMEX_CORE_ATOMIC_DWCAS_DWCAS_CONFIG_HPP
