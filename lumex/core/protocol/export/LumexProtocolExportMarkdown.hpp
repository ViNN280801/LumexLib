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
 * @file LumexProtocolExportMarkdown.hpp
 * @brief `export_markdown (index)`: the protocol layer as a Markdown
 * document.
 * @details One document per index, in the shape of the protocol notes:
 * a title with the layer (and the layer it extends), a section per command
 * or event with its key, description, reply and options and a table of the
 * request fields and of the reply fields (kind, width, scale, unit; the parts
 * of a composite field as further rows), a section per attached registry
 * with a table of its entries, and a list of the units in use. The text is
 * generated from the index alone, so it cannot disagree with the tables the
 * code uses; there is no import: the C++ tables stay the source of truth.
 */
#ifndef LUMEX_CORE_PROTOCOL_EXPORT_EXPORT_MARKDOWN_HPP
#define LUMEX_CORE_PROTOCOL_EXPORT_EXPORT_MARKDOWN_HPP

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <set>
#include <string>

#include "lumex/core/protocol/dsl/LumexProtocolEntry.hpp"
#include "lumex/core/protocol/index/LumexProtocolDescriptor.hpp"
#include "lumex/core/protocol/index/LumexProtocolFormat.hpp"
#include "lumex/core/protocol/index/LumexProtocolIndex.hpp"
#include "lumex/core/protocol/registry/LumexProtocolRegistry.hpp"
#include "lumex/core/protocol/registry/LumexProtocolRegistryText.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#if __has_warning("-Wlifetime-safety-invalidation")
// Clang 23 reports a reference parameter to a container as "invalidated" by
// its own push_back, insert or reserve (a false positive: only references into
// the container are invalidated, not the container).
#pragma clang diagnostic ignored "-Wlifetime-safety-invalidation"
#endif
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
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
namespace index
{
namespace detail
{
// A table cell: the text without the characters that end a cell or a row.
inline std::string
markdown_cell (std::string text)
{
  std::string out;
  for (std::size_t i = 0; i < text.size (); ++i)
    {
      if (text[i] == '|')
        out += "\\|";
      else if (text[i] == '\n' || text[i] == '\r')
        out += ' ';
      else
        out += text[i];
    }
  return out;
}

// A free text as a block quote, one "> " per line: a line that starts with #
// or - cannot become a heading or a list.
inline std::string
markdown_quote (std::string const &text)
{
  std::string out = "> ";
  for (std::size_t i = 0; i < text.size (); ++i)
    {
      char const c = text[i];
      if (c == '\r')
        continue;
      out += c;
      if (c == '\n')
        out += "> ";
    }
  return out;
}

inline std::string
ratio_text (std::intmax_t num, std::intmax_t den)
{
  if (den == 1)
    return std::to_string (static_cast<long long> (num));
  return std::to_string (static_cast<long long> (num)) + "/"
         + std::to_string (static_cast<long long> (den));
}

inline std::string
double_text (double value)
{
  std::string out;
  append_floating (out, value, false);
  return out;
}

inline std::string
size_text (std::size_t size)
{
  return size == 0 ? "var" : std::to_string (size);
}

// The key of a row as the document spells it: 0x1D, "AZ".
inline std::string
key_text_of (::lumex::core::protocol::dsl::key_value_t const &key)
{
  if (key.kind == ::lumex::core::protocol::dsl::key_kind::other)
    return "-";
  return key_name (key);
}

inline char const *
order_text (::lumex::core::protocol::field::field_info_t const &info)
{
  return info.binary_size <= 1 ? "-"
         : info.order == ::lumex::core::protocol::field::byte_order::big
             ? "big"
             : "little";
}

// The parts of a field are rows too, named after their parent.
inline void
append_field_rows (std::string &out, field_descriptor_t const &node,
                   std::string const &path, std::size_t position,
                   std::set<std::string> &units)
{
  std::string name = path;
  if (node.role != descriptor_role::none)
    {
      name += "[";
      name += role_name (node.role);
      if (node.role == descriptor_role::union_case)
        name += " " + std::to_string (static_cast<long long> (node.value));
      // The fields of a tail and the parts of a word without names are told
      // apart by their position.
      else if ((node.role == descriptor_role::tail
                || node.role == descriptor_role::part)
               && node.name[0] == '\0')
        name += " " + std::to_string (position + 1u);
      name += "]";
    }
  if (node.name[0] != '\0')
    {
      if (!name.empty ())
        name += '.';
      name += node.name;
    }
  std::string details;
  if (node.layout_name[0] != '\0')
    details += std::string ("layout ") + node.layout_name;
  if (node.registry_name[0] != '\0')
    details += std::string (details.empty () ? "" : ", ") + "registry "
               + node.registry_name;
  if (node.info.count != 0)
    details += std::string (details.empty () ? "" : ", ") + "count "
               + std::to_string (node.info.count);
  out += "| " + markdown_cell (name) + " | " + field_kind_name (node.info.kind)
         + " | " + size_text (node.info.binary_size) + " | "
         + order_text (node.info) + " | "
         + (node.info.is_signed ? "yes" : "no") + " | "
         + ratio_text (node.info.scale_num, node.info.scale_den) + " | "
         + markdown_cell (node.info.unit) + " | " + markdown_cell (details)
         + " |\n";
  if (node.info.unit[0] != '\0')
    units.insert (node.info.unit);
  for (std::size_t i = 0; i < node.child_count; ++i)
    append_field_rows (out, node.children[i], name, i, units);
}

inline void
append_field_table (std::string &out, field_descriptor_t const *fields,
                    std::size_t count, std::set<std::string> &units)
{
  out += "| Field | Kind | Size | Order | Signed | Scale | Unit | Details |\n";
  out += "| --- | --- | --- | --- | --- | --- | --- | --- |\n";
  for (std::size_t i = 0; i < count; ++i)
    append_field_rows (out, fields[i], std::string (), i, units);
}

inline std::string
flags_text (unsigned flags)
{
  std::string out;
  if ((flags & ::lumex::core::protocol::dsl::row_flags::idempotent) != 0)
    out += "idempotent";
  if ((flags & ::lumex::core::protocol::dsl::row_flags::dangerous) != 0)
    out += std::string (out.empty () ? "" : ", ") + "dangerous";
  if ((flags & ::lumex::core::protocol::dsl::row_flags::deprecated) != 0)
    out += std::string (out.empty () ? "" : ", ") + "deprecated";
  return out;
}

inline char const *
level_name (::lumex::core::protocol::dsl::level value)
{
  static char const *const names[]
      = { "trace", "debug", "info", "warning", "error", "off" };
  unsigned const at = static_cast<unsigned> (value);
  return at < sizeof (names) / sizeof (names[0]) ? names[at] : "unknown";
}

// The options of a row as one line; empty without options.
inline std::string
options_text (::lumex::core::protocol::dsl::options_view_t const &o)
{
  std::string out;
  std::string const separator = "; ";
  if (o.has_timeout)
    out += "timeout " + std::to_string (o.timeout_ms) + " ms";
  if (o.has_retries)
    out += std::string (out.empty () ? "" : separator) + "retries "
           + std::to_string (o.retries);
  if (o.has_stream)
    out += std::string (out.empty () ? "" : separator) + "stream every "
           + std::to_string (o.stream_interval_ms) + " ms";
  if (o.has_log_level)
    out += std::string (out.empty () ? "" : separator) + "log level "
           + level_name (o.log_level);
  if (o.has_firmware)
    out += std::string (out.empty () ? "" : separator) + "firmware "
           + std::to_string (o.firmware_from) + "-"
           + std::to_string (o.firmware_to);
  if (o.models.size () != 0)
    {
      out += std::string (out.empty () ? "" : separator) + "models ";
      for (std::size_t i = 0; i < o.models.size (); ++i)
        out += std::string (i == 0 ? "" : ", ") + o.models.data ()[i];
    }
  if (o.accepted_states.size () != 0)
    {
      out += std::string (out.empty () ? "" : separator) + "accepted in ";
      for (std::size_t i = 0; i < o.accepted_states.size (); ++i)
        out += std::string (i == 0 ? "" : ", ")
               + std::to_string (
                   static_cast<long long> (o.accepted_states.data ()[i]));
    }
  std::string const flags = flags_text (o.flags);
  if (!flags.empty ())
    out += std::string (out.empty () ? "" : separator) + flags;
  for (std::size_t i = 0; i < o.extra.size (); ++i)
    {
      ::lumex::core::protocol::dsl::extra_option_t const &extra
          = o.extra.data ()[i];
      out += std::string (out.empty () ? "" : separator) + extra.name + " "
             + (extra.text != nullptr
                    ? std::string (extra.text)
                    : std::to_string (static_cast<long long> (extra.value)));
    }
  return out;
}

inline std::string
reply_text_of (row_t const &row)
{
  if (row.reply == reply_form::none)
    return "none";
  if (row.reply == reply_form::single_field)
    return "one field";
  if (row.reply == reply_form::layout)
    return std::string ("layout `") + row.reply_layout + "`";
  if (row.reply_kind != nullptr && row.reply_kind[0] != '\0')
    return std::string ("a reply kind of the dialogue, ") + row.reply_kind;
  return "a reply kind of the dialogue";
}

inline std::string
limits_text (::lumex::core::protocol::registry::registry_entry_t const &e)
{
  return e.has_limits ? double_text (e.lower) + " .. " + double_text (e.upper)
                      : std::string ("-");
}

inline std::string
firmware_text (::lumex::core::protocol::registry::registry_entry_t const &e)
{
  return e.has_firmware ? std::to_string (e.firmware_from) + "-"
                              + std::to_string (e.firmware_to)
                        : std::string ("-");
}

inline std::string
models_text (::lumex::core::protocol::registry::registry_entry_t const &e)
{
  std::string out;
  for (std::size_t i = 0; i < e.models.size (); ++i)
    out += std::string (i == 0 ? "" : ", ") + e.models.data ()[i];
  return out.empty () ? std::string ("-") : out;
}
} // namespace detail

/**
 * @brief The Markdown document of an index (see the file description).
 */
inline std::string
export_markdown (protocol_index_t const &index)
{
  std::string out = std::string ("# ") + index.protocol_name () + "\n\n";
  if (index.base_name ()[0] != '\0')
    out += std::string ("Extends `") + index.base_name () + "`.\n\n";
  out += "Key width on the wire: " + std::to_string (index.key_width ())
         + (index.key_width () == 1 ? " byte" : " bytes") + ".\n\n";
  std::set<std::string> units;
  out += "## Commands\n\n";
  for (std::size_t i = 0; i < index.rows ().size (); ++i)
    {
      row_t const &row = index.rows ().data ()[i];
      std::string const name = row.entry->specification_name;
      out += "### " + detail::markdown_cell (name) + "\n\n";
      out += std::string (row.is_event ? "Event" : "Command") + " `"
             + row.entry->identifier + "`, key `"
             + detail::key_text_of (row.key) + "`.\n\n";
      out += detail::markdown_quote (row.entry->description) + "\n\n";
      out += "Reply: " + detail::reply_text_of (row) + ".\n\n";
      std::string const options = detail::options_text (row.entry->options);
      if (!options.empty ())
        out += "Options: " + options + ".\n\n";
      out += "Request fields:\n\n";
      if (row.request_count == 0)
        out += "none\n\n";
      else
        {
          detail::append_field_table (out, row.request, row.request_count,
                                      units);
          out += "\n";
        }
      if (row.reply_count != 0)
        {
          out += "Reply fields:\n\n";
          detail::append_field_table (out, row.reply_fields, row.reply_count,
                                      units);
          out += "\n";
        }
    }
  for (std::size_t r = 0; r < index.registries ().size (); ++r)
    {
      registry_info_t const &registry = index.registries ()[r];
      out += std::string ("## Registry ") + registry.name + "\n\n";
      out += "| Entry | Key | Kind | Size | Signed | Scale | Unit | Access | "
             "Limits | Firmware | Models | Description |\n";
      out += "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | "
             "--- | --- |\n";
      for (std::size_t e = 0; e < registry.entries.size (); ++e)
        {
          ::lumex::core::protocol::registry::registry_entry_t const &entry
              = registry.entries.data ()[e];
          std::string label = entry.identifier;
          if (std::string (entry.specification_name) != label)
            label += std::string (" (") + entry.specification_name + ")";
          out += "| " + detail::markdown_cell (label) + " | "
                 + detail::markdown_cell (
                     ::lumex::core::protocol::registry::key_to_string (
                         entry.key))
                 + " | " + field_kind_name (entry.type.kind) + " | "
                 + detail::size_text (entry.type.binary_size) + " | "
                 + (entry.type.is_signed ? "yes" : "no") + " | "
                 + detail::ratio_text (entry.type.scale_num,
                                       entry.type.scale_den)
                 + " | " + detail::markdown_cell (entry.type.unit) + " | "
                 + ::lumex::core::protocol::registry::access_to_string (
                     entry.access)
                 + " | " + detail::limits_text (entry) + " | "
                 + detail::firmware_text (entry) + " | "
                 + detail::markdown_cell (detail::models_text (entry)) + " | "
                 + detail::markdown_cell (entry.description) + " |\n";
          if (entry.type.unit[0] != '\0')
            units.insert (entry.type.unit);
        }
      out += "\n";
    }
  out += "## Units\n\n";
  if (units.empty ())
    out += "none\n";
  for (std::set<std::string>::const_iterator unit = units.begin ();
       unit != units.end (); ++unit)
    out += "- " + *unit + "\n";
  return out;
}
} // namespace index
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(pop)
#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_EXPORT_EXPORT_MARKDOWN_HPP
