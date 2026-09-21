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

#ifndef LUMEX_CORE_EXPECTED_ERROR_BAD_EXPECTED_ACCESS_HPP
#define LUMEX_CORE_EXPECTED_ERROR_BAD_EXPECTED_ACCESS_HPP

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

#include <exception>
#include <utility>

#include "lumex/core/utility/macros/LumexKeywords.hpp"

// ====================== BadExpectedAccess class (C++23 analogue)
// ======================
/**
 * @brief Exception thrown when accessing a missing value or error in
 * `Expected`.
 * @details Analogue of `std::bad_expected_access` from C++23. Thrown by
 * `value()` or `error()` when `Expected` is in the wrong state (for example,
 * calling `value()` on an object that holds an error).
 * @tparam ErrorType Error type stored in and retrievable from the exception.
 * @note Not thread-safe unless `ErrorType` itself is thread-safe.
 * @warning Constructing `BadExpectedAccess` can be expensive if `ErrorType`
 * has a heavy constructor or allocates. Thrown by `value()` and `error()`.
 */

namespace lumex
{
namespace core
{
namespace expected
{
namespace error
{
template <typename ErrorType> class BadExpectedAccess : public std::exception
{
public:
  /**
   * @brief Constructs the exception, moving `error` into the object.
   * @param[in] error Error value stored inside the exception.
   * @note Noexcept if `ErrorType`'s move constructor does not throw.
   */
  explicit BadExpectedAccess (ErrorType error) : m_error (std::move (error)) {}
  /**
   * @brief Returns a textual description of the exception.
   * @return C-string `"Bad expected access"`.
   * @note Guaranteed not to throw (`noexcept`).
   */
  char const *
  what () const LUMEX_NOEXCEPT override
  {
    return "Bad expected access";
  }

  /**
   * @brief Returns a mutable lvalue reference to the stored error.
   * @return Reference to the `ErrorType` error.
   * @note This function does not throw.
   */
  ErrorType &
  error () &
  {
    return m_error;
  }

  /**
   * @brief Returns a const lvalue reference to the stored error.
   * @return Const reference to the `ErrorType` error.
   * @note This function does not throw.
   */
  ErrorType const &
  error () const &
  {
    return m_error;
  }

  /**
   * @brief Returns an rvalue reference to the stored error.
   * @return Rvalue reference to the `ErrorType` error.
   * @note This function does not throw.
   */
  ErrorType &&
  error () &&
  {
    return std::move (m_error);
  }

  /**
   * @brief Returns a const rvalue reference to the stored error.
   * @return Const rvalue reference to the `ErrorType` error.
   * @note This function does not throw.
   */
  ErrorType const &&
  error () const &&
  {
    return std::move (m_error);
  }

private:
  /**
   * @brief Stored error value.
   * @details Holds an `ErrorType` object that describes the error.
   */
  ErrorType m_error;
};

} // namespace error
} // namespace expected
} // namespace core
} // namespace lumex

using lumex::core::expected::error::BadExpectedAccess;

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_EXPECTED_ERROR_BAD_EXPECTED_ACCESS_HPP
