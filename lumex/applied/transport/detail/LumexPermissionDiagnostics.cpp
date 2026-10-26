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

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#if !defined(_WIN32)
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "lumex/applied/transport/detail/LumexPermissionDiagnostics.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"

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
namespace detail
{
namespace
{
// The counters behind node_query_stats.
struct query_stats_holder
{
  std::mutex guard;
  node_query_stats_t stats;
};

query_stats_holder &
query_stats ()
{
  static query_stats_holder holder;
  return holder;
}

// The mode as four octal digits ("0660"); only the permission bits count.
std::string
octal_mode (std::uint32_t mode)
{
  std::string text (4, '0');
  for (std::size_t i = 4; i > 0; --i)
    {
      text[i - 1] = static_cast<char> ('0' + (mode & 7U));
      mode >>= 3U;
    }
  return text;
}

// "root", else "uid 1000".
std::string
principal (char const *numeric_label, std::string const &name,
           std::uint32_t id)
{
  if (!name.empty ())
    return name;
  return std::string (numeric_label) + " " + std::to_string (id);
}

#if !defined(_WIN32)
std::string
user_name (uid_t uid)
{
  std::vector<char> buffer (1024);
  for (;;)
    {
      ::passwd entry;
      ::passwd *found = nullptr;
      int const status
          = ::getpwuid_r (uid, &entry, buffer.data (), buffer.size (), &found);
      if (status == ERANGE && buffer.size () < 65536)
        {
          buffer.resize (buffer.size () * 2);
          continue;
        }
      if (status != 0 || found == nullptr || found->pw_name == nullptr)
        return std::string ();
      return std::string (found->pw_name);
    }
}

std::string
group_name (gid_t gid)
{
  std::vector<char> buffer (1024);
  for (;;)
    {
      ::group entry;
      ::group *found = nullptr;
      int const status
          = ::getgrgid_r (gid, &entry, buffer.data (), buffer.size (), &found);
      if (status == ERANGE && buffer.size () < 65536)
        {
          buffer.resize (buffer.size () * 2);
          continue;
        }
      if (status != 0 || found == nullptr || found->gr_name == nullptr)
        return std::string ();
      return std::string (found->gr_name);
    }
}
#endif
} // namespace

bool
is_permission_error (int os_error) LUMEX_NOEXCEPT
{
#if defined(_WIN32)
  // ERROR_ACCESS_DENIED on a COM port: the port is busy in another process.
  static_cast<void> (os_error);
  return false;
#else
  return os_error == EACCES || os_error == EPERM;
#endif
}

std::error_code
open_failure_code (int os_error) LUMEX_NOEXCEPT
{
  if (is_permission_error (os_error))
    return error::make_error_code (error::transport_errc::permission_denied);
  return std::error_code (os_error, std::system_category ());
}

node_query_stats_t
node_query_stats ()
{
  query_stats_holder &holder = query_stats ();
  std::lock_guard<std::mutex> const lock (holder.guard);
  return holder.stats;
}

node_access_t
query_node_access (std::string const &path)
{
  {
    query_stats_holder &holder = query_stats ();
    std::lock_guard<std::mutex> const lock (holder.guard);
    ++holder.stats.count;
    holder.stats.last_thread = std::this_thread::get_id ();
  }
  node_access_t access;
#if defined(_WIN32)
  static_cast<void> (path);
#else
  struct ::stat info;
  if (::stat (path.c_str (), &info) != 0)
    return access;
  access.found = true;
  access.uid = static_cast<std::uint32_t> (info.st_uid);
  access.gid = static_cast<std::uint32_t> (info.st_gid);
  access.mode = static_cast<std::uint32_t> (info.st_mode) & 07777U;
  access.owner = user_name (info.st_uid);
  access.group = group_name (info.st_gid);
#endif
  return access;
}

std::string
permission_denied_message (std::string const &path,
                           node_access_t const &access)
{
  std::string text = "permission denied on " + path;
  if (access.found)
    text += ": owner " + principal ("uid", access.owner, access.uid)
            + ", group " + principal ("gid", access.group, access.gid)
            + ", mode " + octal_mode (access.mode);
  else
    text += ": owner, group and mode are unknown (the node cannot be "
            "examined)";
#if defined(__linux__)
  // Joining the owning group helps only when the group may read and write.
  if (!access.found || (access.mode & 060U) == 060U)
    text += "; add the user to the group that owns the node (usually dialout "
            "for ttyUSB and ttyACM, plugdev for hidraw) and log in again, or "
            "install the LumexLib udev rule (share/LumexLib/udev)";
  else
    text += "; the group has no read and write access to the node, so "
            "joining it does not help: install the LumexLib udev rule "
            "(share/LumexLib/udev)";
#else
  text += "; grant the user read and write access to the node";
#endif
  return text;
}

std::string
describe_permission_denied (std::string const &path)
{
  return permission_denied_message (path, query_node_access (path));
}
} // namespace detail
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
