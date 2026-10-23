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
 * @file LumexSessionImpl.hpp
 * @brief `session_impl<Transport>`: the core of a session plus the transport
 * it owns (a `byte_stream` or a `message_transport` made by the factory), and
 * the I/O path of the session fronts.
 * @details Internal to the transport library. The I/O of a session runs on
 * its strand: the step checks that the session is open and starts the
 * operation through the transport's public `async_*` function with an
 * `observed_handler`, a plain (not erased) wrapper of the user's handler.
 * The transport wraps it in its counting handler and erases it once. The
 * observer runs on the session strand (its associated executor), reports the
 * result to the core (a lost link starts reconnecting before the user sees
 * the error) and dispatches the user's handler to the handler's own
 * associated executor; a handler without one runs on the session strand.
 * It keeps no work on that executor (see `LumexBasicSession.hpp`). It holds
 * the core weakly: the transport that keeps it is owned by the core.
 *
 * The other associations (allocator, cancellation slot) of the user's handler
 * are forwarded, except the allocator of an already type-erased handler (an
 * `any_completion_handler` given as the token, also inside a wrapper such as
 * `bind_executor`): it is a view into the erased handler, which the
 * transport's erasure destroys before it frees its own storage (the
 * use-after-free of the reviews of WP12, F1, and WP13, RT_N).
 */
#ifndef LUMEX_APPLIED_TRANSPORT_SESSION_DETAIL_SESSION_IMPL_HPP
#define LUMEX_APPLIED_TRANSPORT_SESSION_DETAIL_SESSION_IMPL_HPP

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <system_error>
#include <type_traits>
#include <utility>

#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/session/LumexSessionTypes.hpp"
#include "lumex/applied/transport/session/detail/LumexSessionCore.hpp"
#include "lumex/applied/transport/session/detail/LumexSessionHandlers.hpp"
#include "lumex/applied/transport/statistics/LumexTransportStatistics.hpp"
#include "lumex/applied/transport/stream/LumexByteStream.hpp"
#include "lumex/applied/transport/stream/LumexStreamTypes.hpp"
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
namespace session
{
namespace detail
{
/// @brief Whether a completion moved data: a send (no value) and a message
/// did, a byte count did when it is not 0.
inline bool
carried_data () LUMEX_NOEXCEPT
{
  return true;
}

inline bool
carried_data (std::size_t bytes) LUMEX_NOEXCEPT
{
  return bytes > 0;
}

template <typename Message>
bool
carried_data (Message const &) LUMEX_NOEXCEPT
{
  return true;
}

/**
 * @brief The handler a session gives the transport for the user's I/O: it
 * reports the result to the core, then dispatches the user's handler.
 */
template <typename Handler> struct observed_handler
{
  // Weak: the transport (owned by the core) keeps this handler while its
  // operation is pending (REVIEW13 L1).
  std::weak_ptr<session_core> core;
  session_core::strand_type strand;
  unsigned generation;
  Handler handler;
  // With a cancellation slot of the user's handler: the session's own
  // signal, whose slot the transport gets (the transport never touches the
  // user's slot), and the mark of the operation (see io_canceller).
  std::shared_ptr<asio::cancellation_signal> signal;
  ::lumex::applied::transport::detail::operation_mark_ptr pending;

  template <typename... Args>
  void
  operator() (std::error_code error, Args &&...args)
  {
    std::shared_ptr<session_core> const alive = core.lock ();
    if (alive)
      alive->note_io_result (generation, error,
                             !error && carried_data (args...));
    pending.reset ();
    asio::dispatch (asio::append (std::move (handler), error,
                                  std::forward<Args> (args)...));
  }
};

/// @brief The step of a session I/O's terminal cancellation, on the session
/// strand (always after the step that started the operation, posted
/// before): the session's signal passes it to the transport, unless the
/// operation completed since (its mark is gone).
struct io_cancel_step
{
  std::shared_ptr<asio::cancellation_signal> signal;
  std::weak_ptr<::lumex::applied::transport::detail::operation_mark> pending;

