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
 * @file LumexDialogueTypes.hpp
 * @brief The types of the dialogue engine of `applied/transport/protocol`:
 * the reply frame and the completion of an exchange, the retry policy, the
 * options and the statistics of a `dialogue_engine`, the verdict of a reply
 * reader, the handlers of unsolicited frames, edges and the link state, the
 * run-time description of what a table says (`protocol_features_t`), and
 * `exchange_t`, what one request needs.
 * @details An exchange is one request written to a `frame_link` and the
 * frame that answers it, matched by the correlation of its row
 * (`dialogue::correlation`). The engine works on the run-time index of a
 * protocol layer (`core/protocol`); the generated client fills `exchange_t`
 * from the compile-time traits of the row (`dialogue::row_traits_t`).
 */
#ifndef LUMEX_APPLIED_TRANSPORT_PROTOCOL_DIALOGUE_TYPES_HPP
#define LUMEX_APPLIED_TRANSPORT_PROTOCOL_DIALOGUE_TYPES_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/protocol/dsl/LumexDialogueVocabulary.hpp"
#include "lumex/applied/transport/timing/LumexTimerSource.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolScenario.hpp"
#include "lumex/core/protocol/index/LumexProtocolIndex.hpp"
#include "lumex/core/protocol/index/LumexProtocolSession.hpp"
#include "lumex/core/protocol/layout/LumexProtocolLayoutBase.hpp"

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
/// @brief The frame that answered an exchange: the codec payload, a whole
/// message of the protocol (empty for a row without a reply). The same type
/// as `link_frame_t` of the link underneath.
using reply_frame_t = std::vector<std::uint8_t>;

/// @brief The completion of an exchange.
using exchange_signature_t = void (std::error_code, reply_frame_t);

/// @brief The completion of an exchange as the engine keeps it (erased once).
using exchange_handler_t = asio::any_completion_handler<exchange_signature_t>;

/**
 * @brief More attempts for an exchange whose attempts are used up: called
 * with the session of the dialogue and the error of the last attempt
 * (`protocol_errc::timeout`), it returns how many attempts to add (0 ends
 * the exchange).
 * @details The engine calls it on its strand each time the attempts run out,
 * so it must return 0 at some point (a device that initializes: more
 * attempts while the session says it initializes). It may keep state: the
 * engine calls the one object of its policy, on the strand of the dialogue;
 * one policy shared by dialogues on different strands is called from their
 * threads at once, so its state then needs its own lock. If it throws, the
 * exchange ends as one granted no more attempts (`retries_exhausted`, or
 * `nak` after a NAK) and the exception leaves the `run ()` of the executor.
 */
using extra_retries_t = std::function<std::uint32_t (
    ::lumex::core::protocol::index::session_context_t const &session,
    std::error_code const &last_error)>;

/// @brief How long one attempt of an exchange waits for its reply and how
/// often the request is sent again when it does not come.
struct retry_policy_t
{
  /// The time of one attempt, from the start of its write, in milliseconds;
  /// 0 waits for the reply for ever (until a cancellation).
  std::uint32_t timeout_ms = 1000;
  /// The attempts after the first one that timed out.
  std::uint32_t retries = 0;
  /// More attempts while it says so (may be empty).
  extra_retries_t extra_retries;
};

struct protocol_features_t;

