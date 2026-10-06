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

#define LUMEX_IMPLEMENTATION
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#if defined(__linux__)
#include <cerrno>
#include <filesystem>
#include <system_error>

#include <fcntl.h>
#include <unistd.h>
#endif

#include "LumexProcessMonitor.hpp"
#include "detail/LumexProcessDetail.hpp"
#if defined(__linux__)
#include "lumex/applied/resource_monitor/monitor/detail/LumexProcFs.hpp"
#endif

namespace lumex
{
namespace applied
{
namespace resource_monitor
{
namespace process
{
namespace
{
using monotonic_clock = std::chrono::steady_clock;
using lumex::core::expected::result::unexpect;

// One reading of a process, before the CPU share is known.
struct reading_t
{
  process_usage_t usage;
  // Identifies the process behind the ID: a reused ID has another start time.
  std::uint64_t start_time{};
};

using reading_result_t
    = lumex::core::expected::result::Expected<reading_t, process_query_error>;

#if defined(__linux__)
// ------------------------------------------------------------------- Linux --

struct proc_text_t
{
  std::optional<std::string> text;
  process_query_error error{ process_query_error::read_failed };
};

// Reads /proc/<pid>/<file>, keeping why it failed.
proc_text_t
read_proc_file (process_id_t pid, char const *file)
{
  std::string const path
      = "/proc/" + std::to_string (pid) + "/" + std::string (file);
  proc_text_t result;
  int const fd = ::open (path.c_str (), O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    {
      result.error = (errno == ENOENT || errno == ESRCH)
                         ? process_query_error::not_found
                     : (errno == EACCES || errno == EPERM)
                         ? process_query_error::access_denied
                         : process_query_error::read_failed;
      return result;
    }
  std::string text;
  char buffer[4096];
  for (;;)
    {
      ssize_t const n = ::read (fd, buffer, sizeof (buffer));
      if (n > 0)
        {
          text.append (buffer, static_cast<std::size_t> (n));
          continue;
        }
      if (n < 0 && errno == EINTR)
        continue;
      if (n < 0)
        result.error = (errno == ESRCH) ? process_query_error::not_found
                                        : process_query_error::read_failed;
      else
        result.text = std::move (text);
      break;
    }
  ::close (fd);
  return result;
}

std::uint32_t
logical_processors ()
{
  long const count = ::sysconf (_SC_NPROCESSORS_ONLN);
  return count > 0 ? static_cast<std::uint32_t> (count) : 1U;
}

// The executable name: /proc/<pid>/exe, else argv[0], else comm.
std::pair<std::string, bool>
process_name (process_id_t pid, std::string const &comm)
{
  std::error_code ec;
  std::filesystem::path const target = std::filesystem::read_symlink (
      "/proc/" + std::to_string (pid) + "/exe", ec);
  if (!ec)
    {
      std::string name = detail::executable_name (target.string ());
      if (!name.empty ())
        return { std::move (name), false };
    }
  proc_text_t const cmdline = read_proc_file (pid, "cmdline");
  if (cmdline.text)
    if (std::optional<std::string> name
        = detail::first_argument_name (*cmdline.text))
      return { std::move (*name), false };
  return { comm, true };
}

reading_result_t
read_process (process_id_t pid)
{
  proc_text_t const stat_text = read_proc_file (pid, "stat");
  if (!stat_text.text)
    return reading_result_t (unexpect, stat_text.error);
  std::optional<detail::proc_pid_stat_t> const stat
      = detail::parse_proc_pid_stat (*stat_text.text);
  if (!stat)
    return reading_result_t (unexpect, process_query_error::read_failed);

  proc_text_t const status_text = read_proc_file (pid, "status");
  if (!status_text.text)
    return reading_result_t (unexpect, status_text.error);
  detail::proc_pid_status_t const status
      = detail::parse_proc_pid_status (*status_text.text);

  long const ticks = ::sysconf (_SC_CLK_TCK);
  std::uint64_t const ticks_per_second
      = ticks > 0 ? static_cast<std::uint64_t> (ticks) : 100U;
  std::uint64_t const cpu_ticks = stat->utime_ticks + stat->stime_ticks;

  reading_t reading;
  reading.usage.pid = pid;
  reading.usage.name = process_name (pid, stat->comm).first;
  reading.usage.cpu_time
      = std::chrono::nanoseconds (static_cast<std::int64_t> (
          cpu_ticks / ticks_per_second * 1000000000U
          + cpu_ticks % ticks_per_second * 1000000000U / ticks_per_second));
  reading.usage.logical_processors = logical_processors ();
  reading.usage.resident_bytes = status.resident_bytes.value_or (0);
  reading.usage.private_bytes = status.anonymous_bytes;
  reading.start_time = stat->start_ticks;
  return reading;
}

std::vector<process_id_t>
find_processes (std::string const &name)
{
  std::vector<process_id_t> found;
  std::error_code ec;
  for (std::filesystem::directory_iterator it ("/proc", ec), end;
       !ec && it != end; it.increment (ec))
    {
      std::string const entry = it->path ().filename ().string ();
      if (entry.empty ()
          || !std::all_of (entry.begin (), entry.end (),
                           [] (char c) { return c >= '0' && c <= '9'; }))
        continue;
      process_id_t pid{};
      try
        {
          pid = static_cast<process_id_t> (std::stoul (entry));
        }
      catch (std::exception const &)
        {
          continue;
        }
      proc_text_t const stat_text = read_proc_file (pid, "stat");
      if (!stat_text.text)
        continue;
      std::optional<detail::proc_pid_stat_t> const stat
          = detail::parse_proc_pid_stat (*stat_text.text);
      if (!stat)
        continue;
      std::pair<std::string, bool> const resolved
          = process_name (pid, stat->comm);
      if (detail::linux_name_matches (name, resolved.first, resolved.second))
        found.push_back (pid);
    }
  return found;
}

bool
process_exists (process_id_t pid)
{
  std::error_code ec;
  return std::filesystem::exists ("/proc/" + std::to_string (pid), ec);
}

process_id_t
own_process_id ()
{
  return static_cast<process_id_t> (::getpid ());
}

#else
// ------------------------------------------------------------- unsupported --

reading_result_t
read_process (process_id_t)
{
  return reading_result_t (unexpect, process_query_error::unsupported);
}

std::vector<process_id_t>
find_processes (std::string const &)
{
  return {};
}

bool
process_exists (process_id_t)
{
  return false;
}

process_id_t
own_process_id ()
{
  return 0;
}
#endif

// The previous sample of one process.
struct previous_t
{
  std::uint64_t start_time{};
  std::chrono::nanoseconds cpu_time{};
  monotonic_clock::time_point taken;
};
} // namespace

struct LumexProcessMonitor::impl_t
{
  std::mutex mutex;
  std::map<process_id_t, previous_t> previous;
};

LumexProcessMonitor::LumexProcessMonitor () : impl_ (new impl_t ()) {}

LumexProcessMonitor::~LumexProcessMonitor () = default;

LumexProcessMonitor::LumexProcessMonitor (LumexProcessMonitor &&)
    LUMEX_NOEXCEPT
    = default;

LumexProcessMonitor &
LumexProcessMonitor::operator= (LumexProcessMonitor &&) LUMEX_NOEXCEPT
    = default;

LUMEX_PUBLIC_API LumexProcessMonitor::result_t
LumexProcessMonitor::sample (process_id_t pid)
{
  reading_result_t reading = read_process (pid);
  if (!reading.has_value ())
    return result_t (unexpect, reading.error ());
  monotonic_clock::time_point const now = monotonic_clock::now ();
  process_usage_t usage = std::move (reading->usage);

  std::lock_guard<std::mutex> lock (impl_->mutex);
  auto const found = impl_->previous.find (pid);
  if (found != impl_->previous.end ()
      && found->second.start_time == reading->start_time)
    {
      previous_t const &before = found->second;
      double const wall
          = std::chrono::duration<double> (now - before.taken).count ();
      double const cpu
          = std::chrono::duration<double> (usage.cpu_time - before.cpu_time)
                .count ();
      if (wall > 0. && usage.logical_processors > 0)
        usage.cpu_percent = std::clamp (
            100. * cpu / (wall * usage.logical_processors), 0., 100.);
    }
  previous_t &state = impl_->previous[pid];
  state.start_time = reading->start_time;
  state.cpu_time = usage.cpu_time;
  state.taken = now;
  return usage;
}

LUMEX_PUBLIC_API std::vector<process_usage_t>
LumexProcessMonitor::sample_by_name (std::string const &name)
{
  std::vector<process_usage_t> usages;
  for (process_id_t const pid : find_processes (name))
    {
      result_t usage = sample (pid);
      if (usage.has_value ())
        usages.push_back (std::move (*usage));
    }
  return usages;
}

LUMEX_PUBLIC_API std::vector<process_id_t>
LumexProcessMonitor::find_by_name (std::string const &name)
{
  return find_processes (name);
}

LUMEX_PUBLIC_API process_id_t
LumexProcessMonitor::current_process_id ()
{
  return own_process_id ();
}

LUMEX_PUBLIC_API void
LumexProcessMonitor::forget_exited ()
{
  std::lock_guard<std::mutex> lock (impl_->mutex);
  for (auto it = impl_->previous.begin (); it != impl_->previous.end ();)
    it = process_exists (it->first) ? std::next (it)
                                    : impl_->previous.erase (it);
}

LUMEX_PUBLIC_API process_usage_t
total (std::vector<process_usage_t> const &usages)
{
  process_usage_t sum;
  sum.process_count = 0;
  if (usages.empty ())
    return sum;
  sum.name = usages.front ().name;
  sum.logical_processors = usages.front ().logical_processors;
  bool all_private = true;
  std::uint64_t private_bytes = 0;
  for (process_usage_t const &usage : usages)
    {
      if (usage.name != sum.name)
        sum.name.clear ();
      if (usage.cpu_percent)
        sum.cpu_percent = sum.cpu_percent.value_or (0.) + *usage.cpu_percent;
      sum.cpu_time += usage.cpu_time;
      sum.resident_bytes += usage.resident_bytes;
      if (usage.private_bytes)
        private_bytes += *usage.private_bytes;
      else
        all_private = false;
      sum.process_count += usage.process_count;
    }
  if (all_private)
    sum.private_bytes = private_bytes;
  if (sum.cpu_percent)
    sum.cpu_percent = std::min (*sum.cpu_percent, 100.);
  return sum;
}
} // namespace process
} // namespace resource_monitor
} // namespace applied
} // namespace lumex
