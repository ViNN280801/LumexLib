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
 * @file LumexDelimiterCodec.hpp
 * @brief `delimiter_codec`: frames that end with a delimiter, as text
 * protocols send them.
 * @details `encode` appends the payload and the transmit suffix; the decoder
 * ends a frame at the receive delimiter and delivers the bytes before it. The
 * two may differ (a device may take `XX, data<LF>` and answer with
 * `<CR><LF><ETB>`), and either may be several bytes long; a multi-byte
 * delimiter is found across calls and across a frame that is being dropped.
 * Nothing is escaped: a payload must not contain the delimiter the peer looks
 * for. An empty frame (a delimiter right after another) is skipped.
 * `max_frame_size` limits the payload; a longer frame is dropped with
 * `frame_too_large` up to the next delimiter.
 */
#ifndef LUMEX_CORE_FRAMING_DELIMITER_CODEC_HPP
#define LUMEX_CORE_FRAMING_DELIMITER_CODEC_HPP

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

#include <cstddef>
#include <vector>

#include "lumex/core/contracts/LumexContracts"
#include "lumex/core/framing/codec/LumexFramingCodec.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace framing
{
namespace delimiter
{
/// @brief The options of `delimiter_codec`.
struct delimiter_options_t : codec::codec_options_t
{
  /// The bytes that end a received frame; not empty.
  codec::bytes_t rx_delimiter = codec::bytes_t (1, '\r');
  /// The bytes `encode` appends to a payload; not empty.
  codec::bytes_t tx_suffix = codec::bytes_t (1, '\r');
};

/// @brief Delimiter framing (see the file description).
class delimiter_codec
{
public:
  using options_type = delimiter_options_t;

  /// @pre `options.max_frame_size > 0`, `options.rx_delimiter` and
  /// `options.tx_suffix` are not empty.
  explicit delimiter_codec (options_type const &options = options_type ())
      : options_ (options),
        frame_ (options.max_frame_size + options.rx_delimiter.size ()),
        fallback_ (options.rx_delimiter.size (), 0)
  {
    LUMEX_CONTRACT_ASSERT (options.max_frame_size > 0);
    LUMEX_CONTRACT_ASSERT (!options.rx_delimiter.empty ());
    LUMEX_CONTRACT_ASSERT (!options.tx_suffix.empty ());
    build_fallback ();
  }

  /// @brief Appends the payload and the transmit suffix to `out`.
  /// @return `payload_too_large` for a payload above `max_frame_size`.
  codec::encode_result_t
  encode (codec::bytes_view_t payload, codec::bytes_t &out) const
  {
    if (payload.size () > options_.max_frame_size)
      return codec::encode_failure (codec::framing_errc::payload_too_large);
    out.insert (out.end (), payload.begin (), payload.end ());
    out.insert (out.end (), options_.tx_suffix.begin (),
                options_.tx_suffix.end ());
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

  /// @brief Forgets a partial frame, a partial delimiter and the drop state.
  void
  reset () LUMEX_NOEXCEPT
  {
    frame_.clear ();
    matched_ = 0;
  }

private:
  // The prefix function of the delimiter (Knuth-Morris-Pratt), so that a
  // mismatch inside a partial delimiter keeps the longest match that remains.
  void
  build_fallback ()
  {
    codec::bytes_t const &pattern = options_.rx_delimiter;
    std::size_t length = 0;
    for (std::size_t i = 1; i < pattern.size (); ++i)
      {
        while (length > 0 && pattern[i] != pattern[length])
          length = fallback_[length - 1];
        if (pattern[i] == pattern[length])
          ++length;
        fallback_[i] = length;
      }
  }

  template <typename Sink>
  void
  take (codec::byte_t value, Sink &sink)
  {
    codec::bytes_t const &pattern = options_.rx_delimiter;
    while (matched_ > 0 && value != pattern[matched_])
      matched_ = fallback_[matched_ - 1];
    if (value == pattern[matched_])
      ++matched_;

    if (!frame_.dropping () && !frame_.push (value))
      codec::detail::deliver (sink, codec::framing_errc::frame_too_large);

    if (matched_ == pattern.size ())
      {
        matched_ = 0;
        if (!frame_.dropping ())
          {
            std::size_t const payload_size = frame_.size () - pattern.size ();
            if (payload_size > 0)
              codec::detail::deliver (
                  sink,
                  codec::bytes_view_t (frame_.view ().data (), payload_size));
          }
        frame_.clear ();
      }
  }

  options_type options_;
  codec::detail::frame_buffer frame_;
  std::vector<std::size_t> fallback_;
  std::size_t matched_ = 0;
};
} // namespace delimiter
} // namespace framing
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_FRAMING_DELIMITER_CODEC_HPP
