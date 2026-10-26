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
 * @file LumexProtocolRegistryVerify.hpp
 * @brief `verify_registry`: compares a registry with the self-description of
 * a device.
 * @details Some devices tell their own table: `GET_PARAMETER` by index gives
 * the key, the width, the limits and the access of entry number `n`, until
 * the device runs out of entries. `verify_registry<R> (callback)` calls the
 * callback with 0, 1, 2 ... until it returns false, matches the entries of
 * the device with the table by key and reports every difference: an entry
 * only in the table or only in the device, and for a matched entry the name,
 * the width, the signedness, the scale, the limits and the access (only what
 * the device reported: size 0 and the `has_*` flags tell what it did not).
 * With a session context (`index::session_context_t`) the entries the
 * firmware or model of the device does not have are left out of the
 * comparison, also when the device reports them, and are listed in
 * `verify_report_t::skipped`. The check needs no I/O: the callback may read a
 * recorded session.
 */
#ifndef LUMEX_CORE_PROTOCOL_REGISTRY_REGISTRY_VERIFY_HPP
#define LUMEX_CORE_PROTOCOL_REGISTRY_REGISTRY_VERIFY_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "lumex/core/protocol/index/LumexProtocolSession.hpp"
#include "lumex/core/protocol/registry/LumexProtocolRegistry.hpp"
#include "lumex/core/protocol/registry/LumexProtocolRegistryText.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(push)
// snprintf is the standard function; the secure variants are not portable.
#pragma warning(disable : 4996)
#endif

namespace lumex
{
namespace core
{
namespace protocol
{
namespace registry
{
/// @brief What a device tells about one entry of its table.
struct device_entry_t
{
  /// The key of the entry.
  registry_key_t key;
  /// The name the device gives, or `nullptr` when it gives none.
  char const *name;
  /// Bytes of the value on the wire, 0 when the device does not tell.
  std::uint32_t size;
  /// Whether `is_signed` is reported.
  bool has_signedness;
  /// Whether the value is signed.
  bool is_signed;
  /// Whether `scale_num` and `scale_den` are reported.
  bool has_scale;
  /// The engineering value is the raw value times `scale_num / scale_den`.
  std::intmax_t scale_num;
  /// See `scale_num`.
  std::intmax_t scale_den;
  /// Whether `min` and `max` are reported.
  bool has_limits;
  /// The smallest engineering value.
  double lower;
  /// The largest engineering value.
  double upper;
  /// Whether `access` is reported.
  bool has_access;
  /// The access.
  access_t access;
};

/// @brief What differs between the table and the device.
enum class mismatch_kind : std::uint8_t
{
  /// The table has the entry, the device does not report it.
  missing_in_device,
  /// The device reports an entry the table does not have.
  missing_in_table,
  /// The names differ.
  name,
  /// The widths differ.
  size,
  /// The signedness differs.
  signedness,
  /// The scales differ.
  scale,
  /// The limits differ.
  limits,
  /// The access differs.
  access
};

/// @brief One difference.
struct registry_mismatch_t
{
  /// What differs.
  mismatch_kind kind;
  /// The entry: its identifier, or the key as text for an entry the table
  /// does not have.
  std::string entry;
  /// The two values, as text (`"table 2 bytes, device 4 bytes"`).
  std::string detail;
};

/// @brief The result of `verify_registry`.
struct verify_report_t
{
  /// The differences, entries of the device in the order it reported them,
  /// then the entries only the table has.
  std::vector<registry_mismatch_t> mismatches;
  /// The number of entries the device reported.
  std::size_t device_entries;
  /// The number of entries found in both.
  std::size_t compared;
  /// The identifiers of the entries left out because the session says the
  /// firmware or model of the device does not have them.
  std::vector<std::string> skipped;

