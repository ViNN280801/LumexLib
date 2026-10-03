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

/**
 * @file CrossModule.hpp
 * @brief Functions of the two shared libraries of the
 * LumexCMake.consumer_atomic_cross_module fixture.
 * @details Both libraries are compiled with hidden visibility; only these
 * functions are exported.
 */
#ifndef LUMEX_TESTS_CMAKE_CONSUMER_ATOMIC_CROSS_MODULE_HPP
#define LUMEX_TESTS_CMAKE_CONSUMER_ATOMIC_CROSS_MODULE_HPP

#include <memory>

#include "lumex/core/atomic/LumexAtomic"

#if defined(_WIN32)
#define CROSS_MODULE_API __declspec (dllexport)
#else
#define CROSS_MODULE_API __attribute__ ((visibility ("default")))
#endif

/// In the sleeper library: blocks until the value differs from @p old.
CROSS_MODULE_API void sleeper_wait (atomic_shared_ptr<int> &a,
                                    std::shared_ptr<int> old);

/// In the sleeper library: stores and loads @p rounds times.
CROSS_MODULE_API void sleeper_hammer (atomic_shared_ptr<int> &a, int rounds);

/// In the waker library: stores @p value and wakes every waiter.
CROSS_MODULE_API void waker_publish (atomic_shared_ptr<int> &a, int value);

/// In the waker library: stores and loads @p rounds times.
CROSS_MODULE_API void waker_hammer (atomic_shared_ptr<int> &a, int rounds);

#endif // !LUMEX_TESTS_CMAKE_CONSUMER_ATOMIC_CROSS_MODULE_HPP
