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
 * @file LumexProtocolRegistryFields.hpp
 * @brief The typed argument of a generic command: `registry_key_<R, F1,
 * F2>` (the entry as a key on the wire), `registry_value_<R, KeyIndex,
 * RawField, Needs>` (its value, checked against the entry) and the value
 * `parameter_value_t`.
 * @details A generic command is a row whose request names a registry entry
 * and a value:
 * @code
 * X (SET_PARAMETER, 0x20, "Write a parameter",
 *    (registry_key_<lamp, u8_>, registry_value_<lamp, 0, i32_le_>),
 *    ("parameter", "value"), (), ())
 * ...
 * layout::encode_request<P, SET_PARAMETER_t> (out, lamp::FREQUENCY, 20.0);
 * @endcode
 * The first argument is an enumerator of the registry (another registry's
 * enumerator does not compile), the second the engineering value. At encode
 * `registry_value_` checks, in this order: the access the command needs
 * (`Needs`, `write` by default; `access_denied`), the limits of the entry
 * (`out_of_limits`), and that the raw value fits the type of the entry and
 * the carrier field (`invalid_value`). At decode it reads the carrier,
 * checks that the raw value fits the type of the entry and gives the
 * engineering value with the entry for the log line. `KeyIndex` is the
 * position of the `registry_key_` field in the same request or reply. Every
 * entry of the registry must have an integer, sign-magnitude or enumeration
 * type (a static_assert says so): a floating or text entry has no raw value,
 * so a registry that lists such entries is kept apart from the generic
 * commands and serves the exports and `verify_registry`.
 *
 * The key is written as one or two integer fields (an index, a Modbus
 * address: `registry_key_<R, u8_>`; `typed` and `unit_param` keys:
 * `registry_key_<R, u8_, u16_le_>`) or one text field for a mnemonic
 * (`registry_key_<R, text_<2> >`). A key the registry does not have is
 * `unknown_case` at decode.
 */
#ifndef LUMEX_CORE_PROTOCOL_REGISTRY_REGISTRY_FIELDS_HPP
#define LUMEX_CORE_PROTOCOL_REGISTRY_REGISTRY_FIELDS_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <system_error>
#include <type_traits>

#include "lumex/core/protocol/error/LumexProtocolErrc.hpp"
#include "lumex/core/protocol/field/LumexProtocolFieldTraits.hpp"
#include "lumex/core/protocol/field/LumexProtocolRecord.hpp"
#include "lumex/core/protocol/field/LumexProtocolTextFields.hpp"
#include "lumex/core/protocol/field/LumexProtocolValues.hpp"
#include "lumex/core/protocol/registry/LumexProtocolRegistry.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

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
/**
 * @brief The value of a registry entry: the engineering value and the raw
 * value on the wire.
 * @details A value to write is given as an engineering value (`has_raw`
 * false, the raw value is computed at encode) or as a raw value (`has_raw`
 * true). A decoded value has both and the `entry` it was decoded for.
 */
struct parameter_value_t
{
  /// The engineering value (the raw value times the scale of the entry).
  double engineering;
  /// The raw value.
  std::int64_t raw;
  /// Whether `raw` is the value (a value to write given raw, a decoded
  /// value).
  bool has_raw;
  /// The entry a decoded value belongs to, otherwise `nullptr`.
  registry_entry_t const *entry;

  /// @brief An engineering value to write.
  static parameter_value_t
  of_engineering (double value) LUMEX_NOEXCEPT
  {
    parameter_value_t result = { value, 0, false, nullptr };
    return result;
  }

  /// @brief A raw value to write.
  static parameter_value_t
  of_raw (std::int64_t value) LUMEX_NOEXCEPT
  {
    parameter_value_t result = { 0.0, value, true, nullptr };
    return result;
  }
};

