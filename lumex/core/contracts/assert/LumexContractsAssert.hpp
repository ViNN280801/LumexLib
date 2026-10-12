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
 * @file LumexContractsAssert.hpp
 * @brief `LUMEX_CONTRACT_ASSERT (expr)`, the contract assertion of C++26
 * (`contract_assert`, P2900) on every standard, and the macros that choose its
 * evaluation semantic.
 * @details Use `LUMEX_CONTRACT_ASSERT (predicate);` where C++26 would write
 * `contract_assert (predicate);`: in a function body, as a statement. The
 * predicate is contextually converted to `bool` ([stmt.contract.assert]); an
 * expression that does not convert does not compile. The macro is variadic,
 * so a comma in a template argument list needs no extra parentheses
 * (`LUMEX_CONTRACT_ASSERT (std::is_same<A, B>::value)`), and the text of the
 * predicate as written (before macro expansion) is what a violation reports as
 * its `comment ()`. Outside C++26 the macro expands to an expression, so it
 * also works in a `constexpr` function body from C++11 (an assertion that
 * holds adds nothing at run time and nothing at compile time; one that fails
 * during constant evaluation is a compile error) and in `if (...) ...; else`
 * without braces.
 *
 * Evaluation semantic ([basic.contract.eval]). Exactly one of five is in
 * force for a translation unit, chosen by the first of these that exists:
 * 1. the macro `LUMEX_CONTRACTS_SEMANTIC` defined before this header is first
 *    included (`#define LUMEX_CONTRACTS_SEMANTIC observe`, or
 *    `-DLUMEX_CONTRACTS_SEMANTIC=observe`);
 * 2. the macro `LUMEX_CONTRACTS_BUILD_SEMANTIC` that the CMake option
 *    `LUMEX_CONTRACTS_SEMANTIC` puts on the target `lumex::contracts` (the
 *    choice of the build);
 * 3. `enforce`.
 * The values are written as the bare words `ignore`, `observe`, `enforce`,
 * `quick_enforce` and `p2900`; anything else is an error. They mean:
 * - `ignore`: the predicate is compiled and checked for syntax and types but
 *   never evaluated, and nothing is reported.
 * - `observe`: the predicate is evaluated once; a false one is reported to the
 *   violation handler, and the program continues after the handler returns.
 * - `enforce` (the default, as [basic.contract.eval] recommends): the same,
 *   then `std::abort ()` when the handler returns. A handler that throws lets
 *   the exception out instead.
 * - `quick_enforce`: the predicate is evaluated once; a false one stops the
 *   program at once (a trap instruction) without calling a handler.
 * - `p2900`: the macro is the compiler's own `contract_assert` where it
 *   exists (`__cpp_contracts` of 202502L or more, which is
 * `LUMEX_CONTRACTS_HAS_NATIVE`), and the compiler's own flags and handler then
 * decide the semantic; where it does not exist the semantic is `enforce`.
 * `LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC` is 1, 2, 3, 4 for the four emulated
 * ones and 5 when the native `contract_assert` is used.
 * `LUMEX_CONTRACT_ASSERT_IGNORE`, `_OBSERVE`, `_ENFORCE`, `_QUICK_ENFORCE` and
 * `_P2900` are the same macro with the semantic fixed, whatever the choice
 * above. The first four are always the emulation of this header; the library
 * handler slot is used by `observe` and `enforce` only.
 *
 * The predicate is evaluated exactly once with `observe`, `enforce` and
 * `quick_enforce`, and zero times with `ignore`, never twice, and
 * `NDEBUG` has no effect on any of this (a contract assertion is not
 * `assert`; to switch the checks off in a release build choose `ignore` for
 * it). An exception that leaves the predicate propagates as any exception
 * would
 * ([basic.contract.eval] would report it as a violation of detection mode
 * `evaluation_exception`); defining `LUMEX_CONTRACTS_CATCH_EXCEPTIONS` to 1
 * before the first include does that instead, at the price of a lambda in
 * every expansion (so no use inside `constexpr` functions before C++17) and
 * the code size of a try block. The violation path is not `noexcept`: the
 * handler may throw. The `observe` and `enforce` paths build a
 * `contract_violation` of kind `assert` and call `invoke_violation_handler`.
 *
 * `LUMEX_ASSERT` of the utility module is a different facility and is not
 * touched: it is always active and its handler is fixed.
 */
