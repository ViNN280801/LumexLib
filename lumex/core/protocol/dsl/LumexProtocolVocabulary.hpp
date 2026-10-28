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
 * @file LumexProtocolVocabulary.hpp
 * @brief `dsl::vocabulary`: the names a protocol table uses without a
 * namespace (field types, modifiers, units, row options, flags, `level`),
 * and the customization point `vocabulary_of<Header>` through which another
 * layer adds its own names.
 * @details Every group of a row (key, fields, reply, options) and of a
 * layout is evaluated inside a class derived from the vocabulary, so class
 * scope lookup finds `u16_le_`, `scale`, `timeout`, `dangerous` or
 * `level::debug` first, then the names of the user's namespace (layouts,
 * state enumerations). The table needs no `using namespace` and the names do
 * not leak into the user's namespace or into the command types.
 *
 * The integer, floating, `char_` and `token_` names are usable with and
 * without `<>` (`u8_` is `u8_<>`): the vocabulary derives from one carrier
 * class per name, whose injected class name is both the type and the
 * template. Every other field is a template with arguments.
 *
 * A layer above the core (the dialogue of `applied/transport/protocol`: reply
 * kinds, correlation, rate limits) extends the vocabulary without editing
 * it: a class derived from `dsl::vocabulary` with the new names, named by the
 * header type of the protocol as `using lumex_vocabulary = ...;` (or given by
 * a specialization of `dsl::vocabulary_of<Header>`). The rows of that
 * protocol and of its layers then see the new names. A word of the
 * vocabulary hides a type of the same name of the user's namespace inside a
 * table (`stream`, `level`, `unit`, `model`, ...): write such a type with
 * its namespace (`::widget::stream`); the DSL says so when a group names a
 * vocabulary word where a field or a reply belongs.
 */
#ifndef LUMEX_CORE_PROTOCOL_DSL_VOCABULARY_HPP
#define LUMEX_CORE_PROTOCOL_DSL_VOCABULARY_HPP

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "lumex/core/protocol/dsl/LumexProtocolOptions.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolRow.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolTypeList.hpp"
#include "lumex/core/protocol/field/LumexProtocolBitFields.hpp"
#include "lumex/core/protocol/field/LumexProtocolCompositeFields.hpp"
#include "lumex/core/protocol/field/LumexProtocolFieldTraits.hpp"
#include "lumex/core/protocol/field/LumexProtocolIntegerFields.hpp"
#include "lumex/core/protocol/field/LumexProtocolModifiers.hpp"
#include "lumex/core/protocol/field/LumexProtocolTextFields.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif

