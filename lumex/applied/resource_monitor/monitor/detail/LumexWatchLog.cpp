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
#include <iomanip>
#include <locale>
#include <sstream>
#include <utility>

#include "LumexWatchLog.hpp"

#include "lumex/core/utility/macros/LumexConstantMacros.hpp"

namespace lumex
{
namespace applied
{
namespace resource_monitor
{
namespace monitor
{
namespace detail
{
namespace
{
using process::LumexProcessMonitor;
using process::process_member_t;
using process::process_query_error;
using process::process_usage_t;

LUMEX_CONST_STR KDETAIL_INDENT = "    ";

std::string
name_or_unknown (std::string const &name)
{
  return name.empty () ? std::string ("?") : name;
}

// "3.1% 210.4Mb"
std::string
usage_text (lumex::core::optional::opt::optional<double> const &cpu,
            std::uint64_t resident_bytes)
{
  return format_cpu_share (cpu) + " " + format_megabytes (resident_bytes);
}

// "pid name 3.1% 210.4Mb"
std::string
detail_line (process::process_id_t pid, std::string const &name,
             lumex::core::optional::opt::optional<double> const &cpu,
             std::uint64_t resident_bytes)
{
  return std::to_string (pid) + " " + name_or_unknown (name) + " "
         + usage_text (cpu, resident_bytes);
}

std::string
error_text (process_query_error error)
{
  switch (error)
    {
    case process_query_error::not_found:
      return "exited";
    case process_query_error::access_denied:
      return "access denied";
    case process_query_error::unsupported:
      return "unsupported";
    case process_query_error::read_failed:
    default:
      return "unreadable";
    }
}
} // namespace

LUMEX_PUBLIC_API std::string
format_megabytes (std::uint64_t bytes)
{
  std::ostringstream out;
  out.imbue (std::locale::classic ());
  out << std::fixed << std::setprecision (1)
      << static_cast<double> (bytes) / (1024. * 1024.) << "Mb";
  return out.str ();
}

LUMEX_PUBLIC_API std::string
format_cpu_share (lumex::core::optional::opt::optional<double> share)
{
  if (!share)
    return "n/a";
  std::ostringstream out;
  out.imbue (std::locale::classic ());
  out << std::fixed << std::setprecision (1) << *share << "%";
  return out.str ();
}

LUMEX_PUBLIC_API watch_entry_t
format_pid_entry (process::process_id_t pid,
                  LumexProcessMonitor::result_t const &result, bool breakdown)
{
  watch_entry_t entry;
  if (!result.has_value ())
    {
      entry.head = std::to_string (pid) + " " + error_text (result.error ());
      return entry;
    }
  process_usage_t const &usage = *result;
  std::string label
      = name_or_unknown (usage.name) + "[" + std::to_string (pid);
  if (usage.process_count > 1)
    label += " +" + std::to_string (usage.process_count - 1);
  entry.head
      = label + "] " + usage_text (usage.cpu_percent, usage.resident_bytes);
  if (breakdown)
    for (process_member_t const &member : usage.members)
      entry.details.push_back (detail_line (
          member.pid, member.name, member.cpu_percent, member.resident_bytes));
  return entry;
}

LUMEX_PUBLIC_API watch_entry_t
format_name_entry (std::string const &name,
                   std::vector<process_usage_t> const &usages, bool breakdown)
{
  watch_entry_t entry;
  if (usages.empty ())
    {
      entry.head = name + "[x0] not running";
      return entry;
    }
  process_usage_t const sum = process::total (usages);
  entry.head = name + "[x" + std::to_string (sum.process_count) + "] "
               + usage_text (sum.cpu_percent, sum.resident_bytes);
  if (breakdown && sum.process_count > 1)
    for (process_usage_t const &usage : usages)
      {
        if (usage.members.empty ())
          entry.details.push_back (detail_line (
              usage.pid, usage.name, usage.cpu_percent, usage.resident_bytes));
        else
          for (process_member_t const &member : usage.members)
            entry.details.push_back (detail_line (member.pid, member.name,
                                                  member.cpu_percent,
                                                  member.resident_bytes));
      }
  return entry;
}

LUMEX_PUBLIC_API std::string
compose_log_line (std::string const &prefix,
                  std::vector<watch_entry_t> const &entries)
{
  std::string text = prefix;
  for (watch_entry_t const &entry : entries)
    text += " | " + entry.head;
  text += "\n";
  for (watch_entry_t const &entry : entries)
    for (std::string const &line : entry.details)
      text += std::string (KDETAIL_INDENT) + line + "\n";
  return text;
}

LUMEX_PUBLIC_API std::vector<watch_entry_t>
collect_watch_entries (LumexProcessMonitor &monitor,
                       process::watch_list_t const &list)
{
  std::vector<watch_entry_t> entries;
  for (process::process_id_t const pid : list.pids)
    entries.push_back (format_pid_entry (
        pid, monitor.sample (pid, list.include_children, list.breakdown),
        list.breakdown));
  for (std::string const &name : list.names)
    entries.push_back (format_name_entry (
        name,
        monitor.sample_by_name (name, list.include_children, list.breakdown),
        list.breakdown));
  // A process that has exited keeps no state in the monitor.
  monitor.forget_exited ();
  return entries;
}
} // namespace detail
} // namespace monitor
} // namespace resource_monitor
} // namespace applied
} // namespace lumex
