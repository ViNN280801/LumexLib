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

#ifndef LUMEX_CORE_UTILITY_COMPILER_FEATURES_HPP
#define LUMEX_CORE_UTILITY_COMPILER_FEATURES_HPP

/**
 * @file LumexCheckFeatures.hpp
 * @brief C++ language, library and attribute feature availability.
 * @details One pair of macros per standard feature-test macro, all of them
 *          always defined, so they can be tested with `#if` under
 *          `-Wundef`:
 *          - `LUMEX_FEATURE_<NAME>` is the value the toolchain reports, or
 *            `0`. Compare it to test a later revision, for example
 *            `LUMEX_FEATURE_CONSTEXPR >= 201907L`.
 *          - `LUMEX_HAS_<NAME>` is true when the first standardized value
 *            is available.
 *
 *          Names: `__cpp_<name>` gives `LUMEX_*_<NAME>`,
 *          `__cpp_lib_<name>` gives `LUMEX_*_STD_<NAME>`, and
 *          `__has_cpp_attribute (<name>)` gives `LUMEX_*_ATTRIBUTE_<NAME>`.
 *
 *          Test these instead of `__cplusplus`, which names the requested
 *          standard only: GCC 8 to 10 report `201709L` for `-std=c++2a`
 *          with most of C++20 missing, and Clang with an older libstdc++
 *          reports `202002L` without `<span>` or `<ranges>`.
 *
 *          Library values come from `<version>`. A library without it
 *          (libstdc++ before 9) defines them in the matching headers only;
 *          there a C++17 or older library feature falls back to the
 *          standard mode, except the ones libstdc++ 8 lacks, and a C++20
 *          or later one reads `0`.
 *
 *          An attribute counts as available only in the standard that
 *          introduced it: GCC and Clang report attributes of later
 *          standards in earlier modes too, as extensions that `-Wpedantic`
 *          reports.
 *
 * @see The list and the values follow
 * https://en.cppreference.com/w/cpp/feature_test (2026-09-30). Compiler
 * versions per feature: https://en.cppreference.com/w/cpp/compiler_support
 */

#if defined(__has_include)
#if __has_include(<version>)
#include <version>
#define LUMEX_HAS_VERSION_HEADER 1
#endif
#endif
#if !defined(LUMEX_HAS_VERSION_HEADER)
#define LUMEX_HAS_VERSION_HEADER 0
#endif

// --- Attributes ---

/// `[[assume]]` (C++23).
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(assume) >= 202207L
#define LUMEX_FEATURE_ATTRIBUTE_ASSUME 202207L
#endif
#endif
#if !defined(LUMEX_FEATURE_ATTRIBUTE_ASSUME)
#define LUMEX_FEATURE_ATTRIBUTE_ASSUME 0
#endif
#define LUMEX_HAS_ATTRIBUTE_ASSUME                                            \
  (LUMEX_FEATURE_ATTRIBUTE_ASSUME >= 202207L && __cplusplus >= 202302L)

/// `[[carries_dependency]]` (C++11).
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(carries_dependency) >= 200809L
#define LUMEX_FEATURE_ATTRIBUTE_CARRIES_DEPENDENCY 200809L
#endif
#endif
#if !defined(LUMEX_FEATURE_ATTRIBUTE_CARRIES_DEPENDENCY)
#define LUMEX_FEATURE_ATTRIBUTE_CARRIES_DEPENDENCY 0
#endif
#define LUMEX_HAS_ATTRIBUTE_CARRIES_DEPENDENCY                                \
  (LUMEX_FEATURE_ATTRIBUTE_CARRIES_DEPENDENCY >= 200809L                      \
   && __cplusplus >= 201103L)

/// `[[deprecated]]` (C++14).
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(deprecated) >= 201309L
#define LUMEX_FEATURE_ATTRIBUTE_DEPRECATED 201309L
#endif
#endif
#if !defined(LUMEX_FEATURE_ATTRIBUTE_DEPRECATED)
#define LUMEX_FEATURE_ATTRIBUTE_DEPRECATED 0
#endif
#define LUMEX_HAS_ATTRIBUTE_DEPRECATED                                        \
  (LUMEX_FEATURE_ATTRIBUTE_DEPRECATED >= 201309L && __cplusplus >= 201402L)

/// `[[fallthrough]]` (C++17).
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(fallthrough) >= 201603L
#define LUMEX_FEATURE_ATTRIBUTE_FALLTHROUGH 201603L
#endif
#endif
#if !defined(LUMEX_FEATURE_ATTRIBUTE_FALLTHROUGH)
#define LUMEX_FEATURE_ATTRIBUTE_FALLTHROUGH 0
#endif
#define LUMEX_HAS_ATTRIBUTE_FALLTHROUGH                                       \
  (LUMEX_FEATURE_ATTRIBUTE_FALLTHROUGH >= 201603L && __cplusplus >= 201703L)

/// `[[indeterminate]]` (C++26).
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(indeterminate) >= 202403L
#define LUMEX_FEATURE_ATTRIBUTE_INDETERMINATE 202403L
#endif
#endif
#if !defined(LUMEX_FEATURE_ATTRIBUTE_INDETERMINATE)
#define LUMEX_FEATURE_ATTRIBUTE_INDETERMINATE 0
#endif
#define LUMEX_HAS_ATTRIBUTE_INDETERMINATE                                     \
  (LUMEX_FEATURE_ATTRIBUTE_INDETERMINATE >= 202403L && __cplusplus > 202302L)

/// `[[likely]]` (C++20).
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(likely) >= 201803L
#define LUMEX_FEATURE_ATTRIBUTE_LIKELY 201803L
#endif
#endif
#if !defined(LUMEX_FEATURE_ATTRIBUTE_LIKELY)
#define LUMEX_FEATURE_ATTRIBUTE_LIKELY 0
#endif
#define LUMEX_HAS_ATTRIBUTE_LIKELY                                            \
  (LUMEX_FEATURE_ATTRIBUTE_LIKELY >= 201803L && __cplusplus >= 202002L)

/// `[[maybe_unused]]` (C++17).
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(maybe_unused) >= 201603L
#define LUMEX_FEATURE_ATTRIBUTE_MAYBE_UNUSED 201603L
#endif
#endif
#if !defined(LUMEX_FEATURE_ATTRIBUTE_MAYBE_UNUSED)
#define LUMEX_FEATURE_ATTRIBUTE_MAYBE_UNUSED 0
#endif
#define LUMEX_HAS_ATTRIBUTE_MAYBE_UNUSED                                      \
  (LUMEX_FEATURE_ATTRIBUTE_MAYBE_UNUSED >= 201603L && __cplusplus >= 201703L)

/// `[[no_unique_address]]` (C++20).
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(no_unique_address) >= 201803L
#define LUMEX_FEATURE_ATTRIBUTE_NO_UNIQUE_ADDRESS 201803L
#endif
#endif
#if !defined(LUMEX_FEATURE_ATTRIBUTE_NO_UNIQUE_ADDRESS)
#define LUMEX_FEATURE_ATTRIBUTE_NO_UNIQUE_ADDRESS 0
#endif
#define LUMEX_HAS_ATTRIBUTE_NO_UNIQUE_ADDRESS                                 \
  (LUMEX_FEATURE_ATTRIBUTE_NO_UNIQUE_ADDRESS >= 201803L                       \
   && __cplusplus >= 202002L)

/// `[[nodiscard]]` (C++17; later: 201907L C++20).
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(nodiscard) >= 201907L
#define LUMEX_FEATURE_ATTRIBUTE_NODISCARD 201907L
#elif __has_cpp_attribute(nodiscard) >= 201603L
#define LUMEX_FEATURE_ATTRIBUTE_NODISCARD 201603L
#endif
#endif
#if !defined(LUMEX_FEATURE_ATTRIBUTE_NODISCARD)
#define LUMEX_FEATURE_ATTRIBUTE_NODISCARD 0
#endif
#define LUMEX_HAS_ATTRIBUTE_NODISCARD                                         \
  (LUMEX_FEATURE_ATTRIBUTE_NODISCARD >= 201603L && __cplusplus >= 201703L)

/// `[[noreturn]]` (C++11).
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(noreturn) >= 200809L
#define LUMEX_FEATURE_ATTRIBUTE_NORETURN 200809L
#endif
#endif
#if !defined(LUMEX_FEATURE_ATTRIBUTE_NORETURN)
#define LUMEX_FEATURE_ATTRIBUTE_NORETURN 0
#endif
#define LUMEX_HAS_ATTRIBUTE_NORETURN                                          \
  (LUMEX_FEATURE_ATTRIBUTE_NORETURN >= 200809L && __cplusplus >= 201103L)

/// `[[unlikely]]` (C++20).
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(unlikely) >= 201803L
#define LUMEX_FEATURE_ATTRIBUTE_UNLIKELY 201803L
#endif
#endif
#if !defined(LUMEX_FEATURE_ATTRIBUTE_UNLIKELY)
#define LUMEX_FEATURE_ATTRIBUTE_UNLIKELY 0
#endif
#define LUMEX_HAS_ATTRIBUTE_UNLIKELY                                          \
  (LUMEX_FEATURE_ATTRIBUTE_UNLIKELY >= 201803L && __cplusplus >= 202002L)

// --- Language features ---

/// `__cpp_aggregate_bases` (C++17).
#if defined(__cpp_aggregate_bases)
#define LUMEX_FEATURE_AGGREGATE_BASES __cpp_aggregate_bases
#else
#define LUMEX_FEATURE_AGGREGATE_BASES 0
#endif
#define LUMEX_HAS_AGGREGATE_BASES (LUMEX_FEATURE_AGGREGATE_BASES >= 201603L)

/// `__cpp_aggregate_nsdmi` (C++14).
#if defined(__cpp_aggregate_nsdmi)
#define LUMEX_FEATURE_AGGREGATE_NSDMI __cpp_aggregate_nsdmi
#else
#define LUMEX_FEATURE_AGGREGATE_NSDMI 0
#endif
#define LUMEX_HAS_AGGREGATE_NSDMI (LUMEX_FEATURE_AGGREGATE_NSDMI >= 201304L)

/// `__cpp_aggregate_paren_init` (C++20).
#if defined(__cpp_aggregate_paren_init)
#define LUMEX_FEATURE_AGGREGATE_PAREN_INIT __cpp_aggregate_paren_init
#else
#define LUMEX_FEATURE_AGGREGATE_PAREN_INIT 0
#endif
#define LUMEX_HAS_AGGREGATE_PAREN_INIT                                        \
  (LUMEX_FEATURE_AGGREGATE_PAREN_INIT >= 201902L)

/// `__cpp_alias_templates` (C++11).
#if defined(__cpp_alias_templates)
#define LUMEX_FEATURE_ALIAS_TEMPLATES __cpp_alias_templates
#else
#define LUMEX_FEATURE_ALIAS_TEMPLATES 0
#endif
#define LUMEX_HAS_ALIAS_TEMPLATES (LUMEX_FEATURE_ALIAS_TEMPLATES >= 200704L)

/// `__cpp_aligned_new` (C++17).
#if defined(__cpp_aligned_new)
#define LUMEX_FEATURE_ALIGNED_NEW __cpp_aligned_new
#else
#define LUMEX_FEATURE_ALIGNED_NEW 0
#endif
#define LUMEX_HAS_ALIGNED_NEW (LUMEX_FEATURE_ALIGNED_NEW >= 201606L)

/// `__cpp_attributes` (C++11).
#if defined(__cpp_attributes)
#define LUMEX_FEATURE_ATTRIBUTES __cpp_attributes
#else
#define LUMEX_FEATURE_ATTRIBUTES 0
#endif
#define LUMEX_HAS_ATTRIBUTES (LUMEX_FEATURE_ATTRIBUTES >= 200809L)

/// `__cpp_auto_cast` (C++23).
#if defined(__cpp_auto_cast)
#define LUMEX_FEATURE_AUTO_CAST __cpp_auto_cast
#else
#define LUMEX_FEATURE_AUTO_CAST 0
#endif
#define LUMEX_HAS_AUTO_CAST (LUMEX_FEATURE_AUTO_CAST >= 202110L)

/// `__cpp_binary_literals` (C++14).
#if defined(__cpp_binary_literals)
#define LUMEX_FEATURE_BINARY_LITERALS __cpp_binary_literals
#else
#define LUMEX_FEATURE_BINARY_LITERALS 0
#endif
#define LUMEX_HAS_BINARY_LITERALS (LUMEX_FEATURE_BINARY_LITERALS >= 201304L)

/// `__cpp_capture_star_this` (C++17).
#if defined(__cpp_capture_star_this)
#define LUMEX_FEATURE_CAPTURE_STAR_THIS __cpp_capture_star_this
#else
#define LUMEX_FEATURE_CAPTURE_STAR_THIS 0
#endif
#define LUMEX_HAS_CAPTURE_STAR_THIS                                           \
  (LUMEX_FEATURE_CAPTURE_STAR_THIS >= 201603L)

/// `__cpp_char8_t` (C++20; later: 202207L C++23).
#if defined(__cpp_char8_t)
#define LUMEX_FEATURE_CHAR8_T __cpp_char8_t
#else
#define LUMEX_FEATURE_CHAR8_T 0
#endif
#define LUMEX_HAS_CHAR8_T (LUMEX_FEATURE_CHAR8_T >= 201811L)

/// `__cpp_concepts` (C++20; later: 202002L C++20, 202606L C++29).
#if defined(__cpp_concepts)
#define LUMEX_FEATURE_CONCEPTS __cpp_concepts
#else
#define LUMEX_FEATURE_CONCEPTS 0
#endif
#define LUMEX_HAS_CONCEPTS (LUMEX_FEATURE_CONCEPTS >= 201907L)

/// `__cpp_conditional_explicit` (C++20).
#if defined(__cpp_conditional_explicit)
#define LUMEX_FEATURE_CONDITIONAL_EXPLICIT __cpp_conditional_explicit
#else
#define LUMEX_FEATURE_CONDITIONAL_EXPLICIT 0
#endif
#define LUMEX_HAS_CONDITIONAL_EXPLICIT                                        \
  (LUMEX_FEATURE_CONDITIONAL_EXPLICIT >= 201806L)

/// `__cpp_consteval` (C++20; later: 202211L C++23, 202606L C++29).
#if defined(__cpp_consteval)
#define LUMEX_FEATURE_CONSTEVAL __cpp_consteval
#else
#define LUMEX_FEATURE_CONSTEVAL 0
#endif
#define LUMEX_HAS_CONSTEVAL (LUMEX_FEATURE_CONSTEVAL >= 201811L)

/// `__cpp_constexpr` (C++11; later: 201304L C++14, 201603L C++17, 201907L
/// C++20, 202002L C++20, 202110L C++23, 202207L C++23, 202211L C++23, 202306L
/// C++26, 202406L C++26).
#if defined(__cpp_constexpr)
#define LUMEX_FEATURE_CONSTEXPR __cpp_constexpr
#else
#define LUMEX_FEATURE_CONSTEXPR 0
#endif
#define LUMEX_HAS_CONSTEXPR (LUMEX_FEATURE_CONSTEXPR >= 200704L)

/// `__cpp_constexpr_dynamic_alloc` (C++20).
#if defined(__cpp_constexpr_dynamic_alloc)
#define LUMEX_FEATURE_CONSTEXPR_DYNAMIC_ALLOC __cpp_constexpr_dynamic_alloc
#else
#define LUMEX_FEATURE_CONSTEXPR_DYNAMIC_ALLOC 0
#endif
#define LUMEX_HAS_CONSTEXPR_DYNAMIC_ALLOC                                     \
  (LUMEX_FEATURE_CONSTEXPR_DYNAMIC_ALLOC >= 201907L)

/// `__cpp_constexpr_exceptions` (C++26).
#if defined(__cpp_constexpr_exceptions)
#define LUMEX_FEATURE_CONSTEXPR_EXCEPTIONS __cpp_constexpr_exceptions
#else
#define LUMEX_FEATURE_CONSTEXPR_EXCEPTIONS 0
#endif
#define LUMEX_HAS_CONSTEXPR_EXCEPTIONS                                        \
  (LUMEX_FEATURE_CONSTEXPR_EXCEPTIONS >= 202411L)

/// `__cpp_constexpr_in_decltype` (C++20).
#if defined(__cpp_constexpr_in_decltype)
#define LUMEX_FEATURE_CONSTEXPR_IN_DECLTYPE __cpp_constexpr_in_decltype
#else
#define LUMEX_FEATURE_CONSTEXPR_IN_DECLTYPE 0
#endif
#define LUMEX_HAS_CONSTEXPR_IN_DECLTYPE                                       \
  (LUMEX_FEATURE_CONSTEXPR_IN_DECLTYPE >= 201711L)

/// `__cpp_constexpr_virtual_inheritance` (C++26).
#if defined(__cpp_constexpr_virtual_inheritance)
#define LUMEX_FEATURE_CONSTEXPR_VIRTUAL_INHERITANCE                           \
  __cpp_constexpr_virtual_inheritance
#else
#define LUMEX_FEATURE_CONSTEXPR_VIRTUAL_INHERITANCE 0
#endif
#define LUMEX_HAS_CONSTEXPR_VIRTUAL_INHERITANCE                               \
  (LUMEX_FEATURE_CONSTEXPR_VIRTUAL_INHERITANCE >= 202506L)

/// `__cpp_constinit` (C++20).
#if defined(__cpp_constinit)
#define LUMEX_FEATURE_CONSTINIT __cpp_constinit
#else
#define LUMEX_FEATURE_CONSTINIT 0
#endif
#define LUMEX_HAS_CONSTINIT (LUMEX_FEATURE_CONSTINIT >= 201907L)

/// `__cpp_contracts` (C++26; later: 202606L C++29).
#if defined(__cpp_contracts)
#define LUMEX_FEATURE_CONTRACTS __cpp_contracts
#else
#define LUMEX_FEATURE_CONTRACTS 0
#endif
#define LUMEX_HAS_CONTRACTS (LUMEX_FEATURE_CONTRACTS >= 202502L)

/// `__cpp_decltype` (C++11).
#if defined(__cpp_decltype)
#define LUMEX_FEATURE_DECLTYPE __cpp_decltype
#else
#define LUMEX_FEATURE_DECLTYPE 0
#endif
#define LUMEX_HAS_DECLTYPE (LUMEX_FEATURE_DECLTYPE >= 200707L)

/// `__cpp_decltype_auto` (C++14).
#if defined(__cpp_decltype_auto)
#define LUMEX_FEATURE_DECLTYPE_AUTO __cpp_decltype_auto
#else
#define LUMEX_FEATURE_DECLTYPE_AUTO 0
#endif
#define LUMEX_HAS_DECLTYPE_AUTO (LUMEX_FEATURE_DECLTYPE_AUTO >= 201304L)

/// `__cpp_deduction_guides` (C++17; later: 201907L C++20).
#if defined(__cpp_deduction_guides)
#define LUMEX_FEATURE_DEDUCTION_GUIDES __cpp_deduction_guides
#else
#define LUMEX_FEATURE_DEDUCTION_GUIDES 0
#endif
#define LUMEX_HAS_DEDUCTION_GUIDES (LUMEX_FEATURE_DEDUCTION_GUIDES >= 201703L)

/// `__cpp_delegating_constructors` (C++11).
#if defined(__cpp_delegating_constructors)
#define LUMEX_FEATURE_DELEGATING_CONSTRUCTORS __cpp_delegating_constructors
#else
#define LUMEX_FEATURE_DELEGATING_CONSTRUCTORS 0
#endif
#define LUMEX_HAS_DELEGATING_CONSTRUCTORS                                     \
  (LUMEX_FEATURE_DELEGATING_CONSTRUCTORS >= 200604L)

/// `__cpp_deleted_function` (C++26).
#if defined(__cpp_deleted_function)
#define LUMEX_FEATURE_DELETED_FUNCTION __cpp_deleted_function
#else
#define LUMEX_FEATURE_DELETED_FUNCTION 0
#endif
#define LUMEX_HAS_DELETED_FUNCTION (LUMEX_FEATURE_DELETED_FUNCTION >= 202403L)

/// `__cpp_designated_initializers` (C++20; later: 202606L C++29).
#if defined(__cpp_designated_initializers)
#define LUMEX_FEATURE_DESIGNATED_INITIALIZERS __cpp_designated_initializers
#else
#define LUMEX_FEATURE_DESIGNATED_INITIALIZERS 0
#endif
#define LUMEX_HAS_DESIGNATED_INITIALIZERS                                     \
  (LUMEX_FEATURE_DESIGNATED_INITIALIZERS >= 201707L)

/// `__cpp_enumerator_attributes` (C++17).
#if defined(__cpp_enumerator_attributes)
#define LUMEX_FEATURE_ENUMERATOR_ATTRIBUTES __cpp_enumerator_attributes
#else
#define LUMEX_FEATURE_ENUMERATOR_ATTRIBUTES 0
#endif
#define LUMEX_HAS_ENUMERATOR_ATTRIBUTES                                       \
  (LUMEX_FEATURE_ENUMERATOR_ATTRIBUTES >= 201411L)

/// `__cpp_expansion_statements` (C++26).
#if defined(__cpp_expansion_statements)
#define LUMEX_FEATURE_EXPANSION_STATEMENTS __cpp_expansion_statements
#else
#define LUMEX_FEATURE_EXPANSION_STATEMENTS 0
#endif
#define LUMEX_HAS_EXPANSION_STATEMENTS                                        \
  (LUMEX_FEATURE_EXPANSION_STATEMENTS >= 202506L)

/// `__cpp_explicit_this_parameter` (C++23).
#if defined(__cpp_explicit_this_parameter)
#define LUMEX_FEATURE_EXPLICIT_THIS_PARAMETER __cpp_explicit_this_parameter
#else
#define LUMEX_FEATURE_EXPLICIT_THIS_PARAMETER 0
#endif
#define LUMEX_HAS_EXPLICIT_THIS_PARAMETER                                     \
  (LUMEX_FEATURE_EXPLICIT_THIS_PARAMETER >= 202110L)

/// `__cpp_fold_expressions` (C++17; later: 202406L C++26).
#if defined(__cpp_fold_expressions)
#define LUMEX_FEATURE_FOLD_EXPRESSIONS __cpp_fold_expressions
#else
#define LUMEX_FEATURE_FOLD_EXPRESSIONS 0
#endif
#define LUMEX_HAS_FOLD_EXPRESSIONS (LUMEX_FEATURE_FOLD_EXPRESSIONS >= 201603L)

/// `__cpp_generic_lambdas` (C++14; later: 201707L C++20).
#if defined(__cpp_generic_lambdas)
#define LUMEX_FEATURE_GENERIC_LAMBDAS __cpp_generic_lambdas
#else
#define LUMEX_FEATURE_GENERIC_LAMBDAS 0
#endif
#define LUMEX_HAS_GENERIC_LAMBDAS (LUMEX_FEATURE_GENERIC_LAMBDAS >= 201304L)

/// `__cpp_guaranteed_copy_elision` (C++17).
#if defined(__cpp_guaranteed_copy_elision)
#define LUMEX_FEATURE_GUARANTEED_COPY_ELISION __cpp_guaranteed_copy_elision
#else
#define LUMEX_FEATURE_GUARANTEED_COPY_ELISION 0
#endif
#define LUMEX_HAS_GUARANTEED_COPY_ELISION                                     \
  (LUMEX_FEATURE_GUARANTEED_COPY_ELISION >= 201606L)