/// @brief The options of a `dialogue_engine`.
struct dialogue_options_t
{
  /// The name for logs (the instance of the log records).
  std::string name = "dialogue";
  /// How many exchanges wait for their reply at once (0 is taken as 1). An
  /// exchange whose row correlates `by_order` is always alone in flight.
  std::size_t max_in_flight = 1;
  /// How many exchanges wait for a place in flight; one more ends at once
  /// with `protocol_errc::queue_full`.
  std::size_t queue_bound = 64;
  /// The policy of an exchange that names none.
  retry_policy_t defaults;
  /// After a `by_order` attempt timed out or was cancelled, the next request
  /// waits this long, and every frame that comes meanwhile is dropped as a
  /// late reply (0: no window, a late reply of a `by_order` row is taken for
  /// the reply of the next one).
  std::uint32_t late_reply_window_ms = 0;
  /// The timers of the engine (the steady source by default; a
  /// `testing::virtual_clock` in tests).
  timing::timer_source timers;
  /// What the table says of the features of the engine: keepalive, watchdog,
  /// rate limits, state, edges (`dialogue::features_of<P> ()`, or everything
  /// the protocol options say at once: `dialogue::options_for<P> ()`); static
  /// data of the program, null without them.
  protocol_features_t const *features = nullptr;
  /// After a read of the link failed, reading starts again this many
  /// milliseconds later, then twice as late each time it fails again up to
  /// `read_restart_max_ms`, so that frames and streams come back with the
  /// link. 0: reading starts again only with the next request.
  std::uint32_t read_restart_ms = 100;
  /// The longest wait between two reads that fail.
  std::uint32_t read_restart_max_ms = 5000;
};

/**
 * @brief The counters of a `dialogue_engine` (a snapshot).
 * @details Each counter is read separately: a snapshot taken during an
 * exchange may show one counter moved and another not yet.
 */
struct dialogue_statistics_t
{
  /// Requests written: every attempt, the retries included.
  std::uint64_t requests = 0;
  /// Frames that answered an exchange (each phase of a two-phase reply is
  /// one) or a step of a scenario that expected them.
  std::uint64_t replies = 0;
  /// Attempts after the first one of an exchange.
  std::uint64_t retries = 0;
  /// Attempts that saw no reply within their timeout.
  std::uint64_t timeouts = 0;
  /// Exchanges that ended with `retries_exhausted`.
  std::uint64_t retries_exhausted = 0;
  /// Negative acknowledgements (`ack_nak`).
  std::uint64_t naks = 0;
  /// Frames of a known row that answered no exchange and were no late reply.
  std::uint64_t unexpected_replies = 0;
  /// Frames dropped as the reply of an attempt that had timed out or was
  /// cancelled.
  std::uint64_t late_replies = 0;
  /// Frames whose key matches no row of the protocol.
  std::uint64_t unknown_frames = 0;
  /// Frames of a row the device sends by itself (a `stream` row) that
  /// answered no exchange.
  std::uint64_t events = 0;
  /// Silences of a watched stream of at least two of its intervals, one per
  /// silence.
  std::uint64_t stream_gaps = 0;
  /// Requests delayed by a minimum interval (each attempt once).
  std::uint64_t rate_limited = 0;
  /// Times the link was opened again.
  std::uint64_t reconnects = 0;
  /// Exchanges refused because the queue was full.
  std::uint64_t queue_refusals = 0;
  /// Exchanges refused before sending: `accepted_in` and the last known state
  /// (`protocol_errc::not_accepted_in_state`).
  std::uint64_t state_refusals = 0;
  /// Edges of `edge_event` seen.
  std::uint64_t edge_events = 0;
  /// Keepalive requests started.
  std::uint64_t keepalives = 0;
  /// Times the watchdog ended the link.
  std::uint64_t watchdog_expiries = 0;
  /// The most exchanges that waited in the queue at once.
  std::size_t max_queued = 0;
};

/// @brief What a reply frame does to its exchange, as the reader of the
/// reply kind of the row says (`reply_reading_t`).
enum class reply_effect : std::uint8_t
{
  /// The exchange ends with the frame.
  complete,
  /// The exchange ends with `reply_reading_t::error` (`refused`,
  /// `not_confirmed`, a decoding error); with `is_part` false a `by_order`
  /// exchange goes on waiting instead.
  fail,
  /// A negative acknowledgement: the attempt is over; the request is sent
  /// again while the policy allows (as after a timeout), else the exchange
  /// ends with `protocol_errc::nak`.
  nak,
  /// The first phase of `two_phase`: the exchange waits for its completion,
  /// with a fresh timeout and without retries (the device has the request).
  /// Only the first acknowledgement arms the timeout: a repeated one leaves
  /// the running timer alone, so the exchange always ends.
  more
};

