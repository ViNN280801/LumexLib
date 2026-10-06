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
#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "LumexProcessDetail.hpp"
#include "lumex/applied/resource_monitor/monitor/detail/LumexProcFs.hpp"

namespace lumex
{
namespace applied
{
namespace resource_monitor
{
namespace process
{
namespace detail
{
namespace
{
constexpr std::string_view KDELETED_SUFFIX = " (deleted)";
constexpr std::string_view KEXE_SUFFIX = ".exe";

char
ascii_lower (char c)
{
  return (c >= 'A' && c <= 'Z') ? static_cast<char> (c - 'A' + 'a') : c;
}

bool
equal_ignoring_ascii_case (std::string_view a, std::string_view b)
{
  if (a.size () != b.size ())
    return false;
  for (std::size_t i = 0; i < a.size (); ++i)
    if (ascii_lower (a[i]) != ascii_lower (b[i]))
      return false;
  return true;
}

std::string_view
without_exe_suffix (std::string_view name)
{
  if (name.size () > KEXE_SUFFIX.size ()
      && equal_ignoring_ascii_case (
          name.substr (name.size () - KEXE_SUFFIX.size ()), KEXE_SUFFIX))
    return name.substr (0, name.size () - KEXE_SUFFIX.size ());
  return name;
}

std::string_view
after_last_slash (std::string_view path)
{
  std::size_t const slash = path.rfind ('/');
  return slash == std::string_view::npos ? path : path.substr (slash + 1);
}
} // namespace

LUMEX_PUBLIC_API std::optional<proc_pid_stat_t>
parse_proc_pid_stat (std::string_view text)
{
  std::size_t const open = text.find ('(');
  std::size_t const close = text.rfind (')');
  if (open == std::string_view::npos || close == std::string_view::npos
      || close < open)
    return std::nullopt;

  // Field 3 (state) is the first token after "comm)"; field N is token N - 3.
  std::istringstream fields (std::string (text.substr (close + 1)));
  std::vector<std::string> tokens;
  std::string token;
  while (tokens.size () < 20 && fields >> token)
    tokens.push_back (token);
  if (tokens.size () < 20)
    return std::nullopt;

  proc_pid_stat_t stat;
  stat.comm = std::string (text.substr (open + 1, close - open - 1));
  try
    {
      stat.utime_ticks = std::stoull (tokens.at (11));
      stat.stime_ticks = std::stoull (tokens.at (12));
      stat.start_ticks = std::stoull (tokens.at (19));
    }
  catch (std::exception const &)
    {
      return std::nullopt;
    }
  return stat;
}

LUMEX_PUBLIC_API proc_pid_status_t
parse_proc_pid_status (std::string_view text)
{
  proc_pid_status_t status;
  status.resident_bytes = monitor::detail::parse_kib_field (text, "VmRSS");
  status.anonymous_bytes = monitor::detail::parse_kib_field (text, "RssAnon");
  return status;
}

LUMEX_PUBLIC_API std::string
executable_name (std::string_view link_target)
{
  if (link_target.size () >= KDELETED_SUFFIX.size ()
      && link_target.substr (link_target.size () - KDELETED_SUFFIX.size ())
             == KDELETED_SUFFIX)
    link_target.remove_suffix (KDELETED_SUFFIX.size ());
  return std::string (after_last_slash (link_target));
}

LUMEX_PUBLIC_API std::optional<std::string>
first_argument_name (std::string_view cmdline)
{
  std::string_view const first = cmdline.substr (0, cmdline.find ('\0'));
  std::string_view const name = after_last_slash (first);
  if (name.empty ())
    return std::nullopt;
  return std::string (name);
}

LUMEX_PUBLIC_API bool
linux_name_matches (std::string_view requested, std::string_view name,
                    bool name_is_comm)
{
  if (requested.empty ())
    return false;
  if (requested == name)
    return true;
  return name_is_comm && name.size () == KCOMM_LENGTH
         && requested.size () > KCOMM_LENGTH
         && requested.substr (0, KCOMM_LENGTH) == name;
}

LUMEX_PUBLIC_API bool
windows_name_matches (std::string_view requested, std::string_view name)
{
  if (requested.empty ())
    return false;
  return equal_ignoring_ascii_case (without_exe_suffix (requested),
                                    without_exe_suffix (name));
}
} // namespace detail
} // namespace process
} // namespace resource_monitor
} // namespace applied
} // namespace lumex
