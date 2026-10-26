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
 * @file LumexProtocolRegistry.hpp
 * @brief Parameter and register tables: `LUMEX_PROTOCOL_REGISTRY`, the keys
 * (`registry_key_t`), the run-time entries (`registry_entry_t`), the
 * checks of a write (`check_access`, `check_limits`, `to_raw`) and the
 * availability by firmware and model (`entry_available`).
 * @details Many devices keep their settings in a table that a few generic
 * commands read and write: a parameter by index, a Modbus register by
 * address, a two-letter mnemonic. The commands are rows of the protocol
 * (`SET_PARAMETER (key, value)`); the table is a registry:
 * @code
 * #define LAMP_PARAMS(P, P_AS) \
 *   P (FREQUENCY, (0x10), "Flash frequency", \
 *      (u16_be_<scale<std::ratio<1, 10> >, unit::hertz>), RW, \
 *      (limits (1, 100), firmware (2, 7))) \
 *   P (LEVEL, (0x11), "Lamp level", (u8_<unit::percent>), R, ())
 * LUMEX_PROTOCOL_REGISTRY (lamp, LAMP_PARAMS);
 * @endcode
 * `P (id, (key), "description", (type), access, (options...))`; `P_AS` takes
 * the specification name after `id` (a name that collides with a platform
 * macro). The key group is one of `5` (an index), `typed (type, index)`,
 * `unit_param (unit, param)`, `mnemonic ("AZ")`, `modbus (0x0100)`; the type
 * is one field of the field table (its width, signedness, scale and unit
 * describe the value); the access is `R`, `W` or `RW`; the options are
 * `limits (min, max)` (engineering values), `firmware (from, to)` and
 * `model ("A", ...)`. The registry is a struct: `lamp::FREQUENCY` is an
 * enumerator of `lamp::id` (the typed argument of a generic command, see
 * `registry_key_` and `registry_value_` in `LumexProtocolRegistryFields.hpp`)
 * and `lamp::entries ()` the run-time table in table order.
 */
#ifndef LUMEX_CORE_PROTOCOL_REGISTRY_REGISTRY_HPP
#define LUMEX_CORE_PROTOCOL_REGISTRY_REGISTRY_HPP

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <system_error>
#include <type_traits>

#include "lumex/core/protocol/dsl/LumexProtocolEntry.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolOptions.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolPlatformMacros.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolRow.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolTypeList.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolVocabulary.hpp"
#include "lumex/core/protocol/error/LumexProtocolErrc.hpp"
#include "lumex/core/protocol/field/LumexProtocolFieldTraits.hpp"
#include "lumex/core/protocol/field/LumexProtocolNumeric.hpp"
#include "lumex/core/span/LumexSpan"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/sequence/LumexIndexSequence.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

