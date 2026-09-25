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
 * @file LumexExceptionWrapper.hpp
 * @brief Exception-safe call wrapper: invokes a callable, catches anything it
 * throws, reports it to `std::cerr`, and returns a safe default value instead
 * of letting the exception propagate.
 * @details This header provides
 * `lumex::core::exceptions::Wrapper::ExceptionWrapper` plus the
 *          `LUMEX_SAFE_CALL`/`LUMEX_SAFE_CALL_MSG`/`LUMEX_SAFE_CALL_LAMBDA_MSG`
 * convenience macros built on top of it. It is a distinct, self-contained
 * concern from `LumexException.hpp` (which defines the `LumexBaseException`
 * base class and the `LUMEX_DEFINE_EXCEPTION`/`LUMEX_THROW_EXCEPTION`/
 *          `LUMEX_EXCEPTION_HANDLE_BEGIN`/`END` machinery for *throwing and
 * handling* exceptions) - this header instead *swallows* exceptions at a call
 * site, which is why it lives in its own file next to `LumexException.hpp`
 * rather than being folded into it.
 *
 *          Unlike `LumexBaseException`, `ExceptionWrapper` does not depend on
 * any logging facility: `core/` must not depend on `lumex/applied/logging`.
 * The default report sink is `std::cerr`. A consumer can install one function
 * pointer (`set_safe_call_reporter`) so the same text goes to its own logger.
 * If that function throws, the wrapper falls back to `std::cerr`.
 */
#ifndef LUMEX_CORE_EXCEPTIONS_HPP
#define LUMEX_CORE_EXCEPTIONS_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#include <cstring>     // std::strlen
#include <iostream>    // std::cerr
#include <string>      // std::string, std::to_string
#include <type_traits> // std::is_default_constructible, std::is_void
#include <utility>     // std::forward

#include "lumex/LumexExport.hpp"

#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/macros/LumexMacros.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

