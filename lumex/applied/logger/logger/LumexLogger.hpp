/**
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

#ifndef LUMEX_APPLIED_LOGGER_LOGGER_HPP
#define LUMEX_APPLIED_LOGGER_LOGGER_HPP

#include "lumex/LumexExport.hpp"

#include <chrono>
#include <cstdint>
#include <deque>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexConstantMacros.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"
// ============= OS Detection Macros =============
// Cross-platform OS macros for detecting operating systems
// Compatible with MSVC, GCC, and Clang compilers

// Reset all OS macros first
#undef LOGGER_OS_WINDOWS
#undef LOGGER_OS_LINUX
#undef LOGGER_OS_MAC
#undef LOGGER_OS_UNIX
#undef LOGGER_OS_UNKNOWN

// Windows (32-bit and 64-bit)
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#define LOGGER_OS_WINDOWS 1
#if defined(_WIN64) || defined(__WIN64__)
#define LOGGER_OS_WINDOWS_64 1
#else
#define LOGGER_OS_WINDOWS_32 1
#endif
// Apple Platforms
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_MAC
#define LOGGER_OS_MAC 1
#define LOGGER_OS_MACOS 1
#endif
#define LOGGER_OS_APPLE 1
// Linux
#elif defined(__linux__) || defined(linux) || defined(__linux)
#define LOGGER_OS_LINUX 1
// Generic Unix
#elif defined(__unix__) || defined(__unix) || defined(unix)
#define LOGGER_OS_UNIX 1
// Unknown OS
#else
#define LOGGER_OS_UNKNOWN 1
#endif

// Architecture Helpers
#if defined(_M_X64) || defined(__x86_64__) || defined(__x86_64)               \
    || defined(__amd64__) || defined(__amd64)
#define LOGGER_ARCH_X64 1
#define LOGGER_ARCH_64BIT 1
#elif defined(_M_IX86) || defined(__i386__) || defined(__i386) || defined(i386)
#define LOGGER_ARCH_X86 1
#define LOGGER_ARCH_32BIT 1
#elif defined(_M_ARM64) || defined(__aarch64__)
#define LOGGER_ARCH_ARM64 1
#define LOGGER_ARCH_64BIT 1
#elif defined(_M_ARM) || defined(__arm__) || defined(__arm)
#define LOGGER_ARCH_ARM 1
#define LOGGER_ARCH_32BIT 1
#endif

// Convenience macros for common checks
#define LOGGER_OS_IS_WINDOWS() (defined (LOGGER_OS_WINDOWS))
#define LOGGER_OS_IS_LINUX() (defined (LOGGER_OS_LINUX))
#define LOGGER_OS_IS_MACOS()                                                  \
  (defined (LOGGER_OS_MAC) || defined (LOGGER_OS_MACOS))
#define LOGGER_OS_IS_APPLE() (defined (LOGGER_OS_APPLE))
#define LOGGER_OS_IS_UNIX()                                                   \
  (defined (LOGGER_OS_UNIX) || defined (LOGGER_OS_LINUX)                      \
   || defined (LOGGER_OS_APPLE))

// File separator and path constants
#ifdef LOGGER_OS_WINDOWS
#define LOGGER_FILE_SEPARATOR '\\'
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#define LOGGER_MAX_PATH MAX_PATH
#else
#define LOGGER_FILE_SEPARATOR '/'
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define LOGGER_MAX_PATH PATH_MAX
#endif

// ============= Function Name Macros =============

#if defined(_WIN32) || defined(__WIN32__) || defined(__WIN64__)               \
    || defined(__MINGW32__) || defined(__MINGW64__)
#define LOGGER_FUNCTION_NAME __FUNCSIG__
#elif defined(__GNUC__) || defined(__clang__)
#define LOGGER_FUNCTION_NAME __PRETTY_FUNCTION__
#else
#define LOGGER_FUNCTION_NAME __func__
#endif

// Short function name (just function name without signature) - standard
// __func__ (C99/C++11)
#define LOGGER_FUNCTION_NAME_SHORT __func__

#define LOGGER_ALIGNMENT_LOGGER_CONFIG 128
#define LOGGER_ALIGNMENT_LOG_ENTRY 64

// ============= C++11 Compatibility Layer =============

#if __cplusplus < 201402L // C++11 or earlier
#include <memory>
#include <utility>
namespace std
{
template <typename T, typename... Args>
typename std::enable_if<!std::is_array<T>::value, std::unique_ptr<T>>::type
make_unique (Args &&...args)
{
  return std::unique_ptr<T> (new T (std::forward<Args> (args)...));
}

template <typename T>
typename std::enable_if<std::is_array<T>::value, std::unique_ptr<T>>::type
make_unique (std::size_t n)
{
  using U = typename std::remove_extent<T>::type;
  return std::unique_ptr<T> (new U[n]);
}

template <class T, class U = T>
LUMEX_CONSTEXPR_FUNCTION T
exchange (T &obj, U &&new_value)
    LUMEX_NOEXCEPT_IF (std::is_nothrow_move_constructible<T>::value
                           &&std::is_nothrow_assignable<T &, U>::value)
{
  T old_value = std::move (obj);
  obj = std::forward<U> (new_value);
  return old_value;
}
} // namespace std
#endif

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
/**
 * @brief Universal file logger, ported and kept up to date from the DChannel
 * project's Logger.
 * @details All symbols below live in this namespace instead of the global
 * namespace specifically to avoid name collisions in a shared library consumed
 * by many projects (see e.g. `logger_stringify`, which would otherwise be a
 * dangerously generic global name).
 */
