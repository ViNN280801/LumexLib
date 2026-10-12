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
 * @file LumexContractsHandler.hpp
 * @brief The contract-violation handler: `set_violation_handler`,
 * `get_violation_handler`, `invoke_violation_handler`,
 * `invoke_default_violation_handler` and `scoped_violation_handler`.
 * @details The handler is a plain function pointer
 * (`void (*) (contract_violation const &)`) kept in a process-wide
 * `lumex::core::utility::callback::lumex_callback_slot`. The slot lives in the
 * compiled library `lumex::contracts` and is reached through the exported free
 * functions below only, so an executable and every shared library it loads see
 * one handler. `set_violation_handler` and `get_violation_handler` are
 * thread-safe (an atomic pointer, release and acquire); a violation reads the
 * pointer once and calls it, so a handler that is replaced while another
 * thread reports a violation is called by the old or by the new one, never by
 * a torn value. The handler itself must be safe to call from any thread.
 *
 * Without an installed handler (`nullptr`) a violation goes to the default
 * handler. It prints one line to standard error ("file:line:column: function:
 * contract violation: ...") and returns, as [basic.contract.handler]
 * recommends for the default handler of the standard; the `enforce` semantic
 * then ends the program, the `observe` semantic continues. A handler may
 * throw: the exception propagates out of the violated `LUMEX_CONTRACT_ASSERT`
 * (in a `noexcept` function this terminates, [basic.contract.eval]). The
 * library never catches it, unlike `lumex_callback_slot::invoke_or`.
 *
 * A violation that is reported while the same thread is inside a handler
 * (the handler itself violates a contract assertion) goes to the default
 * handler, so a faulty handler cannot recurse without end.
 */
#ifndef LUMEX_CORE_CONTRACTS_HANDLER_HPP
#define LUMEX_CORE_CONTRACTS_HANDLER_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/core/contracts/violation/LumexContractsViolation.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace contracts
{
/** @brief The type of a contract-violation handler. */
using violation_handler_type = void (*) (contract_violation const &);

/**
 * @brief Installs `handler` and returns the previously installed one.
 * @param handler The new handler; `nullptr` selects the default handler.
 * @return The previous handler, `nullptr` when the default one was in use.
 * @note Thread-safe.
 */
LUMEX_CONTRACTS_API violation_handler_type
set_violation_handler (violation_handler_type handler) LUMEX_NOEXCEPT;

/**
 * @brief The installed handler, `nullptr` when the default handler is in use.
 * @note Thread-safe.
 */
LUMEX_ATTRIBUTE_NODISCARD ("the installed handler is the result")
LUMEX_CONTRACTS_API violation_handler_type get_violation_handler ()
    LUMEX_NOEXCEPT;

/**
 * @brief Reports `violation` to the installed handler, or to the default
 * handler when none is installed (or when this thread is already inside a
 * handler).
 * @details This is what the semantics `observe` and `enforce` call; it can be
 * called directly to report a violation that a program found by other means.
 * @throws Whatever the installed handler throws.
 */
LUMEX_CONTRACTS_API void
invoke_violation_handler (contract_violation const &violation);

/**
 * @brief The default handler: writes one line about `violation` to standard
 * error and returns.
 * @details Named after
 * `std::contracts::invoke_default_contract_violation_handler`
 * ([support.contract.invoke]); a user handler can call it to add to the
 * default report. Does not allocate, does not throw.
 */
LUMEX_CONTRACTS_API void invoke_default_violation_handler (
    contract_violation const &violation) LUMEX_NOEXCEPT;

/**
 * @class scoped_violation_handler
 * @brief Installs a handler for the lifetime of the object and restores the
 * previous one on destruction.
 * @note Replacing the process-wide handler is not scoped to a thread: other
 * threads see it too.
 */
class scoped_violation_handler
{
public:
  /** @brief Installs `handler` (`nullptr`: the default handler). */
  explicit scoped_violation_handler (violation_handler_type handler)
      LUMEX_NOEXCEPT : m_previous (set_violation_handler (handler))
  {
  }

  ~scoped_violation_handler () { set_violation_handler (m_previous); }

  scoped_violation_handler (scoped_violation_handler const &) = delete;
  scoped_violation_handler &operator= (scoped_violation_handler const &)
      = delete;

private:
  violation_handler_type m_previous;
};
} // namespace contracts
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_CONTRACTS_HANDLER_HPP
