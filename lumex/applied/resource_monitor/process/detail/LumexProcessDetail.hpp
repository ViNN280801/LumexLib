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
 * @file LumexProcessDetail.hpp
 * @brief Text parsers for Linux `/proc/<pid>` files and the executable name
 * rules of `LumexProcessMonitor`.
 * @details Pure functions on text, so they work, and are tested, on any
 * platform; the monitor applies the Linux ones on Linux and the Windows one on
 * Windows.
 */
#ifndef LUMEX_APPLIED_RESOURCE_MONITOR_PROCESS_DETAIL_HPP
#define LUMEX_APPLIED_RESOURCE_MONITOR_PROCESS_DETAIL_HPP

#include "lumex/LumexExport.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

#include "lumex/core/optional/LumexOptional"
#include "lumex/core/string_view/LumexStringView"
#include "lumex/core/utility/macros/LumexConstantMacros.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
namespace resource_monitor
{
namespace process
{
/**
 * @brief Implementation details of `LumexProcessMonitor`, public only for its
 * tests.
 */
namespace detail
{
/// @brief The length the Linux kernel cuts a process's `comm` to.
LUMEX_CONST_NUM std::size_t KCOMM_LENGTH = 15;

/**
 * @brief The fields of `/proc/<pid>/stat` the monitor needs.
 */
struct proc_pid_stat_t
{
  /// @brief Field 2, `comm`, without its parentheses.
  std::string comm;
  /// @brief Field 3, the state: `R`, `S`, `D`, `T`, `Z` (zombie: exited, not
  /// yet reaped by its parent), `X` (dead), ...
  char state{};
  /// @brief Field 14, user CPU time, in clock ticks.
  std::uint64_t utime_ticks{};
  /// @brief Field 15, kernel CPU time, in clock ticks.
  std::uint64_t stime_ticks{};
  /// @brief Field 22, the start time after boot, in clock ticks.
  std::uint64_t start_ticks{};
};

/**
 * @brief The memory fields of `/proc/<pid>/status` the monitor needs.
 */
struct proc_pid_status_t
{
  /// @brief `VmRSS`; empty for kernel threads, which have no memory of their
  /// own.
  optional<std::uint64_t> resident_bytes;
  /// @brief `RssAnon`; empty before Linux 4.5.
  optional<std::uint64_t> anonymous_bytes;
};

/**
 * @brief Parses the text of `/proc/<pid>/stat`.
 * @param text The file text. `comm` may contain spaces and parentheses: it
 * runs from the first `(` to the last `)`.
 * @return The fields, or nothing when the text is not a stat line with at
 * least 22 fields.
 */
LUMEX_API optional<proc_pid_stat_t> parse_proc_pid_stat (LumexStringView text);

/**
 * @brief Parses the text of `/proc/<pid>/status`.
 * @param text The file text.
 * @return The fields that are present.
 */
LUMEX_API proc_pid_status_t parse_proc_pid_status (LumexStringView text);

/**
 * @brief The file name of a `/proc/<pid>/exe` link target.
 * @param link_target The target; ` (deleted)` at its end (the executable was
 * replaced or removed after the start) is dropped.
 * @return The part after the last `/`; empty for an empty target.
 */
LUMEX_API std::string executable_name (LumexStringView link_target);

/**
 * @brief The file name of the first argument in a `/proc/<pid>/cmdline` text.
 * @param cmdline The file text: arguments separated by `\0`.
 * @return The part of the first argument after its last `/`, or nothing when
 * the first argument is empty (kernel threads, zombies).
 */
LUMEX_API optional<std::string> first_argument_name (LumexStringView cmdline);

/**
 * @brief The Linux name rule: does a process named @p name match the
 * requested name?
 * @param requested The requested executable name.
 * @param name The process's name.
 * @param name_is_comm Whether @p name is the kernel's `comm` (the executable
 * and the command line could not be read). A `comm` of exactly
 * @ref KCOMM_LENGTH characters then also matches a longer requested name that
 * starts with it.
 */
LUMEX_API bool linux_name_matches (LumexStringView requested,
                                   LumexStringView name, bool name_is_comm);

/**
 * @brief The Windows name rule: equal without regard to ASCII case, with or
 * without `.exe` on either side.
 * @param requested The requested executable name.
 * @param name The process's executable name (Toolhelp32 gives it with
 * `.exe`).
 */
LUMEX_API bool windows_name_matches (LumexStringView requested,
                                     LumexStringView name);
} // namespace detail
} // namespace process
} // namespace resource_monitor
} // namespace applied
} // namespace lumex

#endif // !LUMEX_APPLIED_RESOURCE_MONITOR_PROCESS_DETAIL_HPP