namespace lumex
{
namespace core
{
namespace protocol
{
namespace registry
{
/// @brief The tag a registry names as `lumex_protocol_registry`.
struct registry_tag
{
};

/// @brief What the key of a registry entry is made of.
enum class registry_key_kind : std::uint8_t
{
  /// One number: the index of the parameter (`5`, `index (5)`).
  index,
  /// A type and an index (`typed (type, index)`).
  typed,
  /// A unit and a parameter of the unit (`unit_param (unit, param)`).
  unit_param,
  /// Up to three characters (`mnemonic ("AZ")`).
  mnemonic,
  /// A Modbus register address (`modbus (0x0100)`).
  modbus
};

/**
 * @brief The key of a registry entry: a kind and its parts.
 * @details `first` is the index, the type, the unit or the address;
 * `second` the index of a `typed` key or the parameter of a `unit_param`
 * key; `mnemonic` the characters of a mnemonic (NUL padded).
 */
struct registry_key_t
{
  /// What the key is made of.
  registry_key_kind kind;
  /// The first number.
  std::uint32_t first;
  /// The second number (`typed`, `unit_param`), otherwise 0.
  std::uint32_t second;
  /// The characters of a `mnemonic`, NUL padded, otherwise empty.
  char mnemonic[4];
};

/// @brief Whether two keys are the same on the wire.
inline bool
same_key (registry_key_t const &left,
          registry_key_t const &right) LUMEX_NOEXCEPT
{
  return left.kind == right.kind && left.first == right.first
         && left.second == right.second
         && left.mnemonic[0] == right.mnemonic[0]
         && left.mnemonic[1] == right.mnemonic[1]
         && left.mnemonic[2] == right.mnemonic[2];
}

/// @brief The key `index (n)`.
LUMEX_CONSTEXPR inline registry_key_t
make_index_key (std::uint32_t number) LUMEX_NOEXCEPT
{
  return registry_key_t{ registry_key_kind::index, number, 0, { 0, 0, 0, 0 } };
}

/// @brief The key `typed (type, index)`.
LUMEX_CONSTEXPR inline registry_key_t
make_typed_key (std::uint32_t type, std::uint32_t number) LUMEX_NOEXCEPT
{
  return registry_key_t{
    registry_key_kind::typed, type, number, { 0, 0, 0, 0 }
  };
}

/// @brief The key `unit_param (unit, param)`.
LUMEX_CONSTEXPR inline registry_key_t
make_unit_param_key (std::uint32_t unit, std::uint32_t param) LUMEX_NOEXCEPT
{
  return registry_key_t{
    registry_key_kind::unit_param, unit, param, { 0, 0, 0, 0 }
  };
}

/// @brief The key `modbus (address)`.
LUMEX_CONSTEXPR inline registry_key_t
make_modbus_key (std::uint32_t address) LUMEX_NOEXCEPT
{
  return registry_key_t{
    registry_key_kind::modbus, address, 0, { 0, 0, 0, 0 }
  };
}

/// @brief The key `mnemonic ("AZ")`: one to three characters.
template <std::size_t N>
LUMEX_CONSTEXPR registry_key_t
make_mnemonic_key (char const (&text)[N]) LUMEX_NOEXCEPT
{
  static_assert (N >= 2 && N <= 4,
                 "LUMEX_PROTOCOL_REGISTRY: a mnemonic has one to three "
                 "characters");
  return registry_key_t{ registry_key_kind::mnemonic,
                         0,
                         0,
                         { text[0], N > 2 ? text[1] : '\0',
                           N > 3 ? text[2] : '\0', '\0' } };
}

/// @brief What the access to an entry may be (a combination of bits).
enum class access_t : std::uint8_t
{
  /// The entry may be read.
  read = 1,
  /// The entry may be written.
  write = 2,
  /// Both.
  read_write = 3
};

/// @brief The access words of a registry row: `R`, `W`, `RW`.
struct access_words
{
  /// The words.
  enum word : unsigned
  {
    /// Read only.
    R = 1u,
    /// Write only.
    W = 2u,
    /// Read and write.
    RW = 3u
  };
};

/// @brief The access a registry row names.
LUMEX_CONSTEXPR inline access_t
access_of (access_words::word word) LUMEX_NOEXCEPT
{
  return static_cast<access_t> (word);
}

/// @brief The kind of the `limits` option.
struct limits_kind
{
};

/// @brief `limits (min, max)`: the engineering values an entry accepts.
struct limits_t
{
  /// The kind of the option.
  typedef limits_kind option_kind;
  /// A word of the table vocabulary (`limits (min, max)`).
  typedef ::lumex::core::protocol::dsl::vocabulary_word_tag
      lumex_vocabulary_word;
  /// The smallest value.
  double lower;
  /// The largest value.
  double upper;

