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

/**
 * @file LumexCrashHandler.hpp
 * @brief `LumexCrashHandler`, the singleton that writes a dump when the
 * process crashes.
 * @details `initialize()` creates the crash directory and, on Unix-like
 * systems, marks the process dumpable, lifts the core file size limit and
 * installs handlers for SIGSEGV, SIGABRT, SIGFPE and SIGILL. Such a handler
 * reports the signal on standard error, tries to point the kernel core pattern
 * at the crash directory (it runs `sudo tee /proc/sys/kernel/core_pattern` and
 * writes a `.info` file when that fails) and raises the signal again with the
 * default action. On these systems the crash directory is `crashes` in the
 * application's directory under `~/.local/share` or `XDG_DATA_HOME`; on
 * Windows it is `crashes` next to the executable.
 *
 * On Windows the handler writes a minidump through DbgHelp and shows a message
 * box, but `initialize()` installs no unhandled-exception filter: a thread
 * reaches the handler through the translator that `SET_SEH_TRANSLATOR` of
 * `WindowsSEHTranslator.hpp` installs. The class is compiled into
 * `lumex::exceptions` and is also visible at global scope.
 */
#ifndef LUMEX_CORE_EXCEPTIONS_CRASH_CRASH_HANDLER_HPP
#define LUMEX_CORE_EXCEPTIONS_CRASH_CRASH_HANDLER_HPP

#include "lumex/LumexExport.hpp"

#include <csignal>
#include <cstdlib>
#include <string>

#if defined(_WIN32)
#include <Windows.h> // This library must be included before DbgHelp.h
                     // because DbgHelp.h uses types from Windows.h

#include <DbgHelp.h>
#include <tchar.h>
#else
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "lumex/core/utility/LumexUtility"
#include "lumex/core/utility/attr/LumexAttributes.hpp"

#if defined(_WIN32)
#pragma comment(lib, "dbghelp.lib")
#endif

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace exceptions
{
namespace crash
{
/**
 * @class LumexCrashHandler
 * @brief Handles application crashes by generating crash dumps (minidumps on
 * Windows, core dumps on Unix).
 *
 * This class provides a cross-platform mechanism to capture crash information
 * and notify the UI. It supports both structured exception handling (SEH) on
 * Windows and signal handling on Unix.
 */
class LUMEX_API LumexCrashHandler
{
public:
  LumexCrashHandler (LumexCrashHandler const &) = delete;
  LumexCrashHandler &operator= (LumexCrashHandler const &) = delete;
  LumexCrashHandler (LumexCrashHandler &&) = delete;
  LumexCrashHandler &operator= (LumexCrashHandler &&) = delete;

  /**
   * @brief Returns the singleton instance of the crash handler.
   * @return Reference to the singleton instance.
   */
  static LumexCrashHandler &instance ();

  /**
   * @brief Initializes the crash handler.
   *
   * Creates the crash dump directory and sets up platform-specific crash
   * handlers.
   */
  void initialize (LUMEX_ATTRIBUTE_MAYBE_UNUSED std::string const &appName
                   = s_defaultAppName);

#if defined(LUMEX_OS_WINDOWS)
  /**
   * @brief Windows-specific crash handler for unhandled exceptions.
   * @param pExInfo Pointer to exception information.
   * @return Execution disposition (always `EXCEPTION_EXECUTE_HANDLER`).
   */
  static LONG WINAPI _onWindowsCrashHandler (PEXCEPTION_POINTERS pExInfo);

  /**
   * @brief Wrapper for the Windows crash handler.
   * @param pExInfo Pointer to exception information.
   */
  void
  _handleSEHException (PEXCEPTION_POINTERS pExInfo)
  {
    _onWindowsCrashHandler (pExInfo);
  }
#endif

private:
  LumexCrashHandler () = default;
  ~LumexCrashHandler () = default;

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable                                                       \
                : 4251) // Suppress C4251 for STL members in DLL interface
#endif
  static std::string s_appName; ///< The name of the application.
#ifdef _WIN32
#pragma warning(pop)
#endif
  static char const *s_defaultAppName;

  /**
   * @brief Generates a filename for the crash dump.
   * @param prefix Prefix for the dump filename.
   * @return Full path to the dump file.
   */
  static std::string _generateDumpFilename (std::string const &prefix);

  /**
   * @brief Notifies the UI and logs crash details.
   * @param errorMessage The error message to log and notify.
   */
  static void _notifyAndLog (std::string const &errorMessage);

#if defined(LUMEX_OS_UNIX) && LUMEX_OS_UNIX
  /// @brief Generates a core dump on Unix systems.
  static void _generateCoreDump ();

  /**
   * @brief Signal handler for Unix systems.
   * @param signum The signal number.
   */
  static void _signalHandler (int signum);

  /// @brief Sets up signal handlers for Unix systems.
  static void _setupSignalHandlers ();

  /// @brief Configures core dump settings on Unix systems.
  static void _setupCoreDumpSettings ();
#endif
};
} // namespace crash
} // namespace exceptions
} // namespace core
} // namespace lumex

using LumexCrashHandler = lumex::core::exceptions::crash::LumexCrashHandler;

#endif // !LUMEX_CORE_EXCEPTIONS_CRASH_CRASH_HANDLER_HPP
