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
 * implementation (`message_impl`) runs every operation on its strand.
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
typedef asio::any_completion_handler<void (std::error_code, message_t)>
    message_handler_t;

/// @brief The result of a synchronous receive.
typedef lumex::core::expected::result::expected<message_t, std::error_code>
    message_result_t;

struct message_impl_access;

/// @brief The implementation behind a `message_transport` (the rules of
/// `stream::stream_impl`).
class message_impl : public std::enable_shared_from_this<message_impl>
{
public:
  typedef asio::strand<executor_type> strand_type;

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

  void start_open (status_handler_t handler);
  void start_receive (message_handler_t handler);
  void start_send (message_t message, status_handler_t handler);
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

  executor_type executor_;
  strand_type strand_;
  std::string name_;
  std::atomic<bool> open_;
};

/// @brief A message transport (see the file description).
class message_transport
{
  struct initiate_open
  {
    template <typename Handler>
    void
    operator() (Handler &&handler, std::shared_ptr<message_impl> impl) const
    {
      impl->start_open (status_handler_t (std::forward<Handler> (handler)));
    }
  };

  struct initiate_receive
  {
    template <typename Handler>
    void
    operator() (Handler &&handler, std::shared_ptr<message_impl> impl) const
    {
      impl->start_receive (
          message_handler_t (std::forward<Handler> (handler)));
    }
  };

  struct initiate_send
  {
    template <typename Handler>
    void
    operator() (Handler &&handler, std::shared_ptr<message_impl> impl,
                message_t message) const
    {
      impl->start_send (std::move (message),
                        status_handler_t (std::forward<Handler> (handler)));
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

  /// @brief Opens within the deadline.
  status_result_t open (deadline_t deadline);
  /// @brief Receives one message within the deadline.
  message_result_t receive (deadline_t deadline);
  /// @brief Sends one message within the deadline.
  status_result_t send (message_t const &message, deadline_t deadline);

  /// @brief Opens; `void (std::error_code)`.
  template <typename Token>
  auto
  async_open (Token &&token)
      -> decltype (asio::async_initiate<Token, void (std::error_code)> (
          std::declval<initiate_open> (), token,
          std::declval<std::shared_ptr<message_impl>> ()))
  {
    return asio::async_initiate<Token, void (std::error_code)> (
        initiate_open (), token, impl_);
  }

  /// @brief Receives one message; `void (std::error_code, message_t)`.
  template <typename Token>
  auto
  async_receive (Token &&token)
      -> decltype (asio::async_initiate<Token,
                                        void (std::error_code, message_t)> (
          std::declval<initiate_receive> (), token,
          std::declval<std::shared_ptr<message_impl>> ()))
  {
    return asio::async_initiate<Token, void (std::error_code, message_t)> (
        initiate_receive (), token, impl_);
  }

  /// @brief Sends one message; `void (std::error_code)`.
  template <typename Token>
  auto
  async_send (message_t message, Token &&token)
      -> decltype (asio::async_initiate<Token, void (std::error_code)> (
          std::declval<initiate_send> (), token,
          std::declval<std::shared_ptr<message_impl>> (),
          std::declval<message_t> ()))
  {
    return asio::async_initiate<Token, void (std::error_code)> (
        initiate_send (), token, impl_, std::move (message));
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
