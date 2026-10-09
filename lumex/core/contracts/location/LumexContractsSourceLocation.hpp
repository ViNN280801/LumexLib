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
 * @file LumexContractsSourceLocation.hpp
 * @brief `source_location`: the place in the program text that a contract
 * violation names, on every C++ standard.
 * @details The class has the accessors of `std::source_location` of C++20
 * ([source.location]): `file_name`, `function_name`, `line` and `column`. It
 * is the module's own class on every standard and toolchain, not an alias of
 * the standard one. A `std::source_location` converts into it implicitly where
 * the standard library has one (C++20 `__cpp_lib_source_location`); the other
 * direction does not exist, because a `std::source_location` can only be
 * produced by `std::source_location::current ()`. Unlike the standard class
 * it can be constructed from four values, which is what the contract macros
 * need, and compared. `current ()` takes the place of its caller where the
 * compiler provides the builtins (GCC, Clang, MSVC 19.26 and later); on
 * another compiler it returns the empty location.
 *
 * The strings are not copied: they must have static storage duration, like
 * `__FILE__` and the function name builtins.
 */
#ifndef LUMEX_CORE_CONTRACTS_LOCATION_HPP
#define LUMEX_CORE_CONTRACTS_LOCATION_HPP

#include <cstdint>

#if defined(__has_include)
#if __has_include(<version>)
#include <version>
#endif
#endif
#if defined(__cpp_lib_source_location) && __cpp_lib_source_location >= 201907L
#include <source_location>
#endif

#include "lumex/core/utility/macros/LumexKeywords.hpp"

/**
 * @def LUMEX_CONTRACTS_HAS_BUILTIN_LOCATION
 * @brief 1 when `source_location::current ()` can take the place of its
 * caller (the compiler has `__builtin_FILE`, `__builtin_FUNCTION` and
 * `__builtin_LINE`), otherwise 0.
 */
#ifndef LUMEX_CONTRACTS_HAS_BUILTIN_LOCATION
#if defined(__clang__) && (__clang_major__ >= 9)
#define LUMEX_CONTRACTS_HAS_BUILTIN_LOCATION 1
#elif defined(__GNUC__) && !defined(__clang__) && (__GNUC__ >= 5)
#define LUMEX_CONTRACTS_HAS_BUILTIN_LOCATION 1
#elif defined(_MSC_VER) && (_MSC_VER >= 1926)
#define LUMEX_CONTRACTS_HAS_BUILTIN_LOCATION 1
#else
#define LUMEX_CONTRACTS_HAS_BUILTIN_LOCATION 0
#endif
#endif

/**
 * @def LUMEX_CONTRACTS_BUILTIN_COLUMN
 * @brief The column of the place where the macro is expanded
 * (`__builtin_COLUMN ()`), or 0 where the compiler has no such builtin (GCC
 * before 15). `LUMEX_CONTRACTS_HAS_BUILTIN_COLUMN` is 1 when it has.
 */
