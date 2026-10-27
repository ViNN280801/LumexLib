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
 * @file LumexProtocolBinaryLayout.hpp
 * @brief `binary_layout<Alignment>`: fields one after another as bytes, and
 * the alignment policies `unaligned` and `natural_alignment<Offset>`.
 * @details An integer is its bytes in the byte order of the field, a
 * floating field its IEEE 754 bytes, a text field its characters. The
 * default layout checks nothing about positions; `binary_layout<
 * natural_alignment<Offset> >` (a protocol family whose 2- and 4-byte fields
 * are aligned) refuses at compile time a row or a reply whose 2-byte field
 * is not at an even offset or whose 4- or 8-byte field is not at a multiple
 * of 4, counted from `Offset` bytes before the first field (the header).
 * The offsets are known up to the first field of variable size; the fields
 * after it are not checked.
 */
#ifndef LUMEX_CORE_PROTOCOL_LAYOUT_BINARY_LAYOUT_HPP
#define LUMEX_CORE_PROTOCOL_LAYOUT_BINARY_LAYOUT_HPP

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "lumex/core/protocol/error/LumexProtocolErrc.hpp"
#include "lumex/core/protocol/field/LumexProtocolCompositeFields.hpp"
#include "lumex/core/protocol/field/LumexProtocolFieldTraits.hpp"
#include "lumex/core/protocol/field/LumexProtocolRecord.hpp"
#include "lumex/core/protocol/layout/LumexProtocolLayoutBase.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

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
namespace layout
{
namespace detail
{
// Whether field implementation Impl at Offset (variable_size: unknown) is
// aligned to its binary_alignment, its parts included.
template <std::size_t Offset, typename Impl> struct aligned;

template <std::size_t Offset, typename... F> struct aligned_fields;

template <std::size_t Offset> struct aligned_fields<Offset> : std::true_type
{
};

template <std::size_t Offset, typename Head, typename... Tail>
struct aligned_fields<Offset, Head, Tail...>
    : std::integral_constant<
          bool,
          aligned<Offset, typename ::lumex::core::protocol::field::impl_of<
                              Head>::type>::value
              && aligned_fields<
                  (Offset == ::lumex::core::protocol::field::variable_size
                   || ::lumex::core::protocol::field::impl_of<
                          Head>::type::binary_size::value
                          == ::lumex::core::protocol::field::variable_size)
                      ? ::lumex::core::protocol::field::variable_size
                      : Offset
                            + ::lumex::core::protocol::field::impl_of<
                                Head>::type::binary_size::value,
                  Tail...>::value>
{
};

template <std::size_t Offset, typename Impl>
struct aligned
    : std::integral_constant<
          bool, Offset == ::lumex::core::protocol::field::variable_size
                    || Offset % Impl::binary_alignment::value == 0>
{
};

template <std::size_t Offset, template <typename...> class List, typename... F>
struct aligned<Offset, ::lumex::core::protocol::field::group_impl<List<F...>>>
    : aligned_fields<Offset, F...>
{
};

template <std::size_t Offset, typename T, std::size_t N>
struct aligned<Offset, ::lumex::core::protocol::field::array_<T, N>>
    : std::integral_constant<
          bool,
          aligned<Offset, typename ::lumex::core::protocol::field::array_<
                              T, N>::element_impl>::value
              && (::lumex::core::protocol::field::array_<
                      T, N>::element_impl::binary_size::value
                      == ::lumex::core::protocol::field::variable_size
                  || ::lumex::core::protocol::field::array_<
                             T, N>::element_impl::binary_size::value
                             % ::lumex::core::protocol::field::array_<
                                 T, N>::element_impl::binary_alignment::value
                         == 0)>
{
};
} // namespace detail

/// @brief Alignment policy of `binary_layout`: no check (the default).
struct unaligned
{
  /// @brief Every field is fine.
  template <typename Impl> struct check : std::true_type
  {
  };
};

/**
 * @brief Alignment policy of `binary_layout`: 2-byte fields at even offsets,
 * 4- and 8-byte fields at multiples of 4, the first field `Offset` bytes
 * after the start of the message (the size of the header).
 */
template <std::size_t Offset = 0> struct natural_alignment
{
  /// @brief Whether field implementation `Impl` (a row's fields, a reply)
  /// is aligned.
  template <typename Impl> struct check : detail::aligned<Offset, Impl>
  {
  };
};

/**
 * @brief The binary layout: fields one after another as bytes (see the file
 * description).
 * @tparam Alignment `unaligned` (default) or `natural_alignment<Offset>`.
 */
template <typename Alignment = unaligned> struct binary_layout
{
  /// The alignment policy.
  typedef Alignment alignment_policy;

  /// @brief Whether field implementation `Impl` satisfies the policy.
  template <typename Impl> struct is_aligned : Alignment::template check<Impl>
  {
  };

  /// @brief Reads the fields of one message.
  class reader : public detail::reader_base
  {
  public:
    /// The bits of an integer are on the wire as they are.
    typedef std::true_type preserves_bits;
    /// Not a tokenized text.
    typedef std::false_type is_tokenized;
    /// Floating fields are their IEEE 754 bytes.
    typedef std::true_type supports_floating;

    /// @brief Reads `input`; `context` gives the session values.
    explicit reader (bytes_view_t input,
                     decode_context_t const *context = nullptr) LUMEX_NOEXCEPT
        : detail::reader_base (input, context)
    {
    }

    /// @brief The bytes left.
    std::size_t
    remaining_units () const LUMEX_NOEXCEPT
    {
      return remaining_bytes ();
    }

    /// @brief An integer of `bytes` bytes in `order`.
    bool
    read_integer (std::size_t bytes, byte_order order, bool,
                  std::uint64_t &bits) LUMEX_NOEXCEPT
    {
      std::uint8_t const *data = nullptr;
      if (!take (bytes, data))
        return false;
      bits = detail::load_bits (data, bytes, order);
      return true;
    }

    /// @brief The IEEE 754 bytes of a float or a double.
    bool
    read_floating (std::size_t bytes, byte_order order,
                   std::uint64_t &bits) LUMEX_NOEXCEPT
    {
      return read_integer (bytes, order, false, bits);
    }

    /// @brief `width` hexadecimal digits.
    bool
    read_hex_text (std::size_t width, std::uint64_t &value) LUMEX_NOEXCEPT
    {
      std::uint8_t const *data = nullptr;
      if (!take (width, data))
        return false;
      if (!detail::parse_hex (as_chars (data), width, value))
        return fail (
            ::lumex::core::protocol::error::protocol_errc::invalid_value);
      return true;
    }

    /// @brief A decimal number of a fixed width.
    bool
    read_decimal_text (std::size_t int_digits, std::size_t frac_digits,
                       std::int64_t &scaled) LUMEX_NOEXCEPT
    {
      std::size_t const width
          = int_digits + (frac_digits == 0 ? 0u : 1u + frac_digits);
      std::uint8_t const *data = nullptr;
      if (!take (width, data))
        return false;
      if (!detail::parse_fixed (as_chars (data), width, frac_digits, scaled)
          || !detail::fits_fixed (scaled, int_digits, frac_digits))
        return fail (
            ::lumex::core::protocol::error::protocol_errc::invalid_value);
      return true;
    }

    /// @brief One character.
    bool
    read_char (char &value) LUMEX_NOEXCEPT
    {
      std::uint8_t const *data = nullptr;
      if (!take (1, data))
        return false;
      value = static_cast<char> (data[0]);
      return true;
    }

    /// @brief `capacity` characters.
    bool
    read_text (std::size_t capacity, char const *&data,
               std::size_t &size) LUMEX_NOEXCEPT
    {
      std::uint8_t const *bytes = nullptr;
      if (!take (capacity, bytes))
        return false;
      data = as_chars (bytes);
      size = capacity;
      return true;
    }

    /// @brief The rest of the message as characters.
    bool
    read_token (char const *&data, std::size_t &size) LUMEX_NOEXCEPT
    {
      std::uint8_t const *bytes = nullptr;
      take_rest (bytes, size);
      data = as_chars (bytes);
      return true;
    }

    /// @brief The rest of the message.
    bool
    read_rest (std::uint8_t const *&data, std::size_t &size) LUMEX_NOEXCEPT
    {
      take_rest (data, size);
      return true;
    }

    /// @brief An integer of `bytes` bytes at the end of the message.
    bool
    read_last_integer (std::size_t bytes, byte_order order, bool,
                       std::uint64_t &bits) LUMEX_NOEXCEPT
    {
      std::uint8_t const *data = nullptr;
      if (!take_last (bytes, data))
        return false;
      bits = detail::load_bits (data, bytes, order);
      return true;
    }

    /// @brief Whether the whole message was read; `trailing_bytes` if not.
    bool
    finish () LUMEX_NOEXCEPT
    {
      return remaining_bytes () == 0
                 ? true
                 : fail (::lumex::core::protocol::error::protocol_errc::
                             trailing_bytes);
    }
  };

  /// @brief Writes the fields of one message.
  class writer : public detail::writer_base
  {
  public:
    /// The bits of an integer are on the wire as they are.
    typedef std::true_type preserves_bits;
    /// Not a tokenized text.
    typedef std::false_type is_tokenized;
    /// Floating fields are their IEEE 754 bytes.
    typedef std::true_type supports_floating;

    /// @brief Appends to `output`.
    explicit writer (bytes_t &output,
                     decode_context_t const *context = nullptr) LUMEX_NOEXCEPT
        : detail::writer_base (output, context)
    {
    }

    /// @brief An integer of `bytes` bytes in `order`.
    bool
    write_integer (std::size_t bytes, byte_order order, bool,
                   std::uint64_t bits)
    {
      std::uint8_t data[8] = {};
      detail::store_bits (bits, bytes, order, data);
      put_bytes (data, bytes);
      return true;
    }

    /// @brief The IEEE 754 bytes of a float or a double.
    bool
    write_floating (std::size_t bytes, byte_order order, std::uint64_t bits)
    {
      return write_integer (bytes, order, false, bits);
    }

    /// @brief `width` hexadecimal digits.
    bool
    write_hex_text (std::size_t width, std::uint64_t value)
    {
      char digits[16] = {};
      if (width > sizeof (digits)
          || !detail::format_hex (value, width, digits))
        return fail (
            ::lumex::core::protocol::error::protocol_errc::invalid_value);
      put (digits, width);
      return true;
    }

    /// @brief A decimal number of a fixed width.
    bool
    write_decimal_text (std::size_t int_digits, std::size_t frac_digits,
                        std::int64_t scaled)
    {
      char text[40] = {};
      if (int_digits + frac_digits > 18
          || !detail::format_fixed (scaled, int_digits, frac_digits, text))
        return fail (
            ::lumex::core::protocol::error::protocol_errc::invalid_value);
      put (text, int_digits + (frac_digits == 0 ? 0u : 1u + frac_digits));
      return true;
    }

    /// @brief One character.
    bool
    write_char (char value)
    {
      put (value);
      return true;
    }

    /// @brief `size` characters padded with NUL to `capacity`.
    bool
    write_text (std::size_t capacity, char const *data, std::size_t size)
    {
      if (size > capacity)
        return fail (
            ::lumex::core::protocol::error::protocol_errc::invalid_value);
      put (data, size);
      for (std::size_t i = size; i < capacity; ++i)
        put ('\0');
      return true;
    }

    /// @brief The characters of a token (the rest of the message).
    bool
    write_token (char const *data, std::size_t size)
    {
      put (data, size);
      return true;
    }

    /// @brief Bytes as they are (the rest of the message).
    bool
    write_rest (std::uint8_t const *data, std::size_t size)
    {
      put_bytes (data, size);
      return true;
    }
  };

  /// @brief The binary layout has no key prefix (the header carries the
  /// key).
  template <typename Row>
  static bool
  read_prefix (reader &) LUMEX_NOEXCEPT
  {
    return true;
  }

  /// @copydoc read_prefix
  template <typename Row>
  static bool
  write_prefix (writer &) LUMEX_NOEXCEPT
  {
    return true;
  }
};
} // namespace layout
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_LAYOUT_BINARY_LAYOUT_HPP