#ifndef LUMEX_CORE_CONTRACTS_ASSERT_HPP
#define LUMEX_CORE_CONTRACTS_ASSERT_HPP

#include <cstdint>
#include <cstdlib>

#if defined(_MSC_VER) && (_MSC_VER >= 1900)
#include <intrin.h>
#endif

#include "lumex/core/contracts/handler/LumexContractsHandler.hpp"
#include "lumex/core/contracts/location/LumexContractsSourceLocation.hpp"
#include "lumex/core/contracts/violation/LumexContractsViolation.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/macros/LumexMacros.hpp"

// --- Configuration ---------------------------------------------------------

/**
 * @def LUMEX_CONTRACTS_HAS_NATIVE
 * @brief 1 when the compiler implements P2900 (`contract_assert` as a
 * keyword), otherwise 0. `__cpp_contracts` of 201906L is the withdrawn
 * contracts of the C++20 draft (GCC `-fcontracts`), which has no
 * `contract_assert`, and does not count. May be defined by the user to 0 or 1
 * before the first include.
 */
#ifndef LUMEX_CONTRACTS_HAS_NATIVE
#if defined(__cpp_contracts) && (__cpp_contracts >= 202502L)
#define LUMEX_CONTRACTS_HAS_NATIVE 1
#else
#define LUMEX_CONTRACTS_HAS_NATIVE 0
#endif
#endif

#ifndef LUMEX_CONTRACTS_CATCH_EXCEPTIONS
#define LUMEX_CONTRACTS_CATCH_EXCEPTIONS 0
#endif

#define LUMEX_CONTRACTS_SEMANTIC_ID_ignore 1
#define LUMEX_CONTRACTS_SEMANTIC_ID_observe 2
#define LUMEX_CONTRACTS_SEMANTIC_ID_enforce 3
#define LUMEX_CONTRACTS_SEMANTIC_ID_quick_enforce 4
#define LUMEX_CONTRACTS_SEMANTIC_ID_p2900 5

#define LUMEX_CONTRACTS_DETAIL_PASTE_(a, b) a##b
#define LUMEX_CONTRACTS_DETAIL_PASTE(a, b) LUMEX_CONTRACTS_DETAIL_PASTE_ (a, b)

#if defined(LUMEX_CONTRACTS_SEMANTIC)
#define LUMEX_CONTRACTS_DETAIL_REQUESTED LUMEX_CONTRACTS_SEMANTIC
#elif defined(LUMEX_CONTRACTS_BUILD_SEMANTIC)
#define LUMEX_CONTRACTS_DETAIL_REQUESTED LUMEX_CONTRACTS_BUILD_SEMANTIC
#else
#define LUMEX_CONTRACTS_DETAIL_REQUESTED enforce
#endif

/**
 * @def LUMEX_CONTRACTS_REQUESTED_SEMANTIC
 * @brief The semantic that was asked for, as a number: 1 ignore, 2 observe, 3
 * enforce, 4 quick_enforce, 5 p2900.
 */
#define LUMEX_CONTRACTS_REQUESTED_SEMANTIC                                    \
  LUMEX_CONTRACTS_DETAIL_PASTE (LUMEX_CONTRACTS_SEMANTIC_ID_,                 \
                                LUMEX_CONTRACTS_DETAIL_REQUESTED)

#if LUMEX_CONTRACTS_REQUESTED_SEMANTIC == 0
#error                                                                        \
    "LUMEX_CONTRACTS_SEMANTIC must be one of: ignore, observe, enforce, quick_enforce, p2900"
#endif

/**
 * @def LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC
 * @brief The semantic `LUMEX_CONTRACT_ASSERT` has in this translation unit:
 * 1 ignore, 2 observe, 3 enforce, 4 quick_enforce, 5 the compiler's own
 * `contract_assert`.
 */