/// `__cpp_hex_float` (C++17).
#if defined(__cpp_hex_float)
#define LUMEX_FEATURE_HEX_FLOAT __cpp_hex_float
#else
#define LUMEX_FEATURE_HEX_FLOAT 0
#endif
#define LUMEX_HAS_HEX_FLOAT (LUMEX_FEATURE_HEX_FLOAT >= 201603L)

/// `__cpp_if_consteval` (C++23).
#if defined(__cpp_if_consteval)
#define LUMEX_FEATURE_IF_CONSTEVAL __cpp_if_consteval
#else
#define LUMEX_FEATURE_IF_CONSTEVAL 0
#endif
#define LUMEX_HAS_IF_CONSTEVAL (LUMEX_FEATURE_IF_CONSTEVAL >= 202106L)

/// `__cpp_if_constexpr` (C++17).
#if defined(__cpp_if_constexpr)
#define LUMEX_FEATURE_IF_CONSTEXPR __cpp_if_constexpr
#else
#define LUMEX_FEATURE_IF_CONSTEXPR 0
#endif
#define LUMEX_HAS_IF_CONSTEXPR (LUMEX_FEATURE_IF_CONSTEXPR >= 201606L)

/// `__cpp_impl_coroutine` (C++20; later: 202606L C++29).
#if defined(__cpp_impl_coroutine)
#define LUMEX_FEATURE_IMPL_COROUTINE __cpp_impl_coroutine
#else
#define LUMEX_FEATURE_IMPL_COROUTINE 0
#endif
#define LUMEX_HAS_IMPL_COROUTINE (LUMEX_FEATURE_IMPL_COROUTINE >= 201902L)

/// `__cpp_impl_destroying_delete` (C++20).
#if defined(__cpp_impl_destroying_delete)
#define LUMEX_FEATURE_IMPL_DESTROYING_DELETE __cpp_impl_destroying_delete
#else
#define LUMEX_FEATURE_IMPL_DESTROYING_DELETE 0
#endif
#define LUMEX_HAS_IMPL_DESTROYING_DELETE                                      \
  (LUMEX_FEATURE_IMPL_DESTROYING_DELETE >= 201806L)

/// `__cpp_impl_reflection` (C++26).
#if defined(__cpp_impl_reflection)
#define LUMEX_FEATURE_IMPL_REFLECTION __cpp_impl_reflection
#else
#define LUMEX_FEATURE_IMPL_REFLECTION 0
#endif
#define LUMEX_HAS_IMPL_REFLECTION (LUMEX_FEATURE_IMPL_REFLECTION >= 202506L)

/// `__cpp_impl_three_way_comparison` (C++20).
#if defined(__cpp_impl_three_way_comparison)
#define LUMEX_FEATURE_IMPL_THREE_WAY_COMPARISON __cpp_impl_three_way_comparison
#else
#define LUMEX_FEATURE_IMPL_THREE_WAY_COMPARISON 0
#endif
#define LUMEX_HAS_IMPL_THREE_WAY_COMPARISON                                   \
  (LUMEX_FEATURE_IMPL_THREE_WAY_COMPARISON >= 201907L)

/// `__cpp_implicit_move` (C++23).
#if defined(__cpp_implicit_move)
#define LUMEX_FEATURE_IMPLICIT_MOVE __cpp_implicit_move
#else
#define LUMEX_FEATURE_IMPLICIT_MOVE 0
#endif
#define LUMEX_HAS_IMPLICIT_MOVE (LUMEX_FEATURE_IMPLICIT_MOVE >= 202207L)

/// `__cpp_inheriting_constructors` (C++11; later: 201511L C++17).
#if defined(__cpp_inheriting_constructors)
#define LUMEX_FEATURE_INHERITING_CONSTRUCTORS __cpp_inheriting_constructors
#else
#define LUMEX_FEATURE_INHERITING_CONSTRUCTORS 0
#endif
#define LUMEX_HAS_INHERITING_CONSTRUCTORS                                     \
  (LUMEX_FEATURE_INHERITING_CONSTRUCTORS >= 200802L)

/// `__cpp_init_captures` (C++14; later: 201803L C++20).
#if defined(__cpp_init_captures)
#define LUMEX_FEATURE_INIT_CAPTURES __cpp_init_captures
#else
#define LUMEX_FEATURE_INIT_CAPTURES 0
#endif
#define LUMEX_HAS_INIT_CAPTURES (LUMEX_FEATURE_INIT_CAPTURES >= 201304L)

/// `__cpp_initializer_lists` (C++11).
#if defined(__cpp_initializer_lists)
#define LUMEX_FEATURE_INITIALIZER_LISTS __cpp_initializer_lists
#else
#define LUMEX_FEATURE_INITIALIZER_LISTS 0
#endif
#define LUMEX_HAS_INITIALIZER_LISTS                                           \
  (LUMEX_FEATURE_INITIALIZER_LISTS >= 200806L)

/// `__cpp_inline_variables` (C++17).
#if defined(__cpp_inline_variables)
#define LUMEX_FEATURE_INLINE_VARIABLES __cpp_inline_variables
#else
#define LUMEX_FEATURE_INLINE_VARIABLES 0
#endif
#define LUMEX_HAS_INLINE_VARIABLES (LUMEX_FEATURE_INLINE_VARIABLES >= 201606L)

/// `__cpp_lambdas` (C++11).
#if defined(__cpp_lambdas)
#define LUMEX_FEATURE_LAMBDAS __cpp_lambdas
#else
#define LUMEX_FEATURE_LAMBDAS 0
#endif
#define LUMEX_HAS_LAMBDAS (LUMEX_FEATURE_LAMBDAS >= 200907L)

/// `__cpp_modules` (C++20).
#if defined(__cpp_modules)
#define LUMEX_FEATURE_MODULES __cpp_modules
#else
#define LUMEX_FEATURE_MODULES 0
#endif
#define LUMEX_HAS_MODULES (LUMEX_FEATURE_MODULES >= 201907L)

/// `__cpp_multidimensional_subscript` (C++23; later: 202211L C++23).
#if defined(__cpp_multidimensional_subscript)
#define LUMEX_FEATURE_MULTIDIMENSIONAL_SUBSCRIPT                              \
  __cpp_multidimensional_subscript
#else
#define LUMEX_FEATURE_MULTIDIMENSIONAL_SUBSCRIPT 0
#endif
#define LUMEX_HAS_MULTIDIMENSIONAL_SUBSCRIPT                                  \
  (LUMEX_FEATURE_MULTIDIMENSIONAL_SUBSCRIPT >= 202110L)

/// `__cpp_named_character_escapes` (C++23).
#if defined(__cpp_named_character_escapes)
#define LUMEX_FEATURE_NAMED_CHARACTER_ESCAPES __cpp_named_character_escapes
#else
#define LUMEX_FEATURE_NAMED_CHARACTER_ESCAPES 0
#endif
#define LUMEX_HAS_NAMED_CHARACTER_ESCAPES                                     \
  (LUMEX_FEATURE_NAMED_CHARACTER_ESCAPES >= 202207L)

/// `__cpp_namespace_attributes` (C++17).
#if defined(__cpp_namespace_attributes)
#define LUMEX_FEATURE_NAMESPACE_ATTRIBUTES __cpp_namespace_attributes
#else
#define LUMEX_FEATURE_NAMESPACE_ATTRIBUTES 0
#endif
#define LUMEX_HAS_NAMESPACE_ATTRIBUTES                                        \
  (LUMEX_FEATURE_NAMESPACE_ATTRIBUTES >= 201411L)

/// `__cpp_noexcept_function_type` (C++17).
#if defined(__cpp_noexcept_function_type)
#define LUMEX_FEATURE_NOEXCEPT_FUNCTION_TYPE __cpp_noexcept_function_type
#else
#define LUMEX_FEATURE_NOEXCEPT_FUNCTION_TYPE 0
#endif
#define LUMEX_HAS_NOEXCEPT_FUNCTION_TYPE                                      \
  (LUMEX_FEATURE_NOEXCEPT_FUNCTION_TYPE >= 201510L)

/// `__cpp_nontype_template_args` (C++17; later: 201911L C++20).
#if defined(__cpp_nontype_template_args)
#define LUMEX_FEATURE_NONTYPE_TEMPLATE_ARGS __cpp_nontype_template_args
#else
#define LUMEX_FEATURE_NONTYPE_TEMPLATE_ARGS 0
#endif
#define LUMEX_HAS_NONTYPE_TEMPLATE_ARGS                                       \
  (LUMEX_FEATURE_NONTYPE_TEMPLATE_ARGS >= 201411L)

/// `__cpp_nontype_template_parameter_auto` (C++17).
#if defined(__cpp_nontype_template_parameter_auto)
#define LUMEX_FEATURE_NONTYPE_TEMPLATE_PARAMETER_AUTO                         \
  __cpp_nontype_template_parameter_auto
#else
#define LUMEX_FEATURE_NONTYPE_TEMPLATE_PARAMETER_AUTO 0
#endif
#define LUMEX_HAS_NONTYPE_TEMPLATE_PARAMETER_AUTO                             \
  (LUMEX_FEATURE_NONTYPE_TEMPLATE_PARAMETER_AUTO >= 201606L)

/// `__cpp_nsdmi` (C++11).
#if defined(__cpp_nsdmi)
#define LUMEX_FEATURE_NSDMI __cpp_nsdmi
#else
#define LUMEX_FEATURE_NSDMI 0
#endif
#define LUMEX_HAS_NSDMI (LUMEX_FEATURE_NSDMI >= 200809L)

/// `__cpp_pack_indexing` (C++26; later: 202606L C++29).
#if defined(__cpp_pack_indexing)
#define LUMEX_FEATURE_PACK_INDEXING __cpp_pack_indexing
#else
#define LUMEX_FEATURE_PACK_INDEXING 0
#endif
#define LUMEX_HAS_PACK_INDEXING (LUMEX_FEATURE_PACK_INDEXING >= 202311L)

/// `__cpp_placeholder_variables` (C++26).
#if defined(__cpp_placeholder_variables)
#define LUMEX_FEATURE_PLACEHOLDER_VARIABLES __cpp_placeholder_variables
#else
#define LUMEX_FEATURE_PLACEHOLDER_VARIABLES 0
#endif
#define LUMEX_HAS_PLACEHOLDER_VARIABLES                                       \
  (LUMEX_FEATURE_PLACEHOLDER_VARIABLES >= 202306L)

/// `__cpp_pp_embed` (C++26; later: 202606L C++29).
#if defined(__cpp_pp_embed)
#define LUMEX_FEATURE_PP_EMBED __cpp_pp_embed
#else
#define LUMEX_FEATURE_PP_EMBED 0
#endif
#define LUMEX_HAS_PP_EMBED (LUMEX_FEATURE_PP_EMBED >= 202502L)

/// `__cpp_range_based_for` (C++11; later: 201603L C++17, 202211L C++23).
#if defined(__cpp_range_based_for)
#define LUMEX_FEATURE_RANGE_BASED_FOR __cpp_range_based_for
#else
#define LUMEX_FEATURE_RANGE_BASED_FOR 0
#endif
#define LUMEX_HAS_RANGE_BASED_FOR (LUMEX_FEATURE_RANGE_BASED_FOR >= 200907L)

/// `__cpp_raw_strings` (C++11).
#if defined(__cpp_raw_strings)
#define LUMEX_FEATURE_RAW_STRINGS __cpp_raw_strings
#else
#define LUMEX_FEATURE_RAW_STRINGS 0
#endif
#define LUMEX_HAS_RAW_STRINGS (LUMEX_FEATURE_RAW_STRINGS >= 200710L)

/// `__cpp_ref_qualifiers` (C++11).
#if defined(__cpp_ref_qualifiers)
#define LUMEX_FEATURE_REF_QUALIFIERS __cpp_ref_qualifiers
#else
#define LUMEX_FEATURE_REF_QUALIFIERS 0
#endif
#define LUMEX_HAS_REF_QUALIFIERS (LUMEX_FEATURE_REF_QUALIFIERS >= 200710L)

/// `__cpp_return_type_deduction` (C++14).
#if defined(__cpp_return_type_deduction)
#define LUMEX_FEATURE_RETURN_TYPE_DEDUCTION __cpp_return_type_deduction
#else
#define LUMEX_FEATURE_RETURN_TYPE_DEDUCTION 0
#endif
#define LUMEX_HAS_RETURN_TYPE_DEDUCTION                                       \
  (LUMEX_FEATURE_RETURN_TYPE_DEDUCTION >= 201304L)

/// `__cpp_rvalue_references` (C++11).
#if defined(__cpp_rvalue_references)
#define LUMEX_FEATURE_RVALUE_REFERENCES __cpp_rvalue_references
#else
#define LUMEX_FEATURE_RVALUE_REFERENCES 0
#endif
#define LUMEX_HAS_RVALUE_REFERENCES                                           \
  (LUMEX_FEATURE_RVALUE_REFERENCES >= 200610L)

/// `__cpp_size_t_suffix` (C++23).
#if defined(__cpp_size_t_suffix)
#define LUMEX_FEATURE_SIZE_T_SUFFIX __cpp_size_t_suffix
#else
#define LUMEX_FEATURE_SIZE_T_SUFFIX 0
#endif
#define LUMEX_HAS_SIZE_T_SUFFIX (LUMEX_FEATURE_SIZE_T_SUFFIX >= 202011L)

/// `__cpp_sized_deallocation` (C++14).
#if defined(__cpp_sized_deallocation)
#define LUMEX_FEATURE_SIZED_DEALLOCATION __cpp_sized_deallocation
#else
#define LUMEX_FEATURE_SIZED_DEALLOCATION 0
#endif
#define LUMEX_HAS_SIZED_DEALLOCATION                                          \
  (LUMEX_FEATURE_SIZED_DEALLOCATION >= 201309L)

/// `__cpp_static_assert` (C++11; later: 201411L C++17, 202306L C++26).
#if defined(__cpp_static_assert)
#define LUMEX_FEATURE_STATIC_ASSERT __cpp_static_assert
#else
#define LUMEX_FEATURE_STATIC_ASSERT 0
#endif
#define LUMEX_HAS_STATIC_ASSERT (LUMEX_FEATURE_STATIC_ASSERT >= 200410L)

/// `__cpp_static_call_operator` (C++23).
#if defined(__cpp_static_call_operator)
#define LUMEX_FEATURE_STATIC_CALL_OPERATOR __cpp_static_call_operator
#else
#define LUMEX_FEATURE_STATIC_CALL_OPERATOR 0
#endif
#define LUMEX_HAS_STATIC_CALL_OPERATOR                                        \
  (LUMEX_FEATURE_STATIC_CALL_OPERATOR >= 202207L)

/// `__cpp_structured_bindings` (C++17; later: 202403L C++26, 202406L C++26,
/// 202411L C++26).
#if defined(__cpp_structured_bindings)
#define LUMEX_FEATURE_STRUCTURED_BINDINGS __cpp_structured_bindings
#else
#define LUMEX_FEATURE_STRUCTURED_BINDINGS 0
#endif
#define LUMEX_HAS_STRUCTURED_BINDINGS                                         \
  (LUMEX_FEATURE_STRUCTURED_BINDINGS >= 201606L)

/// `__cpp_template_parameters` (C++26).
#if defined(__cpp_template_parameters)
#define LUMEX_FEATURE_TEMPLATE_PARAMETERS __cpp_template_parameters
#else
#define LUMEX_FEATURE_TEMPLATE_PARAMETERS 0
#endif
#define LUMEX_HAS_TEMPLATE_PARAMETERS                                         \
  (LUMEX_FEATURE_TEMPLATE_PARAMETERS >= 202502L)

/// `__cpp_template_template_args` (C++17).
#if defined(__cpp_template_template_args)
#define LUMEX_FEATURE_TEMPLATE_TEMPLATE_ARGS __cpp_template_template_args
#else
#define LUMEX_FEATURE_TEMPLATE_TEMPLATE_ARGS 0
#endif
#define LUMEX_HAS_TEMPLATE_TEMPLATE_ARGS                                      \
  (LUMEX_FEATURE_TEMPLATE_TEMPLATE_ARGS >= 201611L)

/// `__cpp_threadsafe_static_init` (C++11).
#if defined(__cpp_threadsafe_static_init)
#define LUMEX_FEATURE_THREADSAFE_STATIC_INIT __cpp_threadsafe_static_init
#else
#define LUMEX_FEATURE_THREADSAFE_STATIC_INIT 0
#endif
#define LUMEX_HAS_THREADSAFE_STATIC_INIT                                      \
  (LUMEX_FEATURE_THREADSAFE_STATIC_INIT >= 200806L)

/// `__cpp_trivial_union` (C++26).
#if defined(__cpp_trivial_union)
#define LUMEX_FEATURE_TRIVIAL_UNION __cpp_trivial_union
#else
#define LUMEX_FEATURE_TRIVIAL_UNION 0
#endif
#define LUMEX_HAS_TRIVIAL_UNION (LUMEX_FEATURE_TRIVIAL_UNION >= 202603L)

/// `__cpp_unicode_characters` (C++11).
#if defined(__cpp_unicode_characters)
#define LUMEX_FEATURE_UNICODE_CHARACTERS __cpp_unicode_characters
#else
#define LUMEX_FEATURE_UNICODE_CHARACTERS 0
#endif
#define LUMEX_HAS_UNICODE_CHARACTERS                                          \
  (LUMEX_FEATURE_UNICODE_CHARACTERS >= 200704L)

/// `__cpp_unicode_literals` (C++11).
#if defined(__cpp_unicode_literals)
#define LUMEX_FEATURE_UNICODE_LITERALS __cpp_unicode_literals
#else
#define LUMEX_FEATURE_UNICODE_LITERALS 0
#endif
#define LUMEX_HAS_UNICODE_LITERALS (LUMEX_FEATURE_UNICODE_LITERALS >= 200710L)

/// `__cpp_user_defined_literals` (C++11).
#if defined(__cpp_user_defined_literals)
#define LUMEX_FEATURE_USER_DEFINED_LITERALS __cpp_user_defined_literals
#else
#define LUMEX_FEATURE_USER_DEFINED_LITERALS 0
#endif
#define LUMEX_HAS_USER_DEFINED_LITERALS                                       \
  (LUMEX_FEATURE_USER_DEFINED_LITERALS >= 200809L)

/// `__cpp_using_enum` (C++20).
#if defined(__cpp_using_enum)
#define LUMEX_FEATURE_USING_ENUM __cpp_using_enum
#else
#define LUMEX_FEATURE_USING_ENUM 0
#endif
#define LUMEX_HAS_USING_ENUM (LUMEX_FEATURE_USING_ENUM >= 201907L)

/// `__cpp_variable_templates` (C++14).
#if defined(__cpp_variable_templates)
#define LUMEX_FEATURE_VARIABLE_TEMPLATES __cpp_variable_templates
#else
#define LUMEX_FEATURE_VARIABLE_TEMPLATES 0
#endif
#define LUMEX_HAS_VARIABLE_TEMPLATES                                          \
  (LUMEX_FEATURE_VARIABLE_TEMPLATES >= 201304L)

/// `__cpp_variadic_friend` (C++26).
#if defined(__cpp_variadic_friend)
#define LUMEX_FEATURE_VARIADIC_FRIEND __cpp_variadic_friend
#else
#define LUMEX_FEATURE_VARIADIC_FRIEND 0
#endif
#define LUMEX_HAS_VARIADIC_FRIEND (LUMEX_FEATURE_VARIADIC_FRIEND >= 202403L)

/// `__cpp_variadic_templates` (C++11).
#if defined(__cpp_variadic_templates)
#define LUMEX_FEATURE_VARIADIC_TEMPLATES __cpp_variadic_templates
#else
#define LUMEX_FEATURE_VARIADIC_TEMPLATES 0
#endif
#define LUMEX_HAS_VARIADIC_TEMPLATES                                          \
  (LUMEX_FEATURE_VARIADIC_TEMPLATES >= 200704L)

/// `__cpp_variadic_using` (C++17).
#if defined(__cpp_variadic_using)
#define LUMEX_FEATURE_VARIADIC_USING __cpp_variadic_using
#else
#define LUMEX_FEATURE_VARIADIC_USING 0
#endif
#define LUMEX_HAS_VARIADIC_USING (LUMEX_FEATURE_VARIADIC_USING >= 201611L)

// --- Library features ---

/// `__cpp_lib_adaptor_iterator_pair_constructor` (C++23).
#if defined(__cpp_lib_adaptor_iterator_pair_constructor)
#define LUMEX_FEATURE_STD_ADAPTOR_ITERATOR_PAIR_CONSTRUCTOR                   \
  __cpp_lib_adaptor_iterator_pair_constructor
#else
#define LUMEX_FEATURE_STD_ADAPTOR_ITERATOR_PAIR_CONSTRUCTOR 0
#endif
#define LUMEX_HAS_STD_ADAPTOR_ITERATOR_PAIR_CONSTRUCTOR                       \
  (LUMEX_FEATURE_STD_ADAPTOR_ITERATOR_PAIR_CONSTRUCTOR >= 202106L)

/// `__cpp_lib_addressof_constexpr` (C++17).
#if defined(__cpp_lib_addressof_constexpr)
#define LUMEX_FEATURE_STD_ADDRESSOF_CONSTEXPR __cpp_lib_addressof_constexpr
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_ADDRESSOF_CONSTEXPR 201603L
#else
#define LUMEX_FEATURE_STD_ADDRESSOF_CONSTEXPR 0
#endif
#define LUMEX_HAS_STD_ADDRESSOF_CONSTEXPR                                     \
  (LUMEX_FEATURE_STD_ADDRESSOF_CONSTEXPR >= 201603L)

/// `__cpp_lib_algorithm_default_value_type_202403L` (C++26).
#if defined(__cpp_lib_algorithm_default_value_type_202403L)
#define LUMEX_FEATURE_STD_ALGORITHM_DEFAULT_VALUE_TYPE_202403L                \
  __cpp_lib_algorithm_default_value_type_202403L
#else
#define LUMEX_FEATURE_STD_ALGORITHM_DEFAULT_VALUE_TYPE_202403L 0
#endif
#define LUMEX_HAS_STD_ALGORITHM_DEFAULT_VALUE_TYPE_202403L                    \
  (LUMEX_FEATURE_STD_ALGORITHM_DEFAULT_VALUE_TYPE_202403L >= 202603L)

/// `__cpp_lib_algorithm_iterator_requirements` (C++23).
#if defined(__cpp_lib_algorithm_iterator_requirements)
#define LUMEX_FEATURE_STD_ALGORITHM_ITERATOR_REQUIREMENTS                     \
  __cpp_lib_algorithm_iterator_requirements
#else
#define LUMEX_FEATURE_STD_ALGORITHM_ITERATOR_REQUIREMENTS 0
#endif
#define LUMEX_HAS_STD_ALGORITHM_ITERATOR_REQUIREMENTS                         \
  (LUMEX_FEATURE_STD_ALGORITHM_ITERATOR_REQUIREMENTS >= 202207L)

/// `__cpp_lib_aligned_accessor` (C++26).
#if defined(__cpp_lib_aligned_accessor)
#define LUMEX_FEATURE_STD_ALIGNED_ACCESSOR __cpp_lib_aligned_accessor
#else
#define LUMEX_FEATURE_STD_ALIGNED_ACCESSOR 0
#endif
#define LUMEX_HAS_STD_ALIGNED_ACCESSOR                                        \
  (LUMEX_FEATURE_STD_ALIGNED_ACCESSOR >= 202411L)

