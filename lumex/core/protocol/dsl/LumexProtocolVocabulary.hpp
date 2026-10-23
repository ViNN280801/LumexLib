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
 * namespace (field types, modifiers, row options, flags, `level`).
 * @details Every group of a row (key, fields, reply, options) and of a
 * layout is evaluated inside a class derived from `vocabulary`, so class
 * scope lookup finds `u16_le_`, `scale`, `timeout`, `dangerous` or
 * `level::debug` first, then the names of the user's namespace (layouts,
 * state enumerations). The table needs no `using namespace` and the names do
 * not leak into the user's namespace or into the command types. A field type
 * of the protocol layer becomes usable in tables by adding its alias here.
 */
#ifndef LUMEX_CORE_PROTOCOL_DSL_VOCABULARY_HPP
#define LUMEX_CORE_PROTOCOL_DSL_VOCABULARY_HPP

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "lumex/core/protocol/dsl/LumexProtocolOptions.hpp"
#include "lumex/core/protocol/field/LumexProtocolIntegerFields.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

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

/**
 * @brief The names of a protocol table.
 * @details Fields: `u8_<>`, `u16_le_<>`, `u16_be_<>` with the modifier
 * `scale<std::ratio<N, D> >`. Options: `timeout (ms)`, `retries (n)`,
 * `stream (interval_ms)`, `log_level (level::...)`, `firmware (from, to)`,
 * `model ("name", ...)`, `accepted_in (state, ...)`, and the flags
 * `idempotent`, `dangerous`, `deprecated` (from `row_flags`).
 */
struct vocabulary : row_flags
{
  /// @brief The log levels of `log_level (...)`.
  typedef ::lumex::core::protocol::dsl::level level;

  /// @brief Field: one unsigned byte.
  template <typename... Modifiers>
  using u8_ = ::lumex::core::protocol::field::u8_<Modifiers...>;

  /// @brief Field: two bytes, little endian.
  template <typename... Modifiers>
  using u16_le_ = ::lumex::core::protocol::field::u16_le_<Modifiers...>;

  /// @brief Field: two bytes, big endian.
  template <typename... Modifiers>
  using u16_be_ = ::lumex::core::protocol::field::u16_be_<Modifiers...>;

  /// @brief Modifier: the engineering value is the raw value times `Ratio`.
  template <typename Ratio>
  using scale = ::lumex::core::protocol::field::scale<Ratio>;

  /// @brief Option: the reply must come within `milliseconds`.
  static LUMEX_CONSTEXPR timeout_t
  timeout (std::uint32_t milliseconds) LUMEX_NOEXCEPT
  {
    return timeout_t{ milliseconds };
  }

  /// @brief Option: send the request again up to `count` times.
  static LUMEX_CONSTEXPR retries_t
  retries (std::uint32_t count) LUMEX_NOEXCEPT
  {
    return retries_t{ count };
  }

  /// @brief Option: the device sends the row by itself every `interval_ms`.
  static LUMEX_CONSTEXPR stream_t
  stream (std::uint32_t interval_ms) LUMEX_NOEXCEPT
  {
    return stream_t{ interval_ms };
  }

  /// @brief Option: the log level of the row.
  static LUMEX_CONSTEXPR log_level_t
  log_level (level value) LUMEX_NOEXCEPT
  {
    return log_level_t{ value };
  }

  /// @brief Option: the row exists in firmware versions `from` to `to`.
  static LUMEX_CONSTEXPR firmware_t
  firmware (std::uint32_t from, std::uint32_t to) LUMEX_NOEXCEPT
  {
    return firmware_t{ from, to };
  }

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
} // namespace dsl
} // namespace protocol
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_PROTOCOL_DSL_VOCABULARY_HPP
