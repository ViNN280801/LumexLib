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

#ifndef LUMEX_CORE_UTILITY_MACROS_CONSTANT_MACROS_HPP
#define LUMEX_CORE_UTILITY_MACROS_CONSTANT_MACROS_HPP

#include "lumex/core/utility/macros/LumexKeywords.hpp"

#define LUMEX_BASE_CONSTANT static LUMEX_INLINE_VARIABLE const

/**
 * @brief Macro for declaring static constants, guaranteeing static
 * initialization.
 *
 * @details This macro provides a unified way to declare static constants
 *          (class members or global variables) with guaranteed initialization
 * at compile time. The behavior of the macro depends on the used C++ standard:
 *
 *          - **C++20 and higher (`-std=c++20` or newer):**
 *            The macro expands to `static [inline] const constinit`. In this
 * case, `constinit` is used, which **guarantees** that the variable will be
 * initialized during static initialization (zero-initialization +
 * constant-initialization). If the variable requires dynamic initialization
 * (i.e. its value cannot be computed at compile time), this will result in a
 * compilation error. The key difference from `constexpr` is that `constinit`
 * does not make the variable `const`-qualified essentially, i.e. its value can
 * be changed at runtime after static initialization, although the use of
 * `const` in the macro prevents this.
 *
 *          - **C++11, C++14, C++17 (`-std=c++11`, `-std=c++14`,
 * `-std=c++17`):** The macro expands to `static [inline] const constexpr`. In
 * these standards, `constexpr` is used to indicate that the variable can be
 * initialized at compile time and its value is a constant expression. Starting
 * with C++17, `inline` for static member variables allows defining them
 * directly in headers without the need to repeat the definition in the `.cpp`
 * file.
 *
 *          - **Before C++11:**
 *            The macro expands to `static const`. In this case, there is no
 * guarantee that the variable will be initialized at compile time for all
 * types, and the behavior will depend on the specific compiler and the type of
 * data.
 *
 * @warning
 *            The macro expands to `static const`. In this case, there is no
 * guarantee that the variable will be initialized at compile time for all
 * types, and the behavior will depend on the specific compiler and the type of
 * data.
 *
 * @warning
 *   - **Compilation error with `constinit` (C++20+):** Do not use this macro
 * for variables that require dynamic initialization. For example, if the
 * initializer calls a function that is not `constexpr`, or performs operations
 * that cannot be computed at compile time.
 *   - **Variable type:** The declared variable must have a type that can be
 * statically initialized (i.e. be part of a constant expression).
 *   - **Mutability:** Although `constinit` itself does not make the variable
 * immutable, `const` in the composition of this macro prevents further
 * changes.
 *
 * @example Examples of usage:
 * @code
 * class MyConfig {
 * public:
 *   // Correct usage: integers and simple constant expressions
 *   LUMEX_CONSTINIT_CONSTANT int kBufferSize = 1024;
 *   LUMEX_CONSTINIT_CONSTANT double kPi = 3.14159265358979323846;
 *
 *   // Correct usage: objects of trivial types with constexpr constructors
 *   struct Point {
 *     int x, y;
 *     constexpr Point(int px, int py) : x(px), y(py) {}
 *   };
 *   LUMEX_CONSTINIT_CONSTANT Point kOrigin = {0, 0};
 *
 *   // Correct usage (starting with C++20 for std::string in certain contexts,
 *   // but for LUMEX_CONSTINIT_CONSTANT this will be const char*):
 *   // LUMEX_CONSTINIT_CONSTANT std::string kAppName = "MyApp"; // ERROR,
 * std::string requires dynamic initialization
 *                                                                         //
 * even if the constructor is constexpr, self-placement
 *                                                                         //
 * of the string in the heap cannot be constinit.
 *                                                                         //
 * Use LUMEX_CONST_STR for string literals.
 *
 *   // Incorrect usage (will cause a compilation error with C++20 constinit):
 *   int getRuntimeValue() { return 42; }
 *   // LUMEX_CONSTINIT_CONSTANT int kRuntimeValue = getRuntimeValue(); //
 * ERROR: requires dynamic initialization
 *
 *   // Incorrect usage (for C++11-17 with constexpr)
 *   // LUMEX_CONSTINIT_CONSTANT std::vector<int> kMyVector = {1, 2, 3}; //
 * ERROR: std::vector is not constexpr-initializable (until C++20)
 *                                                                                 //
 * even in C++20, if it is not trivially copyable/movable,
 *                                                                                 //
 * constinit will not work for the std::vector object.
 * };
 * @endcode
 *
 * @note Remember the differences between `constinit` (C++20+) and `constexpr`
 * (C++11-C++17) in the context of static initialization. This macro abstracts
 * these differences, but the fundamental limitations of each specifier remain
 * in effect.
 *
 * @see https://en.cppreference.com/w/cpp/language/constinit
 * @see https://en.cppreference.com/w/cpp/language/constexpr
 */
#define LUMEX_CONSTINIT_CONSTANT LUMEX_BASE_CONSTANT LUMEX_CONSTINIT
#define LUMEX_STRING_CONSTANT LUMEX_BASE_CONSTANT char *LUMEX_RESTRICT const

#if __cplusplus >= 202002L
#define LUMEX_CONSTEVAL_FUNCTION LUMEX_CONSTEVAL
#else
#define LUMEX_CONSTEVAL_FUNCTION LUMEX_CONSTEXPR_FUNCTION
#endif

#define LUMEX_CONST LUMEX_BASE_CONSTANT
#define LUMEX_CONST_STR LUMEX_STRING_CONSTANT
#define LUMEX_CONST_NUM LUMEX_CONSTINIT_CONSTANT

#endif // !LUMEX_CORE_UTILITY_MACROS_CONSTANT_MACROS_HPP