/// `__cpp_lib_allocate_at_least` (C++23).
#if defined(__cpp_lib_allocate_at_least)
#define LUMEX_FEATURE_STD_ALLOCATE_AT_LEAST __cpp_lib_allocate_at_least
#else
#define LUMEX_FEATURE_STD_ALLOCATE_AT_LEAST 0
#endif
#define LUMEX_HAS_STD_ALLOCATE_AT_LEAST                                       \
  (LUMEX_FEATURE_STD_ALLOCATE_AT_LEAST >= 202302L)

/// `__cpp_lib_allocator_traits_is_always_equal` (C++17).
#if defined(__cpp_lib_allocator_traits_is_always_equal)
#define LUMEX_FEATURE_STD_ALLOCATOR_TRAITS_IS_ALWAYS_EQUAL                    \
  __cpp_lib_allocator_traits_is_always_equal
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_ALLOCATOR_TRAITS_IS_ALWAYS_EQUAL 201411L
#else
#define LUMEX_FEATURE_STD_ALLOCATOR_TRAITS_IS_ALWAYS_EQUAL 0
#endif
#define LUMEX_HAS_STD_ALLOCATOR_TRAITS_IS_ALWAYS_EQUAL                        \
  (LUMEX_FEATURE_STD_ALLOCATOR_TRAITS_IS_ALWAYS_EQUAL >= 201411L)

/// `__cpp_lib_any` (C++17).
#if defined(__cpp_lib_any)
#define LUMEX_FEATURE_STD_ANY __cpp_lib_any
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_ANY 201606L
#else
#define LUMEX_FEATURE_STD_ANY 0
#endif
#define LUMEX_HAS_STD_ANY (LUMEX_FEATURE_STD_ANY >= 201606L)

/// `__cpp_lib_apply` (C++17; later: 202506L C++26).
#if defined(__cpp_lib_apply)
#define LUMEX_FEATURE_STD_APPLY __cpp_lib_apply
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_APPLY 201603L
#else
#define LUMEX_FEATURE_STD_APPLY 0
#endif
#define LUMEX_HAS_STD_APPLY (LUMEX_FEATURE_STD_APPLY >= 201603L)

/// `__cpp_lib_array_constexpr` (C++17; later: 201811L C++20).
#if defined(__cpp_lib_array_constexpr)
#define LUMEX_FEATURE_STD_ARRAY_CONSTEXPR __cpp_lib_array_constexpr
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_ARRAY_CONSTEXPR 201603L
#else
#define LUMEX_FEATURE_STD_ARRAY_CONSTEXPR 0
#endif
#define LUMEX_HAS_STD_ARRAY_CONSTEXPR                                         \
  (LUMEX_FEATURE_STD_ARRAY_CONSTEXPR >= 201603L)

/// `__cpp_lib_as_const` (C++17).
#if defined(__cpp_lib_as_const)
#define LUMEX_FEATURE_STD_AS_CONST __cpp_lib_as_const
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_AS_CONST 201510L
#else
#define LUMEX_FEATURE_STD_AS_CONST 0
#endif
#define LUMEX_HAS_STD_AS_CONST (LUMEX_FEATURE_STD_AS_CONST >= 201510L)

/// `__cpp_lib_associative_heterogeneous_erasure` (C++23).
#if defined(__cpp_lib_associative_heterogeneous_erasure)
#define LUMEX_FEATURE_STD_ASSOCIATIVE_HETEROGENEOUS_ERASURE                   \
  __cpp_lib_associative_heterogeneous_erasure
#else
#define LUMEX_FEATURE_STD_ASSOCIATIVE_HETEROGENEOUS_ERASURE 0
#endif
#define LUMEX_HAS_STD_ASSOCIATIVE_HETEROGENEOUS_ERASURE                       \
  (LUMEX_FEATURE_STD_ASSOCIATIVE_HETEROGENEOUS_ERASURE >= 202110L)

/// `__cpp_lib_associative_heterogeneous_insertion` (C++26).
#if defined(__cpp_lib_associative_heterogeneous_insertion)
#define LUMEX_FEATURE_STD_ASSOCIATIVE_HETEROGENEOUS_INSERTION                 \
  __cpp_lib_associative_heterogeneous_insertion
#else
#define LUMEX_FEATURE_STD_ASSOCIATIVE_HETEROGENEOUS_INSERTION 0
#endif
#define LUMEX_HAS_STD_ASSOCIATIVE_HETEROGENEOUS_INSERTION                     \
  (LUMEX_FEATURE_STD_ASSOCIATIVE_HETEROGENEOUS_INSERTION >= 202306L)

/// `__cpp_lib_assume_aligned` (C++20).
#if defined(__cpp_lib_assume_aligned)
#define LUMEX_FEATURE_STD_ASSUME_ALIGNED __cpp_lib_assume_aligned
#else
#define LUMEX_FEATURE_STD_ASSUME_ALIGNED 0
#endif
#define LUMEX_HAS_STD_ASSUME_ALIGNED                                          \
  (LUMEX_FEATURE_STD_ASSUME_ALIGNED >= 201811L)

/// `__cpp_lib_atomic_flag_test` (C++20).
#if defined(__cpp_lib_atomic_flag_test)
#define LUMEX_FEATURE_STD_ATOMIC_FLAG_TEST __cpp_lib_atomic_flag_test
#else
#define LUMEX_FEATURE_STD_ATOMIC_FLAG_TEST 0
#endif
#define LUMEX_HAS_STD_ATOMIC_FLAG_TEST                                        \
  (LUMEX_FEATURE_STD_ATOMIC_FLAG_TEST >= 201907L)

/// `__cpp_lib_atomic_float` (C++20).
#if defined(__cpp_lib_atomic_float)
#define LUMEX_FEATURE_STD_ATOMIC_FLOAT __cpp_lib_atomic_float
#else
#define LUMEX_FEATURE_STD_ATOMIC_FLOAT 0
#endif
#define LUMEX_HAS_STD_ATOMIC_FLOAT (LUMEX_FEATURE_STD_ATOMIC_FLOAT >= 201711L)

/// `__cpp_lib_atomic_is_always_lock_free` (C++17).
#if defined(__cpp_lib_atomic_is_always_lock_free)
#define LUMEX_FEATURE_STD_ATOMIC_IS_ALWAYS_LOCK_FREE                          \
  __cpp_lib_atomic_is_always_lock_free
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_ATOMIC_IS_ALWAYS_LOCK_FREE 201603L
#else
#define LUMEX_FEATURE_STD_ATOMIC_IS_ALWAYS_LOCK_FREE 0
#endif
#define LUMEX_HAS_STD_ATOMIC_IS_ALWAYS_LOCK_FREE                              \
  (LUMEX_FEATURE_STD_ATOMIC_IS_ALWAYS_LOCK_FREE >= 201603L)

/// `__cpp_lib_atomic_lock_free_type_aliases` (C++20).
#if defined(__cpp_lib_atomic_lock_free_type_aliases)
#define LUMEX_FEATURE_STD_ATOMIC_LOCK_FREE_TYPE_ALIASES                       \
  __cpp_lib_atomic_lock_free_type_aliases
#else
#define LUMEX_FEATURE_STD_ATOMIC_LOCK_FREE_TYPE_ALIASES 0
#endif
#define LUMEX_HAS_STD_ATOMIC_LOCK_FREE_TYPE_ALIASES                           \
  (LUMEX_FEATURE_STD_ATOMIC_LOCK_FREE_TYPE_ALIASES >= 201907L)

/// `__cpp_lib_atomic_min_max` (C++26; later: 202506L C++26).
#if defined(__cpp_lib_atomic_min_max)
#define LUMEX_FEATURE_STD_ATOMIC_MIN_MAX __cpp_lib_atomic_min_max
#else
#define LUMEX_FEATURE_STD_ATOMIC_MIN_MAX 0
#endif
#define LUMEX_HAS_STD_ATOMIC_MIN_MAX                                          \
  (LUMEX_FEATURE_STD_ATOMIC_MIN_MAX >= 202403L)

/// `__cpp_lib_atomic_reductions` (C++26).
#if defined(__cpp_lib_atomic_reductions)
#define LUMEX_FEATURE_STD_ATOMIC_REDUCTIONS __cpp_lib_atomic_reductions
#else
#define LUMEX_FEATURE_STD_ATOMIC_REDUCTIONS 0
#endif
#define LUMEX_HAS_STD_ATOMIC_REDUCTIONS                                       \
  (LUMEX_FEATURE_STD_ATOMIC_REDUCTIONS >= 202506L)

/// `__cpp_lib_atomic_ref` (C++20; later: 202603L C++26).
#if defined(__cpp_lib_atomic_ref)
#define LUMEX_FEATURE_STD_ATOMIC_REF __cpp_lib_atomic_ref
#else
#define LUMEX_FEATURE_STD_ATOMIC_REF 0
#endif
#define LUMEX_HAS_STD_ATOMIC_REF (LUMEX_FEATURE_STD_ATOMIC_REF >= 201806L)

/// `__cpp_lib_atomic_shared_ptr` (C++20).
#if defined(__cpp_lib_atomic_shared_ptr)
#define LUMEX_FEATURE_STD_ATOMIC_SHARED_PTR __cpp_lib_atomic_shared_ptr
#else
#define LUMEX_FEATURE_STD_ATOMIC_SHARED_PTR 0
#endif
#define LUMEX_HAS_STD_ATOMIC_SHARED_PTR                                       \
  (LUMEX_FEATURE_STD_ATOMIC_SHARED_PTR >= 201711L)

/// `__cpp_lib_atomic_value_initialization` (C++20).
#if defined(__cpp_lib_atomic_value_initialization)
#define LUMEX_FEATURE_STD_ATOMIC_VALUE_INITIALIZATION                         \
  __cpp_lib_atomic_value_initialization
#else
#define LUMEX_FEATURE_STD_ATOMIC_VALUE_INITIALIZATION 0
#endif
#define LUMEX_HAS_STD_ATOMIC_VALUE_INITIALIZATION                             \
  (LUMEX_FEATURE_STD_ATOMIC_VALUE_INITIALIZATION >= 201911L)

/// `__cpp_lib_atomic_wait` (C++20).
#if defined(__cpp_lib_atomic_wait)
#define LUMEX_FEATURE_STD_ATOMIC_WAIT __cpp_lib_atomic_wait
#else
#define LUMEX_FEATURE_STD_ATOMIC_WAIT 0
#endif
#define LUMEX_HAS_STD_ATOMIC_WAIT (LUMEX_FEATURE_STD_ATOMIC_WAIT >= 201907L)

/// `__cpp_lib_barrier` (C++20; later: 202302L C++23).
#if defined(__cpp_lib_barrier)
#define LUMEX_FEATURE_STD_BARRIER __cpp_lib_barrier
#else
#define LUMEX_FEATURE_STD_BARRIER 0
#endif
#define LUMEX_HAS_STD_BARRIER (LUMEX_FEATURE_STD_BARRIER >= 201907L)

/// `__cpp_lib_bind_back` (C++23; later: 202306L C++26).
#if defined(__cpp_lib_bind_back)
#define LUMEX_FEATURE_STD_BIND_BACK __cpp_lib_bind_back
#else
#define LUMEX_FEATURE_STD_BIND_BACK 0
#endif
#define LUMEX_HAS_STD_BIND_BACK (LUMEX_FEATURE_STD_BIND_BACK >= 202202L)

/// `__cpp_lib_bind_front` (C++20; later: 202306L C++26).
#if defined(__cpp_lib_bind_front)
#define LUMEX_FEATURE_STD_BIND_FRONT __cpp_lib_bind_front
#else
#define LUMEX_FEATURE_STD_BIND_FRONT 0
#endif
#define LUMEX_HAS_STD_BIND_FRONT (LUMEX_FEATURE_STD_BIND_FRONT >= 201907L)

/// `__cpp_lib_bit_cast` (C++20).
#if defined(__cpp_lib_bit_cast)
#define LUMEX_FEATURE_STD_BIT_CAST __cpp_lib_bit_cast
#else
#define LUMEX_FEATURE_STD_BIT_CAST 0
#endif
#define LUMEX_HAS_STD_BIT_CAST (LUMEX_FEATURE_STD_BIT_CAST >= 201806L)

/// `__cpp_lib_bitops` (C++20; later: 202607L C++29).
#if defined(__cpp_lib_bitops)
#define LUMEX_FEATURE_STD_BITOPS __cpp_lib_bitops
#else
#define LUMEX_FEATURE_STD_BITOPS 0
#endif
#define LUMEX_HAS_STD_BITOPS (LUMEX_FEATURE_STD_BITOPS >= 201907L)

/// `__cpp_lib_bitset` (C++26).
#if defined(__cpp_lib_bitset)
#define LUMEX_FEATURE_STD_BITSET __cpp_lib_bitset
#else
#define LUMEX_FEATURE_STD_BITSET 0
#endif
#define LUMEX_HAS_STD_BITSET (LUMEX_FEATURE_STD_BITSET >= 202306L)

/// `__cpp_lib_bool_constant` (C++17).
#if defined(__cpp_lib_bool_constant)
#define LUMEX_FEATURE_STD_BOOL_CONSTANT __cpp_lib_bool_constant
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_BOOL_CONSTANT 201505L
#else
#define LUMEX_FEATURE_STD_BOOL_CONSTANT 0
#endif
#define LUMEX_HAS_STD_BOOL_CONSTANT                                           \
  (LUMEX_FEATURE_STD_BOOL_CONSTANT >= 201505L)

/// `__cpp_lib_bounded_array_traits` (C++20).
#if defined(__cpp_lib_bounded_array_traits)
#define LUMEX_FEATURE_STD_BOUNDED_ARRAY_TRAITS __cpp_lib_bounded_array_traits
#else
#define LUMEX_FEATURE_STD_BOUNDED_ARRAY_TRAITS 0
#endif
#define LUMEX_HAS_STD_BOUNDED_ARRAY_TRAITS                                    \
  (LUMEX_FEATURE_STD_BOUNDED_ARRAY_TRAITS >= 201902L)

/// `__cpp_lib_boyer_moore_searcher` (C++17).
#if defined(__cpp_lib_boyer_moore_searcher)
#define LUMEX_FEATURE_STD_BOYER_MOORE_SEARCHER __cpp_lib_boyer_moore_searcher
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_BOYER_MOORE_SEARCHER 201603L
#else
#define LUMEX_FEATURE_STD_BOYER_MOORE_SEARCHER 0
#endif
#define LUMEX_HAS_STD_BOYER_MOORE_SEARCHER                                    \
  (LUMEX_FEATURE_STD_BOYER_MOORE_SEARCHER >= 201603L)

/// `__cpp_lib_byte` (C++17).
#if defined(__cpp_lib_byte)
#define LUMEX_FEATURE_STD_BYTE __cpp_lib_byte
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_BYTE 201603L
#else
#define LUMEX_FEATURE_STD_BYTE 0
#endif
#define LUMEX_HAS_STD_BYTE (LUMEX_FEATURE_STD_BYTE >= 201603L)

/// `__cpp_lib_byteswap` (C++23).
#if defined(__cpp_lib_byteswap)
#define LUMEX_FEATURE_STD_BYTESWAP __cpp_lib_byteswap
#else
#define LUMEX_FEATURE_STD_BYTESWAP 0
#endif
#define LUMEX_HAS_STD_BYTESWAP (LUMEX_FEATURE_STD_BYTESWAP >= 202110L)

/// `__cpp_lib_char8_t` (C++20).
#if defined(__cpp_lib_char8_t)
#define LUMEX_FEATURE_STD_CHAR8_T __cpp_lib_char8_t
#else
#define LUMEX_FEATURE_STD_CHAR8_T 0
#endif
#define LUMEX_HAS_STD_CHAR8_T (LUMEX_FEATURE_STD_CHAR8_T >= 201907L)

/// `__cpp_lib_chrono` (C++17; later: 201611L C++17, 201907L C++20, 202306L
/// C++26).
#if defined(__cpp_lib_chrono)
#define LUMEX_FEATURE_STD_CHRONO __cpp_lib_chrono
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_CHRONO 201510L
#else
#define LUMEX_FEATURE_STD_CHRONO 0
#endif
#define LUMEX_HAS_STD_CHRONO (LUMEX_FEATURE_STD_CHRONO >= 201510L)

/// `__cpp_lib_chrono_udls` (C++14).
#if defined(__cpp_lib_chrono_udls)
#define LUMEX_FEATURE_STD_CHRONO_UDLS __cpp_lib_chrono_udls
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201402L
#define LUMEX_FEATURE_STD_CHRONO_UDLS 201304L
#else
#define LUMEX_FEATURE_STD_CHRONO_UDLS 0
#endif
#define LUMEX_HAS_STD_CHRONO_UDLS (LUMEX_FEATURE_STD_CHRONO_UDLS >= 201304L)

/// `__cpp_lib_clamp` (C++17).
#if defined(__cpp_lib_clamp)
#define LUMEX_FEATURE_STD_CLAMP __cpp_lib_clamp
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_CLAMP 201603L
#else
#define LUMEX_FEATURE_STD_CLAMP 0
#endif
#define LUMEX_HAS_STD_CLAMP (LUMEX_FEATURE_STD_CLAMP >= 201603L)

/// `__cpp_lib_common_reference` (C++23).
#if defined(__cpp_lib_common_reference)
#define LUMEX_FEATURE_STD_COMMON_REFERENCE __cpp_lib_common_reference
#else
#define LUMEX_FEATURE_STD_COMMON_REFERENCE 0
#endif
#define LUMEX_HAS_STD_COMMON_REFERENCE                                        \
  (LUMEX_FEATURE_STD_COMMON_REFERENCE >= 202302L)

/// `__cpp_lib_common_reference_wrapper` (C++23).
#if defined(__cpp_lib_common_reference_wrapper)
#define LUMEX_FEATURE_STD_COMMON_REFERENCE_WRAPPER                            \
  __cpp_lib_common_reference_wrapper
#else
#define LUMEX_FEATURE_STD_COMMON_REFERENCE_WRAPPER 0
#endif
#define LUMEX_HAS_STD_COMMON_REFERENCE_WRAPPER                                \
  (LUMEX_FEATURE_STD_COMMON_REFERENCE_WRAPPER >= 202302L)

/// `__cpp_lib_complex_udls` (C++14).
#if defined(__cpp_lib_complex_udls)
#define LUMEX_FEATURE_STD_COMPLEX_UDLS __cpp_lib_complex_udls
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201402L
#define LUMEX_FEATURE_STD_COMPLEX_UDLS 201309L
#else
#define LUMEX_FEATURE_STD_COMPLEX_UDLS 0
#endif
#define LUMEX_HAS_STD_COMPLEX_UDLS (LUMEX_FEATURE_STD_COMPLEX_UDLS >= 201309L)

/// `__cpp_lib_concepts` (C++20; later: 202207L C++23).
#if defined(__cpp_lib_concepts)
#define LUMEX_FEATURE_STD_CONCEPTS __cpp_lib_concepts
#else
#define LUMEX_FEATURE_STD_CONCEPTS 0
#endif
#define LUMEX_HAS_STD_CONCEPTS (LUMEX_FEATURE_STD_CONCEPTS >= 202002L)

/// `__cpp_lib_constant_wrapper` (C++26; later: 202606L C++29).
#if defined(__cpp_lib_constant_wrapper)
#define LUMEX_FEATURE_STD_CONSTANT_WRAPPER __cpp_lib_constant_wrapper
#else
#define LUMEX_FEATURE_STD_CONSTANT_WRAPPER 0
#endif
#define LUMEX_HAS_STD_CONSTANT_WRAPPER                                        \
  (LUMEX_FEATURE_STD_CONSTANT_WRAPPER >= 202603L)

/// `__cpp_lib_constexpr_algorithms` (C++20; later: 202306L C++26).
#if defined(__cpp_lib_constexpr_algorithms)
#define LUMEX_FEATURE_STD_CONSTEXPR_ALGORITHMS __cpp_lib_constexpr_algorithms
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_ALGORITHMS 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_ALGORITHMS                                    \
  (LUMEX_FEATURE_STD_CONSTEXPR_ALGORITHMS >= 201806L)

/// `__cpp_lib_constexpr_atomic` (C++26).
#if defined(__cpp_lib_constexpr_atomic)
#define LUMEX_FEATURE_STD_CONSTEXPR_ATOMIC __cpp_lib_constexpr_atomic
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_ATOMIC 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_ATOMIC                                        \
  (LUMEX_FEATURE_STD_CONSTEXPR_ATOMIC >= 202411L)

/// `__cpp_lib_constexpr_bitset` (C++23).
#if defined(__cpp_lib_constexpr_bitset)
#define LUMEX_FEATURE_STD_CONSTEXPR_BITSET __cpp_lib_constexpr_bitset
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_BITSET 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_BITSET                                        \
  (LUMEX_FEATURE_STD_CONSTEXPR_BITSET >= 202207L)

/// `__cpp_lib_constexpr_charconv` (C++23).
#if defined(__cpp_lib_constexpr_charconv)
#define LUMEX_FEATURE_STD_CONSTEXPR_CHARCONV __cpp_lib_constexpr_charconv
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_CHARCONV 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_CHARCONV                                      \
  (LUMEX_FEATURE_STD_CONSTEXPR_CHARCONV >= 202207L)

/// `__cpp_lib_constexpr_cmath` (C++23; later: 202306L C++26).
#if defined(__cpp_lib_constexpr_cmath)
#define LUMEX_FEATURE_STD_CONSTEXPR_CMATH __cpp_lib_constexpr_cmath
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_CMATH 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_CMATH                                         \
  (LUMEX_FEATURE_STD_CONSTEXPR_CMATH >= 202202L)

/// `__cpp_lib_constexpr_complex` (C++20; later: 202306L C++26).
#if defined(__cpp_lib_constexpr_complex)
#define LUMEX_FEATURE_STD_CONSTEXPR_COMPLEX __cpp_lib_constexpr_complex
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_COMPLEX 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_COMPLEX                                       \
  (LUMEX_FEATURE_STD_CONSTEXPR_COMPLEX >= 201711L)

/// `__cpp_lib_constexpr_deque` (C++26).
#if defined(__cpp_lib_constexpr_deque)
#define LUMEX_FEATURE_STD_CONSTEXPR_DEQUE __cpp_lib_constexpr_deque
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_DEQUE 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_DEQUE                                         \
  (LUMEX_FEATURE_STD_CONSTEXPR_DEQUE >= 202502L)

/// `__cpp_lib_constexpr_dynamic_alloc` (C++20).
#if defined(__cpp_lib_constexpr_dynamic_alloc)
#define LUMEX_FEATURE_STD_CONSTEXPR_DYNAMIC_ALLOC                             \
  __cpp_lib_constexpr_dynamic_alloc
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_DYNAMIC_ALLOC 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_DYNAMIC_ALLOC                                 \
  (LUMEX_FEATURE_STD_CONSTEXPR_DYNAMIC_ALLOC >= 201907L)

/// `__cpp_lib_constexpr_exceptions` (C++26; later: 202502L C++26).
#if defined(__cpp_lib_constexpr_exceptions)
#define LUMEX_FEATURE_STD_CONSTEXPR_EXCEPTIONS __cpp_lib_constexpr_exceptions
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_EXCEPTIONS 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_EXCEPTIONS                                    \
  (LUMEX_FEATURE_STD_CONSTEXPR_EXCEPTIONS >= 202411L)

/// `__cpp_lib_constexpr_flat_map` (C++26).
#if defined(__cpp_lib_constexpr_flat_map)
#define LUMEX_FEATURE_STD_CONSTEXPR_FLAT_MAP __cpp_lib_constexpr_flat_map
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_FLAT_MAP 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_FLAT_MAP                                      \
  (LUMEX_FEATURE_STD_CONSTEXPR_FLAT_MAP >= 202502L)

