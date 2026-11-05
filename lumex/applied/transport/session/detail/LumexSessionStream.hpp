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
 * @file LumexSessionStream.hpp
 * @brief The views of a session as a transport: a `stream::stream_impl` and a
 * `datagram::message_impl` over `session_impl`, what
 * `stream_session::as_byte_stream` and `message_session::as_message_transport`
 * give.
 * @details Internal to the transport library. A view runs on the strand of its
 * session, so what the session decides and what the adapter above it sees
 * have one order: the state change of a lost link (the close count grows, the
 * view reads as closed, the event of the connection is posted) comes before
 * the completion of any operation of the transport that is gone, and the event
 * of a new connection before any byte of it. The view holds the session
 * strongly and the session holds the view weakly; what the view gives the
 * transport holds the view weakly.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_SESSION_DETAIL_SESSION_STREAM_HPP
#define LUMEX_APPLIED_TRANSPORT_SESSION_DETAIL_SESSION_STREAM_HPP

#include <memory>

#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"
#include "lumex/applied/transport/session/detail/LumexSessionImpl.hpp"
#include "lumex/applied/transport/stream/LumexByteStream.hpp"

namespace lumex
{
namespace applied
{
namespace transport
{
namespace session
{
namespace detail
{
/// @brief A stream that is this session; null for a null session.
std::shared_ptr<stream::stream_impl> make_session_stream (
    std::shared_ptr<session_impl<stream::byte_stream>> const &session);

/// @brief A message transport that is this session; null for a null session.
std::shared_ptr<datagram::message_impl> make_session_message (
    std::shared_ptr<session_impl<datagram::message_transport>> const &session);
} // namespace detail
} // namespace session
} // namespace transport
} // namespace applied
} // namespace lumex

#endif // !LUMEX_APPLIED_TRANSPORT_SESSION_DETAIL_SESSION_STREAM_HPP