  void
  operator() ()
  {
    if (!pending.expired ())
      signal->emit (asio::cancellation_type::terminal);
  }
};

/**
 * @brief The handler the session puts in the cancellation slot of the
 * user's I/O handler at the initiation (as the transports do): a terminal
 * cancellation while the operation is pending reaches the transport through
 * the session's own signal, also one emitted before the session started
 * the operation on its transport. `partial` and `total` alone change
 * nothing; once the transport handed the result on it reaches nothing (the
 * transport's own slot handler decides).
 */
struct io_canceller
{
  io_canceller (
      std::weak_ptr<session_core> owner,
      std::shared_ptr<asio::cancellation_signal> own_signal,
      std::weak_ptr<::lumex::applied::transport::detail::operation_mark> mark)
      : core (std::move (owner)), signal (std::move (own_signal)),
        pending (std::move (mark))
  {
  }

  void
  operator() (asio::cancellation_type type) LUMEX_NOEXCEPT
  {
    if ((type & asio::cancellation_type::terminal)
            == asio::cancellation_type::none
        || pending.expired ())
      return;
    std::shared_ptr<session_core> const alive = core.lock ();
    if (!alive)
      return;
    try
      {
        io_cancel_step step = { signal, pending };
        asio::post (alive->strand (), std::move (step));
      }
    catch (...)
      {
        // Out of memory: the operation is not cancelled.
      }
  }

  std::weak_ptr<session_core> core;
  std::shared_ptr<asio::cancellation_signal> signal;
  std::weak_ptr<::lumex::applied::transport::detail::operation_mark> pending;
};

/// @brief `async_read_some` on the current stream.
struct read_some_operation
{
  mutable_bytes_t buffer;

  template <typename Handler>
  void
  start (stream::byte_stream &stream, Handler &&handler)
  {
    stream.async_read_some (buffer, std::forward<Handler> (handler));
  }

  template <typename Handler>
  static void
  fail (session_core::strand_type const &strand, Handler &&handler)
  {
    asio::post (strand, asio::append (
                            std::forward<Handler> (handler),
                            make_error_code (error::transport_errc::not_open),
                            std::size_t (0)));
  }

  template <typename Handler>
  static void
  fail_detached (Handler &&handler)
  {
    complete_detached (std::forward<Handler> (handler),
                       make_error_code (error::transport_errc::not_open),
                       std::size_t (0));
  }
};

/// @brief `async_write_some` on the current stream.
struct write_some_operation
{
  const_bytes_t data;

  template <typename Handler>
  void
  start (stream::byte_stream &stream, Handler &&handler)
  {
    stream.async_write_some (data, std::forward<Handler> (handler));
  }

  template <typename Handler>
  static void
  fail (session_core::strand_type const &strand, Handler &&handler)
  {
    read_some_operation::fail (strand, std::forward<Handler> (handler));
  }

  template <typename Handler>
  static void
  fail_detached (Handler &&handler)
  {
    read_some_operation::fail_detached (std::forward<Handler> (handler));
  }
};

/// @brief `async_write_all` on the current stream.
struct write_all_operation
{
  const_bytes_t data;

  template <typename Handler>
  void
  start (stream::byte_stream &stream, Handler &&handler)
  {
    stream.async_write_all (data, std::forward<Handler> (handler));
  }

  template <typename Handler>
  static void
  fail (session_core::strand_type const &strand, Handler &&handler)
  {
    read_some_operation::fail (strand, std::forward<Handler> (handler));
  }

  template <typename Handler>
  static void
  fail_detached (Handler &&handler)
  {
    read_some_operation::fail_detached (std::forward<Handler> (handler));
  }
};

/// @brief `async_receive` on the current message transport.
struct receive_operation
{
  template <typename Handler>
  void
  start (datagram::message_transport &transport, Handler &&handler)
  {
    transport.async_receive (std::forward<Handler> (handler));
  }

  template <typename Handler>
  static void
  fail (session_core::strand_type const &strand, Handler &&handler)
  {
    asio::post (strand, asio::append (
                            std::forward<Handler> (handler),
                            make_error_code (error::transport_errc::not_open),
                            datagram::message_t ()));
  }

