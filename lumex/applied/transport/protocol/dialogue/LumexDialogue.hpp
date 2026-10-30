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
 * @file LumexDialogue.hpp
 * @brief `dialogue_engine`: the engine of the exchanges with a device over a
 * `frame_link`: the request queue, the exchanges in flight, the correlation
 * of the replies, the timeouts on the timer seam, the retries, the late
 * replies and the counters.
 * @details An exchange (`async_exchange`) writes the request message of an
 * `exchange_t` as one frame and ends with the frame that answers it, matched
 * by the correlation of its row: `by_key` (a frame of the row), `by_reply_key`
 * (a frame of the reply row), `by_order` (the next frame that is no event;
 * such an exchange is alone in flight, nothing else is written meanwhile),
 * `custom` (a frame the matcher accepts). A frame goes first to the
 * exchanges with an exact match (`by_key`, `by_reply_key`), in the order
 * they were started, and only then to the `custom` and `by_order` ones: an
 * exact match never loses its reply to an earlier exchange that would take
 * anything. A `by_order` exchange does not take a frame of a row whose late
 * reply is still expected (see below). Frames that answer no exchange are
 * counted: as late replies, unexpected replies, events or unknown frames.
 * A row without a reply ends its exchange when the write ends.
 *
 * At most `max_in_flight` exchanges wait for their reply; the others wait in
 * a queue of `queue_bound`, in the order they were started; one more ends at
 * once with `protocol_errc::queue_full`. Each attempt waits
 * `timeout_ms` of the exchange's policy from the start of its write, on the
 * timer source of the options; an attempt that times out is sent again
 * while the policy allows (`retries`, then `extra_retries`). The exchange
 * then ends with `protocol_errc::timeout` when it had one attempt and with
 * `protocol_errc::retries_exhausted` when it had more. A write that fails
 * ends the exchange with its error (a write the link refuses at once, out of
 * memory, with `std::errc::not_enough_memory`); a read of the link that
 * fails ends the exchanges in flight with `protocol_errc::link_lost`, and the
 * queued ones start (their writes then fail the same way). A read of a
 * link that is still open ending with `cancelled` only restarts the
 * reading.
 *
 * Late replies. A frame of the row of an attempt that timed out or was
 * cancelled (`by_key`, `by_reply_key`) that answers no exchange is counted
 * as a late reply and dropped; it is never kept for a later exchange. An
 * exchange of the same row already in flight takes it as its reply (the row
 * cannot tell the two apart), and the reply that follows is then the late
 * one. A `by_order` reply carries no key: after a `by_order` attempt timed
 * out, the next request waits `late_reply_window_ms` and the frames of that
 * window are dropped as late; with the default 0 a late `by_order` reply is
 * taken for the reply of the next request (the known limit).
 *
 * Threads and lifetime. Everything runs on the strand of the link
 * (`get_strand ()`); every member may be called from any thread. The
 * completion of an exchange runs on its handler's associated executor
 * (by default the strand), never inside `async_exchange`, `cancel`, `close`
 * or the advance of a virtual clock, exactly once; the dialogue keeps no work
 * on that executor. `cancel ()` ends every exchange with
 * `transport_errc::cancelled` and keeps the link; `close ()` does the same,
 * closes the link and is final: a later exchange ends with
 * `transport_errc::closed`. Destroying the dialogue (also inside one of its
 * handlers) closes it asynchronously and returns at once: the pending
 * exchanges end with `cancelled`. When the `io_context` is destroyed first,
 * the handlers the dialogue keeps are destroyed without being called, as Asio
 * destroys its own.
 *
 * Per-exchange cancellation: a `cancellation_type::terminal` or `partial`
 * emitted on the slot of an exchange's handler (`asio::cancel_after`,
 * `asio::bind_cancellation_slot`, a coroutine) ends that exchange with
 * `cancelled`, queued or in flight; the other exchanges and the link go on,
 * and a reply that comes later is a late one. A `total` cancellation ends it
 * only while it waits in the queue (nothing was written yet). A signal
 * emitted after the result was handed on ends nothing. As for the
 * transports, start an exchange with `asio::cancel_after` inside the strand
 * of the dialogue: the timer of `cancel_after` runs there.
 *
 * Handlers of the user called by the engine: the policy's `extra_retries` and
 * the matcher of a `custom` row run on the strand. If one throws, the engine
 * first ends its step (the exchange of a throwing `extra_retries` ends with
 * `retries_exhausted`, a throwing matcher accepts nothing) and then lets the
 * exception leave the `run ()` of the executor; the dialogue stays usable.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_PROTOCOL_DIALOGUE_DIALOGUE_HPP
#define LUMEX_APPLIED_TRANSPORT_PROTOCOL_DIALOGUE_DIALOGUE_HPP

#include <memory>
#include <string>
#include <system_error>
#include <utility>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexCountingHandler.hpp"
#include "lumex/applied/transport/detail/LumexErasedHandlers.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/protocol/LumexDialogueTypes.hpp"
#include "lumex/applied/transport/protocol/dialogue/detail/LumexDialogueImpl.hpp"
#include "lumex/applied/transport/protocol/link/LumexFrameLink.hpp"
#include "lumex/core/protocol/index/LumexProtocolIndex.hpp"
#include "lumex/core/protocol/index/LumexProtocolSession.hpp"
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
namespace protocol
{
/// @brief The dialogue engine (see the file description).
class dialogue_engine
{
  // The initiating function reports the strand (the system executor for a
  // moved-from dialogue), as the transports do.
  struct initiate_exchange
  {
    using executor_type = transport::executor_type;

    explicit initiate_exchange (executor_type const &executor)
        : executor_ (executor)
    {
    }

