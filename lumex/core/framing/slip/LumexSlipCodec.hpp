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
 * @file LumexSlipCodec.hpp
 * @brief `slip_codec`: SLIP framing of RFC 1055.
 * @details A frame is the payload with `END` (0xC0) and `ESC` (0xDB) replaced
 * by `ESC ESC_END` and `ESC ESC_ESC`, between two `END` bytes. There is no
 * header: the payload is carried as it is. `max_frame_size` limits the
 * payload (the decoded bytes of one frame). An empty frame (two `END` in a
 * row) is line noise and is never delivered, so an empty payload, which
 * `encode` accepts, does not arrive.
 *
 * `ESC` followed by a byte other than `ESC_END` or `ESC_ESC` is
 * `framing_errc::bad_escape`: the frame is dropped up to the next `END`
 * (`ESC END` drops the frame and the `END` still ends it). RFC 1055 itself
 * stores such a byte as it is; `slip_options_t::lenient_escape` restores that
 * behaviour for a peer that relies on it.
 */
#ifndef LUMEX_CORE_FRAMING_SLIP_CODEC_HPP
#define LUMEX_CORE_FRAMING_SLIP_CODEC_HPP

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

#include "lumex/core/contracts/LumexContracts"
#include "lumex/core/framing/codec/LumexFramingCodec.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace framing
{
namespace slip
{
namespace Constants
{
/// @brief Ends a frame.
LUMEX_CONSTEXPR codec::byte_t KEND = 0xC0;
/// @brief Starts an escape sequence.
LUMEX_CONSTEXPR codec::byte_t KESC = 0xDB;
/// @brief `ESC ESC_END` stands for an `END` byte of the payload.
LUMEX_CONSTEXPR codec::byte_t KESC_END = 0xDC;
/// @brief `ESC ESC_ESC` stands for an `ESC` byte of the payload.
LUMEX_CONSTEXPR codec::byte_t KESC_ESC = 0xDD;
} // namespace Constants

/// @brief The options of `slip_codec`.
struct slip_options_t : codec::codec_options_t
{
  /// Store the byte after an invalid escape as it is (RFC 1055) instead of
  /// dropping the frame with `bad_escape`.
  bool lenient_escape = false;
};

/// @brief SLIP framing of RFC 1055 (see the file description).
class slip_codec
{
public:
  typedef slip_options_t options_type;

  /// @pre `options.max_frame_size > 0`.
  explicit slip_codec (options_type const &options = options_type ())
      : options_ (options), frame_ (options.max_frame_size)
  {
    LUMEX_CONTRACT_ASSERT (options.max_frame_size > 0);
  }

  /// @brief Appends `END`, the escaped payload and `END` to `out`.
  /// @return `payload_too_large` for a payload above `max_frame_size`.
  codec::encode_result_t
  encode (codec::bytes_view_t payload, codec::bytes_t &out) const
  {
    if (payload.size () > options_.max_frame_size)
      return codec::encode_failure (codec::framing_errc::payload_too_large);
    out.reserve (out.size () + payload.size () + 2);
    out.push_back (Constants::KEND);
    for (codec::byte_t value : payload)
      {
        if (value == Constants::KEND)
          {
            out.push_back (Constants::KESC);
            out.push_back (Constants::KESC_END);
          }
        else if (value == Constants::KESC)
          {
            out.push_back (Constants::KESC);
            out.push_back (Constants::KESC_ESC);
          }
        else
          out.push_back (value);
      }
    out.push_back (Constants::KEND);
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

  /// @brief Forgets a partial frame, a pending escape and the drop state.
  void
  reset () LUMEX_NOEXCEPT
  {
    frame_.clear ();
    escaped_ = false;
    discarding_ = false;
  }

private:
  template <typename Sink>
  void
  take (codec::byte_t value, Sink &sink)
  {
    if (escaped_)
      {
        escaped_ = false;
        if (value == Constants::KESC_END)
          store (Constants::KEND, sink);
        else if (value == Constants::KESC_ESC)
          store (Constants::KESC, sink);
        else if (options_.lenient_escape)
          store (value, sink);
        else
          {
            discard (codec::framing_errc::bad_escape, sink);
            if (value == Constants::KEND)
              end_frame (sink);
          }
        return;
      }
    if (value == Constants::KEND)
      end_frame (sink);
    else if (value == Constants::KESC)
      escaped_ = true;
    else
      store (value, sink);
  }

  template <typename Sink>
  void
  store (codec::byte_t value, Sink &sink)
  {
    if (discarding_)
      return;
    if (!frame_.push (value))
      discard (codec::framing_errc::frame_too_large, sink);
  }

  // Drops the frame up to the next END and reports why, once.
  template <typename Sink>
  void
  discard (codec::framing_errc error, Sink &sink)
  {
    if (discarding_)
      return;
    discarding_ = true;
    frame_.clear ();
    codec::detail::deliver (sink, error);
  }

  template <typename Sink>
  void
  end_frame (Sink &sink)
  {
    if (!discarding_ && !frame_.empty ())
      codec::detail::deliver (sink, frame_.view ());
    frame_.clear ();
    discarding_ = false;
  }

  options_type options_;
  codec::detail::frame_buffer frame_;
  bool escaped_ = false;
  bool discarding_ = false;
};
} // namespace slip
} // namespace framing
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_FRAMING_SLIP_CODEC_HPP
