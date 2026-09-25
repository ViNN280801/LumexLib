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

#ifndef LUMEX_CORE_EXCEPTIONS_EXCEPTION_HPP
#define LUMEX_CORE_EXCEPTIONS_EXCEPTION_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/core/exceptions/stacktrace/LumexStacktrace.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexExceptionMacros.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

// ================================================================== //
// ====================== Lumex Base Exception ====================== //
// ================================================================== //

#if __cplusplus >= 201703L
#include <string_view>
#endif

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
class LUMEX_API LumexBaseException : public std::exception
{
public:
  /**
   * @brief Constructs a `LumexBaseException` with a message.
   * @param message The error message.
   */
  LumexBaseException (char const *message);

  /**
   * @brief Constructs a `LumexBaseException` with a message.
   * @param message The error message.
   */
  LumexBaseException (std::string const &message);

  /**
   * @brief Constructs a `LumexBaseException` with a message.
   * @param message The error message.
   */
  LumexBaseException (std::string &&message);

#if __cplusplus >= 201703L
  /**
   * @brief Constructs a `LumexBaseException` with a message.
   * @param message The error message.
   */
  LumexBaseException (std::string_view message);
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
  LumexStacktrace
  getStackTrace () const LUMEX_NOEXCEPT
  {
    return m_stacktrace;
  }

  /**
   * @brief Write an error to the standard error stream by the following
   * format: [exception_name] -> custom message Uses demangled exception name
   * to avoid names like "NSt6vectorIiSaIiEEE" -> "std::vector<int,
   * std::allocator<int>>"
   * @example
   * [LumexException] -> Failed to open file
   */
  void to_stderr () const LUMEX_NOEXCEPT;

  /**
   * @brief Write a crash report to a file by pattern:
   * "crash_report_{timestamp}.txt".
   * @param stacktrace The stack trace of the error.
   */
  void to_crash_report () const;

private:
  std::string m_message;        ///< The custom error message to be displayed.
  LumexStacktrace m_stacktrace; ///< The stack trace of the error.
};
#ifdef _WIN32
#pragma warning(pop)
#endif
} // namespace exception
} // namespace exceptions
} // namespace core
} // namespace lumex

// Declare the trampoline function
LUMEX_PUBLIC_API LUMEX_ATTRIBUTE_NOINLINE LumexStacktrace
LumexException_GetStackTraceTrampoline (int skip_frames);

using LumexBaseException
    = lumex::core::exceptions::exception::LumexBaseException;

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
  catch (LumexBaseException const &ex)                                        \
  {                                                                           \
    ex.to_stderr ();                                                          \
    ex.to_crash_report ();                                                    \
  }                                                                           \
  catch (std::exception const &ex)                                            \
  {                                                                           \
    std::cerr << "[std::exception] " << ex.what () << '\n';                   \
    LumexBaseException (ex.what ()).to_stderr ();                             \
    LumexBaseException (ex.what ()).to_crash_report ();                       \
  }                                                                           \
  catch (...) { std::cerr << "[Unknown exception]\n"; }

// ================================================================== //
// ====================== >>>>>>>>>>> <<<<<<<<< ===================== //
// ================================================================== //

#endif // !LUMEX_CORE_EXCEPTIONS_EXCEPTION_HPP
