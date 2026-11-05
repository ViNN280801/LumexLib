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
 * @file LumexDiscover.hpp
 * @brief `async_discover<P, Wire>`: finds the devices of protocol `P` among
 * candidates: serial ports, HID devices, network peers, factories of one's
 * own.
 * @details Each candidate is probed on its own: its transport is made and
 * opened, the out-of-band sequence of the wire (if it has one) is written, the
 * link of the wire goes over the transport and the identification of the table
 * runs (`identify_with<Row>`, `identity_of_t<P>`, `layer_set<...>`, as for
 * `client<P>::identify ()`). At most `discovery_options_t::parallel` probes
 * run at once; each has `per_candidate` for all of it. A probe ends with
 * `discovery_result_t`: found, with the identity and the layer in it, or the
 * reason:
 * - `discovery_errc::open_timeout`: the transport did not open in time;
 * - `discovery_errc::busy`: something else holds it (`EBUSY`; on Windows also
 *   the access denied of a COM port);
 * - `discovery_errc::no_response`: it opened and nothing answered in time, or
 *   the link went while waiting for the answer;
 * - `discovery_errc::wrong_device`: it answered and it is no device of the
 *   table: another row, a reply that does not read, a layer that is not in
 *   `layer_set` (the identity then holds what it said);
 * - any other error of the factory, the open or the wire as it is (a port that
 *   does not exist, `permission_denied`, ...); `not_supported` for a candidate
 *   of a kind the wire has no link for.
 *
 * The wire says how a candidate speaks, since a device may use another framing
 * on another transport:
 * @code
 * struct my_wire_t
 * {
 *   static protocol::frame_link
 *   link (discovery_candidate_t const &, stream::byte_stream opened)
 *   {
 *     return protocol::frame_link (framed::framed_stream<slip_codec> (
 *         std::move (opened), slip_codec ()));
 *   }
 *   // Optional, for a transport of messages: the same with a
 *   // datagram::message_transport and a framed_datagram.
 *   // Optional: written as they are after the open, before the framing (a
 *   // device that has to be switched to the protocol first). Empty: nothing.
 *   static std::vector<std::uint8_t>
 *   out_of_band (discovery_candidate_t const &);
 * };
 * async_discover<my_table_t, my_wire_t> (
 *     runner.get_executor (), candidates, discovery_options_t (),
 *     [] (std::error_code error, std::vector<discovery_result_t> results) {});
 * @endcode
 *
 * The token gets `void (std::error_code, std::vector<discovery_result_t>)`,
 * with one result per candidate, in their order: no error when every probe
 * ended, `cancelled` when a `cancellation_type::terminal`, `partial` or
 * `total` on the slot of the handler ended the discovery (a probe it ended has
 * the reason `cancelled`, and those that had not started too). The completion
 * comes exactly once, never inside the call, on the associated executor of the
 * handler, and no work is kept on that executor. An empty list completes with
 * no error and no result. Each probe closes what it opened before the next one
 * starts in its place, also on a deadline. The discovery is an operation, not
 * an object: it ends by cancellation, and when the `io_context` is destroyed
 * first its handler is destroyed uncalled.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_PROTOCOL_DISCOVERY_DISCOVER_HPP
#define LUMEX_APPLIED_TRANSPORT_PROTOCOL_DISCOVERY_DISCOVER_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#include "lumex/applied/transport/datagram/LumexMessageTransport.hpp"
#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexCountingHandler.hpp"
#include "lumex/applied/transport/detail/LumexErasedHandlers.hpp"
#include "lumex/applied/transport/protocol/client/LumexClientTypes.hpp"
#include "lumex/applied/transport/protocol/client/LumexProtocolClient.hpp"
#include "lumex/applied/transport/protocol/link/LumexFrameLink.hpp"
#include "lumex/applied/transport/session/LumexSessionSelectors.hpp"
#include "lumex/applied/transport/stream/LumexByteStream.hpp"
#include "lumex/applied/transport/timing/LumexTimerSource.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(LUMEX_TRANSPORT_WITH_HID)
#include "lumex/applied/transport/hid/LumexHidSelectors.hpp"
#endif

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
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
/// @brief Why a candidate is no device of the table (see the file
/// description).
enum class discovery_errc : int
{
  /// The transport did not open within the time of the candidate.
  open_timeout = 1,
  /// It opened and nothing answered.
  no_response,
  /// It answered and it is no device of the table.
  wrong_device,
  /// Something else holds it.
  busy
};

/// @brief The error category "lumex.discovery".
std::error_category const &discovery_category () LUMEX_NOEXCEPT;

/// @brief The error code of `value`.
std::error_code make_error_code (discovery_errc value) LUMEX_NOEXCEPT;

