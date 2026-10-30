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
 * @file LumexProtocolExportJson.hpp
 * @brief `export_json (index)`: the protocol layer as a JSON value; needs
 * `LUMEX_PROTOCOL_WITH_JSON` and nlohmann/json 3.x on the include path.
 * @details The document is the data of the Markdown export in a form
 * programs read:
 * @code
 * {
 *   "protocol": "widget_v2", "base": "widget_v1", "key_width": 1,
 *   "commands": [ {
 *     "identifier": "SET_SPEED", "name": "SET_SPEED",
 *     "description": "Set the speed", "event": false,
 *     "key": { "kind": "integer", "value": 2, "width": 1, "hex": "0x02" },
 *     "request": [ { "name": "speed", "kind": "integer", "size": 2,
 *                    "order": "little", "signed": false,
 *                    "scale": { "num": 1, "den": 10 }, "unit": "rpm",
 *                    "count": 0 } ],
 *     "reply": { "form": "none", "fields": [] },
 *     "options": { "timeout_ms": 500, "flags": [ "idempotent" ] } } ],
 *   "registries": [ { "name": "lamp", "entries": [ ... ] } ],
 *   "units": [ "rpm" ]
 * }
 * @endcode
 * An optional member (`layout`, `registry`, `children`, `role`, an option)
 * is present only when it has a value. The header is only compiled with
 * `LUMEX_PROTOCOL_WITH_JSON`: the CMake option of the module defines it
 * (consumers define it themselves when they do not use the CMake target),
 * and the module never puts an nlohmann include directory on a consumer
 * target, like `applied/json`: the consumer brings nlohmann/json.
 */
#ifndef LUMEX_CORE_PROTOCOL_EXPORT_EXPORT_JSON_HPP
#define LUMEX_CORE_PROTOCOL_EXPORT_EXPORT_JSON_HPP

#ifndef LUMEX_PROTOCOL_WITH_JSON
#error                                                                        \
    "LumexProtocolExportJson.hpp needs LUMEX_PROTOCOL_WITH_JSON (CMake option LUMEX_PROTOCOL_WITH_JSON, ON by default) and nlohmann/json 3.x on the include path"
#endif

#include <cstddef>
#include <cstdint>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

#include "lumex/core/protocol/dsl/LumexProtocolEntry.hpp"
#include "lumex/core/protocol/export/LumexProtocolExportMarkdown.hpp"
#include "lumex/core/protocol/index/LumexProtocolDescriptor.hpp"
#include "lumex/core/protocol/index/LumexProtocolIndex.hpp"
#include "lumex/core/protocol/registry/LumexProtocolRegistry.hpp"
#include "lumex/core/protocol/registry/LumexProtocolRegistryText.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
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
using json_t = ::nlohmann::json;

inline json_t
json_field (field_descriptor_t const &node, std::set<std::string> &units)
{
  json_t out = json_t::object ();
  out["name"] = node.name;
  out["kind"] = field_kind_name (node.info.kind);
  out["size"] = node.info.binary_size;
  out["order"]
      = node.info.order == ::lumex::core::protocol::field::byte_order::big
            ? "big"
            : "little";
  out["signed"] = node.info.is_signed;
  json_t scale = json_t::object ();
  scale["num"] = static_cast<long long> (node.info.scale_num);
  scale["den"] = static_cast<long long> (node.info.scale_den);
  out["scale"] = scale;
  out["unit"] = node.info.unit;
  out["count"] = node.info.count;
  if (node.info.unit[0] != '\0')
    units.insert (node.info.unit);
  if (node.layout_name[0] != '\0')
    out["layout"] = node.layout_name;
  if (node.registry_name[0] != '\0')
    out["registry"] = node.registry_name;
  if (node.role != descriptor_role::none)
    out["role"] = role_name (node.role);
  if (node.role == descriptor_role::union_case)
    out["case"] = static_cast<long long> (node.value);
  if (node.child_count != 0)
    {
      json_t children = json_t::array ();
      for (std::size_t i = 0; i < node.child_count; ++i)
        children.push_back (json_field (node.children[i], units));
      out["children"] = children;
    }
  return out;
}

