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
 * @file LumexDeviceSimulator.hpp
 * @brief `device_simulator<P>`: a device made from the table of protocol
 * layer `P` that answers over a mock stream or a mock datagram transport.
 * @details The simulator sits on the mock of the code under test the way the
 * device sits on the wire. It watches the bytes the code writes (the write or
 * send observer of the mock), takes the frames out of them with the codec of
 * `core/framing` it was given, finds the row each frame names in the run-time
 * index of `P`, decodes the request and answers with a message made of the
 * row:
 *  - by default the sample reply of the row (`field::sample_value` of its
 *    reply, the first valid value of every field), so every row of every
 *    table has an answer without a line of code;
 *  - a handler of the test (`on<Row> (function)`) or a fixed value
 *    (`set_reply<Row> (value)`) in place of the sample;
 *  - for a reply kind of the dialogue (`ack_nak`, `two_phase`, `echo_set`,
 *    `echo_zero_on_error`, `state_instead_of_refusal`) the acknowledgement,
 *    the acknowledgement and then the completion, or the echo of the value
 *    that was sent, as the kind says; a row that names `reply_row<R>` is
 *    answered with the message of `R`;
 *  - registries as state: a request that carries a `registry_value_` to
 *    write (a generic "set parameter" row) is checked as the encoder checks
 *    it (`registry::check_record`: the access and the limits of the entry its
 *    key names) and the value is kept; a reply that carries a registry
 *    value (a generic "get parameter" row, or the report that follows a set)
 *    shows the entry named by the key of the request with the value kept
 *    for it, or the first acceptable value of the entry when none was set.
 *    When that entry cannot be read the reply cannot exist: the request is
 *    refused (below) before anything is kept, so a refused set keeps nothing,
 *    and the device never names another entry in its place.
 *
 * State and refusal. `set_state` sets the number the device reports as its
 * state. A row with `accepted_in (...)` is accepted only while the state is
 * one of them; otherwise the device refuses as the reply kind of the row
 * says: `ack_nak` with the negative acknowledgement, `echo_zero_on_error`
 * with the echo of zero, `state_instead_of_refusal` with the state in place
 * of the value; a row whose reply says nothing of the kind (a plain reply,
 * `two_phase`, `echo_set`, no reply) is not answered. The same refusal
 * answers a write the registry rejects, a request whose reply would have to
 * name an entry that cannot be read, and a row marked with `refuse<Row>`.
 * The row named by the `state_from<Row, Field>` option of the protocol shows
 * the state in that field of its default reply. A row that is not in the
 * firmware or the model of the session (`session ().available (...)`) does
 * not exist for the device: it is an unknown request.
 *
 * What a client cannot tell. The refusal of `ack_nak` is a message of the
 * shape of the negative part; when the two parts have one wire shape (a byte
 * and a signed byte) the client reads the refusal as the acknowledgement, and
 * when `state_instead_of_refusal` is refused with a state equal to the value
 * sent, the client reads it as the echo: give the parts of a table different
 * shapes. The default reply of a watching handler (one that returns nothing)
 * is made after it ran, with the state it saw before: a state it sets shows
 * in the next reply.
 *
 * Unknown and malformed requests are untrusted input. A frame whose key is no
 * row's, and a request of a known row that does not decode, are counted
 * (`unknown_requests`, `malformed_requests`) and answered with nothing, or,
 * with `simulator_options_t::answer_unknown`, with the bytes of the frame
 * sent back unchanged (so a client sees a frame it cannot match). The
 * simulator never reads past a frame; a damaged frame the codec drops, and a
 * datagram of the device's own report with no frame in it (empty or cut), are
 * counted as framing errors. A cut frame after a whole one in the same
 * datagram goes unnoticed: the codecs do not tell of it.
 *
 * Streams. `start_stream<Row> ()` makes the device send the row by itself, at
 * the interval of its `stream (ms)` option (or the one given), on the timer
 * source of the virtual clock the simulator was made with, so a test moves
 * the stream by advancing the clock. `start_stream<Row> (value)` repeats a
 * value, `start_stream_with<Row> (function)` asks a function for each
 * message (a state that changes), `start_streams ()` starts every `stream`
 * row with its sample. A stream of a row that is not in the session (firmware,
 * model) sends nothing while the session says so; it goes on when the session
 * changes back. `after (delay, function)` runs a function at a
 * virtual time on the same strand (a scripted sequence: change the state, the
 * reply, the behaviour, start or stop a stream).
 *
 * Scenarios. `play<Scenario> ()` makes the device play its side of a scenario
 * written for the client (`LUMEX_PROTOCOL_SCENARIO`, the same
 * `Scenario::make ()` the dialogue engine runs): one description, read from
 * the other end. `send<Row>` is the device waiting for a request of `Row`
 * and answering it as the row is configured; `expect<Row>` is the device
 * sending a message of `Row` by itself (the value of `set_reply<Row>`, else
 * the sample, as a stream does); `wait_state (s)` is the device setting the
 * state `s` and, when the protocol has a `state_from` row, sending a message
 * of that row that shows it (the way the client learns it);
 * `branch_on_reply`, `jump` and `label` follow the decision the client takes,
 * with the predicate of the scenario on the last message the device sent for
 * the row (a reply, or a message by itself). Everything but a `send` runs at
 * once, on the strand of the mock, after the step before it; a `send` waits.
 * A request of another row is served as usual and does not advance the
 * script, nor does a request of the row that the device refused, did not
 * process (a silent or frozen device) or could not decode. The time of a step
 * is not the device's: it never times out (use `after` for a delay). What
 * the device sends by itself the client has to be waiting for: start the
 * client's run before `play`, as the messages of a real device are lost when
 * nobody listens. A message of a row outside the session (firmware, model)
 * is not sent, the script goes on. `playback ()` tells whether the script
 * waits (`playing` at the step of the `send`), ended (`finished`, the step is
 * the number of steps) or failed (`failed`, the step that failed and the
 * error): a scenario whose `check ()` fails is not played (`play` returns
 * false, `invalid_argument`); a branch on a row with no message yet is
 * `unexpected_reply`; a predicate that throws fails it with `interrupted`
 * and the exception goes on to the code that runs the strand, as that of a
 * handler; a loop that never reaches a `send` (a thousand jumps back in a
 * row) fails with `resource_deadlock_would_occur`. A second `play` while one
 * runs, and `play` after the `io_context` is gone, return false and change
 * nothing; after a script ended or failed another one starts from its first
 * step. A scenario of another protocol does not compile. The messages the
 * script sends by itself are `scripted_messages`.
 *
 * Behaviours. Every message the device sends, a reply or a stream message,
 * goes through the `behavior_applier` of the simulator (`set_behavior`,
 * perfect by default): a silent, frozen or unplugged device does not
 * process the request at all (its handler does not run and the registry and
 * the state are not touched), the other behaviours shape the bytes. A reply
 * of two messages (`two_phase`) is two replies for the behaviours that count
 * replies. The counters follow it: `not_processed` counts requests the device
 * did not process (not stream messages), `replies` and `stream_messages` count
 * messages that reach the mock (not one the behaviour dropped), and
 * `registry_writes` counts values kept (a handler that throws keeps none).
 *
 * Threads. The mock calls the simulator on its strand, and so do its timers.
 * The configuration (`on`, `set_reply`, `refuse`, `set_state`,
 * `set_behavior`, the streams and `after`) may be called from any thread,
 * also while requests are served; `session ()` is a plain value that is
 * changed while no request is served or from `after`. The destructor, called
 * from another thread, returns while a handler may still be running on the
 * strand of the mock (that call finishes, sends nothing and drops what it
 * holds).
 *
 * Lifetime. The simulator owns its handlers and its timers. Destroying it
 * stops the streams, drops the handlers and detaches it from the mock; it
 * may be destroyed inside one of its own handlers (the request being served
 * is answered with nothing), while a stream runs, and after the
 * `io_context` is gone. When the context shuts down the simulator lets go of
 * the mock (so that nothing of the context outlives it) and of the functions
 * of the test, and is dead from then on: `start_stream`,
 * `start_stream_with` and `play` return false, `after` does nothing and
 * `statistics ()` and `playback ()` go on. Its timers on a virtual clock
 * outlive the context; with a real clock
 * (`steady_timer_source`) they are Asio timers, which must not: destroy the
 * simulator before the `io_context` then. A mock that is not given to a
 * simulator has the same limit (see `LumexMockStream.hpp`). A handler that
 * throws leaves the exception to the
 * code that runs the mock's strand (the mock keeps the write it saw
 * complete); the simulator counts it (`handler_errors`), sends nothing for
 * that request and goes on. Nothing the simulator hands to the mock keeps
 * the simulator alive.
 *
 * Available with `LUMEX_TRANSPORT_WITH_TESTING` and
 * `LUMEX_TRANSPORT_WITH_PROTOCOL` (the protocol and the framing modules).
 * @code
 * asio::io_context context;
 * testing::virtual_clock clock;
 * testing::mock_options_t options;
 * options.timers = clock.source ();
 * testing::mock_stream mock
 *     = testing::make_mock_stream (context.get_executor (), options);
 * // ... open mock.stream, hand it to a framed_stream with the same codec,
 * // and the frame_link to a dialogue_engine ...
 * testing::device_simulator<pump_t> device (
 *     mock.control, core::framing::slip::slip_codec (), clock);
 * device.on<pump_t::INFO_t> (
 *     [] (testing::device_simulator<pump_t>::request_of<pump_t::INFO_t> const
 * &) { return my_info (); }); device.start_stream<pump_t::STATUS_t> ();
 * @endcode
 */