namespace logger
{
namespace logger
{
// ============= Stringify Function =============

// Streamability checks: lumex::core::utility::traits::stream (plain
// `os << value`, no stringify extras).

// Stringify implementation variants
#if __cplusplus >= 202002L

// C++20+ concepts version
template <lumex::core::utility::traits::stream::AllStreamable... Args>
inline std::string
logger_stringify (Args &&...args)
{
  LUMEX_CONSTEXPR_IF (sizeof...(args) == 0) { return ""; }
  else
  {
    std::ostringstream oss;
    ((oss << std::forward<Args> (args)), ...);
    return oss.str ();
  }
}

#elif __cplusplus >= 201703L

// C++17 version with fold expressions and if constexpr
template <typename... Args>
inline std::string
logger_stringify (Args &&...args)
{
  LUMEX_STATIC_ASSERT_MSG (
      lumex::core::utility::traits::stream::all_ostreamable_v<Args...>,
      "All arguments must be streamable");

  LUMEX_CONSTEXPR_IF (sizeof...(args) == 0) { return ""; }
  else
  {
    std::ostringstream oss;
    ((oss << std::forward<Args> (args)), ...);
    return oss.str ();
  }
}

#elif __cplusplus >= 201402L

// C++14 version with variable templates
template <typename... Args>
inline std::string
logger_stringify (Args &&...args)
{
  LUMEX_STATIC_ASSERT_MSG (
      lumex::core::utility::traits::stream::all_ostreamable_v<Args...>,
      "All arguments must be streamable");

  if ((sizeof...(args) == 0) ? true : false)
    return "";

  std::ostringstream oss;
  (void)std::initializer_list<int>{ (oss << std::forward<Args> (args), 0)... };
  return oss.str ();
}

#else

// C++11 version
template <typename... Args>
inline std::string
logger_stringify (Args &&...args)
{
  LUMEX_STATIC_ASSERT_MSG (
      lumex::core::utility::traits::stream::all_ostreamable<Args...>::value,
      "All arguments must be streamable");

  if ((sizeof...(args) == 0) ? true : false)
    return "";

  std::ostringstream oss;
  (void)std::initializer_list<int>{ (oss << std::forward<Args> (args), 0)... };
  return oss.str ();
}

#endif

// Helper function for empty case optimization
inline std::string
logger_stringify () LUMEX_NOEXCEPT
{
  return {};
}

/**
 * @brief Log levels that classify messages by severity.
 *
 * Log levels filter messages by severity.
 * From LEVEL_TRACE (most verbose) to LEVEL_FATAL (critical error).
 */
enum class LogLevel : uint8_t
{
  LEVEL_TRACE = 0,   ///< Detailed debug information
  LEVEL_DEBUG = 1,   ///< Debug information
  LEVEL_INFO = 2,    ///< General information
  LEVEL_SUCCESS = 3, ///< Successful completion
  LEVEL_WARNING = 4, ///< Warnings
  LEVEL_ERROR = 5,   ///< Errors
  LEVEL_FATAL = 6    ///< Critical errors
};

/**
 * @brief Formats a function name from __FUNCSIG__/__PRETTY_FUNCTION__,
 * stripping the return type and arguments.
 * @param prettyFunc String from LOGGER_FUNCTION_NAME (__FUNCSIG__ or
 * __PRETTY_FUNCTION__).
 * @return String of the form ClassName::methodName() or functionName().
 *
 * Examples:
 * IN:  "class nlohmann::json __cdecl SomeNamespace::SomeClass::getState()"
 * OUT: "SomeNamespace::SomeClass::getState()"
 * IN:  "bool __cdecl lumex::applied::Something::WriteData(class ... &)"
 * OUT: "lumex::applied::Something::WriteData()"
 */
inline std::string
logger_format_function_name_clean (char const *prettyFunc) LUMEX_NOEXCEPT
{
  if (prettyFunc == nullptr)
    return "";

  std::string func (prettyFunc);
  if (func.empty ())
    return "";

  // Drop arguments (everything after the first '(')
  std::size_t openParen = func.find ('(');
  if (openParen != std::string::npos)
    func = func.substr (0, openParen);

  // Last space ends the return type (and, on MSVC, the calling convention):
  // MSVC "bool __cdecl Class::method" / "int __cdecl compute", GCC "void
  // Class::method" / "void compute". Searching the whole string (not just up
  // to the last "::") also fixes free functions (no "::" at all), which
  // previously short-circuited and returned the return type too.
  std::size_t lastSpace = func.rfind (' ');
  if (lastSpace != std::string::npos)
    func = func.substr (lastSpace + 1);

  return func + "()";
}

/**
 * @brief How the function name is shown in logs.
 */
enum class FunctionNameMode : uint8_t
{
  NONE = 0,  ///< Do not show the function name
  SHORT = 1, ///< Show the short name (__func__)
  FULL = 2,  ///< Full signature (__FUNCSIG__/__PRETTY_FUNCTION__)
  NORMAL = 3 ///< ClassName::methodName() without return type or arguments
};

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4324) // structure was padded due to alignment
                                // specifier (intentional)
#endif
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif
/**
 * @brief Logger configuration read from the enable_logs file.
 *
 * Holds every logger setting that can be specified
 * in the config file. Type-safe and extensible.
 */
struct alignas (LOGGER_ALIGNMENT_LOGGER_CONFIG) logger_config_t
{
  // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
  /// Larger types first (8+ bytes)
  /// ~40 bytes
  std::unordered_set<std::string>
      preset_components; ///< Preset component set (empty = preset
                         ///< disabled)
  /// 8 bytes
  LUMEX_CONST_NUM std::size_t kBufferSize
      = 100; ///< Log buffer size (default 100 entries)
  std::size_t buffer_size
      = kBufferSize; ///< Log buffer size (default 100 entries)

  /// Then medium types (2-4 bytes)
  LUMEX_CONST_NUM short kMaxStackTraceFrames
      = 16; ///< Maximum stack-trace frames (default 16)
  short stack_trace_max_frames = kMaxStackTraceFrames; ///< Maximum stack-trace
                                                       ///< frames (default 16)

  /// Then small types (1 byte), grouped together
  LogLevel log_level = LogLevel::LEVEL_INFO; ///< Minimum log level
  FunctionNameMode func_name_mode
      = FunctionNameMode::FULL;      ///< Function-name display mode
  bool use_timestamped_logs = false; ///< Whether to use timestamped logs
  bool show_stack_trace = false; ///< Whether to show a stack trace (default
                                 ///< false, but true for WARNING/ERROR)
  bool create_hint_file = true;  ///< Whether to create the developer hint
                                 ///< file (default true)
  bool buffering_enabled
      = false; ///< Whether log buffering is enabled (default false)
  bool buffering_trigger_configured
      = false; ///< Whether BUFFERING_TRIGGER is set (if true, flush only on
               ///< level, not on exit)
  LogLevel buffering_trigger_level
      = LogLevel::LEVEL_WARNING; ///< Minimum level that
                                 ///< flushes the buffer (TRACE..FATAL)
  bool describe_frame = false;   ///< Enable tree-shaped frame decoding in
                                 ///< logs (DESCRIBE_FRAME=true|1|yes)
  /// + 2 bytes padding to align to 8 bytes
  // NOLINTEND(misc-non-private-member-variables-in-classes)

  /**
   * @brief Default constructor with default settings.
   */
  logger_config_t () = default;

  /**
   * @brief Constructor with explicit parameters.
   * @param level Log level.
   * @param funcMode Function-name display mode.
   * @param timestamped Whether to use timestamped logs.
   * @param stackTrace Whether to show a stack trace.
   * @param maxFrames Maximum number of stack-trace frames.
   * @param hintFile Whether to create the hint helper file.
   * @param preset Preset component set.
   */
  logger_config_t (LogLevel level, FunctionNameMode funcMode, bool timestamped,
                   bool stackTrace = false,
                   short maxFrames = kMaxStackTraceFrames,
                   bool hintFile = true,
                   std::unordered_set<std::string> const &preset
                   = std::unordered_set<std::string>{}) LUMEX_NOEXCEPT
      : preset_components (preset),
        stack_trace_max_frames (maxFrames),
        log_level (level),
        func_name_mode (funcMode),
        use_timestamped_logs (timestamped),
        show_stack_trace (stackTrace),
        create_hint_file (hintFile)
  {
  }
};

/**
 * @brief Buffered log-record structure.
 *
 * Holds log records in a buffer before they are written to the file.
 * Holds everything needed for a later write.
 */
struct alignas (LOGGER_ALIGNMENT_LOG_ENTRY) log_entry_t
{
  // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
  /// Large types (8+ bytes)
  std::string message;                             ///< Log message
  std::chrono::system_clock::time_point timestamp; ///< Record timestamp

  /// Small types (1 byte)
  LogLevel level; ///< Log level
  /// Padding: 7 bytes to align to 8 bytes
  // NOLINTEND(misc-non-private-member-variables-in-classes)

