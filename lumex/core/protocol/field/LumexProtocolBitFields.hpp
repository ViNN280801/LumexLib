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
 * @file LumexProtocolBitFields.hpp
 * @brief Fields inside a word and enumerations: `bits_<Storage, First,
 * Count, T>`, `mask_<Storage, Mask, T>`, `split_<Word, Parts...>` and
 * `enum_<Storage, E>`.
 * @details `bits_` is a run of `Count` bits from bit `First` (0 is the least
 * significant), `mask_` the bits under a mask packed together (interleaved
 * channels such as `0x55` and `0xAA`: bit `i` of the value is the `i`-th bit
 * of the mask). On its own each one reads a whole word and keeps its part;
 * several parts of one word are a `split_`, whose value keeps the raw word
 * too, so that bits no part covers come back unchanged on encode. A part
 * value is unsigned by default, `bool` for one bit, a signed type
 * (sign-extended) or an enumeration. `enum_` stores an enumeration in an
 * integer or character field. With a `LUMEX_DEFINE_REFLECTED_ENUM`
 * enumeration, in `enum_` and in a part of a word alike, a value that is no
 * enumerator is `invalid_value` (decode, encode, assign) and the names reach
 * the log line.
 */
#ifndef LUMEX_CORE_PROTOCOL_FIELD_BIT_FIELDS_HPP
#define LUMEX_CORE_PROTOCOL_FIELD_BIT_FIELDS_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ratio>
#include <type_traits>
#include <utility>

#include "lumex/core/protocol/error/LumexProtocolErrc.hpp"
#include "lumex/core/protocol/field/LumexProtocolFieldTraits.hpp"
#include "lumex/core/protocol/field/LumexProtocolIntegerFields.hpp"
#include "lumex/core/protocol/field/LumexProtocolNumeric.hpp"
#include "lumex/core/protocol/field/LumexProtocolRecord.hpp"
#include "lumex/core/protocol/field/LumexProtocolValues.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/sequence/LumexIndexSequence.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
// The to_string of a reflected enumeration is declared maybe-unused.
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#endif

/// @cond LUMEX_INTERNAL
// The probe of the to_string of a reflected enumeration: an unqualified call
// from here finds this declaration (which takes no argument) and, by ADL, the
// to_string of the enumeration's namespace, never another to_string of the
// library.
namespace lumex
{
namespace core
{
namespace protocol
{
namespace field
{
namespace enum_probe
{
void to_string ();

template <typename E, typename = void> struct has_name : std::false_type
{
};

template <typename E>
struct has_name<
    E,
    typename std::enable_if<std::is_convertible<
        decltype (to_string (std::declval<E> ())), char const *>::value>::type>
    : std::true_type
{
};

template <typename E>
char const *
name_of (E value, std::true_type) LUMEX_NOEXCEPT
{
  return to_string (value);
}

template <typename E>
char const *
name_of (E, std::false_type) LUMEX_NOEXCEPT
{
  return nullptr;
}
} // namespace enum_probe
} // namespace field
} // namespace protocol
} // namespace core
} // namespace lumex
/// @endcond