#ifndef LUMEX_APPLIED_TRANSPORT_TESTING_SIMULATOR_DEVICE_SIMULATOR_HPP
#define LUMEX_APPLIED_TRANSPORT_TESTING_SIMULATOR_DEVICE_SIMULATOR_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#include "lumex/applied/transport/protocol/dsl/LumexDialogueVocabulary.hpp"
#include "lumex/applied/transport/testing/LumexBehaviors.hpp"
#include "lumex/applied/transport/testing/LumexMockDatagram.hpp"
#include "lumex/applied/transport/testing/LumexMockStream.hpp"
#include "lumex/applied/transport/testing/LumexVirtualClock.hpp"
#include "lumex/core/framing/codec/LumexFramingCodec.hpp"
#include "lumex/core/protocol/LumexProtocol"
#include "lumex/core/protocol/dsl/LumexProtocolScenario.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/sequence/LumexIndexSequence.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#if __has_warning("-Wlifetime-safety-suggestions")
// Clang 23 suggests [[clang::lifetimebound]] for every function that returns
// or stores a parameter (a suggestion, not a finding).
#pragma clang diagnostic ignored "-Wlifetime-safety-suggestions"
#endif
#if __has_warning("-Wlifetime-safety-invalidation")
// Clang 23 reports a reference parameter to a container as "invalidated" by
// its own push_back (a false positive: only references into the container
// are invalidated, not the container).
#pragma clang diagnostic ignored "-Wlifetime-safety-invalidation"
#endif
#if __has_warning("-Wlifetime-safety-intra-tu-constructor-suggestions")
#pragma clang diagnostic ignored                                              \
    "-Wlifetime-safety-intra-tu-constructor-suggestions"
#endif
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#endif

namespace lumex
{
namespace applied
{
namespace transport
{
namespace testing
{
/// @brief The options of a `device_simulator`.
struct simulator_options_t
{
  /// The name for logs.
  std::string name = "simulator";
  /// Answer a frame of no known row, and a request that does not decode,
  /// with the bytes of the frame sent back unchanged (default: with nothing).
  bool answer_unknown = false;
};

/// @brief How many requests one row got.
struct simulator_row_count_t
{
  /// The identifier of the row in the table.
  std::string row;
  /// The requests of the row seen (served or refused).
  std::uint64_t requests = 0;
};

/// @brief Where the playback of a scenario is.
enum class playback_state : std::uint8_t
{
  /// Nothing was played.
  idle,
  /// The script waits at a `send` (or has not run its first steps yet).
  playing,
  /// Every step was played.
  finished,
  /// The script stopped at a step with an error.
  failed
};

/// @brief The playback of the scenario of a `device_simulator` (a snapshot).
struct playback_status_t
{
  /// Where it is.
  playback_state state = playback_state::idle;
  /// The step it waits at, the number of steps when it is finished, the step
  /// that failed when it failed.
  std::size_t step = 0;
  /// The number of steps of the scenario.
  std::size_t steps = 0;
  /// The error of a failed playback.
  std::error_code error;
};

/// @brief The counters of a `device_simulator` (a snapshot).
struct simulator_statistics_t
{
  /// Requests of a known row of the device (also those it refused or did
  /// not process).
  std::uint64_t requests = 0;
  /// Messages that reached the mock as replies of requests (a stream message
  /// is no reply; a reply the behaviour dropped did not reach it).
  std::uint64_t replies = 0;
  /// Requests the device refused (state, registry, `refuse<Row>`); whether
  /// the refusal is a message depends on the reply kind of the row.
  std::uint64_t refused = 0;
  /// Requests the behaviour did not let the device process (not stream
  /// messages).
  std::uint64_t not_processed = 0;
  /// Frames of no known row (or of a row the session does not have).
  std::uint64_t unknown_requests = 0;
  /// Requests of a known row that did not decode.
  std::uint64_t malformed_requests = 0;
  /// Datagrams with another report ID than the device's.
  std::uint64_t foreign_reports = 0;
  /// Frames the codec dropped (damaged, too large, cut), and datagrams of the
  /// device's report with no frame in them.
  std::uint64_t framing_errors = 0;
  /// Stream messages that reached the mock.
  std::uint64_t stream_messages = 0;
  /// Replies the table could not encode (a value the encoder refuses).
  std::uint64_t encode_errors = 0;
  /// Handlers and generators that threw.
  std::uint64_t handler_errors = 0;
  /// Messages a played scenario made the device send by itself (an `expect`
  /// or a `wait_state`) that reached the mock.
  std::uint64_t scripted_messages = 0;
  /// Registry values kept (a refused request, or a handler that threw, keeps
  /// none).
  std::uint64_t registry_writes = 0;
  /// The rows that were requested, in the order of the table.
  std::vector<simulator_row_count_t> by_row;

  /// @brief The requests of the row `identifier` (0 for a row never
  /// requested).
  std::uint64_t requests_of (char const *identifier) const;
};

/// @brief The simulator's internals: erased framing, the typed row
/// machinery and the engine behind `device_simulator`. Not a public API.
namespace simulator_detail
{
namespace dsl = ::lumex::core::protocol::dsl;
namespace field = ::lumex::core::protocol::field;
namespace index = ::lumex::core::protocol::index;
namespace layout = ::lumex::core::protocol::layout;
namespace registry = ::lumex::core::protocol::registry;
namespace dialogue = ::lumex::applied::transport::protocol::dialogue;

using bytes_t = std::vector<std::uint8_t>;

/// The framing of the device, erased: a stream codec or the codec of one
/// datagram.
class simulator_wire_t
{
public:
  simulator_wire_t () = default;
  virtual ~simulator_wire_t ();
  simulator_wire_t (simulator_wire_t const &) = delete;
  simulator_wire_t &operator= (simulator_wire_t const &) = delete;