#ifndef LUMEX_CONTRACTS_BUILTIN_COLUMN
#define LUMEX_CONTRACTS_BUILTIN_COLUMN 0
#define LUMEX_CONTRACTS_HAS_BUILTIN_COLUMN 0
#if defined(__has_builtin)
#if __has_builtin(__builtin_COLUMN)
#undef LUMEX_CONTRACTS_BUILTIN_COLUMN
#undef LUMEX_CONTRACTS_HAS_BUILTIN_COLUMN
#define LUMEX_CONTRACTS_BUILTIN_COLUMN __builtin_COLUMN ()
#define LUMEX_CONTRACTS_HAS_BUILTIN_COLUMN 1
#endif
#elif defined(_MSC_VER) && (_MSC_VER >= 1926)
#undef LUMEX_CONTRACTS_BUILTIN_COLUMN
#undef LUMEX_CONTRACTS_HAS_BUILTIN_COLUMN
#define LUMEX_CONTRACTS_BUILTIN_COLUMN __builtin_COLUMN ()
#define LUMEX_CONTRACTS_HAS_BUILTIN_COLUMN 1
#endif
#endif

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace contracts
{
namespace detail
{
/**
 * @brief Equality of two NUL-terminated strings in a constant expression
 * (C++11 form: one return statement, recursion). Neither may be null.
 */
LUMEX_CONSTEXPR_FUNCTION inline bool
equal_texts (char const *left, char const *right) LUMEX_NOEXCEPT
{
  // No shortcut for equal pointers: comparing the addresses of string
  // literals is unspecified in a constant expression.
  return *left == *right
         && (*left == '\0' || equal_texts (left + 1, right + 1));
}
} // namespace detail

/**
 * @class source_location
 * @brief File, function, line and column of a place in the program text.
 */
class source_location
{
public:
  /** @brief The empty location: empty names, line 0, column 0. */
  LUMEX_CONSTEXPR_CTOR
  source_location () LUMEX_NOEXCEPT : m_file (""),
                                      m_function (""),
                                      m_line (0),
                                      m_column (0)
  {
  }

  /**
   * @brief A location of the given values.
   * @param file_name The file name; static storage, not copied.
   * @param function_name The function name; static storage, not copied.
   * @param line The line, counted from 1 (0 means unknown).
   * @param column The column, counted from 1 (0 means unknown).
   */
  LUMEX_CONSTEXPR_CTOR
  source_location (char const *file_name, char const *function_name,
                   std::uint_least32_t line,
                   std::uint_least32_t column = 0) LUMEX_NOEXCEPT
      : m_file (file_name != nullptr ? file_name : ""),
        m_function (function_name != nullptr ? function_name : ""),
        m_line (line),
        m_column (column)
  {
  }

#if defined(__cpp_lib_source_location) && __cpp_lib_source_location >= 201907L
  /** @brief The location of a `std::source_location` (C++20). */
  LUMEX_CONSTEXPR_CTOR
  source_location (std::source_location const &where) LUMEX_NOEXCEPT
      : m_file (where.file_name ()),
        m_function (where.function_name ()),
        m_line (where.line ()),
        m_column (where.column ())
  {
  }
#endif

#if LUMEX_CONTRACTS_HAS_BUILTIN_LOCATION
  /**
   * @brief The place of the call (the default arguments are evaluated at the
   * call site, as with `std::source_location::current ()`).
   */
  static LUMEX_CONSTEXPR_FUNCTION source_location
  current (char const *file_name = __builtin_FILE (),
           char const *function_name = __builtin_FUNCTION (),
           std::uint_least32_t line = __builtin_LINE (),
           std::uint_least32_t column = static_cast<std::uint_least32_t> (
               LUMEX_CONTRACTS_BUILTIN_COLUMN)) LUMEX_NOEXCEPT
  {
    return source_location (file_name, function_name, line, column);
  }
#endif

  /** @brief The file name; never null. */
  LUMEX_CONSTEXPR_FUNCTION char const *
  file_name () const LUMEX_NOEXCEPT
  {
    return m_file;
  }

  /** @brief The function name; never null. */
  LUMEX_CONSTEXPR_FUNCTION char const *
  function_name () const LUMEX_NOEXCEPT
  {
    return m_function;
  }

  /** @brief The line; 0 when unknown. */
  LUMEX_CONSTEXPR_FUNCTION std::uint_least32_t
  line () const LUMEX_NOEXCEPT
  {
    return m_line;
  }

  /** @brief The column; 0 when unknown. */
  LUMEX_CONSTEXPR_FUNCTION std::uint_least32_t
  column () const LUMEX_NOEXCEPT
  {
    return m_column;
  }

  /** @brief Whether both names and both numbers are equal (by content). */
  friend LUMEX_CONSTEXPR_FUNCTION bool
  operator== (source_location const &left,
              source_location const &right) LUMEX_NOEXCEPT
  {
    return left.m_line == right.m_line && left.m_column == right.m_column
           && detail::equal_texts (left.m_file, right.m_file)
           && detail::equal_texts (left.m_function, right.m_function);
  }

  /** @brief The negation of `operator==`. */
  friend LUMEX_CONSTEXPR_FUNCTION bool
  operator!= (source_location const &left,
              source_location const &right) LUMEX_NOEXCEPT
  {
    return !(left == right);
  }

private:
  char const *m_file;
  char const *m_function;
  std::uint_least32_t m_line;
  std::uint_least32_t m_column;
};
} // namespace contracts
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_CONTRACTS_LOCATION_HPP