namespace lumex
{
namespace core
{
namespace protocol
{
namespace dsl
{
namespace detail
{
template <bool... B> struct vocabulary_bools
{
};

template <bool... B>
struct vocabulary_all
    : std::is_same<vocabulary_bools<true, B...>, vocabulary_bools<B..., true>>
{
};
} // namespace detail

/// @brief The carriers of the field names usable without `<>`: `u8_` in a
/// table is `plain::u8_<>`, which stands for `field::u8_<>`
/// (`field::resolve`).
namespace plain
{
/// @cond LUMEX_INTERNAL
#define LUMEX_PROTOCOL_DETAIL_PLAIN_NAME(name_)                               \
  template <typename... Modifiers> struct name_                               \
  {                                                                           \
    using lumex_named_field                                                   \
        = ::lumex::core::protocol::field::name_<Modifiers...>;                \
  }
#define LUMEX_PROTOCOL_DETAIL_PLAIN_TYPE(name_)                               \
  template <typename... Modifiers> struct name_                               \
  {                                                                           \
    static_assert (sizeof...(Modifiers) == 0, #name_ " takes no argument");   \
    using lumex_named_field = ::lumex::core::protocol::field::name_;          \
  }
/// @endcond

LUMEX_PROTOCOL_DETAIL_PLAIN_NAME (u8_);
LUMEX_PROTOCOL_DETAIL_PLAIN_NAME (i8_);
LUMEX_PROTOCOL_DETAIL_PLAIN_NAME (u16_le_);
LUMEX_PROTOCOL_DETAIL_PLAIN_NAME (u16_be_);
LUMEX_PROTOCOL_DETAIL_PLAIN_NAME (i16_le_);
LUMEX_PROTOCOL_DETAIL_PLAIN_NAME (i16_be_);
LUMEX_PROTOCOL_DETAIL_PLAIN_NAME (u24_le_);
LUMEX_PROTOCOL_DETAIL_PLAIN_NAME (u24_be_);
LUMEX_PROTOCOL_DETAIL_PLAIN_NAME (i24_le_);
LUMEX_PROTOCOL_DETAIL_PLAIN_NAME (i24_be_);
LUMEX_PROTOCOL_DETAIL_PLAIN_NAME (u32_le_);
LUMEX_PROTOCOL_DETAIL_PLAIN_NAME (u32_be_);
LUMEX_PROTOCOL_DETAIL_PLAIN_NAME (i32_le_);
LUMEX_PROTOCOL_DETAIL_PLAIN_NAME (i32_be_);
LUMEX_PROTOCOL_DETAIL_PLAIN_NAME (u64_le_);
LUMEX_PROTOCOL_DETAIL_PLAIN_NAME (u64_be_);
LUMEX_PROTOCOL_DETAIL_PLAIN_NAME (i64_le_);
LUMEX_PROTOCOL_DETAIL_PLAIN_NAME (i64_be_);
LUMEX_PROTOCOL_DETAIL_PLAIN_NAME (f32_le_);
LUMEX_PROTOCOL_DETAIL_PLAIN_NAME (f32_be_);
LUMEX_PROTOCOL_DETAIL_PLAIN_NAME (f64_le_);
LUMEX_PROTOCOL_DETAIL_PLAIN_NAME (f64_be_);
LUMEX_PROTOCOL_DETAIL_PLAIN_TYPE (char_);
LUMEX_PROTOCOL_DETAIL_PLAIN_TYPE (token_);

#undef LUMEX_PROTOCOL_DETAIL_PLAIN_NAME
#undef LUMEX_PROTOCOL_DETAIL_PLAIN_TYPE

/// @brief One base per name: their injected class names are the names of
/// the vocabulary.
struct names : u8_<>,
               i8_<>,
               u16_le_<>,
               u16_be_<>,
               i16_le_<>,
               i16_be_<>,
               u24_le_<>,
               u24_be_<>,
               i24_le_<>,
               i24_be_<>,
               u32_le_<>,
               u32_be_<>,
               i32_le_<>,
               i32_be_<>,
               u64_le_<>,
               u64_be_<>,
               i64_le_<>,
               i64_be_<>,
               f32_le_<>,
               f32_be_<>,
               f64_le_<>,
               f64_be_<>,
               char_<>,
               token_<>
{
};
} // namespace plain

/**
 * @brief The names of a protocol table.
 * @details Fields: the integers `u8_`, `i8_`, `u16_le_` ... `i64_be_`
 * (24-bit `u24_le_`, `i24_be_`, ...), `f32_le_` ... `f64_be_`, `char_`,
 * `token_` (with or without `<>`), `sign_magnitude_<I>`, `ascii_hex_<W>`,
 * `dec_text_<I, F>`, `text_<N>`, `bits_<...>`, `mask_<...>`,
 * `split_<...>`, `enum_<S, E>`, `array_<T, N>`, `counted_<T, C, Max>`,
 * `computed_count_<T, Expr, Max>`, `count_from_<I>`, `optional_tail_<...>`,
 * `by_length_<Min, A, B>`, `union_by_<D, case_<V, L>...>`, `field_<I>`,
 * `at_end_<D>`, `union_by_context_<Key, ...>`, `nested_<P, K>`; modifiers
 * `scale<std::ratio<N, D> >` and `unit::...`. Options: `timeout (ms)`,
 * `retries (n)`, `stream (interval_ms)`, `log_level (level::...)`,
 * `firmware (from, to)`, `model ("name", ...)`, `accepted_in (state, ...)`,
 * and the flags `idempotent`, `dangerous`, `deprecated` (from `row_flags`).
 */
struct vocabulary : row_flags, plain::names
{
  /// @brief The log levels of `log_level (...)`.
  using level = ::lumex::core::protocol::dsl::level;