  /// Feeds `size` bytes; the frames that completed are appended to `frames`.
  /// Returns how many frames the codec dropped.
  virtual std::size_t decode (std::uint8_t const *data, std::size_t size,
                              std::vector<bytes_t> &frames)
      = 0;
  /// Appends the frame of `payload` to `wire`; false if the codec refuses.
  virtual bool encode (bytes_t const &payload, bytes_t &wire) = 0;
  /// Forgets a partial frame.
  virtual void reset () = 0;
};

/// A codec of `core/framing` behind `simulator_wire_t`.
template <typename Codec> class codec_wire_t final : public simulator_wire_t
{
public:
  explicit codec_wire_t (Codec codec) : codec_ (std::move (codec)) {}

  std::size_t
  decode (std::uint8_t const *data, std::size_t size,
          std::vector<bytes_t> &frames) override
  {
    sink_t sink = { &frames, 0 };
    codec_.decode (::lumex::core::framing::codec::bytes_view_t (data, size),
                   sink);
    return sink.dropped;
  }

  bool
  encode (bytes_t const &payload, bytes_t &wire) override
  {
    return codec_
        .encode (::lumex::core::framing::codec::bytes_view_t (payload.data (),
                                                              payload.size ()),
                 wire)
        .has_value ();
  }

  void
  reset () override
  {
    codec_.reset ();
  }

private:
  // The sink of the error form: a dropped frame is heard too.
  struct sink_t
  {
    std::vector<bytes_t> *frames;
    std::size_t dropped;

    void
    operator() (::lumex::core::framing::codec::bytes_view_t frame,
                std::error_code error)
    {
      if (error)
        ++dropped;
      else
        frames->push_back (bytes_t (frame.begin (), frame.end ()));
    }
  };

  Codec codec_;
};

// --- registry state

/// An entry of a registry: the table of the registry and the position.
using registry_slot_t = std::pair<void const *, std::size_t>;
/// The raw value kept for each entry written.
using registry_store_t = std::map<registry_slot_t, std::int64_t>;

/// One value a request writes.
struct registry_write_t
{
  registry_slot_t slot;
  std::int64_t raw = 0;
};

/// An entry a request changed: its slot and the raw value it held before.
struct registry_undo_t
{
  registry_slot_t slot;
  bool had = false;
  std::int64_t raw = 0;
};

/// What a request names in its registry fields.
struct registry_names_t
{
  /// The keys, in field order.
  std::vector<registry_slot_t> keys;
  /// The values to keep.
  std::vector<registry_write_t> writes;
  /// A value that has no raw representation (it must be refused).
  bool unrepresentable = false;
};

/// The first value of an entry the device may hold when none was written:
/// 0, the lower or the upper limit, whichever the entry accepts.
std::int64_t default_raw (registry::registry_entry_t const &entry);

/// The value kept for `slot`, or the first acceptable value of the entry.
std::int64_t stored_or_default (registry_store_t const &store,
                                registry_slot_t const &slot,
                                registry::registry_entry_t const &entry);

/// The raw value a value to write stands for; false if it has none.
bool raw_of (registry::registry_entry_t const &entry,
             registry::parameter_value_t const &value, std::int64_t &raw);

/// Sets `value` to the raw value `raw` of `entry`.
void set_raw (registry::parameter_value_t &value,
              registry::registry_entry_t const &entry, std::int64_t raw);

/// The slot of `key` in the registry `R`.
template <typename R>
registry_slot_t
slot_of (typename R::id key)
{
  return registry_slot_t (static_cast<void const *> (R::entries ().data ()),
                          static_cast<std::size_t> (key));
}

template <bool... B> struct any_t;

template <> struct any_t<> : std::false_type
{
};

template <bool Head, bool... Tail>
struct any_t<Head, Tail...>
    : std::integral_constant<bool, Head || any_t<Tail...>::value>
{
};

// How the registry fields of a field (by its implementation) are walked:
// nothing for a field without registry parts.
template <typename Impl> struct registry_walk_t
{
  using has_registry = std::false_type;
  using has_value = std::false_type;

  template <std::size_t I, typename Record, typename Value>
  static void
  collect (Record const &, Value const &, registry_names_t &) LUMEX_NOEXCEPT
  {
  }

  template <std::size_t I, typename Record, typename Value>
  static void
  patch_keys (Record &, Value &, registry_names_t const &) LUMEX_NOEXCEPT
  {
  }

  template <std::size_t I, typename Record, typename Value>
  static void
  patch_values (Record &, Value &, registry_store_t const &) LUMEX_NOEXCEPT
  {
  }

  static void
  precheck (registry_names_t const &, bool &) LUMEX_NOEXCEPT
  {
  }
};

template <typename Sequence, typename... F> struct group_walk_t;

template <std::size_t... I, typename... F>
struct group_walk_t<::lumex::core::utility::sequence::index_sequence<I...>,
                    F...>
{
  template <typename Record>
  static void
  collect (Record const &record, registry_names_t &names)
  {
    int const done[] = {
      0,
      (registry_walk_t<typename field::impl_of<F>::type>::template collect<I> (
           record, field::get<I> (record), names),
       0)...
    };
    LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (done);
  }

  template <typename Record>
  static void
  patch_keys (Record &record, registry_names_t const &names)
  {
    int const done[] = {
      0,
      (registry_walk_t<typename field::impl_of<F>::type>::template patch_keys<
           I> (record, field::get<I> (record), names),
       0)...
    };
    LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (done);
  }

  static void
  precheck (registry_names_t const &names, bool &denied)
  {
    int const done[]
        = { 0, (registry_walk_t<typename field::impl_of<F>::type>::precheck (
                    names, denied),
                0)... };
    LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (done);
  }

  template <typename Record>
  static void
  patch_values (Record &record, registry_store_t const &store)
  {
    int const done[] = {
      0, (registry_walk_t<typename field::impl_of<F>::type>::
              template patch_values<I> (record, field::get<I> (record), store),
          0)...
    };
    LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (done);
  }
};

// A group (a layout, the fields of a row): its own fields.
template <template <typename...> class List, typename... F>
struct registry_walk_t<field::group_impl<List<F...>>>
{
  using has_registry = std::integral_constant<
      bool, any_t<registry_walk_t<typename field::impl_of<F>::type>::
                      has_registry::value...>::value>;
  using has_value = std::integral_constant<
      bool, any_t<registry_walk_t<typename field::impl_of<F>::type>::
                      has_value::value...>::value>;
  using fields = group_walk_t<
      ::lumex::core::utility::sequence::make_index_sequence<sizeof...(F)>,
      F...>;

  template <std::size_t I, typename Record, typename Value>
  static void
  collect (Record const &, Value const &value, registry_names_t &names)
  {
    fields::collect (value, names);
  }

  template <std::size_t I, typename Record, typename Value>
  static void
  patch_keys (Record &, Value &value, registry_names_t const &names)
  {
    fields::patch_keys (value, names);
  }

  template <std::size_t I, typename Record, typename Value>
  static void
  patch_values (Record &, Value &value, registry_store_t const &store)
  {
    fields::patch_values (value, store);
  }

  static void
  precheck (registry_names_t const &names, bool &denied)
  {
    fields::precheck (names, denied);
  }
};

// The key of a registry entry.
template <typename R, typename F1, typename F2>
struct registry_walk_t<registry::registry_key_<R, F1, F2>>
{
  using has_registry = std::true_type;
  using has_value = std::false_type;

  template <std::size_t I, typename Record, typename Value>
  static void
  collect (Record const &, Value const &key, registry_names_t &names)
  {
    names.keys.push_back (slot_of<R> (key));
  }

  // The reply shows the entry the request named, when it named one of this
  // registry.
  template <std::size_t I, typename Record, typename Value>
  static void
  patch_keys (Record &, Value &key, registry_names_t const &names)
  {
    void const *const table = R::entries ().data ();
    for (std::size_t i = 0; i < names.keys.size (); ++i)
      if (names.keys[i].first == table)
        {
          key = static_cast<typename R::id> (names.keys[i].second);
          return;
        }
  }

  template <std::size_t I, typename Record, typename Value>
  static void
  patch_values (Record &, Value &, registry_store_t const &) LUMEX_NOEXCEPT
  {
  }

  static void
  precheck (registry_names_t const &, bool &) LUMEX_NOEXCEPT
  {
  }
};

// The value of a registry entry.
template <typename R, std::size_t KeyIndex, typename RawField,
          registry::access_t Needs>
struct registry_walk_t<registry::registry_value_<R, KeyIndex, RawField, Needs>>
{
  using has_registry = std::true_type;
  using has_value = std::true_type;

  // A value a request writes is kept.
  template <std::size_t I, typename Record, typename Value>
  static void
  collect (Record const &record, Value const &value, registry_names_t &names)
  {
    if (Needs != registry::access_t::write)
      return;
    registry::entries_t const entries = R::entries ();
    std::size_t const at
        = static_cast<std::size_t> (field::get<KeyIndex> (record));
    registry_write_t write;
    if (at >= entries.size ()
        || !raw_of (entries.data ()[at], value, write.raw))
      {
        names.unrepresentable = true;
        return;
      }
    write.slot
        = registry_slot_t (static_cast<void const *> (entries.data ()), at);
    names.writes.push_back (write);
  }

  template <std::size_t I, typename Record, typename Value>
  static void
  patch_keys (Record &, Value &, registry_names_t const &) LUMEX_NOEXCEPT
  {
  }

  // A value a reply reports is the one kept for the entry its key names.
  template <std::size_t I, typename Record, typename Value>
  static void
  patch_values (Record &record, Value &value, registry_store_t const &store)
  {
    registry::entries_t const entries = R::entries ();
    std::size_t const at
        = static_cast<std::size_t> (field::get<KeyIndex> (record));
    if (at >= entries.size ())
      return;
    // An entry that cannot be read is not reported: the request was refused
    // before (`precheck`), so there is nothing to show.
    if (registry::check_access (entries.data ()[at], Needs))
      return;
    set_raw (
        value, entries.data ()[at],
        stored_or_default (
            store,
            registry_slot_t (static_cast<void const *> (entries.data ()), at),
            entries.data ()[at]));
  }