  /**
   * @brief Constructor with parameters.
   * @param level_ Log level.
   * @param message_ Log message.
   * @param timestamp_ Timestamp (defaults to now).
   */
  log_entry_t (LogLevel level_, std::string message_,
               std::chrono::system_clock::time_point timestamp_
               = std::chrono::system_clock::now ()) LUMEX_NOEXCEPT
      : message (std::move (message_)),
        timestamp (timestamp_),
        level (level_)
  {
  }
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

static inline std::string
log_level_to_string (LogLevel level) LUMEX_NOEXCEPT
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
  switch (level)
    {
    case LogLevel::LEVEL_TRACE:
      return "LEVEL_TRACE";
    case LogLevel::LEVEL_DEBUG:
      return "LEVEL_DEBUG";
    case LogLevel::LEVEL_INFO:
      return "LEVEL_INFO";
    case LogLevel::LEVEL_SUCCESS:
      return "LEVEL_SUCCESS";
    case LogLevel::LEVEL_WARNING:
      return "LEVEL_WARNING";
    case LogLevel::LEVEL_ERROR:
      return "LEVEL_ERROR";
    case LogLevel::LEVEL_FATAL:
      return "LEVEL_FATAL";
    default:
      return "UNKNOWN";
    }
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}

/**
 * @brief Snapshot of configuration applied at start (flags without
 * public getters).
 * @note For diagnostics and trigger-file matrix tests.
 */
struct logger_applied_config_view_t
{
  bool use_timestamped_logs;         ///< TIMESTAMPED=...
  bool show_stack_trace_in_messages; ///< STACKTRACE=...
  short stack_trace_max_frames;      ///< STACKTRACE_FRAMES=...
  bool
      log_buffer_flush_level_configured; ///< whether BUFFERING_TRIGGER= is set
  LogLevel log_buffer_flush_trigger_level; ///< buffer-flush level on
                                           ///< BUFFERING_TRIGGER=
};

/**
 * @brief Singleton logger that writes messages to a file next to the
 * executable file.
 *
 * This class provides centralized file logging. The log file
 * is placed next to the executable (.exe, .dll, .so, .AppImage) or
 * library. The logger detects the executable path
 * and creates a log file next to it, overwriting it on every start
 * of the program.
 *
 * Features:
 * - Automatic detection of the executable path
 * - Support for Windows, Linux, and macOS
 * - Thread safety via mutexes
 * - Automatic formatting of time and log level
 * - Filtering by log level
 * - Conditional enable via a trigger file
 * - Log-level configuration from the trigger-file contents
 * - Timestamped logs in the Logger_logs/ directory
 *
 * Invariants:
 * - The log file is created only when `kLoggingEnableFileName` exists
 * - Every write is protected by a mutex
 * - Log level cannot be below LEVEL_TRACE or above
 * LEVEL_FATAL
 * - Logging is disabled by default if the trigger file is missing
 * - Default log level: LEVEL_INFO
 * - Supported levels in the file: trace, debug, info, warning, error, fatal
 * (case-insensitive)
 * - Timestamped logs are created in Logger_logs/ with names
 * logger_DD.MM.YYYY-hh:mm:ss.log
 *
 * Thread safety:
 * - Every operation is protected by an internal mutex
 * - Concurrent calls are safe
 *
 * Exceptions:
 * - Constructor: may throw std::runtime_error if file creation fails
 * - Logging methods: may throw std::ios_base::failure on
 * write failure
 * - Observers: do not throw
 */
class LUMEX_API LumexLogger final
{
public:
  // Copy and assignment are forbidden for the singleton
  LumexLogger (LumexLogger const &) = delete;
  LumexLogger &operator= (LumexLogger const &) = delete;
  LumexLogger (LumexLogger &&) = delete;
  LumexLogger &operator= (LumexLogger &&) = delete;

  /**
   * @brief Returns the single logger instance (singleton).
   * @return Reference to the logger instance.
   * @note Thread-safe; creates the instance on first call.
   * @throws std::runtime_error if creating the log file fails.
   */
  static LumexLogger &get_instance ();

  /**
   * @brief Sets the minimum log level.
   * @param level New minimum log level.
   * @note Messages below this level are not written.
   */
  void set_log_level (LogLevel level) LUMEX_NOEXCEPT;

  /**
   * @brief Returns the current minimum log level.
   * @return Current minimum log level.
   * @note Does not throw.
   */
  LogLevel get_log_level () const LUMEX_NOEXCEPT;

  /**
   * @brief Writes a LEVEL_TRACE message.
   * @param args Message parts to write to the log.
   * @throws std::ios_base::failure if writing to the file fails.
   * @note Written only if the current log level is <= LEVEL_TRACE.
   */
  template <typename... Args>
  void
  trace (Args &&...args)
  {
    log (LogLevel::LEVEL_TRACE,
         logger_stringify (std::forward<Args> (args)...));
  }

  /**
   * @brief Writes a LEVEL_DEBUG message.
   * @param args Message parts to write to the log.
   * @throws std::ios_base::failure if writing to the file fails.
   * @note Written only if the current log level is <= LEVEL_DEBUG.
   */
  template <typename... Args>
  void
  debug (Args &&...args)
  {
    log (LogLevel::LEVEL_DEBUG,
         logger_stringify (std::forward<Args> (args)...));
  }

  /**
   * @brief Writes a LEVEL_INFO message.
   * @param args Message parts to write to the log.
   * @throws std::ios_base::failure if writing to the file fails.
   * @note Written only if the current log level is <= LEVEL_INFO.
   */
  template <typename... Args>
  void
  info (Args &&...args)
  {
    log (LogLevel::LEVEL_INFO,
         logger_stringify (std::forward<Args> (args)...));
  }

  /**
   * @brief Writes a LEVEL_SUCCESS message.
   * @param args Message parts to write to the log.
   * @throws std::ios_base::failure if writing to the file fails.
   * @note Written only if the current log level is <=
   * LEVEL_SUCCESS.
   */
  template <typename... Args>
  void
  success (Args &&...args)
  {
    log (LogLevel::LEVEL_SUCCESS,
         logger_stringify (std::forward<Args> (args)...));
  }

  /**
   * @brief Writes a LEVEL_WARNING message.
   * @param args Message parts to write to the log.
   * @throws std::ios_base::failure if writing to the file fails.
   * @note Written only if the current log level is <=
   * LEVEL_WARNING.
   */
  template <typename... Args>
  void
  warning (Args &&...args)
  {
    log (LogLevel::LEVEL_WARNING,
         logger_stringify (std::forward<Args> (args)...));
  }

  /**
   * @brief Writes a LEVEL_ERROR message.
   * @param args Message parts to write to the log.
   * @throws std::ios_base::failure if writing to the file fails.
   * @note Written only if the current log level is <= LEVEL_ERROR.
   */
  template <typename... Args>
  void
  error (Args &&...args)
  {
    log (LogLevel::LEVEL_ERROR,
         logger_stringify (std::forward<Args> (args)...));
  }

  /**
   * @brief Writes a LEVEL_FATAL message.
   * @param args Message parts to write to the log.
   * @throws std::ios_base::failure if writing to the file fails.
   * @note Written only if the current log level is <= LEVEL_FATAL.
   */
  template <typename... Args>
  void
  fatal (Args &&...args)
  {
    log (LogLevel::LEVEL_FATAL,
         logger_stringify (std::forward<Args> (args)...));
  }

  /**
   * @brief Writes a message at the given log level.
   * @param level Message log level.
   * @param message Message to write to the log.
   * @throws std::ios_base::failure if writing to the file fails.
   * @note Critical levels (WARNING/ERROR/FATAL, or BUFFERING_TRIGGER and
   * above) are written and flush the buffer unconditionally, from any
   * component, regardless of the preset. The preset filters only non-critical
   * levels.
   */
  void log (LogLevel level, std::string const &message);

  /**
   * @brief Checks whether the given log level is enabled.
   * @param level Log level to check.
   * @return true if the log level is enabled, false otherwise.
   * @note Does not throw.
   */
  bool is_level_enabled (LogLevel level) const LUMEX_NOEXCEPT;

  /**
   * @brief Checks whether tree-style frame parsing is enabled in logs
   * (DESCRIBE_FRAME).
   * @return true if DESCRIBE_FRAME=true|1|yes in the config, otherwise false.
   * @note Does not throw. Case-insensitive parsing of the value.
   */
  bool is_describe_frame_enabled () const LUMEX_NOEXCEPT;

  /**
   * @brief Checks whether logging is enabled at all.
   * @details If a file named `kLoggingEnableFileName` exists next to
   * the executable (or library), logging is enabled. If
   * the file specifies a log level (trace, debug, info, warning, error,
   * fatal), that level is used; otherwise the default is info.
   * Case-insensitive parsing is supported.
   * @return true if logging is enabled, false otherwise.
   * @note Does not throw.
   */
  bool is_logging_enabled () const LUMEX_NOEXCEPT;

  /**
   * @brief Returns the function-name display mode used in logs.
   * @return Function-name display mode.
   * @note Does not throw.
   */
  FunctionNameMode get_function_name_mode () const LUMEX_NOEXCEPT;