  /// @brief `limits (min, max)`.
  LUMEX_CONSTEXPR_CTOR
  limits_t (double lowest, double highest) LUMEX_NOEXCEPT : lower (lowest),
                                                            upper (highest)
  {
  }
};

namespace detail
{
template <typename T> struct is_text_literal : std::false_type
{
};

template <std::size_t N> struct is_text_literal<char[N]> : std::true_type
{
};

template <typename T, typename Enable = void> struct key_maker
{
  static_assert (std::is_integral<T>::value,
                 "LUMEX_PROTOCOL_REGISTRY: a key is a number (an index), "
                 "typed (type, index), unit_param (unit, param), mnemonic "
                 "(\"AZ\") or modbus (address)");
  static LUMEX_CONSTEXPR registry_key_t
  make (T const &value) LUMEX_NOEXCEPT
  {
    return make_index_key (static_cast<std::uint32_t> (value));
  }
};

template <> struct key_maker<registry_key_t, void>
{
  static LUMEX_CONSTEXPR registry_key_t
  make (registry_key_t const &value) LUMEX_NOEXCEPT
  {
    return value;
  }
};

template <std::size_t N> struct key_maker<char[N], void>
{
  static LUMEX_CONSTEXPR registry_key_t
  make (char const (&value)[N]) LUMEX_NOEXCEPT
  {
    return make_mnemonic_key (value);
  }
};
} // namespace detail

/// @brief The key of a row: a number is an index, a string literal a
/// mnemonic, a `registry_key_t` itself.
template <typename T>
LUMEX_CONSTEXPR registry_key_t
make_registry_key (T const &value) LUMEX_NOEXCEPT
{
  return detail::key_maker<T>::make (value);
}

/**
 * @brief The names of a registry table: everything of `dsl::vocabulary`
 * (the field types, `unit::...`, `firmware (from, to)`, `model (...)`) and
 * the key makers, the access words and `limits (min, max)`.
 */
struct vocabulary : ::lumex::core::protocol::dsl::vocabulary, access_words
{
  /// @brief Option: the engineering values `limits (min, max)`.
  typedef limits_t limits;

  /// @brief Key: an index.
  static LUMEX_CONSTEXPR registry_key_t
  index (std::uint32_t number) LUMEX_NOEXCEPT
  {
    return make_index_key (number);
  }

  /// @brief Key: a type and an index.
  static LUMEX_CONSTEXPR registry_key_t
  typed (std::uint32_t type, std::uint32_t number) LUMEX_NOEXCEPT
  {
    return make_typed_key (type, number);
  }

  /// @brief Key: a unit and a parameter.
  static LUMEX_CONSTEXPR registry_key_t
  unit_param (std::uint32_t unit, std::uint32_t param) LUMEX_NOEXCEPT
  {
    return make_unit_param_key (unit, param);
  }

  /// @brief Key: a Modbus address.
  static LUMEX_CONSTEXPR registry_key_t
  modbus (std::uint32_t address) LUMEX_NOEXCEPT
  {
    return make_modbus_key (address);
  }

