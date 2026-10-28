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
 * @file LumexMessageTransport.hpp
 * @brief `message_transport`: a transport of whole messages (UDP datagram,
 * HID report, mock) with the threading, deadline and token rules of
 * `byte_stream`.
 * @details One receive returns one message and one send writes one. A
 * message carries the report ID of a HID device explicitly (0 for a device
 * without numbered reports and for every other transport). The
 * implementation (`message_impl`) runs every operation on its strand. The
 * implementation counts every operation (`statistics ()`, with
 * `messages_received` / `messages_sent` in place of `reads` / `writes`).
 * Per-operation cancellation follows `byte_stream` too: a terminal
 * cancellation cancels the transport, every operation pending on it (a
 * concurrent receive loop too), while the operation is pending: a signal
 * emitted after the transport handed the result on ends nothing; the timer of
 * `asio::cancel_after` runs on the strand. As for `byte_stream`, the
 * transport keeps no work on the executor of a handler: keep an
 * `executor_work_guard` on a context whose `run ()` must not return before
 * the handler.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_DATAGRAM_MESSAGE_TRANSPORT_HPP
#define LUMEX_APPLIED_TRANSPORT_DATAGRAM_MESSAGE_TRANSPORT_HPP

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexCountingHandler.hpp"
#include "lumex/applied/transport/statistics/LumexTransportStatistics.hpp"
#include "lumex/applied/transport/stream/LumexStreamTypes.hpp"
#include "lumex/core/expected/Expected"
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
namespace datagram
{
/// @brief One message.
struct message_t
{
  /// The report ID of a HID report; 0 when the device numbers none and for
  /// every other transport.
  std::uint8_t report_id = 0;
  /// The bytes, without the report ID.
  std::vector<std::uint8_t> payload;
};

/// @brief The completion of a receive.
using message_handler_t
    = asio::any_completion_handler<void (std::error_code, message_t)>;

/// @brief The result of a synchronous receive.
using message_result_t
    = lumex::core::expected::result::expected<message_t, std::error_code>;

struct message_impl_access;

/// @brief The implementation behind a `message_transport` (the rules of
/// `stream::stream_impl`).
class message_impl : public std::enable_shared_from_this<message_impl>
{
public:
  using strand_type = asio::strand<executor_type>;

  message_impl (executor_type const &executor, std::string name);
  virtual ~message_impl ();

  message_impl (message_impl const &) = delete;
  message_impl &operator= (message_impl const &) = delete;

  executor_type const &
  executor () const LUMEX_NOEXCEPT
  {
    return executor_;
  }

  strand_type const &
  strand () const LUMEX_NOEXCEPT
  {
    return strand_;
  }

  std::string const &
  name () const LUMEX_NOEXCEPT
  {
    return name_;
  }

  bool
  is_open () const LUMEX_NOEXCEPT
  {
    return open_.load (std::memory_order_acquire);
  }

  /// @brief How many closes the transport has seen (any thread); see
  /// `stream::stream_impl::connection_epoch`.
  std::size_t
  connection_epoch () const LUMEX_NOEXCEPT
  {
    return connection_epoch_.load (std::memory_order_acquire);
  }

  /// @brief The counters this transport counts in (see `statistics ()`).
  statistics::statistics_counters &
  counters () LUMEX_NOEXCEPT
  {
    return *counters_;
  }

  /// @brief The counters, for reading.
  statistics::statistics_counters const &
  counters () const LUMEX_NOEXCEPT
  {
    return *counters_;
  }

  // The start_* with a handler take the handler as the user gave it and wrap
  // it in the counting handler before the one type erasure, and with a
  // cancellation slot take its cancellation (see `stream::stream_impl`).
  template <typename Handler>
  void
  start_open (Handler &&handler)
  {
    detail::operation_mark_ptr const mark
        = detail::cancellation_mark (handler);
    enqueue_open (status_handler_t (detail::count_with (
                      counters_, detail::open_recorder (),
                      std::forward<Handler> (handler), mark)),
                  mark);
  }

  template <typename Handler>
  void
  start_receive (Handler &&handler)
  {
    detail::operation_mark_ptr const mark
        = detail::cancellation_mark (handler);
    enqueue_receive (message_handler_t (detail::count_with (
                         counters_, detail::receive_recorder (),
                         std::forward<Handler> (handler), mark)),
                     mark);
  }

  template <typename Handler>
  void
  start_send (message_t message, Handler &&handler)
  {
    detail::operation_mark_ptr const mark
        = detail::cancellation_mark (handler);
    detail::send_recorder const recorder = { message.payload.size () };
    enqueue_send (
        std::move (message),
        status_handler_t (detail::count_with (
            counters_, recorder, std::forward<Handler> (handler), mark)),
        mark);
  }

  void start_cancel ();
  void start_close ();

protected:
  virtual void do_open (status_handler_t handler) = 0;
  virtual void do_receive (message_handler_t handler) = 0;
  virtual void do_send (message_t message, status_handler_t handler) = 0;
  virtual void do_cancel () = 0;
  virtual void do_close () = 0;

  void
  set_open (bool open) LUMEX_NOEXCEPT
  {
    open_.store (open, std::memory_order_release);
  }

  void complete (status_handler_t handler, std::error_code error);
  void complete (message_handler_t handler, std::error_code error,
                 message_t message);

private:
  friend struct message_impl_access;

