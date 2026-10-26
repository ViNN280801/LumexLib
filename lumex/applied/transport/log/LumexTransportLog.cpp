/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy
 * of this software and associated
 * documentation files (the "Software"), to deal
 * in the Software without
 * restriction, including without limitation the rights
 * to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the
 * Software, and to permit persons to whom the Software is
 * furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice
 * and this permission notice shall be included in
 * all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT
 * WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO
 * THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <atomic>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include "lumex/applied/transport/log/LumexTransportLog.hpp"
#include "lumex/core/utility/callback/LumexCallbackSlot.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
// The library's clang-cl set: -Wall there is -Weverything (as in the other
// modules' sources).
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#if __has_warning("-Wnrvo")
#pragma clang diagnostic ignored "-Wnrvo"
#endif
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

namespace lumex
{
namespace applied
{
namespace transport
{
namespace log
{
namespace
{
struct transport_log_tag;
typedef lumex::core::utility::callback::lumex_callback_slot<
    transport_log_tag, void (log_record_t const &)>
    sink_slot;

std::atomic<int> g_threshold (static_cast<int> (log_level::warning));
std::atomic<std::size_t> g_dump_limit (256);
std::atomic<bool> g_dump_unlimited (false);
std::atomic<bool> g_filter_active (false);

// The filter lists, read under the mutex only while g_filter_active is set.
struct component_filter_t
{
  std::mutex mutex;
  std::vector<std::string> allow;
  std::vector<std::string> deny;
};

component_filter_t &
filter () LUMEX_NOEXCEPT
{
  // Never destroyed: a transport may still log from a static destructor.
  static component_filter_t *const instance = new component_filter_t ();
  return *instance;
}

bool
starts_with (char const *text, std::string const &prefix) LUMEX_NOEXCEPT
{
  return std::strncmp (text, prefix.c_str (), prefix.size ()) == 0;
}

bool
component_passes (char const *component)
{
  if (!g_filter_active.load (std::memory_order_acquire))
    return true;
  component_filter_t &lists = filter ();
  std::lock_guard<std::mutex> const lock (lists.mutex);
  for (std::string const &prefix : lists.deny)
    if (starts_with (component, prefix))
      return false;
  if (lists.allow.empty ())
    return true;
  for (std::string const &prefix : lists.allow)
    if (starts_with (component, prefix))
      return true;
  return false;
}

char const *
level_name (log_level level) LUMEX_NOEXCEPT
{
  switch (level)
    {
    case log_level::trace:
      return "TRACE";
    case log_level::debug:
      return "DEBUG";
    case log_level::info:
      return "INFO";
    case log_level::warning:
      return "WARNING";
    case log_level::error:
      return "ERROR";
    case log_level::off:
      return "OFF";
    default:
      return "?";
    }
}

char const *
control_name (std::uint8_t value) LUMEX_NOEXCEPT
{
  switch (value)
    {
    case 0x02:
      return "<STX>";
    case 0x03:
      return "<ETX>";
    case 0x06:
      return "<ACK>";
    case 0x0A:
      return "<LF>";
    case 0x0D:
      return "<CR>";
    case 0x15:
      return "<NAK>";
    case 0x1A:
      return "<SUB>";
    default:
      return nullptr;
    }
}

void
default_sink (log_record_t const &record)
{
  if (record.level < log_level::warning)
    return;
  std::string line = "[lumex.transport] ";
  line += level_name (record.level);
  line += ' ';
  line += record.component;
  if (record.instance[0] != '\0')
    {
      line += ' ';
      line += record.instance;
    }
  line += ": ";
  line += record.message;
  if (!record.bytes.empty ())
    {
      line += " | ";
      line += format_dump (record.bytes);
    }
  line += '\n';
  std::cerr << line;
}
} // namespace

void
set_log_sink (log_sink_t sink) LUMEX_NOEXCEPT
{
  sink_slot::set (sink);
}

log_sink_t
get_log_sink () LUMEX_NOEXCEPT
{
  return sink_slot::get ();
}

void
set_log_level (log_level level) LUMEX_NOEXCEPT
{
  g_threshold.store (static_cast<int> (level), std::memory_order_release);
}

log_level
get_log_level () LUMEX_NOEXCEPT
{
  return static_cast<log_level> (g_threshold.load (std::memory_order_acquire));
}

void
set_component_filter (std::vector<std::string> const &allow,
                      std::vector<std::string> const &deny)
{
  component_filter_t &lists = filter ();
  std::lock_guard<std::mutex> const lock (lists.mutex);
  lists.allow = allow;
  lists.deny = deny;
  g_filter_active.store (!allow.empty () || !deny.empty (),
                         std::memory_order_release);
}

void
set_dump_limit (std::size_t bytes) LUMEX_NOEXCEPT
{
  g_dump_limit.store (bytes, std::memory_order_relaxed);
}

std::size_t
get_dump_limit () LUMEX_NOEXCEPT
{
  return g_dump_limit.load (std::memory_order_relaxed);
}

void
set_dump_unlimited (bool unlimited) LUMEX_NOEXCEPT
{
  g_dump_unlimited.store (unlimited, std::memory_order_relaxed);
}

bool
is_dump_unlimited () LUMEX_NOEXCEPT
{
  return g_dump_unlimited.load (std::memory_order_relaxed);
}

std::string
format_dump (log_bytes_t bytes)
{
  static char const digits[] = "0123456789ABCDEF";
  std::size_t const limit
      = is_dump_unlimited () ? bytes.size () : get_dump_limit ();
  std::size_t const shown = bytes.size () < limit ? bytes.size () : limit;
  std::string hex;
  std::string text;
  for (std::size_t i = 0; i < shown; ++i)
    {
      std::uint8_t const value = bytes[i];
      if (i != 0)
        hex += ' ';
      hex += digits[value >> 4u];
      hex += digits[value & 0x0Fu];
      char const *const name = control_name (value);
      if (name != nullptr)
        text += name;
      else if (value >= 0x20 && value < 0x7F)
        text += static_cast<char> (value);
      else
        text += '.';
    }
  std::string out = hex + " |" + text + "|";
  if (shown < bytes.size ())
    out += " (+" + std::to_string (bytes.size () - shown) + " B)";
  return out;
}

bool
log_enabled (log_level level, char const *component) LUMEX_NOEXCEPT
{
  if (level == log_level::off
      || static_cast<int> (level)
             < g_threshold.load (std::memory_order_acquire))
    return false;
  try
    {
      return component_passes (component);
    }
  catch (...)
    {
      return false;
    }
}

void
log_write (log_level level, char const *component, char const *instance,
           std::string const &message, log_bytes_t bytes)
{
  log_record_t record;
  record.level = level;
  record.component = component != nullptr ? component : "";
  record.instance = instance != nullptr ? instance : "";
  record.message = message.c_str ();
  record.bytes = bytes;
  sink_slot::invoke_or (&default_sink, record);
}
} // namespace log
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
