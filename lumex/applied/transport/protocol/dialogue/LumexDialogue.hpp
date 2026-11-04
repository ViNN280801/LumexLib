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
 * replies and the counters; the streams, the keepalive, the watchdog, the edge
 * events, the rate limits, the acceptance by state and the scenarios.
 * @details An exchange (`async_exchange`) writes the request message of an
 * `exchange_t` as one frame and ends with the frame that answers it, matched
 * by the correlation of its row: `by_key` (a frame of the row), `by_reply_key`
 * (a frame of the reply row), `by_order` (such an exchange is alone in flight,
 * nothing else is written meanwhile: with a reader, the next frame the reader
 * calls a part of the reply; without one, the next frame that is no event; a
 * frame of a row with `stream` never reaches the reader of an exchange, so a
 * table whose reply kind names such a row does not compile: use
 * `reply_row`),
 * `custom` (a frame the matcher accepts). A frame goes first to the
 * exchanges with an exact match (`by_key`, `by_reply_key`), in the order
 * they were started, and only then to the `custom` and `by_order` ones: an
 * exact match never loses its reply to an earlier exchange that would take
 * anything. A `by_order` exchange does not take a frame of a row whose late
 * reply is still expected (see below). Frames that answer no exchange are
 * counted: as late replies, unexpected replies, events or unknown frames.
 * A row without a reply ends its exchange when the write ends.
 *
 * A `by_order` exchange with a reader (a reply kind) reads each frame once: a
 * frame that does not decode as the kind goes on as a frame nobody waits for
 * (its subscribers, `on_unknown`, a step of a scenario) and the exchange keeps
 * waiting with its attempts, its acknowledgement and its timer as they were.
 * So a reply of the wrong shape ends in `timeout` (or `retries_exhausted`),
 * not in `unexpected_reply`. A frame that decodes is a part whatever its value
 * (an echo that differs ends with `not_confirmed` or `refused`). An echo kind
 * by order needs the echo to carry the key of the row: a frame without it is
 * no part, and the exchange ends in `timeout`. What can
 * still end an exchange with `unexpected_reply`: a `custom` row with a reader,
 * when its matcher took a frame that is no part; and a reader of one's own
 * that says so with `reply_reading_t::is_part` true. A `by_key` exchange takes
 * only frames of its row and ends with the error of the decoder (`truncated`,
 * `trailing_bytes`, ...) for one of the wrong shape.
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
 * queued ones start (their writes then fail the same way). The reading starts
 * again by itself (`read_restart_ms`, twice as late each time it fails again
 * up to `read_restart_max_ms`) and with the next request, so that frames and
 * streams come back with the link; a frame brings the link state back to
 * `up`. A read of a link that is still open ending with `cancelled` restarts
 * the reading at once the first time, then as after a failure: such a link
 * never spins.
 *
 * Streams. The frames of a `stream` row are watched from its first frame on
 * (or from the reply of the row that enables it): a silence of two intervals
 * counts one gap (`stream_gaps`, warning), once per silence; a late frame
 * within two intervals is none. A lost link ends the watch. Rate limits. A
 * request of a row with `min_interval` or of a protocol with `min_interval_ms`
 * waits until the time since the last request allows it (`rate_limited`; a
 * warning at most once a second of the timer source for the engine, with the
 * number of the requests that went to debug since); it stays in the queue and
 * the requests behind it may go past while it waits, unless they wait
 * themselves; an attempt after a NAK or a timeout waits too and keeps its
 * place in flight. The state. `accepted_in` is checked against the last known
 * state when a request is about to start, after its wait in the queue: it then
 * ends with `protocol_errc::not_accepted_in_state` and nothing is written
 * (`state_refusals`, warning); with no known state it is sent. An attempt
 * after a timeout or a NAK is not checked again. The watchdog
 * (`watchdog_ms`): after that long without any frame, from the start or the
 * last frame, the link is lost (error): the exchanges in flight end with
 * `link_lost`, the transport is closed (`frame_link_impl::drop_connection`),
 * and it watches again once a frame comes: on a link that can connect again,
 * since the default of `drop_connection` closes for good. A `dangerous` row
 * logs each attempt at info.
 *
 * Reply kinds (see `LumexReplyReader.hpp`). The acknowledgement of a
 * `two_phase` row leaves the exchange waiting for its completion with a
 * timeout of its own and no retries; only the first acknowledgement arms that
 * timeout, so a device that keeps acknowledging cannot hold the exchange: it
 * ends with `timeout` the timeout after the first acknowledgement. An
 * `echo_zero_on_error` row fails with `refused` for a zero echo only when
 * the value sent is not zero: writing 0 and getting 0 back succeeds, and a
 * refusal of a write of 0 cannot be told apart from that echo.
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
 * Handlers of the user called by the engine: the policy's `extra_retries`,
 * the matcher of a `custom` row, the subscribers and the handlers of edges and
 * of the link state, the keepalive generator and the predicates of a scenario
 * run on the strand. If one throws, the engine first ends its step (the
 * exchange of a throwing `extra_retries` ends with `retries_exhausted`, a
 * throwing matcher accepts nothing, the run of a throwing predicate ends) and
 * then lets the exception leave the `run ()` of the executor; the dialogue
 * stays usable. One of them may also destroy the dialogue.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_PROTOCOL_DIALOGUE_DIALOGUE_HPP