/// `__cpp_lib_constexpr_flat_set` (C++26).
#if defined(__cpp_lib_constexpr_flat_set)
#define LUMEX_FEATURE_STD_CONSTEXPR_FLAT_SET __cpp_lib_constexpr_flat_set
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_FLAT_SET 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_FLAT_SET                                      \
  (LUMEX_FEATURE_STD_CONSTEXPR_FLAT_SET >= 202502L)

/// `__cpp_lib_constexpr_format` (C++26).
#if defined(__cpp_lib_constexpr_format)
#define LUMEX_FEATURE_STD_CONSTEXPR_FORMAT __cpp_lib_constexpr_format
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_FORMAT 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_FORMAT                                        \
  (LUMEX_FEATURE_STD_CONSTEXPR_FORMAT >= 202511L)

/// `__cpp_lib_constexpr_forward_list` (C++26).
#if defined(__cpp_lib_constexpr_forward_list)
#define LUMEX_FEATURE_STD_CONSTEXPR_FORWARD_LIST                              \
  __cpp_lib_constexpr_forward_list
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_FORWARD_LIST 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_FORWARD_LIST                                  \
  (LUMEX_FEATURE_STD_CONSTEXPR_FORWARD_LIST >= 202502L)

/// `__cpp_lib_constexpr_functional` (C++20).
#if defined(__cpp_lib_constexpr_functional)
#define LUMEX_FEATURE_STD_CONSTEXPR_FUNCTIONAL __cpp_lib_constexpr_functional
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_FUNCTIONAL 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_FUNCTIONAL                                    \
  (LUMEX_FEATURE_STD_CONSTEXPR_FUNCTIONAL >= 201907L)

/// `__cpp_lib_constexpr_inplace_vector` (C++26).
#if defined(__cpp_lib_constexpr_inplace_vector)
#define LUMEX_FEATURE_STD_CONSTEXPR_INPLACE_VECTOR                            \
  __cpp_lib_constexpr_inplace_vector
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_INPLACE_VECTOR 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_INPLACE_VECTOR                                \
  (LUMEX_FEATURE_STD_CONSTEXPR_INPLACE_VECTOR >= 202502L)

/// `__cpp_lib_constexpr_iterator` (C++20).
#if defined(__cpp_lib_constexpr_iterator)
#define LUMEX_FEATURE_STD_CONSTEXPR_ITERATOR __cpp_lib_constexpr_iterator
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_ITERATOR 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_ITERATOR                                      \
  (LUMEX_FEATURE_STD_CONSTEXPR_ITERATOR >= 201811L)

/// `__cpp_lib_constexpr_list` (C++26).
#if defined(__cpp_lib_constexpr_list)
#define LUMEX_FEATURE_STD_CONSTEXPR_LIST __cpp_lib_constexpr_list
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_LIST 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_LIST                                          \
  (LUMEX_FEATURE_STD_CONSTEXPR_LIST >= 202502L)

/// `__cpp_lib_constexpr_map` (C++26).
#if defined(__cpp_lib_constexpr_map)
#define LUMEX_FEATURE_STD_CONSTEXPR_MAP __cpp_lib_constexpr_map
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_MAP 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_MAP                                           \
  (LUMEX_FEATURE_STD_CONSTEXPR_MAP >= 202502L)

/// `__cpp_lib_constexpr_memory` (C++20; later: 202202L C++23, 202506L C++26).
#if defined(__cpp_lib_constexpr_memory)
#define LUMEX_FEATURE_STD_CONSTEXPR_MEMORY __cpp_lib_constexpr_memory
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_MEMORY 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_MEMORY                                        \
  (LUMEX_FEATURE_STD_CONSTEXPR_MEMORY >= 201811L)

/// `__cpp_lib_constexpr_new` (C++26).
#if defined(__cpp_lib_constexpr_new)
#define LUMEX_FEATURE_STD_CONSTEXPR_NEW __cpp_lib_constexpr_new
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_NEW 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_NEW                                           \
  (LUMEX_FEATURE_STD_CONSTEXPR_NEW >= 202406L)

/// `__cpp_lib_constexpr_numeric` (C++20).
#if defined(__cpp_lib_constexpr_numeric)
#define LUMEX_FEATURE_STD_CONSTEXPR_NUMERIC __cpp_lib_constexpr_numeric
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_NUMERIC 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_NUMERIC                                       \
  (LUMEX_FEATURE_STD_CONSTEXPR_NUMERIC >= 201911L)

/// `__cpp_lib_constexpr_queue` (C++26).
#if defined(__cpp_lib_constexpr_queue)
#define LUMEX_FEATURE_STD_CONSTEXPR_QUEUE __cpp_lib_constexpr_queue
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_QUEUE 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_QUEUE                                         \
  (LUMEX_FEATURE_STD_CONSTEXPR_QUEUE >= 202502L)

/// `__cpp_lib_constexpr_set` (C++26).
#if defined(__cpp_lib_constexpr_set)
#define LUMEX_FEATURE_STD_CONSTEXPR_SET __cpp_lib_constexpr_set
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_SET 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_SET                                           \
  (LUMEX_FEATURE_STD_CONSTEXPR_SET >= 202502L)

/// `__cpp_lib_constexpr_stack` (C++26).
#if defined(__cpp_lib_constexpr_stack)
#define LUMEX_FEATURE_STD_CONSTEXPR_STACK __cpp_lib_constexpr_stack
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_STACK 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_STACK                                         \
  (LUMEX_FEATURE_STD_CONSTEXPR_STACK >= 202502L)

/// `__cpp_lib_constexpr_string` (C++17; later: 201811L C++20, 201907L C++20).
#if defined(__cpp_lib_constexpr_string)
#define LUMEX_FEATURE_STD_CONSTEXPR_STRING __cpp_lib_constexpr_string
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_STRING 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_STRING                                        \
  (LUMEX_FEATURE_STD_CONSTEXPR_STRING >= 201611L)

/// `__cpp_lib_constexpr_string_view` (C++20).
#if defined(__cpp_lib_constexpr_string_view)
#define LUMEX_FEATURE_STD_CONSTEXPR_STRING_VIEW __cpp_lib_constexpr_string_view
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_STRING_VIEW 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_STRING_VIEW                                   \
  (LUMEX_FEATURE_STD_CONSTEXPR_STRING_VIEW >= 201811L)

/// `__cpp_lib_constexpr_tuple` (C++20).
#if defined(__cpp_lib_constexpr_tuple)
#define LUMEX_FEATURE_STD_CONSTEXPR_TUPLE __cpp_lib_constexpr_tuple
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_TUPLE 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_TUPLE                                         \
  (LUMEX_FEATURE_STD_CONSTEXPR_TUPLE >= 201811L)

/// `__cpp_lib_constexpr_typeinfo` (C++23).
#if defined(__cpp_lib_constexpr_typeinfo)
#define LUMEX_FEATURE_STD_CONSTEXPR_TYPEINFO __cpp_lib_constexpr_typeinfo
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_TYPEINFO 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_TYPEINFO                                      \
  (LUMEX_FEATURE_STD_CONSTEXPR_TYPEINFO >= 202106L)

/// `__cpp_lib_constexpr_unordered_map` (C++26).
#if defined(__cpp_lib_constexpr_unordered_map)
#define LUMEX_FEATURE_STD_CONSTEXPR_UNORDERED_MAP                             \
  __cpp_lib_constexpr_unordered_map
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_UNORDERED_MAP 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_UNORDERED_MAP                                 \
  (LUMEX_FEATURE_STD_CONSTEXPR_UNORDERED_MAP >= 202502L)

/// `__cpp_lib_constexpr_unordered_set` (C++26).
#if defined(__cpp_lib_constexpr_unordered_set)
#define LUMEX_FEATURE_STD_CONSTEXPR_UNORDERED_SET                             \
  __cpp_lib_constexpr_unordered_set
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_UNORDERED_SET 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_UNORDERED_SET                                 \
  (LUMEX_FEATURE_STD_CONSTEXPR_UNORDERED_SET >= 202502L)

/// `__cpp_lib_constexpr_utility` (C++20).
#if defined(__cpp_lib_constexpr_utility)
#define LUMEX_FEATURE_STD_CONSTEXPR_UTILITY __cpp_lib_constexpr_utility
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_UTILITY 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_UTILITY                                       \
  (LUMEX_FEATURE_STD_CONSTEXPR_UTILITY >= 201811L)

/// `__cpp_lib_constexpr_vector` (C++20).
#if defined(__cpp_lib_constexpr_vector)
#define LUMEX_FEATURE_STD_CONSTEXPR_VECTOR __cpp_lib_constexpr_vector
#else
#define LUMEX_FEATURE_STD_CONSTEXPR_VECTOR 0
#endif
#define LUMEX_HAS_STD_CONSTEXPR_VECTOR                                        \
  (LUMEX_FEATURE_STD_CONSTEXPR_VECTOR >= 201907L)

/// `__cpp_lib_constrained_equality` (C++26; later: 202411L C++26).
#if defined(__cpp_lib_constrained_equality)
#define LUMEX_FEATURE_STD_CONSTRAINED_EQUALITY __cpp_lib_constrained_equality
#else
#define LUMEX_FEATURE_STD_CONSTRAINED_EQUALITY 0
#endif
#define LUMEX_HAS_STD_CONSTRAINED_EQUALITY                                    \
  (LUMEX_FEATURE_STD_CONSTRAINED_EQUALITY >= 202403L)

/// `__cpp_lib_containers_ranges` (C++23).
#if defined(__cpp_lib_containers_ranges)
#define LUMEX_FEATURE_STD_CONTAINERS_RANGES __cpp_lib_containers_ranges
#else
#define LUMEX_FEATURE_STD_CONTAINERS_RANGES 0
#endif
#define LUMEX_HAS_STD_CONTAINERS_RANGES                                       \
  (LUMEX_FEATURE_STD_CONTAINERS_RANGES >= 202202L)

/// `__cpp_lib_contracts` (C++26).
#if defined(__cpp_lib_contracts)
#define LUMEX_FEATURE_STD_CONTRACTS __cpp_lib_contracts
#else
#define LUMEX_FEATURE_STD_CONTRACTS 0
#endif
#define LUMEX_HAS_STD_CONTRACTS (LUMEX_FEATURE_STD_CONTRACTS >= 202502L)

/// `__cpp_lib_copyable_function` (C++26).
#if defined(__cpp_lib_copyable_function)
#define LUMEX_FEATURE_STD_COPYABLE_FUNCTION __cpp_lib_copyable_function
#else
#define LUMEX_FEATURE_STD_COPYABLE_FUNCTION 0
#endif
#define LUMEX_HAS_STD_COPYABLE_FUNCTION                                       \
  (LUMEX_FEATURE_STD_COPYABLE_FUNCTION >= 202306L)

/// `__cpp_lib_coroutine` (C++20).
#if defined(__cpp_lib_coroutine)
#define LUMEX_FEATURE_STD_COROUTINE __cpp_lib_coroutine
#else
#define LUMEX_FEATURE_STD_COROUTINE 0
#endif
#define LUMEX_HAS_STD_COROUTINE (LUMEX_FEATURE_STD_COROUTINE >= 201902L)

/// `__cpp_lib_counting_scope` (C++26).
#if defined(__cpp_lib_counting_scope)
#define LUMEX_FEATURE_STD_COUNTING_SCOPE __cpp_lib_counting_scope
#else
#define LUMEX_FEATURE_STD_COUNTING_SCOPE 0
#endif
#define LUMEX_HAS_STD_COUNTING_SCOPE                                          \
  (LUMEX_FEATURE_STD_COUNTING_SCOPE >= 202506L)

/// `__cpp_lib_debugging` (C++26; later: 202403L C++26).
#if defined(__cpp_lib_debugging)
#define LUMEX_FEATURE_STD_DEBUGGING __cpp_lib_debugging
#else
#define LUMEX_FEATURE_STD_DEBUGGING 0
#endif
#define LUMEX_HAS_STD_DEBUGGING (LUMEX_FEATURE_STD_DEBUGGING >= 202311L)

/// `__cpp_lib_define_static` (C++26).
#if defined(__cpp_lib_define_static)
#define LUMEX_FEATURE_STD_DEFINE_STATIC __cpp_lib_define_static
#else
#define LUMEX_FEATURE_STD_DEFINE_STATIC 0
#endif
#define LUMEX_HAS_STD_DEFINE_STATIC                                           \
  (LUMEX_FEATURE_STD_DEFINE_STATIC >= 202506L)

/// `__cpp_lib_destroying_delete` (C++20).
#if defined(__cpp_lib_destroying_delete)
#define LUMEX_FEATURE_STD_DESTROYING_DELETE __cpp_lib_destroying_delete
#else
#define LUMEX_FEATURE_STD_DESTROYING_DELETE 0
#endif
#define LUMEX_HAS_STD_DESTROYING_DELETE                                       \
  (LUMEX_FEATURE_STD_DESTROYING_DELETE >= 201806L)

/// `__cpp_lib_enable_shared_from_this` (C++17).
#if defined(__cpp_lib_enable_shared_from_this)
#define LUMEX_FEATURE_STD_ENABLE_SHARED_FROM_THIS                             \
  __cpp_lib_enable_shared_from_this
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_ENABLE_SHARED_FROM_THIS 201603L
#else
#define LUMEX_FEATURE_STD_ENABLE_SHARED_FROM_THIS 0
#endif
#define LUMEX_HAS_STD_ENABLE_SHARED_FROM_THIS                                 \
  (LUMEX_FEATURE_STD_ENABLE_SHARED_FROM_THIS >= 201603L)

/// `__cpp_lib_endian` (C++20).
#if defined(__cpp_lib_endian)
#define LUMEX_FEATURE_STD_ENDIAN __cpp_lib_endian
#else
#define LUMEX_FEATURE_STD_ENDIAN 0
#endif
#define LUMEX_HAS_STD_ENDIAN (LUMEX_FEATURE_STD_ENDIAN >= 201907L)

/// `__cpp_lib_erase_if` (C++20).
#if defined(__cpp_lib_erase_if)
#define LUMEX_FEATURE_STD_ERASE_IF __cpp_lib_erase_if
#else
#define LUMEX_FEATURE_STD_ERASE_IF 0
#endif
#define LUMEX_HAS_STD_ERASE_IF (LUMEX_FEATURE_STD_ERASE_IF >= 202002L)

/// `__cpp_lib_exception_ptr_cast` (C++26).
#if defined(__cpp_lib_exception_ptr_cast)
#define LUMEX_FEATURE_STD_EXCEPTION_PTR_CAST __cpp_lib_exception_ptr_cast
#else
#define LUMEX_FEATURE_STD_EXCEPTION_PTR_CAST 0
#endif
#define LUMEX_HAS_STD_EXCEPTION_PTR_CAST                                      \
  (LUMEX_FEATURE_STD_EXCEPTION_PTR_CAST >= 202603L)

/// `__cpp_lib_exchange_function` (C++14).
#if defined(__cpp_lib_exchange_function)
#define LUMEX_FEATURE_STD_EXCHANGE_FUNCTION __cpp_lib_exchange_function
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201402L
#define LUMEX_FEATURE_STD_EXCHANGE_FUNCTION 201304L
#else
#define LUMEX_FEATURE_STD_EXCHANGE_FUNCTION 0
#endif
#define LUMEX_HAS_STD_EXCHANGE_FUNCTION                                       \
  (LUMEX_FEATURE_STD_EXCHANGE_FUNCTION >= 201304L)

/// `__cpp_lib_execution` (C++17; later: 201902L C++20).
#if defined(__cpp_lib_execution)
#define LUMEX_FEATURE_STD_EXECUTION __cpp_lib_execution
#else
#define LUMEX_FEATURE_STD_EXECUTION 0
#endif
#define LUMEX_HAS_STD_EXECUTION (LUMEX_FEATURE_STD_EXECUTION >= 201603L)

/// `__cpp_lib_expected` (C++23; later: 202211L C++23, 202606L C++29).
#if defined(__cpp_lib_expected)
#define LUMEX_FEATURE_STD_EXPECTED __cpp_lib_expected
#else
#define LUMEX_FEATURE_STD_EXPECTED 0
#endif
#define LUMEX_HAS_STD_EXPECTED (LUMEX_FEATURE_STD_EXPECTED >= 202202L)

/// `__cpp_lib_filesystem` (C++17).
#if defined(__cpp_lib_filesystem)
#define LUMEX_FEATURE_STD_FILESYSTEM __cpp_lib_filesystem
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_FILESYSTEM 201703L
#else
#define LUMEX_FEATURE_STD_FILESYSTEM 0
#endif
#define LUMEX_HAS_STD_FILESYSTEM (LUMEX_FEATURE_STD_FILESYSTEM >= 201703L)

/// `__cpp_lib_flat_map` (C++23; later: 202511L C++26).
#if defined(__cpp_lib_flat_map)
#define LUMEX_FEATURE_STD_FLAT_MAP __cpp_lib_flat_map
#else
#define LUMEX_FEATURE_STD_FLAT_MAP 0
#endif
#define LUMEX_HAS_STD_FLAT_MAP (LUMEX_FEATURE_STD_FLAT_MAP >= 202207L)

/// `__cpp_lib_flat_set` (C++23; later: 202511L C++26).
#if defined(__cpp_lib_flat_set)
#define LUMEX_FEATURE_STD_FLAT_SET __cpp_lib_flat_set
#else
#define LUMEX_FEATURE_STD_FLAT_SET 0
#endif
#define LUMEX_HAS_STD_FLAT_SET (LUMEX_FEATURE_STD_FLAT_SET >= 202207L)

/// `__cpp_lib_format` (C++20; later: 202106L C++23, 202110L C++23, 202207L
/// C++23, 202304L C++26, 202305L C++26, 202306L C++26, 202311L C++26, 202603L
/// C++26).
#if defined(__cpp_lib_format)
#define LUMEX_FEATURE_STD_FORMAT __cpp_lib_format
#else
#define LUMEX_FEATURE_STD_FORMAT 0
#endif
#define LUMEX_HAS_STD_FORMAT (LUMEX_FEATURE_STD_FORMAT >= 201907L)

/// `__cpp_lib_format_path` (C++26; later: 202506L C++26).
#if defined(__cpp_lib_format_path)
#define LUMEX_FEATURE_STD_FORMAT_PATH __cpp_lib_format_path
#else
#define LUMEX_FEATURE_STD_FORMAT_PATH 0
#endif
#define LUMEX_HAS_STD_FORMAT_PATH (LUMEX_FEATURE_STD_FORMAT_PATH >= 202403L)

/// `__cpp_lib_format_ranges` (C++23).
#if defined(__cpp_lib_format_ranges)
#define LUMEX_FEATURE_STD_FORMAT_RANGES __cpp_lib_format_ranges
#else
#define LUMEX_FEATURE_STD_FORMAT_RANGES 0
#endif
#define LUMEX_HAS_STD_FORMAT_RANGES                                           \
  (LUMEX_FEATURE_STD_FORMAT_RANGES >= 202207L)

/// `__cpp_lib_format_uchar` (C++26).
#if defined(__cpp_lib_format_uchar)
#define LUMEX_FEATURE_STD_FORMAT_UCHAR __cpp_lib_format_uchar
#else
#define LUMEX_FEATURE_STD_FORMAT_UCHAR 0
#endif
#define LUMEX_HAS_STD_FORMAT_UCHAR (LUMEX_FEATURE_STD_FORMAT_UCHAR >= 202311L)

/// `__cpp_lib_formatters` (C++23).
#if defined(__cpp_lib_formatters)
#define LUMEX_FEATURE_STD_FORMATTERS __cpp_lib_formatters
#else
#define LUMEX_FEATURE_STD_FORMATTERS 0
#endif
#define LUMEX_HAS_STD_FORMATTERS (LUMEX_FEATURE_STD_FORMATTERS >= 202302L)

/// `__cpp_lib_forward_like` (C++23).
#if defined(__cpp_lib_forward_like)
#define LUMEX_FEATURE_STD_FORWARD_LIKE __cpp_lib_forward_like
#else
#define LUMEX_FEATURE_STD_FORWARD_LIKE 0
#endif
#define LUMEX_HAS_STD_FORWARD_LIKE (LUMEX_FEATURE_STD_FORWARD_LIKE >= 202207L)

/// `__cpp_lib_freestanding_algorithm` (C++26; later: 202502L C++26).
#if defined(__cpp_lib_freestanding_algorithm)
#define LUMEX_FEATURE_STD_FREESTANDING_ALGORITHM                              \
  __cpp_lib_freestanding_algorithm
#else
#define LUMEX_FEATURE_STD_FREESTANDING_ALGORITHM 0
#endif
#define LUMEX_HAS_STD_FREESTANDING_ALGORITHM                                  \
  (LUMEX_FEATURE_STD_FREESTANDING_ALGORITHM >= 202311L)

/// `__cpp_lib_freestanding_array` (C++26).
#if defined(__cpp_lib_freestanding_array)
#define LUMEX_FEATURE_STD_FREESTANDING_ARRAY __cpp_lib_freestanding_array
#else
#define LUMEX_FEATURE_STD_FREESTANDING_ARRAY 0
#endif
#define LUMEX_HAS_STD_FREESTANDING_ARRAY                                      \
  (LUMEX_FEATURE_STD_FREESTANDING_ARRAY >= 202311L)

/// `__cpp_lib_freestanding_char_traits` (C++26).
#if defined(__cpp_lib_freestanding_char_traits)
#define LUMEX_FEATURE_STD_FREESTANDING_CHAR_TRAITS                            \
  __cpp_lib_freestanding_char_traits
#else
#define LUMEX_FEATURE_STD_FREESTANDING_CHAR_TRAITS 0
#endif
#define LUMEX_HAS_STD_FREESTANDING_CHAR_TRAITS                                \
  (LUMEX_FEATURE_STD_FREESTANDING_CHAR_TRAITS >= 202306L)

/// `__cpp_lib_freestanding_charconv` (C++26).
#if defined(__cpp_lib_freestanding_charconv)
#define LUMEX_FEATURE_STD_FREESTANDING_CHARCONV __cpp_lib_freestanding_charconv
#else
#define LUMEX_FEATURE_STD_FREESTANDING_CHARCONV 0
#endif
#define LUMEX_HAS_STD_FREESTANDING_CHARCONV                                   \
  (LUMEX_FEATURE_STD_FREESTANDING_CHARCONV >= 202306L)

/// `__cpp_lib_freestanding_cstdlib` (C++26).
#if defined(__cpp_lib_freestanding_cstdlib)
#define LUMEX_FEATURE_STD_FREESTANDING_CSTDLIB __cpp_lib_freestanding_cstdlib
#else
#define LUMEX_FEATURE_STD_FREESTANDING_CSTDLIB 0
#endif
#define LUMEX_HAS_STD_FREESTANDING_CSTDLIB                                    \
  (LUMEX_FEATURE_STD_FREESTANDING_CSTDLIB >= 202306L)

/// `__cpp_lib_freestanding_cstring` (C++26; later: 202311L C++26).
#if defined(__cpp_lib_freestanding_cstring)
#define LUMEX_FEATURE_STD_FREESTANDING_CSTRING __cpp_lib_freestanding_cstring
#else
#define LUMEX_FEATURE_STD_FREESTANDING_CSTRING 0
#endif
#define LUMEX_HAS_STD_FREESTANDING_CSTRING                                    \
  (LUMEX_FEATURE_STD_FREESTANDING_CSTRING >= 202306L)

