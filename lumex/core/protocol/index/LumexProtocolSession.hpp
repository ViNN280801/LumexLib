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
 * @file LumexProtocolSession.hpp
 * @brief `session_context_t`: what identification learned about the
 * device (model, firmware, protocol layer, named values) for the decoders
 * and the filters that depend on it.
 * @details A device is identified by a first exchange (its self-description
 * row): the model name, the firmware version, sometimes a type code that
 * changes the layout of a later reply. The session context keeps those
 * values. A decoder that depends on them (`union_by_context_`) reads the
 * named values through `decode_context ()`; `available ()` tells whether a
 * row or a registry entry exists in the firmware and model, and
 * `verify_registry` uses it to skip the entries the device does not have. The
 * index and the exports list every row and entry: they do not filter, the
 * caller that needs a filtered view asks `available ()`. A value that was
 * never set is "not identified yet" and keeps everything available.
 *
 * The context is a plain value without locking: the owner (the client of
 * the dialogue) sets it from the thread that identifies the device and shares
 * it for reading afterwards. `decode_context ()` points at the object, so
 * the object must not move while a decode uses the view.
 */
#ifndef LUMEX_CORE_PROTOCOL_INDEX_SESSION_HPP
#define LUMEX_CORE_PROTOCOL_INDEX_SESSION_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "lumex/core/protocol/dsl/LumexProtocolEntry.hpp"
#include "lumex/core/protocol/field/LumexProtocolContext.hpp"
#include "lumex/core/protocol/registry/LumexProtocolRegistry.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#endif

namespace lumex
{
namespace core
{
namespace protocol
{
namespace index
{
/// @brief The values a device identified itself with.
class session_context_t
{
public:
  /// @brief An empty context: nothing identified yet.
  session_context_t () : m_firmware (0) {}

  /// @brief Sets the model name (`"A200"`); empty means unknown.
  void
  set_model (std::string const &model)
  {
    m_model = model;
  }

  /// @brief The model name, empty when unknown.
  std::string const &
  model () const LUMEX_NOEXCEPT
  {
    return m_model;
  }

  /// @brief Sets the firmware version (the table chooses how a version is
  /// written as a number, see `firmware (from, to)`); 0 means unknown.
  void
  set_firmware (std::uint32_t firmware) LUMEX_NOEXCEPT
  {
    m_firmware = firmware;
  }

  /// @brief The firmware version, 0 when unknown.
  std::uint32_t
  firmware () const LUMEX_NOEXCEPT
  {
    return m_firmware;
  }

  /// @brief Sets the name of the protocol layer in use (`"widget_v2"`).
  void
  set_layer (std::string const &layer)
  {
    m_layer = layer;
  }

  /// @brief The protocol layer, empty when not selected.
  std::string const &
  layer () const LUMEX_NOEXCEPT
  {
    return m_layer;
  }

  /// @brief Sets the named value `name` (the type of a thermostat), or
  /// replaces it.
  void
  set (std::string const &name, std::int64_t value)
  {
    for (std::size_t i = 0; i < m_values.size (); ++i)
      if (m_values[i].first == name)
        {
          m_values[i].second = value;
          return;
        }
    m_values.push_back (std::make_pair (name, value));
  }

  /// @brief Forgets the named value `name`; false when there was none.
  bool
  erase (std::string const &name)
  {
    for (std::size_t i = 0; i < m_values.size (); ++i)
      if (m_values[i].first == name)
        {
          m_values.erase (m_values.begin () + static_cast<std::ptrdiff_t> (i));
          return true;
        }
    return false;
  }

  /// @brief Forgets everything (a new connection).
  void
  clear ()
  {
    m_model.clear ();
    m_layer.clear ();
    m_firmware = 0;
    m_values.clear ();
  }

  /// @brief The value of `name`: a named value, or `"firmware"` when the
  /// firmware is known; false when the context does not know it.
  bool
  find (char const *name, std::int64_t &value) const
  {
    for (std::size_t i = 0; i < m_values.size (); ++i)
      if (m_values[i].first == name)
        {
          value = m_values[i].second;
          return true;
        }
    if (m_firmware != 0 && std::strcmp (name, "firmware") == 0)
      {
        value = static_cast<std::int64_t> (m_firmware);
        return true;
      }
    return false;
  }

  /// @brief The view the decoders read (`layout::decode_request (...,
  /// &view)`); valid while this object lives and stays where it is.
  ::lumex::core::protocol::field::decode_context_t
  decode_context () const LUMEX_NOEXCEPT
  {
    ::lumex::core::protocol::field::decode_context_t const view
        = { this, &session_context_t::lookup };
    return view;
  }

  /// @brief Whether a row exists in the firmware and the model of the
  /// context (its `firmware` and `model` options).
  bool
  available (::lumex::core::protocol::dsl::command_entry_t const &entry) const
      LUMEX_NOEXCEPT
  {
    ::lumex::core::protocol::dsl::options_view_t const &options
        = entry.options;
    return ::lumex::core::protocol::registry::option_available (
        options.has_firmware, options.firmware_from, options.firmware_to,
        options.models, m_firmware, m_model.c_str ());
  }

  /// @brief Whether a registry entry exists in the firmware and the model
  /// of the context.
  bool
  available (::lumex::core::protocol::registry::registry_entry_t const &entry)
      const LUMEX_NOEXCEPT
  {
    return ::lumex::core::protocol::registry::entry_available (
        entry, m_firmware, m_model.c_str ());
  }

private:
  static bool
  lookup (void const *state, char const *key, std::int64_t &value)
  {
    return static_cast<session_context_t const *> (state)->find (key, value);
  }

  std::string m_model;
  std::string m_layer;
  std::uint32_t m_firmware;
  std::vector<std::pair<std::string, std::int64_t>> m_values;
};
} // namespace index
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_INDEX_SESSION_HPP
