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
 * @file LumexProcFs.hpp
 * @brief Text parsers for the Linux `/proc/stat` and `/proc/meminfo` files
 * the resource monitor samples.
 * @details The parsers take the file text, so they work, and are tested, on
 * any platform; only the sampler reads the files, and only on Linux.
 */
#ifndef LUMEX_APPLIED_RESOURCE_MONITOR_MONITOR_DETAIL_HPP
#define LUMEX_APPLIED_RESOURCE_MONITOR_MONITOR_DETAIL_HPP

#include "lumex/LumexExport.hpp"

#include <cstdint>
#include <string>

#include "lumex/core/optional/LumexOptional"
#include "lumex/core/string_view/LumexStringView"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
namespace resource_monitor
{
namespace monitor
{
/**
 * @brief Implementation details of the resource monitor, public only for its
 * tests.
 */
namespace detail
{
/**
 * @brief The aggregate CPU times of the first `cpu` line of `/proc/stat`, in
 * clock ticks.
 */
struct proc_stat_cpu_t
{
  /// @brief Idle time: `idle` plus `iowait` (a CPU waiting for I/O is idle).
  std::uint64_t idle{};
  /// @brief All time: `user`, `nice`, `system`, `idle`, `iowait`, `irq`,
  /// `softirq` and `steal`. `guest` and `guest_nice` are left out because
  /// `user` and `nice` already contain them.
  std::uint64_t total{};
};

/**
 * @brief Total and available physical memory from `/proc/meminfo`, in bytes.
 */
struct proc_meminfo_t
{
  /// @brief `MemTotal`.
  std::uint64_t total_bytes{};
  /// @brief `MemAvailable`: memory that can be given to new work without
  /// swapping, the page cache that can be dropped included.
  std::uint64_t available_bytes{};
};

/**
 * @brief The value of a `Key:   <number> kB` line, as `/proc/meminfo` and
 * `/proc/<pid>/status` write them.
 * @param text The file text.
 * @param key The key without the colon; a longer key that starts with it does
 * not match.
 * @return The value in bytes (the number times 1024 when the unit is `kB`),
 * or nothing when no line has the key or its value is not a number.
 */
LUMEX_API optional<std::uint64_t> parse_kib_field (lumex_string_view text,
                                                   lumex_string_view key);

/**
 * @brief Parses the text of `/proc/stat`.
 * @param text The file text; only its first line is read.
 * @return The CPU times, or nothing when the first line is not the aggregate
 * `cpu` line with at least the four fields `user nice system idle`.
 */
LUMEX_API optional<proc_stat_cpu_t>
parse_proc_stat_cpu (lumex_string_view text);

/**
 * @brief Parses the text of `/proc/meminfo`.
 * @param text The file text.
 * @return Total and available memory, or nothing when `MemTotal` or
 * `MemAvailable` is missing (`MemAvailable` exists since Linux 3.14).
 */
LUMEX_API optional<proc_meminfo_t> parse_proc_meminfo (lumex_string_view text);

/**
 * @brief Reads a whole file, including a `/proc` file whose size is reported
 * as 0.
 * @param path The file to read.
 * @return The text, or nothing when the file cannot be opened.
 */
LUMEX_API optional<std::string> read_whole_file (std::string const &path);
} // namespace detail
} // namespace monitor
} // namespace resource_monitor
} // namespace applied
} // namespace lumex

#endif // !LUMEX_APPLIED_RESOURCE_MONITOR_MONITOR_DETAIL_HPP