  /// @brief Modifier: the engineering value is the raw value times `Ratio`.
  template <typename Ratio>
  using scale = ::lumex::core::protocol::field::scale<Ratio>;

  /// @brief The units (`unit::microliter`).
  using unit = ::lumex::core::protocol::field::unit;

  /// @brief Field: an integer whose top bit is the sign.
  template <typename IntegerField>
  using sign_magnitude_
      = ::lumex::core::protocol::field::sign_magnitude_<IntegerField>;

  /// @brief Field: `Width` hexadecimal digits.
  template <std::size_t Width, typename... Modifiers>
  using ascii_hex_
      = ::lumex::core::protocol::field::ascii_hex_<Width, Modifiers...>;

  /// @brief Field: a decimal number with a fixed point.
  template <std::size_t IntDigits, std::size_t FracDigits,
            typename... Modifiers>
  using dec_text_
      = ::lumex::core::protocol::field::dec_text_<IntDigits, FracDigits,
                                                  Modifiers...>;

  /// @brief Field: a text of at most `N` characters, NUL-padded.
  template <std::size_t N>
  using text_ = ::lumex::core::protocol::field::text_<N>;

  /// @brief Field: `Count` bits of a word from bit `First`.
  template <typename Storage, std::size_t First, std::size_t Count,
            typename... T>
  using bits_
      = ::lumex::core::protocol::field::bits_<Storage, First, Count, T...>;

  /// @brief Field: the bits of a word under `Mask`, packed.
  template <typename Storage, std::uint64_t Mask, typename... T>
  using mask_ = ::lumex::core::protocol::field::mask_<Storage, Mask, T...>;

  /// @brief Field: several parts of one word.
  template <typename Word, typename... Parts>
  using split_ = ::lumex::core::protocol::field::split_<Word, Parts...>;

  /// @brief Field: an enumeration stored as another field.
  template <typename Storage, typename E>
  using enum_ = ::lumex::core::protocol::field::enum_<Storage, E>;

  /// @brief Field: `N` elements.
  template <typename T, std::size_t N>
  using array_ = ::lumex::core::protocol::field::array_<T, N>;

  /// @brief Field: a count, then that many elements (at most `Max`).
  template <typename T, typename CountField, std::size_t Max>
  using counted_
      = ::lumex::core::protocol::field::counted_<T, CountField, Max>;

  /// @brief Field: elements whose number earlier fields give.
  template <typename T, typename Expr, std::size_t Max>
  using computed_count_
      = ::lumex::core::protocol::field::computed_count_<T, Expr, Max>;

  /// @brief The count of `computed_count_` taken from field `I`.
  template <std::size_t I>
  using count_from_ = ::lumex::core::protocol::field::count_from_<I>;

  /// @brief Field: fields present when the message goes on.
  template <typename... F>
  using optional_tail_ = ::lumex::core::protocol::field::optional_tail_<F...>;

  /// @brief Field: one of two layouts chosen by the length left.
  template <std::size_t Min, typename A, typename B>
  using by_length_ = ::lumex::core::protocol::field::by_length_<Min, A, B>;

  /// @brief Field: one of several layouts chosen by a discriminant.
  template <typename Discriminant, typename... Cases>
  using union_by_
      = ::lumex::core::protocol::field::union_by_<Discriminant, Cases...>;

  /// @brief A case of a union.
  template <std::int64_t Value, typename L>
  using case_ = ::lumex::core::protocol::field::case_<Value, L>;

  /// @brief The discriminant of a union is field `I` of the group.
  template <std::size_t I>
  using field_ = ::lumex::core::protocol::field::field_<I>;

  /// @brief The discriminant of a union is at the end of the message.
  template <typename D>
  using at_end_ = ::lumex::core::protocol::field::at_end_<D>;

  /// @brief Field: one of several layouts chosen by a session value.
  template <typename Key, typename... Cases>
  using union_by_context_
      = ::lumex::core::protocol::field::union_by_context_<Key, Cases...>;