/// @brief Equal when the same value is given the same way.
inline bool
operator== (parameter_value_t const &left,
            parameter_value_t const &right) LUMEX_NOEXCEPT
{
  return left.has_raw == right.has_raw && left.raw == right.raw
         && left.engineering == right.engineering;
}

/// @brief Not equal otherwise.
inline bool
operator!= (parameter_value_t const &left,
            parameter_value_t const &right) LUMEX_NOEXCEPT
{
  return !(left == right);
}

/**
 * @brief Whether the raw value `raw` fits the type of `entry` (its width and
 * signedness; a sign-magnitude type has no most negative value).
 */
inline bool
raw_in_range (registry_entry_t const &entry, std::int64_t raw) LUMEX_NOEXCEPT
{
  namespace field = ::lumex::core::protocol::field;
  field::field_info_t const &type = entry.type;
  bool const magnitude = type.kind == field::field_kind::sign_magnitude;
  if ((type.kind != field::field_kind::integer
       && type.kind != field::field_kind::enumeration && !magnitude)
      || type.binary_size == 0 || type.binary_size > 8)
    return false;
  std::size_t const bits = 8u * type.binary_size;
  if (type.is_signed || magnitude)
    {
      if (bits >= 64u)
        return !magnitude
               || raw != (std::numeric_limits<std::int64_t>::min) ();
      std::int64_t const top = static_cast<std::int64_t> (1) << (bits - 1u);
      return raw >= (magnitude ? -(top - 1) : -top) && raw < top;
    }
  if (raw < 0)
    return false;
  return bits >= 63u || raw < (static_cast<std::int64_t> (1) << bits);
}

