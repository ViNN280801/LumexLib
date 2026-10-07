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
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#ifndef PSAPI_VERSION
#define PSAPI_VERSION 2
#endif
#include <cwchar>

#include <windows.h>

// MinGW's psapi.h and tlhelp32.h need the types of windows.h declared first.
#include <psapi.h>
#include <tlhelp32.h>
#elif defined(__linux__)
#include <cerrno>

#include <fcntl.h>
#include <unistd.h>
#endif

#include "LumexProcessMonitor.hpp"
#include "detail/LumexProcessDetail.hpp"
#if defined(__linux__)
#include "lumex/applied/resource_monitor/monitor/detail/LumexProcFs.hpp"
#include "lumex/core/filesystem/LumexFilesystem"
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
    = lumex::core::expected::result::expected<reading_t, process_query_error>;

#if defined(_WIN32) || defined(_WIN64)
// ----------------------------------------------------------------- Windows --
#if defined(__clang__)
// clang-cl: the Win32 calls take raw buffers (the image path, UTF-16 text).
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#endif

std::uint64_t
file_time_value (FILETIME const &time)
{
  ULARGE_INTEGER value;
  value.LowPart = time.dwLowDateTime;
  value.HighPart = time.dwHighDateTime;
  return value.QuadPart;
}

std::string
to_utf8 (wchar_t const *text, int length)
{
  if (length <= 0)
    return std::string ();
  int const size = WideCharToMultiByte (CP_UTF8, 0, text, length, nullptr, 0,
                                        nullptr, nullptr);
  std::string out (static_cast<std::size_t> (size > 0 ? size : 0), '\0');
  if (size > 0)
    WideCharToMultiByte (CP_UTF8, 0, text, length, &out[0], size, nullptr,
                         nullptr);
  return out;
}

process_query_error
error_of_last_call ()
{
  switch (GetLastError ())
    {
    case ERROR_INVALID_PARAMETER:
      return process_query_error::not_found;
    case ERROR_ACCESS_DENIED:
      return process_query_error::access_denied;
    default:
      return process_query_error::read_failed;
    }
}

std::uint32_t
logical_processors ()
{
  DWORD const count = GetActiveProcessorCount (ALL_PROCESSOR_GROUPS);
  return count > 0 ? static_cast<std::uint32_t> (count) : 1U;
}

// Closes the process handle on every path.
struct handle_t
{
  HANDLE value{ nullptr };
  handle_t () = default;
  explicit handle_t (HANDLE h) : value (h) {}
  handle_t (handle_t const &) = delete;
  handle_t &operator= (handle_t const &) = delete;
  ~handle_t ()
  {
    if (value != nullptr && value != INVALID_HANDLE_VALUE)
      CloseHandle (value);
  }
};

bool
is_running (HANDLE process)
{
  DWORD code = 0;
  return GetExitCodeProcess (process, &code) != 0 && code == STILL_ACTIVE;
}

reading_result_t
read_process (process_id_t pid)
{
  handle_t const process (OpenProcess (
      PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid));
  if (process.value == nullptr)
    return reading_result_t (unexpect, error_of_last_call ());
  // A handle to a process that has exited can still be opened while another
  // handle keeps the process object alive.
  if (!is_running (process.value))
    return reading_result_t (unexpect, process_query_error::not_found);

  FILETIME creation{};
  FILETIME exit_time{};
  FILETIME kernel{};
  FILETIME user{};
  if (GetProcessTimes (process.value, &creation, &exit_time, &kernel, &user)
      == 0)
    return reading_result_t (unexpect, error_of_last_call ());

  PROCESS_MEMORY_COUNTERS_EX memory{};
  memory.cb = sizeof (memory);
  if (GetProcessMemoryInfo (
          process.value, reinterpret_cast<PROCESS_MEMORY_COUNTERS *> (&memory),
          sizeof (memory))
      == 0)
    return reading_result_t (unexpect, error_of_last_call ());

  wchar_t path[MAX_PATH * 4]{};
  DWORD length = static_cast<DWORD> (sizeof (path) / sizeof (path[0]));
  std::string name;
  if (QueryFullProcessImageNameW (process.value, 0, path, &length) != 0)
    {
      std::string const full = to_utf8 (path, static_cast<int> (length));
      std::size_t const slash = full.find_last_of ("\\/");
      name = slash == std::string::npos ? full : full.substr (slash + 1);
    }

  reading_t reading;
  reading.usage.pid = pid;
  reading.usage.name = name;
  // FILETIME counts 100 ns units.
  reading.usage.cpu_time
      = std::chrono::nanoseconds (static_cast<std::int64_t> (
          (file_time_value (kernel) + file_time_value (user)) * 100U));
  reading.usage.logical_processors = logical_processors ();
  reading.usage.resident_bytes = memory.WorkingSetSize;
  reading.usage.private_bytes = memory.PrivateUsage;
  reading.start_time = file_time_value (creation);
  return reading;
}

