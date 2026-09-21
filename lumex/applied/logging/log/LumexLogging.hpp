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

#ifndef LUMEX_APPLIED_LOGGING_LOG_LOGGING_HPP
#define LUMEX_APPLIED_LOGGING_LOG_LOGGING_HPP

#include "lumex/LumexExport.hpp"

#include <cstdint>
#include <mutex>
#include <string>

#include "lumex/core/filesystem/LumexFilesystem" // For `Path`
#include "lumex/core/string/LumexString"         // For `stringify`
#include "lumex/core/utility/macros/LumexConstantMacros.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace applied
{
namespace logging
{
namespace log
{
/// @brief Defines the severity levels for logging.
enum class LumexLogLevel : std::uint8_t
{
  Debug = 10,
  Info = 20,
  Success = 25,
  Warning = 30,
  Error = 40,
  Critical = 50
};

/**
 * @class Logger
 * @brief Thread-safe singleton logger supporting multiple modules.
 *
 * Logger provides methods for logging messages at different severity levels,
 * includes a custom SUCCESS level, and outputs colored messages to the console
 * using ANSI escape codes.
 */
class LUMEX_API LumexLogging
{
public:
  /// @brief Log a DEBUG-level message.
  template <typename... Args>
  static void
  debug (char const *moduleName, Args &&...args)
  {
    _log (LumexLogLevel::Debug, moduleName,
          stringify (std::forward<Args> (args)...));
  }

  /// @brief Log an INFO-level message.
  template <typename... Args>
  static void
  info (char const *moduleName, Args &&...args)
  {
    _log (LumexLogLevel::Info, moduleName,
          stringify (std::forward<Args> (args)...));
  }

  /// @brief Log a SUCCESS-level message (custom level).
  template <typename... Args>
  static void
  success (char const *moduleName, Args &&...args)
  {
    _log (LumexLogLevel::Success, moduleName,
          stringify (std::forward<Args> (args)...));
  }

  /// @brief Log a WARNING-level message.
  template <typename... Args>
  static void
  warning (char const *moduleName, Args &&...args)
  {
    _log (LumexLogLevel::Warning, moduleName,
          stringify (std::forward<Args> (args)...));
  }

  /// @brief Log an ERROR-level message.
  template <typename... Args>
  static void
  error (char const *moduleName, Args &&...args)
  {
    _log (LumexLogLevel::Error, moduleName,
          stringify (std::forward<Args> (args)...));
  }

  /// @brief Log a CRITICAL-level message.
  template <typename... Args>
  static void
  critical (char const *moduleName, Args &&...args)
  {
    _log (LumexLogLevel::Critical, moduleName,
          stringify (std::forward<Args> (args)...));
  }

  /**
   * @brief Write a log to a file in the logs/ directory
   * @param filename The name of the log file
   * @param level The log level
   * @param moduleName The name of the module
   * @param msg The message
   * @param appendTimestamp Whether to add a timestamp to the file name
   * @return true in case of success, false otherwise
   */
  static bool toFile (char const *filename, LumexLogLevel level,
                      char const *moduleName, char const *msg,
                      bool appendTimestamp = true);

  /**
   * @brief Get the path to the logs directory
   * @return The path to the logs directory
   */
  static lumex::path getLogsDirectory ();

  /**
   * @brief Set the name of the application
   * @param appName The name of the application
   */
  static void setAppName (std::string const &appName);

private:
  LUMEX_CONSTEXPR LUMEX_CONST_STR KDEFAULT_LOG_FILE_NAME
      = "log";                          ///< Default log file name.
  LUMEX_CONST_NUM short KLOG_WIDTH = 8; ///< The width of the log level.

#ifdef _WIN32
#pragma warning(push)
#pragma warning(                                                              \
    disable : 4251) // Suppress C4251 for STL members in DLL interface
#endif
  static std::string s_appName; ///< The name of the application, by default it
                                ///< would be empty.
  static std::mutex s_mutex;    ///< Mutex for thread-safe logging.
  static lumex::path s_logsDirectory; ///< The directory for storing logs.
  static std::string
      s_launchTimestamp; ///< The timestamp when the application was launched.
#ifdef _WIN32
#pragma warning(pop)
#endif

  /// @brief Core logging routine: formats and outputs everything to the
  /// console.
  static void _log (LumexLogLevel level, std::string const &moduleName,
                    std::string const &msg);

  /// @brief Converts the level to a string ("DEBUG", "INFO", …).
  static char const *_level_to_string (LumexLogLevel level) LUMEX_NOEXCEPT;

  /// @brief ANSI‑color for the given level.
  static char const *_level_to_color (LumexLogLevel level) LUMEX_NOEXCEPT;
};
} // namespace applied
} // namespace log
} // namespace logging
} // namespace lumex

using LumexLogging = lumex::applied::logging::log::LumexLogging;

#endif // !LUMEX_APPLIED_LOGGING_LOG_LOGGING_HPP