  /**
   * @brief Checks whether the function name should be shown in logs.
   * @return true if the function name should be shown, false otherwise.
   * @note Does not throw.
   * @deprecated Use get_function_name_mode() instead.
   */
  LUMEX_ATTRIBUTE_DEPRECATED_MSG ("Use get_function_name_mode() instead, this "
                                  "method kept for backward compatibility")
  bool should_show_function_name () const LUMEX_NOEXCEPT;

  /**
   * @brief Returns the log-file path.
   * @return Log-file path as a string.
   * @note Does not throw.
   */
  std::string get_log_file_path () const LUMEX_NOEXCEPT;

  /**
   * @brief Prints a developer hint on how to use the logger.
   * @details Writes to stdout how to create the trigger file and example
   * configuration. Bilingual output: Russian and English. Useful when
   * logging does not work because the trigger file is missing or
   * the configuration is invalid.
   * @note Does not throw.
   */
  void print_logger_hint () const LUMEX_NOEXCEPT;

  /**
   * @brief Forces buffers to be flushed to the file.
   * @details When buffering is on, first writes the accumulated records
   * from `log_buffer` (same as a trigger-level flush), then flushes
   * the system buffer of `log_file`.
   * @throws std::ios_base::failure if flushing buffers fails.
   * @note Useful for critical messages that must be written
   * immediately.
   */
  void flush ();

  /**
   * @brief Enables log buffering.
   * @param bufferSize Buffer size (number of records to keep).
   * Default is logger_config_t::kBufferSize (100).
   * @note When buffering is on, logs are not written immediately; they are
   * kept in the buffer. On WARNING/ERROR/FATAL (or BUFFERING_TRIGGER and
   * above) or on flush(), buffered records are written to the file.
   * @note Thread-safe.
   */
  void enable_buffering (std::size_t bufferSize
                         = logger_config_t::kBufferSize) LUMEX_NOEXCEPT;

  /**
   * @brief Disables log buffering.
   * @note After buffering is disabled, every log is written immediately.
   *       If the buffer holds records, they are written before disable.
   * @throws std::ios_base::failure if writing the buffer fails.
   * @note Thread-safe.
   */
  void disable_buffering ();

  /**
   * @brief Sets the log-buffer size.
   * @param bufferSize New buffer size (record count).
   * @note If the new size is smaller than the current buffer, older records
   * are dropped.
   * @note Thread-safe.
   */
  void set_buffer_size (std::size_t bufferSize) LUMEX_NOEXCEPT;

  /**
   * @brief Checks whether buffering is enabled.
   * @return true if buffering is enabled, false otherwise.
   * @note Does not throw.
   */
  bool is_buffering_enabled () const LUMEX_NOEXCEPT;

  /**
   * @brief Returns the current buffer size.
   * @return Current buffer size (record count).
   * @note Does not throw.
   */
  std::size_t get_buffer_size () const LUMEX_NOEXCEPT;

  /**
   * @brief Enables filtering by component presets.
   * @param components Component set used for filtering (only these
   * components are logged).
   * @note If the preset is enabled, only messages from the listed
   * components are logged. Component format in the message: "[ComponentName]:
   * message" or
   * "[ComponentName] message"
   * @note Thread-safe.
   */
  void enable_preset (std::unordered_set<std::string> const &components)
      LUMEX_NOEXCEPT;

  /**
   * @brief Disables preset filtering.
   * @note After disable, every message is logged regardless of component.
   * @note Thread-safe.
   */
  void disable_preset () LUMEX_NOEXCEPT;

  /**
   * @brief Sets the component set for the preset.
   * @param components Component set used for filtering.
   * @note Automatically enables the preset if it was disabled.
   * @note Thread-safe.
   */
  void set_preset_components (
      std::unordered_set<std::string> const &components) LUMEX_NOEXCEPT;

  /**
   * @brief Checks whether preset filtering is enabled.
   * @return true if the preset is enabled, false otherwise.
   * @note Does not throw.
   */
  bool is_preset_enabled () const LUMEX_NOEXCEPT;

  /**
   * @brief Returns the current preset component set.
   * @return Preset component set (copy).
   * @note Does not throw.
   */
  std::unordered_set<std::string>
  get_preset_components () const LUMEX_NOEXCEPT;

  /**
   * @brief Sets the trigger-file name used to enable logging.
   * @param fileName Trigger-file name (for example, "enable_logs").
   * @note The trigger file must sit next to the executable.
   * @note Changing the trigger-file name does not reread configuration
   * automatically. Applying the change may require restarting
   * of the program.
   * @note Thread-safe.
   */
  void set_trigger_file_name (std::string const &fileName) LUMEX_NOEXCEPT;

  /**
   * @brief Reads logger configuration from an absolute enable/config path.
   * @param[in] enableFilePath Full path of the enable/config file.
   * @return Parsed logger_config_t, or defaults on most failures.
   * @throws std::runtime_error when LUMEX_LOGGER_CONFIG_FORMAT=YAML
   *         (LumexSettingsYAML is not implemented yet).
   * @note Exactly one reader is compiled in via LUMEX_LOGGER_CONFIG_FORMAT.
   *       The path extension is not inspected.
   * @note XML uses LumexXml and expects KEY elements below one `logger`
   *       document element.
   */
  static logger_config_t
  read_config_from_path (std::string const &enableFilePath);

  /**
   * @brief Returns the current trigger-file name.
   * @return Trigger-file name.
   * @note Does not throw.
   */
  std::string get_trigger_file_name () const LUMEX_NOEXCEPT;

  /**
   * @brief Returns a snapshot of extended configuration flags (read from
   * the trigger file).
   * @note Thread-safety: uses the internal mutex.
   */
  logger_applied_config_view_t get_applied_config_view () const LUMEX_NOEXCEPT;

private:
  /**
   * @brief Private constructor for the singleton.
   * @throws std::runtime_error if creating the log file fails.
   */
  LumexLogger ();

  /**
   * @brief Destructor; closes the log file.
   */
  ~LumexLogger ();

  /**
   * @brief Resolves the executable path.
   * @return Path of the directory that contains the executable.
   * @throws std::runtime_error if path resolution fails.
   */
  std::string _getExecutableDirectory () const;

  /**
   * @brief Builds the full log-file path.
   * @return Full log-file path.
   */
  std::string _createLogFilePath () const;

  /**
   * @brief Builds the path of the ordinary (overwrite) log file.
   * @return Full path of the ordinary log file.
   */
  std::string _createSingleLogFilePath () const;

  /**
   * @brief Builds the path of a timestamped log file in Logger_logs/.
   * @return Full path of the timestamped log file.
   */
  std::string _createTimestampedLogFilePath () const;

  /**
   * @brief Formats the current time for a file name as
   * DD.MM.YYYY-hh:mm:ss.
   * @return Formatted time string for a file name.
   */
  static std::string _formatTimestampForFilename ();

  /**
   * @brief Formats the current time for a log record.
   * @return Formatted time string.
   */
  static std::string _formatTimestamp ();

  /**
   * @brief Converts a log level to a string.
   * @param level Log level.
   * @return String representation of the log level.
   */
  static std::string _level_to_string (LogLevel level);

  /**
   * @brief Writes a message after checking the log level.
   * @param level Message log level.
   * @param message Message to write.
   * @throws std::ios_base::failure if writing to the file fails.
   */
  void _writeLog (LogLevel level, std::string const &message);

  /**
   * @brief Extracts the directory from a full file path.
   * @param fullPath Full file path.
   * @return Path of the directory that contains the file.
   * @note Cross-platform implementation without std::filesystem.
   */
  static std::string _extractDirectoryFromPath (std::string const &fullPath);

  /**
   * @brief Checks that the logging-enable file exists and reads
   * the log level.
   * @return true if the trigger file (default enable_logs) exists,
   * false otherwise.
   * @note Does not throw.
   */
  bool _isLoggingEnabledFileExists () const LUMEX_NOEXCEPT;

  /**
   * @brief Reads configuration from the logging-enable file next to the
   *        executable (trigger_file_name under executable_directory).
   * @return logger_config_t with logger settings.
   * @throws std::runtime_error when LUMEX_LOGGER_CONFIG_FORMAT=YAML
   *         (LumexSettingsYAML is not implemented yet). Other parse
   *         failures yield defaults.
   */
  logger_config_t _readConfigFromFile () const;

