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
 * @file LumexResourceMonitor.hpp
 * @brief Background CPU/RAM usage sampler that appends one line per sample to
 * a dated log file, optionally with the usage of watched processes.
 * @details Provides an opt-in resource-usage sampler a host application can
 * start on demand. The library itself owns no configuration policy (no
 * environment-variable name, no hardcoded "is this enabled" switch) - a caller
 * decides whether to start it at all, and with what poll interval, purely
 * through @ref
 * lumex::applied::resource_monitor::monitor::LumexResourceMonitor::start_if_enabled
 *          and
 * @ref
 * lumex::applied::resource_monitor::monitor::LumexResourceMonitor::start_with_watch_list
 *          arguments.
 */
#ifndef LUMEX_APPLIED_RESOURCE_MONITOR_MONITOR_HPP
#define LUMEX_APPLIED_RESOURCE_MONITOR_MONITOR_HPP

#include "lumex/LumexExport.hpp"

#include <chrono>
#include <cstdint>
#include <string>

#include "lumex/applied/resource_monitor/process/LumexProcessMonitor.hpp"
#include "lumex/core/utility/macros/LumexConstantMacros.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
/**
 * @brief Background system-resource (CPU/RAM) sampling utilities.
 */
namespace resource_monitor
{
namespace monitor
{
namespace Constants
{
/// @brief Poll interval used when the caller does not request a specific one.
LUMEX_CONST_NUM std::int64_t KDEFAULT_POLL_INTERVAL_MS = 10000; // 10 seconds.

/// @brief Grace period the sampler waits before taking its first sample.
/// @details Gives the host application time to finish initializing before any
/// sampling starts,
///          and keeps a thread/instrumentation sanitizer from reporting noise
///          for activity that happens during that initialization window.
LUMEX_CONST_NUM std::int64_t KSTARTUP_GRACE_PERIOD_MS = 2000; // 2 seconds.
} // namespace Constants

/// @brief Module name this class logs under (see
/// `lumex/applied/logging/LumexLogging`).
LUMEX_CONST_STR KMODULE_NAME = "ResourceMonitor";

/**
 * @class LumexResourceMonitor
 * @brief Starts/stops a background thread that periodically samples total CPU
 * and RAM usage and appends one formatted line per sample to a dedicated log
 * file.
 *
 * @details On Windows, CPU usage is sampled via `GetSystemTimes` and RAM via
 * `GlobalMemoryStatusEx`; on Linux, CPU usage comes from `/proc/stat` and RAM
 * from `sysinfo`. Both platforms use the same delta-based CPU percentage
 * calculation (idle-time delta vs. total-time delta between two consecutive
 * samples). The sampler is a process-wide singleton: only one background
 * thread can run at a time, started and stopped through the static methods
 *          below.
 *
 * With a watch list (@ref start_with_watch_list) every line carries, after
 * the system part, one ` | ` entry per watched process or name: `name[pid]
 * 3.1% 210.4Mb` (CPU share of the whole machine, resident memory), with the
 * processes below it added up (`name[pid +2]`) unless the list says
 * otherwise; `name[x6] ...` for the sum of the six processes with a name;
 * `777 exited`, `777 access denied` for a process that cannot be read and
 * `name[x0] not running` for a name nothing has. With the breakdown on, the
 * lines below the sample line list every process behind a sum, as `pid name
 * 3.1% 210.4Mb`. The first line already has the CPU shares: the sampler takes
 * a baseline of the watched processes together with the system baseline.
 *
 * @note All methods are static and thread-safe.
 *
 * @par Example
 * @code
 * // Host application decides whether to enable it and at what interval - the
 * library does not. if (hostConfig.resourceLoggingEnabled)
 * {
 *   lumex::applied::resource_monitor::monitor::LumexResourceMonitor::start_if_enabled(
 *     hostConfig.logDirectory, hostConfig.resourceLogPollInterval);
 * }
 * // ... later, at shutdown:
 * lumex::applied::resource_monitor::monitor::LumexResourceMonitor::stop();
 * @endcode
 */
class LUMEX_API LumexResourceMonitor
{
public:
  /**
   * @brief Starts the background CPU/RAM sampler, unless it is already
   * running.
   * @param logDirectory Directory the sampler creates (if missing) and writes
   * its dated `system_resource_usage_<timestamp>.log` file into.
   * @param pollInterval How often to sample CPU/RAM usage. Defaults to
   *                     `Constants::KDEFAULT_POLL_INTERVAL_MS`. A non-positive
   * value is treated as invalid and replaced by that same default.
   * @note This call is a no-op if the sampler is already running - call
   * `stop ()` first if a different interval or log directory is needed.
   * @note Whether to call this at all, and with what interval, is entirely the
   * caller's decision; this class does not read any environment variable to
   * decide it.
   */
  static void start_if_enabled (
      std::string const &logDirectory,
      std::chrono::milliseconds pollInterval
      = std::chrono::milliseconds (Constants::KDEFAULT_POLL_INTERVAL_MS));

  /**
   * @brief Starts the background sampler like @ref start_if_enabled, and adds
   * the CPU and RAM of watched processes to every line.
   * @param logDirectory As in @ref start_if_enabled.
   * @param watchList The processes to follow: by process ID and by executable
   * name, with the processes below them (`include_children`, on by default)
   * and the breakdown by process (`breakdown`, off by default). See
   * @ref lumex::applied::resource_monitor::process::LumexProcessMonitor for
   * what a name matches and how the processes below one are found.
   * @param pollInterval As in @ref start_if_enabled.
   * @note No-op if the sampler is already running, like @ref start_if_enabled.
   * An empty watch list logs the same lines as @ref start_if_enabled.
   */
  static void start_with_watch_list (
      std::string const &logDirectory, process::watch_list_t const &watchList,
      std::chrono::milliseconds pollInterval
      = std::chrono::milliseconds (Constants::KDEFAULT_POLL_INTERVAL_MS));

  /**
   * @brief Stops the background sampler and joins its thread.
   * @details Wakes the sampler at once: the call does not wait for the startup
   * pause or the rest of the poll interval.
   * @note No-op if the sampler is not currently running.
   */
  static void stop ();

private:
  LumexResourceMonitor () = default;
};
} // namespace monitor
} // namespace resource_monitor
} // namespace applied
} // namespace lumex

using LumexResourceMonitor
    = lumex::applied::resource_monitor::monitor::LumexResourceMonitor;

#endif // !LUMEX_APPLIED_RESOURCE_MONITOR_MONITOR_HPP
