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
 * @file LumexProcessMonitor.hpp
 * @brief CPU and RAM of single processes, by process ID or by executable
 * name, with the processes they started.
 * @details `LumexProcessMonitor` samples processes on request: each sample
 * gives the process's resident and private memory, its total CPU time, and its
 * share of the whole machine's CPU since the monitor's previous sample of the
 * same process. By default the processes below the asked one (its children,
 * their children, ...) are added to the sample, and the breakdown by process
 * can be asked for. Windows and Linux; elsewhere every query reports
 * `process_query_error::unsupported`.
 */
#ifndef LUMEX_APPLIED_RESOURCE_MONITOR_PROCESS_HPP
#define LUMEX_APPLIED_RESOURCE_MONITOR_PROCESS_HPP

#include "lumex/LumexExport.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "lumex/core/expected/Expected"
#include "lumex/core/optional/LumexOptional"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
namespace resource_monitor
{
/**
 * @brief CPU and RAM of single processes.
 */
namespace process
{
/// @brief A process ID (`DWORD` on Windows, `pid_t` on POSIX).
using process_id_t = std::uint32_t;

/**
 * @brief Why a process could not be sampled.
 */
enum class process_query_error : std::uint8_t
{
  /// @brief No process has this ID (it has exited, or never existed).
  not_found,
  /// @brief The process exists, but this user may not read it.
  access_denied,
  /// @brief The operating system refused the query for another reason.
  read_failed,
  /// @brief This platform is neither Windows nor Linux.
  unsupported
};

/**
 * @brief One process inside a sum: what @ref process_usage_t::members lists.
 */
struct process_member_t
{
  /// @brief The process ID.
  process_id_t pid{};
  /// @brief The executable's file name without its directory.
  std::string name;
  /// @brief Share of the whole machine's CPU, 0-100, since this monitor's
  /// previous sample of the process; empty on its first sample.
  lumex::core::optional::opt::optional<double> cpu_percent;
  /// @brief CPU time (user and kernel) since the process started.
  std::chrono::nanoseconds cpu_time{};
  /// @brief Memory in RAM now, shared libraries included.
  std::uint64_t resident_bytes{};
  /// @brief Memory that belongs to this process only; empty when the system
  /// does not tell.
  lumex::core::optional::opt::optional<std::uint64_t> private_bytes;
};

/**
 * @brief One sample of one process, the sum of a process and the processes
 * below it, or the sum of several (see @ref total).
 */
struct process_usage_t
{
  /// @brief The process ID; 0 in a sum made by @ref total. For a process with
  /// the processes below it, the ID of the top one.
  process_id_t pid{};
  /// @brief The executable's file name without its directory (Windows: with
  /// `.exe`). In a sum made by @ref total, the name the summed processes
  /// share, or empty. For a process with the processes below it, the name of
  /// the top one.
  std::string name;
  /// @brief Share of the whole machine's CPU (all logical processors), 0-100,
  /// since this monitor's previous sample of the same process. Empty on the
  /// first sample of a process.
  lumex::core::optional::opt::optional<double> cpu_percent;
  /// @brief CPU time (user and kernel) since the process started.
  std::chrono::nanoseconds cpu_time{};
  /// @brief Logical processors the share is counted against.
  std::uint32_t logical_processors{};
  /// @brief Memory in RAM now, shared libraries included (Linux `VmRSS`,
  /// Windows `WorkingSetSize`).
  std::uint64_t resident_bytes{};
  /// @brief Memory that belongs to this process only (Linux `RssAnon`,
  /// Windows `PrivateUsage`). Empty on Linux kernels before 4.5.
  lumex::core::optional::opt::optional<std::uint64_t> private_bytes;
  /// @brief How many processes this value covers: 1 for a sample of one
  /// process, more for a sum.
  std::size_t process_count{ 1 };
  /// @brief The processes behind a sum, the top one first and then the ones
  /// below it. Filled only when the breakdown was asked for and the value
  /// covers more than one process; empty otherwise, and in a sum made by
  /// @ref total.
  std::vector<process_member_t> members;
};

/**
 * @brief Processes to follow: by ID, and by executable name (every process
 * with that name).
 */
struct watch_list_t
{
  /// @brief Process IDs.
  std::vector<process_id_t> pids;
  /// @brief Executable names, compared as @ref
  /// LumexProcessMonitor::sample_by_name compares them.
  std::vector<std::string> names;
  /// @brief Whether the processes below each one are counted with it (its
  /// children, their children, ...). See @ref LumexProcessMonitor::sample.
  bool include_children{ true };
  /// @brief Whether the log lists every process behind a sum, one line each.
  /// Off by default: one sum per item is logged.
  bool breakdown{ false };
};

/**
 * @brief Adds samples up, for example every process of one name.
 * @param usages The samples.
 * @return Their sum: CPU time, resident memory and the counts are added; the
 * CPU share is the sum of the shares that are known (empty when none is);
 * private memory is empty unless every sample has it; `pid` is 0.
 */
LUMEX_API process_usage_t total (std::vector<process_usage_t> const &usages);

/**
 * @class LumexProcessMonitor
 * @brief Samples the CPU and RAM of processes on request.
 *
 * @details The CPU share needs two samples of the same process: the monitor
 * remembers the CPU time and the time of its last sample of every process it
 * sampled, and the next sample reports the share of the machine the process
 * used in between. The host decides how often to sample. A process ID that
 * the system gave to a new process in between (detected by the process start
 * time) is sampled as a new process.
 *
 * Names are the executable's file name without its directory. Linux: the name
 * of `/proc/<pid>/exe`, else of the first command-line argument, else the
 * kernel's `comm` (which is cut to 15 characters, so a 15-character `comm`
 * also matches a longer requested name that starts with it). Windows: the
 * Toolhelp32 name, compared without regard to case and with or without
 * `.exe`.
 *
 * @note The methods are thread-safe. The class is movable, not copyable; a
 * moved-from monitor may only be destroyed or assigned to.
 *
 * @par Example
 * @code
 * using namespace lumex::applied::resource_monitor::process;
 * LumexProcessMonitor monitor;
 * monitor.sample (LumexProcessMonitor::current_process_id ()); // baseline
 * // ... some time later:
 * auto const usage = monitor.sample (LumexProcessMonitor::current_process_id
 * ()); if (usage.has_value () && usage->cpu_percent) std::cout <<
 * *usage->cpu_percent << "% " << usage->resident_bytes << '\n';
 * @endcode
 */
class LUMEX_API LumexProcessMonitor
{
public:
  /// @brief The result of a query: a sample, or why there is none.
  using result_t
      = lumex::core::expected::result::expected<process_usage_t,
                                                process_query_error>;