std::vector<process_id_t>
find_processes (std::string const &name)
{
  std::vector<process_id_t> found;
  handle_t const snapshot (CreateToolhelp32Snapshot (TH32CS_SNAPPROCESS, 0));
  if (snapshot.value == INVALID_HANDLE_VALUE)
    return found;
  PROCESSENTRY32W entry{};
  entry.dwSize = sizeof (entry);
  for (BOOL more = Process32FirstW (snapshot.value, &entry); more != 0;
       more = Process32NextW (snapshot.value, &entry))
    {
      std::string const exe = to_utf8 (
          entry.szExeFile, static_cast<int> (wcslen (entry.szExeFile)));
      if (detail::windows_name_matches (name, exe))
        found.push_back (entry.th32ProcessID);
    }
  return found;
}

// When the process was created; 0 when it cannot be read (no access, or it
// is gone).
std::uint64_t
creation_time_of (process_id_t pid)
{
  handle_t const process (
      OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
  if (process.value == nullptr)
    return 0;
  FILETIME creation{};
  FILETIME exit_time{};
  FILETIME kernel{};
  FILETIME user{};
  if (GetProcessTimes (process.value, &creation, &exit_time, &kernel, &user)
      == 0)
    return 0;
  return file_time_value (creation);
}

// Every process with its parent, from one Toolhelp32 snapshot.
std::vector<detail::process_link_t>
read_process_links ()
{
  std::vector<detail::process_link_t> links;
  handle_t const snapshot (CreateToolhelp32Snapshot (TH32CS_SNAPPROCESS, 0));
  if (snapshot.value == INVALID_HANDLE_VALUE)
    return links;
  PROCESSENTRY32W entry{};
  entry.dwSize = sizeof (entry);
  for (BOOL more = Process32FirstW (snapshot.value, &entry); more != 0;
       more = Process32NextW (snapshot.value, &entry))
    {
      detail::process_link_t link;
      link.pid = entry.th32ProcessID;
      link.parent_pid = entry.th32ParentProcessID;
      link.start_time = creation_time_of (link.pid);
      links.push_back (link);
    }
  return links;
}

bool
process_exists (process_id_t pid)
{
  handle_t const process (
      OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
  if (process.value == nullptr)
    return GetLastError () == ERROR_ACCESS_DENIED;
  return is_running (process.value);
}

process_id_t
own_process_id ()
{
  return static_cast<process_id_t> (GetCurrentProcessId ());
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#elif defined(__linux__)
// ------------------------------------------------------------------- Linux --

struct proc_text_t
{
  optional<std::string> text;
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
  lumex::filesystem_result<lumex::path> const target
      = lumex::filesystem::read_symlink (
          lumex::path ("/proc/" + std::to_string (pid) + "/exe"));
  if (target)
    {
      std::string name = detail::executable_name (target.value ().string ());
      if (!name.empty ())
        return { std::move (name), false };
    }
  proc_text_t const cmdline = read_proc_file (pid, "cmdline");
  if (cmdline.text)
    if (optional<std::string> name
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
  optional<detail::proc_pid_stat_t> const stat
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

// The IDs of the /proc/<digits> entries.
std::vector<process_id_t>
list_process_ids ()
{
  std::vector<process_id_t> ids;
  for (lumex::directory_iterator it (lumex::path ("/proc")), end; it != end;
       ++it)
    {
      std::string const entry = it->path ().filename ().string ();
      if (entry.empty ()
          || !std::all_of (entry.begin (), entry.end (),
                           [] (char c) { return c >= '0' && c <= '9'; }))
        continue;
      try
        {
          ids.push_back (static_cast<process_id_t> (std::stoul (entry)));
        }
      catch (std::exception const &)
        {
          continue;
        }
    }
  return ids;
}

// Every live process with its parent; zombies (exited, not reaped) are left
// out, they have no children and use nothing.
std::vector<detail::process_link_t>
read_process_links ()
{
  std::vector<detail::process_link_t> links;
  for (process_id_t const pid : list_process_ids ())
    {
      proc_text_t const stat_text = read_proc_file (pid, "stat");
      if (!stat_text.text)
        continue;
      optional<detail::proc_pid_stat_t> const stat
          = detail::parse_proc_pid_stat (*stat_text.text);
      if (!stat || stat->state == 'Z' || stat->state == 'X')
        continue;
      detail::process_link_t link;
      link.pid = pid;
      link.parent_pid = static_cast<process_id_t> (stat->parent_pid);
      link.start_time = stat->start_ticks;
      links.push_back (link);
    }
  return links;
}

std::vector<process_id_t>
find_processes (std::string const &name)
{
  std::vector<process_id_t> found;
  for (process_id_t const pid : list_process_ids ())
    {
      proc_text_t const stat_text = read_proc_file (pid, "stat");
      if (!stat_text.text)
        continue;
      optional<detail::proc_pid_stat_t> const stat
          = detail::parse_proc_pid_stat (*stat_text.text);
      if (!stat)
        continue;
      // An exited process that its parent has not reaped yet has no memory,
      // no readable executable and only the kernel's cut comm for a name: it
      // would be listed under a truncated name and consume nothing.
      if (stat->state == 'Z' || stat->state == 'X')
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
  return lumex::filesystem::exists (
      lumex::path ("/proc/" + std::to_string (pid)));
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

std::vector<detail::process_link_t>
read_process_links ()
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
LumexProcessMonitor::sample_one (process_id_t pid)
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
        usage.cpu_percent = std::min (
            std::max (100. * cpu / (wall * usage.logical_processors), 0.),
            100.);
    }
  previous_t &state = impl_->previous[pid];
  state.start_time = reading->start_time;
  state.cpu_time = usage.cpu_time;
  state.taken = now;
  return usage;
}

namespace
{
process_member_t
member_of (process_usage_t const &usage)
{
  process_member_t member;
  member.pid = usage.pid;
  member.name = usage.name;
  member.cpu_percent = usage.cpu_percent;
  member.cpu_time = usage.cpu_time;
  member.resident_bytes = usage.resident_bytes;
  member.private_bytes = usage.private_bytes;
  return member;
}
} // namespace

LUMEX_PUBLIC_API LumexProcessMonitor::result_t
LumexProcessMonitor::sample_tree (process_id_t pid,
                                  std::vector<process_id_t> const &below,
                                  bool breakdown)
{
  result_t top = sample_one (pid);
  if (!top.has_value () || below.empty ())
    return top;
  std::vector<process_usage_t> usages;
  usages.push_back (*top);
  for (process_id_t const child : below)
    {
      result_t usage = sample_one (child);
      if (usage.has_value ())
        usages.push_back (std::move (*usage));
    }
  if (usages.size () == 1)
    return top;
  process_usage_t sum = total (usages);
  sum.pid = top->pid;
  sum.name = top->name;
  if (breakdown)
    for (process_usage_t const &usage : usages)
      sum.members.push_back (member_of (usage));
  return sum;
}

LUMEX_PUBLIC_API LumexProcessMonitor::result_t
LumexProcessMonitor::sample (process_id_t pid, bool include_children,
                             bool breakdown)
{
  if (!include_children)
    return sample_one (pid);
  return sample_tree (pid, find_descendants (pid), breakdown);
}

LUMEX_PUBLIC_API std::vector<process_usage_t>
LumexProcessMonitor::sample_by_name (std::string const &name,
                                     bool include_children, bool breakdown)
{
  std::vector<process_id_t> const found = find_processes (name);
  std::vector<process_usage_t> usages;
  if (!include_children)
    {
      for (process_id_t const pid : found)
        {
          result_t usage = sample_one (pid);
          if (usage.has_value ())
            usages.push_back (std::move (*usage));
        }
      return usages;
    }
  for (detail::process_tree_t const &tree :
       detail::process_trees (found, read_process_links ()))
    {
      result_t usage = sample_tree (tree.root, tree.descendants, breakdown);
      if (usage.has_value ())
        usages.push_back (std::move (*usage));
    }
  return usages;
}

LUMEX_PUBLIC_API std::vector<process_id_t>
LumexProcessMonitor::find_descendants (process_id_t pid)
{
  return detail::descendants_of (pid, read_process_links ());
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
