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

#ifndef LUMEX_CORE_EXPECTED_ERROR_UNEXPECTED_HPP
#define LUMEX_CORE_EXPECTED_ERROR_UNEXPECTED_HPP

#include <utility>

// ====================== Unexpected class (C++23 analogue)
// ======================
/**
 * @brief Wrapper that holds an error value for `Expected`.
 * @details Used to construct an `Expected` in the error state. Analogue of
 * `std::unexpected` from C++23.
 * @tparam ErrorType Type of the stored error value.
 * @note An `Unexpected` object is always in the error state.
 */

namespace lumex
{
namespace core
{
namespace expected
{
namespace error
{
template <typename ErrorType> class Unexpected
{
public:
  /**
   * @brief Constructs from a const lvalue error.
   * @param[in] error Const reference to the error to store.
   * @note Noexcept if `ErrorType`'s copy constructor does not throw.
   */
  explicit Unexpected (ErrorType const &error) : m_error (error) {}

  /**
   * @brief Constructs from an rvalue error.
   * @param[in] error Rvalue reference to the error to store.
   * @note Noexcept if `ErrorType`'s move constructor does not throw.
   */
  explicit Unexpected (ErrorType &&error) : m_error (std::move (error)) {}

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
  ErrorType m_error; ///< Stored error value.
};

} // namespace error
} // namespace expected
} // namespace core
} // namespace lumex

using lumex::core::expected::error::Unexpected;

#endif // !LUMEX_CORE_EXPECTED_ERROR_UNEXPECTED_HPP
