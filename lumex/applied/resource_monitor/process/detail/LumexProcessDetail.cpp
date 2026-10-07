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
#include <map>
#include <set>
#include <sstream>
#include <utility>
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
LUMEX_CONST_STR KDELETED_SUFFIX = " (deleted)";
LUMEX_CONST_STR KEXE_SUFFIX = ".exe";

char
ascii_lower (char c)
{
  return (c >= 'A' && c <= 'Z') ? static_cast<char> (c - 'A' + 'a') : c;
}

bool
equal_ignoring_ascii_case (lumex_string_view a, lumex_string_view b)
{
  if (a.size () != b.size ())
    return false;
  for (std::size_t i = 0; i < a.size (); ++i)
    if (ascii_lower (a[i]) != ascii_lower (b[i]))
      return false;
  return true;
}

lumex_string_view
without_exe_suffix (lumex_string_view name)
{
  lumex_string_view const exe (KEXE_SUFFIX);
  if (name.size () > exe.size ()
      && equal_ignoring_ascii_case (name.substr (name.size () - exe.size ()),
                                    exe))
    return name.substr (0, name.size () - exe.size ());
  return name;
}

lumex_string_view
after_last_slash (lumex_string_view path)
{
  std::size_t const slash = path.rfind ('/');
  return slash == lumex_string_view::npos ? path : path.substr (slash + 1);
}
} // namespace

LUMEX_PUBLIC_API optional<proc_pid_stat_t>
parse_proc_pid_stat (lumex_string_view text)
{
  std::size_t const open = text.find ('(');
  std::size_t const close = text.rfind (')');
  if (open == lumex_string_view::npos || close == lumex_string_view::npos
      || close < open)
    return nullopt;

  // Field 3 (state) is the first token after "comm)"; field N is token N - 3.
  std::istringstream fields (std::string (text.substr (close + 1)));
  std::vector<std::string> tokens;
  std::string token;
  while (tokens.size () < 20 && fields >> token)
    tokens.push_back (token);
  if (tokens.size () < 20)
    return nullopt;

  proc_pid_stat_t stat;
  stat.comm = std::string (text.substr (open + 1, close - open - 1));
  stat.state = tokens.at (0).front ();
  try
    {
      stat.parent_pid = std::stoull (tokens.at (1));
      stat.utime_ticks = std::stoull (tokens.at (11));
      stat.stime_ticks = std::stoull (tokens.at (12));
      stat.start_ticks = std::stoull (tokens.at (19));
    }
  catch (std::exception const &)
    {
      return nullopt;
    }
  return stat;
}

LUMEX_PUBLIC_API proc_pid_status_t
parse_proc_pid_status (lumex_string_view text)
{
  proc_pid_status_t status;
  status.resident_bytes = monitor::detail::parse_kib_field (text, "VmRSS");
  status.anonymous_bytes = monitor::detail::parse_kib_field (text, "RssAnon");
  return status;
}

LUMEX_PUBLIC_API std::string
executable_name (lumex_string_view link_target)
{
  lumex_string_view const deleted (KDELETED_SUFFIX);
  if (link_target.ends_with (deleted))
    link_target.remove_suffix (deleted.size ());
  return std::string (after_last_slash (link_target));
}

LUMEX_PUBLIC_API optional<std::string>
first_argument_name (lumex_string_view cmdline)
{
  lumex_string_view const first = cmdline.substr (0, cmdline.find ('\0'));
  lumex_string_view const name = after_last_slash (first);
  if (name.empty ())
    return nullopt;
  return std::string (name);
}

LUMEX_PUBLIC_API bool
linux_name_matches (lumex_string_view requested, lumex_string_view name,
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

LUMEX_PUBLIC_API std::vector<process_id_t>
descendants_of (process_id_t root, std::vector<process_link_t> const &links)
{
  std::map<process_id_t, std::vector<process_link_t const *>> children;
  std::map<process_id_t, std::uint64_t> start_times;
  for (process_link_t const &link : links)
    {
      start_times[link.pid] = link.start_time;
      if (link.pid != link.parent_pid)
        children[link.parent_pid].push_back (&link);
    }

  std::vector<process_id_t> found;
  std::set<process_id_t> seen;
  seen.insert (root);
  // Breadth first: `found` is the queue, read from `next`.
  std::vector<process_id_t> queue (1, root);
  for (std::size_t next = 0; next < queue.size (); ++next)
    {
      process_id_t const parent = queue[next];
      auto const parent_start = start_times.find (parent);
      std::uint64_t const parent_started
          = parent_start == start_times.end () ? 0U : parent_start->second;
      auto const below = children.find (parent);
      if (below == children.end ())
        continue;
      for (process_link_t const *child : below->second)
        {
          if (seen.count (child->pid) != 0)
            continue;
          if (parent_started != 0 && child->start_time != 0
              && child->start_time < parent_started)
            continue;
          seen.insert (child->pid);
          found.push_back (child->pid);
          queue.push_back (child->pid);
        }
    }
  return found;
}

LUMEX_PUBLIC_API std::vector<process_tree_t>
process_trees (std::vector<process_id_t> const &roots,
               std::vector<process_link_t> const &links)
{
  std::vector<process_id_t> sorted = roots;
  std::sort (sorted.begin (), sorted.end ());
  sorted.erase (std::unique (sorted.begin (), sorted.end ()), sorted.end ());

  std::vector<process_tree_t> all;
  for (process_id_t const root : sorted)
    {
      process_tree_t tree;
      tree.root = root;
      tree.descendants = descendants_of (root, links);
      all.push_back (std::move (tree));
    }

  // A tree whose root lies in the descendants of another tree is part of it.
  // When two roots lie in each other's descendants (stale links), the one with
  // the smaller ID stays, so that something is always counted.
  std::vector<process_tree_t> kept;
  for (std::size_t i = 0; i < all.size (); ++i)
    {
      bool inside_other = false;
      for (std::size_t j = 0; j < all.size () && !inside_other; ++j)
        {
          if (i == j)
            continue;
          auto const &below = all[j].descendants;
          bool const in_j
              = std::find (below.begin (), below.end (), all[i].root)
                != below.end ();
          if (!in_j)
            continue;
          auto const &own = all[i].descendants;
          bool const j_in_i = std::find (own.begin (), own.end (), all[j].root)
                              != own.end ();
          inside_other = !j_in_i || j < i;
        }
      if (!inside_other)
        kept.push_back (all[i]);
    }
  return kept;
}

LUMEX_PUBLIC_API bool
windows_name_matches (lumex_string_view requested, lumex_string_view name)
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