inline json_t
json_fields (field_descriptor_t const *fields, std::size_t count,
             std::set<std::string> &units)
{
  json_t out = json_t::array ();
  for (std::size_t i = 0; i < count; ++i)
    out.push_back (json_field (fields[i], units));
  return out;
}

inline json_t
json_key (::lumex::core::protocol::dsl::key_value_t const &key)
{
  json_t out = json_t::object ();
  if (key.kind == ::lumex::core::protocol::dsl::key_kind::text)
    {
      out["kind"] = "text";
      out["text"] = std::string (key.text, key.text_size);
    }
  else if (key.kind == ::lumex::core::protocol::dsl::key_kind::other)
    out["kind"] = "other";
  else
    {
      out["kind"] = key.kind == ::lumex::core::protocol::dsl::key_kind::integer
                        ? "integer"
                        : "composite";
      out["value"] = key.integer;
      out["width"] = key.width;
      out["hex"] = key_name (key);
    }
  return out;
}

inline json_t
json_options (::lumex::core::protocol::dsl::options_view_t const &o)
{
  json_t out = json_t::object ();
  if (o.has_timeout)
    out["timeout_ms"] = o.timeout_ms;
  if (o.has_retries)
    out["retries"] = o.retries;
  if (o.has_stream)
    out["stream_interval_ms"] = o.stream_interval_ms;
  if (o.has_log_level)
    out["log_level"] = level_name (o.log_level);
  if (o.has_firmware)
    {
      json_t firmware = json_t::object ();
      firmware["from"] = o.firmware_from;
      firmware["to"] = o.firmware_to;
      out["firmware"] = firmware;
    }
  if (o.models.size () != 0)
    {
      json_t models = json_t::array ();
      for (std::size_t i = 0; i < o.models.size (); ++i)
        models.push_back (o.models.data ()[i]);
      out["models"] = models;
    }
  if (o.accepted_states.size () != 0)
    {
      json_t states = json_t::array ();
      for (std::size_t i = 0; i < o.accepted_states.size (); ++i)
        states.push_back (
            static_cast<long long> (o.accepted_states.data ()[i]));
      out["accepted_in"] = states;
    }
  json_t flags = json_t::array ();
  if ((o.flags & ::lumex::core::protocol::dsl::row_flags::idempotent) != 0)
    flags.push_back ("idempotent");
  if ((o.flags & ::lumex::core::protocol::dsl::row_flags::dangerous) != 0)
    flags.push_back ("dangerous");
  if ((o.flags & ::lumex::core::protocol::dsl::row_flags::deprecated) != 0)
    flags.push_back ("deprecated");
  if (!flags.empty ())
    out["flags"] = flags;
  // An option of another layer by its name: its text, else its number; the
  // values of options with one name (two edges of one field) in an array.
  for (std::size_t i = 0; i < o.extra.size (); ++i)
    {
      ::lumex::core::protocol::dsl::extra_option_t const &extra
          = o.extra.data ()[i];
      json_t const value = extra.text != nullptr
                               ? json_t (std::string (extra.text))
                               : json_t (static_cast<long long> (extra.value));
      if (out.find (extra.name) == out.end ())
        out[extra.name] = value;
      else
        {
          if (!out[extra.name].is_array ())
            {
              json_t const first = out[extra.name];
              out[extra.name] = json_t::array ();
              out[extra.name].push_back (first);
            }
          out[extra.name].push_back (value);
        }
    }
  return out;
}

inline json_t
json_registry_key (
    ::lumex::core::protocol::registry::registry_key_t const &key)
{
  json_t out = json_t::object ();
  out["kind"] = ::lumex::core::protocol::registry::key_kind_name (key.kind);
  out["text"] = ::lumex::core::protocol::registry::key_to_string (key);
  if (key.kind
      == ::lumex::core::protocol::registry::registry_key_kind::mnemonic)
    out["mnemonic"] = std::string (key.mnemonic);
  else
    {
      out["first"] = key.first;
      out["second"] = key.second;
    }
  return out;
}