/// `__cpp_lib_freestanding_cwchar` (C++26).
#if defined(__cpp_lib_freestanding_cwchar)
#define LUMEX_FEATURE_STD_FREESTANDING_CWCHAR __cpp_lib_freestanding_cwchar
#else
#define LUMEX_FEATURE_STD_FREESTANDING_CWCHAR 0
#endif
#define LUMEX_HAS_STD_FREESTANDING_CWCHAR                                     \
  (LUMEX_FEATURE_STD_FREESTANDING_CWCHAR >= 202306L)

/// `__cpp_lib_freestanding_errc` (C++26).
#if defined(__cpp_lib_freestanding_errc)
#define LUMEX_FEATURE_STD_FREESTANDING_ERRC __cpp_lib_freestanding_errc
#else
#define LUMEX_FEATURE_STD_FREESTANDING_ERRC 0
#endif
#define LUMEX_HAS_STD_FREESTANDING_ERRC                                       \
  (LUMEX_FEATURE_STD_FREESTANDING_ERRC >= 202306L)

/// `__cpp_lib_freestanding_execution` (C++26).
#if defined(__cpp_lib_freestanding_execution)
#define LUMEX_FEATURE_STD_FREESTANDING_EXECUTION                              \
  __cpp_lib_freestanding_execution
#else
#define LUMEX_FEATURE_STD_FREESTANDING_EXECUTION 0
#endif
#define LUMEX_HAS_STD_FREESTANDING_EXECUTION                                  \
  (LUMEX_FEATURE_STD_FREESTANDING_EXECUTION >= 202502L)

/// `__cpp_lib_freestanding_expected` (C++26).
#if defined(__cpp_lib_freestanding_expected)
#define LUMEX_FEATURE_STD_FREESTANDING_EXPECTED __cpp_lib_freestanding_expected
#else
#define LUMEX_FEATURE_STD_FREESTANDING_EXPECTED 0
#endif
#define LUMEX_HAS_STD_FREESTANDING_EXPECTED                                   \
  (LUMEX_FEATURE_STD_FREESTANDING_EXPECTED >= 202311L)

/// `__cpp_lib_freestanding_feature_test_macros` (C++26).
#if defined(__cpp_lib_freestanding_feature_test_macros)
#define LUMEX_FEATURE_STD_FREESTANDING_FEATURE_TEST_MACROS                    \
  __cpp_lib_freestanding_feature_test_macros
#else
#define LUMEX_FEATURE_STD_FREESTANDING_FEATURE_TEST_MACROS 0
#endif
#define LUMEX_HAS_STD_FREESTANDING_FEATURE_TEST_MACROS                        \
  (LUMEX_FEATURE_STD_FREESTANDING_FEATURE_TEST_MACROS >= 202306L)

/// `__cpp_lib_freestanding_functional` (C++26).
#if defined(__cpp_lib_freestanding_functional)
#define LUMEX_FEATURE_STD_FREESTANDING_FUNCTIONAL                             \
  __cpp_lib_freestanding_functional
#else
#define LUMEX_FEATURE_STD_FREESTANDING_FUNCTIONAL 0
#endif
#define LUMEX_HAS_STD_FREESTANDING_FUNCTIONAL                                 \
  (LUMEX_FEATURE_STD_FREESTANDING_FUNCTIONAL >= 202306L)

/// `__cpp_lib_freestanding_iterator` (C++26).
#if defined(__cpp_lib_freestanding_iterator)
#define LUMEX_FEATURE_STD_FREESTANDING_ITERATOR __cpp_lib_freestanding_iterator
#else
#define LUMEX_FEATURE_STD_FREESTANDING_ITERATOR 0
#endif
#define LUMEX_HAS_STD_FREESTANDING_ITERATOR                                   \
  (LUMEX_FEATURE_STD_FREESTANDING_ITERATOR >= 202306L)

/// `__cpp_lib_freestanding_mdspan` (C++26).
#if defined(__cpp_lib_freestanding_mdspan)
#define LUMEX_FEATURE_STD_FREESTANDING_MDSPAN __cpp_lib_freestanding_mdspan
#else
#define LUMEX_FEATURE_STD_FREESTANDING_MDSPAN 0
#endif
#define LUMEX_HAS_STD_FREESTANDING_MDSPAN                                     \
  (LUMEX_FEATURE_STD_FREESTANDING_MDSPAN >= 202311L)

/// `__cpp_lib_freestanding_memory` (C++26; later: 202502L C++26).
#if defined(__cpp_lib_freestanding_memory)
#define LUMEX_FEATURE_STD_FREESTANDING_MEMORY __cpp_lib_freestanding_memory
#else
#define LUMEX_FEATURE_STD_FREESTANDING_MEMORY 0
#endif
#define LUMEX_HAS_STD_FREESTANDING_MEMORY                                     \
  (LUMEX_FEATURE_STD_FREESTANDING_MEMORY >= 202306L)

/// `__cpp_lib_freestanding_numeric` (C++26; later: 202502L C++26).
#if defined(__cpp_lib_freestanding_numeric)
#define LUMEX_FEATURE_STD_FREESTANDING_NUMERIC __cpp_lib_freestanding_numeric
#else
#define LUMEX_FEATURE_STD_FREESTANDING_NUMERIC 0
#endif
#define LUMEX_HAS_STD_FREESTANDING_NUMERIC                                    \
  (LUMEX_FEATURE_STD_FREESTANDING_NUMERIC >= 202311L)

/// `__cpp_lib_freestanding_operator_new` (C++26).
#if defined(__cpp_lib_freestanding_operator_new)
#define LUMEX_FEATURE_STD_FREESTANDING_OPERATOR_NEW                           \
  __cpp_lib_freestanding_operator_new
#else
#define LUMEX_FEATURE_STD_FREESTANDING_OPERATOR_NEW 0
#endif
#define LUMEX_HAS_STD_FREESTANDING_OPERATOR_NEW                               \
  (LUMEX_FEATURE_STD_FREESTANDING_OPERATOR_NEW >= 202306L)

/// `__cpp_lib_freestanding_optional` (C++26; later: 202506L C++26).
#if defined(__cpp_lib_freestanding_optional)
#define LUMEX_FEATURE_STD_FREESTANDING_OPTIONAL __cpp_lib_freestanding_optional
#else
#define LUMEX_FEATURE_STD_FREESTANDING_OPTIONAL 0
#endif
#define LUMEX_HAS_STD_FREESTANDING_OPTIONAL                                   \
  (LUMEX_FEATURE_STD_FREESTANDING_OPTIONAL >= 202311L)

/// `__cpp_lib_freestanding_random` (C++26).
#if defined(__cpp_lib_freestanding_random)
#define LUMEX_FEATURE_STD_FREESTANDING_RANDOM __cpp_lib_freestanding_random
#else
#define LUMEX_FEATURE_STD_FREESTANDING_RANDOM 0
#endif
#define LUMEX_HAS_STD_FREESTANDING_RANDOM                                     \
  (LUMEX_FEATURE_STD_FREESTANDING_RANDOM >= 202502L)

/// `__cpp_lib_freestanding_ranges` (C++26).
#if defined(__cpp_lib_freestanding_ranges)
#define LUMEX_FEATURE_STD_FREESTANDING_RANGES __cpp_lib_freestanding_ranges
#else
#define LUMEX_FEATURE_STD_FREESTANDING_RANGES 0
#endif
#define LUMEX_HAS_STD_FREESTANDING_RANGES                                     \
  (LUMEX_FEATURE_STD_FREESTANDING_RANGES >= 202306L)

/// `__cpp_lib_freestanding_ratio` (C++26).
#if defined(__cpp_lib_freestanding_ratio)
#define LUMEX_FEATURE_STD_FREESTANDING_RATIO __cpp_lib_freestanding_ratio
#else
#define LUMEX_FEATURE_STD_FREESTANDING_RATIO 0
#endif
#define LUMEX_HAS_STD_FREESTANDING_RATIO                                      \
  (LUMEX_FEATURE_STD_FREESTANDING_RATIO >= 202306L)

/// `__cpp_lib_freestanding_string_view` (C++26).
#if defined(__cpp_lib_freestanding_string_view)
#define LUMEX_FEATURE_STD_FREESTANDING_STRING_VIEW                            \
  __cpp_lib_freestanding_string_view
#else
#define LUMEX_FEATURE_STD_FREESTANDING_STRING_VIEW 0
#endif
#define LUMEX_HAS_STD_FREESTANDING_STRING_VIEW                                \
  (LUMEX_FEATURE_STD_FREESTANDING_STRING_VIEW >= 202311L)

/// `__cpp_lib_freestanding_tuple` (C++26).
#if defined(__cpp_lib_freestanding_tuple)
#define LUMEX_FEATURE_STD_FREESTANDING_TUPLE __cpp_lib_freestanding_tuple
#else
#define LUMEX_FEATURE_STD_FREESTANDING_TUPLE 0
#endif
#define LUMEX_HAS_STD_FREESTANDING_TUPLE                                      \
  (LUMEX_FEATURE_STD_FREESTANDING_TUPLE >= 202306L)

/// `__cpp_lib_freestanding_utility` (C++26).
#if defined(__cpp_lib_freestanding_utility)
#define LUMEX_FEATURE_STD_FREESTANDING_UTILITY __cpp_lib_freestanding_utility
#else
#define LUMEX_FEATURE_STD_FREESTANDING_UTILITY 0
#endif
#define LUMEX_HAS_STD_FREESTANDING_UTILITY                                    \
  (LUMEX_FEATURE_STD_FREESTANDING_UTILITY >= 202306L)

/// `__cpp_lib_freestanding_variant` (C++26).
#if defined(__cpp_lib_freestanding_variant)
#define LUMEX_FEATURE_STD_FREESTANDING_VARIANT __cpp_lib_freestanding_variant
#else
#define LUMEX_FEATURE_STD_FREESTANDING_VARIANT 0
#endif
#define LUMEX_HAS_STD_FREESTANDING_VARIANT                                    \
  (LUMEX_FEATURE_STD_FREESTANDING_VARIANT >= 202311L)

/// `__cpp_lib_fstream_native_handle` (C++26).
#if defined(__cpp_lib_fstream_native_handle)
#define LUMEX_FEATURE_STD_FSTREAM_NATIVE_HANDLE __cpp_lib_fstream_native_handle
#else
#define LUMEX_FEATURE_STD_FSTREAM_NATIVE_HANDLE 0
#endif
#define LUMEX_HAS_STD_FSTREAM_NATIVE_HANDLE                                   \
  (LUMEX_FEATURE_STD_FSTREAM_NATIVE_HANDLE >= 202306L)

/// `__cpp_lib_function_ref` (C++26).
#if defined(__cpp_lib_function_ref)
#define LUMEX_FEATURE_STD_FUNCTION_REF __cpp_lib_function_ref
#else
#define LUMEX_FEATURE_STD_FUNCTION_REF 0
#endif
#define LUMEX_HAS_STD_FUNCTION_REF (LUMEX_FEATURE_STD_FUNCTION_REF >= 202306L)

/// `__cpp_lib_gcd_lcm` (C++17).
#if defined(__cpp_lib_gcd_lcm)
#define LUMEX_FEATURE_STD_GCD_LCM __cpp_lib_gcd_lcm
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_GCD_LCM 201606L
#else
#define LUMEX_FEATURE_STD_GCD_LCM 0
#endif
#define LUMEX_HAS_STD_GCD_LCM (LUMEX_FEATURE_STD_GCD_LCM >= 201606L)

/// `__cpp_lib_generator` (C++23).
#if defined(__cpp_lib_generator)
#define LUMEX_FEATURE_STD_GENERATOR __cpp_lib_generator
#else
#define LUMEX_FEATURE_STD_GENERATOR 0
#endif
#define LUMEX_HAS_STD_GENERATOR (LUMEX_FEATURE_STD_GENERATOR >= 202207L)

/// `__cpp_lib_generic_associative_lookup` (C++14).
#if defined(__cpp_lib_generic_associative_lookup)
#define LUMEX_FEATURE_STD_GENERIC_ASSOCIATIVE_LOOKUP                          \
  __cpp_lib_generic_associative_lookup
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201402L
#define LUMEX_FEATURE_STD_GENERIC_ASSOCIATIVE_LOOKUP 201304L
#else
#define LUMEX_FEATURE_STD_GENERIC_ASSOCIATIVE_LOOKUP 0
#endif
#define LUMEX_HAS_STD_GENERIC_ASSOCIATIVE_LOOKUP                              \
  (LUMEX_FEATURE_STD_GENERIC_ASSOCIATIVE_LOOKUP >= 201304L)

/// `__cpp_lib_generic_unordered_lookup` (C++20).
#if defined(__cpp_lib_generic_unordered_lookup)
#define LUMEX_FEATURE_STD_GENERIC_UNORDERED_LOOKUP                            \
  __cpp_lib_generic_unordered_lookup
#else
#define LUMEX_FEATURE_STD_GENERIC_UNORDERED_LOOKUP 0
#endif
#define LUMEX_HAS_STD_GENERIC_UNORDERED_LOOKUP                                \
  (LUMEX_FEATURE_STD_GENERIC_UNORDERED_LOOKUP >= 201811L)

/// `__cpp_lib_hardware_interference_size` (C++17).
#if defined(__cpp_lib_hardware_interference_size)
#define LUMEX_FEATURE_STD_HARDWARE_INTERFERENCE_SIZE                          \
  __cpp_lib_hardware_interference_size
#else
#define LUMEX_FEATURE_STD_HARDWARE_INTERFERENCE_SIZE 0
#endif
#define LUMEX_HAS_STD_HARDWARE_INTERFERENCE_SIZE                              \
  (LUMEX_FEATURE_STD_HARDWARE_INTERFERENCE_SIZE >= 201703L)

/// `__cpp_lib_has_unique_object_representations` (C++17).
#if defined(__cpp_lib_has_unique_object_representations)
#define LUMEX_FEATURE_STD_HAS_UNIQUE_OBJECT_REPRESENTATIONS                   \
  __cpp_lib_has_unique_object_representations
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_HAS_UNIQUE_OBJECT_REPRESENTATIONS 201606L
#else
#define LUMEX_FEATURE_STD_HAS_UNIQUE_OBJECT_REPRESENTATIONS 0
#endif
#define LUMEX_HAS_STD_HAS_UNIQUE_OBJECT_REPRESENTATIONS                       \
  (LUMEX_FEATURE_STD_HAS_UNIQUE_OBJECT_REPRESENTATIONS >= 201606L)

/// `__cpp_lib_hazard_pointer` (C++26; later: 202606L C++29).
#if defined(__cpp_lib_hazard_pointer)
#define LUMEX_FEATURE_STD_HAZARD_POINTER __cpp_lib_hazard_pointer
#else
#define LUMEX_FEATURE_STD_HAZARD_POINTER 0
#endif
#define LUMEX_HAS_STD_HAZARD_POINTER                                          \
  (LUMEX_FEATURE_STD_HAZARD_POINTER >= 202306L)

/// `__cpp_lib_hive` (C++26).
#if defined(__cpp_lib_hive)
#define LUMEX_FEATURE_STD_HIVE __cpp_lib_hive
#else
#define LUMEX_FEATURE_STD_HIVE 0
#endif
#define LUMEX_HAS_STD_HIVE (LUMEX_FEATURE_STD_HIVE >= 202502L)

/// `__cpp_lib_hypot` (C++17).
#if defined(__cpp_lib_hypot)
#define LUMEX_FEATURE_STD_HYPOT __cpp_lib_hypot
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_HYPOT 201603L
#else
#define LUMEX_FEATURE_STD_HYPOT 0
#endif
#define LUMEX_HAS_STD_HYPOT (LUMEX_FEATURE_STD_HYPOT >= 201603L)

/// `__cpp_lib_incomplete_container_elements` (C++17).
#if defined(__cpp_lib_incomplete_container_elements)
#define LUMEX_FEATURE_STD_INCOMPLETE_CONTAINER_ELEMENTS                       \
  __cpp_lib_incomplete_container_elements
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_INCOMPLETE_CONTAINER_ELEMENTS 201505L
#else
#define LUMEX_FEATURE_STD_INCOMPLETE_CONTAINER_ELEMENTS 0
#endif
#define LUMEX_HAS_STD_INCOMPLETE_CONTAINER_ELEMENTS                           \
  (LUMEX_FEATURE_STD_INCOMPLETE_CONTAINER_ELEMENTS >= 201505L)

/// `__cpp_lib_indirect` (C++26).
#if defined(__cpp_lib_indirect)
#define LUMEX_FEATURE_STD_INDIRECT __cpp_lib_indirect
#else
#define LUMEX_FEATURE_STD_INDIRECT 0
#endif
#define LUMEX_HAS_STD_INDIRECT (LUMEX_FEATURE_STD_INDIRECT >= 202502L)

/// `__cpp_lib_initializer_list` (C++26).
#if defined(__cpp_lib_initializer_list)
#define LUMEX_FEATURE_STD_INITIALIZER_LIST __cpp_lib_initializer_list
#else
#define LUMEX_FEATURE_STD_INITIALIZER_LIST 0
#endif
#define LUMEX_HAS_STD_INITIALIZER_LIST                                        \
  (LUMEX_FEATURE_STD_INITIALIZER_LIST >= 202511L)

/// `__cpp_lib_inplace_vector` (C++26; later: 202603L C++26).
#if defined(__cpp_lib_inplace_vector)
#define LUMEX_FEATURE_STD_INPLACE_VECTOR __cpp_lib_inplace_vector
#else
#define LUMEX_FEATURE_STD_INPLACE_VECTOR 0
#endif
#define LUMEX_HAS_STD_INPLACE_VECTOR                                          \
  (LUMEX_FEATURE_STD_INPLACE_VECTOR >= 202406L)

/// `__cpp_lib_int_pow2` (C++20).
#if defined(__cpp_lib_int_pow2)
#define LUMEX_FEATURE_STD_INT_POW2 __cpp_lib_int_pow2
#else
#define LUMEX_FEATURE_STD_INT_POW2 0
#endif
#define LUMEX_HAS_STD_INT_POW2 (LUMEX_FEATURE_STD_INT_POW2 >= 202002L)

/// `__cpp_lib_integer_comparison_functions` (C++20).
#if defined(__cpp_lib_integer_comparison_functions)
#define LUMEX_FEATURE_STD_INTEGER_COMPARISON_FUNCTIONS                        \
  __cpp_lib_integer_comparison_functions
#else
#define LUMEX_FEATURE_STD_INTEGER_COMPARISON_FUNCTIONS 0
#endif
#define LUMEX_HAS_STD_INTEGER_COMPARISON_FUNCTIONS                            \
  (LUMEX_FEATURE_STD_INTEGER_COMPARISON_FUNCTIONS >= 202002L)

/// `__cpp_lib_integer_sequence` (C++14; later: 202511L C++26).
#if defined(__cpp_lib_integer_sequence)
#define LUMEX_FEATURE_STD_INTEGER_SEQUENCE __cpp_lib_integer_sequence
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201402L
#define LUMEX_FEATURE_STD_INTEGER_SEQUENCE 201304L
#else
#define LUMEX_FEATURE_STD_INTEGER_SEQUENCE 0
#endif
#define LUMEX_HAS_STD_INTEGER_SEQUENCE                                        \
  (LUMEX_FEATURE_STD_INTEGER_SEQUENCE >= 201304L)

/// `__cpp_lib_integral_constant_callable` (C++14).
#if defined(__cpp_lib_integral_constant_callable)
#define LUMEX_FEATURE_STD_INTEGRAL_CONSTANT_CALLABLE                          \
  __cpp_lib_integral_constant_callable
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201402L
#define LUMEX_FEATURE_STD_INTEGRAL_CONSTANT_CALLABLE 201304L
#else
#define LUMEX_FEATURE_STD_INTEGRAL_CONSTANT_CALLABLE 0
#endif
#define LUMEX_HAS_STD_INTEGRAL_CONSTANT_CALLABLE                              \
  (LUMEX_FEATURE_STD_INTEGRAL_CONSTANT_CALLABLE >= 201304L)

/// `__cpp_lib_interpolate` (C++20).
#if defined(__cpp_lib_interpolate)
#define LUMEX_FEATURE_STD_INTERPOLATE __cpp_lib_interpolate
#else
#define LUMEX_FEATURE_STD_INTERPOLATE 0
#endif
#define LUMEX_HAS_STD_INTERPOLATE (LUMEX_FEATURE_STD_INTERPOLATE >= 201902L)

/// `__cpp_lib_invoke` (C++17).
#if defined(__cpp_lib_invoke)
#define LUMEX_FEATURE_STD_INVOKE __cpp_lib_invoke
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_INVOKE 201411L
#else
#define LUMEX_FEATURE_STD_INVOKE 0
#endif
#define LUMEX_HAS_STD_INVOKE (LUMEX_FEATURE_STD_INVOKE >= 201411L)

/// `__cpp_lib_invoke_r` (C++23).
#if defined(__cpp_lib_invoke_r)
#define LUMEX_FEATURE_STD_INVOKE_R __cpp_lib_invoke_r
#else
#define LUMEX_FEATURE_STD_INVOKE_R 0
#endif
#define LUMEX_HAS_STD_INVOKE_R (LUMEX_FEATURE_STD_INVOKE_R >= 202106L)

/// `__cpp_lib_ios_noreplace` (C++23).
#if defined(__cpp_lib_ios_noreplace)
#define LUMEX_FEATURE_STD_IOS_NOREPLACE __cpp_lib_ios_noreplace
#else
#define LUMEX_FEATURE_STD_IOS_NOREPLACE 0
#endif
#define LUMEX_HAS_STD_IOS_NOREPLACE                                           \
  (LUMEX_FEATURE_STD_IOS_NOREPLACE >= 202207L)

/// `__cpp_lib_is_aggregate` (C++17).
#if defined(__cpp_lib_is_aggregate)
#define LUMEX_FEATURE_STD_IS_AGGREGATE __cpp_lib_is_aggregate
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_IS_AGGREGATE 201703L
#else
#define LUMEX_FEATURE_STD_IS_AGGREGATE 0
#endif
#define LUMEX_HAS_STD_IS_AGGREGATE (LUMEX_FEATURE_STD_IS_AGGREGATE >= 201703L)

/// `__cpp_lib_is_constant_evaluated` (C++20).
#if defined(__cpp_lib_is_constant_evaluated)
#define LUMEX_FEATURE_STD_IS_CONSTANT_EVALUATED __cpp_lib_is_constant_evaluated
#else
#define LUMEX_FEATURE_STD_IS_CONSTANT_EVALUATED 0
#endif
#define LUMEX_HAS_STD_IS_CONSTANT_EVALUATED                                   \
  (LUMEX_FEATURE_STD_IS_CONSTANT_EVALUATED >= 201811L)

/// `__cpp_lib_is_final` (C++14).
#if defined(__cpp_lib_is_final)
#define LUMEX_FEATURE_STD_IS_FINAL __cpp_lib_is_final
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201402L
#define LUMEX_FEATURE_STD_IS_FINAL 201402L
#else
#define LUMEX_FEATURE_STD_IS_FINAL 0
#endif
#define LUMEX_HAS_STD_IS_FINAL (LUMEX_FEATURE_STD_IS_FINAL >= 201402L)

/// `__cpp_lib_is_implicit_lifetime` (C++23).
#if defined(__cpp_lib_is_implicit_lifetime)
#define LUMEX_FEATURE_STD_IS_IMPLICIT_LIFETIME __cpp_lib_is_implicit_lifetime
#else
#define LUMEX_FEATURE_STD_IS_IMPLICIT_LIFETIME 0
#endif
#define LUMEX_HAS_STD_IS_IMPLICIT_LIFETIME                                    \
  (LUMEX_FEATURE_STD_IS_IMPLICIT_LIFETIME >= 202302L)

