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
 * @file LumexOptionalTags.hpp
 * @brief The tags and the exception of the optional module: `nullopt_t`,
 * `nullopt`, `in_place_t`, `in_place` and `lumex_bad_optional_access`.
 * @details Everything is declared in `lumex::core::optional::opt` and puts no
 * name at global scope. `opt/LumexOptional.hpp` includes this header.
 */
#ifndef LUMEX_CORE_OPTIONAL_OPT_TAGS_HPP
#define LUMEX_CORE_OPTIONAL_OPT_TAGS_HPP

#include <stdexcept> // For std::logic_error
#include <string>    // For std::string (a constructor of the exception)

#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace optional
{
namespace opt
{
/**
 * @brief A tag type used to indicate the absence of a value in
 * `optional`.
 * @details This structure serves a similar purpose to `std::nullopt_t` in
 * C++17. It is an empty type whose sole purpose is to provide a distinct type
 *          for the `nullopt` constant, enabling clear and unambiguous
 *          initialization and assignment of an empty `optional`.
 */
struct nullopt_t
{
  /**
   * @brief Internal tag for explicit construction.
   * @details This nested struct is used to enforce explicit construction of
   * `nullopt_t`, preventing accidental implicit conversions.
   */
  struct init_tag
  {
  };
  /**
   * @brief Explicit constructor for `nullopt_t`.
   */
  explicit LUMEX_CONSTEXPR
  nullopt_t (init_tag /*unused*/)
  {
  }
};
/**
 * @brief A constant of type `nullopt_t` used to represent an empty
 * `optional`.
 * @details This global constant is the canonical way to initialize or assign
 *          an empty state to a `optional` object.
 * @code
 * optional<int> opt1 = lumex::nullopt;
 * optional<std::string> opt2; // Also initializes as empty
 * opt2 = lumex::nullopt;
 * @endcode
 */
LUMEX_CONSTEXPR nullopt_t nullopt{ nullopt_t::init_tag{} };

/**
 * @brief A tag type used to indicate in-place construction of the contained
 * value.
 * @details This structure mirrors `std::in_place_t` from C++17. It is used
 *          in `optional` constructors and `emplace` methods to signify
 *          that the contained object should be constructed directly inside
 *          the optional's storage using the provided arguments, rather than
 *          copying or moving an existing object. This avoids unnecessary
 *          temporary objects and can improve performance.
 */
struct in_place_t
{
  /**
   * @brief Internal tag for explicit construction.
   * @details Similar to `nullopt_t::init_tag`, this enforces explicit
   * construction.
   */
  struct init_tag
  {
  };
  /**
   * @brief Explicit constructor for `in_place_t`.
   */
  explicit LUMEX_CONSTEXPR
  in_place_t (init_tag /*unused*/)
  {
  }
};
/**
 * @brief A constant of type `in_place_t` used for in-place construction.
 * @details This constant is used as the first argument in `optional`
 * constructors when direct construction of the contained value is desired.
 * It has no global alias: write `lumex::core::optional::opt::in_place`.
 * @code
 * using lumex::core::optional::opt::in_place;
 * // Constructs MyClass (arg1, arg2) in the optional's storage.
 * optional<MyClass> opt1 (in_place, arg1, arg2);
 * // Constructs std::vector<int> ({1, 2, 3}).
 * optional<std::vector<int>> opt2 (in_place, { 1, 2, 3 });
 * @endcode
 */
LUMEX_CONSTEXPR in_place_t in_place{ in_place_t::init_tag{} };

/**
 * @brief Exception thrown when attempting to access the value of an empty
 * `optional`.
 * @details This exception class is derived from `std::logic_error` and is
 * thrown by the `value()` method of `optional` if `has_value()` is false.
 *          It indicates a programming error where an attempt was made to
 * dereference an optional object that does not currently hold a value.
 * @note It derives from `std::logic_error`, not from
 * `std::bad_optional_access`: that class exists only from C++17 and the module
 * works from C++11, so one exception type serves every standard. Code that
 * catches `std::bad_optional_access` does not catch it; catch
 * `std::logic_error` or this class. The `std::string` constructor is the
 * reason for including
 * `<string>` here (the header does not rely on `<stdexcept>` to bring it).
 */
class lumex_bad_optional_access : public std::logic_error
{
public:
  /**
   * @brief Constructs a `lumex_bad_optional_access` exception with a default
   * message.
   */
  lumex_bad_optional_access ()
      : std::logic_error ("LumexBadOptionalAccess: Bad optional access")
  {
  }
  /**
   * @brief Constructs a `lumex_bad_optional_access` exception with a custom
   * C-style string message.
   * @param what_arg A C-style string describing the error.
   */
  explicit lumex_bad_optional_access (char const *what_arg)
      : std::logic_error (what_arg)
  {
  }
  /**
   * @brief Constructs a `lumex_bad_optional_access` exception with a custom
   * `std::string` message.
   * @param what_arg A `std::string` describing the error.
   */
  explicit lumex_bad_optional_access (std::string const &what_arg)
      : std::logic_error (what_arg)
  {
  }
};
} // namespace opt
} // namespace optional
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_OPTIONAL_OPT_TAGS_HPP