  /// @brief Key: a mnemonic of one to three characters.
  template <std::size_t N>
  static LUMEX_CONSTEXPR registry_key_t
  mnemonic (char const (&text)[N]) LUMEX_NOEXCEPT
  {
    return make_mnemonic_key (text);
  }
};

/**
 * @brief `field_of<T...>::type`: the one field of the type group of a
 * registry row (a vocabulary name resolved).
 */
template <typename... T> struct field_of
{
  static_assert (sizeof...(T) == 1,
                 "LUMEX_PROTOCOL_REGISTRY: the type group of a row holds "
                 "exactly one field");
  /// Not used: the group is invalid.
  typedef void type;
};

/// @copydoc field_of
template <typename T> struct field_of<T>
{
  static_assert (::lumex::core::protocol::field::is_field<T>::value,
                 "LUMEX_PROTOCOL_REGISTRY: the type of a row is a field "
                 "type (u16_be_<scale<...> >, u8_, ...)");
  /// The field.
  typedef typename ::lumex::core::protocol::field::resolve<T>::type type;
};

/// @brief The run-time entry of one row of a registry.
struct registry_entry_t
{
  /// The C++ identifier of the row (`"FREQUENCY"`).
  char const *identifier;
  /// The name in the specification (`"FREQ"` for a `P_AS` row, else the
  /// identifier).
  char const *specification_name;
  /// The description of the row.
  char const *description;
  /// The key.
  registry_key_t key;
  /// The type of the value: width, signedness, scale and unit.
  ::lumex::core::protocol::field::field_info_t type;
  /// The access.
  access_t access;
  /// `limits (min, max)` was given.
  bool has_limits;
  /// The smallest engineering value, 0 without limits.
  double lower;
  /// The largest engineering value, 0 without limits.
  double upper;
  /// `firmware (from, to)` was given.
  bool has_firmware;
  /// The first firmware version of the entry, 0 without the option.
  std::uint32_t firmware_from;
  /// The last firmware version of the entry, 0 without the option.
  std::uint32_t firmware_to;
  /// The models of the entry, empty without the option.
  ::lumex::core::protocol::dsl::names_t models;
};

/// @brief The run-time table of a registry.
typedef ::lumex::core::span::view::span<registry_entry_t const> entries_t;

namespace detail
{
template <typename Row, bool Present> struct limits_of
{
  static LUMEX_CONSTEXPR limits_t
  get () LUMEX_NOEXCEPT
  {
    return limits_t (0.0, 0.0);
  }
};

template <typename Row> struct limits_of<Row, true>
{
  static LUMEX_CONSTEXPR limits_t
  get () LUMEX_NOEXCEPT
  {
    return ::lumex::core::protocol::dsl::get_option<limits_kind> (
        Row::options ());
  }
};
} // namespace detail

/// @brief The run-time entry of registry row `Row`.
template <typename Row>
registry_entry_t
make_registry_entry () LUMEX_NOEXCEPT
{
  namespace dsl = ::lumex::core::protocol::dsl;
  typedef dsl::detail::command_has_option<Row, limits_kind> has_limits;
  typedef dsl::detail::command_has_option<Row, dsl::kind::firmware>
      has_firmware;
  limits_t const limits = detail::limits_of<Row, has_limits::value>::get ();
  dsl::firmware_t const firmware
      = dsl::detail::firmware_of<Row, has_firmware::value>::get ();
  registry_entry_t entry = {
    Row::identifier (),
    Row::specification_name (),
    Row::description (),
    Row::key (),
    ::lumex::core::protocol::field::impl_of<
        typename Row::field_type>::type::info (),
    Row::access (),
    has_limits::value,
    limits.lower,
    limits.upper,
    has_firmware::value,
    firmware.from,
    firmware.to,
    dsl::detail::models_of<Row, dsl::detail::command_has_option<
                                    Row, dsl::kind::model>::value>::get ()
  };
  return entry;
}

namespace detail
{
template <typename List> struct entry_table;

template <typename... Rows>
struct entry_table<::lumex::core::protocol::dsl::type_list<Rows...>>
{
  static entries_t
  get () LUMEX_NOEXCEPT
  {
    // One entry more than rows: an empty registry still has an array.
    static registry_entry_t const entries[sizeof...(Rows) + 1]
        = { make_registry_entry<Rows> ()..., registry_entry_t () };
    return entries_t (entries, sizeof...(Rows));
  }
};
} // namespace detail

/// @brief Whether `T` is a registry (`LUMEX_PROTOCOL_REGISTRY`).
template <typename T, typename = void> struct is_registry : std::false_type
{
};

/// @copydoc is_registry
template <typename T>
struct is_registry<T,
                   typename ::lumex::core::protocol::dsl::detail::row_voider<
                       typename T::lumex_protocol_registry>::type>
    : std::is_same<typename T::lumex_protocol_registry, registry_tag>
{
};

/**
 * @brief The entry with key `key`, or `nullptr`.
 */
inline registry_entry_t const *
find_entry (entries_t const &entries, registry_key_t const &key) LUMEX_NOEXCEPT
{
  for (std::size_t i = 0; i < entries.size (); ++i)
    if (same_key (entries.data ()[i].key, key))
      return entries.data () + i;
  return nullptr;
}

/**
 * @brief Looks for two entries with the same key; false when the keys are
 * all different, otherwise `first` and `second` are the positions of the
 * first pair found.
 */
inline bool
find_duplicate_key (entries_t const &entries, std::size_t &first,
                    std::size_t &second) LUMEX_NOEXCEPT
{
  for (std::size_t i = 0; i < entries.size (); ++i)
    for (std::size_t j = i + 1u; j < entries.size (); ++j)
      if (same_key (entries.data ()[i].key, entries.data ()[j].key))
        {
          first = i;
          second = j;
          return true;
        }
  return false;
}

/**
 * @brief Whether the access `need` (`read` or `write`) is allowed on
 * `entry`: `access_denied` otherwise.
 */
inline std::error_code
check_access (registry_entry_t const &entry, access_t need) LUMEX_NOEXCEPT
{
  if ((static_cast<unsigned> (entry.access) & static_cast<unsigned> (need))
      != static_cast<unsigned> (need))
    return ::lumex::core::protocol::error::make_error_code (
        ::lumex::core::protocol::error::protocol_errc::access_denied);
  return std::error_code ();
}

/**
 * @brief Whether the engineering value `value` is inside the limits of
 * `entry` (`out_of_limits` otherwise; `invalid_value` for NaN). An entry
 * without limits accepts every value its type can hold.
 */
inline std::error_code
check_limits (registry_entry_t const &entry, double value) LUMEX_NOEXCEPT
{
  if (!(value == value))
    return ::lumex::core::protocol::error::make_error_code (
        ::lumex::core::protocol::error::protocol_errc::invalid_value);
  if (entry.has_limits && (value < entry.lower || value > entry.upper))
    return ::lumex::core::protocol::error::make_error_code (
        ::lumex::core::protocol::error::protocol_errc::out_of_limits);
  return std::error_code ();
}

/**
 * @brief The raw value of the engineering value `value` for the type of
 * `entry`: scaled, rounded to the nearest integer (halves away from zero),
 * false when it does not fit the width and signedness of the type.
 * @details Only the integer kinds (`integer`, `sign_magnitude`,
 * `enumeration`) have a raw value; a floating or text entry gives false.
 */
inline bool
to_raw (registry_entry_t const &entry, double value,
        std::int64_t &raw) LUMEX_NOEXCEPT
{
  namespace field = ::lumex::core::protocol::field;
  field::field_info_t const &type = entry.type;
  bool const integer = type.kind == field::field_kind::integer
                       || type.kind == field::field_kind::enumeration;
  bool const magnitude = type.kind == field::field_kind::sign_magnitude;
  if ((!integer && !magnitude) || type.binary_size == 0 || type.binary_size > 8
      || type.scale_num == 0)
    return false;
  std::size_t const bits = 8u * type.binary_size;
  if (type.is_signed || magnitude)
    {
      std::int64_t candidate = 0;
      if (!field::detail::engineering_to_raw (value, type.scale_num,
                                              type.scale_den, bits, candidate))
        return false;
      // The most negative value has no magnitude.
      std::int64_t const largest
          = bits >= 64u ? (std::numeric_limits<std::int64_t>::max) ()
                        : ((static_cast<std::int64_t> (1) << (bits - 1u)) - 1);
      if (magnitude && candidate < -largest)
        return false;
      raw = candidate;
      return true;
    }
  std::uint64_t candidate = 0;
  if (!field::detail::engineering_to_raw (value, type.scale_num,
                                          type.scale_den, bits, candidate)
      || candidate > static_cast<std::uint64_t> (
             (std::numeric_limits<std::int64_t>::max) ()))
    return false;
  raw = static_cast<std::int64_t> (candidate);
  return true;
}

/// @brief The engineering value of the raw value `raw` of `entry`.
inline double
to_engineering (registry_entry_t const &entry, std::int64_t raw) LUMEX_NOEXCEPT
{
  return static_cast<double> (raw) * static_cast<double> (entry.type.scale_num)
         / static_cast<double> (entry.type.scale_den);
}

/**
 * @brief Whether something limited to the firmware range `from`..`to`
 * (when `has_firmware`) and to `models` (when there are any) exists in the
 * firmware `firmware` and the model `model` of a session.
 * @details It is left out only when the session knows a value outside the
 * range: `firmware` 0 and a `nullptr` or empty `model` mean "not identified
 * yet" and keep everything.
 */
inline bool
option_available (bool has_firmware, std::uint32_t from, std::uint32_t to,
                  ::lumex::core::protocol::dsl::names_t const &models,
                  std::uint32_t firmware, char const *model) LUMEX_NOEXCEPT
{
  if (has_firmware && firmware != 0 && (firmware < from || firmware > to))
    return false;
  if (models.size () != 0 && model != nullptr && model[0] != '\0')
    {
      for (std::size_t i = 0; i < models.size (); ++i)
        {
          char const *name = models.data ()[i];
          std::size_t at = 0;
          while (name[at] != '\0' && name[at] == model[at])
            ++at;
          if (name[at] == '\0' && model[at] == '\0')
            return true;
        }
      return false;
    }
  return true;
}

/// @brief `option_available` for a registry entry.
inline bool
entry_available (registry_entry_t const &entry, std::uint32_t firmware,
                 char const *model) LUMEX_NOEXCEPT
{
  return option_available (entry.has_firmware, entry.firmware_from,
                           entry.firmware_to, entry.models, firmware, model);
}
} // namespace registry
} // namespace protocol
} // namespace core
} // namespace lumex