/// `__cpp_lib_is_invocable` (C++17).
#if defined(__cpp_lib_is_invocable)
#define LUMEX_FEATURE_STD_IS_INVOCABLE __cpp_lib_is_invocable
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_IS_INVOCABLE 201703L
#else
#define LUMEX_FEATURE_STD_IS_INVOCABLE 0
#endif
#define LUMEX_HAS_STD_IS_INVOCABLE (LUMEX_FEATURE_STD_IS_INVOCABLE >= 201703L)

/// `__cpp_lib_is_layout_compatible` (C++20).
#if defined(__cpp_lib_is_layout_compatible)
#define LUMEX_FEATURE_STD_IS_LAYOUT_COMPATIBLE __cpp_lib_is_layout_compatible
#else
#define LUMEX_FEATURE_STD_IS_LAYOUT_COMPATIBLE 0
#endif
#define LUMEX_HAS_STD_IS_LAYOUT_COMPATIBLE                                    \
  (LUMEX_FEATURE_STD_IS_LAYOUT_COMPATIBLE >= 201907L)

/// `__cpp_lib_is_nothrow_convertible` (C++20).
#if defined(__cpp_lib_is_nothrow_convertible)
#define LUMEX_FEATURE_STD_IS_NOTHROW_CONVERTIBLE                              \
  __cpp_lib_is_nothrow_convertible
#else
#define LUMEX_FEATURE_STD_IS_NOTHROW_CONVERTIBLE 0
#endif
#define LUMEX_HAS_STD_IS_NOTHROW_CONVERTIBLE                                  \
  (LUMEX_FEATURE_STD_IS_NOTHROW_CONVERTIBLE >= 201806L)

/// `__cpp_lib_is_null_pointer` (C++14).
#if defined(__cpp_lib_is_null_pointer)
#define LUMEX_FEATURE_STD_IS_NULL_POINTER __cpp_lib_is_null_pointer
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201402L
#define LUMEX_FEATURE_STD_IS_NULL_POINTER 201309L
#else
#define LUMEX_FEATURE_STD_IS_NULL_POINTER 0
#endif
#define LUMEX_HAS_STD_IS_NULL_POINTER                                         \
  (LUMEX_FEATURE_STD_IS_NULL_POINTER >= 201309L)

/// `__cpp_lib_is_pointer_interconvertible` (C++20).
#if defined(__cpp_lib_is_pointer_interconvertible)
#define LUMEX_FEATURE_STD_IS_POINTER_INTERCONVERTIBLE                         \
  __cpp_lib_is_pointer_interconvertible
#else
#define LUMEX_FEATURE_STD_IS_POINTER_INTERCONVERTIBLE 0
#endif
#define LUMEX_HAS_STD_IS_POINTER_INTERCONVERTIBLE                             \
  (LUMEX_FEATURE_STD_IS_POINTER_INTERCONVERTIBLE >= 201907L)

/// `__cpp_lib_is_scoped_enum` (C++23).
#if defined(__cpp_lib_is_scoped_enum)
#define LUMEX_FEATURE_STD_IS_SCOPED_ENUM __cpp_lib_is_scoped_enum
#else
#define LUMEX_FEATURE_STD_IS_SCOPED_ENUM 0
#endif
#define LUMEX_HAS_STD_IS_SCOPED_ENUM                                          \
  (LUMEX_FEATURE_STD_IS_SCOPED_ENUM >= 202011L)

/// `__cpp_lib_is_structural` (C++26).
#if defined(__cpp_lib_is_structural)
#define LUMEX_FEATURE_STD_IS_STRUCTURAL __cpp_lib_is_structural
#else
#define LUMEX_FEATURE_STD_IS_STRUCTURAL 0
#endif
#define LUMEX_HAS_STD_IS_STRUCTURAL                                           \
  (LUMEX_FEATURE_STD_IS_STRUCTURAL >= 202603L)

/// `__cpp_lib_is_sufficiently_aligned` (C++26).
#if defined(__cpp_lib_is_sufficiently_aligned)
#define LUMEX_FEATURE_STD_IS_SUFFICIENTLY_ALIGNED                             \
  __cpp_lib_is_sufficiently_aligned
#else
#define LUMEX_FEATURE_STD_IS_SUFFICIENTLY_ALIGNED 0
#endif
#define LUMEX_HAS_STD_IS_SUFFICIENTLY_ALIGNED                                 \
  (LUMEX_FEATURE_STD_IS_SUFFICIENTLY_ALIGNED >= 202411L)

/// `__cpp_lib_is_swappable` (C++17).
#if defined(__cpp_lib_is_swappable)
#define LUMEX_FEATURE_STD_IS_SWAPPABLE __cpp_lib_is_swappable
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_IS_SWAPPABLE 201603L
#else
#define LUMEX_FEATURE_STD_IS_SWAPPABLE 0
#endif
#define LUMEX_HAS_STD_IS_SWAPPABLE (LUMEX_FEATURE_STD_IS_SWAPPABLE >= 201603L)

/// `__cpp_lib_is_virtual_base_of` (C++26).
#if defined(__cpp_lib_is_virtual_base_of)
#define LUMEX_FEATURE_STD_IS_VIRTUAL_BASE_OF __cpp_lib_is_virtual_base_of
#else
#define LUMEX_FEATURE_STD_IS_VIRTUAL_BASE_OF 0
#endif
#define LUMEX_HAS_STD_IS_VIRTUAL_BASE_OF                                      \
  (LUMEX_FEATURE_STD_IS_VIRTUAL_BASE_OF >= 202406L)

/// `__cpp_lib_is_within_lifetime` (C++26; later: 202603L C++26).
#if defined(__cpp_lib_is_within_lifetime)
#define LUMEX_FEATURE_STD_IS_WITHIN_LIFETIME __cpp_lib_is_within_lifetime
#else
#define LUMEX_FEATURE_STD_IS_WITHIN_LIFETIME 0
#endif
#define LUMEX_HAS_STD_IS_WITHIN_LIFETIME                                      \
  (LUMEX_FEATURE_STD_IS_WITHIN_LIFETIME >= 202306L)

/// `__cpp_lib_jthread` (C++20).
#if defined(__cpp_lib_jthread)
#define LUMEX_FEATURE_STD_JTHREAD __cpp_lib_jthread
#else
#define LUMEX_FEATURE_STD_JTHREAD 0
#endif
#define LUMEX_HAS_STD_JTHREAD (LUMEX_FEATURE_STD_JTHREAD >= 201911L)

/// `__cpp_lib_latch` (C++20).
#if defined(__cpp_lib_latch)
#define LUMEX_FEATURE_STD_LATCH __cpp_lib_latch
#else
#define LUMEX_FEATURE_STD_LATCH 0
#endif
#define LUMEX_HAS_STD_LATCH (LUMEX_FEATURE_STD_LATCH >= 201907L)

/// `__cpp_lib_launder` (C++17).
#if defined(__cpp_lib_launder)
#define LUMEX_FEATURE_STD_LAUNDER __cpp_lib_launder
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_LAUNDER 201606L
#else
#define LUMEX_FEATURE_STD_LAUNDER 0
#endif
#define LUMEX_HAS_STD_LAUNDER (LUMEX_FEATURE_STD_LAUNDER >= 201606L)

/// `__cpp_lib_linalg` (C++26).
#if defined(__cpp_lib_linalg)
#define LUMEX_FEATURE_STD_LINALG __cpp_lib_linalg
#else
#define LUMEX_FEATURE_STD_LINALG 0
#endif
#define LUMEX_HAS_STD_LINALG (LUMEX_FEATURE_STD_LINALG >= 202311L)

/// `__cpp_lib_list_remove_return_type` (C++20).
#if defined(__cpp_lib_list_remove_return_type)
#define LUMEX_FEATURE_STD_LIST_REMOVE_RETURN_TYPE                             \
  __cpp_lib_list_remove_return_type
#else
#define LUMEX_FEATURE_STD_LIST_REMOVE_RETURN_TYPE 0
#endif
#define LUMEX_HAS_STD_LIST_REMOVE_RETURN_TYPE                                 \
  (LUMEX_FEATURE_STD_LIST_REMOVE_RETURN_TYPE >= 201806L)

/// `__cpp_lib_logical_traits` (C++17).
#if defined(__cpp_lib_logical_traits)
#define LUMEX_FEATURE_STD_LOGICAL_TRAITS __cpp_lib_logical_traits
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_LOGICAL_TRAITS 201510L
#else
#define LUMEX_FEATURE_STD_LOGICAL_TRAITS 0
#endif
#define LUMEX_HAS_STD_LOGICAL_TRAITS                                          \
  (LUMEX_FEATURE_STD_LOGICAL_TRAITS >= 201510L)

/// `__cpp_lib_make_from_tuple` (C++17).
#if defined(__cpp_lib_make_from_tuple)
#define LUMEX_FEATURE_STD_MAKE_FROM_TUPLE __cpp_lib_make_from_tuple
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_MAKE_FROM_TUPLE 201606L
#else
#define LUMEX_FEATURE_STD_MAKE_FROM_TUPLE 0
#endif
#define LUMEX_HAS_STD_MAKE_FROM_TUPLE                                         \
  (LUMEX_FEATURE_STD_MAKE_FROM_TUPLE >= 201606L)

/// `__cpp_lib_make_reverse_iterator` (C++14).
#if defined(__cpp_lib_make_reverse_iterator)
#define LUMEX_FEATURE_STD_MAKE_REVERSE_ITERATOR __cpp_lib_make_reverse_iterator
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201402L
#define LUMEX_FEATURE_STD_MAKE_REVERSE_ITERATOR 201402L
#else
#define LUMEX_FEATURE_STD_MAKE_REVERSE_ITERATOR 0
#endif
#define LUMEX_HAS_STD_MAKE_REVERSE_ITERATOR                                   \
  (LUMEX_FEATURE_STD_MAKE_REVERSE_ITERATOR >= 201402L)

/// `__cpp_lib_make_unique` (C++14).
#if defined(__cpp_lib_make_unique)
#define LUMEX_FEATURE_STD_MAKE_UNIQUE __cpp_lib_make_unique
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201402L
#define LUMEX_FEATURE_STD_MAKE_UNIQUE 201304L
#else
#define LUMEX_FEATURE_STD_MAKE_UNIQUE 0
#endif
#define LUMEX_HAS_STD_MAKE_UNIQUE (LUMEX_FEATURE_STD_MAKE_UNIQUE >= 201304L)

/// `__cpp_lib_map_lookup` (C++29).
#if defined(__cpp_lib_map_lookup)
#define LUMEX_FEATURE_STD_MAP_LOOKUP __cpp_lib_map_lookup
#else
#define LUMEX_FEATURE_STD_MAP_LOOKUP 0
#endif
#define LUMEX_HAS_STD_MAP_LOOKUP (LUMEX_FEATURE_STD_MAP_LOOKUP >= 202606L)

/// `__cpp_lib_map_try_emplace` (C++17).
#if defined(__cpp_lib_map_try_emplace)
#define LUMEX_FEATURE_STD_MAP_TRY_EMPLACE __cpp_lib_map_try_emplace
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_MAP_TRY_EMPLACE 201411L
#else
#define LUMEX_FEATURE_STD_MAP_TRY_EMPLACE 0
#endif
#define LUMEX_HAS_STD_MAP_TRY_EMPLACE                                         \
  (LUMEX_FEATURE_STD_MAP_TRY_EMPLACE >= 201411L)

/// `__cpp_lib_math_constants` (C++20).
#if defined(__cpp_lib_math_constants)
#define LUMEX_FEATURE_STD_MATH_CONSTANTS __cpp_lib_math_constants
#else
#define LUMEX_FEATURE_STD_MATH_CONSTANTS 0
#endif
#define LUMEX_HAS_STD_MATH_CONSTANTS                                          \
  (LUMEX_FEATURE_STD_MATH_CONSTANTS >= 201907L)

/// `__cpp_lib_math_special_functions` (C++17).
#if defined(__cpp_lib_math_special_functions)
#define LUMEX_FEATURE_STD_MATH_SPECIAL_FUNCTIONS                              \
  __cpp_lib_math_special_functions
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_MATH_SPECIAL_FUNCTIONS 201603L
#else
#define LUMEX_FEATURE_STD_MATH_SPECIAL_FUNCTIONS 0
#endif
#define LUMEX_HAS_STD_MATH_SPECIAL_FUNCTIONS                                  \
  (LUMEX_FEATURE_STD_MATH_SPECIAL_FUNCTIONS >= 201603L)

/// `__cpp_lib_mdspan` (C++23; later: 202406L C++26).
#if defined(__cpp_lib_mdspan)
#define LUMEX_FEATURE_STD_MDSPAN __cpp_lib_mdspan
#else
#define LUMEX_FEATURE_STD_MDSPAN 0
#endif
#define LUMEX_HAS_STD_MDSPAN (LUMEX_FEATURE_STD_MDSPAN >= 202207L)

/// `__cpp_lib_mdspan_copy` (C++29).
#if defined(__cpp_lib_mdspan_copy)
#define LUMEX_FEATURE_STD_MDSPAN_COPY __cpp_lib_mdspan_copy
#else
#define LUMEX_FEATURE_STD_MDSPAN_COPY 0
#endif
#define LUMEX_HAS_STD_MDSPAN_COPY (LUMEX_FEATURE_STD_MDSPAN_COPY >= 202606L)

/// `__cpp_lib_memory_resource` (C++17).
#if defined(__cpp_lib_memory_resource)
#define LUMEX_FEATURE_STD_MEMORY_RESOURCE __cpp_lib_memory_resource
#else
#define LUMEX_FEATURE_STD_MEMORY_RESOURCE 0
#endif
#define LUMEX_HAS_STD_MEMORY_RESOURCE                                         \
  (LUMEX_FEATURE_STD_MEMORY_RESOURCE >= 201603L)

/// `__cpp_lib_modules` (C++23).
#if defined(__cpp_lib_modules)
#define LUMEX_FEATURE_STD_MODULES __cpp_lib_modules
#else
#define LUMEX_FEATURE_STD_MODULES 0
#endif
#define LUMEX_HAS_STD_MODULES (LUMEX_FEATURE_STD_MODULES >= 202207L)

/// `__cpp_lib_move_iterator_concept` (C++23).
#if defined(__cpp_lib_move_iterator_concept)
#define LUMEX_FEATURE_STD_MOVE_ITERATOR_CONCEPT __cpp_lib_move_iterator_concept
#else
#define LUMEX_FEATURE_STD_MOVE_ITERATOR_CONCEPT 0
#endif
#define LUMEX_HAS_STD_MOVE_ITERATOR_CONCEPT                                   \
  (LUMEX_FEATURE_STD_MOVE_ITERATOR_CONCEPT >= 202207L)

/// `__cpp_lib_move_only_function` (C++23).
#if defined(__cpp_lib_move_only_function)
#define LUMEX_FEATURE_STD_MOVE_ONLY_FUNCTION __cpp_lib_move_only_function
#else
#define LUMEX_FEATURE_STD_MOVE_ONLY_FUNCTION 0
#endif
#define LUMEX_HAS_STD_MOVE_ONLY_FUNCTION                                      \
  (LUMEX_FEATURE_STD_MOVE_ONLY_FUNCTION >= 202110L)

/// `__cpp_lib_node_extract` (C++17).
#if defined(__cpp_lib_node_extract)
#define LUMEX_FEATURE_STD_NODE_EXTRACT __cpp_lib_node_extract
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_NODE_EXTRACT 201606L
#else
#define LUMEX_FEATURE_STD_NODE_EXTRACT 0
#endif
#define LUMEX_HAS_STD_NODE_EXTRACT (LUMEX_FEATURE_STD_NODE_EXTRACT >= 201606L)

/// `__cpp_lib_nonmember_container_access` (C++17).
#if defined(__cpp_lib_nonmember_container_access)
#define LUMEX_FEATURE_STD_NONMEMBER_CONTAINER_ACCESS                          \
  __cpp_lib_nonmember_container_access
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_NONMEMBER_CONTAINER_ACCESS 201411L
#else
#define LUMEX_FEATURE_STD_NONMEMBER_CONTAINER_ACCESS 0
#endif
#define LUMEX_HAS_STD_NONMEMBER_CONTAINER_ACCESS                              \
  (LUMEX_FEATURE_STD_NONMEMBER_CONTAINER_ACCESS >= 201411L)

/// `__cpp_lib_not_fn` (C++17; later: 202306L C++26).
#if defined(__cpp_lib_not_fn)
#define LUMEX_FEATURE_STD_NOT_FN __cpp_lib_not_fn
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_NOT_FN 201603L
#else
#define LUMEX_FEATURE_STD_NOT_FN 0
#endif
#define LUMEX_HAS_STD_NOT_FN (LUMEX_FEATURE_STD_NOT_FN >= 201603L)

/// `__cpp_lib_null_iterators` (C++14).
#if defined(__cpp_lib_null_iterators)
#define LUMEX_FEATURE_STD_NULL_ITERATORS __cpp_lib_null_iterators
#else
#define LUMEX_FEATURE_STD_NULL_ITERATORS 0
#endif
#define LUMEX_HAS_STD_NULL_ITERATORS                                          \
  (LUMEX_FEATURE_STD_NULL_ITERATORS >= 201304L)

/// `__cpp_lib_observable_checkpoint` (C++26).
#if defined(__cpp_lib_observable_checkpoint)
#define LUMEX_FEATURE_STD_OBSERVABLE_CHECKPOINT __cpp_lib_observable_checkpoint
#else
#define LUMEX_FEATURE_STD_OBSERVABLE_CHECKPOINT 0
#endif
#define LUMEX_HAS_STD_OBSERVABLE_CHECKPOINT                                   \
  (LUMEX_FEATURE_STD_OBSERVABLE_CHECKPOINT >= 202506L)

/// `__cpp_lib_optional` (C++17; later: 202106L C++23, 202110L C++23, 202506L
/// C++26).
#if defined(__cpp_lib_optional)
#define LUMEX_FEATURE_STD_OPTIONAL __cpp_lib_optional
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_OPTIONAL 201606L
#else
#define LUMEX_FEATURE_STD_OPTIONAL 0
#endif
#define LUMEX_HAS_STD_OPTIONAL (LUMEX_FEATURE_STD_OPTIONAL >= 201606L)

/// `__cpp_lib_optional_range_support` (C++26).
#if defined(__cpp_lib_optional_range_support)
#define LUMEX_FEATURE_STD_OPTIONAL_RANGE_SUPPORT                              \
  __cpp_lib_optional_range_support
#else
#define LUMEX_FEATURE_STD_OPTIONAL_RANGE_SUPPORT 0
#endif
#define LUMEX_HAS_STD_OPTIONAL_RANGE_SUPPORT                                  \
  (LUMEX_FEATURE_STD_OPTIONAL_RANGE_SUPPORT >= 202406L)

/// `__cpp_lib_out_ptr` (C++23; later: 202311L C++26).
#if defined(__cpp_lib_out_ptr)
#define LUMEX_FEATURE_STD_OUT_PTR __cpp_lib_out_ptr
#else
#define LUMEX_FEATURE_STD_OUT_PTR 0
#endif
#define LUMEX_HAS_STD_OUT_PTR (LUMEX_FEATURE_STD_OUT_PTR >= 202106L)

/// `__cpp_lib_parallel_algorithm` (C++17; later: 202506L C++26).
#if defined(__cpp_lib_parallel_algorithm)
#define LUMEX_FEATURE_STD_PARALLEL_ALGORITHM __cpp_lib_parallel_algorithm
#else
#define LUMEX_FEATURE_STD_PARALLEL_ALGORITHM 0
#endif
#define LUMEX_HAS_STD_PARALLEL_ALGORITHM                                      \
  (LUMEX_FEATURE_STD_PARALLEL_ALGORITHM >= 201603L)

/// `__cpp_lib_parallel_scheduler` (C++26).
#if defined(__cpp_lib_parallel_scheduler)
#define LUMEX_FEATURE_STD_PARALLEL_SCHEDULER __cpp_lib_parallel_scheduler
#else
#define LUMEX_FEATURE_STD_PARALLEL_SCHEDULER 0
#endif
#define LUMEX_HAS_STD_PARALLEL_SCHEDULER                                      \
  (LUMEX_FEATURE_STD_PARALLEL_SCHEDULER >= 202506L)

/// `__cpp_lib_philox_engine` (C++26).
#if defined(__cpp_lib_philox_engine)
#define LUMEX_FEATURE_STD_PHILOX_ENGINE __cpp_lib_philox_engine
#else
#define LUMEX_FEATURE_STD_PHILOX_ENGINE 0
#endif
#define LUMEX_HAS_STD_PHILOX_ENGINE                                           \
  (LUMEX_FEATURE_STD_PHILOX_ENGINE >= 202406L)

/// `__cpp_lib_pointer_tag_pair` (C++29).
#if defined(__cpp_lib_pointer_tag_pair)
#define LUMEX_FEATURE_STD_POINTER_TAG_PAIR __cpp_lib_pointer_tag_pair
#else
#define LUMEX_FEATURE_STD_POINTER_TAG_PAIR 0
#endif
#define LUMEX_HAS_STD_POINTER_TAG_PAIR                                        \
  (LUMEX_FEATURE_STD_POINTER_TAG_PAIR >= 202606L)

/// `__cpp_lib_polymorphic` (C++26).
#if defined(__cpp_lib_polymorphic)
#define LUMEX_FEATURE_STD_POLYMORPHIC __cpp_lib_polymorphic
#else
#define LUMEX_FEATURE_STD_POLYMORPHIC 0
#endif
#define LUMEX_HAS_STD_POLYMORPHIC (LUMEX_FEATURE_STD_POLYMORPHIC >= 202502L)

/// `__cpp_lib_polymorphic_allocator` (C++20).
#if defined(__cpp_lib_polymorphic_allocator)
#define LUMEX_FEATURE_STD_POLYMORPHIC_ALLOCATOR __cpp_lib_polymorphic_allocator
#else
#define LUMEX_FEATURE_STD_POLYMORPHIC_ALLOCATOR 0
#endif
#define LUMEX_HAS_STD_POLYMORPHIC_ALLOCATOR                                   \
  (LUMEX_FEATURE_STD_POLYMORPHIC_ALLOCATOR >= 201902L)

/// `__cpp_lib_print` (C++23; later: 202403L C++26, 202406L C++26).
#if defined(__cpp_lib_print)
#define LUMEX_FEATURE_STD_PRINT __cpp_lib_print
#else
#define LUMEX_FEATURE_STD_PRINT 0
#endif
#define LUMEX_HAS_STD_PRINT (LUMEX_FEATURE_STD_PRINT >= 202207L)

/// `__cpp_lib_quoted_string_io` (C++14).
#if defined(__cpp_lib_quoted_string_io)
#define LUMEX_FEATURE_STD_QUOTED_STRING_IO __cpp_lib_quoted_string_io
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201402L
#define LUMEX_FEATURE_STD_QUOTED_STRING_IO 201304L
#else
#define LUMEX_FEATURE_STD_QUOTED_STRING_IO 0
#endif
#define LUMEX_HAS_STD_QUOTED_STRING_IO                                        \
  (LUMEX_FEATURE_STD_QUOTED_STRING_IO >= 201304L)

/// `__cpp_lib_ranges` (C++20; later: 202106L C++23, 202110L C++23, 202202L
/// C++23, 202207L C++23, 202211L C++23, 202302L C++23, 202406L C++26).
#if defined(__cpp_lib_ranges)
#define LUMEX_FEATURE_STD_RANGES __cpp_lib_ranges
#else
#define LUMEX_FEATURE_STD_RANGES 0
#endif
#define LUMEX_HAS_STD_RANGES (LUMEX_FEATURE_STD_RANGES >= 201911L)

