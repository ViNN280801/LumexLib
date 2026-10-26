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

/**
 * @file LumexPermissionDiagnostics.hpp
 * @brief Why a device node could not be opened: the `permission_denied`
 * decision for a failed serial open and the message that names the node, its
 * owner, group and mode, and the usual fix.
 * @details Internal to the transport library.
 *
 * On POSIX an `EACCES` or `EPERM` from opening `/dev/ttyUSBN`,
 * `/dev/ttyACMN` or `/dev/hidrawN` is a permission problem and becomes
 * `transport_errc::permission_denied`. On Windows nothing is mapped:
 * `ERROR_ACCESS_DENIED` from opening a COM port means that the port is open
 * in another process (or the driver refused a second open), not that the
 * user lacks a right, so it stays the system error it is.
 *
 * `query_node_access` and `permission_denied_message` are split so that the
 * text can be tested with fixed inputs on every platform; `stat`,
 * `getpwuid_r` and `getgrgid_r` are called only on POSIX. The lookups may
 * block (the user database can be a network service): they belong on the
 * thread that opens the port, never on the io thread, and only when the
 * error log is on. `node_query_stats` counts them for the tests.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_DETAIL_PERMISSION_DIAGNOSTICS_HPP
#define LUMEX_APPLIED_TRANSPORT_DETAIL_PERMISSION_DIAGNOSTICS_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <system_error>
#include <thread>

#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif

namespace lumex
{
namespace applied
{
namespace transport
{
namespace detail
{
/// @brief Who owns a device node and what its mode allows.
struct node_access_t
{
  /// `stat` succeeded; when false the other fields mean nothing.
  bool found = false;
  /// Numeric owner.
  std::uint32_t uid = 0;
  /// Numeric group.
  std::uint32_t gid = 0;
  /// Permission bits (`st_mode & 07777`), for example `0660`.
  std::uint32_t mode = 0;
  /// Owner name; empty when the user database has no entry for `uid`.
  std::string owner;
  /// Group name; empty when the group database has no entry for `gid`.
  std::string group;
};

/// @brief How often `query_node_access` ran and on which thread it ran last.
struct node_query_stats_t
{
  /// Calls of `query_node_access` since the process started.
  std::size_t count = 0;
  /// The thread of the latest call; a default-constructed id before the
  /// first one.
  std::thread::id last_thread;
};

/**
 * @brief True when `os_error` (`errno` of a failed open) is a permission
 * problem: `EACCES` or `EPERM` on POSIX; always false on Windows, where
 * `ERROR_ACCESS_DENIED` means "port busy" and stays a system error.
 */
bool is_permission_error (int os_error) LUMEX_NOEXCEPT;

/**
 * @brief The code a caller sees for a failed open of a device node.
 * @return `transport_errc::permission_denied` when `is_permission_error`,
 * else `std::error_code (os_error, std::system_category ())`.
 */
std::error_code open_failure_code (int os_error) LUMEX_NOEXCEPT;

/**
 * @brief Reads owner, group and mode of `path` (`stat`, `getpwuid_r`,
 * `getgrgid_r`). `found` is false when the node cannot be examined and on
 * Windows, where the function does nothing.
 */
node_access_t query_node_access (std::string const &path);

/// @brief The counters of `query_node_access` (a test seam: the lookups must
/// not run on the io thread nor with the error log off).
node_query_stats_t node_query_stats ();

/**
 * @brief The log text for a permission failure on `path`: the node, owner and
 * group (by name, else `uid N` / `gid N`), the mode in octal, and the fix.
 * @details On Linux, example: `permission denied on /dev/ttyUSB0: owner root,
 * group dialout, mode 0660; add the user to the group that owns the node
 * (usually dialout for ttyUSB and ttyACM, plugdev for hidraw) and log in
 * again, or install the LumexLib udev rule (share/LumexLib/udev)`. When the
 * group has no read and write bits (`(mode & 060) != 060`) joining it does
 * not help and only the udev rule is offered. On the other systems the fix
 * is `grant the user read and write access to the node`. With `access.found`
 * false the text says that the node could not be examined and keeps the fix.
 */
std::string permission_denied_message (std::string const &path,
                                       node_access_t const &access);

/// @brief `permission_denied_message (path, query_node_access (path))`.
std::string describe_permission_denied (std::string const &path);
} // namespace detail
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_DETAIL_PERMISSION_DIAGNOSTICS_HPP
