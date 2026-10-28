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
 * @file LumexLengthPrefixCodec.hpp
 * @brief `length_prefix_codec`: frames that start with their length.
 * @details A frame is a length field of 1, 2 or 4 bytes in either byte order
 * followed by the payload. The field holds the payload size, or the payload
 * size plus the width of the field when `length_includes_prefix` is set.
 * A zero-length payload is a frame and is delivered as an empty frame.
 *
 * `encode` refuses with `payload_too_large` a payload above `max_frame_size`
 * or one whose length the field cannot hold; it never truncates. A received
 * length above `max_frame_size` is reported as `frame_too_large` once and the
 * declared bytes are skipped, so the next frame is read from its own length
 * field. A length smaller than the field itself (with
 * `length_includes_prefix`) is `bad_header`, and reading continues at the next
 * byte as a new length field; a stream without boundaries cannot resynchronize
 * better than that.
 */
#ifndef LUMEX_CORE_FRAMING_LENGTH_PREFIX_CODEC_HPP
#define LUMEX_CORE_FRAMING_LENGTH_PREFIX_CODEC_HPP

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

#include "lumex/core/contracts/LumexContracts"
#include "lumex/core/framing/codec/LumexFramingCodec.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace framing
{
namespace length
{
/// @brief The options of `length_prefix_codec`.
struct length_prefix_options_t : codec::codec_options_t
{
  /// The width of the length field: 1, 2 or 4 bytes.
  std::size_t width = 1;
  /// The byte order of a field of 2 or 4 bytes.
  codec::byte_order order = codec::byte_order::big;
  /// The field counts its own bytes too.
  bool length_includes_prefix = false;
};

/// @brief Length-prefix framing (see the file description).
class length_prefix_codec
{
public:
  typedef length_prefix_options_t options_type;

  /// @pre `options.max_frame_size > 0` and `options.width` is 1, 2 or 4.
  explicit length_prefix_codec (options_type const &options = options_type ())
      : options_ (options), frame_ (options.max_frame_size)
  {
    LUMEX_CONTRACT_ASSERT (options.max_frame_size > 0);
    LUMEX_CONTRACT_ASSERT (options.width == 1 || options.width == 2
                           || options.width == 4);
  }

  /// @brief Appends the length field and the payload to `out`.
  /// @return `payload_too_large` for a payload above `max_frame_size` or
  /// above what the field can hold.
  codec::encode_result_t
  encode (codec::bytes_view_t payload, codec::bytes_t &out) const
  {
    std::uint64_t const declared
        = static_cast<std::uint64_t> (payload.size ())
          + (options_.length_includes_prefix ? options_.width : 0);
    if (payload.size () > options_.max_frame_size
        || declared > field_maximum ())
      return codec::encode_failure (codec::framing_errc::payload_too_large);
    codec::detail::put_uint (static_cast<std::uint32_t> (declared),
                             options_.width, options_.order, out);
    out.insert (out.end (), payload.begin (), payload.end ());
    return codec::encode_result_t ();
  }

  /**
   * @brief Consumes a chunk of the stream and delivers every frame it ends.
   * @return `chunk.size ()`: the whole chunk is always consumed.
   */
  template <typename Sink>
  std::size_t
  decode (codec::bytes_view_t chunk, Sink &&sink)
  {
    for (codec::byte_t value : chunk)
      take (value, sink);
    return chunk.size ();
  }

  /// @brief Forgets a partial length field, a partial frame and a skip.
  void
  reset () LUMEX_NOEXCEPT
  {
    prefix_size_ = 0;
    expected_ = 0;
    skip_ = 0;
    in_payload_ = false;
    frame_.clear ();
  }

private:
  std::uint64_t
  field_maximum () const LUMEX_NOEXCEPT
  {
    return options_.width >= 4
               ? 0xFFFFFFFFull
               : (std::uint64_t (1) << (8 * options_.width)) - 1;
  }

  template <typename Sink>
  void
  take (codec::byte_t value, Sink &sink)
  {
    if (skip_ > 0)
      {
        --skip_;
        return;
      }
    if (in_payload_)
      {
        frame_.push (value);
        if (frame_.size () == expected_)
          finish (sink);
        return;
      }
    prefix_[prefix_size_++] = value;
    if (prefix_size_ < options_.width)
      return;
    prefix_size_ = 0;
    std::uint64_t declared
        = codec::detail::get_uint (prefix_, options_.width, options_.order);
    if (options_.length_includes_prefix)
      {
        if (declared < options_.width)
          {
            codec::detail::deliver (sink, codec::framing_errc::bad_header);
            return;
          }
        declared -= options_.width;
      }
    if (declared > options_.max_frame_size)
      {
        codec::detail::deliver (sink, codec::framing_errc::frame_too_large);
        skip_ = static_cast<std::size_t> (declared);
        return;
      }
    expected_ = static_cast<std::size_t> (declared);
    if (expected_ == 0)
      finish (sink);
    else
      in_payload_ = true;
  }

  template <typename Sink>
  void
  finish (Sink &sink)
  {
    in_payload_ = false;
    codec::detail::deliver (sink, frame_.view ());
    frame_.clear ();
  }

  options_type options_;
  codec::detail::frame_buffer frame_;
  codec::byte_t prefix_[4] = { 0, 0, 0, 0 };
  std::size_t prefix_size_ = 0;
  std::size_t expected_ = 0;
  std::size_t skip_ = 0;
  bool in_payload_ = false;
};
} // namespace length
} // namespace framing
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_FRAMING_LENGTH_PREFIX_CODEC_HPP
