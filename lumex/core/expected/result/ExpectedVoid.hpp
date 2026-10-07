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

/**
 * @file ExpectedVoid.hpp
 * @brief The specialization `expected<void, ErrorType>` for operations that
 * succeed without a value; an analogue of C++23 `std::expected<void, E>`.
 * @details It keeps the interface of the primary template where it applies to
 * an absent value: `has_value()`, `value()` (which only checks the state and
 * throws `bad_expected_access` on an error), `error()`, `error_or()`,
 * `emplace()`, `emplace_error()`, `swap()` and the monadic operations. The
 * header includes `Expected.hpp`, so it is enough for both forms.
 */
#ifndef LUMEX_CORE_EXPECTED_RESULT_EXPECTED_VOID_HPP
#define LUMEX_CORE_EXPECTED_RESULT_EXPECTED_VOID_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#if __has_warning("-Wnrvo")
#pragma clang diagnostic ignored "-Wnrvo"
#endif
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#if __has_warning("-Wvariadic-macro-arguments-omitted")
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#endif
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#include "Expected.hpp"
#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

// ====================== Specialization for expected<void, ErrorType>
// ======================

namespace lumex
{
namespace core
{
namespace expected
{
namespace result
{
/**
 * @brief `expected` specialization when the success value is absent (`void`).
 * @details Lets `expected` be used for functions that either succeed
 *          without returning a value, or return an error.
 *          It is the analogue of `std::expected<void, E>` from C++23.
 * @tparam ErrorType Error type.
 * @note Here `has_value()` means success without a returned value,
 *       and `!has_value()` means the object holds an error.
 * @note Works from C++11. As in the primary template, a member that is not
 * const, returns `void` or needs more than one statement is marked
 * `LUMEX_CONSTEXPR_CXX14`: `constexpr` from C++14, an ordinary function at
 * C++11.
 */
template <typename ErrorType> class expected<void, ErrorType>
{
public:
  // ====================== Static assertions ====================== //
  LUMEX_STATIC_ASSERT_MSG (!std::is_reference<ErrorType>::value,
                           "Expected<void,E>: E must not be a reference");
  LUMEX_STATIC_ASSERT_MSG (!std::is_function<ErrorType>::value,
                           "Expected<void,E>: E must not be a function type");

  // ====================== Aliases ====================== //
  using value_type = void;
  using error_type = ErrorType;
  using unexpected_type = unexpected<ErrorType>;

  /**
   * @brief Alias that rebases expected to another success value.
   * @tparam U New success value type.
   * @note This alias simplifies type conversions in user code and tests.
   */
  template <typename U> using rebind = expected<U, ErrorType>;

  // ====================== Constructors ====================== //

  /**
   * @brief Default constructor (successful void state)
   * @details Creates an `expected` in the success state (`m_has_value =
   * true`), holding no value (`SuccessType` is `void`).
   * @note Guaranteed not to throw (`noexcept`).
   */
  LUMEX_CONSTEXPR_CTOR
  expected () LUMEX_NOEXCEPT : m_has_value (true) {}

  /**
   * @brief Copy constructor for the `expected<void, ErrorType>`
   * specialization.
   * @details Creates a new `expected` by copying state and, if `other` holds
   * an error, copies that error. If `other` is in the success state (`void`),
   *          the new object is also in the success state.
   * @param[in] other `expected<void, ErrorType>` object copied from.
   * @note Not declared `noexcept`.
   * @throws May throw if the copy constructor of `ErrorType` throws.
   */
  LUMEX_CONSTEXPR_CXX14
  expected (expected const &other) : m_has_value (other.m_has_value)
  {
    if (!m_has_value)
      { // If it holds an error, copy it
        new (std::addressof (m_storage.m_error))
            ErrorType (other.m_storage.m_error);
      }
  }

  /**
   * @brief Move constructor for `expected<void, ErrorType>` specialization.
   * @details Creates a new `expected` by moving the state and, if `other`
   * holds an error, moves that error. If `other` is in the success state
   * (`void`), the new object is also in the success state. After the call,
   * `other` remains valid but unspecified.
   * @param[in] other `expected<void, ErrorType>` object moved from.
   * @note This constructor is conditionally `noexcept` if the `ErrorType` move
   * constructor does not throw.
   * @throws May throw if the `ErrorType` move constructor throws.
   */
  LUMEX_CONSTEXPR_CXX14
  expected (expected &&other)
      LUMEX_NOEXCEPT_IF (std::is_nothrow_move_constructible<ErrorType>::value)
      : m_has_value (other.m_has_value)
  {
    if (!m_has_value)
      { // If it holds an error, move it
        new (std::addressof (m_storage.m_error))
            ErrorType (std::move (other.m_storage.m_error));
      }
  }

  /**
   * @brief In-place constructor for the successful void state
   * @details Creates an `expected` in the success state (`m_has_value =
   * true`). This constructor states that the object must be created in the
   * success state with no value, using the `in_place_tag` tag.
   * @note Guaranteed not to throw (`noexcept`).
   */
  LUMEX_CONSTEXPR_CTOR explicit expected (in_place_tag /* unused */)
      LUMEX_NOEXCEPT : m_has_value (true)
  {
  }

  /**
   * @brief Constructs the error in place via unexpect (void specialization).
   */
  template <typename... Args>
  LUMEX_CONSTEXPR_CXX14 explicit expected (unexpect_t /*unused*/,
                                           Args &&...args)
      : m_has_value (false)
  {
    new (std::addressof (m_storage.m_error))
        ErrorType (std::forward<Args> (args)...);
  }

  /**
   * @brief Constructor from `unexpected` (copy) for the `expected<void,
   * ErrorType>` specialization.
   * @details Creates an `expected` in the error state (`m_has_value = false`),
   * copying the error from `unexp`. Used when `expected` must hold an error
   * and has no success value (`SuccessType` is `void`).
   * @tparam Err Error type convertible to `ErrorType`.
   * @param[in] unexp Const reference to an `unexpected` that holds an error.
   * @note Participates in SFINAE to avoid conflicts.
   * @throws May throw if the copy constructor of `ErrorType` throws.
   */
  template <
      typename Err = ErrorType,
      // SFINAE: enable only if ErrorType is constructible from Err
      typename = typename std::enable_if<
          std::is_constructible<ErrorType, Err const &>::value
          && !std::is_same<typename std::decay<Err>::type, in_place_tag>::value
          && !std::is_same<typename std::decay<Err>::type,
                           unexpect_t>::value>::type>
  LUMEX_CONSTEXPR_CXX14 explicit expected (unexpected<Err> const &unexp)
      : m_has_value (false)
  {
    new (std::addressof (m_storage.m_error)) ErrorType (unexp.error ());
  }

  /**
   * @brief Constructs an `expected<void, ErrorType>` in the error state from
   * an `unexpected` rvalue.
   * @details Move-constructs the stored error from `unexp.error()`.
   * @param[in] unexp The `unexpected` whose error is moved from.
   * @note This constructor is conditionally `noexcept` if the `ErrorType` move
   * constructor does not throw.
   * @throws May throw if the `ErrorType` move constructor throws.
   */
  template <
      typename Err = ErrorType,
      typename = typename std::enable_if<
          std::is_constructible<ErrorType, Err &&>::value
          && !std::is_same<typename std::decay<Err>::type, in_place_tag>::value
          && !std::is_same<typename std::decay<Err>::type,
                           unexpect_t>::value>::type>
  LUMEX_CONSTEXPR_CXX14 explicit expected (unexpected<Err> &&unexp)
      LUMEX_NOEXCEPT_IF (std::is_nothrow_move_constructible<ErrorType>::value)
      : m_has_value (false)
  {
    new (std::addressof (m_storage.m_error))
        ErrorType (std::move (unexp).error ());
  }

  /**
   * @brief Destructor for the `expected<void, ErrorType>` specialization.
   * @details Destroys the stored error if the object is in the error state.
   *          If the object is in the success state, nothing is done.
   * @note Does not throw (`noexcept`) if the `ErrorType` destructor does not
   * throw.
   */
  LUMEX_CONSTEXPR_DTOR ~expected () LUMEX_NOEXCEPT
  {
    if (!m_has_value)
      { // If it holds an error, destroy it
        m_storage.m_error.~ErrorType ();
      }
  }

  // ====================== Assignment Operators ====================== //
  /**
   * @brief Copy assignment for `expected<void, ErrorType>` specialization.
   * @details Assigns another `expected` into this object.
   *          Uses copy-and-swap for the strong exception
   *          guarantee).
   * @param[in] other `expected<void, ErrorType>` object assigned from.
   * @return Reference to this object `expected<void, ErrorType>`
   * specialization.
   * @note Conditionally `noexcept` if move/assignment constructors and
   * operators of `ErrorType` do not throw.
   * @throws May throw if the copy constructor of `expected` or `std::swap`
   * throw.
   */
  LUMEX_CONSTEXPR_CXX14 expected &
  operator= (expected const &other) LUMEX_NOEXCEPT_IF (
      std::is_nothrow_move_constructible<ErrorType>::value
          &&std::is_nothrow_move_assignable<ErrorType>::value)
  {
    expected temp (other);
    swap (temp);
    return *this;
  }

  /**
   * @brief Move assignment for `expected<void, ErrorType>` specialization.
   * @details Assigns another `expected` into this object by move.
   *          Uses `swap` to exchange resources without extra allocations.
   * @param[in] other `expected<void, ErrorType>` object moved from.
   * @return Reference to this object `expected<void, ErrorType>`
   * specialization.
   * @note Guaranteed not to throw (`noexcept`).
   */
  LUMEX_CONSTEXPR_CXX14 expected &
  operator= (expected &&other) LUMEX_NOEXCEPT
  {
    swap (other);
    return *this;
  }

  // ====================== Observers ======================
  /**
   * @brief Checks whether `expected<void, ErrorType>` is in the success (void)
   * state.
   * @return `true` if the object is in the success state, `false` otherwise
   * (error).
   * @note Does not throw. Marked `[[nodiscard]]` to ensure handling of
   * the returned value.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value indicates state; should always be used.")
  bool
  has_value () const LUMEX_NOEXCEPT
  {
    return m_has_value;
  }

  /**
   * @brief Explicit conversion to `bool` for `expected<void, ErrorType>`
   * specialization.
   * @details Lets `expected` be used where a condition is expected (for
   * example, `if (myExpected)`); elsewhere the conversion must be written out.
   * @return `true` if the object holds the success (void) state, `false`
   * otherwise.
   * @note Does not throw. Marked `[[nodiscard]]` to ensure handling of
   * the returned value.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value indicates state; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION explicit
  operator bool () const LUMEX_NOEXCEPT
  {
    return m_has_value;
  }

  /**
   * @brief Checks for the success (void) state and throws if it is missing
   * (lvalue).
   * @details This function returns no value because the specialization is for
   * `void`. It checks that the `expected` object is in the success state, and
   * throws `bad_expected_access<ErrorType>` if it holds an error.
   * @warning Calling this while the object holds an error throws
   * `bad_expected_access<ErrorType>` specialization.
   * @throws bad_expected_access<ErrorType> if the object holds an error.
   * @note Does not throw in the success state.
   */
  LUMEX_CONSTEXPR_CXX14 void
  value () &
  {
    if (!m_has_value)
      throw bad_expected_access<ErrorType> (m_storage.m_error);
  }

  /**
   * @brief Checks for the success (void) state and throws if it is missing
   * (rvalue).
   * @details Same as the lvalue version, but for rvalue references. Used for
   * `expected` objects that are moved. If the object holds an error, it is
   * moved into the exception.
   * @warning Calling this while the object holds an error throws
   * `bad_expected_access<ErrorType>` specialization.
   * @throws bad_expected_access<ErrorType> if the object holds an error.
   * @note Does not throw in the success state.
   */
  LUMEX_CONSTEXPR_CXX14 void
  value () &&
  {
    if (!m_has_value)
      throw bad_expected_access<ErrorType> (std::move (m_storage.m_error));
  }

  /**
   * @brief Checks for the success (void) state and throws if it is missing
   * (const lvalue).
   * @details Same as the lvalue version, but for const lvalue references. Used
   * for `expected` objects whose state must not change. If the object holds an
   * error, it is copied into the exception.
   * @warning Calling this while the object holds an error throws
   * `bad_expected_access<ErrorType>` specialization.
   * @throws bad_expected_access<ErrorType> if the object holds an error.
   * @note Does not throw in the success state.
   */
  LUMEX_CONSTEXPR_CXX14 void
  value () const &
  {
    if (!m_has_value)
      throw bad_expected_access<ErrorType> (m_storage.m_error);
  }

  /**
   * @brief Checks for the success (void) state and throws if it is missing
   * (const rvalue).
   * @details Same as the rvalue version, but for const rvalue references. Used
   * for `expected` objects that are moved and whose state must not change. If
   * the object holds an error, it is copied into the exception, because a
   * const error cannot be moved from.
   * @warning Calling this while the object holds an error throws
   * `bad_expected_access<ErrorType>` specialization.
   * @throws bad_expected_access<ErrorType> if the object holds an error.
   * @note Does not throw in the success state.
   */
  LUMEX_CONSTEXPR_CXX14 void
  value () const &&
  {
    if (!m_has_value)
      throw bad_expected_access<ErrorType> (std::move (m_storage.m_error));
  }

  /**
   * @brief Returns a mutable lvalue reference to the stored error.
   * @pre !has_value()
   * @warning Calling it without an error violates the precondition:
   * `LUMEX_ASSERT`, active in every build including `NDEBUG`, aborts the
   * program.
   * @details This function returns a mutable reference to the error when
   * `expected` is in the error state. It never throws: in the success state
   * the assertion aborts the program (see the warning).
   * @return Reference to the `ErrorType` error.
   * @note Use `[[nodiscard]]` to ensure handling of the returned value.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained error; should always be used.")
  LUMEX_CONSTEXPR_CXX14 ErrorType &
  error () &
  {
    LUMEX_ASSERT (
        !m_has_value
        && "error() called on an Expected<void> that holds no error");
    return m_storage.m_error;
  }

  /**
   * @brief Returns a const lvalue reference to the stored error.
   * @pre !has_value()
   * @warning Calling it without an error violates the precondition:
   * `LUMEX_ASSERT`, active in every build including `NDEBUG`, aborts the
   * program.
   * @details This function returns a const reference to the error when
   * `expected` is in the error state. It never throws: in the success state
   * the assertion aborts the program (see the warning).
   * @return Const reference to the `ErrorType` error.
   * @note Use `[[nodiscard]]` to ensure handling of the returned value.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained error; should always be used.")
  LUMEX_CONSTEXPR_CXX14 ErrorType const &
  error () const &
  {
    LUMEX_ASSERT (
        !m_has_value
        && "error() called on an Expected<void> that holds no error");
    return m_storage.m_error;
  }

  /**
   * @brief Returns an rvalue reference to the stored error.
   * @pre !has_value()
   * @warning Calling it without an error violates the precondition:
   * `LUMEX_ASSERT`, active in every build including `NDEBUG`, aborts the
   * program.
   * @details This function returns an rvalue reference to the error when
   * `expected` is in the error state, so the caller can move the error out.
   * It never throws: in the success state the assertion aborts the program
   * (see the warning).
   * @return Rvalue reference to the `ErrorType` error.
   * @note Use `[[nodiscard]]` to ensure handling of the returned value.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained error; should always be used.")
  LUMEX_CONSTEXPR_CXX14 ErrorType &&
  error () &&
  {
    LUMEX_ASSERT (
        !m_has_value
        && "error() called on an Expected<void> that holds no error");
    return std::move (m_storage.m_error);
  }

  /**
   * @brief Returns a const rvalue reference to the stored error.
   * @pre !has_value()
   * @warning Calling it without an error violates the precondition:
   * `LUMEX_ASSERT`, active in every build including `NDEBUG`, aborts the
   * program.
   * @details This function returns a const rvalue reference to the error when
   * `expected` is in the error state; being const, it cannot be moved from.
   * It never throws: in the success state the assertion aborts the program
   * (see the warning).
   * @return Const rvalue reference to the `ErrorType` error.
   * @note Use `[[nodiscard]]` to ensure handling of the returned value.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Return value is the contained error; should always be used.")
  LUMEX_CONSTEXPR_CXX14 ErrorType const &&
  error () const &&
  {
    LUMEX_ASSERT (
        !m_has_value
        && "error() called on an Expected<void> that holds no error");
    return std::move (m_storage.m_error);
  }

  /**
   * @brief Returns the stored error if present, otherwise the default error.
   * @tparam U Default-value type; must be convertible to `ErrorType`.
   * @param[in] default_error Error returned if the object is in the success
   * state.
   * @return The stored error or `default_error`.
   * @note The method itself does not throw, but converting/copying/moving `U
   * -> ErrorType` may throw.
   * @note Use `[[nodiscard]]` to ensure handling of the returned value.
   */
  template <typename U>
  LUMEX_ATTRIBUTE_NODISCARD ("Return value is the contained error or a "
                             "default; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION ErrorType error_or (U &&default_error) const &
  {
    return !m_has_value
               ? m_storage.m_error
               : static_cast<ErrorType> (std::forward<U> (default_error));
  }

  /**
   * @brief Returns the error if present, otherwise the given default; rvalue
   * overload.
   * @details For temporaries: if an error is present it is moved out;
   * otherwise a copy/move constructed from `default_error` is returned.
   *
   * @tparam G Default-argument type (defaults to `ErrorType`).
   * @param default_error Default error value.
   * @return ErrorType
   *
   * @note This overload avoids an extra copy of the real error by moving it.
   * @par Exception guarantees
   *      May throw if copy/move of `ErrorType` or converting `G -> ErrorType`
   * can throw.
   */
  template <typename G = ErrorType>
  LUMEX_ATTRIBUTE_NODISCARD ("Return value is the error or a "
                             "default-constructed substitute; should be used.")
  LUMEX_CONSTEXPR_CXX14 ErrorType error_or (G &&default_error) &&
  {
    if (!m_has_value)
      return std::move (m_storage.m_error);
    return static_cast<ErrorType> (std::forward<G> (default_error));
  }

  /**
   * @brief Dereference operator (lvalue) for `expected<void, ErrorType>`
   * specialization.
   * @details Confirms the success state and returns no value.
   * @warning Assumes `expected` is in the success state. If not,
   * `LUMEX_ASSERT`, active in every build including `NDEBUG`, aborts the
   * program.
   * @note This function does not throw, but requires a prior `has_value()`
   * check.
   */
  LUMEX_CONSTEXPR_CXX14 void
      operator* ()
      & LUMEX_NOEXCEPT
  {
    LUMEX_ASSERT (
        m_has_value
        && "operator* called on an Expected<void> that holds an error");
  }

  /**
   * @brief Dereference operator (rvalue) for `expected<void, ErrorType>`
   * specialization.
   * @details Confirms the success state and returns no value.
   * @warning Assumes `expected` is in the success state. If not,
   * `LUMEX_ASSERT`, active in every build including `NDEBUG`, aborts the
   * program.
   * @note This function does not throw, but requires a prior `has_value()`
   * check.
   */
  LUMEX_CONSTEXPR_CXX14 void
      operator* ()
      && LUMEX_NOEXCEPT
  {
    LUMEX_ASSERT (
        m_has_value
        && "operator* called on an Expected<void> that holds an error");
  }

  /**
   * @brief Const dereference operator (lvalue) for `expected<void, ErrorType>`
   * specialization.
   * @details Confirms the success state and returns no value.
   * @warning Assumes `expected` is in the success state. If not,
   * `LUMEX_ASSERT`, active in every build including `NDEBUG`, aborts the
   * program.
   * @note This function does not throw, but requires a prior `has_value()`
   * check.
   */
  LUMEX_CONSTEXPR_CXX14 void
  operator* () const &LUMEX_NOEXCEPT
  {
    LUMEX_ASSERT (
        m_has_value
        && "operator* called on an Expected<void> that holds an error");
  }

  /**
   * @brief Const dereference operator (rvalue) for `expected<void, ErrorType>`
   * specialization.
   * @details Confirms the success state and returns no value.
   * @warning Assumes `expected` is in the success state. If not,
   * `LUMEX_ASSERT`, active in every build including `NDEBUG`, aborts the
   * program.
   * @note This function does not throw, but requires a prior `has_value()`
   * check.
   */
  LUMEX_CONSTEXPR_CXX14 void
  operator* () const &&LUMEX_NOEXCEPT
  {
    LUMEX_ASSERT (
        m_has_value
        && "operator* called on an Expected<void> that holds an error");
  }

  // operator->() is not applicable to expected<void>

  // ====================== Modifiers ======================
  /**
   * @brief Constructs the success "void" state in place, destroying the
   * current contents of `expected<void, ErrorType>` specialization.
   * @details This function move-assigns `expected(in_place)` to this object,
   * which swaps the contents; a previous error is destroyed together with the
   * temporary. The object is then in the success state with no value. This
   * is `emplace` for the `void` specialization.
   * @note Not declared `noexcept`, but it does not throw: the `in_place`
   * constructor and the move assignment are both `noexcept`.
   */
  LUMEX_CONSTEXPR_CXX14 void
  emplace ()
  {
    *this = expected (in_place);
  }

  /**
   * @brief Constructs an `ErrorType` in place, destroying the current contents
   * of `expected<void, ErrorType>` specialization.
   * @details This function first destroys the current stored state (if it was
   * an error), then constructs a new `ErrorType` error in place from the
   * forwarded arguments.
   * @tparam Args Argument types for the `ErrorType` constructor.
   * @param[in] args Arguments forwarded to the `ErrorType` constructor.
   * @return Reference to the newly constructed `ErrorType` error.
   * @note May throw if the `ErrorType` constructor throws. On exception
   *       the `expected` object may be left in an invalid state.
   */
  template <typename... Args>
  LUMEX_CONSTEXPR_CXX14 ErrorType &
  emplace_error (Args &&...args)
  {
    if (!m_has_value)
      {
        // If an error was already stored, destroy it before reconstructing
        m_storage.m_error.~ErrorType ();
      }
    new (std::addressof (m_storage.m_error)) ErrorType (std::forward<Args> (
        args)...);       // Construct the error in place (placement new)
    m_has_value = false; // The object is now in the error state (no value)
    return m_storage.m_error;
  }

  /**
   * @brief Exchanges contents with another `expected<void, ErrorType>`
   * specialization.
   * @details Swaps the `m_has_value` flag and, as needed, the error contents
   * with the other `expected`. If both are success, nothing happens. If both
   * hold an error, `std::swap` is used. If one is success and the other holds
   * an error, contents are moved so both objects change state.
   * @param[in,out] other The other `expected<void, ErrorType>` to swap with.
   * @note The noexcept guarantee depends on
   * `std::is_nothrow_move_constructible` and, from C++17,
   * `std::is_nothrow_swappable` for `ErrorType`.
   * @throws May throw if move constructors or `std::swap` of
   *         `ErrorType` throw.
   */
  LUMEX_CONSTEXPR_CXX14 void
  swap (expected &other)
#if __cplusplus >= 201703L
      LUMEX_NOEXCEPT_IF (std::is_nothrow_move_constructible<ErrorType>::value
                             &&std::is_nothrow_swappable<ErrorType>::value)
#else
      LUMEX_NOEXCEPT_IF (std::is_nothrow_move_constructible<ErrorType>::value)
#endif
  {
    if (this == &other)
      return;

    if (m_has_value && other.m_has_value)
      {
        // Both hold void success; nothing to do
      }
    else if (!m_has_value && !other.m_has_value)
      { // Both hold an error
        std::swap (m_storage.m_error, other.m_storage.m_error);
      }
    else
      { // One holds void success, the other an error. Move is required.
        if (m_has_value)
          { // this holds void success, other holds an error
            new (std::addressof (m_storage.m_error))
                ErrorType (std::move (other.m_storage.m_error));
            other.m_storage.m_error.~ErrorType ();
            // other now holds void success
          }
        else
          { // this holds an error, other holds void success
            new (std::addressof (other.m_storage.m_error))
                ErrorType (std::move (m_storage.m_error));
            m_storage.m_error.~ErrorType ();
            // this now holds void success
          }
        std::swap (m_has_value, other.m_has_value);
      }
  }

  // ====================== Monadic Operations ======================
  /**
   * @brief Applies `func` with no arguments to the success state if present
   * (lvalue).
   * @details If `expected` is in the success (void) state, `func` is called
   * with no arguments, and the result of `func` is returned. `func` must
   * return `expected<U, ErrorType>` specialization. If `expected` holds an
   * error, `func` is not called, and a new `expected` holding the current
   * error is returned.
   * @tparam FunctionType Function type that takes `void` (no arguments) and
   * returns `expected<U, ErrorType>` specialization.
   * @param[in] func Function to apply.
   * @return `expected<U, ErrorType>` holding the result of `func` or the
   * current error.
   * @note This overload lets `and_then` be used on lvalue `expected<void,
   * ErrorType>` objects.
   * @throws May throw if `func` throws or the constructor
   * of `expected` from an error throws.
   */
#if LUMEX_HAS_STD_CONCEPTS
  template <typename FunctionType>
    requires std::invocable<FunctionType>
             && lumex::core::utility::traits::value::is_expected_concept<
                 std::invoke_result_t<FunctionType>>
  LUMEX_CONSTEXPR_CXX14 auto
  and_then (FunctionType func) & -> std::invoke_result_t<FunctionType>
  {
    if (m_has_value)
      return func ();
    return std::invoke_result_t<FunctionType> (
        unexpected<ErrorType> (m_storage.m_error));
  }
#else
  template <typename FunctionType,
            typename ResultOfFunc = typename std::result_of<
                FunctionType ()>::type, // Func is called with no arguments
            typename = typename std::enable_if<
                lumex::core::utility::traits::value::is_expected<
                    ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_CXX14 auto
  and_then (FunctionType func) & -> ResultOfFunc
  {
    if (m_has_value)
      return func ();
    return ResultOfFunc (unexpected<ErrorType> (m_storage.m_error));
  }
#endif

  /**
   * @brief Applies `func` with no arguments to the success state (const
   * lvalue) if present.
   * @details If `expected` is in the success (void) state, `func` is called
   * with no arguments, and the result of `func` is returned. `func` must
   * return `expected<U, ErrorType>` specialization. If `expected` holds an
   * error, `func` is not called, and a new `expected` holding the current
   * error is returned.
   * @tparam FunctionType Function type that takes `void` (no arguments) and
   * returns `expected<U, ErrorType>` specialization.
   * @param[in] func Function to apply.
   * @return `expected<U, ErrorType>` holding the result of `func` or the
   * current error.
   * @note This overload lets `and_then` be used on const lvalue
   * `expected<void, ErrorType>` objects.
   * @throws May throw if `func` throws or the constructor
   * of `expected` from an error throws.
   */
#if LUMEX_HAS_STD_CONCEPTS
  template <typename FunctionType>
    requires std::invocable<FunctionType>
             && lumex::core::utility::traits::value::is_expected_concept<
                 std::invoke_result_t<FunctionType>>
  LUMEX_CONSTEXPR_CXX14 auto
  and_then (FunctionType func) const & -> std::invoke_result_t<FunctionType>
  {
    if (m_has_value)
      return func ();
    return std::invoke_result_t<FunctionType> (
        unexpected<ErrorType> (m_storage.m_error));
  }
#else
  template <
      typename FunctionType,
      typename ResultOfFunc = typename std::result_of<FunctionType ()>::type,
      typename
      = typename std::enable_if<lumex::core::utility::traits::value::
                                    is_expected<ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_CXX14 auto
  and_then (FunctionType func) const & -> ResultOfFunc
  {
    if (m_has_value)
      return func ();
    return ResultOfFunc (unexpected<ErrorType> (m_storage.m_error));
  }
#endif

  /**
   * @brief Applies `func` with no arguments to the success state (rvalue) if
   * present.
   * @details If `expected` is in the success (void) state, `func` is called
   * with no arguments, and the result of `func` is returned. `func` must
   * return `expected<U, ErrorType>` specialization. If `expected` holds an
   * error, `func` is not called, and a new `expected` holding the current
   * error is returned.
   * @tparam FunctionType Function type that takes `void` (no arguments) and
   * returns `expected<U, ErrorType>` specialization.
   * @param[in] func Function to apply.
   * @return `expected<U, ErrorType>` holding the result of `func` or the
   * current error.
   * @note This overload lets `and_then` be used on rvalue `expected` objects,
   * providing move semantics.
   * @throws May throw if `func` throws or the constructor
   * of `expected` from an error throws.
   */
#if LUMEX_HAS_STD_CONCEPTS
  template <typename FunctionType>
    requires std::invocable<FunctionType>
             && lumex::core::utility::traits::value::is_expected_concept<
                 std::invoke_result_t<FunctionType>>
  LUMEX_CONSTEXPR_CXX14 auto
  and_then (FunctionType func) && -> std::invoke_result_t<FunctionType>
  {
    if (m_has_value)
      return func ();
    return std::invoke_result_t<FunctionType> (
        unexpected<ErrorType> (std::move (m_storage.m_error)));
  }
#else
  template <
      typename FunctionType,
      typename ResultOfFunc = typename std::result_of<FunctionType ()>::type,
      typename
      = typename std::enable_if<lumex::core::utility::traits::value::
                                    is_expected<ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_CXX14 auto
  and_then (FunctionType func) && -> ResultOfFunc
  {
    if (m_has_value)
      return func ();
    return ResultOfFunc (
        unexpected<ErrorType> (std::move (m_storage.m_error)));
  }
#endif

  /**
   * @brief Applies `func` with no arguments to the success state (const
   * rvalue) if present.
   * @details If `expected` is in the success (void) state, `func` is called
   * with no arguments, and the result of `func` is returned. `func` must
   * return `expected<U, ErrorType>` specialization. If `expected` holds an
   * error, `func` is not called, and a new `expected` holding the current
   * error is returned.
   * @tparam FunctionType Function type that takes `void` (no arguments) and
   * returns `expected<U, ErrorType>` specialization.
   * @param[in] func Function to apply.
   * @return `expected<U, ErrorType>` holding the result of `func` or the
   * current error.
   * @note This overload lets `and_then` be used on const rvalue `expected`
   * objects.
   * @throws May throw if `func` throws or the constructor
   * of `expected` from an error throws.
   */
#if LUMEX_HAS_STD_CONCEPTS
  template <typename FunctionType>
    requires std::invocable<FunctionType>
             && lumex::core::utility::traits::value::is_expected_concept<
                 std::invoke_result_t<FunctionType>>
  LUMEX_CONSTEXPR_CXX14 auto
  and_then (FunctionType func) const && -> std::invoke_result_t<FunctionType>
  {
    if (m_has_value)
      return func ();
    return std::invoke_result_t<FunctionType> (
        unexpected<ErrorType> (std::move (m_storage.m_error)));
  }
#else
  template <
      typename FunctionType,
      typename ResultOfFunc = typename std::result_of<FunctionType ()>::type,
      typename
      = typename std::enable_if<lumex::core::utility::traits::value::
                                    is_expected<ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_CXX14 auto
  and_then (FunctionType func) const && -> ResultOfFunc
  {
    if (m_has_value)
      return func ();
    return ResultOfFunc (
        unexpected<ErrorType> (std::move (m_storage.m_error)));
  }
#endif

  /**
   * @brief Applies `func` with no arguments to the success state (lvalue) if
   * present, and transforms it into a new `expected` with a non-void result.
   * @details If `expected<void, ErrorType>` is in the success state, `func` is
   * called with no arguments, and its non-void result constructs a new
   * `expected<ResultOfFunc, ErrorType>` specialization. If `expected` holds an
   * error, `func` is not called and the current error is forwarded into a new
   * `expected<ResultOfFunc, ErrorType>` specialization.
   * @tparam FunctionType Function type that takes `void` (no arguments) and
   * returns `ResultOfFunc` (not `void`).
   * @tparam ResultOfFunc Return type of `func`.
   * @param[in] func Function to apply.
   * @return `expected<ResultOfFunc, ErrorType>` holding the transformed value
   * or the current error.
   * @note This overload lets `transform` be used on lvalue `expected<void,
   * ErrorType>` objects to produce an `expected` with a concrete value.
   * @throws May throw if `func` throws or the constructor
   *         `expected` from a value/error throws.
   */
#if LUMEX_HAS_STD_CONCEPTS
  template <typename FunctionType>
    requires std::invocable<FunctionType>
             && (!std::is_void_v<std::invoke_result_t<FunctionType>>)
             && (!lumex::core::utility::traits::value::is_expected_concept<
                 std::invoke_result_t<FunctionType>>)
  LUMEX_CONSTEXPR_CXX14 auto
  transform (FunctionType func)
      & -> expected<std::invoke_result_t<FunctionType>, ErrorType>
  {
    if (m_has_value)
      return expected<std::invoke_result_t<FunctionType>, ErrorType> (in_place,
                                                                      func ());
    return expected<std::invoke_result_t<FunctionType>, ErrorType> (
        unexpected<ErrorType> (m_storage.m_error));
  }
#else
  template <
      typename FunctionType,
      typename ResultOfFunc = typename std::result_of<FunctionType ()>::type,
      typename ReturnType = expected<ResultOfFunc, ErrorType>,
      typename
      = typename std::enable_if<!std::is_void<ResultOfFunc>::value
                                && !lumex::core::utility::traits::value::
                                       is_expected<ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_CXX14 auto
  transform (FunctionType func) & -> ReturnType
  {
    if (m_has_value)
      return ReturnType (in_place, func ());
    return ReturnType (unexpected<ErrorType> (m_storage.m_error));
  }
#endif

  /**
   * @brief Applies `func` with no arguments to the success state (lvalue) if
   * present, and transforms it into a new `expected` with a void result.
   * @details If `expected<void, ErrorType>` is in the success state, `func` is
   * called with no arguments. Its void result (no value) is used to create a
   * new `expected<void, ErrorType>` in the success state. If `expected` holds
   * an error, `func` is not called and the current error is forwarded into a
   * new `expected<void, ErrorType>` specialization.
   * @tparam FunctionType Function type that takes `void` (no arguments) and
   * returns `void`.
   * @param[in] func Function to apply.
   * @return `expected<void, ErrorType>` holding the success (void) state or
   * the current error.
   * @note This overload lets `transform` be used on lvalue `expected<void,
   * ErrorType>` objects to keep `expected` in the `void` specialization.
   * @throws May throw if `func` throws or the constructor
   *         of `expected` from an error throws.
   */
#if LUMEX_HAS_STD_CONCEPTS
  template <typename FunctionType>
    requires std::invocable<FunctionType>
             && std::is_void_v<std::invoke_result_t<FunctionType>>
  LUMEX_CONSTEXPR_CXX14 auto
  transform (FunctionType func) & -> expected<void, ErrorType>
  {
    if (m_has_value)
      {
        func ();
        return expected<void, ErrorType> (in_place);
      }
    return expected<void, ErrorType> (
        unexpected<ErrorType> (m_storage.m_error));
  }
#else
  template <
      typename FunctionType, typename ReturnType = expected<void, ErrorType>,
      typename = typename std::enable_if<std::is_void<typename std::result_of<
          FunctionType ()>::type>::value>::type> // If func returns void
  LUMEX_CONSTEXPR_CXX14 auto
  transform (
      FunctionType func) & -> ReturnType // transform into expected<void, E>
  {
    if (m_has_value)
      {
        func ();
        return ReturnType (in_place);
      }
    return ReturnType (unexpected<ErrorType> (m_storage.m_error));
  }
#endif

  /**
   * @brief Applies `func` with no arguments and transforms the result into a
   * new `expected`.
   * @details Used when `func` returns a non-void value.
   *          If the object is in the success state (`m_has_value == true`),
   *          `func()` is called and the result is packed into a new
   * `expected<ResultOfFunc, ErrorType>` specialization. If the object holds an
   * error, an `expected` holding the current error is returned.
   *
   * @tparam FunctionType Type of the called function (no arguments).
   * @tparam ResultOfFunc Type returned by `func`.
   * @tparam ReturnType Final return type (`expected<ResultOfFunc,
   * ErrorType>`).
   * @return A new `expected` holding the result of `func` or the current
   * error.
   *
   * @note This overload is for lvalue references.
   * @throws May throw if `func` throws.
   */
#if LUMEX_HAS_STD_CONCEPTS
  template <typename FunctionType>
    requires std::invocable<FunctionType>
             && (!std::is_void_v<std::invoke_result_t<FunctionType>>)
             && (!lumex::core::utility::traits::value::is_expected_concept<
                 std::invoke_result_t<FunctionType>>)
  LUMEX_CONSTEXPR_CXX14 auto
  transform (FunctionType func)
      const & -> expected<std::invoke_result_t<FunctionType>, ErrorType>
  {
    if (m_has_value)
      return expected<std::invoke_result_t<FunctionType>, ErrorType> (in_place,
                                                                      func ());
    return expected<std::invoke_result_t<FunctionType>, ErrorType> (
        unexpected<ErrorType> (m_storage.m_error));
  }
#else
  template <
      typename FunctionType,
      typename ResultOfFunc = typename std::result_of<FunctionType ()>::type,
      typename ReturnType = expected<ResultOfFunc, ErrorType>,
      typename
      = typename std::enable_if<!std::is_void<ResultOfFunc>::value
                                && !lumex::core::utility::traits::value::
                                       is_expected<ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_CXX14 auto
  transform (FunctionType func) const & -> ReturnType
  {
    if (m_has_value)
      return ReturnType (in_place, func ());
    return ReturnType (unexpected<ErrorType> (m_storage.m_error));
  }
#endif

  /**
   * @brief Applies `func` with no arguments returning `void`, and transforms
   * the result into `expected<void, ErrorType>` specialization.
   * @details If the object is in the success state, `func()` is called and
   * `expected<void, ErrorType>` in the success state is returned. If the
   * object holds an error, an `expected` holding that error is returned.
   *
   * @tparam FunctionType Type of the called function (no arguments) that
   * returns `void`.
   * @tparam ReturnType Final return type (`expected<void, ErrorType>`).
   * @return `expected<void, ErrorType>` indicating success or an error.
   *
   * @note This overload is used for functions that return no value.
   */
#if LUMEX_HAS_STD_CONCEPTS
  template <typename FunctionType>
    requires std::invocable<FunctionType>
             && std::is_void_v<std::invoke_result_t<FunctionType>>
  LUMEX_CONSTEXPR_CXX14 auto
  transform (FunctionType func) const & -> expected<void, ErrorType>
  {
    if (m_has_value)
      {
        func ();
        return expected<void, ErrorType> (in_place);
      }
    return expected<void, ErrorType> (
        unexpected<ErrorType> (m_storage.m_error));
  }
#else
  template <typename FunctionType,
            typename ReturnType = expected<void, ErrorType>,
            typename = typename std::enable_if<std::is_void<
                typename std::result_of<FunctionType ()>::type>::value>::type>
  LUMEX_CONSTEXPR_CXX14 auto
  transform (FunctionType func) const & -> ReturnType
  {
    if (m_has_value)
      {
        func ();
        return ReturnType (in_place);
      }
    return ReturnType (unexpected<ErrorType> (m_storage.m_error));
  }
#endif

  /**
   * @brief `transform` overload for an rvalue reference and functions that
   * return a non-void value.
   * @details Lets `func()` run on temporary `expected` objects. If a value is
   * present, the result is wrapped in `expected<ResultOfFunc, ErrorType>`
   * specialization. If the object holds an error, an `expected` holding it
   * is returned.
   *
   * @tparam FunctionType Type of the called function.
   * @tparam ResultOfFunc Type returned by `func`.
   * @tparam ReturnType Final return type.
   * @return A new `expected` holding the result of `func` or the current
   * error.
   */
#if LUMEX_HAS_STD_CONCEPTS
  template <typename FunctionType>
    requires std::invocable<FunctionType>
             && (!std::is_void_v<std::invoke_result_t<FunctionType>>)
             && (!lumex::core::utility::traits::value::is_expected_concept<
                 std::invoke_result_t<FunctionType>>)
  LUMEX_CONSTEXPR_CXX14 auto
  transform (FunctionType func)
      && -> expected<std::invoke_result_t<FunctionType>, ErrorType>
  {
    if (m_has_value)
      return expected<std::invoke_result_t<FunctionType>, ErrorType> (in_place,
                                                                      func ());
    return expected<std::invoke_result_t<FunctionType>, ErrorType> (
        unexpected<ErrorType> (std::move (m_storage.m_error)));
  }
#else
  template <
      typename FunctionType,
      typename ResultOfFunc = typename std::result_of<FunctionType ()>::type,
      typename ReturnType = expected<ResultOfFunc, ErrorType>,
      typename
      = typename std::enable_if<!std::is_void<ResultOfFunc>::value
                                && !lumex::core::utility::traits::value::
                                       is_expected<ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_CXX14 auto
  transform (FunctionType func) && -> ReturnType
  {
    if (m_has_value)
      return ReturnType (in_place, func ());
    return ReturnType (unexpected<ErrorType> (std::move (m_storage.m_error)));
  }
#endif

  /**
   * @brief `transform` overload for an rvalue reference and functions that
   * return `void`.
   * @details If the object is in the success state, `func()` is called and
   *          `expected<void, ErrorType>` in the success state is returned.
   * Otherwise the error is returned.
   *
   * @tparam FunctionType Type of the called function.
   * @tparam ReturnType Final return type.
   * @return A new `expected` indicating success or an error.
   */
#if LUMEX_HAS_STD_CONCEPTS
  template <typename FunctionType>
    requires std::invocable<FunctionType>
             && std::is_void_v<std::invoke_result_t<FunctionType>>
  LUMEX_CONSTEXPR_CXX14 auto
  transform (FunctionType func) && -> expected<void, ErrorType>
  {
    if (m_has_value)
      {
        func ();
        return expected<void, ErrorType> (in_place);
      }
    return expected<void, ErrorType> (
        unexpected<ErrorType> (std::move (m_storage.m_error)));
  }
#else
  template <typename FunctionType,
            typename ReturnType = expected<void, ErrorType>,
            typename = typename std::enable_if<std::is_void<
                typename std::result_of<FunctionType ()>::type>::value>::type>
  LUMEX_CONSTEXPR_CXX14 auto
  transform (FunctionType func) && -> ReturnType
  {
    if (m_has_value)
      {
        func ();
        return ReturnType (in_place);
      }
    return ReturnType (unexpected<ErrorType> (std::move (m_storage.m_error)));
  }
#endif

  /**
   * @brief `transform` overload for a const rvalue reference and functions
   * that return a non-void value.
   * @details Lets temporary const objects be transformed. If `m_has_value ==
   * true`, `func()` is called and the result is wrapped in `expected`.
   * Otherwise the error is returned.
   *
   * @tparam FunctionType Type of the called function.
   * @tparam ResultOfFunc Result type of `func`.
   * @tparam ReturnType Final return type.
   * @return A new `expected` with the result of `func` or an error.
   */
#if LUMEX_HAS_STD_CONCEPTS
  template <typename FunctionType>
    requires std::invocable<FunctionType>
             && (!std::is_void_v<std::invoke_result_t<FunctionType>>)
             && (!lumex::core::utility::traits::value::is_expected_concept<
                 std::invoke_result_t<FunctionType>>)
  LUMEX_CONSTEXPR_CXX14 auto
  transform (FunctionType func)
      const && -> expected<std::invoke_result_t<FunctionType>, ErrorType>
  {
    if (m_has_value)
      return expected<std::invoke_result_t<FunctionType>, ErrorType> (in_place,
                                                                      func ());
    return expected<std::invoke_result_t<FunctionType>, ErrorType> (
        unexpected<ErrorType> (std::move (m_storage.m_error)));
  }
#else
  template <
      typename FunctionType,
      typename ResultOfFunc = typename std::result_of<FunctionType ()>::type,
      typename ReturnType = expected<ResultOfFunc, ErrorType>,
      typename
      = typename std::enable_if<!std::is_void<ResultOfFunc>::value
                                && !lumex::core::utility::traits::value::
                                       is_expected<ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_CXX14 auto
  transform (FunctionType func) const && -> ReturnType
  {
    if (m_has_value)
      return ReturnType (in_place, func ());
    return ReturnType (unexpected<ErrorType> (std::move (m_storage.m_error)));
  }
#endif

  /**
   * @brief `transform` overload for a const rvalue reference and functions
   * that return `void`.
   * @details If the object is in the success state, `func()` is called and
   *          `expected<void, ErrorType>` in the success state is returned. If
   * the object holds an error, an `expected` holding that error is returned.
   *
   * @tparam FunctionType Type of the called function.
   * @tparam ReturnType Final return type.
   * @return A new `expected` indicating success or an error.
   */
#if LUMEX_HAS_STD_CONCEPTS
  template <typename FunctionType>
    requires std::invocable<FunctionType>
             && std::is_void_v<std::invoke_result_t<FunctionType>>
  LUMEX_CONSTEXPR_CXX14 auto
  transform (FunctionType func) const && -> expected<void, ErrorType>
  {
    if (m_has_value)
      {
        func ();
        return expected<void, ErrorType> (in_place);
      }
    return expected<void, ErrorType> (
        unexpected<ErrorType> (std::move (m_storage.m_error)));
  }
#else
  template <typename FunctionType,
            typename ReturnType = expected<void, ErrorType>,
            typename = typename std::enable_if<std::is_void<
                typename std::result_of<FunctionType ()>::type>::value>::type>
  LUMEX_CONSTEXPR_CXX14 auto
  transform (FunctionType func) const && -> ReturnType
  {
    if (m_has_value)
      {
        func ();
        return ReturnType (in_place);
      }
    return ReturnType (unexpected<ErrorType> (std::move (m_storage.m_error)));
  }
#endif

  /**
   * @brief Handles the error if present by calling `func`.
   * @details If the object holds an error, `func(error)` is called and its
   * result is returned. If the object is in the success state, an empty
   * success `expected` is returned.
   *
   * @tparam FunctionType Function type that takes a reference to `ErrorType`.
   * @tparam ResultOfFunc Function result type.
   * @return Result of `func` or an empty `expected`.
   */
#if LUMEX_HAS_STD_CONCEPTS
  template <typename FunctionType>
    requires std::invocable<FunctionType, ErrorType &>
             && lumex::core::utility::traits::value::is_expected_concept<
                 std::invoke_result_t<FunctionType, ErrorType &>>
  LUMEX_CONSTEXPR_CXX14 auto
  or_else (
      FunctionType func) & -> std::invoke_result_t<FunctionType, ErrorType &>
  {
    if (m_has_value)
      return std::invoke_result_t<FunctionType, ErrorType &> (in_place);
    return func (m_storage.m_error);
  }
#else
  template <typename FunctionType,
            typename ResultOfFunc
            = typename std::result_of<FunctionType (ErrorType &)>::type,
            typename = typename std::enable_if<
                lumex::core::utility::traits::value::is_expected<
                    ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_CXX14 auto
  or_else (FunctionType func) & -> ResultOfFunc
  {
    if (m_has_value)
      return ResultOfFunc (in_place);
    return func (m_storage.m_error);
  }
#endif

  /**
   * @brief Handles the error (const lvalue) if present.
   * @details If the object holds an error, `func(const ErrorType&)` is called.
   *          On success an empty `expected` is returned.
   *
   * @tparam FunctionType Function type.
   * @tparam ResultOfFunc Result type.
   * @return Result of `func` or an empty `expected`.
   */
#if LUMEX_HAS_STD_CONCEPTS
  template <typename FunctionType>
    requires std::invocable<FunctionType, const ErrorType &>
             && lumex::core::utility::traits::value::is_expected_concept<
                 std::invoke_result_t<FunctionType, const ErrorType &>>
  LUMEX_CONSTEXPR_CXX14 auto
  or_else (FunctionType func)
      const & -> std::invoke_result_t<FunctionType, const ErrorType &>
  {
    if (m_has_value)
      return std::invoke_result_t<FunctionType, ErrorType const &> (in_place);
    return func (m_storage.m_error);
  }
#else
  template <typename FunctionType,
            typename ResultOfFunc
            = typename std::result_of<FunctionType (ErrorType const &)>::type,
            typename = typename std::enable_if<
                lumex::core::utility::traits::value::is_expected<
                    ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_CXX14 auto
  or_else (FunctionType func) const & -> ResultOfFunc
  {
    if (m_has_value)
      return ResultOfFunc (in_place);
    return func (m_storage.m_error);
  }
#endif

  /**
   * @brief Handles the error (rvalue) if present.
   * @details If the object holds an error, `func(ErrorType&&)` is called.
   *          On success an empty `expected` is returned.
   *
   * @tparam FunctionType Function type.
   * @tparam ResultOfFunc Result type.
   * @return Result of `func` or an empty `expected`.
   */
#if LUMEX_HAS_STD_CONCEPTS
  template <typename FunctionType>
    requires std::invocable<FunctionType, ErrorType &&>
             && lumex::core::utility::traits::value::is_expected_concept<
                 std::invoke_result_t<FunctionType, ErrorType &&>>
  LUMEX_CONSTEXPR_CXX14 auto
  or_else (
      FunctionType func) && -> std::invoke_result_t<FunctionType, ErrorType &&>
  {
    if (m_has_value)
      return std::invoke_result_t<FunctionType, ErrorType &&> (in_place);
    return func (std::move (m_storage.m_error));
  }
#else
  template <typename FunctionType,
            typename ResultOfFunc
            = typename std::result_of<FunctionType (ErrorType &&)>::type,
            typename = typename std::enable_if<
                lumex::core::utility::traits::value::is_expected<
                    ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_CXX14 auto
  or_else (FunctionType func) && -> ResultOfFunc
  {
    if (m_has_value)
      return ResultOfFunc (in_place);
    return func (std::move (m_storage.m_error));
  }
#endif

  /**
   * @brief Handles the error (const rvalue) if present.
   * @details If the object holds an error, `func(const ErrorType&&)` is
   * called. On success an empty `expected` is returned.
   *
   * @tparam FunctionType Function type.
   * @tparam ResultOfFunc Result type.
   * @return Result of `func` or an empty `expected`.
   */
#if LUMEX_HAS_STD_CONCEPTS
  template <typename FunctionType>
    requires std::invocable<FunctionType, const ErrorType &&>
             && lumex::core::utility::traits::value::is_expected_concept<
                 std::invoke_result_t<FunctionType, const ErrorType &&>>
  LUMEX_CONSTEXPR_CXX14 auto
  or_else (FunctionType func)
      const && -> std::invoke_result_t<FunctionType, const ErrorType &&>
  {
    if (m_has_value)
      return std::invoke_result_t<FunctionType, ErrorType const &&> (in_place);
    return func (std::move (m_storage.m_error));
  }
#else
  template <typename FunctionType,
            typename ResultOfFunc
            = typename std::result_of<FunctionType (ErrorType const &&)>::type,
            typename = typename std::enable_if<
                lumex::core::utility::traits::value::is_expected<
                    ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_CXX14 auto
  or_else (FunctionType func) const && -> ResultOfFunc
  {
    if (m_has_value)
      return ResultOfFunc (in_place);
    return func (std::move (m_storage.m_error));
  }
#endif

  /**
   * @brief Transforms the error if present by applying `func`.
   * @details If the object holds an error, `func(error)` is called and a new
   *          `expected<void, ResultOfFunc>` is returned. If the object is in
   * the success state, `expected<void, ResultOfFunc>` without an error is
   * returned.
   *
   * @tparam FunctionType Function type that takes `ErrorType`.
   * @tparam ResultOfFunc Function result type.
   * @tparam ReturnType Resulting type `expected<void, ResultOfFunc>`.
   * @return A new `expected` with the transformed error, or without one.
   */
#if LUMEX_HAS_STD_CONCEPTS
  template <typename FunctionType>
    requires std::invocable<FunctionType, ErrorType &>
             && (!std::is_void_v<
                 std::invoke_result_t<FunctionType, ErrorType &>>)
             && (!lumex::core::utility::traits::value::is_expected_concept<
                 std::invoke_result_t<FunctionType, ErrorType &>>)
  LUMEX_CONSTEXPR_CXX14 auto
  transform_error (FunctionType func)
      & -> expected<void, std::invoke_result_t<FunctionType, ErrorType &>>
  {
    if (m_has_value)
      return expected<void, std::invoke_result_t<FunctionType, ErrorType &>> (
          in_place);
    return expected<void, std::invoke_result_t<FunctionType, ErrorType &>> (
        unexpected<std::invoke_result_t<FunctionType, ErrorType &>> (
            func (m_storage.m_error)));
  }
#else
  template <typename FunctionType,
            typename ResultOfFunc
            = typename std::result_of<FunctionType (ErrorType &)>::type,
            typename ReturnType = expected<void, ResultOfFunc>,
            typename = typename std::enable_if<
                !std::is_void<ResultOfFunc>::value
                && !lumex::core::utility::traits::value::is_expected<
                    ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_CXX14 auto
  transform_error (FunctionType func) & -> ReturnType
  {
    if (m_has_value)
      return ReturnType (in_place);
    return ReturnType (unexpected<ResultOfFunc> (func (m_storage.m_error)));
  }
#endif

  /**
   * @brief Transforms the error (const lvalue).
   * @details If the object holds an error, `func(const ErrorType&)` is called.
   *          If the object is in the success state, `expected<void,
   * ResultOfFunc>` without an error is returned.
   */
#if LUMEX_HAS_STD_CONCEPTS
  template <typename FunctionType>
    requires std::invocable<FunctionType, const ErrorType &>
             && (!std::is_void_v<
                 std::invoke_result_t<FunctionType, const ErrorType &>>)
             && (!lumex::core::utility::traits::value::is_expected_concept<
                 std::invoke_result_t<FunctionType, const ErrorType &>>)
  LUMEX_CONSTEXPR_CXX14 auto
  transform_error (FunctionType func) const & -> expected<
      void, std::invoke_result_t<FunctionType, const ErrorType &>>
  {
    if (m_has_value)
      return expected<void,
                      std::invoke_result_t<FunctionType, ErrorType const &>> (
          in_place);
    return expected<void,
                    std::invoke_result_t<FunctionType, ErrorType const &>> (
        unexpected<std::invoke_result_t<FunctionType, ErrorType const &>> (
            func (m_storage.m_error)));
  }
#else
  template <typename FunctionType,
            typename ResultOfFunc
            = typename std::result_of<FunctionType (ErrorType const &)>::type,
            typename ReturnType = expected<void, ResultOfFunc>,
            typename = typename std::enable_if<
                !std::is_void<ResultOfFunc>::value
                && !lumex::core::utility::traits::value::is_expected<
                    ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_CXX14 auto
  transform_error (FunctionType func) const & -> ReturnType
  {
    if (m_has_value)
      return ReturnType (in_place);
    return ReturnType (unexpected<ResultOfFunc> (func (m_storage.m_error)));
  }
#endif

  /**
   * @brief Transforms the error (rvalue).
   * @details If the object holds an error, `func(ErrorType&&)` is called.
   *          If the object is in the success state, `expected<void,
   * ResultOfFunc>` without an error is returned.
   */
#if LUMEX_HAS_STD_CONCEPTS
  template <typename FunctionType>
    requires std::invocable<FunctionType, ErrorType &&>
             && (!std::is_void_v<
                 std::invoke_result_t<FunctionType, ErrorType &&>>)
             && (!lumex::core::utility::traits::value::is_expected_concept<
                 std::invoke_result_t<FunctionType, ErrorType &&>>)
  LUMEX_CONSTEXPR_CXX14 auto
  transform_error (FunctionType func)
      && -> expected<void, std::invoke_result_t<FunctionType, ErrorType &&>>
  {
    if (m_has_value)
      return expected<void, std::invoke_result_t<FunctionType, ErrorType &&>> (
          in_place);
    return expected<void, std::invoke_result_t<FunctionType, ErrorType &&>> (
        unexpected<std::invoke_result_t<FunctionType, ErrorType &&>> (
            func (std::move (m_storage.m_error))));
  }
#else
  template <typename FunctionType,
            typename ResultOfFunc
            = typename std::result_of<FunctionType (ErrorType &&)>::type,
            typename ReturnType = expected<void, ResultOfFunc>,
            typename = typename std::enable_if<
                !std::is_void<ResultOfFunc>::value
                && !lumex::core::utility::traits::value::is_expected<
                    ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_CXX14 auto
  transform_error (FunctionType func) && -> ReturnType
  {
    if (m_has_value)
      return ReturnType (in_place);
    return ReturnType (
        unexpected<ResultOfFunc> (func (std::move (m_storage.m_error))));
  }
#endif

  /**
   * @brief Transforms the error (const rvalue).
   * @details If the object holds an error, `func(const ErrorType&&)` is
   * called. If the object is in the success state, `expected<void,
   * ResultOfFunc>` without an error is returned.
   */
#if LUMEX_HAS_STD_CONCEPTS
  template <typename FunctionType>
    requires std::invocable<FunctionType, const ErrorType &&>
             && (!std::is_void_v<
                 std::invoke_result_t<FunctionType, const ErrorType &&>>)
             && (!lumex::core::utility::traits::value::is_expected_concept<
                 std::invoke_result_t<FunctionType, const ErrorType &&>>)
  LUMEX_CONSTEXPR_CXX14 auto
  transform_error (FunctionType func) const && -> expected<
      void, std::invoke_result_t<FunctionType, const ErrorType &&>>
  {
    if (m_has_value)
      return expected<void,
                      std::invoke_result_t<FunctionType, ErrorType const &&>> (
          in_place);
    return expected<void,
                    std::invoke_result_t<FunctionType, ErrorType const &&>> (
        unexpected<std::invoke_result_t<FunctionType, ErrorType const &&>> (
            func (std::move (m_storage.m_error))));
  }
#else
  template <typename FunctionType,
            typename ResultOfFunc
            = typename std::result_of<FunctionType (ErrorType const &&)>::type,
            typename ReturnType = expected<void, ResultOfFunc>,
            typename = typename std::enable_if<
                !std::is_void<ResultOfFunc>::value
                && !lumex::core::utility::traits::value::is_expected<
                    ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_CXX14 auto
  transform_error (FunctionType func) const && -> ReturnType
  {
    if (m_has_value)
      return ReturnType (in_place);
    return ReturnType (
        unexpected<ResultOfFunc> (func (std::move (m_storage.m_error))));
  }
#endif

private:
  /**
   * @brief Internal union that stores `expected<void, ErrorType>` state.
   * @details The union saves memory because only one member is active at a
   * time: a dummy `Unit` object for the success state with no value, or an
   * `ErrorType` error. The active-member lifetime is managed manually by the
   * outer class.
   *
   * @note Copy/move constructors and assignment operators are deleted because
   *       ownership and lifetime of the active member are controlled by the
   * wrapper class.
   * @note The destructor does not throw; destroying the active member is done
   *       from outside (RAII is provided by the outer class).
   * @warning Accessing an inactive union member is undefined behavior.
   *          Check the state flag (`m_has_value` on the outer class) before
   * access.
   * @par Thread safety
   *       Not thread-safe without external synchronization. Concurrent access
   * to different union branches is not allowed.
   * @par Performance
   *       Using a `union` avoids extra allocations and reduces storage
   * overhead.
   * @par Exception guarantees
   *       The union itself does not throw; exceptions can occur only when
   * constructing/ destroying the active member, which the outer class does.
   */
  union Storage
  {
    /**
     * @brief Dummy used to mark the success state with no value.
     * @details Used only as a marker; its contents are never read.
     * @note Its default member initializer makes it the active member when
     * `Storage` is constructed.
     */
    Unit m_dummy{}; // Dummy for the success state

    /**
     * @brief Stored error value in the failure state.
     * @details Active only when the outer object is in the error state.
     * @warning May be used only while the error state is active.
     */
    ErrorType m_error;

    /**
     * @brief Default constructor.
     * @details Initializes only `m_dummy`, through its default member
     * initializer; constructing `m_error` in its place is done by the outer
     * class.
     * @note Guaranteed not to throw.
     */
    Storage () {}

    /**
     * @brief Destructor.
     * @details Intentionally empty; the outer class destroys the active member
     * manually, based on the current state.
     * @note Guaranteed not to throw.
     */
    ~Storage () {}

    /**
     * @brief Deleted copy constructor.
     * @details Copying `Storage` is forbidden to avoid double management of
     *          the active member lifetime.
     */
    Storage (Storage const &) = delete;

    /**
     * @brief Deleted copy assignment.
     * @details Assignment is forbidden for the same reasons as the copy
     * constructor.
     */
    Storage &operator= (Storage const &) = delete;

    /**
     * @brief Deleted move constructor.
     * @details Move is forbidden because transferring the active member is
     * done explicitly by the outer class.
     */
    Storage (Storage &&) = delete;

    /**
     * @brief Deleted move assignment.
     * @details Forbidden to prevent uncontrolled switching of the active
     * member.
     */
    Storage &operator= (Storage &&) = delete;
  } m_storage; ///< @brief Internal storage instance; the outer class selects
               ///< the active member from state.

  /**
   * @brief Flag: `true` if `expected` holds a success value, `false` if it
   * holds an error.
   * @details This member tells whether `m_storage.m_error` is active, and
   * therefore whether an `ErrorType` must be constructed or destroyed.
   */
  bool m_has_value;
};

} // namespace result
} // namespace expected
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_EXPECTED_RESULT_EXPECTED_VOID_HPP