/// `__cpp_lib_ranges_as_const` (C++23; later: 202311L C++26).
#if defined(__cpp_lib_ranges_as_const)
#define LUMEX_FEATURE_STD_RANGES_AS_CONST __cpp_lib_ranges_as_const
#else
#define LUMEX_FEATURE_STD_RANGES_AS_CONST 0
#endif
#define LUMEX_HAS_STD_RANGES_AS_CONST                                         \
  (LUMEX_FEATURE_STD_RANGES_AS_CONST >= 202207L)

/// `__cpp_lib_ranges_as_input` (C++26).
#if defined(__cpp_lib_ranges_as_input)
#define LUMEX_FEATURE_STD_RANGES_AS_INPUT __cpp_lib_ranges_as_input
#else
#define LUMEX_FEATURE_STD_RANGES_AS_INPUT 0
#endif
#define LUMEX_HAS_STD_RANGES_AS_INPUT                                         \
  (LUMEX_FEATURE_STD_RANGES_AS_INPUT >= 202502L)

/// `__cpp_lib_ranges_as_rvalue` (C++23).
#if defined(__cpp_lib_ranges_as_rvalue)
#define LUMEX_FEATURE_STD_RANGES_AS_RVALUE __cpp_lib_ranges_as_rvalue
#else
#define LUMEX_FEATURE_STD_RANGES_AS_RVALUE 0
#endif
#define LUMEX_HAS_STD_RANGES_AS_RVALUE                                        \
  (LUMEX_FEATURE_STD_RANGES_AS_RVALUE >= 202207L)

/// `__cpp_lib_ranges_cache_latest` (C++26).
#if defined(__cpp_lib_ranges_cache_latest)
#define LUMEX_FEATURE_STD_RANGES_CACHE_LATEST __cpp_lib_ranges_cache_latest
#else
#define LUMEX_FEATURE_STD_RANGES_CACHE_LATEST 0
#endif
#define LUMEX_HAS_STD_RANGES_CACHE_LATEST                                     \
  (LUMEX_FEATURE_STD_RANGES_CACHE_LATEST >= 202411L)

/// `__cpp_lib_ranges_cartesian_product` (C++23).
#if defined(__cpp_lib_ranges_cartesian_product)
#define LUMEX_FEATURE_STD_RANGES_CARTESIAN_PRODUCT                            \
  __cpp_lib_ranges_cartesian_product
#else
#define LUMEX_FEATURE_STD_RANGES_CARTESIAN_PRODUCT 0
#endif
#define LUMEX_HAS_STD_RANGES_CARTESIAN_PRODUCT                                \
  (LUMEX_FEATURE_STD_RANGES_CARTESIAN_PRODUCT >= 202207L)

/// `__cpp_lib_ranges_chunk` (C++23).
#if defined(__cpp_lib_ranges_chunk)
#define LUMEX_FEATURE_STD_RANGES_CHUNK __cpp_lib_ranges_chunk
#else
#define LUMEX_FEATURE_STD_RANGES_CHUNK 0
#endif
#define LUMEX_HAS_STD_RANGES_CHUNK (LUMEX_FEATURE_STD_RANGES_CHUNK >= 202202L)

/// `__cpp_lib_ranges_chunk_by` (C++23).
#if defined(__cpp_lib_ranges_chunk_by)
#define LUMEX_FEATURE_STD_RANGES_CHUNK_BY __cpp_lib_ranges_chunk_by
#else
#define LUMEX_FEATURE_STD_RANGES_CHUNK_BY 0
#endif
#define LUMEX_HAS_STD_RANGES_CHUNK_BY                                         \
  (LUMEX_FEATURE_STD_RANGES_CHUNK_BY >= 202202L)

/// `__cpp_lib_ranges_concat` (C++26).
#if defined(__cpp_lib_ranges_concat)
#define LUMEX_FEATURE_STD_RANGES_CONCAT __cpp_lib_ranges_concat
#else
#define LUMEX_FEATURE_STD_RANGES_CONCAT 0
#endif
#define LUMEX_HAS_STD_RANGES_CONCAT                                           \
  (LUMEX_FEATURE_STD_RANGES_CONCAT >= 202403L)

/// `__cpp_lib_ranges_contains` (C++23).
#if defined(__cpp_lib_ranges_contains)
#define LUMEX_FEATURE_STD_RANGES_CONTAINS __cpp_lib_ranges_contains
#else
#define LUMEX_FEATURE_STD_RANGES_CONTAINS 0
#endif
#define LUMEX_HAS_STD_RANGES_CONTAINS                                         \
  (LUMEX_FEATURE_STD_RANGES_CONTAINS >= 202207L)

/// `__cpp_lib_ranges_enumerate` (C++23).
#if defined(__cpp_lib_ranges_enumerate)
#define LUMEX_FEATURE_STD_RANGES_ENUMERATE __cpp_lib_ranges_enumerate
#else
#define LUMEX_FEATURE_STD_RANGES_ENUMERATE 0
#endif
#define LUMEX_HAS_STD_RANGES_ENUMERATE                                        \
  (LUMEX_FEATURE_STD_RANGES_ENUMERATE >= 202302L)

/// `__cpp_lib_ranges_filter` (C++26).
#if defined(__cpp_lib_ranges_filter)
#define LUMEX_FEATURE_STD_RANGES_FILTER __cpp_lib_ranges_filter
#else
#define LUMEX_FEATURE_STD_RANGES_FILTER 0
#endif
#define LUMEX_HAS_STD_RANGES_FILTER                                           \
  (LUMEX_FEATURE_STD_RANGES_FILTER >= 202603L)

/// `__cpp_lib_ranges_find_last` (C++23).
#if defined(__cpp_lib_ranges_find_last)
#define LUMEX_FEATURE_STD_RANGES_FIND_LAST __cpp_lib_ranges_find_last
#else
#define LUMEX_FEATURE_STD_RANGES_FIND_LAST 0
#endif
#define LUMEX_HAS_STD_RANGES_FIND_LAST                                        \
  (LUMEX_FEATURE_STD_RANGES_FIND_LAST >= 202207L)

/// `__cpp_lib_ranges_fold` (C++23).
#if defined(__cpp_lib_ranges_fold)
#define LUMEX_FEATURE_STD_RANGES_FOLD __cpp_lib_ranges_fold
#else
#define LUMEX_FEATURE_STD_RANGES_FOLD 0
#endif
#define LUMEX_HAS_STD_RANGES_FOLD (LUMEX_FEATURE_STD_RANGES_FOLD >= 202207L)

/// `__cpp_lib_ranges_generate_random` (C++26).
#if defined(__cpp_lib_ranges_generate_random)
#define LUMEX_FEATURE_STD_RANGES_GENERATE_RANDOM                              \
  __cpp_lib_ranges_generate_random
#else
#define LUMEX_FEATURE_STD_RANGES_GENERATE_RANDOM 0
#endif
#define LUMEX_HAS_STD_RANGES_GENERATE_RANDOM                                  \
  (LUMEX_FEATURE_STD_RANGES_GENERATE_RANDOM >= 202403L)

/// `__cpp_lib_ranges_indices` (C++26).
#if defined(__cpp_lib_ranges_indices)
#define LUMEX_FEATURE_STD_RANGES_INDICES __cpp_lib_ranges_indices
#else
#define LUMEX_FEATURE_STD_RANGES_INDICES 0
#endif
#define LUMEX_HAS_STD_RANGES_INDICES                                          \
  (LUMEX_FEATURE_STD_RANGES_INDICES >= 202506L)

/// `__cpp_lib_ranges_iota` (C++23).
#if defined(__cpp_lib_ranges_iota)
#define LUMEX_FEATURE_STD_RANGES_IOTA __cpp_lib_ranges_iota
#else
#define LUMEX_FEATURE_STD_RANGES_IOTA 0
#endif
#define LUMEX_HAS_STD_RANGES_IOTA (LUMEX_FEATURE_STD_RANGES_IOTA >= 202202L)

/// `__cpp_lib_ranges_join_with` (C++23).
#if defined(__cpp_lib_ranges_join_with)
#define LUMEX_FEATURE_STD_RANGES_JOIN_WITH __cpp_lib_ranges_join_with
#else
#define LUMEX_FEATURE_STD_RANGES_JOIN_WITH 0
#endif
#define LUMEX_HAS_STD_RANGES_JOIN_WITH                                        \
  (LUMEX_FEATURE_STD_RANGES_JOIN_WITH >= 202202L)

/// `__cpp_lib_ranges_repeat` (C++23).
#if defined(__cpp_lib_ranges_repeat)
#define LUMEX_FEATURE_STD_RANGES_REPEAT __cpp_lib_ranges_repeat
#else
#define LUMEX_FEATURE_STD_RANGES_REPEAT 0
#endif
#define LUMEX_HAS_STD_RANGES_REPEAT                                           \
  (LUMEX_FEATURE_STD_RANGES_REPEAT >= 202207L)

/// `__cpp_lib_ranges_reserve_hint` (C++26).
#if defined(__cpp_lib_ranges_reserve_hint)
#define LUMEX_FEATURE_STD_RANGES_RESERVE_HINT __cpp_lib_ranges_reserve_hint
#else
#define LUMEX_FEATURE_STD_RANGES_RESERVE_HINT 0
#endif
#define LUMEX_HAS_STD_RANGES_RESERVE_HINT                                     \
  (LUMEX_FEATURE_STD_RANGES_RESERVE_HINT >= 202502L)

/// `__cpp_lib_ranges_slide` (C++23).
#if defined(__cpp_lib_ranges_slide)
#define LUMEX_FEATURE_STD_RANGES_SLIDE __cpp_lib_ranges_slide
#else
#define LUMEX_FEATURE_STD_RANGES_SLIDE 0
#endif
#define LUMEX_HAS_STD_RANGES_SLIDE (LUMEX_FEATURE_STD_RANGES_SLIDE >= 202202L)

/// `__cpp_lib_ranges_starts_ends_with` (C++23).
#if defined(__cpp_lib_ranges_starts_ends_with)
#define LUMEX_FEATURE_STD_RANGES_STARTS_ENDS_WITH                             \
  __cpp_lib_ranges_starts_ends_with
#else
#define LUMEX_FEATURE_STD_RANGES_STARTS_ENDS_WITH 0
#endif
#define LUMEX_HAS_STD_RANGES_STARTS_ENDS_WITH                                 \
  (LUMEX_FEATURE_STD_RANGES_STARTS_ENDS_WITH >= 202106L)

/// `__cpp_lib_ranges_stride` (C++23).
#if defined(__cpp_lib_ranges_stride)
#define LUMEX_FEATURE_STD_RANGES_STRIDE __cpp_lib_ranges_stride
#else
#define LUMEX_FEATURE_STD_RANGES_STRIDE 0
#endif
#define LUMEX_HAS_STD_RANGES_STRIDE                                           \
  (LUMEX_FEATURE_STD_RANGES_STRIDE >= 202207L)

/// `__cpp_lib_ranges_to_container` (C++23).
#if defined(__cpp_lib_ranges_to_container)
#define LUMEX_FEATURE_STD_RANGES_TO_CONTAINER __cpp_lib_ranges_to_container
#else
#define LUMEX_FEATURE_STD_RANGES_TO_CONTAINER 0
#endif
#define LUMEX_HAS_STD_RANGES_TO_CONTAINER                                     \
  (LUMEX_FEATURE_STD_RANGES_TO_CONTAINER >= 202202L)

/// `__cpp_lib_ranges_zip` (C++23).
#if defined(__cpp_lib_ranges_zip)
#define LUMEX_FEATURE_STD_RANGES_ZIP __cpp_lib_ranges_zip
#else
#define LUMEX_FEATURE_STD_RANGES_ZIP 0
#endif
#define LUMEX_HAS_STD_RANGES_ZIP (LUMEX_FEATURE_STD_RANGES_ZIP >= 202110L)

/// `__cpp_lib_ratio` (C++26).
#if defined(__cpp_lib_ratio)
#define LUMEX_FEATURE_STD_RATIO __cpp_lib_ratio
#else
#define LUMEX_FEATURE_STD_RATIO 0
#endif
#define LUMEX_HAS_STD_RATIO (LUMEX_FEATURE_STD_RATIO >= 202306L)

/// `__cpp_lib_raw_memory_algorithms` (C++17; later: 202411L C++26).
#if defined(__cpp_lib_raw_memory_algorithms)
#define LUMEX_FEATURE_STD_RAW_MEMORY_ALGORITHMS __cpp_lib_raw_memory_algorithms
#else
#define LUMEX_FEATURE_STD_RAW_MEMORY_ALGORITHMS 0
#endif
#define LUMEX_HAS_STD_RAW_MEMORY_ALGORITHMS                                   \
  (LUMEX_FEATURE_STD_RAW_MEMORY_ALGORITHMS >= 201606L)

/// `__cpp_lib_rcu` (C++26).
#if defined(__cpp_lib_rcu)
#define LUMEX_FEATURE_STD_RCU __cpp_lib_rcu
#else
#define LUMEX_FEATURE_STD_RCU 0
#endif
#define LUMEX_HAS_STD_RCU (LUMEX_FEATURE_STD_RCU >= 202306L)

/// `__cpp_lib_reference_from_temporary` (C++23).
#if defined(__cpp_lib_reference_from_temporary)
#define LUMEX_FEATURE_STD_REFERENCE_FROM_TEMPORARY                            \
  __cpp_lib_reference_from_temporary
#else
#define LUMEX_FEATURE_STD_REFERENCE_FROM_TEMPORARY 0
#endif
#define LUMEX_HAS_STD_REFERENCE_FROM_TEMPORARY                                \
  (LUMEX_FEATURE_STD_REFERENCE_FROM_TEMPORARY >= 202202L)

/// `__cpp_lib_reference_wrapper` (C++26).
#if defined(__cpp_lib_reference_wrapper)
#define LUMEX_FEATURE_STD_REFERENCE_WRAPPER __cpp_lib_reference_wrapper
#else
#define LUMEX_FEATURE_STD_REFERENCE_WRAPPER 0
#endif
#define LUMEX_HAS_STD_REFERENCE_WRAPPER                                       \
  (LUMEX_FEATURE_STD_REFERENCE_WRAPPER >= 202403L)

/// `__cpp_lib_reflection` (C++26).
#if defined(__cpp_lib_reflection)
#define LUMEX_FEATURE_STD_REFLECTION __cpp_lib_reflection
#else
#define LUMEX_FEATURE_STD_REFLECTION 0
#endif
#define LUMEX_HAS_STD_REFLECTION (LUMEX_FEATURE_STD_REFLECTION >= 202506L)

/// `__cpp_lib_remove_cvref` (C++20).
#if defined(__cpp_lib_remove_cvref)
#define LUMEX_FEATURE_STD_REMOVE_CVREF __cpp_lib_remove_cvref
#else
#define LUMEX_FEATURE_STD_REMOVE_CVREF 0
#endif
#define LUMEX_HAS_STD_REMOVE_CVREF (LUMEX_FEATURE_STD_REMOVE_CVREF >= 201711L)

/// `__cpp_lib_replaceable_contract_violation_handler` (C++26).
#if defined(__cpp_lib_replaceable_contract_violation_handler)
#define LUMEX_FEATURE_STD_REPLACEABLE_CONTRACT_VIOLATION_HANDLER              \
  __cpp_lib_replaceable_contract_violation_handler
#else
#define LUMEX_FEATURE_STD_REPLACEABLE_CONTRACT_VIOLATION_HANDLER 0
#endif
#define LUMEX_HAS_STD_REPLACEABLE_CONTRACT_VIOLATION_HANDLER                  \
  (LUMEX_FEATURE_STD_REPLACEABLE_CONTRACT_VIOLATION_HANDLER >= 202603L)

/// `__cpp_lib_result_of_sfinae` (C++14).
#if defined(__cpp_lib_result_of_sfinae)
#define LUMEX_FEATURE_STD_RESULT_OF_SFINAE __cpp_lib_result_of_sfinae
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201402L
#define LUMEX_FEATURE_STD_RESULT_OF_SFINAE 201210L
#else
#define LUMEX_FEATURE_STD_RESULT_OF_SFINAE 0
#endif
#define LUMEX_HAS_STD_RESULT_OF_SFINAE                                        \
  (LUMEX_FEATURE_STD_RESULT_OF_SFINAE >= 201210L)

/// `__cpp_lib_robust_nonmodifying_seq_ops` (C++14).
#if defined(__cpp_lib_robust_nonmodifying_seq_ops)
#define LUMEX_FEATURE_STD_ROBUST_NONMODIFYING_SEQ_OPS                         \
  __cpp_lib_robust_nonmodifying_seq_ops
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201402L
#define LUMEX_FEATURE_STD_ROBUST_NONMODIFYING_SEQ_OPS 201304L
#else
#define LUMEX_FEATURE_STD_ROBUST_NONMODIFYING_SEQ_OPS 0
#endif
#define LUMEX_HAS_STD_ROBUST_NONMODIFYING_SEQ_OPS                             \
  (LUMEX_FEATURE_STD_ROBUST_NONMODIFYING_SEQ_OPS >= 201304L)

/// `__cpp_lib_sample` (C++17).
#if defined(__cpp_lib_sample)
#define LUMEX_FEATURE_STD_SAMPLE __cpp_lib_sample
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_SAMPLE 201603L
#else
#define LUMEX_FEATURE_STD_SAMPLE 0
#endif
#define LUMEX_HAS_STD_SAMPLE (LUMEX_FEATURE_STD_SAMPLE >= 201603L)

/// `__cpp_lib_saturation_arithmetic` (C++26; later: 202603L C++26).
#if defined(__cpp_lib_saturation_arithmetic)
#define LUMEX_FEATURE_STD_SATURATION_ARITHMETIC __cpp_lib_saturation_arithmetic
#else
#define LUMEX_FEATURE_STD_SATURATION_ARITHMETIC 0
#endif
#define LUMEX_HAS_STD_SATURATION_ARITHMETIC                                   \
  (LUMEX_FEATURE_STD_SATURATION_ARITHMETIC >= 202311L)

/// `__cpp_lib_scoped_lock` (C++17).
#if defined(__cpp_lib_scoped_lock)
#define LUMEX_FEATURE_STD_SCOPED_LOCK __cpp_lib_scoped_lock
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_SCOPED_LOCK 201703L
#else
#define LUMEX_FEATURE_STD_SCOPED_LOCK 0
#endif
#define LUMEX_HAS_STD_SCOPED_LOCK (LUMEX_FEATURE_STD_SCOPED_LOCK >= 201703L)

/// `__cpp_lib_semaphore` (C++20).
#if defined(__cpp_lib_semaphore)
#define LUMEX_FEATURE_STD_SEMAPHORE __cpp_lib_semaphore
#else
#define LUMEX_FEATURE_STD_SEMAPHORE 0
#endif
#define LUMEX_HAS_STD_SEMAPHORE (LUMEX_FEATURE_STD_SEMAPHORE >= 201907L)

/// `__cpp_lib_senders` (C++26; later: 202506L C++26).
#if defined(__cpp_lib_senders)
#define LUMEX_FEATURE_STD_SENDERS __cpp_lib_senders
#else
#define LUMEX_FEATURE_STD_SENDERS 0
#endif
#define LUMEX_HAS_STD_SENDERS (LUMEX_FEATURE_STD_SENDERS >= 202406L)

/// `__cpp_lib_shared_mutex` (C++17).
#if defined(__cpp_lib_shared_mutex)
#define LUMEX_FEATURE_STD_SHARED_MUTEX __cpp_lib_shared_mutex
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_SHARED_MUTEX 201505L
#else
#define LUMEX_FEATURE_STD_SHARED_MUTEX 0
#endif
#define LUMEX_HAS_STD_SHARED_MUTEX (LUMEX_FEATURE_STD_SHARED_MUTEX >= 201505L)

/// `__cpp_lib_shared_ptr_arrays` (C++17; later: 201707L C++20).
#if defined(__cpp_lib_shared_ptr_arrays)
#define LUMEX_FEATURE_STD_SHARED_PTR_ARRAYS __cpp_lib_shared_ptr_arrays
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_SHARED_PTR_ARRAYS 201611L
#else
#define LUMEX_FEATURE_STD_SHARED_PTR_ARRAYS 0
#endif
#define LUMEX_HAS_STD_SHARED_PTR_ARRAYS                                       \
  (LUMEX_FEATURE_STD_SHARED_PTR_ARRAYS >= 201611L)

/// `__cpp_lib_shared_ptr_weak_type` (C++17).
#if defined(__cpp_lib_shared_ptr_weak_type)
#define LUMEX_FEATURE_STD_SHARED_PTR_WEAK_TYPE __cpp_lib_shared_ptr_weak_type
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_SHARED_PTR_WEAK_TYPE 201606L
#else
#define LUMEX_FEATURE_STD_SHARED_PTR_WEAK_TYPE 0
#endif
#define LUMEX_HAS_STD_SHARED_PTR_WEAK_TYPE                                    \
  (LUMEX_FEATURE_STD_SHARED_PTR_WEAK_TYPE >= 201606L)

/// `__cpp_lib_shared_timed_mutex` (C++14).
#if defined(__cpp_lib_shared_timed_mutex)
#define LUMEX_FEATURE_STD_SHARED_TIMED_MUTEX __cpp_lib_shared_timed_mutex
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201402L
#define LUMEX_FEATURE_STD_SHARED_TIMED_MUTEX 201402L
#else
#define LUMEX_FEATURE_STD_SHARED_TIMED_MUTEX 0
#endif
#define LUMEX_HAS_STD_SHARED_TIMED_MUTEX                                      \
  (LUMEX_FEATURE_STD_SHARED_TIMED_MUTEX >= 201402L)

/// `__cpp_lib_shift` (C++20; later: 202202L C++23).
#if defined(__cpp_lib_shift)
#define LUMEX_FEATURE_STD_SHIFT __cpp_lib_shift
#else
#define LUMEX_FEATURE_STD_SHIFT 0
#endif
#define LUMEX_HAS_STD_SHIFT (LUMEX_FEATURE_STD_SHIFT >= 201806L)

/// `__cpp_lib_simd` (C++26; later: 202606L C++29).
#if defined(__cpp_lib_simd)
#define LUMEX_FEATURE_STD_SIMD __cpp_lib_simd
#else
#define LUMEX_FEATURE_STD_SIMD 0
#endif
#define LUMEX_HAS_STD_SIMD (LUMEX_FEATURE_STD_SIMD >= 202411L)

/// `__cpp_lib_simd_bitops` (C++29).
#if defined(__cpp_lib_simd_bitops)
#define LUMEX_FEATURE_STD_SIMD_BITOPS __cpp_lib_simd_bitops
#else
#define LUMEX_FEATURE_STD_SIMD_BITOPS 0
#endif
#define LUMEX_HAS_STD_SIMD_BITOPS (LUMEX_FEATURE_STD_SIMD_BITOPS >= 202607L)

/// `__cpp_lib_simd_complex` (C++26).
#if defined(__cpp_lib_simd_complex)
#define LUMEX_FEATURE_STD_SIMD_COMPLEX __cpp_lib_simd_complex
#else
#define LUMEX_FEATURE_STD_SIMD_COMPLEX 0
#endif
#define LUMEX_HAS_STD_SIMD_COMPLEX (LUMEX_FEATURE_STD_SIMD_COMPLEX >= 202502L)