namespace lumex
{
namespace core
{
namespace protocol
{
namespace field
{
/**
 * @brief The name of `value` when `E` is a `LUMEX_DEFINE_REFLECTED_ENUM`
 * (its `to_string`), `nullptr` for another enumeration or an unknown value.
 */
template <typename E>
char const *
enum_name (E value) LUMEX_NOEXCEPT
{
  char const *const name = enum_probe::name_of (
      value, std::integral_constant<bool, enum_probe::has_name<E>::value> ());
  return name == nullptr || std::strcmp (name, "<Unknown>") == 0 ? nullptr
                                                                 : name;
}

namespace detail
{
// The value of a part of a word (Bits bits) as T and back.
template <typename T, std::size_t Bits, typename = void> struct part_value
{
  static_assert (std::is_integral<T>::value,
                 "LUMEX_PROTOCOL: a part of a word is an integer, bool or an "
                 "enumeration");
  static_assert (Bits <= 8 * sizeof (T),
                 "LUMEX_PROTOCOL: the type of a part of a word is narrower "
                 "than the part");

  static T
  from (std::uint64_t bits) LUMEX_NOEXCEPT
  {
    return std::is_signed<T>::value ? static_cast<T> (sign_extend (bits, Bits))
                                    : static_cast<T> (bits);
  }

  static bool
  valid (T) LUMEX_NOEXCEPT
  {
    return true;
  }

  static bool
  to (T value, std::uint64_t &bits) LUMEX_NOEXCEPT
  {
    if (std::is_signed<T>::value
            ? !fits_signed_bits (static_cast<std::int64_t> (value), Bits)
            : !fits_unsigned_bits (static_cast<std::uint64_t> (value), Bits))
      return false;
    bits = to_bits (static_cast<std::int64_t> (value), Bits);
    return true;
  }
};

template <std::size_t Bits> struct part_value<bool, Bits, void>
{
  static_assert (Bits == 1,
                 "LUMEX_PROTOCOL: a bool part of a word is one bit");

  static bool
  from (std::uint64_t bits) LUMEX_NOEXCEPT
  {
    return bits != 0;
  }

  static bool
  valid (bool) LUMEX_NOEXCEPT
  {
    return true;
  }

  static bool
  to (bool value, std::uint64_t &bits) LUMEX_NOEXCEPT
  {
    bits = value ? 1u : 0u;
    return true;
  }
};

template <typename E, std::size_t Bits>
struct part_value<E, Bits,
                  typename std::enable_if<std::is_enum<E>::value>::type>
{
  using underlying = typename std::underlying_type<E>::type;

  static E
  from (std::uint64_t bits) LUMEX_NOEXCEPT
  {
    return static_cast<E> (part_value<underlying, Bits>::from (bits));
  }

  // A reflected enumeration takes only its enumerators, as enum_ does.
  static bool
  valid (E value) LUMEX_NOEXCEPT
  {
    return !enum_probe::has_name<E>::value || enum_name (value) != nullptr;
  }

  static bool
  to (E value, std::uint64_t &bits) LUMEX_NOEXCEPT
  {
    return part_value<underlying, Bits>::to (static_cast<underlying> (value),
                                             bits);
  }
};

// A plain integer field holding a word: the word's raw type and the bits.
template <typename Storage> struct word_of
{
  using impl = typename impl_of<Storage>::type;
  static_assert (is_plain_integer<impl>::value,
                 "LUMEX_PROTOCOL: the word of bits_, mask_ and split_ is an "
                 "integer field without scale<> (u8_, u16_le_, ...)");
  using raw_type = typename impl::raw_type;
  static LUMEX_CONSTEXPR std::size_t
  bits () LUMEX_NOEXCEPT
  {
    return 8u * impl::binary_size::value;
  }
};

// The common part of bits_ and mask_: Mask is the word bits of the part,
// Bits its width, Gather whether the bits are gathered (mask_) or shifted
// (bits_).
template <typename Storage, std::uint64_t Mask, std::size_t Bits, typename T,
          field_kind Kind>
struct part_impl
{
  using word = word_of<Storage>;
  using storage_impl = typename word::impl;
  using raw_type = typename word::raw_type;

  static_assert (Mask != 0, "LUMEX_PROTOCOL: a part of a word has no bit");
  static_assert (fits_unsigned_bits (Mask,
                                     8u * storage_impl::binary_size::value),
                 "LUMEX_PROTOCOL: a part of a word reaches past the word");

  /// Marks the type as a field.
  using lumex_protocol_field = field_tag;
  /// The value.
  using value_type = T;
  /// The storage field (the word).
  using storage_type = Storage;
  /// The bits of the word this part uses.
  using part_mask = std::integral_constant<std::uint64_t, Mask>;
  /// Bytes in a binary layout: the word.
  using binary_size = typename storage_impl::binary_size;
  /// The alignment of the word.
  using binary_alignment = typename storage_impl::binary_alignment;

  static LUMEX_CONSTEXPR field_info_t
  info () LUMEX_NOEXCEPT
  {
    return make_info (Kind, binary_size::value, storage_impl::order (),
                      std::is_signed<T>::value, 1, 1, "", Bits);
  }

  /// The part of `word` (the bits of the word).
  static T
  extract (std::uint64_t word_bits) LUMEX_NOEXCEPT
  {
    std::uint64_t const bits = Kind == field_kind::mask
                                   ? gather_bits (word_bits, Mask)
                                   : (word_bits & Mask) >> lowest_bit (Mask);
    return part_value<T, Bits>::from (bits);
  }

  /// Puts `value` into the bits of `word_bits` this part uses; false when it
  /// does not fit.
  static bool
  is_valid (T value) LUMEX_NOEXCEPT
  {
    return part_value<T, Bits>::valid (value);
  }

