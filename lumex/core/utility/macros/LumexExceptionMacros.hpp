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

/**
 * @file LumexExceptionMacros.hpp
 * @brief Macros that declare an exception class in one line.
 *
 * @details Dependency-free on purpose (only `<string>` and `<utility>`), so
 * any module can declare its exceptions without linking
 * `lumex::exceptions`. `LumexException.hpp` includes this header, so existing
 * users of `LUMEX_DEFINE_EXCEPTION` keep working.
 */
#ifndef LUMEX_CORE_UTILITY_MACROS_EXCEPTION_MACROS_HPP
#define LUMEX_CORE_UTILITY_MACROS_EXCEPTION_MACROS_HPP

#include <string>
#include <utility>

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

/**
 * @brief Declares `class exception_name : public inherit_from` with
 * constructors from `char const *`, `std::string const &` and
 * `std::string &&`, followed by the extra members passed as the trailing
 * arguments (functions, data members with default initializers, access
 * specifiers). The class body starts in the `public` section.
 * @details `inherit_from` must be constructible from `std::string`
 * (`std::runtime_error`, `std::logic_error`, `LumexBaseException`, ...).
 * The macro expands to a complete declaration including the trailing `;`.
 */
#define LUMEX_DEFINE_EXCEPTION_WITH_BODY(exception_name, inherit_from, ...)   \
  class exception_name : public inherit_from                                  \
  {                                                                           \
  public:                                                                     \
    exception_name (char const *message) : inherit_from (message) {}          \
    exception_name (std::string const &message) : inherit_from (message) {}   \
    exception_name (std::string &&message)                                    \
        : inherit_from (std::move (message))                                  \
    {                                                                         \
    }                                                                         \
    __VA_ARGS__                                                               \
  };

/**
 * @brief Declares `class exception_name : public inherit_from` with the
 * three message constructors and nothing else.
 */
#define LUMEX_DEFINE_EXCEPTION(exception_name, inherit_from)                  \
  LUMEX_DEFINE_EXCEPTION_WITH_BODY (exception_name, inherit_from, )

// NOLINTEND(cppcoreguidelines-macro-usage)

#endif // !LUMEX_CORE_UTILITY_MACROS_EXCEPTION_MACROS_HPP