  LumexProcessMonitor ();
  ~LumexProcessMonitor ();
  LumexProcessMonitor (LumexProcessMonitor const &) = delete;
  LumexProcessMonitor &operator= (LumexProcessMonitor const &) = delete;
  LumexProcessMonitor (LumexProcessMonitor &&) LUMEX_NOEXCEPT;
  LumexProcessMonitor &operator= (LumexProcessMonitor &&) LUMEX_NOEXCEPT;

  /**
   * @brief Samples one process, with the processes below it.
   * @param pid The process ID.
   * @param include_children Whether the processes below it (its children,
   * their children, ...) are added up with it: CPU time, CPU share and memory
   * are sums, `process_count` counts them, `pid` and `name` are the top
   * process's. A process below it that exits, or may not be read, while it is
   * being sampled is left out. A process that started after the previous
   * sample has no CPU share yet, so the share of a sum counts the processes
   * that have one.
   * @param breakdown Whether `members` lists every process behind the sum. It
   * stays empty when there is only one.
   * @return The sample, or `not_found`, `access_denied`, `read_failed` or
   * `unsupported` for the top process.
   */
  result_t sample (process_id_t pid, bool include_children = true,
                   bool breakdown = false);

  /**
   * @brief Samples the processes with this executable name, with the
   * processes below them.
   * @param name The executable's file name without its directory.
   * @param include_children Whether the processes below each one are added up
   * with it, as @ref sample does. Every process is counted once: a process of
   * this name that lies below another one is part of that one's sum, so the
   * result has one sample per tree. `false`: one sample per process.
   * @param breakdown As in @ref sample, for every sample of the result.
   * @return The samples, in no particular order; empty when no process has the
   * name. Processes that exit, or may not be read, while they are being
   * sampled are left out, and so are Linux zombies (exited, not yet reaped by
   * their parent).
   */
  std::vector<process_usage_t> sample_by_name (std::string const &name,
                                               bool include_children = true,
                                               bool breakdown = false);

  /**
   * @brief Finds the processes with this executable name, without sampling
   * them.
   * @param name The executable's file name without its directory.
   * @return Their IDs, in no particular order; Linux zombies (exited, not yet
   * reaped by their parent) are left out.
   */
  static std::vector<process_id_t> find_by_name (std::string const &name);

  /**
   * @brief Finds every process below one process: its children, their
   * children, and so on.
   * @param pid The process ID.
   * @return Their IDs, nearest first, each once; empty when it has none or
   * does not exist. A process whose parent link is stale (the parent's ID was
   * reused after the parent exited) is not listed, and Linux zombies are left
   * out.
   */
  static std::vector<process_id_t> find_descendants (process_id_t pid);

  /// @brief The ID of the calling process.
  static process_id_t current_process_id ();

  /**
   * @brief Drops what the monitor remembers about processes that no longer
   * exist.
   */
  void forget_exited ();

private:
  struct impl_t;
  // One process, without the processes below it.
  result_t sample_one (process_id_t pid);
  // A top process and the processes below it, added up.
  result_t sample_tree (process_id_t pid,
                        std::vector<process_id_t> const &below,
                        bool breakdown);
  std::unique_ptr<impl_t> impl_;
};
} // namespace process
} // namespace resource_monitor
} // namespace applied
} // namespace lumex

#endif // !LUMEX_APPLIED_RESOURCE_MONITOR_PROCESS_HPP
