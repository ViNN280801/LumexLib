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
 * @file LumexContractsViolation.hpp
 * @brief The enumerations `assertion_kind`, `evaluation_semantic` and
 * `detection_mode`, and the class `contract_violation` that a violation
 * handler receives.
 * @details The names, the enumerator values and the accessors follow
 * `<contracts>` of the C++26 draft ([support.contract], P2900):
 * `assertion_kind`
 * (`pre` 1, `post` 2, `assert` 3), `evaluation_semantic` (`ignore` 1,
 * `observe` 2, `enforce` 3, `quick_enforce` 4) and `detection_mode`
 * (`predicate_false` 1, `evaluation_exception` 2), and `contract_violation`
 * with `comment`, `detection_mode`, `is_terminating`, `kind`, `location` and
 * `semantic`. The types are the module's own on every standard and toolchain,
 * not aliases of the standard ones (no library ships `<contracts>` yet).
 * Differences from the standard class: it is a plain value that can be
 * constructed (the macros of this library create it) and copied, and its
 * `location` is the module's `source_location`. Only `assertion_kind::assert`
 * is produced by `LUMEX_CONTRACT_ASSERT`; `pre` and `post` exist so that a
 * violation reported by the compiler's own contracts converts into the same
 * class.
 *
 * A `contract_violation` converts from any object that has the six accessors
 * of the standard class (the `std::contracts::contract_violation` of a C++26
 * library, or another type of that shape) through an explicit template
 * constructor: the strings and the location are taken by value, so the result
 * does not depend on the lifetime of the source.
 */
#ifndef LUMEX_CORE_CONTRACTS_VIOLATION_HPP
#define LUMEX_CORE_CONTRACTS_VIOLATION_HPP

#include <cstdint>
#include <type_traits>
#include <utility>

