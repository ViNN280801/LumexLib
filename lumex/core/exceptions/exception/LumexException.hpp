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
 * @file LumexException.hpp
 * @brief `lumex_base_exception`, the base exception of LumexLib that records
 * the stack where it was constructed, and the macros that throw and catch it.
 * @details The exception keeps its message and a `lumex_stacktrace` captured
 * by the constructor. `to_stderr()` prints the demangled exception type and
 * the message; `to_crash_report()` appends the message and the stack to one
 * `crash_report_<timestamp>.txt` per run in the `crashes` directory next to
 * the executable. The constructors compiled into `lumex::exceptions` have
 * the same signatures in every C++ standard; the `std::string_view`
 * constructor (C++17) is an inline wrapper. The class itself is not
 * exported, so that wrapper is not `dllimport`.
 *
 * `LUMEX_THROW_EXCEPTION` throws an exception type with its demangled name in
 * front of the message. `LUMEX_EXCEPTION_HANDLE_BEGIN` and
 * `LUMEX_EXCEPTION_HANDLE_END` wrap a block in a `try` that reports a
 * `lumex_base_exception`, any other `std::exception` and an unknown exception;
 * the opening macro also installs the Windows translator through
 * `SET_SEH_TRANSLATOR` of the included `WindowsSEHTranslator.hpp`, so the
 * macros work with this header alone.
 * `LUMEX_DEFINE_EXCEPTION`, which declares new exception types, comes from the
 * included `LumexExceptionMacros.hpp`.
 */
#ifndef LUMEX_CORE_EXCEPTIONS_EXCEPTION_HPP
#define LUMEX_CORE_EXCEPTIONS_EXCEPTION_HPP

#include "lumex/LumexExport.hpp"

#include <exception>
#include <iostream>
#include <string>
#if __cplusplus >= 201703L
#include <string_view>
#endif

#include "lumex/core/exceptions/crash/WindowsSEHTranslator.hpp"
#include "lumex/core/exceptions/stacktrace/LumexStacktrace.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/demangle/LumexDemangle.hpp"
#include "lumex/core/utility/macros/LumexExceptionMacros.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

// ================================================================== //
// ====================== Lumex Base Exception ====================== //
// ================================================================== //

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace exceptions
{
namespace exception
{
// Suppress C4275 warning for std::exception base class not having DLL
// interface
#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4275 4251)
#endif
/**
 * @brief Base exception that records the stack where it was constructed.
 * @details `lumex_base_exception(char const *)`, both `std::string`
 * constructors, `to_stderr` and `to_crash_report` are exported and have the
 * same signature in every C++ standard, so a consumer built at another
 * standard than the library links. The `std::string_view` constructor
 * (C++17) is an inline wrapper over `std::string &&`. The class itself is
 * not exported: a dllimport class makes clang-cl emit an import for an
 * inline member it does not inline, and the library does not provide the
 * standard-dependent constructor.
 */
class lumex_base_exception : public std::exception
{
public:
  /**
   * @brief Constructs a `lumex_base_exception` with a message.
   * @param message The error message.
   */
  LUMEX_API lumex_base_exception (char const *message);

  /**
   * @brief Constructs a `lumex_base_exception` with a message.
   * @param message The error message.
   */
  LUMEX_API lumex_base_exception (std::string const &message);

  /**
   * @brief Constructs a `lumex_base_exception` with a message.
   * @param message The error message.
   */
  LUMEX_API lumex_base_exception (std::string &&message);

#if __cplusplus >= 201703L
  /**
   * @brief Constructs a `lumex_base_exception` with a message.
   * @details Inline and delegating to the `std::string &&` constructor, so
   * the library exports the same constructors in every C++ standard and a
   * consumer built at another standard links.
   * @param message The error message; may contain NUL characters.
   */
  lumex_base_exception (std::string_view message)
      : lumex_base_exception (std::string (message))
  {
  }
#endif

  /**
   * @brief Returns the error message as a C-string.
   * @details This method overrides the `std::exception::what` method to return
   *          the error message as a C-string.
   * @return A pointer to the error message as a C-string.
   * @note This method is `noexcept` because it only returns a pointer to a
   * member variable.
   */
  char const *
  what () const LUMEX_NOEXCEPT override
  {
    return m_message.c_str ();
  }

  /**
   * @brief Returns the stack trace of the error.
   * @details This method returns the stack trace of the error.
   * @return The stack trace of the error.
   * @note This method is `noexcept` because it only returns a member variable.
   */
  lumex_stacktrace
  get_stack_trace () const LUMEX_NOEXCEPT
  {
    return m_stacktrace;
  }

  /**
   * @brief Write an error to the standard error stream by the following
   * format: [exception_name] -> custom message Uses demangled exception name
   * to avoid names like "NSt6vectorIiSaIiEEE" -> "std::vector<int,
   * std::allocator<int>>"
   * @par Example
   * @code
   * [LumexException] -> Failed to open file
   * @endcode
   */
  LUMEX_API void to_stderr () const LUMEX_NOEXCEPT;

  /**
   * @brief Write a crash report to a file by pattern:
   * "crash_report_{timestamp}.txt".
   * @note The report includes the stored stack trace.
   */
  LUMEX_API void to_crash_report () const;

private:
  std::string m_message;         ///< The custom error message to be displayed.
  lumex_stacktrace m_stacktrace; ///< The stack trace of the error.
};
#ifdef _WIN32
#pragma warning(pop)
#endif

// Declare the trampoline function
LUMEX_PUBLIC_API LUMEX_ATTRIBUTE_NOINLINE lumex_stacktrace
lumex_exception_get_stack_trace_trampoline (int skip_frames);
} // namespace exception
} // namespace exceptions
} // namespace core
} // namespace lumex

using lumex_base_exception
    = lumex::core::exceptions::exception::lumex_base_exception;

// ================================================================== //
// ====================== Lumex Exception Macro ===================== //
// ================================================================== //

// 1 option. Define the exception class: LUMEX_DEFINE_EXCEPTION and
// LUMEX_DEFINE_EXCEPTION_WITH_BODY live in LumexExceptionMacros.hpp (included
// above) so modules that must not depend on this one can use them too.

// 2 option. Throw the exception.
// Pattern:
// [exception_name] -> custom message
// Example:
// [LumexException] -> Failed to open file
#include "lumex/core/string/LumexString"
#define LUMEX_THROW_EXCEPTION(exception_name, msg)                            \
  throw exception_name (lumex::core::string::utility::stringify (             \
      lumDemangle (exception_name), ": ", msg));

// 3. Handle the exception.
#define LUMEX_EXCEPTION_HANDLE_BEGIN                                          \
  try                                                                         \
    {                                                                         \
      SET_SEH_TRANSLATOR

#define LUMEX_EXCEPTION_HANDLE_END                                            \
  }                                                                           \
  catch (lumex_base_exception const &ex)                                      \
  {                                                                           \
    ex.to_stderr ();                                                          \
    ex.to_crash_report ();                                                    \
  }                                                                           \
  catch (std::exception const &ex)                                            \
  {                                                                           \
    std::cerr << "[std::exception] " << ex.what () << '\n';                   \
    lumex_base_exception (ex.what ()).to_stderr ();                           \
    lumex_base_exception (ex.what ()).to_crash_report ();                     \
  }                                                                           \
  catch (...) { std::cerr << "[Unknown exception]\n"; }

// ================================================================== //
// ====================== >>>>>>>>>>> <<<<<<<<< ===================== //
// ================================================================== //

#endif // !LUMEX_CORE_EXCEPTIONS_EXCEPTION_HPP
