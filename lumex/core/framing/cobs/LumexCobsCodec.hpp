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
 * @file LumexCobsCodec.hpp
 * @brief `cobs_codec`: Consistent Overhead Byte Stuffing (Cheshire and
 * Baker, 1999) with a settable delimiter.
 * @details A frame is the COBS encoding of the payload followed by the
 * delimiter. COBS removes every zero byte: the payload is cut at its zeros
 * into groups, and each group is sent as a code byte (its length + 1) and
 * its non-zero bytes; a group of 254 non-zero bytes has the code 0xFF and no
 * zero after it. The overhead is one byte per 254 bytes, at least one. With a
 * delimiter other than 0x00 every encoded byte is XORed with it, so the
 * delimiter never occurs inside a frame.
 *
 * An empty payload is the frame `01` + delimiter and is delivered as an empty
 * frame; a delimiter alone (an empty frame) is line noise and is skipped. A
 * code byte that points past the end of the frame is `framing_errc::truncated`
 * and drops the frame. `max_frame_size` limits the decoded payload; a longer
 * frame is dropped with `frame_too_large` up to the next delimiter.
 */
#ifndef LUMEX_CORE_FRAMING_COBS_CODEC_HPP
#define LUMEX_CORE_FRAMING_COBS_CODEC_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#endif

#include <cstddef>

#include "lumex/core/contracts/LumexContracts"
#include "lumex/core/framing/codec/LumexFramingCodec.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace framing
{
namespace cobs
{
namespace Constants
{
/// @brief The code of a full group: 254 non-zero bytes, no zero after it.
LUMEX_CONSTEXPR codec::byte_t KFULL_GROUP_CODE = 0xFF;
/// @brief The number of data bytes of a full group.
LUMEX_CONSTEXPR std::size_t KFULL_GROUP_SIZE = 254;
} // namespace Constants

/// @brief The options of `cobs_codec`.
struct cobs_options_t : codec::codec_options_t
{
  /// The byte that ends a frame; the encoded bytes are XORed with it.
  codec::byte_t delimiter = 0x00;
};

/// @brief The largest encoded size of a payload of `payload_size` bytes,
/// without the delimiter.
inline std::size_t
cobs_encoded_bound (std::size_t payload_size) LUMEX_NOEXCEPT
{
  return payload_size + payload_size / Constants::KFULL_GROUP_SIZE + 1;
}

/// @brief COBS framing (see the file description).
class cobs_codec
{
public:
  typedef cobs_options_t options_type;

  /// @pre `options.max_frame_size > 0`.
  explicit cobs_codec (options_type const &options = options_type ())
      : options_ (options),
        encoded_ (cobs_encoded_bound (options.max_frame_size))
  {
    LUMEX_CONTRACT_ASSERT (options.max_frame_size > 0);
  }

  /// @brief Appends the encoded payload and the delimiter to `out`.
  /// @return `payload_too_large` for a payload above `max_frame_size`.
  codec::encode_result_t
  encode (codec::bytes_view_t payload, codec::bytes_t &out) const
  {
    if (payload.size () > options_.max_frame_size)
      return codec::encode_failure (codec::framing_errc::payload_too_large);
    out.reserve (out.size () + cobs_encoded_bound (payload.size ()) + 1);
    std::size_t code_at = out.size ();
    out.push_back (0);
    codec::byte_t code = 1;
    // After a full group the next group opens only when a byte follows, so
    // that 254 non-zero bytes encode as FF and the bytes, with nothing after.
    bool group_open = true;
    for (codec::byte_t value : payload)
      {
        if (!group_open)
          {
            code_at = out.size ();
            out.push_back (0);
            code = 1;
            group_open = true;
          }
        if (value == 0)
          {
            out[code_at] = mask (code);
            code_at = out.size ();
            out.push_back (0);
            code = 1;
            continue;
          }
        out.push_back (mask (value));
        ++code;
        if (code == Constants::KFULL_GROUP_CODE)
          {
            out[code_at] = mask (code);
            group_open = false;
          }
      }
    if (group_open)
      out[code_at] = mask (code);
    out.push_back (options_.delimiter);
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
      {
        if (value == options_.delimiter)
          end_frame (sink);
        else if (!encoded_.dropping () && !encoded_.push (mask (value)))
          codec::detail::deliver (sink, codec::framing_errc::frame_too_large);
      }
    return chunk.size ();
  }

  /// @brief Forgets a partial frame and the drop state.
  void
  reset () LUMEX_NOEXCEPT
  {
    encoded_.clear ();
  }

private:
  codec::byte_t
  mask (codec::byte_t value) const LUMEX_NOEXCEPT
  {
    return static_cast<codec::byte_t> (value ^ options_.delimiter);
  }

  template <typename Sink>
  void
  end_frame (Sink &sink)
  {
    if (encoded_.dropping () || encoded_.empty ())
      {
        encoded_.clear ();
        return;
      }
    codec::framing_errc error = codec::framing_errc::truncated;
    bool const ok = unstuff (error);
    encoded_.clear ();
    if (ok)
      codec::detail::deliver (
          sink, codec::bytes_view_t (decoded_.data (), decoded_.size ()));
    else
      codec::detail::deliver (sink, error);
  }

  // Decodes the unmasked bytes of encoded_ into decoded_.
  bool
  unstuff (codec::framing_errc &error)
  {
    decoded_.clear ();
    codec::bytes_view_t const in = encoded_.view ();
    std::size_t at = 0;
    while (at < in.size ())
      {
        std::size_t const code = in[at];
        if (code == 0 || at + code > in.size ())
          {
            error = codec::framing_errc::truncated;
            return false;
          }
        decoded_.insert (decoded_.end (), in.begin () + at + 1,
                         in.begin () + at + code);
        at += code;
        if (code != Constants::KFULL_GROUP_CODE && at < in.size ())
          decoded_.push_back (0);
        if (decoded_.size () > options_.max_frame_size)
          {
            error = codec::framing_errc::frame_too_large;
            return false;
          }
      }
    return true;
  }

  options_type options_;
  codec::detail::frame_buffer encoded_;
  codec::bytes_t decoded_;
};
} // namespace cobs
} // namespace framing
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_FRAMING_COBS_CODEC_HPP