/// @brief The verdict of a reply reader on one frame.
struct reply_reading_t
{
  /// What the frame does.
  reply_effect effect = reply_effect::complete;
  /// The error of `reply_effect::fail`.
  std::error_code error;
  /// False when the frame is no part of the reply at all (it does not decode
  /// as the reply kind): a `by_order` exchange then leaves it to whoever
  /// else may want it and goes on waiting; any other correlation already knows
  /// the frame is its reply and ends with `error`.
  bool is_part = true;
};

/**
 * @brief Reads a frame that answers an exchange by the reply kind of its row
 * (`dialogue::reply_reader_of<P, Row> ()` gives it): the frame, the session
 * values of the decoders and the request message (for an echo).
 */
using reply_reader_t = reply_reading_t (*) (
    ::lumex::core::protocol::layout::bytes_view_t message,
    ::lumex::core::protocol::field::decode_context_t const *context,
    ::lumex::core::protocol::layout::bytes_view_t request);

/// @brief An edge of a row at run time: reply field `field` goes from `from`
/// to `to` (`dialogue::edge_event<Field, From, To>`).
struct edge_spec_t
{
  /// The index of the field in the reply of the row.
  std::size_t field = 0;
  /// The value before the edge.
  std::int64_t from = 0;
  /// The value after it.
  std::int64_t to = 0;
};

/**
 * @brief Reads integer fields of a reply message: `values[i]` gets field
 * `fields[i]`; false when the message does not decode.
 */
using level_reader_t = bool (*) (
    ::lumex::core::protocol::layout::bytes_view_t message,
    ::lumex::core::protocol::field::decode_context_t const *context,
    std::size_t const *fields, std::size_t count, std::int64_t *values);

/// @brief Appends the message of a request without fields.
using empty_request_t = std::error_code (*) (
    ::lumex::core::protocol::layout::bytes_t &message,
    ::lumex::core::protocol::field::decode_context_t const &context);

/**
 * @brief What the dialogue needs to know of one row at run time, made from
 * the table by `dialogue::features_of<P> ()`.
 */
struct row_features_t
{
  /// The identifier of the row (the engine finds it in its index by it).
  char const *identifier = nullptr;
  /// How its reply is matched.
  dialogue::correlation how = dialogue::correlation::by_key;
  /// The identifier of `reply_row<R>`, else null.
  char const *reply_row = nullptr;
  /// The function of `match_with`, else null.
  dialogue::reply_matcher_t matcher = nullptr;
  /// The reader of its reply kind, else null.
  reply_reader_t reader = nullptr;
  /// `min_interval (ms)`, 0 without it.
  std::uint32_t min_interval_ms = 0;
  /// The edges of the row in table order, null without them.
  edge_spec_t const *edges = nullptr;
  /// How many.
  std::size_t edge_count = 0;
  /// Reads its reply fields; null unless the row has edges or is the row of
  /// `state_from`.
  level_reader_t read_levels = nullptr;
  /// The identifier of the stream row of `enables_stream<R>`, else null.
  char const *enables = nullptr;
  /// The message of a row without request fields, else null.
  empty_request_t empty_request = nullptr;
};

/**
 * @brief What the protocol options and the rows of a layer tell the engine,
 * as static data of the program (`dialogue::features_of<P> ()`).
 * @details Without it (`dialogue_options_t::features` null) the engine does
 * what the index alone allows: gaps of `stream` rows, `accepted_in`, the log
 * of `dangerous` rows.
 */
struct protocol_features_t
{
  /// The rows in table order.
  row_features_t const *rows = nullptr;
  /// How many.
  std::size_t row_count = 0;
  /// The identifier of the row of `keepalive`, else null.
  char const *keepalive_row = nullptr;
  /// Its period in milliseconds.
  std::uint32_t keepalive_period_ms = 0;
  /// `watchdog_ms<Ms>`, 0 without it.
  std::uint32_t watchdog_ms = 0;
  /// `min_interval_ms<Ms>`, 0 without it.
  std::uint32_t min_interval_ms = 0;
  /// The identifier of the row of `state_from`, else null.
  char const *state_row = nullptr;
  /// The field of `state_from`.
  std::size_t state_field = 0;
};

