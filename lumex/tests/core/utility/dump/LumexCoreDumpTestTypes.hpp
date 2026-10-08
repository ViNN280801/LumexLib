/*
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

#ifndef LUMEX_TESTS_CORE_UTILITY_DUMP_TEST_TYPES_HPP
#define LUMEX_TESTS_CORE_UTILITY_DUMP_TEST_TYPES_HPP

#include <string>
#include <system_error>
#include <type_traits>
#include <utility>

#include "lumex/core/utility/dump/LumexCoreDumpGenerator.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

// Argument types of the string-taking members of dump_configuration and
// core_dump_generator, and the detectors of which types those members take,
// that the tests of several standards share (LumexCoreDumpConfigStrings,
// .cxx11 to .cxx20, and LumexCoreDumpInstance). The classes are in a named
// namespace so every test source sees one definition.

namespace lumex_dump_test
{
/**
 * @brief A user type with an implicit conversion to `std::string`, the
 * stand-in for a string view of a library without `std::string_view`
 * (`lumex_string_view` is not linked into the utility tests).
 */
struct implicit_text
{
  explicit implicit_text (char const *chars) : text (chars) {}

  operator std::string () const // NOLINT(google-explicit-constructor)
  {
    return text;
  }

  std::string text;
};

/// A user type whose only conversion to `std::string` is `explicit`.
struct explicit_text
{
  explicit_text () : text ("explicit.dump") {}

  explicit
  operator std::string () const
  {
    return text;
  }

  std::string text;
};

/// A class derived from `std::string`.
struct derived_text : std::string
{
  explicit derived_text (char const *chars) : std::string (chars) {}
};

// The detectors ask the compiler whether a call is well formed instead of
// compiling a rejected call. `T` is the argument type as `std::declval<T> ()`
// gives it.
#define LUMEX_DUMP_TEST_DETECTOR(NAME, OBJECT, CALL)                          \
  template <typename T, typename = void> struct NAME : std::false_type        \
  {                                                                           \
  };                                                                          \
  template <typename T>                                                       \
  struct NAME<T, lumex::core::utility::traits::meta::void_t<                  \
                     decltype (std::declval<OBJECT> ().CALL)>>                \
      : std::true_type                                                        \
  {                                                                           \
  }

LUMEX_DUMP_TEST_DETECTOR (takes_in_set_filename,
                          lumex::core::utility::dump::dump_configuration &,
                          set_filename (std::declval<T> ()));
LUMEX_DUMP_TEST_DETECTOR (takes_in_set_directory,
                          lumex::core::utility::dump::dump_configuration &,
                          set_directory (std::declval<T> ()));
LUMEX_DUMP_TEST_DETECTOR (takes_in_add_memory_filter,
                          lumex::core::utility::dump::dump_configuration &,
                          add_memory_filter (std::declval<T> ()));
LUMEX_DUMP_TEST_DETECTOR (takes_in_instance_dump,
                          lumex::core::utility::dump::core_dump_generator &,
                          generate_instance_dump (std::declval<T> ()));
LUMEX_DUMP_TEST_DETECTOR (
    takes_in_instance_dump_with_code,
    lumex::core::utility::dump::core_dump_generator &,
    generate_instance_dump (std::declval<T> (),
                            std::declval<std::error_code &> ()));

#undef LUMEX_DUMP_TEST_DETECTOR

/// All five members take `T`.
template <typename T>
struct accepted_by_all
    : std::integral_constant<bool,
                             takes_in_set_filename<T>::value
                                 && takes_in_set_directory<T>::value
                                 && takes_in_add_memory_filter<T>::value
                                 && takes_in_instance_dump<T>::value
                                 && takes_in_instance_dump_with_code<T>::value>
{
};

/// None of the five members takes `T`.
template <typename T>
struct rejected_by_all
    : std::integral_constant<
          bool, !takes_in_set_filename<T>::value
                    && !takes_in_set_directory<T>::value
                    && !takes_in_add_memory_filter<T>::value
                    && !takes_in_instance_dump<T>::value
                    && !takes_in_instance_dump_with_code<T>::value>
{
};

/// The five members take `T` exactly when `is_string_convertible<T>` is true.
template <typename T>
struct follows_the_trait
    : std::integral_constant<
          bool,
          lumex::core::utility::traits::string::is_string_convertible<T>::value
              ? accepted_by_all<T>::value
              : rejected_by_all<T>::value>
{
};
} // namespace lumex_dump_test

#endif // LUMEX_TESTS_CORE_UTILITY_DUMP_TEST_TYPES_HPP
