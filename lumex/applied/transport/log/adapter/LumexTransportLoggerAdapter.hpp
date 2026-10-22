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
 * @file LumexTransportLoggerAdapter.hpp
 * @brief The adapter from the transport log to the `LumexLogger` file logger:
 * `install_lumex_logger_sink` and the `lumex_logger_watch_timer`.
 * @details Compiled into `lumex::transport` only when the logger module is
 * built (`LUMEX_BUILD_LOGGER`); the library then defines
 * `LUMEX_TRANSPORT_WITH_LOGGER=1` for itself and its consumers and links
 * `lumex::logger`. Without the logger the transport builds as before and this
 * header is not installed.
 *
 * `install_lumex_logger_sink` makes the `LumexLogger` singleton the sink of
 * this binary's transport log (the slot is per binary, see
 * `LumexTransportLog.hpp`): every record becomes one logger line
 * `[component instance] message | dump` at the logger level of the same name
 * (trace TRACE, debug DEBUG, info INFO, warning WARNING, error ERROR). The
 * transport threshold follows the logger: it is set from the logger's state
 * at once and again after every change of it (a re-read of the trigger file,
 * `LumexLogger::set_log_level`), through a level change callback of the
 * logger (`LumexLogger::add_level_change_callback`), so a record the logger
 * would drop is not formatted. A disabled logger (no trigger file) sets the
 * threshold to `off`.
 *
 * Each binary that links the static transport library installs the sink in
 * its own slot and registers its own callback; the logger keeps up to 8
 * (spec G6). `uninstall_lumex_logger_sink` gives the slot and the callback
 * back; a shared library that installed the sink calls it before it is
 * unloaded.
 *
 * The logger notices a new trigger file only when something checks it: the
 * lazy check runs in `LumexLogger::log()` and `is_level_enabled()`, and with
 * the threshold at `off` the transport calls neither (a record below the
 * threshold never reaches the sink). A program that logs only through the
 * transport therefore needs a `trigger_file_watcher` (logger module, own
 * thread) or a `lumex_logger_watch_timer` (below, on an `io_runner`) to
 * notice the trigger file appearing.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_LOG_ADAPTER_LOGGER_ADAPTER_HPP
#define LUMEX_APPLIED_TRANSPORT_LOG_ADAPTER_LOGGER_ADAPTER_HPP

#include <chrono>
#include <memory>

#include "lumex/applied/transport/stream/LumexIoRunner.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace applied
{
namespace transport
{
namespace log
{
/**
 * @brief Makes the `LumexLogger` singleton the sink of this binary's
 * transport log and lets the transport threshold follow the logger's state.
 * @details Sets the sink, then adds this binary's level change callback to
 * the logger, which sets the threshold from the logger's current state
 * before it returns. Calling it again changes nothing. A record that the
 * logger cannot write (an I/O error) is dropped; the transport operation
 * that logged goes on.
 *
 * While the logger is disabled the threshold is `off` and no transport
 * record calls the logger, so nothing in a program that logs only through
 * the transport would check the trigger file: such a program also creates a
 * `trigger_file_watcher` or a `lumex_logger_watch_timer`.
 * @return True when the threshold follows the logger; false when the
 * logger's 8 callback entries are taken by other binaries: the sink is
 * installed and the threshold is set once from the logger's current state,
 * but does not follow later changes.
 * @throws std::runtime_error when the logger singleton cannot be created.
 */
bool install_lumex_logger_sink ();

/**
 * @brief Gives the sink slot back (default sink) when the logger sink is in
 * it, and removes the level change callback when it is the adapter's.
 * @details The threshold keeps its last value.
 */
void uninstall_lumex_logger_sink () LUMEX_NOEXCEPT;

/**
 * @brief A timer on an `io_runner` that calls `LumexLogger::force_reread()`
 * every period: the trigger file is re-read without a thread of its own.
 * @details The first check runs one period after the start. The check runs
 * in a thread of the runner and may block it for the time of one re-read
 * (a file stat, and on a change the read of the trigger file and the open or
 * close of the log). The destructor stops the timer and waits for a check
 * that is running: after it returns, no check of this timer runs. Destroy
 * the timer before its runner, as the transports. A level change callback
 * that a check of this timer runs (in the runner's thread) must not destroy
 * the timer: the destructor would wait for that same check and deadlock.
 * Move-only.
 */
class lumex_logger_watch_timer
{
public:
  /**
   * @brief Starts the timer on `runner`'s context.
   * @details Creates the `LumexLogger` singleton first, so that it outlives
   * a timer that is a static object.
   * @param runner The runner whose threads run the checks.
   * @param period Time between two checks (at least 1 ms; a smaller value is
   * raised to 1 ms).
   * @throws std::runtime_error when the logger singleton cannot be created.
   */
  explicit lumex_logger_watch_timer (stream::io_runner &runner,
                                     std::chrono::milliseconds period
                                     = std::chrono::milliseconds (1000));

  /// @brief Stops the timer (see the class).
  ~lumex_logger_watch_timer ();

  /// @brief Takes the timer of `other`, which then has none.
  lumex_logger_watch_timer (lumex_logger_watch_timer &&other) LUMEX_NOEXCEPT;

  /// @brief Stops the own timer, then takes the timer of `other`.
  lumex_logger_watch_timer &
  operator= (lumex_logger_watch_timer &&other) LUMEX_NOEXCEPT;

  lumex_logger_watch_timer (lumex_logger_watch_timer const &) = delete;
  lumex_logger_watch_timer &operator= (lumex_logger_watch_timer const &)
      = delete;

  /// @brief True while the object owns a running timer.
  bool is_running () const LUMEX_NOEXCEPT;

  /// @brief The time between two checks; zero once moved from.
  std::chrono::milliseconds period () const LUMEX_NOEXCEPT;

private:
  struct state_t;

  /// @brief Waits one period, then checks and waits again (the caller holds
  /// the state's mutex).
  static void arm (std::shared_ptr<state_t> const &state);

  /// @brief Stops the timer (idempotent).
  void stop () LUMEX_NOEXCEPT;

  std::shared_ptr<state_t> state_;
};
} // namespace log
} // namespace transport
} // namespace applied
} // namespace lumex

#endif // !LUMEX_APPLIED_TRANSPORT_LOG_ADAPTER_LOGGER_ADAPTER_HPP