  template <typename Handler>
  static void
  fail_detached (Handler &&handler)
  {
    complete_detached (std::forward<Handler> (handler),
                       make_error_code (error::transport_errc::not_open),
                       datagram::message_t ());
  }
};

/// @brief `async_send` on the current message transport.
struct send_operation
{
  datagram::message_t message;

  template <typename Handler>
  void
  start (datagram::message_transport &transport, Handler &&handler)
  {
    transport.async_send (std::move (message),
                          std::forward<Handler> (handler));
  }

  template <typename Handler>
  static void
  fail (session_core::strand_type const &strand, Handler &&handler)
  {
    asio::post (strand, asio::append (std::forward<Handler> (handler),
                                      make_error_code (
                                          error::transport_errc::not_open)));
  }

  template <typename Handler>
  static void
  fail_detached (Handler &&handler)
  {
    complete_detached (std::forward<Handler> (handler),
                       make_error_code (error::transport_errc::not_open));
  }
};

/// @brief The posted step of an I/O operation of a session.
template <typename Impl, typename Operation, typename Handler> struct io_step
{
  std::shared_ptr<Impl> impl;
  Operation operation;
  Handler handler;
  std::shared_ptr<asio::cancellation_signal> signal;
  ::lumex::applied::transport::detail::operation_mark_ptr pending;

  void
  operator() ()
  {
    impl->run_io (operation, std::move (handler), signal, pending);
  }
};

/**
 * @brief A session over `Transport` (`stream::byte_stream` or
 * `datagram::message_transport`).
 */
template <typename Transport> class session_impl final : public session_core
{
public:
  /// @brief Makes a closed transport on the executor it is given.
  typedef std::function<Transport (executor_type const &)> factory_t;

  session_impl (executor_type const &executor, factory_t factory,
                session_options_t options);
  ~session_impl () override;

  /// @brief The counters of the transport of the last attempt (any thread).
  statistics::statistics_t transport_statistics () const;

  /// @brief Sets them to zero (any thread).
  void reset_transport_statistics ();

  /// @brief Posts `operation` with the user's `handler` to the strand.
  template <typename Operation, typename Handler>
  void
  start_io (Operation operation, Handler &&handler)
  {
    typedef typename std::decay<Handler>::type handler_type;
    // With a cancellation slot, the session's canceller goes in at the
    // initiation: a signal emitted before the operation reached the
    // transport is not lost.
    ::lumex::applied::transport::detail::operation_mark_ptr const mark
        = ::lumex::applied::transport::detail::cancellation_mark (handler);
    std::shared_ptr<asio::cancellation_signal> signal;
    if (mark)
      {
        signal = std::make_shared<asio::cancellation_signal> ();
        asio::get_associated_cancellation_slot (handler)
            .template emplace<io_canceller> (
                std::weak_ptr<session_core> (shared_from_this ()), signal,
                std::weak_ptr<
                    ::lumex::applied::transport::detail::operation_mark> (
                    mark));
      }
    io_step<session_impl, Operation, handler_type> step
        = { std::static_pointer_cast<session_impl> (shared_from_this ()),
            std::move (operation), std::forward<Handler> (handler), signal,
            mark };
    asio::post (strand (), std::move (step));
  }

  /// @brief On the strand: starts `operation` on the current transport, or
  /// completes with `not_open` when the session is not open.
  template <typename Operation, typename Handler>
  void
  run_io (
      Operation &operation, Handler handler,
      std::shared_ptr<asio::cancellation_signal> const &signal,
      ::lumex::applied::transport::detail::operation_mark_ptr const &pending)
  {
    unsigned generation = 0;
    if (!usable (generation) || !current_)
      {
        Operation::fail (strand (), std::move (handler));
        return;
      }
    observed_handler<Handler> observed
        = { shared_from_this (), strand (), generation,
            std::move (handler), signal,    pending };
    operation.start (*current_, std::move (observed));
  }

protected:
  void make_transport () override;
  void open_transport (unsigned generation) override;
  void drop_transport () LUMEX_NOEXCEPT override;
  void cancel_transport () LUMEX_NOEXCEPT override;

private:
  factory_t factory_;
  std::unique_ptr<Transport> current_;
  mutable std::mutex statistics_mutex_;
  std::shared_ptr<statistics::statistics_counters> statistics_;
};

extern template class session_impl<stream::byte_stream>;
extern template class session_impl<datagram::message_transport>;
} // namespace detail
} // namespace session
} // namespace transport
} // namespace applied
} // namespace lumex