    executor_type
    get_executor () const LUMEX_NOEXCEPT
    {
      return executor_;
    }

    template <typename Handler>
    void
    operator() (Handler &&handler,
                std::shared_ptr<detail::dialogue_impl> const &impl,
                exchange_t request) const
    {
      if (!impl)
        {
          ::lumex::applied::transport::detail::complete_detached (
              std::forward<Handler> (handler),
              make_error_code (error::transport_errc::not_open),
              reply_frame_t ());
          return;
        }
      ::lumex::applied::transport::detail::operation_mark_ptr const mark
          = ::lumex::applied::transport::detail::cancellation_mark (handler);
      impl->enqueue (
          std::move (request),
          ::lumex::applied::transport::detail::erase_once<exchange_handler_t> (
              std::forward<Handler> (handler), mark),
          mark);
    }

  private:
    executor_type executor_;
  };

public:
  /// @brief The executor type the initiating function reports.
  using executor_type = transport::executor_type;
  /// @brief The strand of the dialogue (the link's).
  using strand_type = frame_link::strand_type;

  /**
   * @brief A dialogue over `link` (not empty; moved in) with the protocol
   * layer of `index` (it must outlive the dialogue: `index::protocol_index
   * <P> ()` lives for the program). Reading begins at once.
   */
  dialogue_engine (
      frame_link link,
      ::lumex::core::protocol::index::protocol_index_t const &index,
      dialogue_options_t const &options = dialogue_options_t ());

  /// @brief Closes (asynchronously) and returns; the pending exchanges end
  /// with `cancelled`.
  ~dialogue_engine ();

  dialogue_engine (dialogue_engine const &) = delete;
  dialogue_engine &operator= (dialogue_engine const &) = delete;
  dialogue_engine (dialogue_engine &&other) LUMEX_NOEXCEPT;
  /// @brief Closes this dialogue (as the destructor), then takes `other`'s.
  dialogue_engine &operator= (dialogue_engine &&other) LUMEX_NOEXCEPT;

  /**
   * @brief Starts an exchange; the token gets `void (std::error_code,
   * reply_frame_t)`: the frame that answered, or the error (`timeout`,
   * `retries_exhausted`, `queue_full`, `link_lost`, `not_supported` for
   * a row the index of the dialogue has not, `std::errc::invalid_argument`
   * for a correlation without its row or matcher, `cancelled`, `closed`, the
   * error of the write).
   */
  template <typename Token>
  auto
  async_exchange (exchange_t request, Token &&token)
      -> decltype (asio::async_initiate<Token, exchange_signature_t> (
          std::declval<initiate_exchange> (), token,
          std::declval<std::shared_ptr<detail::dialogue_impl> &> (),
          std::declval<exchange_t> ()))
  {
    return asio::async_initiate<Token, exchange_signature_t> (
        initiate_exchange (initiation_executor ()), token, impl_,
        std::move (request));
  }

  /// @brief The executor of the transport (an empty one when moved from).
  executor_type get_executor () const;

  /// @brief The strand every step of the dialogue runs on (not for a
  /// moved-from dialogue).
  strand_type get_strand () const;

  /// @brief The name for logs (`""` when moved from).
  std::string const &name () const;

  /// @brief The index of the protocol layer (not for a moved-from
  /// dialogue).
  ::lumex::core::protocol::index::protocol_index_t const &index () const;

  /**
   * @brief The session of the device: what identification learned, read by
   * `extra_retries` and the decoders (not for a moved-from dialogue).
   * @details A plain value: change it while no exchange is pending, or from
   * the strand of the dialogue.
   */
  ::lumex::core::protocol::index::session_context_t &session ();
  ::lumex::core::protocol::index::session_context_t const &session () const;

  /**
   * @brief Calls `handler` with every frame of `row` that answers no
   * exchange (an event of a `stream` row, a report the device sends by
   * itself), on the strand of the dialogue, in the order of the frames.
   * @details Any thread; the subscription acts on the frames that come after
   * the call. A late reply is never handed over. The handler may call the
   * dialogue (also destroy it); if it throws, the other handlers of the
   * frame still run and the exception then leaves the `run ()` of the
   * executor. `close ()` and the destruction drop every handler.
   * @return The subscription; an id of 0 for a row the index of the dialogue
   * has not, an empty handler or a moved-from dialogue.
   */
  subscription_t subscribe (::lumex::core::protocol::index::row_t const *row,
                            frame_handler_t handler);

  /// @brief Ends `subscription` (any thread); false when it was not one.
  /// A frame being handed over may still reach the handler.
  bool unsubscribe (subscription_t subscription);

  /// @brief Calls `handler` with every frame whose key matches no row of the
  /// index (also logged at warning), with the rules of `subscribe`; an empty
  /// handler removes it.
  void on_unknown (frame_handler_t handler);

  /// @brief A snapshot of the counters (any thread; zeros when moved from).
  dialogue_statistics_t statistics () const LUMEX_NOEXCEPT;

  /// @brief Sets every counter to zero (approximate during an exchange).
  void reset_statistics () LUMEX_NOEXCEPT;

  /// @brief Ends every queued and pending exchange with `cancelled`; the
  /// link stays open and the dialogue usable (any thread).
  void cancel () LUMEX_NOEXCEPT;

  /// @brief Ends every exchange with `cancelled` and closes the link, for
  /// good (any thread, idempotent).
  void close () LUMEX_NOEXCEPT;

private:
  executor_type initiation_executor () const;
  void release () LUMEX_NOEXCEPT;

  std::shared_ptr<detail::dialogue_impl> impl_;
};
} // namespace protocol
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_PROTOCOL_DIALOGUE_DIALOGUE_HPP
