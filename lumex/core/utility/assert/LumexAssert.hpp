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

#ifndef LUMEX_CORE_UTILITY_ASSERT_HPP
#define LUMEX_CORE_UTILITY_ASSERT_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/core/utility/macros/LumexKeywords.hpp"
LUMEX_EXTERN_C_BEGIN
/**
 * @brief Handles failed assertions within the Lumex codebase.
 *
 * This function serves as the central handler for assertion failures. It is
 * typically invoked by an assertion macro when a specified condition evaluates
 * to false. The handler receives details about the failed assertion, including
 * the assertion string itself, the source file where the failure occurred, and
 * the exact line number.
 *
 * Its primary responsibility is to process the assertion failure, which might
 * involve logging the details, breaking into a debugger, or terminating the
 * application. Implementations should provide clear diagnostic information
 * to aid in debugging.
 *
 * @param[in] assertion A null-terminated string representing the failed
 * assertion expression. This pointer is caller-owned and must remain valid for
 * the duration of the call.
 * @param[in] file A null-terminated string representing the name of the source
 * file where the assertion failed. This pointer is caller-owned.
 * @param[in] line The line number within the `file` where the assertion
 * failed.
 * @note This function does not return to the caller under normal assertion
 * failure scenarios, as it is expected to either terminate the program or
 * enter a debugger.
 * @note This function is declared `noexcept`, indicating that it does not
 * throw C++ exceptions. Any internal failures should be handled without
 * propagating exceptions.
 * @warning Calling this function directly is typically not recommended; it is
 * intended to be used by internal assertion macros or mechanisms.
 */
LUMEX_PUBLIC_API
void lumex_assert_handler (char const *assertion, char const *file,
                           int line) LUMEX_NOEXCEPT;
LUMEX_EXTERN_C_END

#define LUMEX_ASSERT(cond)                                                    \
  ((cond) ? static_cast<void> (0)                                             \
          : lumex_assert_handler (#cond, __FILE__, __LINE__))

#if __cplusplus >= 201703L
#define LUMEX_STATIC_ASSERT(cond) static_assert (cond)
#else
#define LUMEX_STATIC_ASSERT(cond) static_assert (cond, #cond)
#endif

// Variadic so commas in template argument lists remain part of the
// static_assert declaration instead of being parsed as macro arguments.
#define LUMEX_STATIC_ASSERT_MSG(...) static_assert (__VA_ARGS__)

#endif // !LUMEX_CORE_UTILITY_ASSERT_HPP