/// @brief One place a device may be.
struct discovery_candidate_t
{
  /// Which of the members below says where.
  enum class candidate_kind : std::uint8_t
  {
    /// `serial`: a stream.
    serial,
    /// `hid`: messages (only with `LUMEX_TRANSPORT_WITH_HID`).
    hid,
    /// `network`: a TCP stream.
    network,
    /// `network`: UDP messages.
    network_datagram,
    /// `stream_factory`: a stream of one's own.
    stream,
    /// `message_factory`: messages of one's own.
    message
  };

  /// Which one it is.
  candidate_kind kind = candidate_kind::serial;
  session::serial_selector_t serial;
#if defined(LUMEX_TRANSPORT_WITH_HID)
  hid::hid_selector_t hid;
#endif
  session::net_selector_t network;
  session::stream_session::factory_t stream_factory;
  session::message_session::factory_t message_factory;
};

/// @brief What a probe found.
struct discovery_result_t
{
  /// The candidate.
  discovery_candidate_t candidate;
  /// A device of the table is there.
  bool found = false;
  /// What it said of itself (`layer`: the layer of the table it speaks); also
  /// what a `wrong_device` said, as far as it read.
  identity_t identity;
  /// Why not (empty when found).
  std::error_code reason;
};

/// @brief How a discovery runs.
struct discovery_options_t
{
  /// The time of one candidate, from the start of its probe to the end of its
  /// identification (0: no limit but the timeouts of the row).
  std::chrono::milliseconds per_candidate = std::chrono::milliseconds (1000);
  /// How many probes run at once (0 is taken as 1).
  std::size_t parallel = 4;
  /// The wait after the out-of-band sequence, before the identification.
  std::chrono::milliseconds out_of_band_pause = std::chrono::milliseconds (0);
  /// The timers of the discovery and of the identification (the steady source
  /// by default; a `testing::virtual_clock` in tests).
  timing::timer_source timers;
};

/// @brief The completion of a discovery.
using discovery_signature_t
    = void (std::error_code, std::vector<discovery_result_t>);

namespace detail
{
/// @brief The end of an identification, on the strand of its dialogue.
using discovery_identified_t = std::function<void (
    std::error_code const &error, identity_t const &identity)>;

/// @brief What a table and a wire give the discovery, which is no template.
struct discovery_ops_t
{
  /// The link over an open stream; null: the wire has none.
  frame_link (*stream_link) (discovery_candidate_t const &,
                             stream::byte_stream)
      = nullptr;
  /// The link over an open message transport; null: the wire has none.
  frame_link (*message_link) (discovery_candidate_t const &,
                              datagram::message_transport)
      = nullptr;
  /// The out-of-band sequence; null: the wire has none.
  std::vector<std::uint8_t> (*out_of_band) (discovery_candidate_t const &)
      = nullptr;
  /// Starts the identification over the link; what it gives keeps it going
  /// (dropping it closes the dialogue).
  std::shared_ptr<void> (*identify) (frame_link, timing::timer_source const &,
                                     discovery_identified_t)
      = nullptr;
};

/// @brief The handler of a discovery as it is kept (erased once).
using discovery_handler_t
    = asio::any_completion_handler<discovery_signature_t>;

/// @brief Starts a discovery on `strand`.
void start_discovery (
    asio::strand<executor_type> const &strand,
    std::vector<discovery_candidate_t> candidates,
    discovery_options_t const &options, discovery_ops_t const &ops,
    discovery_handler_t handler,
    ::lumex::applied::transport::detail::operation_mark_ptr const &mark);

template <typename Wire, typename = void> struct wire_stream_link_t
{
  static LUMEX_CONSTEXPR
  frame_link (*const value) (discovery_candidate_t const &,
                             stream::byte_stream)
      = nullptr;
};

template <typename Wire>
struct wire_stream_link_t<
    Wire, typename dialogue::detail::voider_t<decltype (Wire::link (
              std::declval<discovery_candidate_t const &> (),
              std::declval<stream::byte_stream> ()))>::type>
{
  static frame_link
  value (discovery_candidate_t const &candidate, stream::byte_stream opened)
  {
    return Wire::link (candidate, std::move (opened));
  }
};

template <typename Wire, typename = void> struct wire_message_link_t
{
  static LUMEX_CONSTEXPR
  frame_link (*const value) (discovery_candidate_t const &,
                             datagram::message_transport)
      = nullptr;
};

template <typename Wire>
struct wire_message_link_t<
    Wire, typename dialogue::detail::voider_t<decltype (Wire::link (
              std::declval<discovery_candidate_t const &> (),
              std::declval<datagram::message_transport> ()))>::type>
{
  static frame_link
  value (discovery_candidate_t const &candidate,
         datagram::message_transport opened)
  {
    return Wire::link (candidate, std::move (opened));
  }
};

template <typename Wire, typename = void> struct wire_out_of_band_t
{
  static LUMEX_CONSTEXPR
  std::vector<std::uint8_t> (*const value) (discovery_candidate_t const &)
      = nullptr;
};

template <typename Wire>
struct wire_out_of_band_t<
    Wire, typename dialogue::detail::voider_t<decltype (Wire::out_of_band (
              std::declval<discovery_candidate_t const &> ()))>::type>
{
  static std::vector<std::uint8_t>
  value (discovery_candidate_t const &candidate)
  {
    return Wire::out_of_band (candidate);
  }
};

/// @brief The end of the identification of a probe.
struct discovery_identify_end_t
{
  discovery_identified_t done;

