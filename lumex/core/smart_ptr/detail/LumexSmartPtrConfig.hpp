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
 * @file LumexSmartPtrConfig.hpp
 * @brief Configuration macros of `core/smart_ptr`: exception handling,
 * ThreadSanitizer detection, debug checks and the
 * `std::enable_shared_from_this` policy.
 * @details Everything here is a macro; the header declares nothing else.
 *
 * - `LUMEX_SMART_PTR_HAS_EXCEPTIONS` is 1 when the translation unit is
 *   compiled with exceptions. Without them `LUMEX_SMART_PTR_TRY` and
 *   `LUMEX_SMART_PTR_CATCH_ALL` keep the code compiling (the handler is dead
 *   code), `LUMEX_SMART_PTR_RETHROW` is empty and
 *   `LUMEX_SMART_PTR_THROW (x)` ends the program with `std::abort ()`.
 * - `LUMEX_SMART_PTR_TSAN` is 1 under ThreadSanitizer. The tool does not
 *   model `std::atomic_thread_fence`, so the reference counters use an
 *   `acq_rel` read-modify-write there instead of `release` plus an acquire
 *   fence (see `ctl/LumexSmartPtrCounter.hpp`).
 * - `LUMEX_SMART_PTR_DEBUG_ASSERT (cond)` is `LUMEX_ASSERT (cond)` unless
 *   `NDEBUG` is defined, and nothing otherwise.
 * - `LUMEX_SMART_PTR_ALLOW_STD_ENABLE_SHARED_FROM_THIS`: define it before the
 *   first include to turn the `static_assert` that rejects
 *   `shared_ptr<T> (Y *)` for a class derived from
 *   `std::enable_shared_from_this` (and not from the module's own
 *   `enable_shared_from_this`) into silence. The pointer is then created
 *   normally and the standard base stays unset, so `shared_from_this ()` of
 *   the standard base throws `std::bad_weak_ptr`.
 */
#ifndef LUMEX_CORE_SMART_PTR_DETAIL_SMART_PTR_CONFIG_HPP
#define LUMEX_CORE_SMART_PTR_DETAIL_SMART_PTR_CONFIG_HPP

#include <cstdlib>

#include "lumex/core/utility/assert/LumexAssert.hpp"

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#define LUMEX_SMART_PTR_HAS_EXCEPTIONS 1
#else
#define LUMEX_SMART_PTR_HAS_EXCEPTIONS 0
#endif

#if LUMEX_SMART_PTR_HAS_EXCEPTIONS
#define LUMEX_SMART_PTR_TRY try
#define LUMEX_SMART_PTR_CATCH_ALL catch (...)
#define LUMEX_SMART_PTR_RETHROW throw
#define LUMEX_SMART_PTR_THROW(expression) throw expression
#else
#define LUMEX_SMART_PTR_TRY if (true)
#define LUMEX_SMART_PTR_CATCH_ALL else
#define LUMEX_SMART_PTR_RETHROW static_cast<void> (0)
#define LUMEX_SMART_PTR_THROW(expression)                                     \
  (static_cast<void> (sizeof (expression)), std::abort ())
#endif

#if defined(__SANITIZE_THREAD__)
#define LUMEX_SMART_PTR_TSAN 1
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define LUMEX_SMART_PTR_TSAN 1
#endif
#endif
#if !defined(LUMEX_SMART_PTR_TSAN)
#define LUMEX_SMART_PTR_TSAN 0
#endif

#if defined(NDEBUG)
#define LUMEX_SMART_PTR_DEBUG_ASSERT(cond) static_cast<void> (0)
#else
#define LUMEX_SMART_PTR_DEBUG_ASSERT(cond) LUMEX_ASSERT (cond)
#endif

#endif // !LUMEX_CORE_SMART_PTR_DETAIL_SMART_PTR_CONFIG_HPP