// ================================================================== //
// ================ Lumex Exception-Safe Call Wrapper =============== //
// ================================================================== //

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace exceptions
{
namespace Wrapper
{
/**
 * @brief Consumer report sink for a fully built failure line.
 * @param message Non-owning, null-terminated text. Valid only for the call.
 * The function may throw; `ExceptionWrapper` then writes the same line to
 * `std::cerr`.
 */
using safe_call_report_fn = void (*) (char const *message);

/**
 * @brief Installs the process-wide report sink. `nullptr` restores
 * `std::cerr`.
 * @note The pointer is stored in this library's binary, so the executable and
 * the exceptions DLL share one sink.
 */
LUMEX_API void
set_safe_call_reporter (safe_call_report_fn reporter) LUMEX_NOEXCEPT;

/** @brief Returns the installed sink, or `nullptr` when reports go to
 * `std::cerr`. */
LUMEX_API safe_call_report_fn get_safe_call_reporter () LUMEX_NOEXCEPT;

namespace detail
{
/**
 * @brief Writes "prefix. Reason: <whatMsg>\n" to `std::cerr` using only
 * `noexcept` operations.
 * @param prefix Human-readable context message, already fully built by the
 * caller (e.g. one of the `LUMEX_SAFE_CALL*` macros) - building it can throw,
 * but that happens before this function is entered, not inside it.
 * @param whatMsg The result of `std::exception::what()`; may be `nullptr`.
 * @note Deliberately avoids any operation that could itself throw (no further
 * string concatenation, no formatting/logging library) so that reporting a
 * failure can never itself introduce a new one.
 */
inline void
write_to_stderr (std::string const &prefix, char const *whatMsg) LUMEX_NOEXCEPT
{
  std::cerr.write (prefix.data (),
                   static_cast<std::streamsize> (prefix.size ()));

  static char const kReason[] = ". Reason: ";
  std::cerr.write (kReason,
                   static_cast<std::streamsize> (sizeof (kReason) - 1));

  if (whatMsg != nullptr)
    {
      std::cerr.write (whatMsg,
                       static_cast<std::streamsize> (std::strlen (whatMsg)));
    }
  else
    {
      static char const kUnknownWhat[] = "<unknown exception message>";
      std::cerr.write (kUnknownWhat, static_cast<std::streamsize> (
                                         sizeof (kUnknownWhat) - 1));
    }

  std::cerr.put ('\n');
  std::cerr.flush ();
}

/**
 * @brief Writes "line\n" to `std::cerr` using only `noexcept` operations.
 * @param line Message to write, already fully built by the caller.
 */
inline void
write_to_stderr (std::string const &line) LUMEX_NOEXCEPT
{
  std::cerr.write (line.data (), static_cast<std::streamsize> (line.size ()));
  std::cerr.put ('\n');
  std::cerr.flush ();
}

/**
 * @brief Sends `line` to the installed sink. On a null sink or a throw from
 * the sink, writes `line` to `std::cerr`.
 */
inline void
report_line (std::string const &line) LUMEX_NOEXCEPT
{
  safe_call_report_fn const reporter = get_safe_call_reporter ();
  if (reporter != nullptr)
    {
      try
        {
          reporter (line.c_str ());
          return;
        }
      catch (...)
        {
        }
    }
  write_to_stderr (line);
}

/**
 * @brief Builds "prefix. Reason: what" and reports it.
 * @param whatMsg May be `nullptr`; then the reason is
 * `<unknown exception message>`.
 */
inline void
report_exception (std::string const &prefix,
                  char const *whatMsg) LUMEX_NOEXCEPT
{
  safe_call_report_fn const reporter = get_safe_call_reporter ();
  if (reporter != nullptr)
    {
      try
        {
          std::string line = prefix;
          line += ". Reason: ";
          line += (whatMsg != nullptr) ? whatMsg
                                       : "<unknown exception message>";
          reporter (line.c_str ());
          return;
        }
      catch (...)
        {
        }
    }
  write_to_stderr (prefix, whatMsg);
}
} // namespace detail

// NOLINTBEGIN(cppcoreguidelines-avoid-do-while)
/**
 * @brief Calls `func(args...)`, catching and safely reporting any exception it
 * throws instead of letting it propagate.
 * @tparam Function Callable type; must be invocable with `Args...`.
 * @tparam Args Types of the arguments forwarded to `func`.
 * @param excMessage Message reported (to `std::cerr`) when `func` throws a
 * `std::exception`; the exception's own `what()` is appended after it.
 * @param unknownExcMessage Message reported when `func` throws anything that
 * is not a `std::exception` (e.g. a plain `int`, or a type from another ABI).
 * @param func The callable to invoke.
 * @param args Arguments forwarded to `func`.
 * @return Whatever `func(args...)` returns on success; a value-initialized
 * default
 *         (`lumex::core::utility::traits::meta::default_return<ReturnType>::value()`)
 * if it throws.
 *
 * @note `noexcept` is conditional on whether the wrapped call itself is
 * `noexcept`: reporting a failure is unconditionally `noexcept` (see
 * `detail::write_to_stderr`), so it never needs to be part of this check.
 * @note Invocation intentionally uses plain `func(args...)`, not
 *       `lumex::core::utility::traits::invoke::detail::INVOKE` - that helper
 * only has a `decltype`-computed trailing return type (no function body)
 * because it exists solely to drive `invoke_result`/ `is_callable` at compile
 * time, not to be called at runtime. `invoke_result_t`/`is_callable_v` (built
 * on top of it) remain the right tools for return-type deduction and the
 *       Callable-Named-Requirement check below.
 *
 * @example
 * @code
 * // Simple usage with default, source-location-based messages.
 * int result = LUMEX_SAFE_CALL(someFunction(42));
 *
 * // Usage with custom messages.
 * int result = LUMEX_SAFE_CALL_MSG(
 *     someFunction(42),
 *     "Failed to execute someFunction",
 *     "Unknown error in someFunction"
 * );
 * @endcode
 */
template <typename Function, typename... Args>
auto
ExceptionWrapper (
    std::string const
        &excMessage, // NOLINT(bugprone-easily-swappable-parameters)
    std::string const &unknownExcMessage, Function &&func, Args &&...args)
    LUMEX_NOEXCEPT_IF (noexcept (
        std::forward<Function> (func) (std::forward<Args> (args)...)))
        -> lumex::core::utility::traits::invoke::invoke_result_t<Function,
                                                                 Args...>
{
  using ReturnType
      = lumex::core::utility::traits::invoke::invoke_result_t<Function,
                                                              Args...>;

  LUMEX_STATIC_ASSERT_MSG (
      lumex::core::utility::traits::invoke::is_callable_v<Function, Args...>,
      "Function must be callable with the supplied arguments. Check:\n"
      "1) Function type is correct\n"
      "2) Number of arguments matches\n"
      "3) Argument types are compatible");

  LUMEX_STATIC_ASSERT_MSG (
      std::is_default_constructible<ReturnType>::value
          || std::is_void<ReturnType>::value,
      "ReturnType must be default-constructible or void. "
      "For non-default-constructible types, consider using std::optional.");

  try
    {
      return std::forward<Function> (func) (std::forward<Args> (args)...);
    }
  catch (std::exception const &exc)
    {
      detail::report_exception (excMessage, exc.what ());
    }
  catch (...)
    {
      detail::report_line (unknownExcMessage);
    }

  // static_assert above guarantees ReturnType is default-constructible or
  // void, so this cannot throw.
  return lumex::core::utility::traits::meta::default_return<
      ReturnType>::value ();
}
// NOLINTEND(cppcoreguidelines-avoid-do-while)
} // namespace Wrapper
} // namespace exceptions
} // namespace core
} // namespace lumex