  static bool
  insert (T value, std::uint64_t &word_bits) LUMEX_NOEXCEPT
  {
    std::uint64_t bits = 0;
    if (!is_valid (value) || !part_value<T, Bits>::to (value, bits))
      return false;
    std::uint64_t const placed = Kind == field_kind::mask
                                     ? scatter_bits (bits, Mask)
                                     : (bits << lowest_bit (Mask)) & Mask;
    word_bits = (word_bits & ~Mask) | placed;
    return true;
  }

  template <bool Last, typename Reader, typename Siblings>
  static bool
  decode (Reader &reader, value_type &value, Siblings const &siblings)
  {
    raw_type raw = raw_type ();
    if (!storage_impl::template decode<Last> (reader, raw, siblings))
      return false;
    value = extract (storage_impl::to_bits (raw));
    if (!is_valid (value))
      return reader.fail (error::protocol_errc::invalid_value);
    return true;
  }

  template <bool Last, typename Writer, typename Siblings>
  static bool
  encode (Writer &writer, value_type const &value, Siblings const &siblings)
  {
    std::uint64_t bits = 0;
    if (!insert (value, bits))
      return writer.fail (error::protocol_errc::invalid_value);
    return storage_impl::template encode<Last> (
        writer, storage_impl::from_bits (bits), siblings);
  }

