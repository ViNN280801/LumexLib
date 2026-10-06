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
 * name.
 * @details `LumexProcessMonitor` samples processes on request: each sample
 * gives the process's resident and private memory, its total CPU time, and its
 * share of the whole machine's CPU since the monitor's previous sample of the
 * same process. Windows and Linux; elsewhere every query reports
 * `process_query_error::unsupported`.
 */
#ifndef LUMEX_APPLIED_RESOURCE_MONITOR_PROCESS_HPP
#define LUMEX_APPLIED_RESOURCE_MONITOR_PROCESS_HPP

#include "lumex/LumexExport.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "lumex/core/expected/Expected"
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
 * @brief One sample of one process, or the sum of several (see @ref total).
 */
struct process_usage_t
{
  /// @brief The process ID; 0 in a sum.
  process_id_t pid{};
  /// @brief The executable's file name without its directory (Windows: with
  /// `.exe`). In a sum, the name the summed processes share, or empty.
  std::string name;
  /// @brief Share of the whole machine's CPU (all logical processors), 0-100,
  /// since this monitor's previous sample of the same process. Empty on the
  /// first sample of a process.
  std::optional<double> cpu_percent;
  /// @brief CPU time (user and kernel) since the process started.
  std::chrono::nanoseconds cpu_time{};
  /// @brief Logical processors the share is counted against.
  std::uint32_t logical_processors{};
  /// @brief Memory in RAM now, shared libraries included (Linux `VmRSS`,
  /// Windows `WorkingSetSize`).
  std::uint64_t resident_bytes{};
  /// @brief Memory that belongs to this process only (Linux `RssAnon`,
  /// Windows `PrivateUsage`). Empty on Linux kernels before 4.5.
  std::optional<std::uint64_t> private_bytes;
  /// @brief How many processes this value covers: 1 for a sample.
  std::size_t process_count{ 1 };
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
      = lumex::core::expected::result::Expected<process_usage_t,
                                                process_query_error>;

  LumexProcessMonitor ();
  ~LumexProcessMonitor ();
  LumexProcessMonitor (LumexProcessMonitor const &) = delete;
  LumexProcessMonitor &operator= (LumexProcessMonitor const &) = delete;
  LumexProcessMonitor (LumexProcessMonitor &&) LUMEX_NOEXCEPT;
  LumexProcessMonitor &operator= (LumexProcessMonitor &&) LUMEX_NOEXCEPT;

  /**
   * @brief Samples one process.
   * @param pid The process ID.
   * @return The sample, or `not_found`, `access_denied`, `read_failed` or
   * `unsupported`.
   */
  result_t sample (process_id_t pid);

  /**
   * @brief Samples every process with this executable name.
   * @param name The executable's file name without its directory.
   * @return One sample per process, in no particular order; empty when no
   * process has the name. Processes that exit, or may not be read, while they
   * are being sampled are left out.
   */
  std::vector<process_usage_t> sample_by_name (std::string const &name);

  /**
   * @brief Finds the processes with this executable name, without sampling
   * them.
   * @param name The executable's file name without its directory.
   * @return Their IDs, in no particular order.
   */
  static std::vector<process_id_t> find_by_name (std::string const &name);

  /// @brief The ID of the calling process.
  static process_id_t current_process_id ();

  /**
   * @brief Drops what the monitor remembers about processes that no longer
   * exist.
   */
  void forget_exited ();

private:
  struct impl_t;
  std::unique_ptr<impl_t> impl_;
};
} // namespace process
} // namespace resource_monitor
} // namespace applied
} // namespace lumex

#endif // !LUMEX_APPLIED_RESOURCE_MONITOR_PROCESS_HPP
