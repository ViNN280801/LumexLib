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
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef LUMEX_TESTS_SUPPORT_PERF_SKIP_HPP
#define LUMEX_TESTS_SUPPORT_PERF_SKIP_HPP

/*
 * Wall-clock Perf_* thresholds are Release-only and sanitizer-free.
 * Debug plus ASan/UBSan/TSan/MSan inflate elapsed time (instrumentation,
 * allocator hooks, no inlining). A GTEST_SKIP at the top of the function
 * still leaves the timed body compiled and trips MSVC C4702. Gate the
 * body with #if LUMEX_PERF_WALL_CLOCK_ENABLED and skip in #else.
 */

#if !defined(NDEBUG)
#define LUMEX_PERF_WALL_CLOCK_ENABLED 0
#elif defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_UNDEFINED__)        \
    || defined(__SANITIZE_THREAD__) || defined(__SANITIZE_MEMORY__)
#define LUMEX_PERF_WALL_CLOCK_ENABLED 0
#elif defined(__clang__) && defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)       \
    || __has_feature(memory_sanitizer)
#define LUMEX_PERF_WALL_CLOCK_ENABLED 0
#else
#define LUMEX_PERF_WALL_CLOCK_ENABLED 1
#endif
#else
#define LUMEX_PERF_WALL_CLOCK_ENABLED 1
#endif

#endif // !LUMEX_TESTS_SUPPORT_PERF_SKIP_HPP
