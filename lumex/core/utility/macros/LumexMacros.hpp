/**
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
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

#ifndef LUMEX_CORE_UTILITY_MACROS_MACROS_HPP
#define LUMEX_CORE_UTILITY_MACROS_MACROS_HPP

#if defined(_WIN32) || defined(__WIN32__) || defined(__WIN64__)               \
    || defined(__MINGW32__) || defined(__MINGW64__)
#define LUMEX_FUNCTION_NAME __FUNCSIG__
#elif defined(__linux__) || defined(__APPLE__) || defined(__MACH__)           \
    || defined(__MACOS__) || defined(__GNUC__) || defined(__clang__)
#define LUMEX_FUNCTION_NAME __PRETTY_FUNCTION__
#elif defined(__ICC) || defined(__INTEL_COMPILER)
#define LUMEX_FUNCTION_NAME __FUNCTION__
#else
#define LUMEX_FUNCTION_NAME __FUNCTION__
#endif

#define LUMEX_STRINGIZE_DETAIL(x) #x
#define LUMEX_STRINGIZE(x) LUMEX_STRINGIZE_DETAIL (x)

#define LUMEX_CONCAT_DETAIL(x, y) x##y
#define LUMEX_CONCAT(x, y) LUMEX_CONCAT_DETAIL (x, y)

#endif // !LUMEX_CORE_UTILITY_MACROS_MACROS_HPP