namespace asio
{
// Every association of the user's handler goes through the observer, but its
// executor: the observer runs on the session strand.
template <template <typename, typename> class Associator, typename Handler,
          typename DefaultCandidate>
struct associator<
    Associator,
    lumex::applied::transport::session::detail::observed_handler<Handler>,
    DefaultCandidate> : Associator<Handler, DefaultCandidate>
{
  typedef lumex::applied::transport::session::detail::observed_handler<Handler>
      wrapper_type;

  static typename Associator<Handler, DefaultCandidate>::type
  get (wrapper_type const &wrapper) LUMEX_NOEXCEPT
  {
    return Associator<Handler, DefaultCandidate>::get (wrapper.handler);
  }

  static auto
  get (wrapper_type const &wrapper,
       DefaultCandidate const &candidate) LUMEX_NOEXCEPT
      -> decltype (Associator<Handler, DefaultCandidate>::get (wrapper.handler,
                                                               candidate))
  {
    return Associator<Handler, DefaultCandidate>::get (wrapper.handler,
                                                       candidate);
  }
};

template <typename Handler, typename DefaultCandidate>
struct associator<
    associated_executor,
    lumex::applied::transport::session::detail::observed_handler<Handler>,
    DefaultCandidate>
{
  typedef lumex::applied::transport::session::detail::session_core::strand_type
      type;

  static type
  get (lumex::applied::transport::session::detail::observed_handler<
       Handler> const &wrapper) LUMEX_NOEXCEPT
  {
    return wrapper.strand;
  }

  static type
  get (lumex::applied::transport::session::detail::observed_handler<
           Handler> const &wrapper,
       DefaultCandidate const &) LUMEX_NOEXCEPT
  {
    return wrapper.strand;
  }
};

// The cancellation slot is the session's own signal's (or none): the
// transport never touches the user's slot, where the session's canceller
// went at the initiation.
template <typename Handler, typename DefaultCandidate>
struct associator<
    associated_cancellation_slot,
    lumex::applied::transport::session::detail::observed_handler<Handler>,
    DefaultCandidate>
{
  typedef cancellation_slot type;

  static type
  get (lumex::applied::transport::session::detail::observed_handler<
       Handler> const &wrapper) LUMEX_NOEXCEPT
  {
    return wrapper.signal ? wrapper.signal->slot () : cancellation_slot ();
  }

  static type
  get (lumex::applied::transport::session::detail::observed_handler<
           Handler> const &wrapper,
       DefaultCandidate const &) LUMEX_NOEXCEPT
  {
    return wrapper.signal ? wrapper.signal->slot () : cancellation_slot ();
  }
};

// The allocator of the user's handler, unless it is the view allocator of an
// already type-erased handler, at any depth (see the file description): then
// the default candidate.
template <typename Handler, typename DefaultCandidate>
struct associator<
    associated_allocator,
    lumex::applied::transport::session::detail::observed_handler<Handler>,
    DefaultCandidate>
{
  typedef lumex::applied::transport::session::detail::observed_handler<Handler>
      wrapper_type;
  typedef lumex::applied::transport::session::detail::shielded_allocator<
      Handler, DefaultCandidate>
      shield_type;
  typedef typename shield_type::type type;

  static type
  get (wrapper_type const &wrapper) LUMEX_NOEXCEPT
  {
    return shield_type::get (wrapper.handler, DefaultCandidate ());
  }

  static type
  get (wrapper_type const &wrapper,
       DefaultCandidate const &candidate) LUMEX_NOEXCEPT
  {
    return shield_type::get (wrapper.handler, candidate);
  }
};
} // namespace asio

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_SESSION_DETAIL_SESSION_IMPL_HPP