/// @cond LUMEX_INTERNAL
#define LUMEX_PROTOCOL_DETAIL_REGISTRY_ROW_BODY(type_, id_text_, expanded_,   \
                                                spec_, key_, desc_, field_,   \
                                                access_, options_)            \
  struct type_                                                                \
  {                                                                           \
    struct lumex_row : lumex_vocabulary                                       \
    {                                                                         \
      typedef ::lumex::core::protocol::registry::field_of<                    \
          LUMEX_PP_UNWRAP field_>::type field_type;                           \
      typedef decltype (::lumex::core::protocol::dsl::make_options (          \
          LUMEX_PP_UNWRAP options_)) options_type;                            \
      static LUMEX_CONSTEXPR ::lumex::core::protocol::registry::              \
          registry_key_t                                                      \
          make_key_value () LUMEX_NOEXCEPT                                    \
      {                                                                       \
        return ::lumex::core::protocol::registry::make_registry_key (         \
            LUMEX_PP_UNWRAP key_);                                            \
      }                                                                       \
      static LUMEX_CONSTEXPR ::lumex::core::protocol::registry::access_t      \
      make_access_value () LUMEX_NOEXCEPT                                     \
      {                                                                       \
        return ::lumex::core::protocol::registry::access_of (access_);        \
      }                                                                       \
      static LUMEX_CONSTEXPR options_type                                     \
      make_options_value () LUMEX_NOEXCEPT                                    \
      {                                                                       \
        return ::lumex::core::protocol::dsl::make_options (                   \
            LUMEX_PP_UNWRAP options_);                                        \
      }                                                                       \
    };                                                                        \
    typedef lumex_row::field_type field_type;                                 \
    typedef lumex_row::options_type options_type;                             \
    static LUMEX_CONSTEXPR char const *                                       \
    identifier () LUMEX_NOEXCEPT                                              \
    {                                                                         \
      return id_text_;                                                        \
    }                                                                         \
    static LUMEX_CONSTEXPR char const *                                       \
    specification_name () LUMEX_NOEXCEPT                                      \
    {                                                                         \
      return spec_;                                                           \
    }                                                                         \
    static LUMEX_CONSTEXPR char const *                                       \
    description () LUMEX_NOEXCEPT                                             \
    {                                                                         \
      return desc_;                                                           \
    }                                                                         \
    static LUMEX_CONSTEXPR ::lumex::core::protocol::registry::registry_key_t  \
    key () LUMEX_NOEXCEPT                                                     \
    {                                                                         \
      return lumex_row::make_key_value ();                                    \
    }                                                                         \
    static LUMEX_CONSTEXPR ::lumex::core::protocol::registry::access_t        \
    access () LUMEX_NOEXCEPT                                                  \
    {                                                                         \
      return lumex_row::make_access_value ();                                 \
    }                                                                         \
    static LUMEX_CONSTEXPR options_type                                       \
    options () LUMEX_NOEXCEPT                                                 \
    {                                                                         \
      return lumex_row::make_options_value ();                                \
    }                                                                         \
    LUMEX_PROTOCOL_DETAIL_CHECK_TEXT (id_text_, expanded_);                   \
  };