inline json_t
json_entry (::lumex::core::protocol::registry::registry_entry_t const &entry,
            std::set<std::string> &units)
{
  json_t out = json_t::object ();
  out["identifier"] = entry.identifier;
  out["name"] = entry.specification_name;
  out["description"] = entry.description;
  out["key"] = json_registry_key (entry.key);
  field_descriptor_t type = field_descriptor_t ();
  type.name = "";
  type.info = entry.type;
  type.layout_name = "";
  type.registry_name = "";
  out["type"] = json_field (type, units);
  out["access"]
      = ::lumex::core::protocol::registry::access_to_string (entry.access);
  if (entry.has_limits)
    {
      json_t limits = json_t::object ();
      limits["min"] = entry.lower;
      limits["max"] = entry.upper;
      out["limits"] = limits;
    }
  if (entry.has_firmware)
    {
      json_t firmware = json_t::object ();
      firmware["from"] = entry.firmware_from;
      firmware["to"] = entry.firmware_to;
      out["firmware"] = firmware;
    }
  if (entry.models.size () != 0)
    {
      json_t models = json_t::array ();
      for (std::size_t i = 0; i < entry.models.size (); ++i)
        models.push_back (entry.models.data ()[i]);
      out["models"] = models;
    }
  return out;
}
} // namespace detail

/**
 * @brief The JSON value of an index (see the file description).
 */
inline ::nlohmann::json
export_json (protocol_index_t const &index)
{
  std::set<std::string> units;
  detail::json_t out = detail::json_t::object ();
  out["protocol"] = index.protocol_name ();
  out["base"] = index.base_name ();
  out["key_width"] = index.key_width ();
  detail::json_t commands = detail::json_t::array ();
  for (std::size_t i = 0; i < index.rows ().size (); ++i)
    {
      row_t const &row = index.rows ().data ()[i];
      detail::json_t command = detail::json_t::object ();
      command["identifier"] = row.entry->identifier;
      command["name"] = row.entry->specification_name;
      command["description"] = row.entry->description;
      command["event"] = row.is_event;
      command["key"] = detail::json_key (row.key);
      command["request"]
          = detail::json_fields (row.request, row.request_count, units);
      detail::json_t reply = detail::json_t::object ();
      reply["form"]
          = row.reply == reply_form::none
                ? "none"
                : (row.reply == reply_form::single_field
                       ? "single_field"
                       : (row.reply == reply_form::layout ? "layout"
                                                          : "dialogue"));
      if (row.reply_layout[0] != '\0')
        reply["layout"] = row.reply_layout;
      if (row.reply_kind != nullptr && row.reply_kind[0] != '\0')
        reply["kind"] = row.reply_kind;
      reply["fields"]
          = detail::json_fields (row.reply_fields, row.reply_count, units);
      command["reply"] = reply;
      command["options"] = detail::json_options (row.entry->options);
      commands.push_back (command);
    }
  out["commands"] = commands;
  detail::json_t registries = detail::json_t::array ();
  for (std::size_t r = 0; r < index.registries ().size (); ++r)
    {
      registry_info_t const &registry = index.registries ()[r];
      detail::json_t item = detail::json_t::object ();
      item["name"] = registry.name;
      detail::json_t entries = detail::json_t::array ();
      for (std::size_t e = 0; e < registry.entries.size (); ++e)
        entries.push_back (
            detail::json_entry (registry.entries.data ()[e], units));
      item["entries"] = entries;
      registries.push_back (item);
    }
  out["registries"] = registries;
  detail::json_t unit_list = detail::json_t::array ();
  for (std::set<std::string>::const_iterator unit = units.begin ();
       unit != units.end (); ++unit)
    unit_list.push_back (*unit);
  out["units"] = unit_list;
  return out;
}

/// @brief `export_json (index).dump (indent)`: the document as text. A
/// description that is not UTF-8 is written with U+FFFD in place of the bad
/// bytes (the `replace` error handler) instead of throwing.
inline std::string
export_json_text (protocol_index_t const &index, int indent = 2)
{
  return export_json (index).dump (indent, ' ', false,
                                   ::nlohmann::json::error_handler_t::replace);
}
} // namespace index
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_EXPORT_EXPORT_JSON_HPP
