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
 * @file LumexMessageSession.hpp
 * @brief `message_session`: a `message_transport` (UDP socket, HID device,
 * mock) made by a factory, kept open by reconnecting, with the asynchronous
 * receive and send of `message_transport` for any completion token.
 * @details The same session as `stream_session` (see
 * `LumexBasicSession.hpp`): a receive or send started while the session is
 * not open completes with `not_open`; one that ends with an error
 * `is_link_lost` accepts starts reconnecting before its handler runs. The
 * cancellation slot and the outstanding work are as for `stream_session`.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_SESSION_MESSAGE_SESSION_HPP
#define LUMEX_APPLIED_TRANSPORT_SESSION_MESSAGE_SESSION_HPP

#include <memory>
#include <system_error>
#include <utility>

#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/session/LumexBasicSession.hpp"
#include "lumex/applied/transport/session/LumexSessionTypes.hpp"
#include "lumex/applied/transport/session/detail/LumexSessionImpl.hpp"
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
namespace session
{
/// @brief A session over message transports (see the file description).
class message_session : public basic_session<datagram::message_transport>
{
  struct initiate_io : initiate_base
  {
    explicit initiate_io (executor_type const &executor)
        : initiate_base (executor)
    {
    }

    template <typename Handler, typename Operation>
    void
    operator() (Handler &&handler, std::shared_ptr<impl_type> impl,
                Operation operation) const
    {
      if (!impl)
        {
          Operation::fail_detached (std::forward<Handler> (handler));
          return;
        }
      impl->start_io (std::move (operation), std::forward<Handler> (handler));
    }
  };

public:
  /// @brief A closed session over the transports `factory` makes.
  message_session (executor_type const &executor, factory_t factory,
                   session_options_t options = session_options_t ())
      : basic_session<datagram::message_transport> (
            executor, std::move (factory), std::move (options))
  {
  }

  /// @brief Receives one message; `void (std::error_code, message_t)`.
  template <typename Token>
  auto
  async_receive (Token &&token)
      -> decltype (asio::async_initiate<Token, void (std::error_code,
                                                     datagram::message_t)> (
          std::declval<initiate_io> (), token,
          std::declval<std::shared_ptr<impl_type> &> (),
          std::declval<detail::receive_operation> ()))
  {
    return asio::async_initiate<Token,
                                void (std::error_code, datagram::message_t)> (
        initiate_io (initiation_executor ()), token, impl_,
        detail::receive_operation ());
  }

  /// @brief Sends one message; `void (std::error_code)`.
  template <typename Token>
  auto
  async_send (datagram::message_t message, Token &&token)
      -> decltype (asio::async_initiate<Token, void (std::error_code)> (
          std::declval<initiate_io> (), token,
          std::declval<std::shared_ptr<impl_type> &> (),
          std::declval<detail::send_operation> ()))
  {
    detail::send_operation operation;
    operation.message = std::move (message);
    return asio::async_initiate<Token, void (std::error_code)> (
        initiate_io (initiation_executor ()), token, impl_,
        std::move (operation));
  }
};
} // namespace session
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_SESSION_MESSAGE_SESSION_HPP