#include "lumex/core/contracts/location/LumexContractsSourceLocation.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace contracts
{
/** @brief The syntactic form of the violated contract assertion. */
enum class assertion_kind : int
{
  pre = 1,
  post = 2,
  assert = 3
};

/** @brief How a contract assertion was evaluated. */
enum class evaluation_semantic : int
{
  ignore = 1,
  observe = 2,
  enforce = 3,
  quick_enforce = 4
};

/** @brief How a violation was found. */
enum class detection_mode : int
{
  /** The predicate evaluated to false. */
  predicate_false = 1,
  /** The evaluation of the predicate exited by an exception. */
  evaluation_exception = 2
};

/** @brief The enumerator name of `kind`; `"unknown"` for a stray value. */
LUMEX_CONSTEXPR_FUNCTION char const *
to_string (assertion_kind kind) LUMEX_NOEXCEPT
{
  return kind == assertion_kind::pre
             ? "pre"
             : (kind == assertion_kind::post
                    ? "post"
                    : (kind == assertion_kind::assert ? "assert" : "unknown"));
}

/** @brief The enumerator name of `semantic`; `"unknown"` for a stray value. */
LUMEX_CONSTEXPR_FUNCTION char const *
to_string (evaluation_semantic semantic) LUMEX_NOEXCEPT
{
  return semantic == evaluation_semantic::ignore
             ? "ignore"
             : (semantic == evaluation_semantic::observe
                    ? "observe"
                    : (semantic == evaluation_semantic::enforce
                           ? "enforce"
                           : (semantic == evaluation_semantic::quick_enforce
                                  ? "quick_enforce"
                                  : "unknown")));
}

/** @brief The enumerator name of `mode`; `"unknown"` for a stray value. */
LUMEX_CONSTEXPR_FUNCTION char const *
to_string (detection_mode mode) LUMEX_NOEXCEPT
{
  return mode == detection_mode::predicate_false
             ? "predicate_false"
             : (mode == detection_mode::evaluation_exception
                    ? "evaluation_exception"
                    : "unknown");
}

/**
 * @brief Whether `semantic` ends the program after the violation handler
 * returns ([basic.contract.eval]: enforce and quick-enforce are the
 * terminating semantics).
 */
LUMEX_CONSTEXPR_FUNCTION bool
is_terminating (evaluation_semantic semantic) LUMEX_NOEXCEPT
{
  return semantic == evaluation_semantic::enforce
         || semantic == evaluation_semantic::quick_enforce;
}

namespace detail
{
/** @brief Detects an object with the accessors of a contract violation. */
template <typename Violation, typename = void>
struct has_violation_accessors : std::false_type
{
};

template <typename Violation>
struct has_violation_accessors<
    Violation,
    typename std::enable_if<
        std::is_convertible<
            decltype (std::declval<Violation const &> ().comment ()),
            char const *>::value
        && std::is_convertible<
            decltype (std::declval<Violation const &> ().location ().line ()),
            std::uint_least32_t>::value
        && std::is_integral<typename std::underlying_type<
            decltype (std::declval<Violation const &> ().kind ())>::type>::
            value
        && std::is_integral<typename std::underlying_type<
            decltype (std::declval<Violation const &> ().semantic ())>::type>::
            value
        && std::is_integral<typename std::underlying_type<
            decltype (std::declval<Violation const &> ()
                          .detection_mode ())>::type>::value>::type>
    : std::true_type
{
};
} // namespace detail

/**
 * @class contract_violation
 * @brief What a violation handler is told about a violated contract
 * assertion.
 */
class contract_violation
{
public:
  /**
   * @brief A violation of the given properties.
   * @param comment The text of the predicate (or any text; empty when none);
   * static storage, not copied. Null is taken as empty.
   */
  LUMEX_CONSTEXPR_CTOR
  contract_violation (char const *comment, assertion_kind kind,
                      evaluation_semantic semantic,
                      contracts::detection_mode mode,
                      source_location const &where) LUMEX_NOEXCEPT
      : m_comment (comment != nullptr ? comment : ""),
        m_kind (kind),
        m_semantic (semantic),
        m_mode (mode),
        m_location (where)
  {
  }

  /**
   * @brief A violation taken from an object with the accessors of the
   * standard `std::contracts::contract_violation`.
   * @details The enumerators are converted by value (the standard fixes
   * them), the location through its accessors.
   */
  template <typename Violation,
            typename = typename std::enable_if<
                detail::has_violation_accessors<Violation>::value
                && !std::is_same<typename std::decay<Violation>::type,
                                 contract_violation>::value>::type>
  explicit contract_violation (Violation const &other) LUMEX_NOEXCEPT
      : m_comment (other.comment () != nullptr ? other.comment () : ""),
        m_kind (static_cast<assertion_kind> (other.kind ())),
        m_semantic (static_cast<evaluation_semantic> (other.semantic ())),
        m_mode (
            static_cast<contracts::detection_mode> (other.detection_mode ())),
        m_location (
            other.location ().file_name (), other.location ().function_name (),
            static_cast<std::uint_least32_t> (other.location ().line ()),
            static_cast<std::uint_least32_t> (other.location ().column ()))
  {
  }

  /** @brief The text of the predicate; never null. */
  LUMEX_CONSTEXPR_FUNCTION char const *
  comment () const LUMEX_NOEXCEPT
  {
    return m_comment;
  }

  /** @brief How the violation was found. */
  LUMEX_CONSTEXPR_FUNCTION contracts::detection_mode
  detection_mode () const LUMEX_NOEXCEPT
  {
    return m_mode;
  }

  /** @brief Whether the evaluation semantic terminates the program. */
  LUMEX_CONSTEXPR_FUNCTION bool
  is_terminating () const LUMEX_NOEXCEPT
  {
    return contracts::is_terminating (m_semantic);
  }

  /** @brief The syntactic form of the violated assertion. */
  LUMEX_CONSTEXPR_FUNCTION contracts::assertion_kind
  kind () const LUMEX_NOEXCEPT
  {
    return m_kind;
  }

  /** @brief The place of the violated assertion. */
  LUMEX_CONSTEXPR_FUNCTION contracts::source_location const &
  location () const LUMEX_NOEXCEPT
  {
    return m_location;
  }

  /** @brief The semantic the assertion was evaluated with. */
  LUMEX_CONSTEXPR_FUNCTION contracts::evaluation_semantic
  semantic () const LUMEX_NOEXCEPT
  {
    return m_semantic;
  }

private:
  char const *m_comment;
  contracts::assertion_kind m_kind;
  contracts::evaluation_semantic m_semantic;
  contracts::detection_mode m_mode;
  contracts::source_location m_location;
};
} // namespace contracts
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_CONTRACTS_VIOLATION_HPP
