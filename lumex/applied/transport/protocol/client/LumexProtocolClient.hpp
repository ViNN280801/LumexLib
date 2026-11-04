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
 * @file LumexProtocolClient.hpp
 * @brief `client<P>`: the class generated from a protocol table. Every row of
 * layer `P` is a method named by the identifier of the row, in every token
 * style; `identify ()` learns what the device is and selects its layer;
 * `as<L> ()` reaches the rows of the layer it selected.
 * @details The client is a thin layer of templates over `dialogue_engine`: a
 * method instantiates what that one row needs (its request, its reply) and
 * nothing else. The forms of a method, told apart by the number of arguments
 * at compile time:
 * @code
 * client.ROW (args...);                // sync: the row's timeout and retries
 * client.ROW (args..., deadline_t (500));  // sync: at most that long
 * client.ROW (args..., token);         // async: void (error_code,
 * reply_of<Row>) client.ERASE (confirm, args...);     // a dangerous row, in
 * any of the forms
 * @endcode
 * There is one argument per request field. A wrong number of them, a dangerous
 * row without `confirm` and `confirm` on another row do not compile. An
 * argument that does not fit its field (`invalid_value`) sends nothing and
 * ends the call the way any error does. The specification name of a row is
 * what the logs and `index.describe` write; the method carries the identifier,
 * since `X_AS` exists for a name C++ cannot spell. A row named like a member
 * of the class (`close`, `on`, `as`, ...) does not compile: the member would
 * hide the method and the call would reach the member, so the table names the
 * row with another identifier by `X_AS`, keeping the name as its specification
 * name.
 *
 * Results. A sync call returns `call_result_t<Row>`: the value or the error of
 * the exchange (`timeout`, `retries_exhausted`, `refused`, `link_lost`, ...).
 * The error of `would_deadlock` comes at once, with nothing sent, from a
 * thread that runs the executor of the link: the reply would need that very
 * thread. A sync call needs another thread to run the executor; on a stopped
 * `io_context` it ends with `cancelled`, and it blocks as long as no thread
 * has started to run one that never stopped (the deadline form ends in any
 * case). The deadline ends the wait with `protocol_errc::timeout` and cancels
 * the exchange, whose late reply then is a late one; it is wall-clock time
 * whatever the timer source of the options is, while the timeouts and the
 * retries of the row run on that source. An async call completes exactly once,
 * never inside the call, on the associated executor of its handler (by default
 * the strand of the dialogue), and the client keeps no work on that executor.
 * On an error the handler gets a default value, except a reply that was read
 * and refused (`refused`, `not_confirmed`): then the value the device sent.
 * What `dialogue_engine` says of per-exchange cancellation holds for the
 * async form.
 *
 * Layers. The client is typed by one layer `P`. A protocol option
 * `layer_set<L...>` names the layers a device may speak and
 * `identify_with<Row>` the row that tells; `identity_of_t<P>` says how its
 * reply reads. `identify ()` then fills the session (model, firmware, layer,
 * named values) and selects the layer the identity names: frames, logs,
 * events, the features of the table and the decoders from then on follow it.
 * Of the options, `max_in_flight`, the default timeout, the default retries
 * and `late_reply_window_ms` follow it only where they are what the layer in
 * use gives: a value the application changed stays (the options of
 * `client_options_t ()` hold none of the table, so they stay all). A device
 * that names no layer of the set ends `identify ()` with `not_supported` and
 * changes nothing. After that, a method of `P` whose row the identified layer
 * has not (it is another layer's, or the layer replaced it), or which is not
 * in the firmware and the model of the session, answers `not_supported` and
 * sends nothing. A row of another layer goes through `client.template as<L>
 * ()`, which answers `not_supported` until `identify ()` found `L` or a layer
 * derived from it; a layer that is no layer of this client does not compile.
 * Without `layer_set` the identified layer is `P`. The codec belongs to the
 * link the client was given: a layer of another wire form needs a client over
 * a link of its own.
 *
 * Threads and lifetime. Every member may be called from any thread. The
 * session is a value the client replaces as a whole, so `session ()` gives a
 * copy that stays true; a request is encoded and a reply decoded with the
 * session of that moment. A table the index refuses (two rows of one name)
 * answers every call with the error of the index. Destroying the client, also
 * inside one of its handlers and from a handler that owns it, closes the
 * dialogue asynchronously and returns: the calls in flight end with
 * `cancelled`. A handler that owns the client is a cycle until `close ()` or
 * the end of the `io_context`, which break it as they do for
 * `dialogue_engine`. The client is neither copied nor moved. Like every object
 * on a strand, it goes before the `io_context` when nothing else owns it.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_PROTOCOL_CLIENT_PROTOCOL_CLIENT_HPP
#define LUMEX_APPLIED_TRANSPORT_PROTOCOL_CLIENT_PROTOCOL_CLIENT_HPP

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexErasedHandlers.hpp"
#include "lumex/applied/transport/detail/LumexSyncWait.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/log/LumexTransportLog.hpp"
#include "lumex/applied/transport/protocol/LumexDialogueTypes.hpp"
#include "lumex/applied/transport/protocol/client/LumexClientTypes.hpp"
#include "lumex/applied/transport/protocol/dialogue/LumexDialogue.hpp"
#include "lumex/applied/transport/protocol/dialogue/LumexDialogueFeatures.hpp"
#include "lumex/applied/transport/protocol/dialogue/LumexReplyReader.hpp"
#include "lumex/applied/transport/protocol/dsl/LumexDialogueVocabulary.hpp"
#include "lumex/applied/transport/protocol/link/LumexFrameLink.hpp"
#include "lumex/core/expected/Expected"
#include "lumex/core/protocol/dsl/LumexProtocolRow.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolTypeList.hpp"
#include "lumex/core/protocol/error/LumexProtocolErrc.hpp"
#include "lumex/core/protocol/field/LumexProtocolContext.hpp"
#include "lumex/core/protocol/index/LumexProtocolIndex.hpp"
#include "lumex/core/protocol/index/LumexProtocolSession.hpp"
#include "lumex/core/protocol/layout/LumexProtocolCodec.hpp"
#include "lumex/core/protocol/layout/LumexProtocolLayoutBase.hpp"
#include "lumex/core/protocol/layout/LumexProtocolMessage.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/sequence/LumexIndexSequence.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

namespace lumex
{
namespace applied
{
namespace transport
{
namespace protocol
{
template <typename P> class client;
template <typename Owner, typename L> class client_layer;

namespace detail
{
template <typename P> class client_members_t;
}

namespace detail
{
namespace client_dsl = ::lumex::core::protocol::dsl;
namespace client_index = ::lumex::core::protocol::index;
namespace client_layout = ::lumex::core::protocol::layout;
namespace client_field = ::lumex::core::protocol::field;
namespace client_error = ::lumex::core::protocol::error;
namespace client_tdetail = ::lumex::applied::transport::detail;

/// @brief What a call reads of the client at one moment: never changed once
/// made.
struct client_snapshot_t
{
  /// The session of the device.
  client_index::session_context_t session;
  /// The position of the identified layer in the layers of the client, -1
  /// before `identify ()` found one.
  int layer = -1;
};

/// @brief What the client shares with its calls in flight: nothing that keeps
/// the dialogue or a handler alive.
class client_state_t
{
public:
  client_state_t () : current_ (std::make_shared<client_snapshot_t const> ())
  {
  }

  /// @brief What holds now (any thread).
  std::shared_ptr<client_snapshot_t const>
  snapshot () const
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    return current_;
  }

  /// @brief Replaces it (any thread).
  void
  publish (client_index::session_context_t session, int layer)
  {
    std::shared_ptr<client_snapshot_t> made
        = std::make_shared<client_snapshot_t> ();
    made->session = std::move (session);
    made->layer = layer;
    std::lock_guard<std::mutex> const lock (mutex_);
    current_ = std::move (made);
  }

private:
  mutable std::mutex mutex_;
  std::shared_ptr<client_snapshot_t const> current_;
};

/// @brief The dialogue and the state of a client, as a call holds them.
struct client_core_t
{
  std::weak_ptr<dialogue_engine> engine;
  std::shared_ptr<client_state_t> state;

  // The strand the initiating functions report (the system executor when the
  // client is gone).
  executor_type
  executor () const
  {
    std::shared_ptr<dialogue_engine> const live = engine.lock ();
    return live ? executor_type (live->get_strand ())
                : executor_type (asio::system_executor ());
  }
};

/// @brief The layers a client may find, at run time.
struct client_layer_entry_t
{
  char const *name;
  client_index::index_result_t const *index;
  protocol_features_t const *features;
};

template <typename List> struct client_layers_t;

template <typename... L> struct client_layers_t<client_dsl::type_list<L...>>
{
  static client_layer_entry_t const *
  get ()
  {
    static client_layer_entry_t const items[sizeof...(L)]
        = { { L::protocol_name (), &client_index::protocol_index<L> (),
              dialogue::features_of<L> () }... };
    return items;
  }

  // The layer at `at` is `Base` or derived from it.
  template <typename Base>
  static bool
  is_a (int at) LUMEX_NOEXCEPT
  {
    static bool const items[sizeof...(L)]
        = { std::is_base_of<Base, L>::value... };
    return items[at];
  }

  // The layer at `at` has this very row.
  template <typename Row>
  static bool
  has_row (int at) LUMEX_NOEXCEPT
  {
    static bool const items[sizeof...(L)] = { client_dsl::list_contains<
        typename client_dsl::commands_of<L>::type, Row>::value... };
    return items[at];
  }

  static LUMEX_CONSTEXPR int
  count () LUMEX_NOEXCEPT
  {
    return static_cast<int> (sizeof...(L));
  }
};

// Whether `L` can be the layer of a client of `Owner`: its own layer or one it
// derives from, a layer of the set or one those derive from.
template <typename Owner, typename L, typename List> struct layer_reachable_t;

template <typename Owner, typename L, typename... Set>
struct layer_reachable_t<Owner, L, client_dsl::type_list<Set...>>
    : dialogue::detail::all_t<std::is_base_of<L, Owner>::value
                              || !dialogue::detail::all_t<
                                  !std::is_base_of<L, Set>::value...>::value>
{
};

// The row's value as the client gives it.
template <typename Value>
inline client_layout::decode_result_t<Value>
decoded_value (Value value)
{
  return client_layout::decode_result_t<Value> (std::move (value));
}

template <typename Decoded, typename Reply> struct kind_value_of_t;

template <typename Decoded, typename A, typename N>
struct kind_value_of_t<Decoded, dialogue::ack_nak_t<A, N>>
{
  static typename dialogue::ack_nak_t<A, N>::value_type
  get (Decoded const &decoded)
  {
    return decoded.ack;
  }
};

template <typename Decoded, typename A, typename C>
struct kind_value_of_t<Decoded, dialogue::two_phase_t<A, C>>
{
  static typename dialogue::two_phase_t<A, C>::value_type
  get (Decoded const &decoded)
  {
    return decoded.completion;
  }
};

template <typename Decoded, typename T>
struct kind_value_of_t<Decoded, dialogue::echo_set_t<T>>
{
  static typename dialogue::echo_set_t<T>::value_type
  get (Decoded const &decoded)
  {
    return decoded.value;
  }
};

template <typename Decoded, typename T>
struct kind_value_of_t<Decoded, dialogue::echo_zero_on_error_t<T>>
{
  static typename dialogue::echo_zero_on_error_t<T>::value_type
  get (Decoded const &decoded)
  {
    return decoded.value;
  }
};

template <typename Decoded, typename T>
struct kind_value_of_t<Decoded, dialogue::state_instead_of_refusal_t<T>>
{
  static typename dialogue::state_instead_of_refusal_t<T>::value_type
  get (Decoded const &decoded)
  {
    return decoded.value;
  }
};

/// @brief How the frame that answered an exchange of `Row` reads, in layer
/// `L`.
template <typename L, typename Row,
          typename ReplyRow =
              typename dialogue::row_traits_t<L, Row>::reply_row,
          int Form = dialogue::detail::reply_form_t<Row>::value>
struct reply_decoder_t;

// No reply: the exchange ended with the write.
template <typename L, typename Row> struct reply_decoder_t<L, Row, void, 0>
{
  static client_layout::decode_result_t<reply_of<Row>>
  decode (client_layout::bytes_view_t, client_field::decode_context_t const *)
  {
    return decoded_value (reply_of<Row> ());
  }
};

// A field or a layout of the core.
template <typename L, typename Row> struct reply_decoder_t<L, Row, void, 1>
{
  static client_layout::decode_result_t<reply_of<Row>>
  decode (client_layout::bytes_view_t frame,
          client_field::decode_context_t const *context)
  {
    return client_layout::decode_reply_message<L, Row> (frame, context);
  }
};

// A reply kind of the dialogue: the reader already said it completes.
template <typename L, typename Row> struct reply_decoder_t<L, Row, void, 2>
{
  static client_layout::decode_result_t<reply_of<Row>>
  decode (client_layout::bytes_view_t frame,
          client_field::decode_context_t const *context)
  {
    using decoded_type = dialogue::decoded_reply_t<typename Row::reply_type>;
    client_layout::decode_result_t<decoded_type> const decoded
        = dialogue::decode_reply_kind<L, Row> (frame, context);
    if (!decoded)
      return ::lumex::core::expected::error::unexpected<std::error_code> (
          decoded.error ());
    return decoded_value (
        kind_value_of_t<decoded_type, typename Row::reply_type>::get (
            *decoded));
  }
};

// The message of the row of `reply_row<R>`.
template <typename L, typename Row, typename ReplyRow>
struct reply_decoder_t<L, Row, ReplyRow, 0>
{
  static client_layout::decode_result_t<reply_of<Row>>
  decode (client_layout::bytes_view_t frame,
          client_field::decode_context_t const *context)
  {
    return client_layout::decode_reply_message<L, ReplyRow> (frame, context);
  }
};

/**
 * @brief The handler of an exchange of a call: reads the frame into the value
 * of the row and calls the handler of the user, whose every association it
 * forwards. It holds nothing of the dialogue.
 */
template <typename L, typename Row, typename Handler> struct call_handler_t
{
  Handler handler;
  std::shared_ptr<client_state_t> state;

  void
  operator() (std::error_code error, reply_frame_t frame)
  {
    using value_type = reply_of<Row>;
    using decoder = reply_decoder_t<L, Row>;
    if (error && frame.empty ())
      {
        std::move (handler) (error, value_type ());
        return;
      }
    std::shared_ptr<client_snapshot_t const> const snapshot
        = state->snapshot ();
    client_field::decode_context_t const context
        = snapshot->session.decode_context ();
    client_layout::decode_result_t<value_type> decoded = decoder::decode (
        client_layout::bytes_view_t (frame.data (), frame.size ()), &context);
    if (decoded)
      std::move (handler) (error, std::move (*decoded));
    else
      std::move (handler) (error ? error : decoded.error (), value_type ());
  }
};

/// @brief What a synchronous call waits on.
template <typename Value> struct call_wait_t
{
  std::mutex mutex;
  std::condition_variable done_signal;
  bool done = false;
  std::error_code error;
  Value value;
  // Emitted on the strand only, and only after the initiation returned.
  asio::cancellation_signal signal;

  call_wait_t () : value () {}
};

template <typename Value> struct call_wait_handler_t
{
  std::shared_ptr<call_wait_t<Value>> wait;

  void
  operator() (std::error_code error, Value value)
  {
    std::lock_guard<std::mutex> const lock (wait->mutex);
    wait->error = error;
    wait->value = std::move (value);
    wait->done = true;
    wait->done_signal.notify_all ();
  }
};

template <typename Value> struct call_cancel_step_t
{
  std::shared_ptr<call_wait_t<Value>> wait;

  void
  operator() ()
  {
    wait->signal.emit (asio::cancellation_type::terminal);
  }
};

// Whether the `io_context` under `executor` stopped: nothing will complete.
inline bool
executor_stopped (executor_type const &executor) LUMEX_NOEXCEPT
{
  using context_executor = asio::io_context::executor_type;
  if (context_executor const *const context
      = executor.target<context_executor> ())
    return context->context ().stopped ();
  if (asio::strand<context_executor> const *const strand
      = executor.target<asio::strand<context_executor>> ())
    return strand->get_inner_executor ().context ().stopped ();
  return false;
}

// Whether the calling thread is one the reply would need.
inline bool
would_deadlock (dialogue_engine const &engine) LUMEX_NOEXCEPT
{
  return client_tdetail::runs_on (engine.get_executor ())
         || engine.get_strand ().running_in_this_thread ();
}

/**
 * @brief Waits for the call of `wait`: the error (or none, with the value in
 * `wait`). A deadline ends the wait with `timeout` and cancels the exchange; a
 * stopped `io_context` ends it with `cancelled`.
 */
template <typename Value>
std::error_code
wait_for_call (dialogue_engine const &engine,
               std::shared_ptr<call_wait_t<Value>> const &wait,
               deadline_t const *deadline)
{
  using clock_type = std::chrono::steady_clock;
  // How often the wait looks whether the context stopped.
  std::chrono::milliseconds const poll (50);
  clock_type::time_point const started = clock_type::now ();
  executor_type const executor = engine.get_executor ();
  std::unique_lock<std::mutex> lock (wait->mutex);
  for (;;)
    {
      clock_type::duration step = poll;
      if (deadline != nullptr)
        {
          clock_type::duration const left
              = *deadline - (clock_type::now () - started);
          if (left <= clock_type::duration::zero () && !wait->done)
            {
              lock.unlock ();
              call_cancel_step_t<Value> cancel = { wait };
              asio::post (engine.get_strand (), std::move (cancel));
              return client_error::make_error_code (
                  client_error::protocol_errc::timeout);
            }
          if (left < step)
            step = left;
        }
      if (wait->done_signal.wait_for (lock, step,
                                      [&wait] { return wait->done; }))
        return wait->error;
      if (executor_stopped (executor))
        return make_error_code (error::transport_errc::cancelled);
    }
}

template <typename ReplyRow> struct reply_row_of_t
{
  static client_index::row_t const *
  get (client_index::protocol_index_t const &where)
  {
    return where.find_name (ReplyRow::identifier ());
  }
};

template <> struct reply_row_of_t<void>
{
  static client_index::row_t const *
  get (client_index::protocol_index_t const &)
  {
    return nullptr;
  }
};

/// @brief Fills `request` with the message of a call and what the engine
/// needs.
template <typename L, typename Row>
std::error_code
fill_request (exchange_t &request,
              typename client_layout::request_of<Row>::type const &record,
              client_index::row_t const &row,
              client_index::protocol_index_t const &where,
              client_index::session_context_t const &session)
{
  using traits = dialogue::row_traits_t<L, Row>;
  client_field::decode_context_t const context = session.decode_context ();
  client_layout::encode_result_t const encoded
      = client_layout::encode_message_record<L, Row> (request.message, record,
                                                      &context);
  if (!encoded)
    return encoded.error ();
  request.row = &row;
  request.how = traits::how ();
  request.matcher = traits::matcher ();
  request.reader = dialogue::reply_reader_of<L, Row> ();
  request.row_policy = true;
  request.reply_row = reply_row_of_t<typename traits::reply_row>::get (where);
  return std::error_code ();
}

/**
 * @brief The initiating function of a call of `Row` of layer `L` on a client
 * of `Owner`; `Gate`: through `as<L> ()`, which needs the identified layer to
 * be `L`. It reports the strand, as the transports do.
 */
template <typename Owner, typename L, typename Row, bool Gate>
struct initiate_call_t
{
  using executor_type = transport::executor_type;
  using record_result = client_layout::decode_result_t<
      typename client_layout::request_of<Row>::type>;
  using layers
      = client_layers_t<typename dialogue::protocol_traits_t<Owner>::layers>;

  explicit initiate_call_t (executor_type const &executor)
      : executor_ (executor)
  {
  }

  executor_type
  get_executor () const LUMEX_NOEXCEPT
  {
    return executor_;
  }

  static std::error_code
  make (exchange_t &request, record_result const &record,
        client_snapshot_t const &snapshot)
  {
    if (!record)
      return record.error ();
    client_index::index_result_t const &indexed
        = client_index::protocol_index<L> ();
    if (!indexed)
      return indexed.error ().code;
    if ((Gate
         && (snapshot.layer < 0 || !layers::template is_a<L> (snapshot.layer)))
        || (snapshot.layer >= 0
            && !layers::template has_row<Row> (snapshot.layer)))
      return client_error::make_error_code (
          client_error::protocol_errc::not_supported);
    client_index::row_t const *const row
        = (*indexed).find_name (Row::identifier ());
    if (row == nullptr || !snapshot.session.available (*row->entry))
      return client_error::make_error_code (
          client_error::protocol_errc::not_supported);
    return fill_request<L, Row> (request, *record, *row, *indexed,
                                 snapshot.session);
  }

  template <typename Handler>
  void
  operator() (Handler &&handler, std::weak_ptr<dialogue_engine> engine,
              std::shared_ptr<client_state_t> state,
              record_result record) const
  {
    using value_type = reply_of<Row>;
    std::shared_ptr<dialogue_engine> const live = engine.lock ();
    if (!live)
      {
        client_tdetail::complete_detached (
            std::forward<Handler> (handler),
            make_error_code (error::transport_errc::closed), value_type ());
        return;
      }
    std::shared_ptr<client_snapshot_t const> const snapshot
        = state->snapshot ();
    exchange_t request;
    std::error_code const refused = make (request, record, *snapshot);
    if (refused)
      {
        asio::post (live->get_strand (),
                    asio::append (std::forward<Handler> (handler), refused,
                                  value_type ()));
        return;
      }
    call_handler_t<L, Row, typename std::decay<Handler>::type> wrapped
        = { std::forward<Handler> (handler), std::move (state) };
    live->async_exchange (std::move (request), std::move (wrapped));
  }

private:
  executor_type executor_;
};

/// @brief The shape of the arguments of a generated method.
template <typename... Args> struct first_is_confirm_t : std::false_type
{
};

template <typename First, typename... Rest>
struct first_is_confirm_t<First, Rest...>
    : std::is_same<typename std::decay<First>::type, confirm_t>
{
};

template <typename... Args> struct last_of_t
{
  using type = void;
};

template <typename Only> struct last_of_t<Only>
{
  using type = Only;
};

template <typename First, typename Second, typename... Rest>
struct last_of_t<First, Second, Rest...> : last_of_t<Second, Rest...>
{
};

// What may stand last as a token: not a value of a field by mistake.
template <typename T>
struct may_be_token_t
    : std::integral_constant<
          bool, !std::is_void<T>::value && !std::is_arithmetic<T>::value
                    && !std::is_enum<T>::value
                    && (!std::is_pointer<T>::value
                        || std::is_function<
                            typename std::remove_pointer<T>::type>::value)>
{
};

/// @brief What a method does with its arguments.
enum class call_form : int
{
  /// Sync, with the row's timeout.
  sync,
  /// Sync, with a deadline.
  deadline,
  /// Async, with a token.
  token,
  /// A mistake (a `static_assert` of `client_call_t` names it).
  wrong
};

template <typename Row, typename... Args> struct call_shape_t
{
  using last_type =
      typename std::decay<typename last_of_t<Args...>::type>::type;
  using confirmed = first_is_confirm_t<Args...>;
  using dangerous = dialogue::detail::row_dangerous_t<Row>;
  using confirm_right
      = std::integral_constant<bool, confirmed::value == dangerous::value>;
  using offset = std::integral_constant<std::size_t, confirmed::value ? 1 : 0>;
  using given
      = std::integral_constant<std::size_t, sizeof...(Args) - offset::value>;
  using fields = typename client_layout::request_of<Row>::type::size;
  using timed = std::integral_constant<
      bool, given::value == fields::value + 1
                && std::is_convertible<last_type, deadline_t>::value>;
  using tokened
      = std::integral_constant<bool, given::value == fields::value + 1
                                         && !timed::value
                                         && may_be_token_t<last_type>::value>;
  using count_right
      = std::integral_constant<bool, given::value == fields::value
                                         || timed::value || tokened::value>;
  static LUMEX_CONSTEXPR call_form const form
      = !confirm_right::value || !count_right::value ? call_form::wrong
        : timed::value                               ? call_form::deadline
        : tokened::value                             ? call_form::token
                                                     : call_form::sync;
};

/// @brief What a method returns.
template <typename Owner, typename L, typename Row, bool Gate, call_form Form,
          typename... Args>
struct call_return_t
{
  using type = call_result_t<Row>;
};

template <typename Owner, typename L, typename Row, bool Gate,
          typename... Args>
struct call_return_t<Owner, L, Row, Gate, call_form::token, Args...>
{
  using token_type = typename last_of_t<Args...>::type;
  using initiation = initiate_call_t<Owner, L, Row, Gate>;
  using type
      = decltype (asio::async_initiate<token_type,
                                       void (std::error_code, reply_of<Row>)> (
          std::declval<initiation> (), std::declval<token_type &> (),
          std::declval<std::weak_ptr<dialogue_engine>> (),
          std::declval<std::shared_ptr<client_state_t>> (),
          std::declval<typename initiation::record_result> ()));
};

template <typename Owner, typename L, typename Row, bool Gate,
          typename... Args>
using call_return_of =
    typename call_return_t<Owner, L, Row, Gate,
                           call_shape_t<Row, Args...>::form, Args...>::type;

/// @brief A generated method of `Row` of layer `L` on a client of `Owner`.
template <typename Owner, typename L, typename Row, bool Gate>
struct client_call_t
{
  using value_type = reply_of<Row>;
  using signature = void (std::error_code, value_type);
  using initiation = initiate_call_t<Owner, L, Row, Gate>;
  using record_result = typename initiation::record_result;
  using result_type = call_result_t<Row>;

  static result_type
  failed (std::error_code const &error)
  {
    return ::lumex::core::expected::error::unexpected<std::error_code> (error);
  }

  template <std::size_t Offset, typename Tuple, std::size_t... I>
  static record_result
  make_record (Tuple &arguments,
               ::lumex::core::utility::sequence::index_sequence<I...>)
  {
    return client_layout::make_request<Row> (
        std::get<Offset + I> (arguments)...);
  }

  template <typename Handler>
  static void
  start (client_core_t const &core, record_result record, Handler &&handler)
  {
    asio::async_initiate<Handler, signature> (
        initiation (core.executor ()), handler,
        std::weak_ptr<dialogue_engine> (core.engine),
        std::shared_ptr<client_state_t> (core.state), std::move (record));
  }

  static result_type
  run_sync (client_core_t const &core, record_result record,
            deadline_t const *deadline)
  {
    std::shared_ptr<dialogue_engine> const live = core.engine.lock ();
    if (!live)
      return failed (make_error_code (error::transport_errc::closed));
    if (would_deadlock (*live))
      return failed (make_error_code (error::transport_errc::would_deadlock));
    std::shared_ptr<call_wait_t<value_type>> const wait
        = std::make_shared<call_wait_t<value_type>> ();
    call_wait_handler_t<value_type> handler = { wait };
    start (core, std::move (record),
           asio::bind_cancellation_slot (wait->signal.slot (),
                                         std::move (handler)));
    std::error_code const ended = wait_for_call (*live, wait, deadline);
    if (ended)
      return failed (ended);
    // The handler is done: nobody writes the value any more.
    return result_type (std::move (wait->value));
  }

  template <typename Shape, typename Tuple>
  static record_result
  record_of (Tuple &arguments)
  {
    return make_record<Shape::offset::value> (
        arguments, ::lumex::core::utility::sequence::make_index_sequence<
                       Shape::fields::value> ());
  }

  template <typename Shape, typename Tuple>
  static result_type
  dispatch (client_core_t const &core, Tuple &arguments,
            std::integral_constant<call_form, call_form::sync>)
  {
    return run_sync (core, record_of<Shape> (arguments), nullptr);
  }

  template <typename Shape, typename Tuple>
  static result_type
  dispatch (client_core_t const &core, Tuple &arguments,
            std::integral_constant<call_form, call_form::deadline>)
  {
    deadline_t const deadline
        = std::get<std::tuple_size<Tuple>::value - 1> (arguments);
    return run_sync (core, record_of<Shape> (arguments), &deadline);
  }

  template <typename Shape, typename Tuple>
  static result_type
  dispatch (client_core_t const &, Tuple &,
            std::integral_constant<call_form, call_form::wrong>)
  {
    return failed (client_error::make_error_code (
        client_error::protocol_errc::not_supported));
  }

  template <typename Shape, typename Token, typename Tuple>
  static auto
  dispatch_token (client_core_t const &core, Tuple &arguments, Token &&token)
      -> decltype (asio::async_initiate<Token, signature> (
          std::declval<initiation> (), token,
          std::declval<std::weak_ptr<dialogue_engine>> (),
          std::declval<std::shared_ptr<client_state_t>> (),
          std::declval<record_result> ()))
  {
    return asio::async_initiate<Token, signature> (
        initiation (core.executor ()), token,
        std::weak_ptr<dialogue_engine> (core.engine),
        std::shared_ptr<client_state_t> (core.state),
        record_of<Shape> (arguments));
  }

  template <typename... Args>
  static typename std::enable_if<
      call_shape_t<Row, Args...>::form != call_form::token, result_type>::type
  invoke (client_core_t const &core, Args &&...args)
  {
    check<Args...> ();
    using shape = call_shape_t<Row, Args...>;
    std::tuple<Args &&...> arguments (std::forward<Args> (args)...);
    return dispatch<shape> (core, arguments,
                            std::integral_constant<call_form, shape::form> ());
  }

  template <typename... Args>
  static typename std::enable_if<
      call_shape_t<Row, Args...>::form == call_form::token,
      call_return_of<Owner, L, Row, Gate, Args...>>::type
  invoke (client_core_t const &core, Args &&...args)
  {
    check<Args...> ();
    using shape = call_shape_t<Row, Args...>;
    using token_type = typename last_of_t<Args...>::type;
    std::tuple<Args &&...> arguments (std::forward<Args> (args)...);
    return dispatch_token<shape, token_type> (
        core, arguments,
        std::forward<token_type> (std::get<sizeof...(Args) - 1> (arguments)));
  }

  template <typename... Args>
  static void
  check ()
  {
    using shape = call_shape_t<Row, Args...>;
    static_assert (
        client_dsl::list_contains<typename client_dsl::commands_of<L>::type,
                                  Row>::value,
        "LUMEX_PROTOCOL: client: the row is no row of the layer of "
        "the client: a row of another layer goes through "
        "client.template as<Layer> ()");
    static_assert (!std::is_void<value_type>::value,
                   "LUMEX_PROTOCOL: client: the reply of the row is a reply "
                   "kind of another layer: the client cannot return it");
    static_assert (shape::confirmed::value || !shape::dangerous::value,
                   "LUMEX_PROTOCOL: client: a dangerous row needs confirm as "
                   "its first argument");
    static_assert (!shape::confirmed::value || shape::dangerous::value,
                   "LUMEX_PROTOCOL: client: confirm on a row that is not "
                   "dangerous");
    static_assert (shape::count_right::value,
                   "LUMEX_PROTOCOL: client: give one argument per request "
                   "field of the row, then at most a deadline_t or a "
                   "completion token");
  }
};

/// @brief The handler `on<Row>` gives the dialogue.
template <typename L, typename Row, typename Handler> struct event_handler_t
{
  Handler handler;
  std::shared_ptr<client_state_t> state;

  void
  operator() (reply_frame_t const &frame)
  {
    std::shared_ptr<client_snapshot_t const> const snapshot
        = state->snapshot ();
    client_field::decode_context_t const context
        = snapshot->session.decode_context ();
    client_layout::decode_result_t<
        typename client_layout::reply_message_of<Row>::type> const decoded
        = client_layout::decode_reply_message<L, Row> (
            client_layout::bytes_view_t (frame.data (), frame.size ()),
            &context);
    if (decoded)
      handler (*decoded);
    else
      LUMEX_TRANSPORT_LOG (
          warning, "transport.client", L::protocol_name (),
          std::string ("a frame of ") + Row::specification_name ()
              + " does not decode: " + decoded.error ().message (),
          log::log_bytes_t ());
  }
};

// `on<Row>`, for the client and for a layer of it.
template <typename L, typename Row, typename Handler>
subscription_t
subscribe_row (client_core_t const &core, Handler &&handler)
{
  static_assert (
      client_dsl::list_contains<typename client_dsl::commands_of<L>::type,
                                Row>::value,
      "LUMEX_PROTOCOL: client: on<Row>: the row is no row of the "
      "layer of the client");
  static_assert (dialogue::detail::reply_form_t<Row>::value != 2,
                 "LUMEX_PROTOCOL: client: on<Row>: a row whose reply is a "
                 "reply kind is no event: subscribe the row of its message");
  static_assert (
      std::is_void<typename dialogue::row_traits_t<L, Row>::reply_row>::value,
      "LUMEX_PROTOCOL: client: on<Row>: a row with reply_row<R> sends no "
      "message of its own: subscribe R");
  std::shared_ptr<dialogue_engine> const live = core.engine.lock ();
  if (!live)
    return subscription_t ();
  client_index::index_result_t const &indexed
      = client_index::protocol_index<L> ();
  if (!indexed)
    return subscription_t ();
  event_handler_t<L, Row, typename std::decay<Handler>::type> made
      = { std::forward<Handler> (handler), core.state };
  return live->subscribe ((*indexed).find_name (Row::identifier ()),
                          frame_handler_t (std::move (made)));
}

// What the reply of the row of `identify_with` teaches, on the strand of the
// dialogue: the session and the layer; nothing changes on an error.
template <typename Owner>
std::error_code
apply_identity (std::weak_ptr<dialogue_engine> const &engine,
                client_state_t &state, reply_frame_t const &frame,
                identity_t &identity)
{
  using traits = dialogue::protocol_traits_t<Owner>;
  using row_type = typename traits::identify_row;
  using layers = client_layers_t<typename traits::layers>;
  using named_set = std::integral_constant<
      bool, !std::is_void<typename dialogue::detail::protocol_option_t<
                Owner, dialogue::detail::layer_set_kind_t>::type>::value>;
  std::shared_ptr<client_snapshot_t const> const snapshot = state.snapshot ();
  client_field::decode_context_t const context
      = snapshot->session.decode_context ();
  client_layout::decode_result_t<reply_of<row_type>> const decoded
      = reply_decoder_t<Owner, row_type>::decode (
          client_layout::bytes_view_t (frame.data (), frame.size ()),
          &context);
  if (!decoded)
    return decoded.error ();
  std::error_code const read = identity_of_t<Owner>::read (*decoded, identity);
  if (read)
    return std::error_code (read);
  int at = -1;
  if (named_set::value)
    {
      for (int i = 0; i < layers::count () && at < 0; ++i)
        if (identity.layer == layers::get ()[i].name)
          at = i;
      if (at < 0)
        return client_error::make_error_code (
            client_error::protocol_errc::not_supported);
    }
  else
    {
      at = 0;
      identity.layer = Owner::protocol_name ();
    }
  client_layer_entry_t const &entry = layers::get ()[at];
  if (!*entry.index)
    return entry.index->error ().code;
  std::shared_ptr<dialogue_engine> const live = engine.lock ();
  if (!live)
    return make_error_code (error::transport_errc::cancelled);
  client_index::session_context_t next = snapshot->session;
  next.set_model (identity.model);
  next.set_firmware (identity.firmware);
  next.set_layer (identity.layer);
  for (std::size_t i = 0; i < identity.values.size (); ++i)
    next.set (identity.values[i].first, identity.values[i].second);
  live->session () = next;
  live->select_layer (**entry.index, entry.features);
  state.publish (std::move (next), at);
  return std::error_code ();
}

/// @brief The end of the exchange of `identify ()`, on the strand of the
/// dialogue; the handler of the user then runs on its own executor.
template <typename Owner, typename Handler> struct identify_step_t
{
  Handler handler;
  std::weak_ptr<dialogue_engine> engine;
  std::shared_ptr<client_state_t> state;
  dialogue_engine::strand_type strand;

  void
  operator() (std::error_code error, reply_frame_t frame)
  {
    identity_t identity;
    if (!error)
      error = apply_identity<Owner> (engine, *state, frame, identity);
    asio::post (strand, asio::append (std::move (handler), error,
                                      std::move (identity)));
  }
};

/// @brief The initiating function of `identify ()`.
template <typename Owner> struct initiate_identify_t
{
  using executor_type = transport::executor_type;
  using row_type = typename dialogue::protocol_traits_t<Owner>::identify_row;

  explicit initiate_identify_t (executor_type const &executor)
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
  operator() (Handler &&handler, std::weak_ptr<dialogue_engine> engine,
              std::shared_ptr<client_state_t> state) const
  {
    std::shared_ptr<dialogue_engine> const live = engine.lock ();
    if (!live)
      {
        client_tdetail::complete_detached (
            std::forward<Handler> (handler),
            make_error_code (error::transport_errc::closed), identity_t ());
        return;
      }
    client_index::index_result_t const &indexed
        = client_index::protocol_index<Owner> ();
    exchange_t request;
    std::error_code refused;
    if (!indexed)
      refused = indexed.error ().code;
    else
      {
        // Whatever the identified layer says: this row finds it out again.
        std::shared_ptr<client_snapshot_t const> const snapshot
            = state->snapshot ();
        refused = fill_request<Owner, row_type> (
            request, typename client_layout::request_of<row_type>::type (),
            *(*indexed).find_name (row_type::identifier ()), *indexed,
            snapshot->session);
      }
    if (refused)
      {
        asio::post (live->get_strand (),
                    asio::append (std::forward<Handler> (handler), refused,
                                  identity_t ()));
        return;
      }
    // The slot of the user's handler still ends the exchange; the step has no
    // executor of its own, so it runs on the strand, as what it changes asks.
    auto const slot = asio::get_associated_cancellation_slot (handler);
    identify_step_t<Owner, typename std::decay<Handler>::type> step
        = { std::forward<Handler> (handler), std::move (engine),
            std::move (state), live->get_strand () };
    live->async_exchange (std::move (request), asio::bind_cancellation_slot (
                                                   slot, std::move (step)));
  }

private:
  executor_type executor_;
};

/// @brief `set_session` on the strand.
struct set_session_step_t
{
  std::weak_ptr<dialogue_engine> engine;
  client_index::session_context_t session;

  void
  operator() ()
  {
    std::shared_ptr<dialogue_engine> const live = engine.lock ();
    if (live)
      live->session () = std::move (session);
  }
};

/// @brief What the checks of `identify ()` need of a table.
template <typename Owner> struct identify_check_t
{
  using row_type = typename dialogue::protocol_traits_t<Owner>::identify_row;

  template <typename T, typename = void> struct no_reader_t : std::false_type
  {
  };

  template <typename T>
  struct no_reader_t<T,
                     typename dialogue::detail::voider_t<
                         typename identity_of_t<T>::lumex_no_identity>::type>
      : std::true_type
  {
  };

  static_assert (!std::is_void<row_type>::value,
                 "LUMEX_PROTOCOL: client: identify (): the protocol names no "
                 "identify_with<Row>");
  static_assert (!no_reader_t<Owner>::value,
                 "LUMEX_PROTOCOL: client: identify (): specialize "
                 "identity_of_t<P> for the layer or a base of it: it says how "
                 "the reply of identify_with<Row> reads as an identity_t");

  template <typename Row, bool Fields> struct no_fields_t : std::true_type
  {
    static_assert (client_layout::request_of<Row>::type::size::value == 0,
                   "LUMEX_PROTOCOL: client: identify (): the row of "
                   "identify_with<Row> has no request fields: identify () "
                   "takes no arguments");
  };

  template <typename Row> struct no_fields_t<Row, false> : std::true_type
  {
  };

  static LUMEX_CONSTEXPR bool const value
      = no_fields_t<row_type, !std::is_void<row_type>::value>::value;
};
/// @brief Why a table cannot name a row so: the member hides its method.
/// @details A client has methods named by the rows through a base class, and a
/// member of the same name in a class derived from it hides them (the call
/// reaches the member). The members of a client are all declared in one class
/// (`client_members_t`, `client_layer_members_t`), so every name they declare,
/// of any kind (a function, an overloaded one, a template, a type, a data
/// member, private ones too), is found by the lookup of the row's identifier
/// in a class derived from them and from a class that declares only that
/// identifier: it is ambiguous there (`lumex_name_free_in_t` of the row).
template <typename Row, typename Own> struct client_row_name_t
{
  static_assert (
      Row::template lumex_name_free_in_t<Own>::value,
      "LUMEX_PROTOCOL: client<P>: a row is named like a member of client<P> "
      "(the Row of this instance), which would hide the method of the row: "
      "give the row another identifier with X_AS (identifier, "
      "\"specification name\", ...); the specification name may stay");
  static LUMEX_CONSTEXPR bool const value = true;
};

template <typename Row, typename Own> struct layer_row_name_t
{
  static_assert (
      Row::template lumex_name_free_in_t<Own>::value,
      "LUMEX_PROTOCOL: client_layer<Owner, L>: a row is named like a member "
      "of client_layer<Owner, L> (the Row of this instance), which would hide "
      "the method of the row: give the row another identifier with X_AS "
      "(identifier, \"specification name\", ...); the specification name may "
      "stay");
  static LUMEX_CONSTEXPR bool const value = true;
};

template <template <typename, typename> class Check, typename Own,
          typename List>
struct rows_named_free_t;

template <template <typename, typename> class Check, typename Own,
          typename... Rows>
struct rows_named_free_t<Check, Own, client_dsl::type_list<Rows...>>
    : dialogue::detail::all_t<Check<Rows, Own>::value...>
{
};

/// @brief The names a class declares apart from its members: its own name.
struct client_class_names_t
{
  using client = void;
};

struct client_layer_class_names_t
{
  using client_layer = void;
};

/// @brief Every member of `client_layer<Owner, L>` apart from the methods of
/// the rows.
template <typename Owner, typename L> class client_layer_members_t
{
public:
  /// @brief What a generated method calls.
  template <typename Row, typename... Args>
  auto
  lumex_invoke (Args &&...args) -> call_return_of<Owner, L, Row, true, Args...>
  {
    return client_call_t<Owner, L, Row, true>::invoke (
        core_, std::forward<Args> (args)...);
  }

  /// @brief `client::on<Row>` for a row of this layer: an id of 0 until the
  /// layer in use has the row.
  template <typename Row, typename Handler>
  subscription_t
  on (Handler &&handler)
  {
    return subscribe_row<L, Row> (core_, std::forward<Handler> (handler));
  }

protected:
  explicit client_layer_members_t (client_core_t core)
      : core_ (std::move (core))
  {
  }

private:
  client_core_t core_;
};

/// @brief The names of a layer of a client that a row may not take.
template <typename Owner, typename L>
struct client_layer_names_t : client_layer_members_t<Owner, L>,
                              client_layer_class_names_t
{
};

} // namespace detail

/**
 * @brief The rows of layer `L` on a client of `Owner` (see the file
 * description): what `client.template as<L> ()` gives. A small value that may
 * be kept: after the client is gone its calls end with `closed`.
 */
template <typename Owner, typename L>
class client_layer : public detail::client_layer_members_t<Owner, L>,
                     public L::template lumex_methods<client_layer<Owner, L>>
{
  static_assert (
      detail::rows_named_free_t<
          detail::layer_row_name_t, detail::client_layer_names_t<Owner, L>,
          typename detail::client_dsl::commands_of<L>::type>::value,
      "LUMEX_PROTOCOL: client_layer<Owner, L>: no row is named like a member");

private:
  friend class detail::client_members_t<Owner>;

  explicit client_layer (detail::client_core_t core)
      : detail::client_layer_members_t<Owner, L> (std::move (core))
  {
  }
};

namespace detail
{
/// @brief Every member of `client<P>` apart from the methods of the rows.
template <typename P> class client_members_t
{
  static_assert (::lumex::core::protocol::dsl::is_protocol<P>::value,
                 "LUMEX_PROTOCOL: client<P>: P is a LUMEX_PROTOCOL_DEFINE or "
                 "LUMEX_PROTOCOL_EXTEND type");
  static_assert (dialogue::check_protocol_t<P>::value,
                 "LUMEX_PROTOCOL: client<P>: the protocol is valid for the "
                 "dialogue");

  using traits = dialogue::protocol_traits_t<P>;
  using identify_signature = void (std::error_code, identity_t);

public:
  /// @brief The executor type the initiating functions report.
  using executor_type = transport::executor_type;
  /// @brief The strand of the dialogue.
  using strand_type = dialogue_engine::strand_type;
  /// @brief The layer the client is typed by.
  using protocol_type = P;

  /**
   * @brief A client over `link` (not empty; moved in). Reading begins at once.
   * The default options are what the table says; start from
   * `client_options_for<P> ()` to change some (the name, the timers, ...).
   */
  client_members_t (frame_link link, client_options_t options)
      : state_ (std::make_shared<client_state_t> ())
  {
    namespace index = ::lumex::core::protocol::index;
    static index::protocol_index_t const nothing;
    index::index_result_t const &indexed = index::protocol_index<P> ();
    if (options.dialogue.features == nullptr)
      options.dialogue.features = dialogue::features_of<P> ();
    engine_ = std::make_shared<dialogue_engine> (
        std::move (link), indexed ? *indexed : nothing, options.dialogue);
  }

  client_members_t (client_members_t const &) = delete;
  client_members_t &operator= (client_members_t const &) = delete;

  /// @brief Closes (asynchronously) and returns; the calls in flight end with
  /// `cancelled`.
  ~client_members_t () = default;

  /// @brief What a generated method calls: `client.ROW (args...)`.
  template <typename Row, typename... Args>
  auto
  lumex_invoke (Args &&...args)
      -> detail::call_return_of<P, P, Row, false, Args...>
  {
    return detail::client_call_t<P, P, Row, false>::invoke (
        core (), std::forward<Args> (args)...);
  }

  /**
   * @brief The rows of layer `L`: it answers `not_supported` until `identify
   * ()` found `L` or a layer derived from it. A layer that is neither that of
   * the client, nor one of `layer_set<...>`, nor a base of those does not
   * compile.
   */
  template <typename L>
  client_layer<P, L>
  as ()
  {
    static_assert (
        detail::layer_reachable_t<P, L, typename traits::layers>::value,
        "LUMEX_PROTOCOL: client: as<L> (): L is no layer of this client: "
        "neither its own layer, nor a layer of layer_set<...>, nor a base of "
        "one");
    return client_layer<P, L> (core ());
  }

  /**
   * @brief Calls `handler` (`void (M const &)`, `M` the value of the message
   * of the row) with every frame of `Row` that answers no call, on the strand;
   * with the rules of `dialogue_engine::subscribe`. A frame that does not
   * decode is logged and dropped.
   * @return The subscription for `unsubscribe`; an id of 0 when refused.
   */
  template <typename Row, typename Handler>
  subscription_t
  on (Handler &&handler)
  {
    return detail::subscribe_row<P, Row> (core (),
                                          std::forward<Handler> (handler));
  }

  /// @brief Ends a subscription (any thread); false when it was none.
  bool
  unsubscribe (subscription_t subscription)
  {
    return engine_->unsubscribe (subscription);
  }

  /// @brief Calls `handler` with every frame whose key matches no row
  /// (`dialogue_engine::on_unknown`).
  template <typename Handler>
  void
  on_unknown (Handler &&handler)
  {
    engine_->on_unknown (frame_handler_t (std::forward<Handler> (handler)));
  }

  /**
   * @brief Sends the row of `identify_with`, fills the session and selects the
   * layer the device names; the token gets `void (std::error_code,
   * identity_t)`, the handler sees the new session and layer.
   * @details The errors are those of the exchange and the decoder, the error
   * of `identity_of_t<P>::read`, `not_supported` for a device that names no
   * layer of `layer_set<...>` (with what it said in the identity); on an error
   * the session and the layer stay. The row needs no request fields and has a
   * reply of the core, which the table and the client check at compile time.
   */
  template <typename Token>
  auto
  async_identify (Token &&token)
      -> decltype (asio::async_initiate<Token, identify_signature> (
          std::declval<detail::initiate_identify_t<P>> (), token,
          std::declval<std::weak_ptr<dialogue_engine>> (),
          std::declval<std::shared_ptr<detail::client_state_t>> ()))
  {
    static_assert (detail::identify_check_t<P>::value,
                   "LUMEX_PROTOCOL: client: identify (): checked");
    return asio::async_initiate<Token, identify_signature> (
        detail::initiate_identify_t<P> (core ().executor ()), token,
        std::weak_ptr<dialogue_engine> (engine_),
        std::shared_ptr<detail::client_state_t> (state_));
  }

  /// @brief `async_identify` as a synchronous call: the rules of a generated
  /// method without a deadline.
  identity_result_t
  identify ()
  {
    return identify_sync (nullptr);
  }

  /// @brief `async_identify` as a synchronous call with a deadline.
  identity_result_t
  identify (deadline_t deadline)
  {
    return identify_sync (&deadline);
  }

  /// @brief Sends the rows that turn on streams
  /// (`dialogue_engine::async_enable_streams`).
  template <typename Token>
  auto
  async_enable_streams (Token &&token)
      -> decltype (std::declval<dialogue_engine &> ().async_enable_streams (
          std::forward<Token> (token)))
  {
    return engine_->async_enable_streams (std::forward<Token> (token));
  }

  /// @brief The session as it is now: a copy (any thread).
  ::lumex::core::protocol::index::session_context_t
  session () const
  {
    return state_->snapshot ()->session;
  }

  /**
   * @brief Replaces the session (any thread), for what the application knows
   * of the device: requests started after the call use it. The layer stays
   * what `identify ()` said; `identify ()` keeps the values it does not name.
   */
  void
  set_session (::lumex::core::protocol::index::session_context_t session)
  {
    std::shared_ptr<detail::client_snapshot_t const> const old
        = state_->snapshot ();
    session.set_layer (old->session.layer ());
    detail::set_session_step_t step = { engine_, session };
    state_->publish (std::move (session), old->layer);
    asio::post (engine_->get_strand (), std::move (step));
  }

  /// @brief The counters of the dialogue (any thread).
  dialogue_statistics_t
  statistics () const LUMEX_NOEXCEPT
  {
    return engine_->statistics ();
  }

  /// @brief Ends every call with `cancelled`; the link stays (any thread).
  void
  cancel () LUMEX_NOEXCEPT
  {
    engine_->cancel ();
  }

  /// @brief Ends every call with `cancelled` and closes the link, for good
  /// (any thread, idempotent).
  void
  close () LUMEX_NOEXCEPT
  {
    engine_->close ();
  }

  /// @brief The executor of the transport.
  executor_type
  get_executor () const
  {
    return engine_->get_executor ();
  }

  /// @brief The strand of the dialogue.
  strand_type
  get_strand () const
  {
    return engine_->get_strand ();
  }

  /**
   * @brief The dialogue under the client, for what it does not repeat: edges,
   * the link state, the keepalive generator, the last known state, scenarios.
   * Leave the session and the layer to the client.
   */
  dialogue_engine &
  engine () LUMEX_NOEXCEPT
  {
    return *engine_;
  }

private:
  detail::client_core_t
  core () const
  {
    detail::client_core_t made;
    made.engine = engine_;
    made.state = state_;
    return made;
  }

  identity_result_t
  identify_sync (deadline_t const *deadline)
  {
    static_assert (detail::identify_check_t<P>::value,
                   "LUMEX_PROTOCOL: client: identify (): checked");
    using unexpected_error
        = ::lumex::core::expected::error::unexpected<std::error_code>;
    if (detail::would_deadlock (*engine_))
      return unexpected_error (
          make_error_code (error::transport_errc::would_deadlock));
    std::shared_ptr<detail::call_wait_t<identity_t>> const wait
        = std::make_shared<detail::call_wait_t<identity_t>> ();
    detail::call_wait_handler_t<identity_t> handler = { wait };
    async_identify (asio::bind_cancellation_slot (wait->signal.slot (),
                                                  std::move (handler)));
    std::error_code const ended
        = detail::wait_for_call (*engine_, wait, deadline);
    if (ended)
      return unexpected_error (ended);
    return identity_result_t (std::move (wait->value));
  }

  std::shared_ptr<client_state_t> state_;
  std::shared_ptr<dialogue_engine> engine_;
};

/// @brief The names of a client that a row may not take.
template <typename P>
struct client_names_t : client_members_t<P>, client_class_names_t
{
};
} // namespace detail

/// @brief The client of a protocol layer (see the file description).
template <typename P>
class client : public detail::client_members_t<P>,
               public P::template lumex_methods<client<P>>
{
  static_assert (detail::rows_named_free_t<
                     detail::client_row_name_t, detail::client_names_t<P>,
                     typename detail::client_dsl::commands_of<P>::type>::value,
                 "LUMEX_PROTOCOL: client<P>: no row is named like a member");

public:
  /// @brief A client over `link` (not empty; moved in). Reading begins at
  /// once. The default options are what the table says; start from
  /// `client_options_for<P> ()` to change some (the name, the timers, ...).
  explicit client (frame_link link,
                   client_options_t options = client_options_for<P> ())
      : detail::client_members_t<P> (std::move (link), std::move (options))
  {
  }

  client (client const &) = delete;
  client &operator= (client const &) = delete;

  /// @brief Closes (asynchronously) and returns; the calls in flight end with
  /// `cancelled`.
  ~client () = default;
};
} // namespace protocol
} // namespace transport
} // namespace applied
} // namespace lumex

namespace asio
{
template <template <typename, typename> class Associator, typename L,
          typename Row, typename Handler, typename DefaultCandidate>
struct associator<Associator,
                  lumex::applied::transport::protocol::detail::call_handler_t<
                      L, Row, Handler>,
                  DefaultCandidate> : Associator<Handler, DefaultCandidate>
{
  using wrapper_type
      = lumex::applied::transport::protocol::detail::call_handler_t<L, Row,
                                                                    Handler>;

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

template <typename L, typename Row, typename Handler,
          typename DefaultCandidate>
struct associator<associated_allocator,
                  lumex::applied::transport::protocol::detail::call_handler_t<
                      L, Row, Handler>,
                  DefaultCandidate>
{
  using wrapper_type
      = lumex::applied::transport::protocol::detail::call_handler_t<L, Row,
                                                                    Handler>;
  using shield_type = lumex::applied::transport::detail::shielded_allocator<
      Handler, DefaultCandidate>;
  using type = typename shield_type::type;

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

#endif // !LUMEX_APPLIED_TRANSPORT_PROTOCOL_CLIENT_PROTOCOL_CLIENT_HPP