/// `__cpp_lib_simd_permutations` (C++26).
#if defined(__cpp_lib_simd_permutations)
#define LUMEX_FEATURE_STD_SIMD_PERMUTATIONS __cpp_lib_simd_permutations
#else
#define LUMEX_FEATURE_STD_SIMD_PERMUTATIONS 0
#endif
#define LUMEX_HAS_STD_SIMD_PERMUTATIONS                                       \
  (LUMEX_FEATURE_STD_SIMD_PERMUTATIONS >= 202506L)

/// `__cpp_lib_smart_ptr_for_overwrite` (C++20).
#if defined(__cpp_lib_smart_ptr_for_overwrite)
#define LUMEX_FEATURE_STD_SMART_PTR_FOR_OVERWRITE                             \
  __cpp_lib_smart_ptr_for_overwrite
#else
#define LUMEX_FEATURE_STD_SMART_PTR_FOR_OVERWRITE 0
#endif
#define LUMEX_HAS_STD_SMART_PTR_FOR_OVERWRITE                                 \
  (LUMEX_FEATURE_STD_SMART_PTR_FOR_OVERWRITE >= 202002L)

/// `__cpp_lib_smart_ptr_owner_equality` (C++26).
#if defined(__cpp_lib_smart_ptr_owner_equality)
#define LUMEX_FEATURE_STD_SMART_PTR_OWNER_EQUALITY                            \
  __cpp_lib_smart_ptr_owner_equality
#else
#define LUMEX_FEATURE_STD_SMART_PTR_OWNER_EQUALITY 0
#endif
#define LUMEX_HAS_STD_SMART_PTR_OWNER_EQUALITY                                \
  (LUMEX_FEATURE_STD_SMART_PTR_OWNER_EQUALITY >= 202306L)

/// `__cpp_lib_source_location` (C++20).
#if defined(__cpp_lib_source_location)
#define LUMEX_FEATURE_STD_SOURCE_LOCATION __cpp_lib_source_location
#else
#define LUMEX_FEATURE_STD_SOURCE_LOCATION 0
#endif
#define LUMEX_HAS_STD_SOURCE_LOCATION                                         \
  (LUMEX_FEATURE_STD_SOURCE_LOCATION >= 201907L)

/// `__cpp_lib_span` (C++20; later: 202311L C++26).
#if defined(__cpp_lib_span)
#define LUMEX_FEATURE_STD_SPAN __cpp_lib_span
#else
#define LUMEX_FEATURE_STD_SPAN 0
#endif
#define LUMEX_HAS_STD_SPAN (LUMEX_FEATURE_STD_SPAN >= 202002L)

/// `__cpp_lib_spanstream` (C++23).
#if defined(__cpp_lib_spanstream)
#define LUMEX_FEATURE_STD_SPANSTREAM __cpp_lib_spanstream
#else
#define LUMEX_FEATURE_STD_SPANSTREAM 0
#endif
#define LUMEX_HAS_STD_SPANSTREAM (LUMEX_FEATURE_STD_SPANSTREAM >= 202106L)

/// `__cpp_lib_ssize` (C++20).
#if defined(__cpp_lib_ssize)
#define LUMEX_FEATURE_STD_SSIZE __cpp_lib_ssize
#else
#define LUMEX_FEATURE_STD_SSIZE 0
#endif
#define LUMEX_HAS_STD_SSIZE (LUMEX_FEATURE_STD_SSIZE >= 201902L)

/// `__cpp_lib_sstream_from_string_view` (C++26).
#if defined(__cpp_lib_sstream_from_string_view)
#define LUMEX_FEATURE_STD_SSTREAM_FROM_STRING_VIEW                            \
  __cpp_lib_sstream_from_string_view
#else
#define LUMEX_FEATURE_STD_SSTREAM_FROM_STRING_VIEW 0
#endif
#define LUMEX_HAS_STD_SSTREAM_FROM_STRING_VIEW                                \
  (LUMEX_FEATURE_STD_SSTREAM_FROM_STRING_VIEW >= 202306L)

/// `__cpp_lib_stacktrace` (C++23).
#if defined(__cpp_lib_stacktrace)
#define LUMEX_FEATURE_STD_STACKTRACE __cpp_lib_stacktrace
#else
#define LUMEX_FEATURE_STD_STACKTRACE 0
#endif
#define LUMEX_HAS_STD_STACKTRACE (LUMEX_FEATURE_STD_STACKTRACE >= 202011L)

/// `__cpp_lib_start_lifetime` (C++26).
#if defined(__cpp_lib_start_lifetime)
#define LUMEX_FEATURE_STD_START_LIFETIME __cpp_lib_start_lifetime
#else
#define LUMEX_FEATURE_STD_START_LIFETIME 0
#endif
#define LUMEX_HAS_STD_START_LIFETIME                                          \
  (LUMEX_FEATURE_STD_START_LIFETIME >= 202603L)

/// `__cpp_lib_start_lifetime_as` (C++23).
#if defined(__cpp_lib_start_lifetime_as)
#define LUMEX_FEATURE_STD_START_LIFETIME_AS __cpp_lib_start_lifetime_as
#else
#define LUMEX_FEATURE_STD_START_LIFETIME_AS 0
#endif
#define LUMEX_HAS_STD_START_LIFETIME_AS                                       \
  (LUMEX_FEATURE_STD_START_LIFETIME_AS >= 202207L)

/// `__cpp_lib_starts_ends_with` (C++20).
#if defined(__cpp_lib_starts_ends_with)
#define LUMEX_FEATURE_STD_STARTS_ENDS_WITH __cpp_lib_starts_ends_with
#else
#define LUMEX_FEATURE_STD_STARTS_ENDS_WITH 0
#endif
#define LUMEX_HAS_STD_STARTS_ENDS_WITH                                        \
  (LUMEX_FEATURE_STD_STARTS_ENDS_WITH >= 201711L)

/// `__cpp_lib_stdatomic_h` (C++23).
#if defined(__cpp_lib_stdatomic_h)
#define LUMEX_FEATURE_STD_STDATOMIC_H __cpp_lib_stdatomic_h
#else
#define LUMEX_FEATURE_STD_STDATOMIC_H 0
#endif
#define LUMEX_HAS_STD_STDATOMIC_H (LUMEX_FEATURE_STD_STDATOMIC_H >= 202011L)

/// `__cpp_lib_stdbit_h` (C++26).
#if defined(__cpp_lib_stdbit_h)
#define LUMEX_FEATURE_STD_STDBIT_H __cpp_lib_stdbit_h
#else
#define LUMEX_FEATURE_STD_STDBIT_H 0
#endif
#define LUMEX_HAS_STD_STDBIT_H (LUMEX_FEATURE_STD_STDBIT_H >= 202603L)

/// `__cpp_lib_stdckdint_h` (C++26).
#if defined(__cpp_lib_stdckdint_h)
#define LUMEX_FEATURE_STD_STDCKDINT_H __cpp_lib_stdckdint_h
#else
#define LUMEX_FEATURE_STD_STDCKDINT_H 0
#endif
#define LUMEX_HAS_STD_STDCKDINT_H (LUMEX_FEATURE_STD_STDCKDINT_H >= 202603L)

/// `__cpp_lib_string_contains` (C++23).
#if defined(__cpp_lib_string_contains)
#define LUMEX_FEATURE_STD_STRING_CONTAINS __cpp_lib_string_contains
#else
#define LUMEX_FEATURE_STD_STRING_CONTAINS 0
#endif
#define LUMEX_HAS_STD_STRING_CONTAINS                                         \
  (LUMEX_FEATURE_STD_STRING_CONTAINS >= 202011L)

/// `__cpp_lib_string_resize_and_overwrite` (C++23).
#if defined(__cpp_lib_string_resize_and_overwrite)
#define LUMEX_FEATURE_STD_STRING_RESIZE_AND_OVERWRITE                         \
  __cpp_lib_string_resize_and_overwrite
#else
#define LUMEX_FEATURE_STD_STRING_RESIZE_AND_OVERWRITE 0
#endif
#define LUMEX_HAS_STD_STRING_RESIZE_AND_OVERWRITE                             \
  (LUMEX_FEATURE_STD_STRING_RESIZE_AND_OVERWRITE >= 202110L)

/// `__cpp_lib_string_subview` (C++26).
#if defined(__cpp_lib_string_subview)
#define LUMEX_FEATURE_STD_STRING_SUBVIEW __cpp_lib_string_subview
#else
#define LUMEX_FEATURE_STD_STRING_SUBVIEW 0
#endif
#define LUMEX_HAS_STD_STRING_SUBVIEW                                          \
  (LUMEX_FEATURE_STD_STRING_SUBVIEW >= 202506L)

/// `__cpp_lib_string_udls` (C++14).
#if defined(__cpp_lib_string_udls)
#define LUMEX_FEATURE_STD_STRING_UDLS __cpp_lib_string_udls
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201402L
#define LUMEX_FEATURE_STD_STRING_UDLS 201304L
#else
#define LUMEX_FEATURE_STD_STRING_UDLS 0
#endif
#define LUMEX_HAS_STD_STRING_UDLS (LUMEX_FEATURE_STD_STRING_UDLS >= 201304L)

/// `__cpp_lib_string_view` (C++17; later: 201803L C++20, 202403L C++26).
#if defined(__cpp_lib_string_view)
#define LUMEX_FEATURE_STD_STRING_VIEW __cpp_lib_string_view
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_STRING_VIEW 201606L
#else
#define LUMEX_FEATURE_STD_STRING_VIEW 0
#endif
#define LUMEX_HAS_STD_STRING_VIEW (LUMEX_FEATURE_STD_STRING_VIEW >= 201606L)

/// `__cpp_lib_submdspan` (C++26; later: 202403L C++26).
#if defined(__cpp_lib_submdspan)
#define LUMEX_FEATURE_STD_SUBMDSPAN __cpp_lib_submdspan
#else
#define LUMEX_FEATURE_STD_SUBMDSPAN 0
#endif
#define LUMEX_HAS_STD_SUBMDSPAN (LUMEX_FEATURE_STD_SUBMDSPAN >= 202306L)

/// `__cpp_lib_syncbuf` (C++20).
#if defined(__cpp_lib_syncbuf)
#define LUMEX_FEATURE_STD_SYNCBUF __cpp_lib_syncbuf
#else
#define LUMEX_FEATURE_STD_SYNCBUF 0
#endif
#define LUMEX_HAS_STD_SYNCBUF (LUMEX_FEATURE_STD_SYNCBUF >= 201803L)

/// `__cpp_lib_task` (C++26).
#if defined(__cpp_lib_task)
#define LUMEX_FEATURE_STD_TASK __cpp_lib_task
#else
#define LUMEX_FEATURE_STD_TASK 0
#endif
#define LUMEX_HAS_STD_TASK (LUMEX_FEATURE_STD_TASK >= 202506L)

/// `__cpp_lib_text_encoding` (C++26).
#if defined(__cpp_lib_text_encoding)
#define LUMEX_FEATURE_STD_TEXT_ENCODING __cpp_lib_text_encoding
#else
#define LUMEX_FEATURE_STD_TEXT_ENCODING 0
#endif
#define LUMEX_HAS_STD_TEXT_ENCODING                                           \
  (LUMEX_FEATURE_STD_TEXT_ENCODING >= 202306L)

/// `__cpp_lib_thread_attributes` (C++29).
#if defined(__cpp_lib_thread_attributes)
#define LUMEX_FEATURE_STD_THREAD_ATTRIBUTES __cpp_lib_thread_attributes
#else
#define LUMEX_FEATURE_STD_THREAD_ATTRIBUTES 0
#endif
#define LUMEX_HAS_STD_THREAD_ATTRIBUTES                                       \
  (LUMEX_FEATURE_STD_THREAD_ATTRIBUTES >= 202606L)

/// `__cpp_lib_three_way_comparison` (C++20).
#if defined(__cpp_lib_three_way_comparison)
#define LUMEX_FEATURE_STD_THREE_WAY_COMPARISON __cpp_lib_three_way_comparison
#else
#define LUMEX_FEATURE_STD_THREE_WAY_COMPARISON 0
#endif
#define LUMEX_HAS_STD_THREE_WAY_COMPARISON                                    \
  (LUMEX_FEATURE_STD_THREE_WAY_COMPARISON >= 201907L)

/// `__cpp_lib_to_address` (C++20).
#if defined(__cpp_lib_to_address)
#define LUMEX_FEATURE_STD_TO_ADDRESS __cpp_lib_to_address
#else
#define LUMEX_FEATURE_STD_TO_ADDRESS 0
#endif
#define LUMEX_HAS_STD_TO_ADDRESS (LUMEX_FEATURE_STD_TO_ADDRESS >= 201711L)

/// `__cpp_lib_to_array` (C++20).
#if defined(__cpp_lib_to_array)
#define LUMEX_FEATURE_STD_TO_ARRAY __cpp_lib_to_array
#else
#define LUMEX_FEATURE_STD_TO_ARRAY 0
#endif
#define LUMEX_HAS_STD_TO_ARRAY (LUMEX_FEATURE_STD_TO_ARRAY >= 201907L)

/// `__cpp_lib_to_chars` (C++17; later: 202306L C++26, 202606L C++29).
#if defined(__cpp_lib_to_chars)
#define LUMEX_FEATURE_STD_TO_CHARS __cpp_lib_to_chars
#else
#define LUMEX_FEATURE_STD_TO_CHARS 0
#endif
#define LUMEX_HAS_STD_TO_CHARS (LUMEX_FEATURE_STD_TO_CHARS >= 201611L)

/// `__cpp_lib_to_string` (C++26).
#if defined(__cpp_lib_to_string)
#define LUMEX_FEATURE_STD_TO_STRING __cpp_lib_to_string
#else
#define LUMEX_FEATURE_STD_TO_STRING 0
#endif
#define LUMEX_HAS_STD_TO_STRING (LUMEX_FEATURE_STD_TO_STRING >= 202306L)

/// `__cpp_lib_to_underlying` (C++23).
#if defined(__cpp_lib_to_underlying)
#define LUMEX_FEATURE_STD_TO_UNDERLYING __cpp_lib_to_underlying
#else
#define LUMEX_FEATURE_STD_TO_UNDERLYING 0
#endif
#define LUMEX_HAS_STD_TO_UNDERLYING                                           \
  (LUMEX_FEATURE_STD_TO_UNDERLYING >= 202102L)

/// `__cpp_lib_transformation_trait_aliases` (C++14).
#if defined(__cpp_lib_transformation_trait_aliases)
#define LUMEX_FEATURE_STD_TRANSFORMATION_TRAIT_ALIASES                        \
  __cpp_lib_transformation_trait_aliases
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201402L
#define LUMEX_FEATURE_STD_TRANSFORMATION_TRAIT_ALIASES 201304L
#else
#define LUMEX_FEATURE_STD_TRANSFORMATION_TRAIT_ALIASES 0
#endif
#define LUMEX_HAS_STD_TRANSFORMATION_TRAIT_ALIASES                            \
  (LUMEX_FEATURE_STD_TRANSFORMATION_TRAIT_ALIASES >= 201304L)

/// `__cpp_lib_transparent_operators` (C++14; later: 201510L C++17).
#if defined(__cpp_lib_transparent_operators)
#define LUMEX_FEATURE_STD_TRANSPARENT_OPERATORS __cpp_lib_transparent_operators
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201402L
#define LUMEX_FEATURE_STD_TRANSPARENT_OPERATORS 201210L
#else
#define LUMEX_FEATURE_STD_TRANSPARENT_OPERATORS 0
#endif
#define LUMEX_HAS_STD_TRANSPARENT_OPERATORS                                   \
  (LUMEX_FEATURE_STD_TRANSPARENT_OPERATORS >= 201210L)

/// `__cpp_lib_tuple_element_t` (C++14).
#if defined(__cpp_lib_tuple_element_t)
#define LUMEX_FEATURE_STD_TUPLE_ELEMENT_T __cpp_lib_tuple_element_t
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201402L
#define LUMEX_FEATURE_STD_TUPLE_ELEMENT_T 201402L
#else
#define LUMEX_FEATURE_STD_TUPLE_ELEMENT_T 0
#endif
#define LUMEX_HAS_STD_TUPLE_ELEMENT_T                                         \
  (LUMEX_FEATURE_STD_TUPLE_ELEMENT_T >= 201402L)

/// `__cpp_lib_tuple_like` (C++23; later: 202311L C++26).
#if defined(__cpp_lib_tuple_like)
#define LUMEX_FEATURE_STD_TUPLE_LIKE __cpp_lib_tuple_like
#else
#define LUMEX_FEATURE_STD_TUPLE_LIKE 0
#endif
#define LUMEX_HAS_STD_TUPLE_LIKE (LUMEX_FEATURE_STD_TUPLE_LIKE >= 202207L)

/// `__cpp_lib_tuples_by_type` (C++14).
#if defined(__cpp_lib_tuples_by_type)
#define LUMEX_FEATURE_STD_TUPLES_BY_TYPE __cpp_lib_tuples_by_type
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201402L
#define LUMEX_FEATURE_STD_TUPLES_BY_TYPE 201304L
#else
#define LUMEX_FEATURE_STD_TUPLES_BY_TYPE 0
#endif
#define LUMEX_HAS_STD_TUPLES_BY_TYPE                                          \
  (LUMEX_FEATURE_STD_TUPLES_BY_TYPE >= 201304L)

/// `__cpp_lib_type_identity` (C++20).
#if defined(__cpp_lib_type_identity)
#define LUMEX_FEATURE_STD_TYPE_IDENTITY __cpp_lib_type_identity
#else
#define LUMEX_FEATURE_STD_TYPE_IDENTITY 0
#endif
#define LUMEX_HAS_STD_TYPE_IDENTITY                                           \
  (LUMEX_FEATURE_STD_TYPE_IDENTITY >= 201806L)

/// `__cpp_lib_type_order` (C++26).
#if defined(__cpp_lib_type_order)
#define LUMEX_FEATURE_STD_TYPE_ORDER __cpp_lib_type_order
#else
#define LUMEX_FEATURE_STD_TYPE_ORDER 0
#endif
#define LUMEX_HAS_STD_TYPE_ORDER (LUMEX_FEATURE_STD_TYPE_ORDER >= 202506L)

/// `__cpp_lib_type_trait_variable_templates` (C++17).
#if defined(__cpp_lib_type_trait_variable_templates)
#define LUMEX_FEATURE_STD_TYPE_TRAIT_VARIABLE_TEMPLATES                       \
  __cpp_lib_type_trait_variable_templates
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_TYPE_TRAIT_VARIABLE_TEMPLATES 201510L
#else
#define LUMEX_FEATURE_STD_TYPE_TRAIT_VARIABLE_TEMPLATES 0
#endif
#define LUMEX_HAS_STD_TYPE_TRAIT_VARIABLE_TEMPLATES                           \
  (LUMEX_FEATURE_STD_TYPE_TRAIT_VARIABLE_TEMPLATES >= 201510L)

/// `__cpp_lib_uncaught_exceptions` (C++17).
#if defined(__cpp_lib_uncaught_exceptions)
#define LUMEX_FEATURE_STD_UNCAUGHT_EXCEPTIONS __cpp_lib_uncaught_exceptions
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_UNCAUGHT_EXCEPTIONS 201411L
#else
#define LUMEX_FEATURE_STD_UNCAUGHT_EXCEPTIONS 0
#endif
#define LUMEX_HAS_STD_UNCAUGHT_EXCEPTIONS                                     \
  (LUMEX_FEATURE_STD_UNCAUGHT_EXCEPTIONS >= 201411L)

/// `__cpp_lib_unordered_map_try_emplace` (C++17).
#if defined(__cpp_lib_unordered_map_try_emplace)
#define LUMEX_FEATURE_STD_UNORDERED_MAP_TRY_EMPLACE                           \
  __cpp_lib_unordered_map_try_emplace
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_UNORDERED_MAP_TRY_EMPLACE 201411L
#else
#define LUMEX_FEATURE_STD_UNORDERED_MAP_TRY_EMPLACE 0
#endif
#define LUMEX_HAS_STD_UNORDERED_MAP_TRY_EMPLACE                               \
  (LUMEX_FEATURE_STD_UNORDERED_MAP_TRY_EMPLACE >= 201411L)

/// `__cpp_lib_unreachable` (C++23).
#if defined(__cpp_lib_unreachable)
#define LUMEX_FEATURE_STD_UNREACHABLE __cpp_lib_unreachable
#else
#define LUMEX_FEATURE_STD_UNREACHABLE 0
#endif
#define LUMEX_HAS_STD_UNREACHABLE (LUMEX_FEATURE_STD_UNREACHABLE >= 202202L)

/// `__cpp_lib_unwrap_ref` (C++20).
#if defined(__cpp_lib_unwrap_ref)
#define LUMEX_FEATURE_STD_UNWRAP_REF __cpp_lib_unwrap_ref
#else
#define LUMEX_FEATURE_STD_UNWRAP_REF 0
#endif
#define LUMEX_HAS_STD_UNWRAP_REF (LUMEX_FEATURE_STD_UNWRAP_REF >= 201811L)

/// `__cpp_lib_valarray` (C++26).
#if defined(__cpp_lib_valarray)
#define LUMEX_FEATURE_STD_VALARRAY __cpp_lib_valarray
#else
#define LUMEX_FEATURE_STD_VALARRAY 0
#endif
#define LUMEX_HAS_STD_VALARRAY (LUMEX_FEATURE_STD_VALARRAY >= 202511L)

/// `__cpp_lib_variant` (C++17; later: 202102L C++23, 202106L C++23, 202306L
/// C++26).
#if defined(__cpp_lib_variant)
#define LUMEX_FEATURE_STD_VARIANT __cpp_lib_variant
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_VARIANT 201606L
#else
#define LUMEX_FEATURE_STD_VARIANT 0
#endif
#define LUMEX_HAS_STD_VARIANT (LUMEX_FEATURE_STD_VARIANT >= 201606L)

/// `__cpp_lib_view_interface` (C++29).
#if defined(__cpp_lib_view_interface)
#define LUMEX_FEATURE_STD_VIEW_INTERFACE __cpp_lib_view_interface
#else
#define LUMEX_FEATURE_STD_VIEW_INTERFACE 0
#endif
#define LUMEX_HAS_STD_VIEW_INTERFACE                                          \
  (LUMEX_FEATURE_STD_VIEW_INTERFACE >= 202606L)

/// `__cpp_lib_void_t` (C++17).
#if defined(__cpp_lib_void_t)
#define LUMEX_FEATURE_STD_VOID_T __cpp_lib_void_t
#elif !LUMEX_HAS_VERSION_HEADER && __cplusplus >= 201703L
#define LUMEX_FEATURE_STD_VOID_T 201411L
#else
#define LUMEX_FEATURE_STD_VOID_T 0
#endif
#define LUMEX_HAS_STD_VOID_T (LUMEX_FEATURE_STD_VOID_T >= 201411L)

// --- Combined forms used by the library ---

/// `[[nodiscard ("reason")]]`: the C++20 revision of `[[nodiscard]]`.
#define LUMEX_HAS_NODISCARD_MESSAGE                                           \
  (LUMEX_FEATURE_ATTRIBUTE_NODISCARD >= 201907L && __cplusplus > 201703L)

/// `operator<=>` in the language and `<compare>` in the library.
#define LUMEX_HAS_THREE_WAY_COMPARISON                                        \
  (LUMEX_HAS_IMPL_THREE_WAY_COMPARISON && LUMEX_HAS_STD_THREE_WAY_COMPARISON)

#endif // LUMEX_CORE_UTILITY_COMPILER_FEATURES_HPP