  /**
   * @brief Applies one KEY/value pair onto a logger_config_t.
   * @param[in,out] config Configuration being filled.
   * @param[in] key Config key (case-insensitive), without a PREFIX= wrapper.
   * @param[in] value Raw value string.
   * @note Used by the compiled reader (plain text, INI, JSON, XML).
   *       Does not throw. YAML throws before this helper is reached.
   */
  static void _applyLoggerConfigKey (logger_config_t &config,
                                     std::string const &key,
                                     std::string const &value) LUMEX_NOEXCEPT;

  /**
   * @brief Converts a string to a log level.
   * @param levelStr Log-level string (case-insensitive).
   * @return Matching LogLevel, or LEVEL_INFO by default.
   * @note Does not throw.
   */
  static LogLevel
  _string_to_log_level (std::string const &levelStr) LUMEX_NOEXCEPT;

  /**
   * @brief Parses a string into a function-name display mode.
   * @param funcNameStr Setting string (case-insensitive):
   * "none"/"NO_FUNCNAME", "short", "normal", "full"/"signature".
   * @return Function-name display mode.
   * @note Does not throw.
   */
  static FunctionNameMode
  _parseFunctionNameMode (std::string const &funcNameStr) LUMEX_NOEXCEPT;

  /**
   * @brief Checks whether the function name should be shown.
   * @param funcNameStr Setting string (case-insensitive).
   * @return true if the function name should be shown, false if NO_FUNCNAME.
   * @note Does not throw.
   * @deprecated Use _parseFunctionNameMode() instead.
   */
  LUMEX_ATTRIBUTE_DEPRECATED_MSG (
      "Use _parseFunctionNameMode() instead, this method kept for backward "
      "compatibility")
  static bool
  _shouldShowFunctionName (std::string const &funcNameStr) LUMEX_NOEXCEPT;

  /**
   * @brief Parses a string into a stacktrace-enable value.
   * @param stackTraceStr Setting string (case-insensitive):
   * "true"/"yes"/"1" or "false"/"no"/"0".
   * @return true if stacktrace should be shown, false otherwise.
   * @note Does not throw.
   */
  static bool
  _should_show_stack_trace (std::string const &stackTraceStr) LUMEX_NOEXCEPT;

  /**
   * @brief Parses a string into the stacktrace frame count.
   * @param framesStr String with a frame count.
   * @param defaultFrames Default value if parsing fails.
   * @return Number of stacktrace frames.
   * @note Does not throw.
   */
  static short _parseStackTraceFrames (std::string const &framesStr,
                                       short defaultFrames) LUMEX_NOEXCEPT;

  /**
   * @brief Checks whether timestamped logs should be used.
   * @param timestampedStr Setting string (case-insensitive).
   * @return true if timestamped logs should be used, false
   * otherwise.
   * @note Does not throw.
   */
  static bool
  _shouldUseTimestampedLogs (std::string const &timestampedStr) LUMEX_NOEXCEPT;

  /**
   * @brief Checks whether log size stays within allowed limits.
   * @details Checks the log file or folder size against free
   * disk space. Uses dynamic limits based on available space.
   * @return true if the size is within limits, false if the limit is exceeded.
   * @note Does not throw.
   */
  bool _checkLogSizeLimits () const LUMEX_NOEXCEPT;

  /**
   * @brief Computes the maximum allowed log size from free
   * space.
   * @details Takes both absolute limits and a percentage of free
   * space.
   * @param freeSpace Free disk space in bytes.
   * @param isDirectory true for the log folder, false for a log file.
   * @return Maximum allowed size in bytes.
   * @note Does not throw.
   */
  static int64_t _calculateMaxAllowedSize (int64_t freeSpace,
                                           bool isDirectory) LUMEX_NOEXCEPT;

  /**
   * @brief Removes old logs when limits are exceeded.
   * @details Deletes the oldest log files to free space.
   * @param logPath Path to the log file or folder.
   * @param targetSize Target size after cleanup, in bytes.
   * @return true if cleanup succeeded, false otherwise.
   * @note Does not throw.
   */
  static bool _cleanupOldLogs (std::string const &logPath,
                               int64_t targetSize) LUMEX_NOEXCEPT;

  /**
   * @brief Returns log files sorted by creation time.
   * @param directoryPath Path to the log directory.
   * @return Vector of (file_path, creation_time) pairs, sorted by
   * time (oldest first).
   * @note Does not throw.
   */
  static std::vector<std::pair<std::string, std::time_t>>
  _getLogFilesSortedByTime (std::string const &directoryPath) LUMEX_NOEXCEPT;

  /**
   * @brief Checks whether a log can be written without exceeding limits.
   * @details Performs a full size check before writing.
   * @param messageSize Size of the message about to be written.
   * @return true if the write is allowed, false if the limit is exceeded.
   * @note Does not throw.
   */
  bool _canWriteLog (std::size_t messageSize) const LUMEX_NOEXCEPT;

  /**
   * @brief Formats a byte size into a readable string.
   * @param sizeInBytes Size in bytes.
   * @return String with the formatted size (for example, "1.5 GB").
   * @note Does not throw.
   */
  static std::string _format_size (int64_t sizeInBytes) LUMEX_NOEXCEPT;

  /**
   * @brief Writes every buffered record to the file.
   * @details Used on WARNING/ERROR/FATAL to write
   * log history.
   * @throws std::ios_base::failure if writing to the file fails.
   * @note Called only while the mutex is locked.
   */
  void _flushBuffer ();

  /**
   * @brief Extracts the component name from a message.
   * @param message Log message in the form "[ComponentName]: message" or
   * "[ComponentName] message".
   * @return Component name if found, otherwise an empty string.
   * @note Does not throw.
   */
  static std::string
  _extractComponentName (std::string const &message) LUMEX_NOEXCEPT;

  /**
   * @brief Returns the short name form (method name only, without class or
   * signature).
   * @param component Extracted component name (short, normal, or full
   * signature).
   */
  static std::string
  _getShortFormFromComponent (std::string const &component) LUMEX_NOEXCEPT;

  /**
   * @brief Returns the normal name form (ClassName::methodName()).
   * @param component Extracted component name.
   */
  static std::string
  _getNormalFormFromComponent (std::string const &component) LUMEX_NOEXCEPT;

  /**
   * @brief Checks whether one preset string matches the component name from
   * the message. Supports exact match, short name (WriteData),
   * normal (Class::method), and the full signature.
   */
  static bool
  _presetMatchesComponent (std::string const &preset,
                           std::string const &component) LUMEX_NOEXCEPT;

  /**
   * @brief Checks whether the log should be written according to the preset.
   * @param message Log message.
   * @return true if the log should be written, false otherwise.
   * @note If the preset is disabled, always returns true.
   * @note Does not throw.
   */
  bool _shouldLogByPreset (std::string const &message) const LUMEX_NOEXCEPT;

  /**
   * @brief Parses a comma-separated component list for the preset.
   * @param componentsStr Comma-separated component string
   * (for example, "A,B,E").
   * @return Preset component set.
   * @note Does not throw.
   */
  static std::unordered_set<std::string>
  _parse_preset_components (std::string const &componentsStr) LUMEX_NOEXCEPT;

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif
  std::ofstream log_file;           ///< Stream used to write the log file
  std::string log_file_path;        ///< Log-file path
  LogLevel current_log_level;       ///< Current minimum log level
  mutable std::mutex log_mutex;     ///< Mutex that protects write operations
  std::string executable_directory; ///< Executable directory
  bool logging_enabled;             ///< Logging-enabled flag
  FunctionNameMode function_name_mode; ///< Function-name display mode in logs
  bool use_timestamped_logs;           ///< Timestamped-log flag
  bool show_stack_trace;               ///< Show-stacktrace flag
  short stack_trace_max_frames;        ///< Maximum stack-trace frames
  mutable bool hint_shown;       ///< Flag that the hint has already been shown
  std::string trigger_file_name; ///< Trigger-file name used to enable logging