  void enqueue_open (status_handler_t handler,
                     detail::operation_mark_ptr const &mark);
  void enqueue_receive (message_handler_t handler,
                        detail::operation_mark_ptr const &mark);
  void enqueue_send (message_t message, status_handler_t handler,
                     detail::operation_mark_ptr const &mark);

  executor_type executor_;
  strand_type strand_;
  std::string name_;
  std::atomic<bool> open_;
  // Bumped by every close (see connection_epoch).
  std::atomic<std::size_t> connection_epoch_;
  std::shared_ptr<statistics::statistics_counters> counters_;
  // The marks of the pending operations with a cancellation slot, settled
  // by complete () (see stream::stream_impl).
  detail::mark_registry marks_;
};

/// @brief A message transport (see the file description).
class message_transport
{
  // Each initiation carries the transport's strand, which tokens such as
  // asio::cancel_after ask for (see byte_stream).
  struct initiate_base
  {
    using executor_type = transport::executor_type;

    explicit initiate_base (executor_type const &executor)
        : executor_ (executor)
    {
    }

    executor_type
    get_executor () const LUMEX_NOEXCEPT
    {
      return executor_;
    }

  private:
    executor_type executor_;
  };

  struct initiate_open : initiate_base
  {
    explicit initiate_open (executor_type const &executor)
        : initiate_base (executor)
    {
    }

    template <typename Handler>
    void
    operator() (Handler &&handler, std::shared_ptr<message_impl> impl) const
    {
      impl->start_open (std::forward<Handler> (handler));
    }
  };

  struct initiate_receive : initiate_base
  {
    explicit initiate_receive (executor_type const &executor)
        : initiate_base (executor)
    {
    }

    template <typename Handler>
    void
    operator() (Handler &&handler, std::shared_ptr<message_impl> impl) const
    {
      impl->start_receive (std::forward<Handler> (handler));
    }
  };

  struct initiate_send : initiate_base
  {
    explicit initiate_send (executor_type const &executor)
        : initiate_base (executor)
    {
    }

    template <typename Handler>
    void
    operator() (Handler &&handler, std::shared_ptr<message_impl> impl,
                message_t message) const
    {
      impl->start_send (std::move (message), std::forward<Handler> (handler));
    }
  };

public:
  explicit message_transport (std::shared_ptr<message_impl> impl);
  ~message_transport ();

  message_transport (message_transport const &) = delete;
  message_transport &operator= (message_transport const &) = delete;
  message_transport (message_transport &&other) LUMEX_NOEXCEPT;
  message_transport &operator= (message_transport &&other) LUMEX_NOEXCEPT;

  executor_type get_executor () const LUMEX_NOEXCEPT;
  std::string const &name () const LUMEX_NOEXCEPT;
  bool is_open () const LUMEX_NOEXCEPT;

  /// @brief A snapshot of the counters (any thread; no lock).
  statistics::statistics_t statistics () const LUMEX_NOEXCEPT;
  /// @brief Sets every counter to zero (any thread).
  void reset_statistics () LUMEX_NOEXCEPT;

  /// @brief Opens within the deadline.
  status_result_t open (deadline_t deadline);
  /// @brief Receives one message within the deadline.
  message_result_t receive (deadline_t deadline);
  /// @brief Sends one message within the deadline.
  status_result_t send (message_t const &message, deadline_t deadline);

  // The return types name every argument as the body passes it (`impl_` is
  // an lvalue): a token such as asio::deferred keeps the argument types in
  // the type it returns.

  /// @brief Opens; `void (std::error_code)`.
  template <typename Token>
  auto
  async_open (Token &&token)
      -> decltype (asio::async_initiate<Token, void (std::error_code)> (
          std::declval<initiate_open> (), token,
          std::declval<std::shared_ptr<message_impl> &> ()))
  {
    return asio::async_initiate<Token, void (std::error_code)> (
        initiate_open (impl_->strand ()), token, impl_);
  }

  /// @brief Receives one message; `void (std::error_code, message_t)`.
  template <typename Token>
  auto
  async_receive (Token &&token)
      -> decltype (asio::async_initiate<Token,
                                        void (std::error_code, message_t)> (
          std::declval<initiate_receive> (), token,
          std::declval<std::shared_ptr<message_impl> &> ()))
  {
    return asio::async_initiate<Token, void (std::error_code, message_t)> (
        initiate_receive (impl_->strand ()), token, impl_);
  }

  /// @brief Sends one message; `void (std::error_code)`.
  template <typename Token>
  auto
  async_send (message_t message, Token &&token)
      -> decltype (asio::async_initiate<Token, void (std::error_code)> (
          std::declval<initiate_send> (), token,
          std::declval<std::shared_ptr<message_impl> &> (),
          std::declval<message_t> ()))
  {
    return asio::async_initiate<Token, void (std::error_code)> (
        initiate_send (impl_->strand ()), token, impl_, std::move (message));
  }

  /// @brief Ends the pending operations with `cancelled` (any thread).
  void cancel () LUMEX_NOEXCEPT;
  /// @brief Closes (any thread, idempotent).
  void close () LUMEX_NOEXCEPT;

  std::shared_ptr<message_impl> const &
  impl () const LUMEX_NOEXCEPT
  {
    return impl_;
  }

private:
  std::shared_ptr<message_impl> impl_;
};
} // namespace datagram
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_DATAGRAM_MESSAGE_TRANSPORT_HPP
