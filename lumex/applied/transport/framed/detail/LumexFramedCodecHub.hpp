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
 * @file LumexFramedCodecHub.hpp
 * @brief `codec_hub<Transport, Codec>`: the hub with its codec.
 * @details Internal to the transport library. The codec is touched only on
 * the strand of the hub: `encode` when a write is taken from the caller,
 * `decode` and `reset` in the pump. A codec must be movable; an exception of
 * the codec is the caller's concern (`std::bad_alloc` only, per the contract
 * of `core/framing`).
 */
#ifndef LUMEX_APPLIED_TRANSPORT_FRAMED_DETAIL_FRAMED_CODEC_HUB_HPP
#define LUMEX_APPLIED_TRANSPORT_FRAMED_DETAIL_FRAMED_CODEC_HUB_HPP

#include <system_error>
#include <utility>

#include "lumex/applied/transport/framed/LumexFramedTypes.hpp"
#include "lumex/applied/transport/framed/detail/LumexFramedHub.hpp"
#include "lumex/core/framing/codec/LumexFramingCodec.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif

namespace lumex
{
namespace applied
{
namespace transport
{
namespace framed
{
namespace detail
{
/// @brief A hub that frames with a `Codec` (see the file description).
template <typename Transport, typename Codec>
class codec_hub final : public framed_hub<Transport>
{
  typedef framed_hub<Transport> base_type;

public:
  typedef typename base_type::bytes_t bytes_t;
  typedef typename base_type::bytes_view_t bytes_view_t;

  codec_hub (Transport transport, Codec codec, framed_options_t const &options)
      : base_type (std::move (transport), options), codec_ (std::move (codec))
  {
  }

protected:
  std::error_code
  do_encode (bytes_view_t payload, bytes_t &wire) override
  {
    core::framing::codec::encode_result_t const result
        = codec_.encode (payload, wire);
    if (result.has_value ())
      return std::error_code ();
    return result.error ();
  }

  void
  do_decode (bytes_view_t chunk) override
  {
    decode_sink sink = { this };
    codec_.decode (chunk, sink);
  }

  void
  do_reset () LUMEX_NOEXCEPT override
  {
    codec_.reset ();
  }

private:
  // The sink of the codec: of the error form, so that a dropped frame is
  // heard too.
  struct decode_sink
  {
    codec_hub *owner;

    void
    operator() (bytes_view_t frame, std::error_code error) const
    {
      owner->decoded (frame, error);
    }
  };

  Codec codec_;
};
} // namespace detail
} // namespace framed
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_FRAMED_DETAIL_FRAMED_CODEC_HUB_HPP