/// @brief Receives a frame that answered no exchange: a frame of a
/// subscribed row (`dialogue_engine::subscribe`), or a frame whose key
/// matches no row (`dialogue_engine::on_unknown`).
using frame_handler_t = std::function<void (reply_frame_t const &frame)>;

/// @brief An edge that happened (`dialogue_engine::on_edge`).
struct edge_t
{
  /// The row of the frame.
  ::lumex::core::protocol::index::row_t const *row = nullptr;
  /// The index of the field in its reply.
  std::size_t field = 0;
  /// The value before the edge.
  std::int64_t from = 0;
  /// The value after it.
  std::int64_t to = 0;
};

/// @brief Receives the edges of a row.
using edge_handler_t = std::function<void (edge_t const &edge)>;

/// @brief What the engine knows of its link.
enum class link_state : std::uint8_t
{
  /// Nothing says it is gone (also before the first frame).
  up,
  /// A read failed or the watchdog ended it; a frame brings it back.
  lost
};

/// @brief Receives the changes of the state of the link; `reason` says why it
/// was lost (empty for `up`).
using link_state_handler_t
    = std::function<void (link_state state, std::error_code const &reason)>;

/**
 * @brief Appends the message of the next keepalive request; the error skips
 * this one. Called on the strand of the dialogue, once per period (the
 * "arguments generator" of the table's `keepalive<Row, PeriodMs>`).
 */
using keepalive_generator_t = ::lumex::core::protocol::dsl::request_encoder_t;

/// @brief The completion of a sequence of steps (a scenario, the enabling of
/// the streams): the error and the step it ended at, the number of steps on
/// success.
using sequence_signature_t = void (std::error_code, std::size_t);

/// @brief The completion of a sequence as the engine keeps it (erased once).
using sequence_handler_t = asio::any_completion_handler<sequence_signature_t>;

/// @brief A subscription of `dialogue_engine::subscribe` (0: none).
struct subscription_t
{
  /// The number of the subscription, 0 when it was refused.
  std::uint64_t id = 0;
};

/**
 * @brief What one request needs: the message, its row and how its reply is
 * recognised.
 * @details The rows are those of the index the dialogue works on (another
 * index of the same layer works too: the engine finds them again by
 * identifier). The correlation decides which frame answers the exchange:
 * `by_key`, a frame of `row`; `by_reply_key`, a frame of `reply_row`;
 * `by_order`, the next frame that is no event and, with a `reader`, a part of
 * the reply (the exchange is then alone in flight); `custom`, a frame
 * `matcher` accepts. A row without a reply (and no `reply_row`, no `matcher`)
 * expects no frame: its exchange ends when the write ends.
 */
struct exchange_t
{
  /// The request message (`layout::encode_message`), written as one frame.
  std::vector<std::uint8_t> message;
  /// The row of the request (`by_key` needs it).
  ::lumex::core::protocol::index::row_t const *row = nullptr;
  /// How the reply is matched (`dialogue::row_traits_t<P, Row>::how ()`).
  dialogue::correlation how = dialogue::correlation::by_key;
  /// The row whose message answers (`by_reply_key` needs it).
  ::lumex::core::protocol::index::row_t const *reply_row = nullptr;
  /// The matcher of a `custom` correlation.
  dialogue::reply_matcher_t matcher = nullptr;
  /// The policy; empty takes `dialogue_options_t::defaults`. One object may
  /// serve many exchanges (its `extra_retries` is called, never copied).
  std::shared_ptr<retry_policy_t const> policy;
  /// The reader of the reply kind of the row
  /// (`dialogue::reply_reader_of<P, Row> ()`); null takes the frame as the
  /// reply as it is.
  reply_reader_t reader = nullptr;
};
} // namespace protocol
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_PROTOCOL_DIALOGUE_TYPES_HPP
