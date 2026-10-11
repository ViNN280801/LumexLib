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
 * @file LumexSpanChecks.hpp
 * @brief The switch of the precondition checks of `span`: the macro
 * `LUMEX_SPAN_CHECKS` and the macro `LUMEX_SPAN_CHECKED` that the members use.
 * @details The preconditions of `std::span` (an index below `size ()`,
 * `front` and `back` of a span that is not empty, `first`, `last` and
 * `subspan` within the size, a count equal to a static extent, [span.cons],
 * [span.sub], [span.elem]) are checked when `LUMEX_SPAN_CHECKS` is 1 and are
 * not checked when it is 0. A violated check calls the handler of this
 * header, `detail::check_failed`, which prints
 * `LUMEX_SPAN_CHECKS: <condition> failed at <file>:<line>` to standard error
 * and calls `std::abort`, in every build mode. The handler is an inline
 * function of this header, so the module has no link dependency. With the
 * checks off the members are the code they were before the checks existed.
 */
#ifndef LUMEX_CORE_SPAN_VIEW_SPAN_CHECKS_HPP
#define LUMEX_CORE_SPAN_VIEW_SPAN_CHECKS_HPP

#include <cstdio>
#include <cstdlib>

#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

/**
 * @def LUMEX_SPAN_CHECKS
 * @brief 1 when the preconditions of `span` are checked, 0 when they are not.
 * @details The default is 1 when `NDEBUG` is not defined and 0 when it is. A
 * program may define the macro as 0 or 1 before the first include of the
 * library (`-DLUMEX_SPAN_CHECKS=1` keeps the checks in a release build).
 * Every translation unit of one program must see the same value: the members
 * of `span` are inline templates, and two units that differ would hand the
 * linker two bodies of one function (MSVC reports a mismatch at link time
 * through `detect_mismatch`, GCC and Clang do not detect it). A violated
 * check calls the inline handler of this header: it needs nothing to link.
 */
#ifndef LUMEX_SPAN_CHECKS
#if defined(NDEBUG)
#define LUMEX_SPAN_CHECKS 0
#else
#define LUMEX_SPAN_CHECKS 1
#endif
#endif

#if defined(_MSC_VER)
#if LUMEX_SPAN_CHECKS
#pragma detect_mismatch("LUMEX_SPAN_CHECKS", "1")
#else
#pragma detect_mismatch("LUMEX_SPAN_CHECKS", "0")
#endif
#endif

#if LUMEX_SPAN_CHECKS
namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace span
{
namespace view
{
namespace detail
{
/**
 * @brief The handler of a violated precondition: prints
 * `LUMEX_SPAN_CHECKS: <condition> failed at <file>:<line>` to standard error
 * and aborts. Not `constexpr`, so a violated check in a constant expression is
 * a compile error.
 * @param condition The text of the failed condition.
 * @param file The source file of the check.
 * @param line The line of the check.
 */
LUMEX_ATTRIBUTE_NORETURN inline void
check_failed (char const *condition, char const *file, int line) LUMEX_NOEXCEPT
{
  std::fprintf (stderr, "LUMEX_SPAN_CHECKS: %s failed at %s:%d\n", condition,
                file, line);
  std::abort ();
}
} // namespace detail
} // namespace view
} // namespace span
} // namespace core
} // namespace lumex
#endif

/**
 * @def LUMEX_SPAN_CHECKED
 * @brief `LUMEX_SPAN_CHECKED (condition, expression...)` is `expression`,
 * preceded by the check of `condition` when `LUMEX_SPAN_CHECKS` is 1.
 * @details An expression, so that it can stand in a `return` of a C++11
 * `constexpr` function and in a mem-initializer. In a constant expression a
 * true condition adds nothing and a false one is not a constant expression
 * (the handler is not `constexpr`): the compiler rejects the program. The
 * condition is evaluated once, or not at all when the checks are off; it may
 * not have a comma outside parentheses. The expression after the condition
 * may.
 */
#if LUMEX_SPAN_CHECKS
#define LUMEX_SPAN_CHECKED(condition, ...)                                    \
  (static_cast<void> ((condition)                                             \
                          ? static_cast<void> (0)                             \
                          : ::lumex::core::span::view::detail::check_failed ( \
                                #condition, __FILE__, __LINE__)),             \
   __VA_ARGS__)
#else
#define LUMEX_SPAN_CHECKED(condition, ...) (__VA_ARGS__)
#endif

#endif // !LUMEX_CORE_SPAN_VIEW_SPAN_CHECKS_HPP
