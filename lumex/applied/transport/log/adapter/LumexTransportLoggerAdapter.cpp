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

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>

#include "lumex/applied/logger/logger/LumexLogger.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/log/LumexTransportLog.hpp"
#include "lumex/applied/transport/log/adapter/LumexTransportLoggerAdapter.hpp"

#if defined(__clang__)
// The library's clang-cl set: -Wall there is -Weverything (as in the other
// modules' sources).
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
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

namespace lumex
{
namespace applied
{
namespace transport
{
namespace log
{
namespace
{
using lumex::applied::logger::logger::LogLevel;
using file_logger = lumex::applied::logger::logger::LumexLogger;

/// The logger level of a transport record.
LogLevel
to_logger_level (log_level level) LUMEX_NOEXCEPT
{
  switch (level)
    {
    case log_level::trace:
      return LogLevel::LEVEL_TRACE;
    case log_level::debug:
      return LogLevel::LEVEL_DEBUG;
    case log_level::info:
      return LogLevel::LEVEL_INFO;
    case log_level::warning:
      return LogLevel::LEVEL_WARNING;
    case log_level::error:
    case log_level::off:
    default:
      return LogLevel::LEVEL_ERROR;
    }
}

/**
 * @brief The transport threshold for a logger state: the lowest transport
 * level the logger writes. SUCCESS lies between info and warning (warning);
 * FATAL is above every transport level (off), as is a disabled logger.
 */
log_level
to_threshold (bool enabled, LogLevel level) LUMEX_NOEXCEPT
{
  if (!enabled)
    return log_level::off;
  switch (level)
    {
    case LogLevel::LEVEL_TRACE:
      return log_level::trace;
    case LogLevel::LEVEL_DEBUG:
      return log_level::debug;
    case LogLevel::LEVEL_INFO:
      return log_level::info;
    case LogLevel::LEVEL_SUCCESS:
    case LogLevel::LEVEL_WARNING:
      return log_level::warning;
    case LogLevel::LEVEL_ERROR:
      return log_level::error;
    case LogLevel::LEVEL_FATAL:
    default:
      return log_level::off;
    }
}

/// The sink: one logger line per record.
void
lumex_logger_sink (log_record_t const &record)
{
  try
    {
      std::string text = "[";
      text += record.component;
      if (record.instance[0] != '\0')
        {
          text += ' ';
          text += record.instance;
        }
      text += "] ";
      text += record.message;
      if (!record.bytes.empty ())
        {
          text += " | ";
          text += format_dump (record.bytes);
        }
      file_logger::get_instance ().log (to_logger_level (record.level), text);
    }
  catch (...) // NOLINT(bugprone-empty-catch)
    {
      // The log file failed: the transport operation that logged goes on.
    }
}

/// The level change callback: the threshold follows the logger.
void
follow_logger_level (bool enabled, LogLevel level)
{
  set_log_level (to_threshold (enabled, level));
}
} // namespace

bool
install_lumex_logger_sink ()
{
  file_logger &logger = file_logger::get_instance ();
  set_log_sink (&lumex_logger_sink);
  if (logger.add_level_change_callback (&follow_logger_level))
    return true;
  // Every entry is taken: the threshold of this binary starts right at least
  follow_logger_level (logger.is_logging_enabled (), logger.get_log_level ());
  return false;
}

void
uninstall_lumex_logger_sink () LUMEX_NOEXCEPT
{
  if (get_log_sink () == &lumex_logger_sink)
    set_log_sink (nullptr);
  try
    {
      file_logger::get_instance ().remove_level_change_callback (
          &follow_logger_level);
    }
  catch (...) // NOLINT(bugprone-empty-catch)
    {
      // No singleton, so no callback to remove
    }
}

/// The timer and what its handler shares with the owner.
struct lumex_logger_watch_timer::state_t
{
  state_t (asio::io_context &context, std::chrono::milliseconds every)
      : timer (context), period (every), stopped (false)
  {
  }

  asio::steady_timer timer;               ///< Guarded by mutex
  std::chrono::milliseconds const period; ///< Time between two checks
  std::mutex mutex; ///< The owner's thread and the runner's touch the timer
  bool stopped;     ///< Guarded by mutex
};

lumex_logger_watch_timer::lumex_logger_watch_timer (
    stream::io_runner &runner, std::chrono::milliseconds period)
{
  // The singleton first: it then outlives a timer that is a static object
  (void)file_logger::get_instance ();
  state_ = std::make_shared<state_t> (runner.context (),
                                      period < std::chrono::milliseconds (1)
                                          ? std::chrono::milliseconds (1)
                                          : period);
  std::lock_guard<std::mutex> const lock (state_->mutex);
  arm (state_);
}

lumex_logger_watch_timer::~lumex_logger_watch_timer () { stop (); }

lumex_logger_watch_timer::lumex_logger_watch_timer (
    lumex_logger_watch_timer &&other) LUMEX_NOEXCEPT
    : state_ (std::move (other.state_))
{
}

lumex_logger_watch_timer &
lumex_logger_watch_timer::operator= (lumex_logger_watch_timer &&other)
    LUMEX_NOEXCEPT
{
  if (this != &other)
    {
      stop ();
      state_ = std::move (other.state_);
    }
  return *this;
}

bool
lumex_logger_watch_timer::is_running () const LUMEX_NOEXCEPT
{
  return state_ != nullptr;
}

std::chrono::milliseconds
lumex_logger_watch_timer::period () const LUMEX_NOEXCEPT
{
  return state_ != nullptr ? state_->period : std::chrono::milliseconds (0);
}

void
lumex_logger_watch_timer::arm (std::shared_ptr<state_t> const &state)
{
  state->timer.expires_after (state->period);
  // The handler owns a reference: the state outlives a wait that the
  // destructor cancels.
  state->timer.async_wait (
      [state] (std::error_code const &error)
        {
          std::lock_guard<std::mutex> const lock (state->mutex);
          if (error || state->stopped)
            return;
          // Under the mutex: the destructor waits for a running check.
          try
            {
              file_logger::get_instance ().force_reread ();
              arm (state);
            }
          catch (...) // NOLINT(bugprone-empty-catch)
            {
              // No singleton or no memory for the next wait: the timer ends
            }
        });
}

void
lumex_logger_watch_timer::stop () LUMEX_NOEXCEPT
{
  std::shared_ptr<state_t> const state = std::move (state_);
  state_.reset ();
  if (state == nullptr)
    return;
  try
    {
      std::lock_guard<std::mutex> const lock (state->mutex);
      state->stopped = true;
      state->timer.cancel ();
    }
  catch (...) // NOLINT(bugprone-empty-catch)
    {
      // stopped is set either way: the next handler returns at once
    }
}
} // namespace log
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
