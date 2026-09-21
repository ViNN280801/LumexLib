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

/**
 * @file LumexPortProcessResolver.hpp
 * @brief Identifies the process that holds a serial port open.
 *
 * @details Cross-platform facade with platform strategies:
 * - Windows: `NtQuerySystemInformation` / `NtDuplicateObject` /
 * `NtQueryObject` walk system handles and match the object name to a COM
 * display name. Requires `SeDebugPrivilege` (typically an elevated process) to
 * inspect handles owned by other processes.
 * - POSIX: scan `/proc/<pid>/fd/` symbolic links for `/dev/tty*` and read
 *   `/proc/<pid>/exe` plus `/proc/<pid>/cmdline`.
 *
 * Methods do not mutate facade state. The Meyers singletons used internally
 * are initialized once and are safe to call from multiple threads.
 */
#ifndef LUMEX_APPLIED_SERIAL_RESOLVER_HPP
#define LUMEX_APPLIED_SERIAL_RESOLVER_HPP

#include "lumex/LumexExport.hpp"

#include <string>

#include "lumex/core/optional/LumexOptional"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace applied
{
namespace serial
{
namespace resolver
{
/**
 * @brief Process that currently holds a serial-port file descriptor or handle.
 */
struct port_holder_info_t
{
  unsigned long pid;    ///< Process identifier.
  std::string exe_name; ///< Basename of the executable, e.g. `"PeakExpert"`.
  std::string exe_path; ///< Full path to the executable when available.
  std::string cmdline;  ///< Command line when the platform exposes it.
};

/**
 * @brief Formats a platform error code into a diagnostic string.
 *
 * Implementations must be stateless and thread-safe. Windows uses
 * `FormatMessageW`; POSIX uses `strerror`.
 */
class system_error_formatter
{
public:
  virtual ~system_error_formatter () = default;

  /**
   * @brief Convert `err_code` to a human-readable string.
   * @param err_code `GetLastError` on Windows, `errno` on POSIX.
   * @return `"Permission denied (13)"` or `"error 13"` when the code is
   * unknown. Empty when `err_code == 0`.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "System error formatting result should be stored for diagnostics.")
  virtual std::string format (int err_code) const LUMEX_NOEXCEPT = 0;
};

/**
 * @brief Locates the process that holds a given serial port.
 *
 * Windows requires `SeDebugPrivilege`. Implementations must be stateless
 * and thread-safe.
 */
class port_holder_resolver
{
public:
  virtual ~port_holder_resolver () = default;

  /**
   * @brief Resolve the holder of `port_path`.
   * @param port_path Path passed to `CreateFile`/`open` (`"\\\\.\\COM10"`,
   * `"/dev/ttyUSB0"`).
   * @param port_display_name Short name used for matching (`"COM10"`,
   * `"ttyUSB0"`).
   * @return Holder information, or empty when none was found or privileges
   * are insufficient.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Port holder resolution result must be checked before dereference.")
  virtual optional<port_holder_info_t>
  resolve (std::string const &port_path,
           std::string const &port_display_name) const LUMEX_NOEXCEPT
      = 0;
};

/**
 * @brief Static facade over the platform error formatter and port-holder
 * resolver.
 *
 * The class is not constructible. All entry points are static and delegate
 * to a Meyers singleton of the matching platform strategy.
 */
class LUMEX_PUBLIC_API port_process_resolver final
{
public:
  port_process_resolver (port_process_resolver const &) = delete;
  port_process_resolver &operator= (port_process_resolver const &) = delete;

  /**
   * @brief Format a system error code.
   * @param err_code `GetLastError` on Windows, `errno` on POSIX.
   * @return Diagnostic string, or empty when `err_code == 0`.
   */
  static std::string format_system_error (int err_code) LUMEX_NOEXCEPT;

  /**
   * @brief Identify the process holding `port_path`.
   *
   * Do not call this for a port the current process already has open on
   * Windows: `NtQuerySystemInformation` can deadlock against that handle.
   *
   * @param port_path Path passed to `CreateFile`/`open`.
   * @param port_display_name Short name used for matching.
   * @return Holder information, or empty when the port is free, privileges
   * are insufficient, or the platform cannot resolve a holder.
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "Port holder resolution result must be checked before dereference.")
  static optional<port_holder_info_t> get_process_holding_port (
      std::string const &port_path,
      std::string const &port_display_name) LUMEX_NOEXCEPT;

private:
  port_process_resolver () = default;

  static system_error_formatter const &error_formatter () LUMEX_NOEXCEPT;
  static port_holder_resolver const &holder_resolver () LUMEX_NOEXCEPT;
};

} // namespace resolver
} // namespace serial
} // namespace applied
} // namespace lumex

#endif
