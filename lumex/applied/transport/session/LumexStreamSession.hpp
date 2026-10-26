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
 * @file LumexStreamSession.hpp
 * @brief `stream_session`: a `byte_stream` made by a factory, kept open by
 * reconnecting (spec G1, transport level), with the asynchronous I/O of
 * `byte_stream` for any completion token.
 * @details See `LumexBasicSession.hpp` for the states, the cycle and the
 * threading. The I/O runs on the session's current stream: an operation
 * started while the session is not open completes with `not_open`; one that
 * ends with an error `is_link_lost` accepts starts reconnecting before its
 * handler runs. A handler runs on its associated executor, by default on the
 * session strand; the session keeps no work on that executor (keep an
 * `executor_work_guard` on a context whose `run ()` must not return before
 * the handler). A terminal cancellation on the handler's slot cancels the
 * pending I/O of the current stream (see `LumexBasicSession.hpp`). Nothing
 * is resent: a protocol client decides that (phases 4 and 5).
 *
 * @code
 * session::stream_session link (
 *     runner.get_executor (), session::make_tcp_factory ({ "10.0.0.5", 5000
 * })); link.open (deadline_t (5000)); link.async_read_some (buffer, handler);
 * @endcode
 */
#ifndef LUMEX_APPLIED_TRANSPORT_SESSION_STREAM_SESSION_HPP
#define LUMEX_APPLIED_TRANSPORT_SESSION_STREAM_SESSION_HPP

#include <cstddef>
#include <memory>
#include <system_error>
#include <utility>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/session/LumexBasicSession.hpp"
#include "lumex/applied/transport/session/LumexSessionTypes.hpp"
#include "lumex/applied/transport/session/detail/LumexSessionImpl.hpp"
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
namespace session
{
/// @brief A session over byte streams (see the file description).
class stream_session : public basic_session<stream::byte_stream>
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
  /// @brief A closed session over the streams `factory` makes.
  stream_session (executor_type const &executor, factory_t factory,
                  session_options_t options = session_options_t ())
      : basic_session<stream::byte_stream> (executor, std::move (factory),
                                            std::move (options))
  {
  }

  /// @brief Reads at least one byte; `void (std::error_code, std::size_t)`.
  template <typename Token>
  auto
  async_read_some (mutable_bytes_t buffer, Token &&token)
      -> decltype (asio::async_initiate<Token,
                                        void (std::error_code, std::size_t)> (
          std::declval<initiate_io> (), token,
          std::declval<std::shared_ptr<impl_type> &> (),
          std::declval<detail::read_some_operation const &> ()))
  {
    detail::read_some_operation const operation = { buffer };
    return asio::async_initiate<Token, void (std::error_code, std::size_t)> (
        initiate_io (initiation_executor ()), token, impl_, operation);
  }

  /// @brief Writes at least one byte; `void (std::error_code, std::size_t)`.
  template <typename Token>
  auto
  async_write_some (const_bytes_t data, Token &&token)
      -> decltype (asio::async_initiate<Token,
                                        void (std::error_code, std::size_t)> (
          std::declval<initiate_io> (), token,
          std::declval<std::shared_ptr<impl_type> &> (),
          std::declval<detail::write_some_operation const &> ()))
  {
    detail::write_some_operation const operation = { data };
    return asio::async_initiate<Token, void (std::error_code, std::size_t)> (
        initiate_io (initiation_executor ()), token, impl_, operation);
  }

  /// @brief Writes all of `data`; `void (std::error_code, std::size_t)` with
  /// the bytes written before an error.
  template <typename Token>
  auto
  async_write_all (const_bytes_t data, Token &&token)
      -> decltype (asio::async_initiate<Token,
                                        void (std::error_code, std::size_t)> (
          std::declval<initiate_io> (), token,
          std::declval<std::shared_ptr<impl_type> &> (),
          std::declval<detail::write_all_operation const &> ()))
  {
    detail::write_all_operation const operation = { data };
    return asio::async_initiate<Token, void (std::error_code, std::size_t)> (
        initiate_io (initiation_executor ()), token, impl_, operation);
  }
};
} // namespace session
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_SESSION_STREAM_SESSION_HPP
