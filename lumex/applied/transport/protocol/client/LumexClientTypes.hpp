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
 * @file LumexClientTypes.hpp
 * @brief The types of the generated client of `applied/transport/protocol`:
 * the confirmation of a dangerous row, what a row returns, the identity of a
 * device, the options of a client.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_PROTOCOL_CLIENT_CLIENT_TYPES_HPP
#define LUMEX_APPLIED_TRANSPORT_PROTOCOL_CLIENT_CLIENT_TYPES_HPP

#include <cstdint>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "lumex/applied/transport/protocol/LumexDialogueTypes.hpp"
#include "lumex/applied/transport/protocol/dialogue/LumexDialogueFeatures.hpp"
#include "lumex/applied/transport/protocol/dsl/LumexDialogueVocabulary.hpp"
#include "lumex/core/expected/Expected"
#include "lumex/core/protocol/dsl/LumexProtocolScenario.hpp"

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
/// @brief The confirmation a `dangerous` row needs as its first argument, the
/// type a scenario takes too.
using ::lumex::core::protocol::dsl::confirm_t;

/// @brief `client.ROW (confirm, args...)`.
using ::lumex::core::protocol::dsl::confirm;

/// @brief What the client gives for `Row`: the value of a field or a layout,
/// `field::record<>` without a reply, the value of the message of
/// `reply_row<R>`, the `value_type` of a reply kind of the dialogue.
template <typename Row>
using reply_of = typename dialogue::reply_value_of_t<Row>::type;

/// @brief The result of a synchronous call of `Row`.
template <typename Row>
using call_result_t
    = ::lumex::core::expected::result::expected<reply_of<Row>,
                                                std::error_code>;

/**
 * @brief What a device says of itself in the reply of `identify_with<Row>`.
 * @details `identity_of_t<P>::read` fills it. `model`, `firmware` and `values`
 * go into the session of the client (`session_context_t`), which the decoders
 * and the availability of the rows read; `layer` names the layer of
 * `layer_set<...>` the device speaks.
 */
struct identity_t
{
  /// The model name, empty when the device does not say.
  std::string model;
  /// The firmware version as the table writes it (see `firmware (from, to)`),
  /// 0 when the device does not say.
  std::uint32_t firmware = 0;
  /// The serial number, empty when the device does not say.
  std::string serial_number;
  /// The `protocol_name ()` of the layer of the device; without `layer_set`
  /// the client puts the name of its own layer here.
  std::string layer;
  /// Named values for the session (the key of a `union_by_context_`).
  std::vector<std::pair<std::string, std::int64_t>> values;
};

/// @brief The result of a synchronous `identify ()`.
using identity_result_t
    = ::lumex::core::expected::result::expected<identity_t, std::error_code>;

/**
 * @brief How the reply of `identify_with<Row>` of layer `P` reads as an
 * identity; the author of the table specializes it, for the layer that names
 * the row or a base of it (a layer takes what its base says).
 * @code
 * template <> struct identity_of_t<my_base>
 * {
 *   static std::error_code
 *   read (reply_of<my_base::INFO_t> const &reply, identity_t &identity)
 *   {
 *     identity.firmware = field::get<1> (reply);
 *     identity.layer = field::get<2> (reply) == 2 ? "my_v2" : "my_v1";
 *     return std::error_code ();
 *   }
 * };
 * @endcode
 * An error ends the identification with it.
 */
template <typename P>
struct identity_of_t : identity_of_t<typename P::base_protocol>
{
};

/// @brief The end of the chain: nobody said how the identity reads.
template <> struct identity_of_t<void>
{
  /// Marks the end of the chain.
  using lumex_no_identity = void;
};

/// @brief The options of a `client`.
struct client_options_t
{
  /// The options of its dialogue: `dialogue::options_for<P> ()` holds what
  /// the table says, `client_options_for<P> ()` starts from them. The
  /// features of the table are put in when empty.
  dialogue_options_t dialogue;
};

/// @brief The options of a client of layer `P` as the table says.
template <typename P>
client_options_t
client_options_for ()
{
  client_options_t out;
  out.dialogue = dialogue::options_for<P> ();
  return out;
}
} // namespace protocol
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_PROTOCOL_CLIENT_CLIENT_TYPES_HPP
