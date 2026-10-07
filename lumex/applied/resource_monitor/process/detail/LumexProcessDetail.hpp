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
 * @brief Text parsers for Linux `/proc/<pid>` files, the executable name
 * rules and the process tree rules of `LumexProcessMonitor`.
 * @details Pure functions on text and on lists of process links, so they work,
 * and are tested, on any platform; the monitor applies the Linux ones on Linux
 * and the Windows one on Windows.
 */
#ifndef LUMEX_APPLIED_RESOURCE_MONITOR_PROCESS_DETAIL_HPP
#define LUMEX_APPLIED_RESOURCE_MONITOR_PROCESS_DETAIL_HPP

#include "lumex/LumexExport.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

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
  /// @brief Field 4, the ID of the parent process (0 for the first process and
  /// for a process whose parent is outside its PID namespace).
  std::uint64_t parent_pid{};
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

/// @brief A process ID (`DWORD` on Windows, `pid_t` on POSIX).
using process_id_t = std::uint32_t;

/**
 * @brief One process and its parent, as the process tree rules need them.
 */
struct process_link_t
{
  /// @brief The process ID.
  process_id_t pid{};
  /// @brief The ID of its parent process.
  process_id_t parent_pid{};
  /// @brief When the process started, in any unit that grows with time (Linux:
  /// clock ticks after boot, Windows: the creation `FILETIME`); 0 when it is
  /// not known.
  std::uint64_t start_time{};
};

/**
 * @brief A process and every process below it.
 */
struct process_tree_t
{
  /// @brief The top process of the tree.
  process_id_t root{};
  /// @brief The processes below it, nearest first (children, then their
  /// children, ...).
  std::vector<process_id_t> descendants;
};

/**
 * @brief Parses the text of `/proc/<pid>/stat`.
 * @param text The file text. `comm` may contain spaces and parentheses: it
 * runs from the first `(` to the last `)`.
 * @return The fields, or nothing when the text is not a stat line with at
 * least 22 fields.
 */
LUMEX_API optional<proc_pid_stat_t>
parse_proc_pid_stat (lumex_string_view text);

/**
 * @brief Parses the text of `/proc/<pid>/status`.
 * @param text The file text.
 * @return The fields that are present.
 */
LUMEX_API proc_pid_status_t parse_proc_pid_status (lumex_string_view text);

/**
 * @brief The file name of a `/proc/<pid>/exe` link target.
 * @param link_target The target; ` (deleted)` at its end (the executable was
 * replaced or removed after the start) is dropped.
 * @return The part after the last `/`; empty for an empty target.
 */
LUMEX_API std::string executable_name (lumex_string_view link_target);

/**
 * @brief The file name of the first argument in a `/proc/<pid>/cmdline` text.
 * @param cmdline The file text: arguments separated by `\0`.
 * @return The part of the first argument after its last `/`, or nothing when
 * the first argument is empty (kernel threads, zombies).
 */
LUMEX_API optional<std::string>
first_argument_name (lumex_string_view cmdline);

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
LUMEX_API bool linux_name_matches (lumex_string_view requested,
                                   lumex_string_view name, bool name_is_comm);

/**
 * @brief Every process below one process: its children, their children, and
 * so on.
 * @param root The process ID to start from. It does not have to be in
 * @p links.
 * @param links The processes that exist, with their parents.
 * @return The IDs below @p root, nearest first, each once. A process is left
 * out (with everything below it) when it started before its parent: the
 * parent's ID was then reused by another process, and the link is stale. A
 * process that is its own parent (Windows' idle process) has no children of
 * its own.
 */
LUMEX_API std::vector<process_id_t>
descendants_of (process_id_t root, std::vector<process_link_t> const &links);

/**
 * @brief Groups processes into trees, so that every process is counted once.
 * @param roots Processes, for example every process with one name.
 * @param links The processes that exist, with their parents.
 * @return A tree for every process of @p roots that is not below another
 * process of @p roots, in ascending order of the root ID. A repeated ID gives
 * one tree.
 */
LUMEX_API std::vector<process_tree_t>
process_trees (std::vector<process_id_t> const &roots,
               std::vector<process_link_t> const &links);

/**
 * @brief The Windows name rule: equal without regard to ASCII case, with or
 * without `.exe` on either side.
 * @param requested The requested executable name.
 * @param name The process's executable name (Toolhelp32 gives it with
 * `.exe`).
 */
LUMEX_API bool windows_name_matches (lumex_string_view requested,
                                     lumex_string_view name);
} // namespace detail
} // namespace process
} // namespace resource_monitor
} // namespace applied
} // namespace lumex

#endif // !LUMEX_APPLIED_RESOURCE_MONITOR_PROCESS_DETAIL_HPP