  // Log buffering
  bool buffering_enabled;             ///< Buffering-enabled flag
  std::size_t buffer_size;            ///< Buffer size (record count)
  std::deque<log_entry_t> log_buffer; ///< Buffer that stores log records
  bool buffering_trigger_configured;  ///< Whether BUFFERING_TRIGGER is set
                                      ///< (flush only by level, not on exit)
  LogLevel buffering_trigger_level; ///< Minimum level that flushes the buffer

  // Presets for component filtering
  bool preset_enabled; ///< Preset-enabled flag
  std::unordered_set<std::string>
      preset_components; ///< Component set used for filtering

  bool describe_frame_enabled
      = false; ///< Tree-style frame parsing (DESCRIBE_FRAME=true|1|yes)

  std::chrono::system_clock::time_point
      log_file_start_time;                                 ///< Log start time
  std::chrono::system_clock::time_point log_file_end_time; ///< Log end time
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

  LUMEX_CONSTEXPR LUMEX_CONST_STR kDefaultLogFileName = "logger.log";
  LUMEX_CONSTEXPR LUMEX_CONST_STR kDefaultLogFilePrefix = "logger";
  LUMEX_CONSTEXPR LUMEX_CONST_STR kLogDirectoryName = "Logger_logs";
  LUMEX_CONSTEXPR LUMEX_CONST_STR kDefaultTriggerFileName = "enable_logs";
  LUMEX_CONSTEXPR LUMEX_CONST_STR kHintFileName = "LOGGER_README.txt";

  // Constants that control log size
  LUMEX_CONST_NUM int64_t kMaxLogFileSizeBytes
      = static_cast<int64_t> (1024) * static_cast<int64_t> (1024)
        * static_cast<int64_t> (1024); ///< Maximum log-file size (1 GB)
  LUMEX_CONST_NUM int64_t kMaxLogDirectorySizeBytes
      = static_cast<int64_t> (2) * static_cast<int64_t> (1024)
        * static_cast<int64_t> (1024)
        * static_cast<int64_t> (1024); ///< Maximum log-folder size (2 GB)
  LUMEX_CONST_NUM double kMinFreeSpaceRatio
      = 0.1; ///< Minimum free-space fraction (10%)
  LUMEX_CONST_NUM int64_t kMinFreeSpaceBytes
      = static_cast<int64_t> (100) * static_cast<int64_t> (1024)
        * static_cast<int64_t> (1024); ///< Minimum free space (100 MB)
};

/**
 * @brief String form of the current std::thread id for
 * the log prefix.
 */
inline std::string
logger_format_current_thread_id ()
{
  std::ostringstream oss;
  oss << std::this_thread::get_id ();
  return oss.str ();
}

// MSVC: reinterpret_cast<uintptr_t>(x) is ill-formed when x is already
// uintptr_t (use identity/static_cast).
namespace LoggerInternals
{
inline std::string
AddressToHexLogString (std::nullptr_t)
{
  return "<0x0>";
}

inline std::string
AddressToHexLogString (std::uintptr_t uintValue)
{
  return "<0x" + std::to_string (uintValue) + ">";
}

inline std::string
AddressToHexLogString (std::intptr_t intValue)
{
  return "<0x" + std::to_string (static_cast<std::uintptr_t> (intValue)) + ">";
}

template <typename T>
inline typename std::enable_if<std::is_pointer<T>::value, std::string>::type
AddressToHexLogString (T pointerValue)
{
  return "<0x"
         + std::to_string (reinterpret_cast<std::uintptr_t> (pointerValue))
         + ">";
}

template <typename T>
inline typename std::enable_if<std::is_integral<T>::value
                                   && !std::is_same<T, std::uintptr_t>::value
                                   && !std::is_same<T, std::intptr_t>::value,
                               std::string>::type
AddressToHexLogString (T integralValue)
{
  return "<0x" + std::to_string (static_cast<std::uintptr_t> (integralValue))
         + ">";
}
} // namespace LoggerInternals
} // namespace logger
} // namespace logger
} // namespace applied
} // namespace lumex

/**
 * @brief Alias for lumex::applied::logger::logger::LumexLogger.
 * @details Simplifies usage of the LumexLogger singleton by allowing it to be
 * referred to without its full namespace qualification, matching the
 * convention used by LumexEnvironment and LumexLogging.
 */
using LumexLogger = lumex::applied::logger::logger::LumexLogger;

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

/**
 * @brief Convenience logging macros with automatic
 * function.
 *
 * @details These macros provide convenient logging with
 * automatic function-name prefixing and message formatting.
 * The macros check the log level at compile time and do nothing
 * if that level is disabled.
 *
 *          The macros use LOGGER_FUNCTION_NAME to obtain the current
 * function name for the compiler and platform.
 *
 *          The macros accept variadic arguments and automatically
 * convert them to a string via logger_stringify().
 *
 * @note Each record is prefixed with the thread id: `[TID=...]`
 * (or after the function name in SHORT/NORMAL/FULL). NONE mode: only
 * `[TID=...]` and the message text.
 *
 * @note `LOGGER_LOG_*` is a generic macro layer suitable for
 * reuse in another project. `LUMEX_LOG_*` is the project-specific layer for
 * this project (LumexLib); use it in LumexLib and dependent
 * projects.
 *
 * @example Usage examples:
 * @code
 * void myFunction() {
 *   LUMEX_LOG_TRACE("Entering function");
 *   LUMEX_LOG_DEBUG("Processing data:", value, "bytes");
 *   LUMEX_LOG_INFO("Operation completed successfully");
 *   LUMEX_LOG_SUCCESS("Operation completed successfully");
 *   LUMEX_LOG_WARNING("Deprecated API used, consider using:", newApiName);
 *   LUMEX_LOG_ERROR("Failed to open file:", filename, "error code:",
 * errorCode); LUMEX_LOG_FATAL("Critical system error, shutting down");
 * }
 * @endcode
 */

#define LOGGER_LOG_IF(level, ...)                                             \
  if (::LumexLogger::get_instance ().is_level_enabled (level))                \
    {                                                                         \
      ::lumex::applied::logger::logger::FunctionNameMode const funcMode       \
          = ::LumexLogger::get_instance ().get_function_name_mode ();         \
      if (funcMode                                                            \
          == ::lumex::applied::logger::logger::FunctionNameMode::NONE)        \
        ::LumexLogger::get_instance ().log (                                  \
            level, std::string ("[TID=")                                      \
                       + ::lumex::applied::logger::logger::                   \
                           logger_format_current_thread_id ()                 \
                       + "] "                                                 \
                       + ::lumex::applied::logger::logger::logger_stringify ( \
                           __VA_ARGS__));                                     \
      else if (funcMode                                                       \
               == ::lumex::applied::logger::logger::FunctionNameMode::SHORT)  \
        ::LumexLogger::get_instance ().log (                                  \
            level, std::string ("[") + LOGGER_FUNCTION_NAME_SHORT + "] [TID=" \
                       + ::lumex::applied::logger::logger::                   \
                           logger_format_current_thread_id ()                 \
                       + "] "                                                 \
                       + ::lumex::applied::logger::logger::logger_stringify ( \
                           __VA_ARGS__));                                     \
      else if (funcMode                                                       \
               == ::lumex::applied::logger::logger::FunctionNameMode::NORMAL) \
        ::LumexLogger::get_instance ().log (                                  \
            level,                                                            \
            std::string ("[")                                                 \
                + ::lumex::applied::logger::logger::                          \
                    logger_format_function_name_clean (LOGGER_FUNCTION_NAME)  \
                + "] [TID="                                                   \
                + ::lumex::applied::logger::logger::                          \
                    logger_format_current_thread_id ()                        \
                + "] "                                                        \
                + ::lumex::applied::logger::logger::logger_stringify (        \
                    __VA_ARGS__));                                            \
      else                                                                    \
        ::LumexLogger::get_instance ().log (                                  \
            level, std::string ("[") + LOGGER_FUNCTION_NAME + "] [TID="       \
                       + ::lumex::applied::logger::logger::                   \
                           logger_format_current_thread_id ()                 \
                       + "] "                                                 \
                       + ::lumex::applied::logger::logger::logger_stringify ( \
                           __VA_ARGS__));                                     \
    }                                                                         \
  else                                                                        \
    (void)0