  void
  operator() (std::error_code error, identity_t identity)
  {
    done (error, identity);
  }
};

/// @brief The identification of table `P` over `link`.
template <typename P>
std::shared_ptr<void>
discovery_identify (frame_link link, timing::timer_source const &timers,
                    discovery_identified_t done)
{
  namespace index = ::lumex::core::protocol::index;
  static_assert (identify_check_t<P>::value,
                 "LUMEX_PROTOCOL: async_discover: the identification of P");
  static index::protocol_index_t const nothing;
  index::index_result_t const &indexed = index::protocol_index<P> ();
  // Nothing of the table but its identification: a probe sends no keepalive,
  // no stream row, nothing else to what may be another device.
  dialogue_options_t options = dialogue::options_for<P> ();
  options.features = nullptr;
  options.timers = timers;
  std::shared_ptr<dialogue_engine> const engine
      = std::make_shared<dialogue_engine> (
          std::move (link), indexed ? *indexed : nothing, options);
  std::shared_ptr<client_state_t> const state
      = std::make_shared<client_state_t> ();
  state->set_engine (engine);
  discovery_identify_end_t ended = { std::move (done) };
  initiate_identify_t<P> initiate (executor_type (engine->get_strand ()));
  initiate (std::move (ended), std::weak_ptr<dialogue_engine> (engine), state);
  return engine;
}

/// @brief What `P` and `Wire` give the discovery.
template <typename P, typename Wire>
discovery_ops_t
discovery_ops_of () LUMEX_NOEXCEPT
{
  static_assert (
      !std::is_void<
          typename dialogue::protocol_traits_t<P>::identify_row>::value,
      "LUMEX_PROTOCOL: async_discover: the protocol names no "
      "identify_with<Row>");
  discovery_ops_t made;
  made.stream_link = wire_stream_link_t<Wire>::value;
  made.message_link = wire_message_link_t<Wire>::value;
  made.out_of_band = wire_out_of_band_t<Wire>::value;
  made.identify = &discovery_identify<P>;
  return made;
}

/// @brief The initiating function of a discovery; it reports its strand, as
/// the transports do.
class initiate_discover_t
{
public:
  using executor_type = transport::executor_type;

  explicit initiate_discover_t (asio::strand<executor_type> const &strand)
      : strand_ (strand)
  {
  }

  executor_type
  get_executor () const LUMEX_NOEXCEPT
  {
    return executor_type (strand_);
  }

  template <typename Handler>
  void
  operator() (Handler &&handler, std::vector<discovery_candidate_t> candidates,
              discovery_options_t options, discovery_ops_t ops) const
  {
    ::lumex::applied::transport::detail::operation_mark_ptr const mark
        = ::lumex::applied::transport::detail::cancellation_mark (handler);
    start_discovery (
        strand_, std::move (candidates), options, ops,
        ::lumex::applied::transport::detail::erase_once<discovery_handler_t> (
            std::forward<Handler> (handler), mark),
        mark);
  }

private:
  asio::strand<executor_type> strand_;
};
} // namespace detail

/// @brief Finds the devices of `P` among `candidates` (see the file
/// description).
template <typename P, typename Wire, typename Token>
auto
async_discover (executor_type const &executor,
                std::vector<discovery_candidate_t> candidates,
                discovery_options_t options, Token &&token)
    -> decltype (asio::async_initiate<Token, discovery_signature_t> (
        std::declval<detail::initiate_discover_t> (), token,
        std::declval<std::vector<discovery_candidate_t>> (),
        std::declval<discovery_options_t> (),
        std::declval<detail::discovery_ops_t> ()))
{
  return asio::async_initiate<Token, discovery_signature_t> (
      detail::initiate_discover_t (asio::make_strand (executor)), token,
      std::move (candidates), std::move (options),
      detail::discovery_ops_of<P, Wire> ());
}
} // namespace protocol
} // namespace transport
} // namespace applied
} // namespace lumex

namespace std
{
template <>
struct is_error_code_enum<lumex::applied::transport::protocol::discovery_errc>
    : true_type
{
};
} // namespace std

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_PROTOCOL_DISCOVERY_DISCOVER_HPP