  template <typename Arg>
  static bool
  assign (value_type &value, Arg const &argument)
  {
    static_assert (std::is_convertible<Arg, T>::value,
                   "LUMEX_PROTOCOL: the argument of a part of a word is not "
                   "convertible to its type");
    value = static_cast<T> (argument);
    std::uint64_t bits = 0;
    return insert (value, bits);
  }
};

template <typename Storage> struct default_part
{
  using type =
      typename std::make_unsigned<typename word_of<Storage>::raw_type>::type;
};
} // namespace detail

/**
 * @brief `Count` bits of a word from bit `First` (0 is the least
 * significant), as a value of type `T`.
 * @tparam Storage The word: an integer field without `scale<>`.
 * @tparam T The value: unsigned by default, `bool` for one bit, a signed
 * type (sign-extended) or an enumeration (a reflected one takes only its
 * enumerators: another value is `invalid_value`).
 */
template <typename Storage, std::size_t First, std::size_t Count,
          typename T = typename detail::default_part<Storage>::type>
struct bits_
    : detail::part_impl<
          Storage,
          (Count >= 64 ? ~static_cast<std::uint64_t> (0)
                       : ((static_cast<std::uint64_t> (1) << Count) - 1u))
              << First,
          Count, T, field_kind::bits>
{
  static_assert (Count >= 1 && First + Count <= 64,
                 "bits_<Storage, First, Count>: Count must be positive and "
                 "the run must end within 64 bits");
};

/**
 * @brief The bits of a word under `Mask`, packed together: bit `i` of the
 * value is the `i`-th set bit of the mask (interleaved channels).
 * @tparam T The value: `bool` by default for a one-bit mask, else unsigned.
 */
template <typename Storage, std::uint64_t Mask,
          typename T = typename std::conditional<
              detail::bit_count (Mask) == 1, bool,
              typename detail::default_part<Storage>::type>::type>
struct mask_ : detail::part_impl<Storage, Mask, detail::bit_count (Mask), T,
                                 field_kind::mask>
{
};

namespace detail
{
template <typename... P> struct part_list
{
};

// The parts of split_<Word, Parts...>: the parts, or the fields of one
// layout (named parts).
template <typename... Parts> struct single_group : std::false_type
{
};

template <typename Part>
struct single_group<Part> : has_fields<typename resolve<Part>::type>
{
};

template <typename List> struct list_to_parts;

template <template <typename...> class List, typename... P>
struct list_to_parts<List<P...>>
{
  using type = part_list<P...>;
};

template <bool OneGroup, typename... Parts> struct parts_of
{
  using type = part_list<Parts...>;
};

template <typename Layout> struct parts_of<true, Layout>
{
  using type = typename list_to_parts<
      typename resolve<Layout>::type::fields_type>::type;
};

template <typename Part, typename = void> struct is_part : std::false_type
{
};

template <typename Part>
struct is_part<Part,
               typename voider<typename impl_of<Part>::type::part_mask>::type>
    : std::true_type
{
};

template <std::uint64_t... M> struct mask_or;

template <> struct mask_or<> : std::integral_constant<std::uint64_t, 0>
{
};

template <std::uint64_t Head, std::uint64_t... Tail>
struct mask_or<Head, Tail...>
    : std::integral_constant<std::uint64_t, Head | mask_or<Tail...>::value>
{
};

template <std::size_t... N> struct count_sum;

template <> struct count_sum<> : std::integral_constant<std::size_t, 0>
{
};

template <std::size_t Head, std::size_t... Tail>
struct count_sum<Head, Tail...>
    : std::integral_constant<std::size_t, Head + count_sum<Tail...>::value>
{
};

template <typename Word, typename Parts, typename Sequence> struct split_body;

template <typename Word, typename... P, std::size_t... I>
struct split_body<Word, part_list<P...>,
                  ::lumex::core::utility::sequence::index_sequence<I...>>
{
  using word = word_of<Word>;
  using word_impl = typename word::impl;
  using raw_type = typename word::raw_type;

  static_assert (sizeof...(P) > 0, "split_: give at least one part");
  static_assert (values_all<is_part<P>::value...>::value,
                 "LUMEX_PROTOCOL: every part of split_<Word, ...> is a "
                 "bits_<...> or a mask_<...>");
  static_assert (
      values_all<std::is_same<
          typename impl_of<typename impl_of<P>::type::storage_type>::type,
          word_impl>::value...>::value,
      "LUMEX_PROTOCOL: every part of split_<Word, ...> uses the word of the "
      "split as its storage");
  static_assert (
      bit_count (mask_or<impl_of<P>::type::part_mask::value...>::value)
          == count_sum<
              bit_count (impl_of<P>::type::part_mask::value)...>::value,
      "LUMEX_PROTOCOL: two parts of split_<Word, ...> share a bit");

  using covered = std::integral_constant<
      std::uint64_t, mask_or<impl_of<P>::type::part_mask::value...>::value>;

  /// Marks the type as a field.
  using lumex_protocol_field = field_tag;
  /// The value: the raw word and the parts.
  using value_type = split_value<raw_type, record<P...>>;
  /// Bytes in a binary layout: the word.
  using binary_size = typename word_impl::binary_size;
  /// The alignment of the word.
  using binary_alignment = typename word_impl::binary_alignment;

  static LUMEX_CONSTEXPR field_info_t
  info () LUMEX_NOEXCEPT
  {
    return make_info (field_kind::split, binary_size::value,
                      word_impl::order (), false, 1, 1, "", sizeof...(P));
  }

  template <bool Last, typename Reader, typename Siblings>
  static bool
  decode (Reader &reader, value_type &value, Siblings const &siblings)
  {
    if (!word_impl::template decode<Last> (reader, value.raw, siblings))
      return false;
    std::uint64_t const bits = word_impl::to_bits (value.raw);
    bool valid = true;
    bool const results[] = {
      true,
      (get<I> (value.parts) = impl_of<P>::type::extract (bits),
       valid = valid && impl_of<P>::type::is_valid (get<I> (value.parts)))...
    };
    LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (results);
    if (!valid)
      return reader.fail (error::protocol_errc::invalid_value);
    return true;
  }

  template <bool Last, typename Writer, typename Siblings>
  static bool
  encode (Writer &writer, value_type const &value, Siblings const &siblings)
  {
    std::uint64_t bits = word_impl::to_bits (value.raw);
    bool ok = true;
    bool const results[] = {
      true,
      (ok = ok && impl_of<P>::type::insert (get<I> (value.parts), bits))...
    };
    (void)results;
    if (!ok)
      return writer.fail (error::protocol_errc::invalid_value);
    return word_impl::template encode<Last> (
        writer, word_impl::from_bits (bits), siblings);
  }

  template <typename Arg>
  static bool
  assign (value_type &value, Arg const &argument)
  {
    static_assert (std::is_same<Arg, value_type>::value,
                   "LUMEX_PROTOCOL: split_ is given as its split_value");
    value = argument;
    return true;
  }
};
template <typename Word, typename Parts> struct split_impl;

template <typename Word, typename... P>
struct split_impl<Word, part_list<P...>>
    : split_body<
          Word, part_list<P...>,
          ::lumex::core::utility::sequence::make_index_sequence<sizeof...(P)>>
{
};
} // namespace detail

/**
 * @brief Several parts of one word: `split_<u16_le_, bits_<u16_le_, 0, 12>,
 * bits_<u16_le_, 12, 4> >`, or `split_<u16_le_, parts_t>` with a
 * `LUMEX_PROTOCOL_LAYOUT` of parts (named parts).
 * @details Every part is a `bits_` or `mask_` of the same word; two parts
 * may not share a bit. The value keeps the raw word (`raw`) and the parts
 * (`parts`, a record); on encode the bits no part covers come from `raw`.
 */
template <typename Word, typename... Parts> struct split_
{
  /// Marks the type as a field.
  using lumex_protocol_field = field_tag;
  /// The implementation.
  using lumex_field_impl = detail::split_impl<
      Word, typename detail::parts_of<detail::single_group<Parts...>::value,
                                      Parts...>::type>;
};

/**
 * @brief An enumeration `E` stored as field `Storage` (an integer field
 * without `scale<>`, `char_`, `ascii_hex_<W>`).
 * @details With a `LUMEX_DEFINE_REFLECTED_ENUM` enumeration a value that is
 * no enumerator is `invalid_value` (decode and encode); another enumeration
 * takes any value of its underlying type.
 */
template <typename Storage, typename E> struct enum_
{
  static_assert (std::is_enum<E>::value,
                 "enum_<Storage, E>: E must be an enumeration");

  /// The storage implementation.
  using storage_impl = typename impl_of<Storage>::type;
  /// The value of the storage.
  using storage_value = typename storage_impl::value_type;
  static_assert (std::is_integral<storage_value>::value,
                 "enum_<Storage, E>: Storage must hold an integer or a "
                 "character (no scale<>)");

  /// Marks the type as a field.
  using lumex_protocol_field = field_tag;
  /// The value.
  using value_type = E;
  /// Bytes in a binary layout: those of the storage.
  using binary_size = typename storage_impl::binary_size;
  /// The alignment of the storage.
  using binary_alignment = typename storage_impl::binary_alignment;

  /// @brief The descriptor (`order` and `binary_size` of the storage).
  static LUMEX_CONSTEXPR field_info_t
  info () LUMEX_NOEXCEPT
  {
    return detail::make_info (
        field_kind::enumeration, storage_impl::info ().binary_size,
        storage_impl::info ().order, std::is_signed<storage_value>::value, 1,
        1, "", 0u);
  }

  /// @brief Whether `value` is a value the field accepts.
  static bool
  is_valid (E value) LUMEX_NOEXCEPT
  {
    return !enum_probe::has_name<E>::value || enum_name (value) != nullptr;
  }

  /// @brief Reads the field; `invalid_value` for an unknown enumerator.
  template <bool Last, typename Reader, typename Siblings>
  static bool
  decode (Reader &reader, value_type &value, Siblings const &siblings)
  {
    storage_value stored = storage_value ();
    if (!storage_impl::template decode<Last> (reader, stored, siblings))
      return false;
    value = static_cast<E> (
        static_cast<typename std::underlying_type<E>::type> (stored));
    if (!is_valid (value))
      return reader.fail (error::protocol_errc::invalid_value);
    return true;
  }

  /// @brief Writes the field; `invalid_value` for an unknown enumerator or
  /// one the storage cannot hold.
  template <bool Last, typename Writer, typename Siblings>
  static bool
  encode (Writer &writer, value_type const &value, Siblings const &siblings)
  {
    storage_value stored = storage_value ();
    if (!is_valid (value) || !store (value, stored))
      return writer.fail (error::protocol_errc::invalid_value);
    return storage_impl::template encode<Last> (writer, stored, siblings);
  }

  /// @brief Sets `value` from an enumerator of `E`.
  template <typename Arg>
  static bool
  assign (value_type &value, Arg const &argument)
  {
    static_assert (std::is_same<Arg, E>::value,
                   "LUMEX_PROTOCOL: enum_<Storage, E> takes an enumerator of "
                   "E");
    value = argument;
    storage_value stored = storage_value ();
    return is_valid (value) && store (value, stored);
  }

private:
  static bool
  store (E value, storage_value &stored)
  {
    using underlying = typename std::underlying_type<E>::type;
    return storage_impl::assign (stored, static_cast<storage_value> (
                                             static_cast<underlying> (value)))
           && static_cast<underlying> (stored)
                  == static_cast<underlying> (value);
  }
};
} // namespace field
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_FIELD_BIT_FIELDS_HPP