#define LUMEX_PROTOCOL_DETAIL_REGISTRY_ROW(id_, key_, desc_, field_, access_, \
                                           options_)                          \
  LUMEX_PROTOCOL_DETAIL_REGISTRY_ROW_BODY (                                   \
      id_##_t, #id_, LUMEX_PROTOCOL_DETAIL_EXPANDED (id_), #id_, key_, desc_, \
      field_, access_, options_)
#define LUMEX_PROTOCOL_DETAIL_REGISTRY_ROW_AS(id_, spec_, key_, desc_,        \
                                              field_, access_, options_)      \
  LUMEX_PROTOCOL_DETAIL_REGISTRY_ROW_BODY (                                   \
      id_##_t, #id_, LUMEX_PROTOCOL_DETAIL_EXPANDED (id_), spec_, key_,       \
      desc_, field_, access_, options_)

#define LUMEX_PROTOCOL_DETAIL_REGISTRY_ENUMERATOR(id_, key_, desc_, field_,   \
                                                  access_, options_)          \
  id_,
#define LUMEX_PROTOCOL_DETAIL_REGISTRY_ENUMERATOR_AS(                         \
    id_, spec_, key_, desc_, field_, access_, options_)                       \
  id_,

#define LUMEX_PROTOCOL_DETAIL_REGISTRY_RESOLVE(id_, key_, desc_, field_,      \
                                               access_, options_)             \
  , id_##_t