  /// @brief Field: a message of another protocol.
  template <typename Protocol, typename KeyField>
  using nested_ = ::lumex::core::protocol::field::nested_<Protocol, KeyField>;

  /// @brief Option: the reply must come within `timeout (ms)`.
  using timeout = timeout_t;

  /// @brief Option: send the request again up to `retries (n)` times.
  using retries = retries_t;

  /// @brief Option: the device sends the row by itself every
  /// `stream (interval_ms)`.
  using stream = stream_t;

  /// @brief Option: the log level of the row, `log_level (level::...)`.
  using log_level = log_level_t;

  /// @brief Option: the row exists in firmware versions `firmware (from,
  /// to)`.
  using firmware = firmware_t;

  /// @brief Option: the row exists in these models only.
  template <typename... Rest>
  static LUMEX_CONSTEXPR model_t<1 + sizeof...(Rest)>
  model (char const *first, Rest const *...rest) LUMEX_NOEXCEPT
  {
    static_assert (
        detail::vocabulary_all<std::is_same<Rest, char>::value...>::value,
        "LUMEX_PROTOCOL: model () takes string literals");
    return model_t<1 + sizeof...(Rest)>{ { first, rest... } };
  }

  /// @brief Option: the device accepts the row in these states only.
  template <typename E, typename... Rest>
  static LUMEX_CONSTEXPR accepted_in_t<E, 1 + sizeof...(Rest)>
  accepted_in (E first, Rest... rest) LUMEX_NOEXCEPT
  {
    static_assert (std::is_enum<E>::value,
                   "LUMEX_PROTOCOL: accepted_in () takes enumerators");
    static_assert (
        detail::vocabulary_all<std::is_same<Rest, E>::value...>::value,
        "LUMEX_PROTOCOL: accepted_in () takes enumerators of one "
        "enumeration");
    return accepted_in_t<E, 1 + sizeof...(Rest)>{ { first, rest... } };
  }
};

namespace detail
{
template <typename T> struct vocabulary_voider
{
  using type = void;
};
} // namespace detail

/**
 * @brief `vocabulary_of<Header>::type`: the vocabulary of the tables whose
 * protocol has header type `Header`: `Header::lumex_vocabulary` when the
 * header names one, `dsl::vocabulary` otherwise. A specialization is the
 * other way to choose it.
 */
template <typename Header, typename = void> struct vocabulary_of
{
  /// The vocabulary.
  using type = vocabulary;
};

/// @copydoc vocabulary_of
template <typename Header>
struct vocabulary_of<Header, typename detail::vocabulary_voider<
                                 typename Header::lumex_vocabulary>::type>
{
  static_assert (
      std::is_base_of<vocabulary, typename Header::lumex_vocabulary>::value,
      "LUMEX_PROTOCOL: the lumex_vocabulary of a header derives "
      "from dsl::vocabulary");
  /// The vocabulary.
  using type = typename Header::lumex_vocabulary;
};

/// @brief Whether `T` is a word of the vocabulary that is not a field (an
/// option type, `level`, `unit`).
template <typename T, typename = void>
struct is_vocabulary_word
    : std::integral_constant<
          bool,
          std::is_same<T, level>::value
              || std::is_same<T, ::lumex::core::protocol::field::unit>::value>
{
};

/// @copydoc is_vocabulary_word
template <typename T>
struct is_vocabulary_word<T, typename detail::vocabulary_voider<
                                 typename T::lumex_vocabulary_word>::type>
    : std::is_same<typename T::lumex_vocabulary_word, vocabulary_word_tag>
{
};

/// @brief `group_check<Owner, is_vocabulary_word, T...>` of the types of a
/// `type_list` (what the row and layout macros check).
template <typename Owner, typename List> struct group_check_of;

/// @copydoc group_check_of
template <typename Owner, typename... T>
struct group_check_of<Owner, type_list<T...>>
    : group_check<Owner, is_vocabulary_word, T...>
{
};
} // namespace dsl
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_DSL_VOCABULARY_HPP