#if LUMEX_CONTRACTS_REQUESTED_SEMANTIC == 5
#if LUMEX_CONTRACTS_HAS_NATIVE
#define LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC 5
#else
#define LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC 3
#endif
#else
#define LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC LUMEX_CONTRACTS_REQUESTED_SEMANTIC
#endif

// --- Expansion helpers -----------------------------------------------------

#if defined(__GNUC__) || defined(__clang__)
#define LUMEX_CONTRACTS_DETAIL_LIKELY(condition)                              \
  __builtin_expect (static_cast<bool> (condition), 1)
#else
#define LUMEX_CONTRACTS_DETAIL_LIKELY(condition) (condition)
#endif

#define LUMEX_CONTRACTS_DETAIL_LOCATION                                       \
  ::lumex::core::contracts::source_location (                                 \
      __FILE__, LUMEX_FUNCTION_NAME,                                          \
      static_cast<std::uint_least32_t> (__LINE__),                            \
      static_cast<std::uint_least32_t> (LUMEX_CONTRACTS_BUILTIN_COLUMN))

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace contracts
{
namespace detail
{
/**
 * @brief The emulated semantics that check the predicate. Each has the static
 * function `violated`, called on the failing path only (static members, one
 * type per semantic: GCC 8 mistakes overloaded `noinline` inline functions of
 * a namespace for redeclarations).
 */
struct observe_t
{
  /** @brief `observe`: the handler runs, the program continues. */
  static LUMEX_ATTRIBUTE_NOINLINE void
  violated (char const *comment, source_location const &where,
            contracts::detection_mode mode)
  {
    invoke_violation_handler (
        contract_violation (comment, assertion_kind::assert,
                            evaluation_semantic::observe, mode, where));
  }
};

struct enforce_t
{
  /** @brief `enforce`: the handler runs, then the program is aborted. */
  LUMEX_ATTRIBUTE_NORETURN static LUMEX_ATTRIBUTE_NOINLINE void
  violated (char const *comment, source_location const &where,
            contracts::detection_mode mode)
  {
    invoke_violation_handler (
        contract_violation (comment, assertion_kind::assert,
                            evaluation_semantic::enforce, mode, where));
    std::abort ();
  }
};

struct quick_enforce_t
{
  /** @brief `quick_enforce`: no handler, the program stops at once. */
  LUMEX_ATTRIBUTE_NORETURN static LUMEX_ATTRIBUTE_NOINLINE void
  violated (char const *, source_location const &,
            contracts::detection_mode) LUMEX_NOEXCEPT
  {
#if defined(__GNUC__) || defined(__clang__)
    __builtin_trap ();
#elif defined(_MSC_VER) && (_MSC_VER >= 1900)
    __fastfail (7); // FAST_FAIL_FATAL_APP_EXIT
#else
    std::abort ();
#endif
  }
};
} // namespace detail
} // namespace contracts
} // namespace core
} // namespace lumex

// `comment` is the stringized predicate, made by the macro the user wrote
// (before macro expansion of its argument); the predicate follows as the
// variable arguments, so that commas of template arguments stay in it.
#if LUMEX_CONTRACTS_CATCH_EXCEPTIONS
#define LUMEX_CONTRACTS_DETAIL_CHECK(tag, comment, ...)                       \
  (                                                                           \
      [&] (::lumex::core::contracts::source_location const                    \
               &lumex_contracts_where) -> void                                \
        {                                                                     \
          bool lumex_contracts_holds = false;                                 \
          try                                                                 \
            {                                                                 \
              lumex_contracts_holds = static_cast<bool> ((__VA_ARGS__));      \
            }                                                                 \
          catch (...)                                                         \
            {                                                                 \
              ::lumex::core::contracts::detail::tag::violated (               \
                  comment, lumex_contracts_where,                             \
                  ::lumex::core::contracts::detection_mode::                  \
                      evaluation_exception);                                  \
              return;                                                         \
            }                                                                 \
          if (!lumex_contracts_holds)                                         \
            ::lumex::core::contracts::detail::tag::violated (                 \
                comment, lumex_contracts_where,                               \
                ::lumex::core::contracts::detection_mode::predicate_false);   \
        }) (LUMEX_CONTRACTS_DETAIL_LOCATION)