#define LUMEX_PROTOCOL_DETAIL_REGISTRY_RESOLVE_AS(id_, spec_, key_, desc_,    \
                                                  field_, access_, options_)  \
  , id_##_t
/// @endcond

/**
 * @brief Defines registry `name_`: a struct with a row type per entry, the
 * enumeration `id` of the entries (the typed argument of a generic command)
 * and `entries ()`, the run-time table.
 * @param name_ The registry type.
 * @param LIST_ The row list macro `LIST_ (P, P_AS)`, see the file
 * description.
 */
#define LUMEX_PROTOCOL_REGISTRY(name_, LIST_)                                 \
  struct name_                                                                \
  {                                                                           \
    typedef ::lumex::core::protocol::registry::registry_tag                   \
        lumex_protocol_registry;                                              \
    typedef ::lumex::core::protocol::registry::vocabulary lumex_vocabulary;   \
    static LUMEX_CONSTEXPR char const *                                       \
    registry_name () LUMEX_NOEXCEPT                                           \
    {                                                                         \
      return #name_;                                                          \
    }                                                                         \
    LIST_ (LUMEX_PROTOCOL_DETAIL_REGISTRY_ROW,                                \
           LUMEX_PROTOCOL_DETAIL_REGISTRY_ROW_AS)                             \
    enum id : unsigned                                                        \
    {                                                                         \
      LIST_ (LUMEX_PROTOCOL_DETAIL_REGISTRY_ENUMERATOR,                       \
             LUMEX_PROTOCOL_DETAIL_REGISTRY_ENUMERATOR_AS)                    \
          lumex_end_of_entries                                                \
    };                                                                        \
    typedef ::lumex::core::protocol::dsl::list_of<void LIST_ (                \
        LUMEX_PROTOCOL_DETAIL_REGISTRY_RESOLVE,                               \
        LUMEX_PROTOCOL_DETAIL_REGISTRY_RESOLVE_AS)>::type rows;               \
    static ::lumex::core::protocol::registry::entries_t                       \
    entries () LUMEX_NOEXCEPT                                                 \
    {                                                                         \
      return ::lumex::core::protocol::registry::detail::entry_table<          \
          rows>::get ();                                                      \
    }                                                                         \
  }

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_REGISTRY_REGISTRY_HPP