namespace detail
{
namespace field = ::lumex::core::protocol::field;

template <typename V> struct is_text_value : std::false_type
{
};

template <> struct is_text_value<field::text_view> : std::true_type
{
};

template <std::size_t N>
struct is_text_value<field::fixed_string<N>> : std::true_type
{
};

// One wire part of a key (void: no part).
template <typename F> struct key_part
{
  typedef typename field::impl_of<F>::type impl;
  typedef typename impl::value_type value_type;
  static_assert (std::is_integral<value_type>::value
                     || is_text_value<value_type>::value,
                 "registry_key_: a part of a key is an integer field "
                 "(without scale<>) or a text field");
  typedef typename impl::binary_size binary_size;
  typedef typename impl::binary_alignment binary_alignment;
  static LUMEX_CONSTEXPR bool
  present () LUMEX_NOEXCEPT
  {
    return true;
  }
};

template <> struct key_part<void>
{
  struct impl
  {
    template <bool Last, typename Reader, typename Siblings>
    static bool
    decode (Reader &, std::uint8_t &, Siblings const &)
    {
      return true;
    }

    template <bool Last, typename Writer, typename Siblings>
    static bool
    encode (Writer &, std::uint8_t const &, Siblings const &)
    {
      return true;
    }

    static bool
    assign (std::uint8_t &, std::uint32_t const &)
    {
      return true;
    }
  };
  typedef std::uint8_t value_type;
  typedef std::integral_constant<std::size_t, 0> binary_size;
  typedef std::integral_constant<std::size_t, 1> binary_alignment;
  static LUMEX_CONSTEXPR bool
  present () LUMEX_NOEXCEPT
  {
    return false;
  }
};

// The number of a numeric part.
template <typename V>
typename std::enable_if<!is_text_value<V>::value, bool>::type
part_number (V const &value, std::uint32_t &number) LUMEX_NOEXCEPT
{
  // A negative value becomes a huge one and is refused with the others.
  std::uint64_t const wide = static_cast<std::uint64_t> (value);
  if (wide > (std::numeric_limits<std::uint32_t>::max) ())
    return false;
  number = static_cast<std::uint32_t> (wide);
  return true;
}

template <typename V>
typename std::enable_if<is_text_value<V>::value, bool>::type
part_number (V const &, std::uint32_t &) LUMEX_NOEXCEPT
{
  return false;
}

// The characters of a text part (one to three).
inline bool
part_text (field::text_view const &view, registry_key_t &key) LUMEX_NOEXCEPT
{
  if (view.size () == 0 || view.size () > 3)
    return false;
  for (std::size_t i = 0; i < view.size (); ++i)
    key.mnemonic[i] = view.data ()[i];
  return true;
}

template <typename V>
typename std::enable_if<is_text_value<V>::value, bool>::type
part_text (V const &value, registry_key_t &key) LUMEX_NOEXCEPT
{
  return part_text (value.view (), key);
}

template <typename V>
typename std::enable_if<!is_text_value<V>::value, bool>::type
part_text (V const &, registry_key_t &) LUMEX_NOEXCEPT
{
  return false;
}

// The key a decoded pair of parts stands for.
template <typename P1, typename P2>
bool
build_key (registry_key_kind kind, typename P1::value_type const &first,
           typename P2::value_type const &second,
           registry_key_t &key) LUMEX_NOEXCEPT
{
  key = registry_key_t{ kind, 0, 0, { 0, 0, 0, 0 } };
  if (kind == registry_key_kind::mnemonic)
    return !P2::present () && part_text (first, key);
  if (!part_number (first, key.first))
    return false;
  bool const two = kind == registry_key_kind::typed
                   || kind == registry_key_kind::unit_param;
  if (two != P2::present ())
    return false;
  return !two || part_number (second, key.second);
}

template <typename V>
typename std::enable_if<!is_text_value<V>::value, bool>::type
assign_number (V &target, std::uint32_t number)
{
  target = static_cast<V> (number);
  return static_cast<std::uint64_t> (target) == number;
}

template <typename V>
typename std::enable_if<is_text_value<V>::value, bool>::type
assign_number (V &, std::uint32_t)
{
  return false;
}

template <typename V>
typename std::enable_if<is_text_value<V>::value, bool>::type
assign_text (V &target, registry_key_t const &key)
{
  std::size_t length = 0;
  while (length < 3 && key.mnemonic[length] != '\0')
    ++length;
  return target.assign (field::text_view (key.mnemonic, length));
}

template <typename V>
typename std::enable_if<!is_text_value<V>::value, bool>::type
assign_text (V &, registry_key_t const &)
{
  return false;
}

// A 64-bit unsigned carrier value above the largest int64 has no raw value.
template <typename V>
bool
fits_int64 (V value) LUMEX_NOEXCEPT
{
  return std::is_signed<V>::value
         || static_cast<std::uint64_t> (value) <= static_cast<std::uint64_t> (
                (std::numeric_limits<std::int64_t>::max) ());
}

template <std::size_t A, std::size_t B> struct size_plus
{
  typedef std::integral_constant<std::size_t, (A == field::variable_size
                                               || B == field::variable_size)
                                                  ? field::variable_size
                                                  : A + B>
      type;
};
} // namespace detail

/**
 * @brief The key of a registry entry on the wire: one or two fields.
 * @tparam R The registry.
 * @tparam F1 The first part: an integer field, or a text field (`text_<N>`,
 * `token_`) for a mnemonic key.
 * @tparam F2 The second part of a `typed` or `unit_param` key, otherwise
 * `void`.
 * @details The value is an enumerator of `R::id`. The wire part for each
 * kind of key: `index`, `modbus`: `F1`; `typed`, `unit_param`: `F1` then
 * `F2`; `mnemonic`: a text `F1`.
 */
