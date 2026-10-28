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
 * @file LumexFramingStages.hpp
 * @brief The stages of `pipeline`: transformations of a complete frame in
 * both directions.
 * @details A stage `S` provides
 * - `codec::encode_result_t encode (bytes_view_t in, bytes_t &out) const`:
 *   writes the transformed frame to `out` (which is empty on entry);
 * - `std::error_code decode (bytes_view_t in, bytes_t &out) const`: writes
 *   the frame with the transformation undone to `out`, or returns why it
 *   cannot (the frame is then dropped).
 *
 * The stages here are a fixed prefix byte (`prefix_char_stage`), ASCII hex
 * (`ascii_hex_stage`), a check value (`checksum_stage` over `xor8`, `sum8` or
 * any CRC of `core/crc`, also over word-reversed data with `word_reversed`,
 * written in binary or as ASCII hex, required or optional) and the reversal
 * of every word of N bytes (`word_reverse_stage`).
 */
#ifndef LUMEX_CORE_FRAMING_STAGE_STAGES_HPP
#define LUMEX_CORE_FRAMING_STAGE_STAGES_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#if __has_warning("-Wlifetime-safety-invalidation")
// Clang 23 reports a reference parameter to a container as "invalidated" by
// its own push_back, insert or reserve (a false positive: only references into
// the container are invalidated, not the container).
#pragma clang diagnostic ignored "-Wlifetime-safety-invalidation"
#endif
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#endif

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <system_error>