#define LOGGER_LOG_TRACE(...)                                                 \
  LOGGER_LOG_IF (::lumex::applied::logger::logger::LogLevel::LEVEL_TRACE,     \
                 __VA_ARGS__)
#define LOGGER_LOG_DEBUG(...)                                                 \
  LOGGER_LOG_IF (::lumex::applied::logger::logger::LogLevel::LEVEL_DEBUG,     \
                 __VA_ARGS__)
#define LOGGER_LOG_INFO(...)                                                  \
  LOGGER_LOG_IF (::lumex::applied::logger::logger::LogLevel::LEVEL_INFO,      \
                 __VA_ARGS__)
#define LOGGER_LOG_SUCCESS(...)                                               \
  LOGGER_LOG_IF (::lumex::applied::logger::logger::LogLevel::LEVEL_SUCCESS,   \
                 __VA_ARGS__)
#define LOGGER_LOG_WARNING(...)                                               \
  LOGGER_LOG_IF (::lumex::applied::logger::logger::LogLevel::LEVEL_WARNING,   \
                 __VA_ARGS__)
#define LOGGER_LOG_ERROR(...)                                                 \
  LOGGER_LOG_IF (::lumex::applied::logger::logger::LogLevel::LEVEL_ERROR,     \
                 __VA_ARGS__)
#define LOGGER_LOG_FATAL(...)                                                 \
  LOGGER_LOG_IF (::lumex::applied::logger::logger::LogLevel::LEVEL_FATAL,     \
                 __VA_ARGS__)

#define LOGGER_ADDRESS_TO_STRING(address)                                     \
  (::lumex::applied::logger::logger::LoggerInternals::AddressToHexLogString ( \
      (address)))

#define LOGGER_LOG_ADDRESS_TRACE(message, address)                            \
  LOGGER_LOG_IF (::lumex::applied::logger::logger::LogLevel::LEVEL_TRACE,     \
                 (message) + LOGGER_ADDRESS_TO_STRING (address))
#define LOGGER_LOG_ADDRESS_DEBUG(message, address)                            \
  LOGGER_LOG_IF (::lumex::applied::logger::logger::LogLevel::LEVEL_DEBUG,     \
                 (message) + LOGGER_ADDRESS_TO_STRING (address))
#define LOGGER_LOG_ADDRESS_INFO(message, address)                             \
  LOGGER_LOG_IF (::lumex::applied::logger::logger::LogLevel::LEVEL_INFO,      \
                 (message) + LOGGER_ADDRESS_TO_STRING (address))
#define LOGGER_LOG_ADDRESS_SUCCESS(message, address)                          \
  LOGGER_LOG_IF (::lumex::applied::logger::logger::LogLevel::LEVEL_SUCCESS,   \
                 (message) + LOGGER_ADDRESS_TO_STRING (address))
#define LOGGER_LOG_ADDRESS_WARNING(message, address)                          \
  LOGGER_LOG_IF (::lumex::applied::logger::logger::LogLevel::LEVEL_WARNING,   \
                 (message) + LOGGER_ADDRESS_TO_STRING (address))
#define LOGGER_LOG_ADDRESS_ERROR(message, address)                            \
  LOGGER_LOG_IF (::lumex::applied::logger::logger::LogLevel::LEVEL_ERROR,     \
                 (message) + LOGGER_ADDRESS_TO_STRING (address))
#define LOGGER_LOG_ADDRESS_FATAL(message, address)                            \
  LOGGER_LOG_IF (::lumex::applied::logger::logger::LogLevel::LEVEL_FATAL,     \
                 (message) + LOGGER_ADDRESS_TO_STRING (address))

#define LOGGER_LOG_OBJECT_TRACE(object, ...)                                  \
  LOGGER_LOG_IF (::lumex::applied::logger::logger::LogLevel::LEVEL_TRACE,     \
                 "Object " + LOGGER_ADDRESS_TO_STRING (object) + ": "         \
                     + __VA_ARGS__)
#define LOGGER_LOG_OBJECT_DEBUG(object, ...)                                  \
  LOGGER_LOG_IF (::lumex::applied::logger::logger::LogLevel::LEVEL_DEBUG,     \
                 "Object " + LOGGER_ADDRESS_TO_STRING (object) + ": "         \
                     + __VA_ARGS__)
#define LOGGER_LOG_OBJECT_INFO(object, ...)                                   \
  LOGGER_LOG_IF (::lumex::applied::logger::logger::LogLevel::LEVEL_INFO,      \
                 "Object " + LOGGER_ADDRESS_TO_STRING (object) + ": "         \
                     + __VA_ARGS__)
#define LOGGER_LOG_OBJECT_SUCCESS(object, ...)                                \
  LOGGER_LOG_IF (::lumex::applied::logger::logger::LogLevel::LEVEL_SUCCESS,   \
                 "Object " + LOGGER_ADDRESS_TO_STRING (object) + ": "         \
                     + __VA_ARGS__)
#define LOGGER_LOG_OBJECT_WARNING(object, ...)                                \
  LOGGER_LOG_IF (::lumex::applied::logger::logger::LogLevel::LEVEL_WARNING,   \
                 "Object " + LOGGER_ADDRESS_TO_STRING (object) + ": "         \
                     + __VA_ARGS__)
#define LOGGER_LOG_OBJECT_ERROR(object, ...)                                  \
  LOGGER_LOG_IF (::lumex::applied::logger::logger::LogLevel::LEVEL_ERROR,     \
                 "Object " + LOGGER_ADDRESS_TO_STRING (object) + ": "         \
                     + __VA_ARGS__)
#define LOGGER_LOG_OBJECT_FATAL(object, ...)                                  \
  LOGGER_LOG_IF (::lumex::applied::logger::logger::LogLevel::LEVEL_FATAL,     \
                 "Object " + LOGGER_ADDRESS_TO_STRING (object) + ": "         \
                     + __VA_ARGS__)

/** @brief Logging with tree-style frame parsing. Writes only when LEVEL
 * is enabled AND DESCRIBE_FRAME=true|1|yes. */
#define LOGGER_LOG_IF_DESCRIBE_FRAME(level, ...)                              \
  if (::LumexLogger::get_instance ().is_level_enabled (level)                 \
      && ::LumexLogger::get_instance ().is_describe_frame_enabled ())         \
    {                                                                         \
      ::lumex::applied::logger::logger::FunctionNameMode const funcMode       \
          = ::LumexLogger::get_instance ().get_function_name_mode ();         \
      if (funcMode                                                            \
          == ::lumex::applied::logger::logger::FunctionNameMode::NONE)        \
        ::LumexLogger::get_instance ().log (                                  \
            level, std::string ("[TID=")                                      \
                       + ::lumex::applied::logger::logger::                   \
                           logger_format_current_thread_id ()                 \
                       + "] "                                                 \
                       + ::lumex::applied::logger::logger::logger_stringify ( \
                           __VA_ARGS__));                                     \
      else if (funcMode                                                       \
               == ::lumex::applied::logger::logger::FunctionNameMode::SHORT)  \
        ::LumexLogger::get_instance ().log (                                  \
            level, std::string ("[") + LOGGER_FUNCTION_NAME_SHORT + "] [TID=" \
                       + ::lumex::applied::logger::logger::                   \
                           logger_format_current_thread_id ()                 \
                       + "] "                                                 \
                       + ::lumex::applied::logger::logger::logger_stringify ( \
                           __VA_ARGS__));                                     \
      else if (funcMode                                                       \
               == ::lumex::applied::logger::logger::FunctionNameMode::NORMAL) \
        ::LumexLogger::get_instance ().log (                                  \
            level,                                                            \
            std::string ("[")                                                 \
                + ::lumex::applied::logger::logger::                          \
                    logger_format_function_name_clean (LOGGER_FUNCTION_NAME)  \
                + "] [TID="                                                   \
                + ::lumex::applied::logger::logger::                          \
                    logger_format_current_thread_id ()                        \
                + "] "                                                        \
                + ::lumex::applied::logger::logger::logger_stringify (        \
                    __VA_ARGS__));                                            \
      else                                                                    \
        ::LumexLogger::get_instance ().log (                                  \
            level, std::string ("[") + LOGGER_FUNCTION_NAME + "] [TID="       \
                       + ::lumex::applied::logger::logger::                   \
                           logger_format_current_thread_id ()                 \
                       + "] "                                                 \
                       + ::lumex::applied::logger::logger::logger_stringify ( \
                           __VA_ARGS__));                                     \
    }                                                                         \
  else                                                                        \
    (void)0