template <typename R, typename F1, typename F2 = void> struct registry_key_
{
  static_assert (is_registry<R>::value,
                 "registry_key_<R, ...>: R is a LUMEX_PROTOCOL_REGISTRY type");
  /// The first part.
  typedef detail::key_part<
      typename ::lumex::core::protocol::field::resolve<F1>::type>
      part1;
  /// The second part.
  typedef detail::key_part<typename std::conditional<
      std::is_void<F2>::value, void,
      typename ::lumex::core::protocol::field::resolve<F2>::type>::type>
      part2;

  /// Marks the type as a field.
  typedef ::lumex::core::protocol::field::field_tag lumex_protocol_field;
  /// The registry.
  typedef R registry_type;
  /// The value: an enumerator of the registry.
  typedef typename R::id value_type;
  /// Bytes in a binary layout.
  typedef
      typename detail::size_plus<part1::binary_size::value,
                                 part2::binary_size::value>::type binary_size;
  /// The alignment of the first part.
  typedef typename part1::binary_alignment binary_alignment;

  /// @brief The descriptor (`count` the number of entries).
  static LUMEX_CONSTEXPR ::lumex::core::protocol::field::field_info_t
  info () LUMEX_NOEXCEPT
  {
    return ::lumex::core::protocol::field::detail::make_info (
        ::lumex::core::protocol::field::field_kind::registry_key,
        ::lumex::core::protocol::field::detail::info_size (binary_size::value),
        ::lumex::core::protocol::field::byte_order::big, false, 1, 1, "",
        R::lumex_end_of_entries);
  }

  /// @brief Reads the parts and finds the entry.
  template <bool Last, typename Reader, typename Siblings>
  static bool
  decode (Reader &reader, value_type &value, Siblings const &siblings)
  {
    typename part1::value_type first = typename part1::value_type ();
    typename part2::value_type second = typename part2::value_type ();
    entries_t const entries = R::entries ();
    if (entries.size () == 0)
      return reader.fail (
          ::lumex::core::protocol::error::protocol_errc::unknown_case);
    if (!part1::impl::template decode<(!part2::present () && Last)> (
            reader, first, siblings)
        || !part2::impl::template decode<Last> (reader, second, siblings))
      return false;
    registry_key_t key = entries.data ()[0].key;
    if (!detail::build_key<part1, part2> (entries.data ()[0].key.kind, first,
                                          second, key))
      return reader.fail (
          ::lumex::core::protocol::error::protocol_errc::invalid_value);
    registry_entry_t const *const found = find_entry (entries, key);
    if (found == nullptr)
      return reader.fail (
          ::lumex::core::protocol::error::protocol_errc::unknown_case);
    value = static_cast<value_type> (found - entries.data ());
    return true;
  }

  /// @brief Writes the parts of the key of the entry.
  template <bool Last, typename Writer, typename Siblings>
  static bool
  encode (Writer &writer, value_type const &value, Siblings const &siblings)
  {
    entries_t const entries = R::entries ();
    if (static_cast<std::size_t> (value) >= entries.size ())
      return writer.fail (
          ::lumex::core::protocol::error::protocol_errc::invalid_value);
    registry_key_t const key = entries.data ()[value].key;
    typename part1::value_type first = typename part1::value_type ();
    typename part2::value_type second = typename part2::value_type ();
    bool const two = key.kind == registry_key_kind::typed
                     || key.kind == registry_key_kind::unit_param;
    bool const filled
        = key.kind == registry_key_kind::mnemonic
              ? !part2::present () && detail::assign_text (first, key)
              : (two == part2::present ()
                 && detail::assign_number (first, key.first)
                 && (!two || detail::assign_number (second, key.second)));
    if (!filled
        || !part1::impl::template encode<(!part2::present () && Last)> (
            writer, first, siblings)
        || !part2::impl::template encode<Last> (writer, second, siblings))
      return writer.fail (
          ::lumex::core::protocol::error::protocol_errc::invalid_value);
    return true;
  }

  /// @brief Sets `value` from an enumerator of the registry.
  template <typename Arg>
  static bool
  assign (value_type &value, Arg const &argument)
  {
    static_assert (std::is_same<Arg, value_type>::value,
                   "LUMEX_PROTOCOL: a registry_key_ takes an enumerator of "
                   "its own registry (lamp::FREQUENCY)");
    value = argument;
    return true;
  }
};