// ================================================================== //
// ===================== Lumex Safe-Call Macros ====================== //
// ================================================================== //

/**
 * @brief Safely evaluates `expr`, with default, source-location-based
 * diagnostic messages.
 * @param expr Expression to evaluate (any callable expression or function
 * call).
 * @note Captures the enclosing scope by reference (`[&]`) so `expr` can use
 * local variables.
 * @warning Do not use with an expression that has side effects intended to run
 * more than once - `expr` is only evaluated once here, but is written twice
 * into the macro expansion's lambda body only in the `decltype`/return
 * position, not re-evaluated.
 */
// NOLINTBEGIN(cppcoreguidelines-macro-usage) - macros are the only way to
// capture __FILE__/__LINE__ and build a `[&]`-capturing lambda around an
// arbitrary caller expression.
#define LUMEX_SAFE_CALL(expr)                                                 \
  lumex::core::exceptions::Wrapper::ExceptionWrapper (                        \
      std::string ("Exception in ") + std::string (LUMEX_FUNCTION_NAME)       \
          + " at " + std::string (__FILE__) + ":"                             \
          + std::to_string (__LINE__),                                        \
      std::string ("Unknown exception in ")                                   \
          + std::string (LUMEX_FUNCTION_NAME) + " at "                        \
          + std::string (__FILE__) + ":" + std::to_string (__LINE__),         \
      [&] () -> decltype (expr) { return expr; })

/**
 * @brief Safely evaluates `expr`, with caller-supplied diagnostic messages.
 * @param expr Expression to evaluate (any callable expression or function
 * call).
 * @param excMessage Message reported for a `std::exception`.
 * @param unknExcMessage Message reported for any other exception.
 */
#define LUMEX_SAFE_CALL_MSG(expr, excMessage, unknExcMessage)                 \
  lumex::core::exceptions::Wrapper::ExceptionWrapper (                        \
      excMessage, unknExcMessage, [&] () -> decltype (expr) { return expr; })

/**
 * @brief Safely invokes `lambda`, with caller-supplied diagnostic messages.
 * @param lambda Callable to invoke directly (useful when the body is more than
 * one statement, so it cannot be written as a single `expr`).
 * @param excMessage Message reported for a `std::exception`.
 * @param unknExcMessage Message reported for any other exception.
 *
 * @example
 * @code
 * LUMEX_SAFE_CALL_LAMBDA_MSG(
 *     [&]() {
 *         config = loadConfig("configure.txt");
 *         if(!tryApply(config)) config = defaultConfig();
 *     },
 *     "Failed to load configure.txt",
 *     "Unknown error while loading configure.txt");
 * @endcode
 */
#define LUMEX_SAFE_CALL_LAMBDA_MSG(lambda, excMessage, unknExcMessage)        \
  lumex::core::exceptions::Wrapper::ExceptionWrapper (excMessage,             \
                                                      unknExcMessage, lambda)
// NOLINTEND(cppcoreguidelines-macro-usage)

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_EXCEPTIONS_HPP