  // The reply names the entry the request named (the first key of this
  // registry); it cannot be reported when the entry cannot be read.
  static void
  precheck (registry_names_t const &names, bool &denied)
  {
    registry::entries_t const entries = R::entries ();
    void const *const table = entries.data ();
    for (std::size_t i = 0; i < names.keys.size (); ++i)
      if (names.keys[i].first == table)
        {
          if (names.keys[i].second < entries.size ()
              && registry::check_access (entries.data ()[names.keys[i].second],
                                         Needs))
            denied = true;
          return;
        }
  }
};

// --- the call of a row

/// Everything one request (or one stream message) needs and leaves.
struct request_call_t
{
  /// The request message (empty for a stream message).
  layout::bytes_view_t message;
  /// The session values for the decoders.
  field::decode_context_t const *context = nullptr;
  /// The session of the device.
  index::session_context_t *session = nullptr;
  /// The state of the device.
  std::int64_t state = 0;
  /// The row is refused (state, flag).
  bool refuse = false;
  /// The registry state.
  registry_store_t *store = nullptr;
  /// What the request named.
  registry_names_t names;
  /// The messages to send, in order.
  std::vector<bytes_t> messages;
  /// The device refused the request.
  bool refused = false;
  /// The request of a known row did not decode.
  bool malformed = false;
  /// A reply could not be encoded.
  bool encode_failed = false;
  /// How many registry values the request kept.
  std::size_t kept = 0;
  /// What the entries it kept held before, to put back if the handler throws.
  std::vector<registry_undo_t> undone;
};

/// What answers a request of a row: fills `messages` of the call.
using answer_fn_t = std::function<void (request_call_t &)>;

/// The typed side of one row, by the function pointers of a table.
struct row_ops_t
{
  /// The identifier of the row.
  char const *identifier;
  /// Answers a request by the default of the row.
  void (*answer) (request_call_t &);
  /// Makes the stream message of the row from its sample (null for a row
  /// without `stream`).
  void (*stream) (request_call_t &);
  /// Makes the message of the row from its sample with no request (the
  /// stream message, for every row; a scenario sends it).
  void (*message) (request_call_t &);
  /// Whether the row has a reply at all.
  bool replies;
};

template <typename Value> struct is_record_t : std::false_type
{
};

template <typename... F>
struct is_record_t<field::record<F...>> : std::true_type
{
};

// Sets a state field from the number the device reports.
template <typename T>
void
assign_state_scalar (T &slot, std::int64_t state,
                     std::true_type) LUMEX_NOEXCEPT
{
  slot = static_cast<T> (state);
}

template <typename T>
void
assign_state_scalar (T &, std::int64_t, std::false_type) LUMEX_NOEXCEPT
{
}

template <typename T>
void
assign_state_enum (T &slot, std::int64_t state, std::true_type) LUMEX_NOEXCEPT
{
  slot = static_cast<T> (
      static_cast<typename std::underlying_type<T>::type> (state));
}

template <typename T>
void
assign_state_enum (T &slot, std::int64_t state, std::false_type) LUMEX_NOEXCEPT
{
  assign_state_scalar (
      slot, state,
      std::integral_constant<bool,
                             std::is_integral<T>::value
                                 || std::is_floating_point<T>::value> ());
}

/// Sets `slot` to the state when it is a number or an enumeration (and
/// leaves it alone otherwise).
template <typename T>
void
assign_state (T &slot, std::int64_t state) LUMEX_NOEXCEPT
{
  assign_state_enum (slot, state, std::is_enum<T> ());
}

// The sample reply of a row whose reply may be none.
template <typename Row, bool Has = !std::is_same<typename Row::reply_type,
                                                 dsl::no_reply>::value>
struct row_sample_t
{
  static typename layout::reply_message_of<Row>::type
  get ()
  {
    return field::sample_value<typename Row::reply_type> ();
  }
};

template <typename Row> struct row_sample_t<Row, false>
{
  static field::record<>
  get () LUMEX_NOEXCEPT
  {
    return field::record<> ();
  }
};

// The registry fields of the default reply value, from what the request
// named and what is kept.
template <typename Fields, typename Value>
void
patch_registry (Value &value, request_call_t &call, std::true_type)
{
  using walker = registry_walk_t<typename field::impl_of<Fields>::type>;
  walker::template patch_keys<0> (value, value, call.names);
  if (call.store != nullptr)
    walker::template patch_values<0> (value, value, *call.store);
}

template <typename Fields, typename Value>
void
patch_registry (Value &, request_call_t &, std::false_type) LUMEX_NOEXCEPT
{
}

template <typename Fields, typename Value>
void
patch_registry (Value &value, request_call_t &call)
{
  patch_registry<Fields> (
      value, call,
      typename registry_walk_t<
          typename field::impl_of<Fields>::type>::has_registry ());
}

// The registry fields of the reply of a row, when it has fields.
template <typename Row,
          bool Core = field::is_field<typename Row::reply_type>::value>
struct reply_patch_t
{
  template <typename Value>
  static void
  apply (Value &value, request_call_t &call)
  {
    patch_registry<typename Row::reply_type> (value, call);
  }
};

template <typename Row> struct reply_patch_t<Row, false>
{
  template <typename Value>
  static void
  apply (Value &, request_call_t &) LUMEX_NOEXCEPT
  {
  }
};

// The state shown in the reply of the row `state_from` names.
template <typename P, typename Row,
          typename StateRow = typename dialogue::detail::option_row_t<
              typename dialogue::detail::protocol_option_t<
                  P, dialogue::detail::state_from_kind_t>::type>::type>
struct state_patch_t
{
  template <typename Value>
  static void
  apply (Value &, std::int64_t) LUMEX_NOEXCEPT
  {
  }
};

template <typename P, typename Row> struct state_patch_t<P, Row, Row>
{
  using index_type = std::integral_constant<
      std::size_t, static_cast<std::size_t> (
                       dialogue::detail::option_number_t<
                           typename dialogue::detail::protocol_option_t<
                               P, dialogue::detail::state_from_kind_t>::type,
                           0>::get ())>;

  template <typename Value>
  static void
  apply (Value &value, std::int64_t state)
  {
    apply_to (value, state, is_record_t<Value> ());
  }

private:
  template <typename Value>
  static void
  apply_to (Value &value, std::int64_t state, std::true_type)
  {
    assign_state (field::get<index_type::value> (value), state);
  }

  template <typename Value>
  static void
  apply_to (Value &value, std::int64_t state, std::false_type)
  {
    assign_state (value, state);
  }
};

// The registry effects of a request: keys named, values written.
template <typename Row, typename Request,
          typename Walker = registry_walk_t<
              typename field::impl_of<typename Row::fields_type>::type>>
struct request_registry_t
{
  // Collects the keys and the values the request names and checks the
  // values as the encoder does; false when the request is to be refused.
  static bool
  prepare (request_call_t &call, Request const &request)
  {
    return apply_to (call, request, typename Walker::has_registry ());
  }

  // Keeps the values (after `prepare`, once the reply is known to exist).
  static void
  keep (request_call_t &call)
  {
    if (call.store == nullptr)
      return;
    for (std::size_t i = 0; i < call.names.writes.size (); ++i)
      {
        registry_store_t::iterator const held
            = call.store->find (call.names.writes[i].slot);
        registry_undo_t before;
        before.slot = call.names.writes[i].slot;
        before.had = held != call.store->end ();
        before.raw = before.had ? held->second : 0;
        call.undone.push_back (before);
        (*call.store)[call.names.writes[i].slot] = call.names.writes[i].raw;
      }
    call.kept = call.names.writes.size ();
  }

  // Puts back what `keep` changed (a handler threw: nothing is kept).
  static void
  undo (request_call_t &call)
  {
    if (call.store == nullptr)
      return;
    for (std::size_t i = call.undone.size (); i != 0; --i)
      {
        registry_undo_t const &each = call.undone[i - 1];
        if (each.had)
          (*call.store)[each.slot] = each.raw;
        else
          call.store->erase (each.slot);
      }
    call.undone.clear ();
    call.kept = 0;
  }

private:
  static bool
  apply_to (request_call_t &, Request const &, std::false_type) LUMEX_NOEXCEPT
  {
    return true;
  }

  static bool
  apply_to (request_call_t &call, Request const &request, std::true_type)
  {
    Walker::template collect<0> (request, request, call.names);
    if (call.names.unrepresentable)
      return false;
    return check (request, typename Walker::has_value ());
  }

  static bool
  check (Request const &, std::false_type) LUMEX_NOEXCEPT
  {
    return true;
  }