namespace detail
{
// The entries whose value is an integer on the wire: integer, sign-magnitude
// and enumeration types have a raw value; floating and text types do not.
LUMEX_CONSTEXPR inline bool
has_raw_value (::lumex::core::protocol::field::field_kind kind) LUMEX_NOEXCEPT
{
  return kind == ::lumex::core::protocol::field::field_kind::integer
         || kind == ::lumex::core::protocol::field::field_kind::sign_magnitude
         || kind == ::lumex::core::protocol::field::field_kind::enumeration;
}

template <typename Row>
struct row_has_raw_value
    : std::integral_constant<
          bool, has_raw_value (::lumex::core::protocol::field::impl_of<
                                   typename Row::field_type>::type::info ()
                                   .kind)>
{
};

template <typename List> struct all_raw_entries;

template <typename... Rows>
struct all_raw_entries<::lumex::core::protocol::dsl::type_list<Rows...>>
    : ::lumex::core::protocol::field::detail::values_all<
          row_has_raw_value<Rows>::value...>
{
};
} // namespace detail

/**
 * @brief The value of a registry entry on the wire, checked against the
 * entry (see the file description).
 * @tparam R The registry.
 * @tparam KeyIndex The position of the `registry_key_<R, ...>` field among
 * the fields of the same request or reply.
 * @tparam RawField The carrier: an integer field without `scale<>` that
 * holds the raw value of any entry (`i32_le_`).
 * @tparam Needs The access the command needs: `access_t::write` (a command
 * that sets a parameter, the default) or `access_t::read` (a reply that
 * reports it).
 */
template <typename R, std::size_t KeyIndex, typename RawField,
          access_t Needs = access_t::write>
struct registry_value_
{
  static_assert (is_registry<R>::value,
                 "registry_value_<R, ...>: R is a LUMEX_PROTOCOL_REGISTRY "
                 "type");
  /// The carrier implementation.
  typedef typename ::lumex::core::protocol::field::impl_of<RawField>::type
      raw_impl;
  static_assert (std::is_integral<typename raw_impl::value_type>::value,
                 "registry_value_<R, I, RawField>: RawField holds an integer "
                 "(no scale<>)");
  static_assert (
      detail::all_raw_entries<typename R::rows>::value,
      "registry_value_<R, ...>: every entry of the registry has an "
      "integer, sign-magnitude or enumeration type (a floating or "
      "text entry has no raw value; keep such entries in a "
      "registry of their own, for the exports and verify_registry)");

  /// Marks the type as a field.
  typedef ::lumex::core::protocol::field::field_tag lumex_protocol_field;
  /// The registry.
  typedef R registry_type;
  /// The value.
  typedef parameter_value_t value_type;
  /// Bytes in a binary layout.
  typedef typename raw_impl::binary_size binary_size;
  /// The alignment of the carrier.
  typedef typename raw_impl::binary_alignment binary_alignment;

  /// @brief The descriptor: the carrier's width and order.
  static LUMEX_CONSTEXPR ::lumex::core::protocol::field::field_info_t
  info () LUMEX_NOEXCEPT
  {
    return ::lumex::core::protocol::field::detail::make_info (
        ::lumex::core::protocol::field::field_kind::registry_value,
        raw_impl::info ().binary_size, raw_impl::info ().order,
        std::is_signed<typename raw_impl::value_type>::value, 1, 1, "",
        KeyIndex);
  }

