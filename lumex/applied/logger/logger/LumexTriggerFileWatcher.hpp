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
 * @file LumexTriggerFileWatcher.hpp
 * @brief `trigger_file_watcher`: a thread of the application that checks the
 * trigger file of the `LumexLogger` singleton periodically.
 * @details Without a watcher the logger checks its trigger file lazily, from
 * `log()` and `is_level_enabled()`, at most once per 2 s; a program that
 * logs nothing for a while (or a transport whose threshold filters every
 * record) then sees a new trigger file only at its next log call. A watcher
 * calls `LumexLogger::force_reread()` every period from its own thread, so a
 * change is applied within one period.
 *
 * The watcher is an RAII object: its constructor starts the thread, its
 * destructor stops and joins it, so the thread lives exactly as long as the
 * object. The singleton owns no thread. Create the watcher in `main` (or in
 * an object `main` owns), not as a static object of a shared library: a
 * thread joined while the loader lock is held (static destruction of a DLL)
 * would deadlock. A program that runs an `io_runner` of the transports can
 * use `lumex_logger_watch_timer` of the transport adapter instead, which needs
 * no thread of its own.
 */
#ifndef LUMEX_APPLIED_LOGGER_LOGGER_TRIGGER_FILE_WATCHER_HPP
#define LUMEX_APPLIED_LOGGER_LOGGER_TRIGGER_FILE_WATCHER_HPP

#include "lumex/LumexExport.hpp"

#include <chrono>
#include <cstddef>

#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
namespace logger
{
namespace logger
{
/**
 * @brief A thread that calls `LumexLogger::force_reread()` every period.
 * @details Move-only. A moved-from watcher owns no thread. Like
 * `std::thread`, one watcher object is used from one thread at a time.
 */
class LUMEX_API trigger_file_watcher final
{
public:
  /**
   * @brief Creates the `LumexLogger` singleton (so that it outlives a
   * watcher that is a static object), then starts the thread.
   * @param period Time between two checks (at least 1 ms; a smaller value is
   * raised to 1 ms). The first check runs one period after the start.
   * @throws std::runtime_error if the logger singleton cannot be created.
   * @throws std::system_error if the thread cannot be started.
   */
  explicit trigger_file_watcher (std::chrono::milliseconds period
                                 = std::chrono::milliseconds (1000));

  /**
   * @brief Stops the thread and joins it.
   * @note Must not run in the logger's level change callback (that callback
   * runs in the watcher's thread).
   */
  ~trigger_file_watcher ();

  /// @brief Takes the thread of `other`, which then owns none.
  trigger_file_watcher (trigger_file_watcher &&other) LUMEX_NOEXCEPT;

  /// @brief Stops the own thread, then takes the thread of `other`.
  trigger_file_watcher &
  operator= (trigger_file_watcher &&other) LUMEX_NOEXCEPT;

  trigger_file_watcher (trigger_file_watcher const &) = delete;
  trigger_file_watcher &operator= (trigger_file_watcher const &) = delete;

  /// @brief True while the object owns a thread (false once moved from).
  bool is_running () const LUMEX_NOEXCEPT;

  /// @brief The time between two checks; zero once moved from.
  std::chrono::milliseconds period () const LUMEX_NOEXCEPT;

  /**
   * @brief The number of watcher threads of the process that have started
   * and not yet finished.
   * @details Counted from before the thread starts to the end of its
   * function: after the destructor of the last watcher returns, the count is
   * back where it was. For tests and diagnostics.
   */
  static std::size_t running_threads () LUMEX_NOEXCEPT;

private:
  struct state_t;

  /// @brief Stops and joins the thread and releases the state (idempotent).
  void _stop () LUMEX_NOEXCEPT;

  state_t *watcher_state; ///< Thread and its stop flag; null once moved from
};
} // namespace logger
} // namespace logger
} // namespace applied
} // namespace lumex

#endif // !LUMEX_APPLIED_LOGGER_LOGGER_TRIGGER_FILE_WATCHER_HPP
