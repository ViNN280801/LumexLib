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
 * @file LumexFramingPipeline.hpp
 * @brief `pipeline<Outer, Stage...>`: a codec that finds frames with `Outer`
 * and transforms each with the stages, listed from the outside in.
 * @details `encode` applies the stages from the last to the first and then
 * `Outer::encode`; the decoder applies them to every frame `Outer` delivers,
 * from the first to the last. A stage error drops the frame and reaches a
 * sink of the error form. For a device that sends `%` + hex text + an
 * XOR of that text + CR:
 * @code
 * using text_t = pipeline<delimiter_codec, prefix_char_stage<'%'>,
 *                         checksum_stage<xor8, ascii_hex_render,
 *                                        checksum_optional>,
 *                         ascii_hex_stage>;
 * @endcode
 * The options are those of `Outer`; the stages are default-constructed or
 * passed after them.
 */
#ifndef LUMEX_CORE_FRAMING_STAGE_PIPELINE_HPP
#define LUMEX_CORE_FRAMING_STAGE_PIPELINE_HPP

#include <cstddef>
#include <system_error>
#include <tuple>
#include <type_traits>

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
namespace detail
{
// Applies the stages I, I + 1, ..., N - 1 to a decoded frame.
template <std::size_t I, std::size_t N> struct decode_stages
{
  template <typename Tuple>
  static std::error_code
  run (Tuple const &stages, codec::bytes_t &frame, codec::bytes_t &scratch)
  {
    scratch.clear ();
    // One named result for every path, so that it is returned in place.
    std::error_code error = std::get<I> (stages).decode (
        codec::bytes_view_t (frame.data (), frame.size ()), scratch);
    if (!error)
      {
        frame.swap (scratch);
        error = decode_stages<I + 1, N>::run (stages, frame, scratch);
      }
    return error;
  }
};

template <std::size_t N> struct decode_stages<N, N>
{
  template <typename Tuple>
  static std::error_code
  run (Tuple const &, codec::bytes_t &, codec::bytes_t &)
  {
    return std::error_code ();
  }
};

// Applies the stages I - 1, I - 2, ..., 0 to a payload.
template <std::size_t I> struct encode_stages
{
  template <typename Tuple>
  static codec::encode_result_t
  run (Tuple const &stages, codec::bytes_t &frame, codec::bytes_t &scratch)
  {
    scratch.clear ();
    codec::encode_result_t result = std::get<I - 1> (stages).encode (
        codec::bytes_view_t (frame.data (), frame.size ()), scratch);
    if (result.has_value ())
      {
        frame.swap (scratch);
        result = encode_stages<I - 1>::run (stages, frame, scratch);
      }
    return result;
  }
};

template <> struct encode_stages<0>
{
  template <typename Tuple>
  static codec::encode_result_t
  run (Tuple const &, codec::bytes_t &, codec::bytes_t &)
  {
    return codec::encode_result_t ();
  }
};
} // namespace detail

/// @brief A codec of `Outer` followed by stages (see the file description).
template <typename Outer, typename... Stages> class pipeline
{
  static_assert (sizeof...(Stages) > 0, "a pipeline needs at least one stage");

public:
  typedef typename Outer::options_type options_type;

  /// @brief The outer codec with these options, default stages.
  explicit pipeline (options_type const &options = options_type ())
      : outer_ (options)
  {
  }

  /// @brief The outer codec with these options and these stages.
  pipeline (options_type const &options, Stages const &...stages)
      : outer_ (options), stages_ (stages...)
  {
  }

  /// @brief Applies the stages to the payload and frames the result.
  codec::encode_result_t
  encode (codec::bytes_view_t payload, codec::bytes_t &out) const
  {
    codec::bytes_t frame (payload.begin (), payload.end ());
    codec::bytes_t scratch;
    codec::encode_result_t result
        = detail::encode_stages<sizeof...(Stages)>::run (stages_, frame,
                                                         scratch);
    if (result.has_value ())
      result = outer_.encode (
          codec::bytes_view_t (frame.data (), frame.size ()), out);
    return result;
  }

  /**
   * @brief Consumes a chunk of the stream and delivers every frame it ends,
   * with the stages undone.
   * @return What `Outer::decode` returns.
   */
  template <typename Sink>
  std::size_t
  decode (codec::bytes_view_t chunk, Sink &&sink)
  {
    staged_sink<typename std::remove_reference<Sink>::type> inner{ this,
                                                                   &sink };
    return outer_.decode (chunk, inner);
  }

  /// @brief Forgets the partial frame of the outer codec.
  void
  reset () LUMEX_NOEXCEPT
  {
    outer_.reset ();
  }

private:
  // The sink the outer codec calls: runs the stages, then the user's sink.
  template <typename Sink> struct staged_sink
  {
    pipeline *owner;
    Sink *sink;

    void
    operator() (codec::bytes_view_t frame, std::error_code error) const
    {
      if (error)
        {
          codec::detail::deliver (*sink, error);
          return;
        }
      owner->frame_.assign (frame.begin (), frame.end ());
      std::error_code const stage_error
          = detail::decode_stages<0, sizeof...(Stages)>::run (
              owner->stages_, owner->frame_, owner->scratch_);
      if (stage_error)
        codec::detail::deliver (*sink, stage_error);
      else
        codec::detail::deliver (*sink,
                                codec::bytes_view_t (owner->frame_.data (),
                                                     owner->frame_.size ()));
    }
  };

  Outer outer_;
  std::tuple<Stages...> stages_;
  codec::bytes_t frame_;
  codec::bytes_t scratch_;
};
} // namespace stage
} // namespace framing
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_FRAMING_STAGE_PIPELINE_HPP
