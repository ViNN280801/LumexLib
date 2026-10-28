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
 * @file LumexHeaderCodec.hpp
 * @brief `header_codec<Check>`: binary frames with a preamble, a fixed
 * header that holds the length of the data block, the data and a check value.
 * @details The frame is
 * @code
 * [preamble][fields before the length][length][fields after it][data][check]
 * @endcode
 * `header_size` counts everything before the data (the preamble and the
 * length field included); the length field sits at `length_offset` from the
 * start of the frame and holds the size of the data block, or of the whole
 * frame (`length_counts::frame`). The check value (`Check`: `stage::xor8`,
 * `stage::sum8`, `stage::crc<Spec, Order>` or `no_check`) covers the frame
 * from `check_from` to the end of the data.
 *
 * The payload of `encode` and of the sink is the header fields without the
 * preamble and the length field, followed by the data: for a frame
 * `[PRE:2][VAR:2][SIZE:2][CMD:2][DATA][CRC32]` it is VAR, CMD and DATA. The
 * preamble may differ per direction (`tx_preamble`, `rx_preamble`).
 *
 * The decoder searches the stream for the receive preamble and drops the
 * bytes before it without a report (line noise). A length above
 * `max_frame_size` (which limits the data block) is `frame_too_large`, a
 * whole-frame length smaller than the header and the check is `bad_header`,
 * a wrong check value `bad_checksum`; after each the search restarts one byte
 * after the rejected preamble, so a preamble inside the bad frame is found.
 */