  static bool
  check (Request const &request, std::true_type)
  {
    return !registry::check_record<typename Row::fields_type> (request);
  }
};

// Whether the reply fields `Fields` can be reported for what the request
// named: false when they hold a registry value of an entry that cannot be
// read.
template <typename Fields>
bool
reply_readable (request_call_t const &call)
{
  bool denied = false;
  registry_walk_t<typename field::impl_of<Fields>::type>::precheck (call.names,
                                                                    denied);
  return !denied;
}

// The same for the reply of a row (true for a row without fields).
template <typename Row,
          bool Core = field::is_field<typename Row::reply_type>::value>
struct row_readable_t
{
  static bool
  get (request_call_t const &call)
  {
    return reply_readable<typename Row::reply_type> (call);
  }
};

template <typename Row> struct row_readable_t<Row, false>
{
  static bool
  get (request_call_t const &) LUMEX_NOEXCEPT
  {
    return true;
  }
};

// --- the parts of a reply kind

// A part of a reply kind: a field (the reply message of the row itself with
// these fields) or a row (a message of its own).
template <typename P, typename Row, typename T,
          int Form = dialogue::detail::part_form_t<T>::value>
struct part_t
{
  using value_type = typename dialogue::detail::part_value_t<T>::type;
  using part_type = typename dialogue::detail::part_type_t<T>::type;

  static value_type
  sample ()
  {
    return field::sample_value<part_type> ();
  }

  // The sample showing the entries the request named.
  static value_type
  sample_for (request_call_t &call)
  {
    value_type value = sample ();
    patch_registry<part_type> (value, call);
    return value;
  }

  static bool
  readable (request_call_t const &call)
  {
    return reply_readable<part_type> (call);
  }

  static void
  encode (request_call_t &call, value_type const &value)
  {
    bytes_t message;
    if (layout::encode_reply_message<
            P, dialogue::detail::as_reply_t<Row, part_type>> (message, value,
                                                              call.context)
            .has_value ())
      call.messages.push_back (std::move (message));
    else
      call.encode_failed = true;
  }
};

template <typename P, typename Row, typename T> struct part_t<P, Row, T, 1>
{
  using value_type = typename dialogue::detail::part_value_t<T>::type;

  static value_type
  sample ()
  {
    return row_sample_t<T>::get ();
  }

  static value_type
  sample_for (request_call_t &call)
  {
    value_type value = sample ();
    reply_patch_t<T>::apply (value, call);
    state_patch_t<P, T>::apply (value, call.state);
    return value;
  }

  static bool
  readable (request_call_t const &call)
  {
    return row_readable_t<T>::get (call);
  }

  static void
  encode (request_call_t &call, value_type const &value)
  {
    bytes_t message;
    if (layout::encode_reply_message<P, T> (message, value, call.context)
            .has_value ())
      call.messages.push_back (std::move (message));
    else
      call.encode_failed = true;
  }
};

// The echo of the value sent, as the reply reader compares it: the whole
// request when the echo is that record, else the last field of the request.
template <typename Echo, typename Sent,
          int Category = std::is_same<Echo, Sent>::value
                             ? 0
                             : ((std::is_arithmetic<Echo>::value
                                 || std::is_enum<Echo>::value)
                                        && (std::is_arithmetic<Sent>::value
                                            || std::is_enum<Sent>::value)
                                    ? 2
                                    : 3)>
struct echo_convert_t
{
  static Echo
  get (Sent const &)
  {
    return Echo ();
  }
};

template <typename Echo, typename Sent> struct echo_convert_t<Echo, Sent, 0>
{
  static Echo
  get (Sent const &sent)
  {
    return sent;
  }
};

template <typename Echo, typename Sent> struct echo_convert_t<Echo, Sent, 2>
{
  static Echo
  get (Sent const &sent)
  {
    return static_cast<Echo> (sent);
  }
};

template <typename Echo, typename Request,
          bool Whole = std::is_same<Echo, Request>::value,
          bool Fields = (Request::size::value > 0)>
struct echo_of_t
{
  // The last field.
  static Echo
  get (Request const &request)
  {
    return echo_convert_t<
        Echo,
        typename std::decay<decltype (field::get<Request::size::value - 1> (
            request))>::type>::get (field::get<Request::size::value
                                               - 1> (request));
  }
};

template <typename Echo, typename Request>
struct echo_of_t<Echo, Request, true, true>
{
  static Echo
  get (Request const &request)
  {
    return request;
  }
};

template <typename Echo, typename Request, bool Whole>
struct echo_of_t<Echo, Request, Whole, false>
{
  static Echo
  get (Request const &)
  {
    return Echo ();
  }
};

// --- the answer of a row, by the form of its reply

// Primary: a field or a layout of the core (the message of the row), and a
// reply of another layer (nothing to send).
template <typename P, typename Row,
          typename ReplyRow = typename dialogue::detail::reply_row_of_t<
              typename Row::options_type>::type,
          typename Reply = typename Row::reply_type,
          bool Core = field::is_field<Reply>::value>
struct answerer_t
{
  using value_type = typename layout::reply_message_of<Row>::type;
  using request_type = typename layout::request_of<Row>::type;

  static LUMEX_CONSTEXPR bool
  replies () LUMEX_NOEXCEPT
  {
    return true;
  }

  // Whether the reply can be reported for what the request named.
  static bool
  readable (request_call_t const &call)
  {
    return reply_readable<Reply> (call);
  }

  static value_type
  default_value (request_call_t &call, request_type const *)
  {
    value_type value = field::sample_value<Reply> ();
    reply_patch_t<Row>::apply (value, call);
    state_patch_t<P, Row>::apply (value, call.state);
    return value;
  }

  static void
  accept (request_call_t &call, value_type const &value)
  {
    bytes_t message;
    if (layout::encode_reply_message<P, Row> (message, value, call.context)
            .has_value ())
      call.messages.push_back (std::move (message));
    else
      call.encode_failed = true;
  }

  static void
  refuse (request_call_t &call, request_type const *) LUMEX_NOEXCEPT
  {
    call.refused = true;
  }
};

// No reply.
template <typename P, typename Row>
struct answerer_t<P, Row, void, dsl::no_reply, false>
{
  using value_type = field::record<>;
  using request_type = typename layout::request_of<Row>::type;

  static LUMEX_CONSTEXPR bool
  replies () LUMEX_NOEXCEPT
  {
    return false;
  }

  // Whether the reply can be reported for what the request named.
  static bool
  readable (request_call_t const &) LUMEX_NOEXCEPT
  {
    return true;
  }

  static value_type
  default_value (request_call_t &, request_type const *) LUMEX_NOEXCEPT
  {
    return value_type ();
  }

  static void
  accept (request_call_t &, value_type const &) LUMEX_NOEXCEPT
  {
  }

  static void
  refuse (request_call_t &call, request_type const *) LUMEX_NOEXCEPT
  {
    call.refused = true;
  }
};

// A reply of another layer: the simulator cannot answer it.
template <typename P, typename Row, typename Reply>
struct answerer_t<P, Row, void, Reply, false>
{
  using value_type = field::record<>;
  using request_type = typename layout::request_of<Row>::type;

  static LUMEX_CONSTEXPR bool
  replies () LUMEX_NOEXCEPT
  {
    return false;
  }

  // Whether the reply can be reported for what the request named.
  static bool
  readable (request_call_t const &) LUMEX_NOEXCEPT
  {
    return true;
  }

  static value_type
  default_value (request_call_t &, request_type const *) LUMEX_NOEXCEPT
  {
    return value_type ();
  }

  static void
  accept (request_call_t &, value_type const &) LUMEX_NOEXCEPT
  {
  }

  static void
  refuse (request_call_t &call, request_type const *) LUMEX_NOEXCEPT
  {
    call.refused = true;
  }
};

// The message of another row answers (reply_row<R>).
template <typename P, typename Row, typename ReplyRow>
struct answerer_t<P, Row, ReplyRow, dsl::no_reply, false>
{
  using value_type = typename layout::reply_message_of<ReplyRow>::type;
  using request_type = typename layout::request_of<Row>::type;

  static LUMEX_CONSTEXPR bool
  replies () LUMEX_NOEXCEPT
  {
    return true;
  }

  // Whether the reply can be reported for what the request named.
  static bool
  readable (request_call_t const &call)
  {
    return row_readable_t<ReplyRow>::get (call);
  }

  static value_type
  default_value (request_call_t &call, request_type const *)
  {
    value_type value = row_sample_t<ReplyRow>::get ();
    reply_patch_t<ReplyRow>::apply (value, call);
    state_patch_t<P, ReplyRow>::apply (value, call.state);
    return value;
  }

  static void
  accept (request_call_t &call, value_type const &value)
  {
    bytes_t message;
    if (layout::encode_reply_message<P, ReplyRow> (message, value,
                                                   call.context)
            .has_value ())
      call.messages.push_back (std::move (message));
    else
      call.encode_failed = true;
  }

  static void
  refuse (request_call_t &call, request_type const *) LUMEX_NOEXCEPT
  {
    call.refused = true;
  }
};

// ack_nak<Ack, Nak>: the acknowledgement; the negative one to refuse.
template <typename P, typename Row, typename A, typename N>
struct answerer_t<P, Row, void, dialogue::ack_nak_t<A, N>, false>
{
  using value_type = typename dialogue::ack_nak_t<A, N>::value_type;
  using request_type = typename layout::request_of<Row>::type;

