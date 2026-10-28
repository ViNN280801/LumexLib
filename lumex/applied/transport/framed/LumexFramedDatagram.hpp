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
 * @file LumexFramedDatagram.hpp
 * @brief `framed_datagram<Codec>`: frames over a
 * `datagram::message_transport` (UDP, HID reports), with the codec of
 * `core/framing` applied to each datagram.
 * @details The contracts of `framed_stream` hold (ownership, the queue, the
 * statistics, cancellation, the strand, destruction at any time); this file
 * lists what differs. A datagram is a unit of its own: the codec starts afresh
 * with every datagram, so a partial frame at the end of one datagram is never
 * completed by the next, and a lost datagram cannot corrupt its neighbours.
 * A datagram may hold several frames; each is delivered separately as a
 * `datagram::message_t` that carries the decoded payload and the report ID of
 * the datagram it came from. A datagram without a complete frame is counted
 * as `framing_errc::truncated` (a codec does not tell about a partial frame
 * that follows a complete one). An empty datagram is ignored.
 *
 * As for the stream, the decoder keeps a receive pending from the first read
 * until `close ()`: with a real transport it is Asio work on the transport's
 * `io_context`, so `run ()` does not return while the adapter is open (see
 * `LumexFramedStream.hpp`); end it with `close ()` or bound the run.
 *
 * Writing: `write_frame (payload, deadline)` sends one datagram with the
 * encoded frame and report ID 0; `write_frame (report_id, payload, deadline)`
 * names the report ID of a HID device. One frame is one datagram.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_FRAMED_FRAMED_DATAGRAM_HPP
#define LUMEX_APPLIED_TRANSPORT_FRAMED_FRAMED_DATAGRAM_HPP

#include <cstdint>
#include <memory>
#include <system_error>
#include <utility>

#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"
#include "lumex/applied/transport/framed/LumexFramedStatistics.hpp"
#include "lumex/applied/transport/framed/LumexFramedTypes.hpp"
#include "lumex/applied/transport/framed/detail/LumexBasicFramed.hpp"
#include "lumex/applied/transport/framed/detail/LumexFramedCodecHub.hpp"

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
/// @brief Frames over a message transport (see the file description).
template <typename Codec>
class framed_datagram
    : public detail::basic_framed<datagram::message_transport>
{
  using base_type = detail::basic_framed<datagram::message_transport>;

public:
  /// @brief The codec of `core/framing`.
  using codec_type = Codec;

  /// @brief Frames over `device` (not empty; moved in) with `codec` (moved
  /// in).
  framed_datagram (datagram::message_transport device, Codec codec,
                   framed_options_t const &options = framed_options_t ())
      : base_type (std::make_shared<
                   detail::codec_hub<datagram::message_transport, Codec>> (
            std::move (device), std::move (codec), options))
  {
  }

  framed_datagram (framed_datagram &&other) LUMEX_NOEXCEPT
      : base_type (std::move (static_cast<base_type &> (other)))
  {
  }

  framed_datagram &
  operator= (framed_datagram &&other) LUMEX_NOEXCEPT
  {
    base_type::operator= (std::move (static_cast<base_type &> (other)));
    return *this;
  }

  framed_datagram (framed_datagram const &) = delete;
  framed_datagram &operator= (framed_datagram const &) = delete;

  ~framed_datagram () {}

  using base_type::async_write_frame;
  using base_type::write_frame;

  /// @brief Writes the frame of `payload` in a report with `report_id`
  /// within the deadline.
  status_result_t
  write_frame (std::uint8_t report_id, const_bytes_t payload,
               deadline_t deadline)
  {
    return this->write_frame_as (report_id, payload, deadline);
  }

  /// @brief Writes the frame of `payload` in a report with `report_id`; the
  /// token gets `void (std::error_code)`.
  template <typename Token>
  auto
  async_write_frame (std::uint8_t report_id, const_bytes_t payload,
                     Token &&token)
      -> decltype (asio::async_initiate<Token, void (std::error_code)> (
          std::declval<typename base_type::initiate_write_frame> (), token,
          std::declval<std::shared_ptr<typename base_type::hub_type> &> (),
          std::declval<std::uint8_t &> (), std::declval<const_bytes_t &> ()))
  {
    return asio::async_initiate<Token, void (std::error_code)> (
        typename base_type::initiate_write_frame (
            this->initiation_executor ()),
        token, this->hub_, report_id, payload);
  }
};

/// @brief `framed_datagram<Codec>` from the arguments.
template <typename Codec>
framed_datagram<Codec>
make_framed_datagram (datagram::message_transport device, Codec codec,
                      framed_options_t const &options = framed_options_t ())
{
  return framed_datagram<Codec> (std::move (device), std::move (codec),
                                 options);
}
} // namespace framed
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_FRAMED_FRAMED_DATAGRAM_HPP