#include "lumex/core/crc/LumexCrc"
#include "lumex/core/framing/codec/LumexFramingCodec.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace framing
{
namespace stage
{
/// @brief A fixed first byte: added by `encode`, required and removed by
/// `decode` (`bad_header` when it differs or the frame is empty).
template <codec::byte_t Prefix> struct prefix_char_stage
{
  codec::encode_result_t
  encode (codec::bytes_view_t in, codec::bytes_t &out) const
  {
    out.push_back (Prefix);
    out.insert (out.end (), in.begin (), in.end ());
    return codec::encode_result_t ();
  }

  std::error_code
  decode (codec::bytes_view_t in, codec::bytes_t &out) const
  {
    if (in.empty () || in[0] != Prefix)
      return codec::framing_errc::bad_header;
    out.assign (in.begin () + 1, in.end ());
    return std::error_code ();
  }
};

/// @brief Every byte as two hex digits: upper case out, either case in. An
/// odd length is `truncated`, a byte that is no hex digit `bad_escape`.
struct ascii_hex_stage
{
  codec::encode_result_t
  encode (codec::bytes_view_t in, codec::bytes_t &out) const
  {
    out.reserve (out.size () + 2 * in.size ());
    for (codec::byte_t value : in)
      {
        out.push_back (
            codec::detail::hex_digit (static_cast<unsigned> (value) >> 4u));
        out.push_back (
            codec::detail::hex_digit (static_cast<unsigned> (value)));
      }
    return codec::encode_result_t ();
  }

  std::error_code
  decode (codec::bytes_view_t in, codec::bytes_t &out) const
  {
    if (in.size () % 2 != 0)
      return codec::framing_errc::truncated;
    out.reserve (in.size () / 2);
    for (std::size_t i = 0; i < in.size (); i += 2)
      {
        unsigned high = 0;
        unsigned low = 0;
        if (!codec::detail::hex_value (in[i], high)
            || !codec::detail::hex_value (in[i + 1], low))
          return codec::framing_errc::bad_escape;
        out.push_back (static_cast<codec::byte_t> ((high << 4u) | low));
      }
    return std::error_code ();
  }
};

// --- Check value algorithms -------------------------------------------------

/// @brief XOR of all bytes, one byte.
struct xor8
{
  static std::size_t
  width () LUMEX_NOEXCEPT
  {
    return 1;
  }

  static codec::byte_order
  order () LUMEX_NOEXCEPT
  {
    return codec::byte_order::big;
  }

  static std::uint64_t
  compute (codec::bytes_view_t data) LUMEX_NOEXCEPT
  {
    codec::byte_t value = 0;
    for (codec::byte_t byte : data)
      value = static_cast<codec::byte_t> (value ^ byte);
    return value;
  }
};

/// @brief Sum of all bytes modulo 256, one byte.
struct sum8
{
  static std::size_t
  width () LUMEX_NOEXCEPT
  {
    return 1;
  }

  static codec::byte_order
  order () LUMEX_NOEXCEPT
  {
    return codec::byte_order::big;
  }

  static std::uint64_t
  compute (codec::bytes_view_t data) LUMEX_NOEXCEPT
  {
    unsigned value = 0;
    for (codec::byte_t byte : data)
      value = (value + byte) & 0xFFu;
    return value;
  }
};

/**
 * @brief A CRC of `core/crc` (`Spec` is one of its spec types, for example
 * `crc16_modbus_spec_t`), written in `Order`: `(kWidth + 7) / 8` bytes.
 */
template <typename Spec, codec::byte_order Order = codec::byte_order::big>
struct crc
{
  static std::size_t
  width () LUMEX_NOEXCEPT
  {
    return static_cast<std::size_t> ((Spec::kWidth + 7) / 8);
  }

  static codec::byte_order
  order () LUMEX_NOEXCEPT
  {
    return Order;
  }

  static std::uint64_t
  compute (codec::bytes_view_t data) LUMEX_NOEXCEPT
  {
    return static_cast<std::uint64_t> (
        lumex::core::crc::parametric::crc_parametric<Spec>::calculate (
            lumex::core::span::view::span<std::uint8_t const> (data.data (),
                                                               data.size ())));
  }
};

/**
 * @brief `Algorithm` over the data with the bytes of every word of `N` bytes
 * reversed (a trailing partial word is reversed as it is). A device may
 * compute its CRC-32/MPEG-2 this way, the way a microcontroller CRC unit
 * reads little-endian 32-bit words: `word_reversed<crc<crc32_mpeg2_spec_t,
 * byte_order::little>, 4>`.
 */
template <typename Algorithm, std::size_t N> struct word_reversed
{
  static_assert (N > 1, "a word of one byte has nothing to reverse");

  static std::size_t
  width () LUMEX_NOEXCEPT
  {
    return Algorithm::width ();
  }

  static codec::byte_order
  order () LUMEX_NOEXCEPT
  {
    return Algorithm::order ();
  }

  static std::uint64_t
  compute (codec::bytes_view_t data)
  {
    codec::bytes_t reversed (data.begin (), data.end ());
    for (std::size_t at = 0; at < reversed.size (); at += N)
      {
        std::size_t const end = std::min (at + N, reversed.size ());
        std::reverse (reversed.begin () + static_cast<std::ptrdiff_t> (at),
                      reversed.begin () + static_cast<std::ptrdiff_t> (end));
      }
    return Algorithm::compute (
        codec::bytes_view_t (reversed.data (), reversed.size ()));
  }
};

// --- How the check value is written -----------------------------------------

/// @brief The check value as `width` bytes in the algorithm's byte order.
struct binary_render
{
  static std::size_t
  size (std::size_t width) LUMEX_NOEXCEPT
  {
    return width;
  }

  static void
  write (std::uint64_t value, std::size_t width, codec::byte_order order,
         codec::bytes_t &out)
  {
    codec::detail::put_uint (value, width, order, out);
  }

  static bool
  read (codec::bytes_view_t in, std::size_t width, codec::byte_order order,
        std::uint64_t &value) LUMEX_NOEXCEPT
  {
    value = codec::detail::get_uint (in.data (), width, order);
    return true;
  }
};

/// @brief The check value as `2 * width` upper-case hex digits, the most
/// significant first (either case is read).
struct ascii_hex_render
{
  static std::size_t
  size (std::size_t width) LUMEX_NOEXCEPT
  {
    return 2 * width;
  }

  static void
  write (std::uint64_t value, std::size_t width, codec::byte_order,
         codec::bytes_t &out)
  {
    for (std::size_t i = 2 * width; i > 0; --i)
      out.push_back (codec::detail::hex_digit (
          static_cast<unsigned> (value >> (4 * (i - 1)))));
  }

  static bool
  read (codec::bytes_view_t in, std::size_t width, codec::byte_order,
        std::uint64_t &value) LUMEX_NOEXCEPT
  {
    value = 0;
    for (std::size_t i = 0; i < 2 * width; ++i)
      {
        unsigned digit = 0;
        if (!codec::detail::hex_value (in[i], digit))
          return false;
        value = (value << 4u) | digit;
      }
    return true;
  }
};

// --- Whether the check value must be there ----------------------------------

/// @brief A frame without a valid check value is dropped: `bad_checksum`,
/// or `truncated` when it is shorter than the check value.
struct checksum_required
{
  static bool
  optional () LUMEX_NOEXCEPT
  {
    return false;
  }
};

/**
 * @brief A frame whose last bytes are not a valid check value is passed on
 * whole, as a frame sent without one. A device that answers sometimes with
 * and sometimes without the check value needs it; a damaged frame then passes
 * too, so use it only for such a device.
 */
struct checksum_optional
{
  static bool
  optional () LUMEX_NOEXCEPT
  {
    return true;
  }
};

/// @brief Appends the check value of the frame (`encode`) and checks and
/// removes it (`decode`).
template <typename Algorithm, typename Render = binary_render,
          typename Policy = checksum_required>
struct checksum_stage
{
  codec::encode_result_t
  encode (codec::bytes_view_t in, codec::bytes_t &out) const
  {
    out.insert (out.end (), in.begin (), in.end ());
    Render::write (Algorithm::compute (in), Algorithm::width (),
                   Algorithm::order (), out);
    return codec::encode_result_t ();
  }

  std::error_code
  decode (codec::bytes_view_t in, codec::bytes_t &out) const
  {
    std::size_t const check_size = Render::size (Algorithm::width ());
    if (in.size () >= check_size)
      {
        std::size_t const data_size = in.size () - check_size;
        codec::bytes_view_t const data (in.data (), data_size);
        std::uint64_t stated = 0;
        if (Render::read (
                codec::bytes_view_t (in.data () + data_size, check_size),
                Algorithm::width (), Algorithm::order (), stated)
            && stated == Algorithm::compute (data))
          {
            out.assign (data.begin (), data.end ());
            return std::error_code ();
          }
      }
    if (Policy::optional ())
      {
        out.assign (in.begin (), in.end ());
        return std::error_code ();
      }
    if (in.size () < check_size)
      return codec::framing_errc::truncated;
    return codec::framing_errc::bad_checksum;
  }
};

/// @brief Reverses the bytes of every word of `N` bytes, in both directions;
/// a frame that is not a whole number of words is `truncated`.
template <std::size_t N> struct word_reverse_stage
{
  static_assert (N > 1, "a word of one byte has nothing to reverse");

  codec::encode_result_t
  encode (codec::bytes_view_t in, codec::bytes_t &out) const
  {
    if (in.size () % N != 0)
      return codec::encode_failure (codec::framing_errc::truncated);
    reverse_words (in, out);
    return codec::encode_result_t ();
  }

  std::error_code
  decode (codec::bytes_view_t in, codec::bytes_t &out) const
  {
    if (in.size () % N != 0)
      return codec::framing_errc::truncated;
    reverse_words (in, out);
    return std::error_code ();
  }

private:
  static void
  reverse_words (codec::bytes_view_t in, codec::bytes_t &out)
  {
    out.assign (in.begin (), in.end ());
    for (std::size_t at = 0; at < out.size (); at += N)
      std::reverse (out.begin () + static_cast<std::ptrdiff_t> (at),
                    out.begin () + static_cast<std::ptrdiff_t> (at + N));
  }
};
} // namespace stage
} // namespace framing
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_FRAMING_STAGE_STAGES_HPP