  /// @brief Whether the table and the device agree.
  bool
  ok () const LUMEX_NOEXCEPT
  {
    return mismatches.empty ();
  }
};

/// @brief The name of a kind of difference.
inline char const *
mismatch_kind_name (mismatch_kind kind) LUMEX_NOEXCEPT
{
  static char const *const names[] = { "missing_in_device",
                                       "missing_in_table",
                                       "name",
                                       "size",
                                       "signedness",
                                       "scale",
                                       "limits",
                                       "access" };
  unsigned const at = static_cast<unsigned> (kind);
  return at < sizeof (names) / sizeof (names[0]) ? names[at] : "unknown";
}

/// @brief A difference as one line: `size of FREQUENCY: table 2 bytes,
/// device 4 bytes`.
inline std::string
to_string (registry_mismatch_t const &mismatch)
{
  return std::string (mismatch_kind_name (mismatch.kind)) + " of "
         + mismatch.entry + ": " + mismatch.detail;
}

namespace detail
{
inline bool
close_enough (double left, double right) LUMEX_NOEXCEPT
{
  double const scale = (std::max)(std::fabs (left), std::fabs (right));
  return std::fabs (left - right) <= 1e-9 * (scale > 1.0 ? scale : 1.0);
}

inline std::string
number_text (double value)
{
  char buffer[40];
  std::snprintf (buffer, sizeof (buffer), "%g", value);
  return buffer;
}

inline void
add_mismatch (std::vector<registry_mismatch_t> &out, mismatch_kind kind,
              std::string const &entry, std::string const &text)
{
  registry_mismatch_t mismatch;
  mismatch.kind = kind;
  mismatch.entry = entry;
  mismatch.detail = text;
  out.push_back (mismatch);
}

inline void
compare_entry (registry_entry_t const &table, device_entry_t const &device,
               std::vector<registry_mismatch_t> &out)
{
  std::string const entry = table.identifier;
  if (device.name != nullptr
      && std::strcmp (device.name, table.identifier) != 0
      && std::strcmp (device.name, table.specification_name) != 0)
    add_mismatch (out, mismatch_kind::name, entry,
                  std::string ("table ") + table.specification_name
                      + ", device " + device.name);
  if (device.size != 0 && table.type.binary_size != 0
      && device.size != table.type.binary_size)
    add_mismatch (out, mismatch_kind::size, entry,
                  "table " + std::to_string (table.type.binary_size)
                      + " bytes, device " + std::to_string (device.size)
                      + " bytes");
  if (device.has_signedness && device.is_signed != table.type.is_signed)
    add_mismatch (
        out, mismatch_kind::signedness, entry,
        std::string ("table ") + (table.type.is_signed ? "signed" : "unsigned")
            + ", device " + (device.is_signed ? "signed" : "unsigned"));
  if (device.has_scale && device.scale_den != 0 && table.type.scale_den != 0
      && device.scale_num * table.type.scale_den
             != table.type.scale_num * device.scale_den)
    add_mismatch (out, mismatch_kind::scale, entry,
                  "table " + std::to_string (table.type.scale_num) + "/"
                      + std::to_string (table.type.scale_den) + ", device "
                      + std::to_string (device.scale_num) + "/"
                      + std::to_string (device.scale_den));
  if (device.has_limits && table.has_limits
      && (!close_enough (device.lower, table.lower)
          || !close_enough (device.upper, table.upper)))
    add_mismatch (out, mismatch_kind::limits, entry,
                  "table " + number_text (table.lower) + ".."
                      + number_text (table.upper) + ", device "
                      + number_text (device.lower) + ".."
                      + number_text (device.upper));
  if (device.has_access && device.access != table.access)
    add_mismatch (out, mismatch_kind::access, entry,
                  std::string ("table ") + access_to_string (table.access)
                      + ", device " + access_to_string (device.access));
}
} // namespace detail

/**
 * @brief Compares the table `entries` with a device.
 * @param entries The table (`R::entries ()`).
 * @param describe `bool describe (std::size_t index, device_entry_t &entry)`:
 * fills `entry` with the device entry number `index` and returns true, or
 * returns false when the device has no entry with that number.
 * @param max_entries The most entries asked of the device (a device that
 * never ends does not hang the check).
 * @param session What identification learned, or `nullptr`: the entries it
 * says the device does not have are skipped (see `verify_report_t::skipped`).
 */
template <typename Describe>
verify_report_t
verify_entries (
    entries_t const &entries, Describe &&describe,
    std::size_t max_entries = 65536,
    ::lumex::core::protocol::index::session_context_t const *session = nullptr)
{
  verify_report_t report;
  report.device_entries = 0;
  report.compared = 0;
  std::vector<bool> seen (entries.size (), false);
  std::vector<bool> left_out (entries.size (), false);
  for (std::size_t i = 0; i < entries.size (); ++i)
    if (session != nullptr && !session->available (entries.data ()[i]))
      {
        left_out[i] = true;
        seen[i] = true;
        report.skipped.push_back (entries.data ()[i].identifier);
      }
  for (std::size_t position = 0; position < max_entries; ++position)
    {
      device_entry_t device = device_entry_t ();
      if (!describe (position, device))
        break;
      ++report.device_entries;
      registry_entry_t const *const table = find_entry (entries, device.key);
      if (table == nullptr)
        {
          detail::add_mismatch (
              report.mismatches, mismatch_kind::missing_in_table,
              key_to_string (device.key),
              std::string ("the device reports ")
                  + (device.name != nullptr ? device.name : "an entry"));
          continue;
        }
      std::size_t const at
          = static_cast<std::size_t> (table - entries.data ());
      if (left_out[at])
        continue;
      seen[at] = true;
      ++report.compared;
      detail::compare_entry (*table, device, report.mismatches);
    }
  for (std::size_t i = 0; i < entries.size (); ++i)
    if (!seen[i])
      detail::add_mismatch (
          report.mismatches, mismatch_kind::missing_in_device,
          entries.data ()[i].identifier,
          "the table has " + key_to_string (entries.data ()[i].key));
  return report;
}

/// @brief `verify_entries (R::entries (), describe, max_entries, session)`.
template <typename R, typename Describe>
verify_report_t
verify_registry (
    Describe &&describe, std::size_t max_entries = 65536,
    ::lumex::core::protocol::index::session_context_t const *session = nullptr)
{
  static_assert (is_registry<R>::value,
                 "verify_registry<R>: R is a LUMEX_PROTOCOL_REGISTRY type");
  return verify_entries (R::entries (), std::forward<Describe> (describe),
                         max_entries, session);
}
} // namespace registry
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(pop)
#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_REGISTRY_REGISTRY_VERIFY_HPP
