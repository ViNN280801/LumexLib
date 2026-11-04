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
 * @file LumexProtocolScenario.hpp
 * @brief A scenario of a protocol as a description: `LUMEX_PROTOCOL_SCENARIO`,
 * its steps (`scenario_step_t`), `scenario_t` and the words of its table
 * (`scenario_words_t`). No I/O: the dialogue engine of `applied/transport`
 * runs it, and a device simulator can read it as a script.
 * @details A scenario is an ordered list of steps over the rows of one
 * protocol layer, written once beside the table:
 * @code
 * LUMEX_PROTOCOL_SCENARIO (injection, my_proto,
 *   (send<my_proto::WASH_t> (2, 500)),            // send, wait for its reply
 *   (expect<my_proto::STATUS_t> ()),              // a frame of the row comes
 *   (wait_state (my_state::idle, 10000)),         // the state, within a time
 *   (label ("poll")),
 *   (send<my_proto::STATUS_t> ()),
 *   (branch_on_reply<my_proto::STATUS_t> (&is_ready, "go")),
 *   (jump ("poll")),
 *   (label ("go")),
 *   (send<my_proto::INJECT_t> ()),
 *   (send<my_proto::ERASE_t> (confirm, 7)));      // a dangerous row
 * @endcode
 * The macro defines the type `injection`, a type of the namespace it stands
 * in: `injection::make ()` gives the `scenario_t`, a plain value made anew on
 * each call, and `injection::protocol_type` the layer. Each step stands in
 * its own parentheses (its commas do not split the macro's arguments); a
 * scenario has at least one step. A lambda may be a predicate: the steps are
 * values built in a function body, not types.
 *
 * The steps:
 * - `send<Row> (args...)`: writes the message of `Row` (the arguments as
 *   `layout::make_request` takes them, one per request field) and waits for
 *   its reply. An argument that does not fit its field is the error of the
 *   step when it runs (`invalid_value`), not of the description. A row with
 *   the `dangerous` flag needs `confirm` as its first argument, and only such
 *   a row takes it: a mistake does not compile.
 * - `expect<Row> ()`, `expect<Row> (ms)`: waits for a frame of `Row` that
 *   answers no request, with the time of the step (the time the executor
 *   takes by default, or `ms`; 0 waits for ever, also on a lost link: only a
 *   cancellation ends it).
 * - `wait_state (state)`, `wait_state (state, ms)`: waits until the last known
 *   state of the device is `state` (an integer or an enumerator); `ms` of 0
 *   waits for ever, as for `expect`.
 * - `branch_on_reply<Row> (predicate, "label")`: goes on at the label when
 *   `predicate` says so for the value of the last reply of `Row` in this run
 *   (`bool (value const &)`, the value `layout::decode_reply_message<P, Row>`
 *   gives), else at the next step. `Row` has a reply of the core (a field or
 *   a layout): a reply kind of the dialogue is refused at compile time.
 * - `jump ("label")`: goes on at the label.
 * - `label ("name")`: a place to go to; it does nothing.
 *
 * A label is a string with static storage, a literal as a rule: the step keeps
 * the pointer (a `std::string::c_str ()` dangles). `scenario_t::check ()` says
 * whether every label is not null, known and given once.
 */
#ifndef LUMEX_CORE_PROTOCOL_DSL_SCENARIO_HPP
#define LUMEX_CORE_PROTOCOL_DSL_SCENARIO_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#include "lumex/core/protocol/dsl/LumexProtocolOptions.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolRow.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolTypeList.hpp"
#include "lumex/core/protocol/field/LumexProtocolContext.hpp"
#include "lumex/core/protocol/field/LumexProtocolRecord.hpp"
#include "lumex/core/protocol/layout/LumexProtocolCodec.hpp"
#include "lumex/core/protocol/layout/LumexProtocolLayoutBase.hpp"
#include "lumex/core/protocol/layout/LumexProtocolMessage.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#endif

namespace lumex
{
namespace core
{
namespace protocol
{
namespace dsl
{
/// @brief The confirmation a `dangerous` row needs as its first argument.
struct confirm_t
{
};

/// @brief `send<Row> (confirm, args...)`, `client.ROW (confirm, args...)`.
LUMEX_CONSTEXPR confirm_t confirm{};

/// @brief What a step of a scenario is.
enum class scenario_step_kind : std::uint8_t
{
  /// Write a request and wait for its reply.
  send,
  /// Wait for a frame of a row.
  expect,
  /// Wait for a state of the device.
  wait_state,
  /// Go on at a label, by the last reply of a row or always.
  branch,
  /// A place to go to.
  label
};

/// @brief Appends the message of a request: the session values come from
/// `context`, the error is that of the encoder.
using request_encoder_t = std::function<std::error_code (
    ::lumex::core::protocol::layout::bytes_t &message,
    ::lumex::core::protocol::field::decode_context_t const &context)>;

/// @brief Decides a branch by the frame of its row; the error of the decoder
/// leaves `taken` as it was.
using branch_decider_t = std::function<std::error_code (
    ::lumex::core::protocol::layout::bytes_view_t message,
    ::lumex::core::protocol::field::decode_context_t const &context,
    bool &taken)>;

/// @brief One step of a scenario (see the file description).
struct scenario_step_t
{
  /// What the step does.
  scenario_step_kind kind = scenario_step_kind::label;
  /// The identifier of its row (`send`, `expect`, a branch on a reply), else
  /// null.
  char const *row = nullptr;
  /// The message of a `send`.
  request_encoder_t encode;
  /// The decision of a branch on a reply (empty: always, `jump`).
  branch_decider_t decide;
  /// The name of a `label`, the target of a branch.
  char const *label = nullptr;
  /// The state of a `wait_state`.
  std::int64_t state = 0;
  /// The time of an `expect` or a `wait_state`, in milliseconds (0: for ever).
  std::uint32_t timeout_ms = 0;
  /// The step takes the time the executor uses by default.
  bool default_timeout = false;
};

/// @brief A scenario: its name and its steps.
struct scenario_t
{
  /// The name for logs.
  char const *name = "";
  /// The steps, in order.
  std::vector<scenario_step_t> steps;

  /// @brief The position of label `wanted`, `steps.size ()` when there is
  /// none.
  std::size_t
  find_label (char const *wanted) const
  {
    if (wanted == nullptr)
      return steps.size ();
    for (std::size_t i = 0; i < steps.size (); ++i)
      if (steps[i].kind == scenario_step_kind::label
          && steps[i].label != nullptr
          && std::strcmp (steps[i].label, wanted) == 0)
        return i;
    return steps.size ();
  }

  /// @brief `std::errc::invalid_argument` when a label is null, a branch goes
  /// to a label that does not exist or a label is given twice, otherwise
  /// empty.
  std::error_code
  check () const
  {
    for (std::size_t i = 0; i < steps.size (); ++i)
      {
        scenario_step_t const &step = steps[i];
        if ((step.kind == scenario_step_kind::branch
             || step.kind == scenario_step_kind::label)
            && step.label == nullptr)
          return std::make_error_code (std::errc::invalid_argument);
        if (step.kind == scenario_step_kind::branch
            && find_label (step.label) == steps.size ())
          return std::make_error_code (std::errc::invalid_argument);
        if (step.kind == scenario_step_kind::label
            && find_label (step.label) != i)
          return std::make_error_code (std::errc::invalid_argument);
      }
    return std::error_code ();
  }
};

namespace detail
{
// The message of a send: the record made when the scenario was described.
template <typename P, typename Row> struct scenario_encoder_of_t
{
  ::lumex::core::protocol::layout::decode_result_t<
      typename ::lumex::core::protocol::layout::request_of<Row>::type>
      record;

  std::error_code
  operator() (
      ::lumex::core::protocol::layout::bytes_t &message,
      ::lumex::core::protocol::field::decode_context_t const &context) const
  {
    if (!record)
      return record.error ();
    ::lumex::core::protocol::layout::encode_result_t const encoded
        = ::lumex::core::protocol::layout::encode_message_record<P, Row> (
            message, *record, &context);
    return encoded ? std::error_code () : encoded.error ();
  }
};

// The decision of a branch: the predicate on the value of the reply of Row.
template <typename P, typename Row, typename Predicate>
struct scenario_decider_of_t
{
  Predicate predicate;

  std::error_code
  operator() (::lumex::core::protocol::layout::bytes_view_t message,
              ::lumex::core::protocol::field::decode_context_t const &context,
              bool &taken)
  {
    ::lumex::core::protocol::layout::decode_result_t<
        typename ::lumex::core::protocol::layout::reply_message_of<
            Row>::type> const decoded
        = ::lumex::core::protocol::layout::decode_reply_message<P, Row> (
            message, &context);
    if (!decoded)
      return decoded.error ();
    taken = predicate (*decoded);
    return std::error_code ();
  }
};

// Whether a row is a row of layer P, the replacements of the layer included.
template <typename P, typename Row>
struct scenario_row_t : list_contains<typename commands_of<P>::type, Row>
{
};
} // namespace detail

/**
 * @brief The words of a scenario of layer `P`, the base of the type
 * `LUMEX_PROTOCOL_SCENARIO` defines (see the file description).
 */
template <typename P> struct scenario_words_t
{
  /// The layer of the scenario.
  using protocol_type = P;

  /// @brief The confirmation of a `dangerous` row.
  static LUMEX_CONSTEXPR confirm_t confirm{};

  /// @brief Writes the message of `Row` and waits for its reply.
  template <typename Row, typename... Args>
  static scenario_step_t
  send (Args const &...args)
  {
    static_assert (detail::scenario_row_t<P, Row>::value,
                   "LUMEX_PROTOCOL_SCENARIO: send<Row>: Row is a row of the "
                   "protocol of the scenario");
    static_assert ((flags_of (Row::options ()) & row_flags::dangerous) == 0,
                   "LUMEX_PROTOCOL_SCENARIO: send<Row> of a dangerous row "
                   "needs confirm as its first argument");
    return send_step<Row> (args...);
  }

  /// @brief The same for a `dangerous` row: `send<Row> (confirm, args...)`.
  template <typename Row, typename... Args>
  static scenario_step_t
  send (confirm_t, Args const &...args)
  {
    static_assert (detail::scenario_row_t<P, Row>::value,
                   "LUMEX_PROTOCOL_SCENARIO: send<Row>: Row is a row of the "
                   "protocol of the scenario");
    static_assert ((flags_of (Row::options ()) & row_flags::dangerous) != 0,
                   "LUMEX_PROTOCOL_SCENARIO: confirm on a row that is not "
                   "dangerous");
    return send_step<Row> (args...);
  }

  /// @brief Waits for a frame of `Row`, the time the executor takes by
  /// default.
  template <typename Row>
  static scenario_step_t
  expect ()
  {
    scenario_step_t out = expect_step<Row> ();
    out.default_timeout = true;
    return out;
  }

  /// @brief Waits `milliseconds` for a frame of `Row` (0: for ever).
  template <typename Row>
  static scenario_step_t
  expect (std::uint32_t milliseconds)
  {
    scenario_step_t out = expect_step<Row> ();
    out.timeout_ms = milliseconds;
    return out;
  }

  /// @brief Waits for `state`, the time the executor takes by default.
  template <typename State>
  static scenario_step_t
  wait_state (State state)
  {
    scenario_step_t out = state_step (state);
    out.default_timeout = true;
    return out;
  }

  /// @brief Waits `milliseconds` for `state` (0: for ever).
  template <typename State>
  static scenario_step_t
  wait_state (State state, std::uint32_t milliseconds)
  {
    scenario_step_t out = state_step (state);
    out.timeout_ms = milliseconds;
    return out;
  }

  /// @brief Goes on at `target` when `predicate` says so for the value of the
  /// last reply of `Row`.
  template <typename Row, typename Predicate>
  static scenario_step_t
  branch_on_reply (Predicate predicate, char const *target)
  {
    static_assert (detail::scenario_row_t<P, Row>::value,
                   "LUMEX_PROTOCOL_SCENARIO: branch_on_reply<Row>: Row is a "
                   "row of the protocol of the scenario");
    static_assert (::lumex::core::protocol::layout::has_core_reply<Row>::value,
                   "LUMEX_PROTOCOL_SCENARIO: branch_on_reply<Row>: the reply "
                   "of Row is a field or a layout, not a reply kind");
    scenario_step_t out;
    out.kind = scenario_step_kind::branch;
    out.row = Row::identifier ();
    out.label = target;
    detail::scenario_decider_of_t<P, Row, Predicate> decider
        = { std::move (predicate) };
    out.decide = std::move (decider);
    return out;
  }

  /// @brief Goes on at `target`.
  static scenario_step_t
  jump (char const *target)
  {
    scenario_step_t out;
    out.kind = scenario_step_kind::branch;
    out.label = target;
    return out;
  }

  /// @brief A place to go to.
  static scenario_step_t
  label (char const *name)
  {
    scenario_step_t out;
    out.kind = scenario_step_kind::label;
    out.label = name;
    return out;
  }

private:
  template <typename Row, typename... Args>
  static scenario_step_t
  send_step (Args const &...args)
  {
    scenario_step_t out;
    out.kind = scenario_step_kind::send;
    out.row = Row::identifier ();
    detail::scenario_encoder_of_t<P, Row> encoder
        = { ::lumex::core::protocol::layout::make_request<Row> (args...) };
    out.encode = std::move (encoder);
    return out;
  }

  template <typename Row>
  static scenario_step_t
  expect_step ()
  {
    static_assert (detail::scenario_row_t<P, Row>::value,
                   "LUMEX_PROTOCOL_SCENARIO: expect<Row>: Row is a row of the "
                   "protocol of the scenario");
    scenario_step_t out;
    out.kind = scenario_step_kind::expect;
    out.row = Row::identifier ();
    return out;
  }

  template <typename State>
  static scenario_step_t
  state_step (State state)
  {
    static_assert (std::is_integral<State>::value
                       || std::is_enum<State>::value,
                   "LUMEX_PROTOCOL_SCENARIO: wait_state (state): the state is "
                   "an integer or an enumerator");
    scenario_step_t out;
    out.kind = scenario_step_kind::wait_state;
    out.state = static_cast<std::int64_t> (state);
    return out;
  }
};

#if !defined(__cpp_inline_variables)
template <typename P> LUMEX_CONSTEXPR confirm_t scenario_words_t<P>::confirm;
#endif

/// @brief The scenario of `steps` under `name` (what the macro calls).
inline scenario_t
make_scenario (char const *name, std::initializer_list<scenario_step_t> steps)
{
  scenario_t out;
  out.name = name;
  out.steps.assign (steps.begin (), steps.end ());
  return out;
}
} // namespace dsl
} // namespace protocol
} // namespace core
} // namespace lumex

/**
 * @brief Defines the scenario `name_` of protocol layer `protocol_` (see the
 * file description); each step stands in parentheses.
 */
#define LUMEX_PROTOCOL_SCENARIO(name_, protocol_, ...)                        \
  struct name_ : ::lumex::core::protocol::dsl::scenario_words_t<protocol_>    \
  {                                                                           \
    static ::lumex::core::protocol::dsl::scenario_t                           \
    make ()                                                                   \
    {                                                                         \
      return ::lumex::core::protocol::dsl::make_scenario (#name_,             \
                                                          { __VA_ARGS__ });   \
    }                                                                         \
  }

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_DSL_SCENARIO_HPP