#ifndef LUMEX_CORE_FRAMING_HEADER_CODEC_HPP
#define LUMEX_CORE_FRAMING_HEADER_CODEC_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#endif

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "lumex/core/contracts/LumexContracts"
#include "lumex/core/framing/codec/LumexFramingCodec.hpp"
#include "lumex/core/framing/stage/LumexFramingStages.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace framing
{
namespace header
{
/// @brief No check value: the frame ends with the data.
struct no_check
{
  static std::size_t
  width () LUMEX_NOEXCEPT
  {
    return 0;
  }

  static codec::byte_order
  order () LUMEX_NOEXCEPT
  {
    return codec::byte_order::big;
  }

  static std::uint64_t
  compute (codec::bytes_view_t) LUMEX_NOEXCEPT
  {
    return 0;
  }
};

/// @brief What the length field of the header counts.
enum class length_counts
{
  /// The data block only.
  data,
  /// The whole frame: header, data and check value.
  frame
};

/// @brief The options of `header_codec`.
struct header_options_t : codec::codec_options_t
{
  /// The bytes that start a frame `encode` writes; not empty.
  codec::bytes_t tx_preamble;
  /// The bytes that start a frame the decoder accepts; as long as
  /// `tx_preamble`.
  codec::bytes_t rx_preamble;
  /// The bytes before the data, the preamble and the length field included.
  std::size_t header_size = 0;
  /// Where the length field starts, from the start of the frame; at or after
  /// the end of the preamble.
  std::size_t length_offset = 0;
  /// The width of the length field: 1, 2 or 4.
  std::size_t length_width = 2;
  /// The byte order of the length field.
  codec::byte_order length_order = codec::byte_order::little;
  /// What the length field counts.
  length_counts counts = length_counts::data;
  /// Where the check value starts covering the frame (0: from the preamble).
  std::size_t check_from = 0;
};

/// @brief Header framing (see the file description).
template <typename Check = no_check> class header_codec
{
public:
  using options_type = header_options_t;

  /// @pre `max_frame_size > 0`; the preambles are equally long and not
  /// empty; the length field (1, 2 or 4 bytes) lies between the preamble and
  /// the end of the header; `check_from` is inside the header.
  explicit header_codec (options_type const &options) : options_ (options)
  {
    LUMEX_CONTRACT_ASSERT (options.max_frame_size > 0);
    LUMEX_CONTRACT_ASSERT (!options.tx_preamble.empty ());
    LUMEX_CONTRACT_ASSERT (options.rx_preamble.size ()
                           == options.tx_preamble.size ());
    LUMEX_CONTRACT_ASSERT (options.length_width == 1
                           || options.length_width == 2
                           || options.length_width == 4);
    LUMEX_CONTRACT_ASSERT (options.length_offset
                           >= options.tx_preamble.size ());
    LUMEX_CONTRACT_ASSERT (options.length_offset + options.length_width
                           <= options.header_size);
    LUMEX_CONTRACT_ASSERT (options.check_from <= options.header_size);
  }

  /**
   * @brief Appends the frame of the payload (header fields without the
   * preamble and the length field, then the data) to `out`.
   * @return `truncated` for a payload shorter than the header fields,
   * `payload_too_large` for data above `max_frame_size` or above what the
   * length field can hold.
   */
  codec::encode_result_t
  encode (codec::bytes_view_t payload, codec::bytes_t &out) const
  {
    std::size_t const fields = header_fields ();
    if (payload.size () < fields)
      return codec::encode_failure (codec::framing_errc::truncated);
    std::size_t const data_size = payload.size () - fields;
    std::uint64_t const declared
        = options_.counts == length_counts::data
              ? data_size
              : options_.header_size + data_size + Check::width ();
    if (data_size > options_.max_frame_size || declared > field_maximum ())
      return codec::encode_failure (codec::framing_errc::payload_too_large);

    std::size_t const start = out.size ();
    std::size_t const before = before_length ();
    out.insert (out.end (), options_.tx_preamble.begin (),
                options_.tx_preamble.end ());
    out.insert (out.end (), payload.begin (),
                payload.begin () + static_cast<std::ptrdiff_t> (before));
    codec::detail::put_uint (declared, options_.length_width,
                             options_.length_order, out);
    out.insert (out.end (),
                payload.begin () + static_cast<std::ptrdiff_t> (before),
                payload.end ());
    codec::bytes_view_t const checked (
        out.data () + start + options_.check_from,
        out.size () - start - options_.check_from);
    codec::detail::put_uint (Check::compute (checked), Check::width (),
                             Check::order (), out);
    return codec::encode_result_t ();
  }

  /**
   * @brief Consumes a chunk of the stream and delivers every frame it
   * completes.
   * @return `chunk.size ()`: the whole chunk is always consumed.
   */
  template <typename Sink>
  std::size_t
  decode (codec::bytes_view_t chunk, Sink &&sink)
  {
    buffer_.insert (buffer_.end (), chunk.begin (), chunk.end ());
    while (next_frame (sink))
      {
      }
    return chunk.size ();
  }

  /// @brief Forgets the received bytes.
  void
  reset () LUMEX_NOEXCEPT
  {
    buffer_.clear ();
  }

private:
  std::size_t
  before_length () const LUMEX_NOEXCEPT
  {
    return options_.length_offset - options_.rx_preamble.size ();
  }

  std::size_t
  header_fields () const LUMEX_NOEXCEPT
  {
    return options_.header_size - options_.rx_preamble.size ()
           - options_.length_width;
  }

  std::uint64_t
  field_maximum () const LUMEX_NOEXCEPT
  {
    return options_.length_width >= 4
               ? 0xFFFFFFFFull
               : (std::uint64_t (1) << (8 * options_.length_width)) - 1;
  }

  // Drops the bytes before the first (possibly partial) preamble.
  void
  synchronize ()
  {
    codec::bytes_t const &preamble = options_.rx_preamble;
    std::size_t at = 0;
    while (at < buffer_.size ())
      {
        std::size_t const available
            = std::min (preamble.size (), buffer_.size () - at);
        if (std::equal (preamble.begin (),
                        preamble.begin ()
                            + static_cast<std::ptrdiff_t> (available),
                        buffer_.begin () + static_cast<std::ptrdiff_t> (at)))
          break;
        ++at;
      }
    drop (at);
  }

  void
  drop (std::size_t count)
  {
    buffer_.erase (buffer_.begin (),
                   buffer_.begin () + static_cast<std::ptrdiff_t> (count));
  }

  // Rejects the frame at the front: reports, then searches again from the
  // byte after its preamble.
  template <typename Sink>
  bool
  reject (codec::framing_errc error, Sink &sink)
  {
    codec::detail::deliver (sink, error);
    drop (1);
    return true;
  }

  // Delivers the frame at the front of the buffer when it is complete;
  // false when more bytes are needed.
  template <typename Sink>
  bool
  next_frame (Sink &sink)
  {
    synchronize ();
    std::size_t const check_width = Check::width ();
    if (buffer_.size () < options_.header_size)
      return false;
    std::uint64_t const declared = codec::detail::get_uint (
        buffer_.data () + options_.length_offset, options_.length_width,
        options_.length_order);
    std::uint64_t data_size = declared;
    if (options_.counts == length_counts::frame)
      {
        if (declared < options_.header_size + check_width)
          return reject (codec::framing_errc::bad_header, sink);
        data_size = declared - options_.header_size - check_width;
      }
    if (data_size > options_.max_frame_size)
      return reject (codec::framing_errc::frame_too_large, sink);
    std::size_t const data_end
        = options_.header_size + static_cast<std::size_t> (data_size);
    if (buffer_.size () < data_end + check_width)
      return false;

    codec::bytes_view_t const checked (buffer_.data () + options_.check_from,
                                       data_end - options_.check_from);
    if (check_width > 0
        && codec::detail::get_uint (buffer_.data () + data_end, check_width,
                                    Check::order ())
               != Check::compute (checked))
      return reject (codec::framing_errc::bad_checksum, sink);

    std::size_t const preamble = options_.rx_preamble.size ();
    std::size_t const after = options_.length_offset + options_.length_width;
    payload_.assign (
        buffer_.begin () + static_cast<std::ptrdiff_t> (preamble),
        buffer_.begin ()
            + static_cast<std::ptrdiff_t> (options_.length_offset));
    payload_.insert (payload_.end (),
                     buffer_.begin () + static_cast<std::ptrdiff_t> (after),
                     buffer_.begin ()
                         + static_cast<std::ptrdiff_t> (data_end));
    drop (data_end + check_width);
    codec::detail::deliver (
        sink, codec::bytes_view_t (payload_.data (), payload_.size ()));
    return true;
  }

  options_type options_;
  codec::bytes_t buffer_;
  codec::bytes_t payload_;
};
} // namespace header
} // namespace framing
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_FRAMING_HEADER_CODEC_HPP