  static LUMEX_CONSTEXPR bool
  replies () LUMEX_NOEXCEPT
  {
    return true;
  }

  // Whether the reply can be reported for what the request named.
  static bool
  readable (request_call_t const &call)
  {
    return part_t<P, Row, A>::readable (call);
  }

  static value_type
  default_value (request_call_t &call, request_type const *)
  {
    return part_t<P, Row, A>::sample_for (call);
  }

  static void
  accept (request_call_t &call, value_type const &value)
  {
    part_t<P, Row, A>::encode (call, value);
  }

  static void
  refuse (request_call_t &call, request_type const *)
  {
    call.refused = true;
    part_t<P, Row, N>::encode (call, part_t<P, Row, N>::sample ());
  }
};

// two_phase<Ack, Completion>: the acknowledgement, then the completion; no
// refusal.
template <typename P, typename Row, typename A, typename C>
struct answerer_t<P, Row, void, dialogue::two_phase_t<A, C>, false>
{
  using value_type = typename dialogue::two_phase_t<A, C>::value_type;
  using request_type = typename layout::request_of<Row>::type;

  static LUMEX_CONSTEXPR bool
  replies () LUMEX_NOEXCEPT
  {
    return true;
  }

  // Whether the reply can be reported for what the request named.
  static bool
  readable (request_call_t const &call)
  {
    return part_t<P, Row, C>::readable (call);
  }

  static value_type
  default_value (request_call_t &call, request_type const *)
  {
    return part_t<P, Row, C>::sample_for (call);
  }

  static void
  accept (request_call_t &call, value_type const &value)
  {
    part_t<P, Row, A>::encode (call, part_t<P, Row, A>::sample ());
    part_t<P, Row, C>::encode (call, value);
  }

  static void
  refuse (request_call_t &call, request_type const *) LUMEX_NOEXCEPT
  {
    call.refused = true;
  }
};

// The echo kinds: the echo of the value sent.
template <typename P, typename Row, typename Kind, typename T>
struct echo_answerer_t
{
  using echo_type = typename Kind::echo_type;
  using value_type = typename Kind::value_type;
  using request_type = typename layout::request_of<Row>::type;

  static LUMEX_CONSTEXPR bool
  replies () LUMEX_NOEXCEPT
  {
    return true;
  }

  // Whether the reply can be reported for what the request named.
  static bool
  readable (request_call_t const &) LUMEX_NOEXCEPT
  {
    return true;
  }

  static value_type
  default_value (request_call_t &, request_type const *request)
  {
    if (request == nullptr)
      return field::sample_value<echo_type> ();
    return echo_of_t<value_type, request_type>::get (*request);
  }

  static void
  accept (request_call_t &call, value_type const &value)
  {
    part_t<P, Row, T>::encode (call, value);
  }
};

template <typename P, typename Row, typename T>
struct answerer_t<P, Row, void, dialogue::echo_set_t<T>, false>
    : echo_answerer_t<P, Row, dialogue::echo_set_t<T>, T>
{
  static void
  refuse (request_call_t &call,
          typename layout::request_of<Row>::type const *) LUMEX_NOEXCEPT
  {
    call.refused = true;
  }
};

// echo_zero_on_error<T>: a zero echo refuses.
template <typename P, typename Row, typename T>
struct answerer_t<P, Row, void, dialogue::echo_zero_on_error_t<T>, false>
    : echo_answerer_t<P, Row, dialogue::echo_zero_on_error_t<T>, T>
{
  static void
  refuse (request_call_t &call, typename layout::request_of<Row>::type const *)
  {
    call.refused = true;
    part_t<P, Row, T>::encode (
        call, typename dialogue::echo_zero_on_error_t<T>::value_type ());
  }
};

// state_instead_of_refusal<T>: the state to refuse.
template <typename P, typename Row, typename T>
struct answerer_t<P, Row, void, dialogue::state_instead_of_refusal_t<T>, false>
    : echo_answerer_t<P, Row, dialogue::state_instead_of_refusal_t<T>, T>
{
  static void
  refuse (request_call_t &call, typename layout::request_of<Row>::type const *)
  {
    call.refused = true;
    typename dialogue::state_instead_of_refusal_t<T>::value_type value
        = field::sample_value<
            typename dialogue::state_instead_of_refusal_t<T>::echo_type> ();
    assign_state (value, call.state);
    part_t<P, Row, T>::encode (call, value);
  }
};

// --- answering a request

// Decodes the request of `Row`, keeps what it writes to the registries, and
// answers with the value `producer` gives (the refusal of the row when the
// device refuses). Nothing is sent from here: the messages are in the call.
template <typename P, typename Row, typename Producer>
void
answer_request (request_call_t &call, Producer &producer)
{
  using answerer = answerer_t<P, Row>;
  using request_type = typename layout::request_of<Row>::type;
  layout::decode_result_t<request_type> const decoded
      = layout::decode_message<P, Row> (call.message, call.context);
  if (!decoded)
    {
      call.malformed = true;
      return;
    }
  // Through a named reference: GCC reports the result of operator* as
  // unused when the value is an empty record (-Wunused-result).
  typename decltype (decoded)::value_type const &request = *decoded;
  // A request is refused before anything is kept when the reply it asks
  // for cannot exist: the default reply of a row names the entries the
  // request named, and an entry that cannot be read cannot be named.
  if (call.refuse
      || !request_registry_t<Row, request_type>::prepare (call, request)
      || (producer.uses_default () && !answerer::readable (call)))
    {
      answerer::refuse (call, &request);
      return;
    }
  request_registry_t<Row, request_type>::keep (call);
  try
    {
      answerer::accept (call, producer (call, request));
    }
  catch (...)
    {
      request_registry_t<Row, request_type>::undo (call);
      throw;
    }
}

// The default answer of a row.
template <typename P, typename Row> struct default_producer_t
{
  bool
  uses_default () const LUMEX_NOEXCEPT
  {
    return true;
  }

  typename answerer_t<P, Row>::value_type
  operator() (request_call_t &call,
              typename layout::request_of<Row>::type const &request) const
  {
    return answerer_t<P, Row>::default_value (call, &request);
  }
};

template <typename P, typename Row> struct row_thunk_t
{
  static void
  answer (request_call_t &call)
  {
    default_producer_t<P, Row> producer;
    answer_request<P, Row> (call, producer);
  }

  // The message of the row from its sample, with no request.
  static void
  stream (request_call_t &call)
  {
    answerer_t<P, Row>::accept (
        call, answerer_t<P, Row>::default_value (call, nullptr));
  }
};

using stream_fn_t = void (*) (request_call_t &);

template <typename P, typename Row,
          bool Streams
          = dialogue::detail::row_has_t<Row, dsl::kind::stream>::value>
struct stream_thunk_t
{
  static stream_fn_t
  get () LUMEX_NOEXCEPT
  {
    return &row_thunk_t<P, Row>::stream;
  }
};

template <typename P, typename Row> struct stream_thunk_t<P, Row, false>
{
  static stream_fn_t
  get () LUMEX_NOEXCEPT
  {
    return nullptr;
  }
};

template <typename P, typename List> struct ops_table_t;

template <typename P, typename... Rows>
struct ops_table_t<P, dsl::type_list<Rows...>>
{
  static row_ops_t const *
  get (std::size_t &count) LUMEX_NOEXCEPT
  {
    // One entry more than rows: a table without rows still has an array.
    static row_ops_t const table[sizeof...(Rows) + 1]
        = { { Rows::identifier (), &row_thunk_t<P, Rows>::answer,
              stream_thunk_t<P, Rows>::get (), &row_thunk_t<P, Rows>::stream,
              answerer_t<P, Rows>::replies () }...,
            { nullptr, nullptr, nullptr, nullptr, false } };
    count = sizeof...(Rows);
    return table;
  }
};

// --- handlers of the test

// Calls a handler with the request and the session, or the request alone.
template <typename F, typename Request>
auto
call_handler (F &function, Request const &request,
              index::session_context_t &session, int)
    -> decltype (function (request, session))
{
  return function (request, session);
}

template <typename F, typename Request>
auto
call_handler (F &function, Request const &request, index::session_context_t &,
              long) -> decltype (function (request))
{
  return function (request);
}

template <typename F, typename Request>
using handled_t
    = decltype (call_handler (std::declval<F &> (),
                              std::declval<Request const &> (),
                              std::declval<index::session_context_t &> (), 0));

// The producer of a handler: its value, or the default when it returns
// nothing (a handler that only watches).
template <typename P, typename Row, typename F> struct handler_producer_t
{
  using request_type = typename layout::request_of<Row>::type;
  using value_type = typename answerer_t<P, Row>::value_type;

  F function;

  bool
  uses_default () const LUMEX_NOEXCEPT
  {
    return std::is_void<handled_t<F, request_type>>::value;
  }

  value_type
  operator() (request_call_t &call, request_type const &request)
  {
    return produce (call, request,
                    std::is_void<handled_t<F, request_type>> ());
  }

private:
  value_type
  produce (request_call_t &call, request_type const &request, std::true_type)
  {
    call_handler (function, request, *call.session, 0);
    return answerer_t<P, Row>::default_value (call, &request);
  }

  value_type
  produce (request_call_t &call, request_type const &request, std::false_type)
  {
    return call_handler (function, request, *call.session, 0);
  }
};

template <typename P, typename Row> struct value_producer_t
{
  using request_type = typename layout::request_of<Row>::type;
  using value_type = typename answerer_t<P, Row>::value_type;

  value_type value;

  bool
  uses_default () const LUMEX_NOEXCEPT
  {
    return false;
  }

  value_type
  operator() (request_call_t &, request_type const &) const
  {
    return value;
  }
};

template <typename P, typename Row, typename Producer> struct producer_answer_t
{
  Producer producer;

  void
  operator() (request_call_t &call)
  {
    answer_request<P, Row> (call, producer);
  }
};

template <typename P, typename Row, typename Producer>
answer_fn_t
make_answer (Producer const &producer)
{
  producer_answer_t<P, Row, Producer> answer = { producer };
  return answer_fn_t (answer);
}

// A stream message made by a function of the test.
template <typename P, typename Row, typename F> struct stream_answer_t
{
  F make;

  void
  operator() (request_call_t &call)
  {
    answerer_t<P, Row>::accept (call, make ());
  }
};

/// The engine behind a `device_simulator`, without the types of the table
/// (see the file description for what it does).
class simulator_api
{
public:
  simulator_api () = default;
  virtual ~simulator_api ();
  simulator_api (simulator_api const &) = delete;
  simulator_api &operator= (simulator_api const &) = delete;

