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
 * @file LumexFramingCodec.hpp
 * @brief The contract every frame codec of `lumex::core::framing` follows,
 * the byte types of the interface and the helpers the codecs share.
 * @details A codec `C` turns a payload into a frame and a byte stream back
 * into payloads. It provides:
 * - `typedef ... options_type;` with at least `max_frame_size`;
 * - `explicit C (options_type const &)`; invalid options are a contract
 *   violation (`LUMEX_CONTRACT_ASSERT`), never replaced by a default;
 * - `encode_result_t encode (bytes_view_t payload, bytes_t &out) const`:
 *   appends one frame to `out`; a payload the codec cannot carry fails with
 *   `framing_errc::payload_too_large` and leaves `out` unchanged, it is never
 *   truncated;
 * - `template <typename Sink> std::size_t decode (bytes_view_t chunk,
 *   Sink &&sink)`: consumes the chunk, keeps a partial frame for the next
 *   call and calls the sink once per complete frame. Every codec of this
 *   module consumes the whole chunk and returns `chunk.size ()`. The sink is
 *   `void (bytes_view_t frame)` or `void (bytes_view_t frame,
 *   std::error_code error)`; the second form also hears about dropped frames
 *   (an empty frame and the error), the first one does not. The frame view
 *   is valid only during the call;
 * - `void reset () noexcept`: forgets a partial frame and any drop state.
 *
 * A decoder never holds more than `max_frame_size` bytes of one frame: a
 * longer frame is dropped with `framing_errc::frame_too_large` once, and the
 * decoder resynchronizes on the next frame boundary its format defines.
 * Nothing throws except `std::bad_alloc` and an exception of the sink.
 */
#ifndef LUMEX_CORE_FRAMING_CODEC_CODEC_HPP
#define LUMEX_CORE_FRAMING_CODEC_CODEC_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#endif

