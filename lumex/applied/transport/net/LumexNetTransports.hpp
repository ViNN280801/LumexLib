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
 * @file LumexNetTransports.hpp
 * @brief TCP as a `byte_stream` and UDP as a `message_transport`.
 * @details `open` resolves the host and connects; a synchronous `open` ends
 * with `timeout` at its deadline, an asynchronous one is bounded by the
 * operating system. A cancel or close during the open ends it at once with
 * `cancelled`, also while the name is still resolving. A host name that does
 * not resolve is `invalid_endpoint`; a refused connection keeps its system
 * error. A TCP connection the peer closes, resets or aborts ends the pending
 * and later reads and writes with `disconnected`.
 *
 * The UDP transport is connected to one peer: a receive returns one
 * datagram, a send writes one. A datagram longer than `max_datagram_size` is
 * returned cut to that size on every platform (and logged as a warning). A
 * message to send must carry report ID 0, or the send fails with
 * `std::errc::invalid_argument`; received messages carry 0.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_NET_NET_TRANSPORTS_HPP
#define LUMEX_APPLIED_TRANSPORT_NET_NET_TRANSPORTS_HPP

#include <cstddef>
#include <cstdint>
#include <string>

#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"
#include "lumex/applied/transport/stream/LumexByteStream.hpp"
#include "lumex/applied/transport/stream/LumexStreamTypes.hpp"

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
namespace net
{
/// @brief What `make_tcp_stream` connects to.
struct tcp_options_t
{
  /// Host name or address ("192.0.2.10", "device.local").
  std::string host;
  /// Port number.
  std::uint16_t port = 0;
  /// TCP_NODELAY: send small writes at once (default on: request/response).
  bool no_delay = true;
  /// SO_KEEPALIVE: let the operating system probe an idle connection.
  bool keep_alive = false;
};

/// @brief What `make_udp_socket` connects to.
struct udp_options_t
{
  /// Host name or address of the peer.
  std::string host;
  /// Port of the peer.
  std::uint16_t port = 0;
  /// Local port to bind (0: any).
  std::uint16_t local_port = 0;
  /// The receive buffer: the longest datagram a receive returns whole.
  std::size_t max_datagram_size = 65507;
};

/// @brief A TCP stream on `executor` (closed; call `open`).
stream::byte_stream make_tcp_stream (executor_type const &executor,
                                     tcp_options_t const &options);

/// @brief A connected UDP socket on `executor` (closed; call `open`).
datagram::message_transport make_udp_socket (executor_type const &executor,
                                             udp_options_t const &options);
} // namespace net
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_NET_NET_TRANSPORTS_HPP