#define LUMEX_APPLIED_TRANSPORT_PROTOCOL_DIALOGUE_DIALOGUE_HPP

#include <cstddef>
#include <cstdint>
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
#include "lumex/core/protocol/dsl/LumexProtocolScenario.hpp"
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

  struct initiate_scenario
  {
    using executor_type = transport::executor_type;

    explicit initiate_scenario (executor_type const &executor)
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
                ::lumex::core::protocol::dsl::scenario_t scenario,
                protocol_features_t const *features) const
    {
      if (!impl)
        {
          ::lumex::applied::transport::detail::complete_detached (
              std::forward<Handler> (handler),
              make_error_code (error::transport_errc::not_open),
              std::size_t (0));
          return;
        }
      ::lumex::applied::transport::detail::operation_mark_ptr const mark
          = ::lumex::applied::transport::detail::cancellation_mark (handler);
      impl->start_scenario (
          std::move (scenario), features,
          ::lumex::applied::transport::detail::erase_once<sequence_handler_t> (
              std::forward<Handler> (handler), mark),
          mark);
    }

  private:
    executor_type executor_;
  };

  struct initiate_enable_streams
  {
    using executor_type = transport::executor_type;

    explicit initiate_enable_streams (executor_type const &executor)
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
                std::shared_ptr<detail::dialogue_impl> const &impl) const
    {
      if (!impl)
        {
          ::lumex::applied::transport::detail::complete_detached (
              std::forward<Handler> (handler),
              make_error_code (error::transport_errc::not_open),
              std::size_t (0));
          return;
        }
      ::lumex::applied::transport::detail::operation_mark_ptr const mark
          = ::lumex::applied::transport::detail::cancellation_mark (handler);
      impl->start_enable_streams (
          ::lumex::applied::transport::detail::erase_once<sequence_handler_t> (
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

  /**
   * @brief Sends the rows that turn on streams (`enables_stream<R>`), one
   * after the other in table order, each waiting for its reply as its row
   * says; the token gets `void (std::error_code, std::size_t step)`: an empty
   * error and how many rows were sent, or the error and the position of the
   * row that failed (the later ones are not sent).
   * @details A row that answered starts the watch of its stream at once, so a
   * stream that never starts is a gap. Without features or enable rows it
   * ends with no error and 0. A `terminal` or `partial` cancellation on the
   * slot of the handler ends it with `cancelled`; `cancel ()` and `close ()`
   * end it too.
   */
  template <typename Token>
  auto
  async_enable_streams (Token &&token)
      -> decltype (asio::async_initiate<Token, sequence_signature_t> (
          std::declval<initiate_enable_streams> (), token,
          std::declval<std::shared_ptr<detail::dialogue_impl> &> ()))
  {
    return asio::async_initiate<Token, sequence_signature_t> (
        initiate_enable_streams (initiation_executor ()), token, impl_);
  }

  /**
   * @brief Runs the steps of `scenario`; the rows are those of the layer
   * `features` describes (`async_run_scenario<Scenario> (engine, token)` gives
   * both). The token gets `void (std::error_code, std::size_t step)`: an empty
   * error and the number of steps, or the error and the index of the step
   * that failed; `std::errc::invalid_argument` and 0 when a label is missing
   * or given twice.
   * @details How the steps run, on the strand of the dialogue:
   * - `send` writes its message as an exchange of its row (the correlation
   *   and the reply kind of the row, its `timeout` and `retries` in front of
   *   the defaults of the options, the rate limits and `accepted_in` as for
   *   any request) and ends with its reply; a row without a reply ends when
   *   the write ends. An error of the exchange (or `invalid_value` for an
   *   argument that did not fit, `not_supported` for a row the index has not)
   *   fails the step.
   * - An `expect` or a `wait_state` with a time of 0 waits for ever, also on a
   *   lost link: only a cancellation ends it.
   * - `expect` ends with a frame of its row that answers no exchange; the
   *   frame may have come since the step before began, so a frame right
   *   before the reply of a `send` counts. Its time runs out with
   *   `protocol_errc::timeout`. A frame of a `stream` row still counts as an
   *   event and reaches the subscribers; a frame of another row is the reply
   *   of the step (counted in `replies`, no subscriber).
   * - `wait_state` ends when the last known state is the value, at once when
   *   it already is; every value counts, also one that changes right after.
   *   Its time runs out with `protocol_errc::timeout`.
   * - `branch_on_reply` asks its predicate with the last frame of its row in
   *   this run: the frame of a `send` of that row or of its reply row, or of
   *   an `expect`; `unexpected_reply` when there is none, the error of the
   *   decoder when it does not decode. A predicate that throws ends the run
   *   with `std::errc::interrupted` and the exception leaves the `run ()` of
   *   the executor.
   * Several runs may go at once; each frame reaches each `expect`. A run of
   * labels and branches without a wait gives way to the other handlers now
   * and then, and a loop ends with its cancellation. A `terminal` or `partial`
   * cancellation on the slot of the handler ends the run with `cancelled` and
   * the exchange of its step; `cancel ()` and `close ()` end every run with
   * `cancelled`; a later run on a closed dialogue ends with `closed`.
   */
  template <typename Token>
  auto
  async_run_scenario (::lumex::core::protocol::dsl::scenario_t scenario,
                      protocol_features_t const *features, Token &&token)
      -> decltype (asio::async_initiate<Token, sequence_signature_t> (
          std::declval<initiate_scenario> (), token,
          std::declval<std::shared_ptr<detail::dialogue_impl> &> (),
          std::declval<::lumex::core::protocol::dsl::scenario_t> (), features))
  {
    return asio::async_initiate<Token, sequence_signature_t> (
        initiate_scenario (initiation_executor ()), token, impl_,
        std::move (scenario), features);
  }

  /// @brief The executor of the transport (an empty one when moved from).
  executor_type get_executor () const;

  /// @brief The strand every step of the dialogue runs on (not for a
  /// moved-from dialogue).
  strand_type get_strand () const;

  /// @brief The name for logs (`""` when moved from).
  std::string const &name () const;

  /// @brief The index of the protocol layer in use (not for a moved-from
  /// dialogue).
  ::lumex::core::protocol::index::protocol_index_t const &index () const;

  /**
   * @brief Goes on with another layer of the table: the layer identification
   * found (any thread; it acts on the strand, in order with the requests
   * started after it). `index` and `features` (null: none) are those of the
   * layer (`index::protocol_index<L> ()`, `dialogue::features_of<L> ()`), and
   * must outlive the dialogue.
   * @details Frames are from then on matched and described by the new index.
   * What is under way follows by the identifier of the row: the exchanges,
   * queued or in flight, the subscribers, the scenarios and the late replies
   * still expected; an exchange of a row the layer has not ends with
   * `protocol_errc::not_supported`, a subscriber of such a row hears nothing
   * until a layer has it again. The features become those of the layer
   * (keepalive, watchdog, rate limits, state, edges; the four values below
   * follow them only where the application did not set them): a row of both
   * layers keeps the time of its last frame and its last request, and a stream
   * stays watched; its levels stay when it reads the same fields. A keepalive
   * generator stays only when the row of the keepalive keeps its identifier.
   * `max_in_flight`, the default timeout, the default retries and
   * `late_reply_window_ms` stay when the application set them: a value that
   * differs from what the layer in use gives was set; one that equals it, a
   * value the application set to the same number included, takes what the new
   * layer gives. A layer with no default timeout gives the timeout the
   * dialogue was made with. Selecting the layer in use again changes nothing.
   * The codec of the link stays: a layer of another wire form needs a link of
   * its own.
   */
  void
  select_layer (::lumex::core::protocol::index::protocol_index_t const &index,
                protocol_features_t const *features);

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
   * executor. `close ()` and the destruction drop every handler, and a
   * closed dialogue keeps none.
   * @return The subscription; an id of 0 for a row the index of the dialogue
   * has not, an empty handler, a closed or a moved-from dialogue.
   */
  subscription_t subscribe (::lumex::core::protocol::index::row_t const *row,
                            frame_handler_t handler);

  /// @brief Ends `subscription` (any thread); false when it was not one.
  /// A frame being handed over may still reach the handler.
  bool unsubscribe (subscription_t subscription);

  /// @brief Calls `handler` with every frame whose key matches no row of the
  /// index, with the rules of `subscribe` (a closed dialogue ignores the
  /// handler); an empty handler removes it. Such a frame is also logged: at
  /// warning at most once a second of the timer source of the options, with
  /// the number of the frames that went to debug since the last warning;
  /// the others at debug.
  void on_unknown (frame_handler_t handler);

  /**
   * @brief Calls `handler` with every edge of `row` (`edge_event`), on the
   * strand, after the frame was counted; with the rules of `subscribe`.
   * @details An edge is a change between two frames of the row that decode:
   * the first frame after the start or after the link was lost only sets the
   * level. Any frame of the row counts, also a reply. Needs
   * `dialogue_options_t::features`.
   * @return The subscription for `unsubscribe`; an id of 0 as for `subscribe`.
   */
  subscription_t on_edge (::lumex::core::protocol::index::row_t const *row,
                          edge_handler_t handler);

  /**
   * @brief Calls `handler` when the state of the link changes (with the rules
   * of `subscribe`; an empty handler removes it): `link_state::lost` when a
   * read fails or the watchdog ends the link, `link_state::up` with the next
   * frame.
   */
  void on_link_state (link_state_handler_t handler);

  /// @brief False while the link is lost, and when moved from (any thread).
  bool link_is_up () const LUMEX_NOEXCEPT;

  /**
   * @brief Gives the keepalive of the table (`keepalive<Row, PeriodMs>`) what
   * makes its requests (`dialogue::keepalive_from<P> (source)`; any thread);
   * an empty one ends the keepalive of a row with fields.
   * @details Every period, on the strand, the generator makes a request and
   * the engine starts it as an exchange of the row, behind the queue. A row
   * without request fields needs no generator: its keepalive runs from the
   * start. No request starts while the previous one is pending or while the
   * link is lost and closed. An error of the generator skips that request
   * (warning). If the generator throws, the request is skipped, the period
   * goes on and the exception leaves the `run ()` of the executor; it may also
   * destroy the dialogue. `close ()` and the destruction drop it.
   * @return False when the table names no keepalive (no features), the
   * dialogue is closed or moved from.
   */
  bool set_keepalive (keepalive_generator_t generator);

  /**
   * @brief Sets the last known state of the device, for a table without
   * `state_from` or beside it (any thread): `accepted_in` is checked against
   * it from the next request on, and the runs waiting for it go on.
   */
  void set_state (std::int64_t state);

  /// @brief Forgets the last known state (a new connection; any thread):
  /// `accepted_in` then lets every request go.
  void clear_state () LUMEX_NOEXCEPT;

  /// @brief The last known state, 0 when none is known (any thread).
  std::int64_t last_state () const LUMEX_NOEXCEPT;

  /// @brief Whether a state is known: `set_state`, or a frame of the row of
  /// `state_from` (any thread).
  bool state_known () const LUMEX_NOEXCEPT;

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