#define LOGGER_LOG_TRACE_DESCRIBE_FRAME(...)                                  \
  LOGGER_LOG_IF_DESCRIBE_FRAME (                                              \
      ::lumex::applied::logger::logger::LogLevel::LEVEL_TRACE, __VA_ARGS__)
#define LOGGER_LOG_DEBUG_DESCRIBE_FRAME(...)                                  \
  LOGGER_LOG_IF_DESCRIBE_FRAME (                                              \
      ::lumex::applied::logger::logger::LogLevel::LEVEL_DEBUG, __VA_ARGS__)
#define LOGGER_LOG_INFO_DESCRIBE_FRAME(...)                                   \
  LOGGER_LOG_IF_DESCRIBE_FRAME (                                              \
      ::lumex::applied::logger::logger::LogLevel::LEVEL_INFO, __VA_ARGS__)
#define LOGGER_LOG_SUCCESS_DESCRIBE_FRAME(...)                                \
  LOGGER_LOG_IF_DESCRIBE_FRAME (                                              \
      ::lumex::applied::logger::logger::LogLevel::LEVEL_SUCCESS, __VA_ARGS__)
#define LOGGER_LOG_WARNING_DESCRIBE_FRAME(...)                                \
  LOGGER_LOG_IF_DESCRIBE_FRAME (                                              \
      ::lumex::applied::logger::logger::LogLevel::LEVEL_WARNING, __VA_ARGS__)
#define LOGGER_LOG_ERROR_DESCRIBE_FRAME(...)                                  \
  LOGGER_LOG_IF_DESCRIBE_FRAME (                                              \
      ::lumex::applied::logger::logger::LogLevel::LEVEL_ERROR, __VA_ARGS__)
#define LOGGER_LOG_FATAL_DESCRIBE_FRAME(...)                                  \
  LOGGER_LOG_IF_DESCRIBE_FRAME (                                              \
      ::lumex::applied::logger::logger::LogLevel::LEVEL_FATAL, __VA_ARGS__)

// ---------------------------------------------------------------------------------------
// // Macro layer adapted for LumexLib (counterpart of DCHANNEL_LOG_* in
// the DChannel project).   //
// ---------------------------------------------------------------------------------------
// //
#define LUMEX_ADDRESS_TO_STRING(address) LOGGER_ADDRESS_TO_STRING (address)

#define LUMEX_LOG_TRACE(...) LOGGER_LOG_TRACE (__VA_ARGS__)
#define LUMEX_LOG_DEBUG(...) LOGGER_LOG_DEBUG (__VA_ARGS__)
#define LUMEX_LOG_INFO(...) LOGGER_LOG_INFO (__VA_ARGS__)
#define LUMEX_LOG_SUCCESS(...) LOGGER_LOG_SUCCESS (__VA_ARGS__)
#define LUMEX_LOG_WARNING(...) LOGGER_LOG_WARNING (__VA_ARGS__)
#define LUMEX_LOG_ERROR(...) LOGGER_LOG_ERROR (__VA_ARGS__)
#define LUMEX_LOG_FATAL(...) LOGGER_LOG_FATAL (__VA_ARGS__)

#define LUMEX_LOG_ADDRESS_TRACE(message, address)                             \
  LOGGER_LOG_ADDRESS_TRACE (message, address)
#define LUMEX_LOG_ADDRESS_DEBUG(message, address)                             \
  LOGGER_LOG_ADDRESS_DEBUG (message, address)
#define LUMEX_LOG_ADDRESS_INFO(message, address)                              \
  LOGGER_LOG_ADDRESS_INFO (message, address)
#define LUMEX_LOG_ADDRESS_SUCCESS(message, address)                           \
  LOGGER_LOG_ADDRESS_SUCCESS (message, address)
#define LUMEX_LOG_ADDRESS_WARNING(message, address)                           \
  LOGGER_LOG_ADDRESS_WARNING (message, address)
#define LUMEX_LOG_ADDRESS_ERROR(message, address)                             \
  LOGGER_LOG_ADDRESS_ERROR (message, address)
#define LUMEX_LOG_ADDRESS_FATAL(message, address)                             \
  LOGGER_LOG_ADDRESS_FATAL (message, address)

#define LUMEX_LOG_OBJECT_TRACE(object, ...)                                   \
  LOGGER_LOG_OBJECT_TRACE (object, __VA_ARGS__)
#define LUMEX_LOG_OBJECT_DEBUG(object, ...)                                   \
  LOGGER_LOG_OBJECT_DEBUG (object, __VA_ARGS__)
#define LUMEX_LOG_OBJECT_INFO(object, ...)                                    \
  LOGGER_LOG_OBJECT_INFO (object, __VA_ARGS__)
#define LUMEX_LOG_OBJECT_SUCCESS(object, ...)                                 \
  LOGGER_LOG_OBJECT_SUCCESS (object, __VA_ARGS__)
#define LUMEX_LOG_OBJECT_WARNING(object, ...)                                 \
  LOGGER_LOG_OBJECT_WARNING (object, __VA_ARGS__)
#define LUMEX_LOG_OBJECT_ERROR(object, ...)                                   \
  LOGGER_LOG_OBJECT_ERROR (object, __VA_ARGS__)
#define LUMEX_LOG_OBJECT_FATAL(object, ...)                                   \
  LOGGER_LOG_OBJECT_FATAL (object, __VA_ARGS__)

#define LUMEX_LOG_TRACE_DESCRIBE_FRAME(...)                                   \
  LOGGER_LOG_TRACE_DESCRIBE_FRAME (__VA_ARGS__)
#define LUMEX_LOG_DEBUG_DESCRIBE_FRAME(...)                                   \
  LOGGER_LOG_DEBUG_DESCRIBE_FRAME (__VA_ARGS__)
#define LUMEX_LOG_INFO_DESCRIBE_FRAME(...)                                    \
  LOGGER_LOG_INFO_DESCRIBE_FRAME (__VA_ARGS__)
#define LUMEX_LOG_SUCCESS_DESCRIBE_FRAME(...)                                 \
  LOGGER_LOG_SUCCESS_DESCRIBE_FRAME (__VA_ARGS__)
#define LUMEX_LOG_WARNING_DESCRIBE_FRAME(...)                                 \
  LOGGER_LOG_WARNING_DESCRIBE_FRAME (__VA_ARGS__)
#define LUMEX_LOG_ERROR_DESCRIBE_FRAME(...)                                   \
  LOGGER_LOG_ERROR_DESCRIBE_FRAME (__VA_ARGS__)
#define LUMEX_LOG_FATAL_DESCRIBE_FRAME(...)                                   \
  LOGGER_LOG_FATAL_DESCRIBE_FRAME (__VA_ARGS__)
// ---------------------------------------------------------------------------------------

// NOLINTEND(cppcoreguidelines-macro-usage)

#endif // !LUMEX_APPLIED_LOGGER_LOGGER_HPP