#include <cstddef>
#include <cstdint>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#include "lumex/core/expected/Expected"
#include "lumex/core/framing/codec/LumexFramingErrc.hpp"
#include "lumex/core/span/LumexSpan"
#include "lumex/core/utility/bit/LumexByteOrder.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace framing
{
namespace codec
{
/// @brief One byte on the wire.
typedef std::uint8_t byte_t;

/// @brief A read-only view of bytes: a payload, a chunk or a frame.
typedef lumex::core::span::view::span<byte_t const> bytes_view_t;

/// @brief An owning byte buffer: where `encode` appends a frame.
typedef std::vector<byte_t> bytes_t;

/// @brief The result of `encode`: nothing, or why the payload was refused.
typedef lumex::core::expected::result::expected<void, std::error_code>
    encode_result_t;

namespace Constants
{
/// @brief The default limit of one frame: 64 KiB.
LUMEX_CONSTEXPR std::size_t KDEFAULT_MAX_FRAME_SIZE = 65536;
} // namespace Constants

/// @brief The order of the bytes of a multi-byte field on the wire (the
/// type of `lumex/core/utility/bit/LumexByteOrder.hpp`, shared with the
/// protocol fields): `byte_order::big`, `byte_order::little`.
typedef ::lumex::core::utility::bit::byte_order byte_order;

namespace detail
{
/// @brief Appends the low `width` bytes (1..8) of `value` to `out` in
/// `order`.
inline void
put_uint (std::uint64_t value, std::size_t width, byte_order order,
          bytes_t &out)
{
  for (std::size_t i = 0; i < width; ++i)
    {
      std::size_t const shift
          = 8 * (order == byte_order::big ? width - 1 - i : i);
      out.push_back (static_cast<byte_t> ((value >> shift) & 0xFFu));
    }
}

/// @brief Reads `width` bytes (1..8) from `in` in `order`.
inline std::uint64_t
get_uint (byte_t const *in, std::size_t width, byte_order order) LUMEX_NOEXCEPT
{
  std::uint64_t value = 0;
  for (std::size_t i = 0; i < width; ++i)
    {
      std::size_t const shift
          = 8 * (order == byte_order::big ? width - 1 - i : i);
      value |= static_cast<std::uint64_t> (in[i]) << shift;
    }
  return value;
}

/// @brief The upper-case hex digit of the low four bits of `value`.
inline byte_t
hex_digit (unsigned value) LUMEX_NOEXCEPT
{
  value &= 0x0Fu;
  return static_cast<byte_t> (value < 10 ? '0' + value : 'A' + value - 10);
}

/// @brief The value of a hex digit of either case; false for another byte.
inline bool
hex_value (byte_t digit, unsigned &value) LUMEX_NOEXCEPT
{
  if (digit >= '0' && digit <= '9')
    value = static_cast<unsigned> (digit - '0');
  else if (digit >= 'A' && digit <= 'F')
    value = static_cast<unsigned> (digit - 'A' + 10);
  else if (digit >= 'a' && digit <= 'f')
    value = static_cast<unsigned> (digit - 'a' + 10);
  else
    return false;
  return true;
}

/// @brief Calls an error-form sink, or nothing for a frame-only sink, with
/// any error code (a stage of a pipeline reports through it).
template <typename Sink>
void
deliver_code (Sink &sink, std::error_code error, std::true_type)
{
  sink (bytes_view_t (), error);
}

template <typename Sink>
void
deliver_code (Sink &, std::error_code, std::false_type)
{
}
} // namespace detail

/// @brief The options every codec has.
struct codec_options_t
{
  /// The largest frame a decoder keeps; also the largest payload `encode`
  /// accepts where the format carries the payload unchanged. Must not be 0.
  std::size_t max_frame_size = Constants::KDEFAULT_MAX_FRAME_SIZE;
};

/// @brief The failed result of `encode` with the given error.
inline encode_result_t
encode_failure (framing_errc error)
{
  return encode_result_t (lumex::core::expected::result::unexpect,
                          make_error_code (error));
}

namespace detail
{
/// True when `Sink` can be called with a frame and an error code.
template <typename Sink> struct sink_takes_error
{
private:
  template <typename S>
  static auto test (int)
      -> decltype (std::declval<S &> () (std::declval<bytes_view_t> (),
                                         std::declval<std::error_code> ()),
                   std::true_type ());
  template <typename S> static std::false_type test (...);

public:
  static bool const value = decltype (test<Sink> (0))::value;
};

template <typename Sink>
void
deliver_frame (Sink &sink, bytes_view_t frame, std::true_type)
{
  sink (frame, std::error_code ());
}

template <typename Sink>
void
deliver_frame (Sink &sink, bytes_view_t frame, std::false_type)
{
  sink (frame);
}

template <typename Sink>
void
deliver_error (Sink &sink, framing_errc error, std::true_type)
{
  sink (bytes_view_t (), make_error_code (error));
}

template <typename Sink>
void
deliver_error (Sink &, framing_errc, std::false_type)
{
}

/// @brief Calls the sink with a complete frame, in either sink form.
template <typename Sink>
void
deliver (Sink &sink, bytes_view_t frame)
{
  deliver_frame (
      sink, frame,
      std::integral_constant<bool, sink_takes_error<Sink>::value> ());
}

/// @brief Tells a sink of the error form that a frame was dropped; a sink of
/// the frame-only form is not called.
template <typename Sink>
void
deliver (Sink &sink, framing_errc error)
{
  deliver_error (
      sink, error,
      std::integral_constant<bool, sink_takes_error<Sink>::value> ());
}

/// @brief The same for an error code of any category.
template <typename Sink>
void
deliver (Sink &sink, std::error_code error)
{
  deliver_code (
      sink, error,
      std::integral_constant<bool, sink_takes_error<Sink>::value> ());
}

/**
 * @brief The bytes of the frame being received, bounded by `max_frame_size`.
 * @details `push` refuses a byte past the limit and puts the buffer into the
 * dropping state, in which it takes nothing until `clear`. A codec reports
 * `frame_too_large` once at that moment and clears the buffer when it sees
 * the next frame boundary.
 */
class frame_buffer
{
public:
  explicit frame_buffer (std::size_t max_size) : max_size_ (max_size) {}

  /// @brief Appends a byte; false (and the dropping state) past the limit.
  bool
  push (byte_t value)
  {
    if (dropping_)
      return false;
    if (bytes_.size () >= max_size_)
      {
        dropping_ = true;
        bytes_.clear ();
        return false;
      }
    bytes_.push_back (value);
    return true;
  }

  /// @brief True after `push` refused a byte, until `clear`.
  bool
  dropping () const LUMEX_NOEXCEPT
  {
    return dropping_;
  }

  /// @brief Forgets the bytes and leaves the dropping state.
  void
  clear () LUMEX_NOEXCEPT
  {
    bytes_.clear ();
    dropping_ = false;
  }

  std::size_t
  size () const LUMEX_NOEXCEPT
  {
    return bytes_.size ();
  }

  bool
  empty () const LUMEX_NOEXCEPT
  {
    return bytes_.empty ();
  }

  std::size_t
  max_size () const LUMEX_NOEXCEPT
  {
    return max_size_;
  }

  /// @brief The bytes received so far; valid until the next change.
  bytes_view_t
  view () const LUMEX_NOEXCEPT
  {
    return bytes_view_t (bytes_.data (), bytes_.size ());
  }

  /// @brief The underlying storage, for a codec that edits the frame.
  bytes_t &
  storage () LUMEX_NOEXCEPT
  {
    return bytes_;
  }

private:
  bytes_t bytes_;
  std::size_t max_size_;
  bool dropping_ = false;
};
} // namespace detail
} // namespace codec
} // namespace framing
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_FRAMING_CODEC_CODEC_HPP
