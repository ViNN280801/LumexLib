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
 * through `detect_mismatch` in the translation units of a program; the
 * compiled modules of LumexLib do not record the value, see
 * `LUMEX_SPAN_NO_MISMATCH_GUARD`; GCC and Clang do not detect it). A violated
 * check calls the inline handler of this header: it needs nothing to link.
 */
#ifndef LUMEX_SPAN_CHECKS
#if defined(NDEBUG)
#define LUMEX_SPAN_CHECKS 0
#else
#define LUMEX_SPAN_CHECKS 1
#endif
#endif

/**
 * @def LUMEX_SPAN_NO_MISMATCH_GUARD
 * @brief Defined to drop the `detect_mismatch` record of `LUMEX_SPAN_CHECKS`
 * from a translation unit.
 * @details On MSVC every translation unit that includes this header records
 * the value of `LUMEX_SPAN_CHECKS` (`#pragma detect_mismatch`), and the linker
 * rejects objects that disagree (LNK2038). The compiled modules of LumexLib
 * include this header too (through `LumexUtility`), and their inline span code
 * calls only `data ()` and `size ()`, which are never checked, so what they
 * record would only make a program built with `-DLUMEX_SPAN_CHECKS=1` fail to
 * link against a prebuilt module. The build of the library defines this macro
 * on its own targets; a consumer does not define it, so the mismatch between
 * the consumer's translation units is still found. The macro has no other
 * effect.
 * @def LUMEX_SPAN_MISMATCH_GUARD
 * @brief 1 when a translation unit records the value of `LUMEX_SPAN_CHECKS`
 * for the linker (MSVC only), 0 when it does not. Informational: computed from
 * `LUMEX_SPAN_NO_MISMATCH_GUARD`, defined on every compiler so that the
 * plumbing can be tested without MSVC; on a compiler other than MSVC nothing
 * is recorded whatever the value is.
 */
#if defined(LUMEX_SPAN_NO_MISMATCH_GUARD)
#define LUMEX_SPAN_MISMATCH_GUARD 0
#else
#define LUMEX_SPAN_MISMATCH_GUARD 1
#endif

#if defined(_MSC_VER) && LUMEX_SPAN_MISMATCH_GUARD
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
 * and aborts. It returns `void`, so the check is the conditional
 * `condition ? static_cast<void> (0) : check_failed (...)`: both operands are
 * `void` and no cast of a `void` expression is needed (`-Wuseless-cast`), and
 * `[[noreturn]]` goes with a `void` return on MSVC (C4646). Not `constexpr`,
 * so a violated check in a constant expression is a compile error. On the MSVC
 * debug runtime `std::abort` may show a modal dialog.
 * @param condition The text of the failed condition.
 * @param file The source file of the check.
 * @param line The line of the check.
 */
LUMEX_ATTRIBUTE_NORETURN inline void
check_failed (char const *condition, char const *file, int line) LUMEX_NOEXCEPT
{
#if defined(__clang__)
#pragma clang diagnostic push
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#endif
  std::fprintf (stderr, "LUMEX_SPAN_CHECKS: %s failed at %s:%d\n", condition,
                file, line);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
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
  ((condition) ? static_cast<void> (0)                                        \
               : ::lumex::core::span::view::detail::check_failed (            \
                     #condition, __FILE__, __LINE__),                         \
   __VA_ARGS__)
#else
#define LUMEX_SPAN_CHECKED(condition, ...) (__VA_ARGS__)
#endif

#endif // !LUMEX_CORE_SPAN_VIEW_SPAN_CHECKS_HPP
