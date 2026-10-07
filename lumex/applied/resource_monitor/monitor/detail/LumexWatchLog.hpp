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
 * @file LumexWatchLog.hpp
 * @brief The text the background sampler logs for a watch list.
 * @details Formatting functions that take samples and give text, so they work,
 * and are tested, on any platform; `collect_watch_entries` is the one that
 * samples.
 */
#ifndef LUMEX_APPLIED_RESOURCE_MONITOR_MONITOR_WATCH_LOG_HPP
#define LUMEX_APPLIED_RESOURCE_MONITOR_MONITOR_WATCH_LOG_HPP

#include "lumex/LumexExport.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include "lumex/applied/resource_monitor/process/LumexProcessMonitor.hpp"
#include "lumex/core/optional/LumexOptional"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
namespace resource_monitor
{
namespace monitor
{
namespace detail
{
/**
 * @brief One watch-list item as the log shows it.
 */
struct watch_entry_t
{
  /// @brief The text that goes on the sample line after ` | `.
  std::string head;
  /// @brief The breakdown: one line per process behind a sum, written under
  /// the sample line. Empty unless the breakdown was asked for and the item
  /// covers more than one process.
  std::vector<std::string> details;
};

/**
 * @brief A size in megabytes with one decimal, for example `210.4Mb`.
 * @param bytes The size in bytes (1 Mb is 1048576 bytes).
 */
LUMEX_API std::string format_megabytes (std::uint64_t bytes);

/**
 * @brief A CPU share with one decimal, for example `3.1%`; `n/a` when the
 * process has no share yet (its first sample).
 */
LUMEX_API std::string
format_cpu_share (lumex::core::optional::opt::optional<double> share);

/**
 * @brief The log text of a watch-list item given by process ID.
 * @param pid The process ID.
 * @param result What the process monitor returned for it.
 * @param breakdown Whether to list the processes behind a sum.
 * @details A sample is `name[pid] 3.1% 210.4Mb`; with processes below it,
 * `name[pid +k] ...` (k of them are added). A process that cannot be read is
 * `pid exited`, `pid access denied`, `pid unreadable` or `pid unsupported`.
 * The breakdown lines read `pid name 3.1% 210.4Mb`.
 */
LUMEX_API watch_entry_t format_pid_entry (
    process::process_id_t pid,
    process::LumexProcessMonitor::result_t const &result, bool breakdown);

/**
 * @brief The log text of a watch-list item given by executable name.
 * @param name The requested name.
 * @param usages What the process monitor returned for it.
 * @param breakdown Whether to list the processes behind the sum.
 * @details `name[xN] 6.0% 812.7Mb`: N processes are added up. With no process
 * `name[x0] not running`.
 */
LUMEX_API watch_entry_t format_name_entry (
    std::string const &name,
    std::vector<process::process_usage_t> const &usages, bool breakdown);

/**
 * @brief The text of one sample: the line, then the breakdown lines.
 * @param prefix What the line starts with (the time and the system usage).
 * @param entries The watch-list items, in order.
 * @return `prefix | head | head`, a line break, then every detail line
 * indented by four spaces, each ending in a line break. Without entries,
 * `prefix` and a line break.
 */
LUMEX_API std::string
compose_log_line (std::string const &prefix,
                  std::vector<watch_entry_t> const &entries);

/**
 * @brief Samples a watch list and formats every item.
 * @param monitor The process monitor that keeps the previous samples.
 * @param list The watch list: process IDs first, then names.
 * @return One entry per item, in the order of the list.
 */
LUMEX_API std::vector<watch_entry_t>
collect_watch_entries (process::LumexProcessMonitor &monitor,
                       process::watch_list_t const &list);
} // namespace detail
} // namespace monitor
} // namespace resource_monitor
} // namespace applied
} // namespace lumex

#endif // !LUMEX_APPLIED_RESOURCE_MONITOR_MONITOR_WATCH_LOG_HPP