  /// Detaches from the mock and drops everything the test gave (any thread,
  /// idempotent).
  virtual void shutdown () = 0;
  /// False when the index of the table could not be built.
  virtual bool valid () const = 0;
  /// The row with this identifier, or null.
  virtual index::row_t const *find_row (char const *identifier) const = 0;
  /// The answer of the test for the row; empty goes back to the default.
  virtual void set_answer (index::row_t const *row, answer_fn_t answer) = 0;
  /// The message the device sends by itself for the row in a scenario (the
  /// value of `set_reply`); empty goes back to the sample.
  virtual void set_message (index::row_t const *row, answer_fn_t message) = 0;
  /// Plays the scenario (see the file description): false when one runs, the
  /// device is dead or the scenario fails its check. `state_row` is the
  /// identifier of the row of `state_from`, null without one.
  virtual bool play (dsl::scenario_t scenario, char const *state_row) = 0;
  /// Where the playback is.
  virtual playback_status_t playback () const = 0;
  /// Marks the row refused (or not).
  virtual void set_refusing (index::row_t const *row, bool refusing) = 0;
  /// Sends the message `make` builds every `interval`; false when the
  /// interval is not above 0.
  virtual bool start_stream (index::row_t const *row,
                             std::chrono::milliseconds interval,
                             answer_fn_t make)
      = 0;
  /// Starts every stream row from its sample; returns how many.
  virtual std::size_t start_default_streams () = 0;
  /// Stops the stream of the row (nothing for a row without one).
  virtual void stop_stream (index::row_t const *row) = 0;
  /// Stops every stream.
  virtual void stop_streams () = 0;
  /// The interval of the `stream` option of the row, 0 without it.
  virtual std::chrono::milliseconds
  stream_interval (index::row_t const *row) const
      = 0;
  virtual void set_state (std::int64_t state) = 0;
  virtual std::int64_t state () const = 0;
  virtual void set_behavior (behavior const &shaped) = 0;
  virtual void after (std::chrono::milliseconds delay,
                      std::function<void ()> action)
      = 0;
  virtual index::session_context_t &session () = 0;
  virtual simulator_statistics_t statistics () const = 0;
  virtual void reset_statistics () = 0;
};

/// Makes the engine over a mock stream.
std::shared_ptr<simulator_api>
make_stream_simulator (index::protocol_index_t const *table,
                       row_ops_t const *ops, std::size_t op_count,
                       std::unique_ptr<simulator_wire_t> wire,
                       mock_stream_control control, virtual_clock &clock,
                       simulator_options_t const &options);

/// Makes the engine over a mock datagram transport.
std::shared_ptr<simulator_api> make_datagram_simulator (
    index::protocol_index_t const *table, row_ops_t const *ops,
    std::size_t op_count, std::unique_ptr<simulator_wire_t> wire,
    mock_datagram_control control, std::uint8_t report_id,
    virtual_clock &clock, simulator_options_t const &options);

/// The index of `P`, or null when the table is refused.
template <typename P>
index::protocol_index_t const *
index_of () LUMEX_NOEXCEPT
{
  index::index_result_t const &built = index::protocol_index<P> ();
  return built.has_value () ? &*built : nullptr;
}
} // namespace simulator_detail

/**
 * @brief A device of the table of protocol layer `P` (see the file
 * description).
 * @tparam P A protocol layer (`LUMEX_PROTOCOL_DEFINE` or
 * `LUMEX_PROTOCOL_EXTEND`). `Row` below is one of its row types
 * (`P::START_t`; a row of the layer a table extends is named through the
 * layer when the layer replaced it).
 */
template <typename P> class device_simulator
{
  using api_type = simulator_detail::simulator_api;

public:
  /// @brief The record of the request fields of `Row`.
  template <typename Row>
  using request_of =
      typename ::lumex::core::protocol::layout::request_of<Row>::type;

  /// @brief What a handler returns for `Row` and `set_reply` takes: the
  /// value of its reply (`field::record<>` for a row without one; the value
  /// of the message of `reply_row<R>`; the value of the acknowledgement of
  /// `ack_nak`, of the completion of `two_phase`, the echo of the echo
  /// kinds).
  template <typename Row>
  using reply_of = typename simulator_detail::answerer_t<P, Row>::value_type;

  /**
   * @brief A device on the mock stream behind `control`; the mock must run
   * on timers of `clock` (`mock_options_t::timers`).
   * @param codec The frame codec the code under test frames with (a codec of
   * `core/framing`, moved in).
   */
  template <typename Codec>
  device_simulator (mock_stream_control control, Codec codec,
                    virtual_clock &clock,
                    simulator_options_t options = simulator_options_t ())
      : api_ (make_stream (control, std::move (codec), clock, options))
  {
  }

  /**
   * @brief A device on the mock datagram transport behind `control`; every
   * message it sends is a report with `report_id`, and it listens to the
   * reports with that ID (each datagram holds whole frames, so the codec
   * starts afresh with each).
   */
  template <typename Codec>
  device_simulator (mock_datagram_control control, Codec codec,
                    std::uint8_t report_id, virtual_clock &clock,
                    simulator_options_t options = simulator_options_t ())
      : api_ (make_datagram (control, std::move (codec), report_id, clock,
                             options))
  {
  }

  /// @brief Stops the streams, drops the handlers, detaches from the mock.
  ~device_simulator ()
  {
    if (api_)
      api_->shutdown ();
  }

  device_simulator (device_simulator const &) = delete;
  device_simulator &operator= (device_simulator const &) = delete;

  /// @brief False when the table of `P` has no index (a duplicate key): the
  /// device then answers nothing.
  bool
  valid () const
  {
    return api_->valid ();
  }

  /**
   * @brief Answers the requests of `Row` with `function (request, session)`
   * or `function (request)`: the value it returns is the reply
   * (`reply_of<Row>`), or, when it returns nothing, the default reply (a
   * handler that only watches). It runs on the strand of the mock, after the
   * request was decoded and the registry values it writes were kept, and
   * only for a request the behaviour lets the device process.
   * @return False when `Row` is no row of the index of `P`.
   */
  template <typename Row, typename F>
  bool
  on (F &&function)
  {
    check_row<Row> ();
    using decayed = typename std::decay<F>::type;
    simulator_detail::handler_producer_t<P, Row, decayed> producer
        = { decayed (std::forward<F> (function)) };
    return set_answer<Row> (simulator_detail::make_answer<P, Row> (producer));
  }

  /// @brief Answers the requests of `Row` with `value`.
  template <typename Row>
  bool
  set_reply (reply_of<Row> const &value)
  {
    check_row<Row> ();
    simulator_detail::value_producer_t<P, Row> producer = { value };
    // The same value is the message the device sends by itself for the row.
    simulator_detail::stream_answer_t<P, Row, value_maker_t<Row>> message
        = { value_maker_t<Row>{ value } };
    return set_answer<Row> (simulator_detail::make_answer<P, Row> (producer),
                            simulator_detail::answer_fn_t (message));
  }

  /// @brief Goes back to the default reply of `Row`.
  template <typename Row>
  bool
  clear_reply ()
  {
    check_row<Row> ();
    return set_answer<Row> (simulator_detail::answer_fn_t ());
  }

  /**
   * @brief Plays the device's side of `Scenario` (see the file description).
   * @tparam Scenario A type of `LUMEX_PROTOCOL_SCENARIO` of the layer `P`;
   * one of another layer does not compile.
   * @return False when a script is running, the scenario fails its
   * `check ()` or the simulator is dead (the context is gone); true when it
   * started (its first steps run on the strand of the mock).
   */
  template <typename Scenario>
  typename std::enable_if<
      std::is_same<typename Scenario::protocol_type, P>::value, bool>::type
  play ()
  {
    return api_->play (
        Scenario::make (),
        state_row_identifier (typename std::is_void<state_row_type>::type ()));
  }

  /// @brief Where the playback of the last scenario is.
  playback_status_t
  playback () const
  {
    return api_->playback ();
  }

  /// @brief Makes the device refuse (or accept again) the requests of
  /// `Row`, as it refuses outside the states of `accepted_in`.
  template <typename Row>
  bool
  refuse (bool refusing = true)
  {
    check_row<Row> ();
    simulator_detail::index::row_t const *const row = row_of<Row> ();
    if (row == nullptr)
      return false;
    api_->set_refusing (row, refusing);
    return true;
  }

  /**
   * @brief The device sends the sample message of `Row` by itself, every
   * `interval` (0: the interval of the row's `stream (ms)`), the first one
   * after one interval.
   * @return False for a row that is not in the index or has no interval.
   */
  template <typename Row>
  bool
  start_stream (std::chrono::milliseconds interval
                = std::chrono::milliseconds (0))
  {
    check_row<Row> ();
    simulator_detail::index::row_t const *const row = row_of<Row> ();
    if (row == nullptr)
      return false;
    std::chrono::milliseconds const period
        = interval.count () > 0 ? interval : api_->stream_interval (row);
    return api_->start_stream (
        row, period,
        simulator_detail::answer_fn_t (
            &simulator_detail::row_thunk_t<P, Row>::stream));
  }

  /// @brief As `start_stream<Row> ()`, with `value` in each message.
  template <typename Row>
  bool
  start_stream (reply_of<Row> const &value, std::chrono::milliseconds interval
                                            = std::chrono::milliseconds (0))
  {
    check_row<Row> ();
    return start_stream_with<Row> (value_maker_t<Row>{ value }, interval);
  }

  /// @brief As `start_stream<Row> ()`, with `make ()` (a `reply_of<Row>`)
  /// called for each message, on the strand of the mock.
  template <typename Row, typename F>
  bool
  start_stream_with (F &&make, std::chrono::milliseconds interval
                               = std::chrono::milliseconds (0))
  {
    check_row<Row> ();
    simulator_detail::index::row_t const *const row = row_of<Row> ();
    if (row == nullptr)
      return false;
    std::chrono::milliseconds const period
        = interval.count () > 0 ? interval : api_->stream_interval (row);
    using decayed = typename std::decay<F>::type;
    simulator_detail::stream_answer_t<P, Row, decayed> answer
        = { decayed (std::forward<F> (make)) };
    return api_->start_stream (row, period,
                               simulator_detail::answer_fn_t (answer));
  }

  /// @brief Stops the stream of `Row`.
  template <typename Row>
  void
  stop_stream ()
  {
    check_row<Row> ();
    simulator_detail::index::row_t const *const row = row_of<Row> ();
    if (row != nullptr)
      api_->stop_stream (row);
  }

  /// @brief Starts every `stream` row with its sample at its interval;
  /// returns how many.
  std::size_t
  start_streams ()
  {
    return api_->start_default_streams ();
  }

  /// @brief Stops every stream.
  void
  stop_streams ()
  {
    api_->stop_streams ();
  }

  /// @brief Sets the state the device reports and `accepted_in` checks.
  void
  set_state (std::int64_t state)
  {
    api_->set_state (state);
  }

  /// @brief The state (0 at first).
  std::int64_t
  state () const
  {
    return api_->state ();
  }

  /// @brief Shapes everything the device sends from now on (a new
  /// behaviour starts with fresh counters).
  void
  set_behavior (behavior const &shaped)
  {
    api_->set_behavior (shaped);
  }

  /// @brief Runs `action` on the strand of the mock `delay` of virtual time
  /// from now; dropped when the simulator is destroyed first.
  void
  after (std::chrono::milliseconds delay, std::function<void ()> action)
  {
    api_->after (delay, std::move (action));
  }

  /// @brief What identification learned: the model and the firmware that
  /// decide which rows exist, and the values the decoders read.
  ::lumex::core::protocol::index::session_context_t &
  session ()
  {
    return api_->session ();
  }

  /// @brief The counters.
  simulator_statistics_t
  statistics () const
  {
    return api_->statistics ();
  }

  /// @brief Sets every counter to zero.
  void
  reset_statistics ()
  {
    api_->reset_statistics ();
  }

private:
  template <typename Row> struct value_maker_t
  {
    reply_of<Row> value;

    reply_of<Row>
    operator() () const
    {
      return value;
    }
  };

  template <typename Row>
  static void
  check_row () LUMEX_NOEXCEPT
  {
    static_assert (
        ::lumex::core::protocol::dsl::list_contains<
            typename ::lumex::core::protocol::dsl::commands_of<P>::type,
            Row>::value,
        "device_simulator<P>: Row is a row of the layer P (name a row the "
        "layer replaced through the layer, P::ROW_t)");
  }

  static simulator_detail::row_ops_t const *
  table (std::size_t &count) LUMEX_NOEXCEPT
  {
    return simulator_detail::ops_table_t<
        P, typename ::lumex::core::protocol::dsl::commands_of<P>::type>::
        get (count);
  }

  template <typename Row>
  simulator_detail::index::row_t const *
  row_of () const
  {
    return api_->find_row (Row::identifier ());
  }

  template <typename Row>
  bool
  set_answer (simulator_detail::answer_fn_t answer,
              simulator_detail::answer_fn_t message
              = simulator_detail::answer_fn_t ())
  {
    simulator_detail::index::row_t const *const row = row_of<Row> ();
    if (row == nullptr)
      return false;
    api_->set_answer (row, std::move (answer));
    api_->set_message (row, std::move (message));
    return true;
  }

  // The row of `state_from`, void without one.
  using state_row_type = typename ::lumex::applied::transport::protocol::
      dialogue::protocol_traits_t<P>::state_row;

  static char const *
  state_row_identifier (std::true_type) LUMEX_NOEXCEPT
  {
    return nullptr;
  }

  static char const *
  state_row_identifier (std::false_type)
  {
    return state_row_name<state_row_type> ();
  }

  template <typename Row>
  static char const *
  state_row_name ()
  {
    return Row::identifier ();
  }

  template <typename Codec>
  static std::shared_ptr<api_type>
  make_stream (mock_stream_control control, Codec codec, virtual_clock &clock,
               simulator_options_t const &options)
  {
    std::size_t count = 0;
    simulator_detail::row_ops_t const *const ops = table (count);
    return simulator_detail::make_stream_simulator (
        simulator_detail::index_of<P> (), ops, count,
        std::unique_ptr<simulator_detail::simulator_wire_t> (
            new simulator_detail::codec_wire_t<Codec> (std::move (codec))),
        control, clock, options);
  }

  template <typename Codec>
  static std::shared_ptr<api_type>
  make_datagram (mock_datagram_control control, Codec codec,
                 std::uint8_t report_id, virtual_clock &clock,
                 simulator_options_t const &options)
  {
    std::size_t count = 0;
    simulator_detail::row_ops_t const *const ops = table (count);
    return simulator_detail::make_datagram_simulator (
        simulator_detail::index_of<P> (), ops, count,
        std::unique_ptr<simulator_detail::simulator_wire_t> (
            new simulator_detail::codec_wire_t<Codec> (std::move (codec))),
        control, report_id, clock, options);
  }

  std::shared_ptr<api_type> api_;
};
} // namespace testing
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_TESTING_SIMULATOR_DEVICE_SIMULATOR_HPP