#else
#define LUMEX_CONTRACTS_DETAIL_CHECK(tag, comment, ...)                       \
  (LUMEX_CONTRACTS_DETAIL_LIKELY (static_cast<bool> ((__VA_ARGS__)))          \
       ? static_cast<void> (0)                                                \
       : ::lumex::core::contracts::detail::tag::violated (                    \
             comment, LUMEX_CONTRACTS_DETAIL_LOCATION,                        \
             ::lumex::core::contracts::detection_mode::predicate_false))
#endif

// --- The macros ------------------------------------------------------------

/**
 * @def LUMEX_CONTRACT_ASSERT_IGNORE
 * @brief The `ignore` semantic: the predicate is compiled, never evaluated.
 */
#define LUMEX_CONTRACT_ASSERT_IGNORE(...)                                     \
  static_cast<void> (false && static_cast<bool> ((__VA_ARGS__)))

/** @def LUMEX_CONTRACT_ASSERT_OBSERVE
 * @brief The `observe` semantic: report a false predicate, then continue. */
#define LUMEX_CONTRACT_ASSERT_OBSERVE(...)                                    \
  LUMEX_CONTRACTS_DETAIL_CHECK (observe_t, #__VA_ARGS__, __VA_ARGS__)

/** @def LUMEX_CONTRACT_ASSERT_ENFORCE
 * @brief The `enforce` semantic: report a false predicate, then abort. */
#define LUMEX_CONTRACT_ASSERT_ENFORCE(...)                                    \
  LUMEX_CONTRACTS_DETAIL_CHECK (enforce_t, #__VA_ARGS__, __VA_ARGS__)

/** @def LUMEX_CONTRACT_ASSERT_QUICK_ENFORCE
 * @brief The `quick_enforce` semantic: stop at once on a false predicate. */
#define LUMEX_CONTRACT_ASSERT_QUICK_ENFORCE(...)                              \
  LUMEX_CONTRACTS_DETAIL_CHECK (quick_enforce_t, #__VA_ARGS__, __VA_ARGS__)

/**
 * @def LUMEX_CONTRACT_ASSERT_P2900
 * @brief The compiler's `contract_assert` where `LUMEX_CONTRACTS_HAS_NATIVE`
 * is 1, otherwise `LUMEX_CONTRACT_ASSERT_ENFORCE`. Use as a statement.
 */
#if LUMEX_CONTRACTS_HAS_NATIVE
#define LUMEX_CONTRACT_ASSERT_P2900(...) contract_assert (__VA_ARGS__)
#else
#define LUMEX_CONTRACT_ASSERT_P2900(...)                                      \
  LUMEX_CONTRACTS_DETAIL_CHECK (enforce_t, #__VA_ARGS__, __VA_ARGS__)
#endif

/**
 * @def LUMEX_CONTRACT_ASSERT
 * @brief The contract assertion, with the semantic of the translation unit
 * (see the file description).
 * @param ... The predicate, contextually converted to `bool`.
 */
#if LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC == 1
#define LUMEX_CONTRACT_ASSERT(...)                                            \
  static_cast<void> (false && static_cast<bool> ((__VA_ARGS__)))
#elif LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC == 2
#define LUMEX_CONTRACT_ASSERT(...)                                            \
  LUMEX_CONTRACTS_DETAIL_CHECK (observe_t, #__VA_ARGS__, __VA_ARGS__)
#elif LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC == 3
#define LUMEX_CONTRACT_ASSERT(...)                                            \
  LUMEX_CONTRACTS_DETAIL_CHECK (enforce_t, #__VA_ARGS__, __VA_ARGS__)
#elif LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC == 4
#define LUMEX_CONTRACT_ASSERT(...)                                            \
  LUMEX_CONTRACTS_DETAIL_CHECK (quick_enforce_t, #__VA_ARGS__, __VA_ARGS__)
#else
#define LUMEX_CONTRACT_ASSERT(...) contract_assert (__VA_ARGS__)
#endif

#endif // !LUMEX_CORE_CONTRACTS_ASSERT_HPP