  /// @brief Reads the carrier and interprets it for the entry of the key.
  template <bool Last, typename Reader, typename Siblings>
  static bool
  decode (Reader &reader, value_type &value, Siblings const &siblings)
  {
    typename raw_impl::value_type raw = typename raw_impl::value_type ();
    if (!raw_impl::template decode<Last> (reader, raw, siblings))
      return false;
    entries_t const entries = R::entries ();
    std::size_t const at = static_cast<std::size_t> (
        ::lumex::core::protocol::field::get<KeyIndex> (siblings));
    if (at >= entries.size () || !detail::fits_int64 (raw)
        || !raw_in_range (entries.data ()[at],
                          static_cast<std::int64_t> (raw)))
      return reader.fail (
          ::lumex::core::protocol::error::protocol_errc::invalid_value);
    registry_entry_t const &entry = entries.data ()[at];
    value.raw = static_cast<std::int64_t> (raw);
    value.has_raw = true;
    value.engineering = to_engineering (entry, value.raw);
    value.entry = &entry;
    return true;
  }

  /// @brief Checks the value against the entry of the key and writes the
  /// carrier.
  template <bool Last, typename Writer, typename Siblings>
  static bool
  encode (Writer &writer, value_type const &value, Siblings const &siblings)
  {
    namespace error = ::lumex::core::protocol::error;
    entries_t const entries = R::entries ();
    std::size_t const at = static_cast<std::size_t> (
        ::lumex::core::protocol::field::get<KeyIndex> (siblings));
    if (at >= entries.size ())
      return writer.fail (error::protocol_errc::invalid_value);
    registry_entry_t const &entry = entries.data ()[at];
    std::error_code const denied = check_access (entry, Needs);
    if (denied)
      return writer.fail (static_cast<error::protocol_errc> (denied.value ()));
    double const engineering = value.has_raw
                                   ? to_engineering (entry, value.raw)
                                   : value.engineering;
    std::error_code const outside = check_limits (entry, engineering);
    if (outside)
      return writer.fail (
          static_cast<error::protocol_errc> (outside.value ()));
    std::int64_t raw = value.raw;
    if (value.has_raw ? !raw_in_range (entry, raw)
                      : !to_raw (entry, engineering, raw))
      return writer.fail (error::protocol_errc::invalid_value);
    typename raw_impl::value_type carrier = typename raw_impl::value_type ();
    if (!raw_impl::assign (carrier, raw))
      return writer.fail (error::protocol_errc::invalid_value);
    return raw_impl::template encode<Last> (writer, carrier, siblings);
  }

  /// @brief Sets `value` from an engineering value (a number) or a
  /// `parameter_value_t`.
  template <typename Arg>
  static bool
  assign (value_type &value, Arg const &argument)
  {
    return assign_value (value, argument, std::is_arithmetic<Arg> ());
  }

private:
  template <typename Arg>
  static bool
  assign_value (value_type &value, Arg const &argument, std::true_type)
  {
    value = parameter_value_t::of_engineering (static_cast<double> (argument));
    return true;
  }

  template <typename Arg>
  static bool
  assign_value (value_type &value, Arg const &argument, std::false_type)
  {
    static_assert (std::is_same<Arg, parameter_value_t>::value,
                   "LUMEX_PROTOCOL: a registry_value_ takes an engineering "
                   "value (a number) or a parameter_value_t");
    value = argument;
    return true;
  }
};

/**
 * @brief The vocabulary of a protocol whose rows use a registry: everything
 * of `dsl::vocabulary` and the names `registry_key_` and `registry_value_`.
 * @details The header type of the protocol names it:
 * @code
 * struct lamp_header
 * {
 *   typedef registry::table_vocabulary lumex_vocabulary;
 * };
 * @endcode
 */
struct table_vocabulary : ::lumex::core::protocol::dsl::vocabulary
{
  /// @brief Field: the key of a registry entry.
  template <typename R, typename F1, typename F2 = void>
  using registry_key_
      = ::lumex::core::protocol::registry::registry_key_<R, F1, F2>;

  /// @brief Field: the value of a registry entry.
  template <typename R, std::size_t KeyIndex, typename RawField,
            access_t Needs = access_t::write>
  using registry_value_
      = ::lumex::core::protocol::registry::registry_value_<R, KeyIndex,
                                                           RawField, Needs>;
};
} // namespace registry
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_REGISTRY_REGISTRY_FIELDS_HPP
