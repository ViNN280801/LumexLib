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
 * @file LumexProtocolEntry.hpp
 * @brief The run-time entry of a table row: `command_entry_t`,
 * `options_view_t`, `make_entry<Command> ()` and `command_entries<P> ()`.
 * @details The entries are what the run-time index, the decoded log line
 * and the export read: names, description, key, field names and the
 * options as plain values. The typed values stay on the command type
 * (`get_option<kind::accepted_in> (C::options ())`); the entry keeps the
 * accepted states as 64-bit integers.
 */
#ifndef LUMEX_CORE_PROTOCOL_DSL_ENTRY_HPP
#define LUMEX_CORE_PROTOCOL_DSL_ENTRY_HPP

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "lumex/core/protocol/dsl/LumexProtocolKey.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolOptions.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolRow.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolTypeList.hpp"
#include "lumex/core/span/LumexSpan"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/sequence/LumexIndexSequence.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#endif

namespace lumex
{
namespace core
{
namespace protocol
{
namespace dsl
{
/// @brief The accepted states of a row, converted to 64-bit integers.
using states_t = ::lumex::core::span::view::span<std::int64_t const>;

/**
 * @brief An option of another layer (an option type whose kind is not one
 * of `dsl::kind`) in the run-time entry: its name and one number.
 * @details Such an option type gives `static char const *option_name ()`
 * and `std::int64_t view_value () const` (the dialogue's correlation policy,
 * rate limit or keepalive period); the core keeps its typed value on the
 * command type and this pair in the entry, for the index and the export.
 */
struct extra_option_t
{
  /// The name of the option (`"rate_limit"`).
  char const *name;
  /// Its value as one number.
  std::int64_t value;
};

/// @brief The options of other layers of a row.
using extra_options_t = ::lumex::core::span::view::span<extra_option_t const>;

/// @brief The options of a row as plain values; a `has_*` flag tells
/// whether the row set the option.
struct options_view_t
{
  /// `timeout (ms)` was given.
  bool has_timeout;
  /// `retries (n)` was given.
  bool has_retries;
  /// `stream (interval_ms)` was given.
  bool has_stream;
  /// `log_level (level)` was given.
  bool has_log_level;
  /// `firmware (from, to)` was given.
  bool has_firmware;
  /// The level of `log_level`, `level::debug` without it.
  level log_level;
  /// Milliseconds of `timeout`, 0 without it.
  std::uint32_t timeout_ms;
  /// The count of `retries`, 0 without it.
  std::uint32_t retries;
  /// The interval of `stream`, 0 without it.
  std::uint32_t stream_interval_ms;
  /// The first firmware version of `firmware`, 0 without it.
  std::uint32_t firmware_from;
  /// The last firmware version of `firmware`, 0 without it.
  std::uint32_t firmware_to;
  /// `row_flags` combined.
  unsigned flags;
  /// The names of `model`, empty without it.
  names_t models;
  /// The states of `accepted_in`, empty without it.
  states_t accepted_states;
  /// The options of other layers, in table order, empty without them.
  extra_options_t extra;
};

/// @brief The run-time entry of one row of a protocol.
struct command_entry_t
{
  /// The C++ identifier of the row (`"ERASE_LOG"`).
  char const *identifier;
  /// The name in the specification (`"DELETE"` for an `X_AS` row, else the
  /// identifier).
  char const *specification_name;
  /// The description of the row.
  char const *description;
  /// The key.
  key_value_t key;
  /// The field names of the request, in field order.
  names_t field_names;
  /// The number of request fields (`field_names.size ()`).
  std::size_t field_count;
  /// Whether the row has a reply.
  bool has_reply;
  /// The options.
  options_view_t options;
};

namespace detail
{
template <typename Command, typename K>
struct command_has_option : has_option_type<K, typename Command::options_type>
{
};

template <typename Command, bool Present> struct models_of
{
  static names_t
  get () LUMEX_NOEXCEPT
  {
    return names_t ();
  }
};

template <typename Command, typename Indices> struct models_storage;

template <typename Command, std::size_t... I>
struct models_storage<Command,
                      ::lumex::core::utility::sequence::index_sequence<I...>>
{
  static names_t
  get () LUMEX_NOEXCEPT
  {
    static char const *const values[]
        = { get_option<kind::model> (Command::options ()).names[I]... };
    return names_t (values, sizeof...(I));
  }
};

template <typename Command> struct models_of<Command, true>
{
  using option_type = decltype (get_option<kind::model> (Command::options ()));

  static names_t
  get () LUMEX_NOEXCEPT
  {
    return models_storage<
        Command,
        ::lumex::core::utility::sequence::make_index_sequence<
            sizeof (option_type::names) / sizeof (char const *)>>::get ();
  }
};

template <typename Command, typename Indices> struct states_storage;

template <typename Command, std::size_t... I>
struct states_storage<Command,
                      ::lumex::core::utility::sequence::index_sequence<I...>>
{
  static states_t
  get () LUMEX_NOEXCEPT
  {
    static std::int64_t const values[] = { static_cast<std::int64_t> (
        get_option<kind::accepted_in> (Command::options ()).states[I])... };
    return states_t (values, sizeof...(I));
  }
};

template <typename Command, bool Present> struct states_of
{
  static states_t
  get () LUMEX_NOEXCEPT
  {
    return states_t ();
  }
};

template <typename Command> struct states_of<Command, true>
{
  using option_type
      = decltype (get_option<kind::accepted_in> (Command::options ()));

  static states_t
  get () LUMEX_NOEXCEPT
  {
    return states_storage<
        Command, ::lumex::core::utility::sequence::make_index_sequence<
                     sizeof (option_type::states)
                     / sizeof (typename option_type::state_type)>>::get ();
  }
};

// The value of an option or a fallback, for the options that hold one
// number: Present picks the branch at compile time, so get_option is never
// instantiated for an option the row does not have.
template <typename Command, typename K, bool Present> struct number_of
{
  static LUMEX_CONSTEXPR std::uint32_t
  get () LUMEX_NOEXCEPT
  {
    return 0u;
  }
};

template <typename Command> struct number_of<Command, kind::timeout, true>
{
  static LUMEX_CONSTEXPR std::uint32_t
  get () LUMEX_NOEXCEPT
  {
    return get_option<kind::timeout> (Command::options ()).milliseconds;
  }
};

template <typename Command> struct number_of<Command, kind::retries, true>
{
  static LUMEX_CONSTEXPR std::uint32_t
  get () LUMEX_NOEXCEPT
  {
    return get_option<kind::retries> (Command::options ()).count;
  }
};

template <typename Command> struct number_of<Command, kind::stream, true>
{
  static LUMEX_CONSTEXPR std::uint32_t
  get () LUMEX_NOEXCEPT
  {
    return get_option<kind::stream> (Command::options ()).interval_ms;
  }
};

template <typename Command, bool Present> struct firmware_of
{
  static LUMEX_CONSTEXPR firmware_t
  get () LUMEX_NOEXCEPT
  {
    return firmware_t{ 0u, 0u };
  }
};

template <typename Command> struct firmware_of<Command, true>
{
  static LUMEX_CONSTEXPR firmware_t
  get () LUMEX_NOEXCEPT
  {
    return get_option<kind::firmware> (Command::options ());
  }
};

template <typename Command, bool Present> struct level_of
{
  static LUMEX_CONSTEXPR level
  get () LUMEX_NOEXCEPT
  {
    return level::debug;
  }
};

template <typename Command> struct level_of<Command, true>
{
  static LUMEX_CONSTEXPR level
  get () LUMEX_NOEXCEPT
  {
    return get_option<kind::log_level> (Command::options ()).value;
  }
};

template <typename K> struct is_core_kind : std::false_type
{
};

template <> struct is_core_kind<kind::timeout> : std::true_type
{
};

template <> struct is_core_kind<kind::retries> : std::true_type
{
};

template <> struct is_core_kind<kind::stream> : std::true_type
{
};

template <> struct is_core_kind<kind::log_level> : std::true_type
{
};

template <> struct is_core_kind<kind::firmware> : std::true_type
{
};

template <> struct is_core_kind<kind::model> : std::true_type
{
};

template <> struct is_core_kind<kind::accepted_in> : std::true_type
{
};

template <> struct is_core_kind<kind::flag> : std::true_type
{
};

// The positions of the options of other layers among T...
template <typename Found, std::size_t I, typename... T> struct collect_extras;

template <std::size_t... K, std::size_t I>
struct collect_extras<::lumex::core::utility::sequence::index_sequence<K...>,
                      I>
{
  using type = ::lumex::core::utility::sequence::index_sequence<K...>;
};

template <std::size_t... K, std::size_t I, typename Head, typename... Tail>
struct collect_extras<::lumex::core::utility::sequence::index_sequence<K...>,
                      I, Head, Tail...>
    : std::conditional<
          is_core_kind<typename kind_of<Head>::type>::value,
          collect_extras<
              ::lumex::core::utility::sequence::index_sequence<K...>, I + 1,
              Tail...>,
          collect_extras<
              ::lumex::core::utility::sequence::index_sequence<K..., I>, I + 1,
              Tail...>>::type
{
};

template <typename Set> struct extras_of;

template <typename... T> struct extras_of<option_set<T...>>
{
  using type = typename collect_extras<
      ::lumex::core::utility::sequence::index_sequence<>, 0, T...>::type;
};

template <typename Option>
extra_option_t
make_extra (Option const &option) LUMEX_NOEXCEPT
{
  extra_option_t const extra
      = { Option::option_name (),
          static_cast<std::int64_t> (option.view_value ()) };
  return extra;
}

template <typename Command, typename Indices> struct extras_table;

template <typename Command, std::size_t... I>
struct extras_table<Command,
                    ::lumex::core::utility::sequence::index_sequence<I...>>
{
  static extra_options_t
  get () LUMEX_NOEXCEPT
  {
    // One element more than options: a row without them still has an array.
    static extra_option_t const values[sizeof...(I) + 1]
        = { make_extra (option_value_at<I> (Command::options ()))...,
            extra_option_t () };
    return extra_options_t (values, sizeof...(I));
  }
};

template <typename Command>
options_view_t
make_options_view () LUMEX_NOEXCEPT
{
  using has_timeout = command_has_option<Command, kind::timeout>;
  using has_retries = command_has_option<Command, kind::retries>;
  using has_stream = command_has_option<Command, kind::stream>;
  using has_level = command_has_option<Command, kind::log_level>;
  using has_firmware = command_has_option<Command, kind::firmware>;
  firmware_t const firmware
      = firmware_of<Command, has_firmware::value>::get ();
  options_view_t view = {
    has_timeout::value,
    has_retries::value,
    has_stream::value,
    has_level::value,
    has_firmware::value,
    level_of<Command, has_level::value>::get (),
    number_of<Command, kind::timeout, has_timeout::value>::get (),
    number_of<Command, kind::retries, has_retries::value>::get (),
    number_of<Command, kind::stream, has_stream::value>::get (),
    firmware.from,
    firmware.to,
    flags_of (Command::options ()),
    models_of<Command,
              command_has_option<Command, kind::model>::value>::get (),
    states_of<Command,
              command_has_option<Command, kind::accepted_in>::value>::get (),
    extras_table<Command, typename extras_of<
                              typename Command::options_type>::type>::get ()
  };
  return view;
}

template <typename List> struct entry_table;

template <typename... Commands> struct entry_table<type_list<Commands...>>
{
  static ::lumex::core::span::view::span<command_entry_t const>
  get () LUMEX_NOEXCEPT;
};
} // namespace detail

/// @brief The run-time entry of command type `Command`.
template <typename Command>
command_entry_t
make_entry () LUMEX_NOEXCEPT
{
  static_assert (is_command<Command>::value,
                 "make_entry<C>: C must be a command type of a table row");
  command_entry_t entry
      = { Command::identifier (),
          Command::specification_name (),
          Command::description (),
          key_value (Command::key ()),
          Command::field_names (),
          Command::fields_type::count::value,
          !std::is_same<typename Command::reply_type, no_reply>::value,
          detail::make_options_view<Command> () };
  return entry;
}

namespace detail
{
template <typename... Commands>
::lumex::core::span::view::span<command_entry_t const>
entry_table<type_list<Commands...>>::get () LUMEX_NOEXCEPT
{
  // One entry more than commands: an empty protocol still has an array.
  static command_entry_t const entries[sizeof...(Commands) + 1]
      = { make_entry<Commands> ()..., command_entry_t () };
  return ::lumex::core::span::view::span<command_entry_t const> (
      entries, sizeof...(Commands));
}
} // namespace detail

/**
 * @brief The entries of every command of protocol `P`, in the order of
 * `commands_of<P>`; one array per protocol, built at the first call.
 */
template <typename P>
::lumex::core::span::view::span<command_entry_t const>
command_entries () LUMEX_NOEXCEPT
{
  return detail::entry_table<typename commands_of<P>::type>::get ();
}
} // namespace dsl
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_DSL_ENTRY_HPP
