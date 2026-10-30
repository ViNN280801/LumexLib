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
 * @file LumexFrameLink.hpp
 * @brief `frame_link`: a framed link of any kind behind one type, the link
 * a `dialogue_engine` talks over: a `framed::framed_stream<Codec>`, a
 * `framed::framed_datagram<Codec>` with the report ID of its requests, or an
 * implementation of `frame_link_impl` of one's own.
 * @details The link owns the adapter it is given (moved in), and with it the
 * transport underneath; open the transport before handing the adapter over.
 * A frame is the codec payload: one whole message of the protocol. The
 * operations follow the adapter they wrap: they run on its strand (the
 * strand of the transport, `get_strand ()`), a completion never runs inside
 * the call that started it and runs on its handler's associated executor,
 * no work is kept on that executor, `cancel ()` ends every pending read and
 * write with `cancelled` (the link stays open), `close ()` ends them and
 * closes the transport, and a `cancellation_type::terminal` emitted on the
 * slot of a pending read or write does what `cancel ()` does. A datagram
 * link writes every frame in a report with its report ID and delivers the
 * payload of every frame it receives, whatever its report ID. Moved from
 * (or default constructed) it is empty: a read or a write ends with
 * `not_open` on the handler's associated executor.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_PROTOCOL_LINK_FRAME_LINK_HPP
#define LUMEX_APPLIED_TRANSPORT_PROTOCOL_LINK_FRAME_LINK_HPP

#include <cstdint>
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
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/framed/LumexFramedDatagram.hpp"
#include "lumex/applied/transport/framed/LumexFramedStream.hpp"
#include "lumex/applied/transport/stream/LumexStreamTypes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#if __has_warning("-Wlifetime-safety-suggestions")
// Clang 23 suggests [[clang::lifetimebound]] for every function that returns
// or stores a parameter (a suggestion, not a finding).
#pragma clang diagnostic ignored "-Wlifetime-safety-suggestions"
#endif
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
/// @brief One frame of a link: the codec payload.
using link_frame_t = std::vector<std::uint8_t>;

/// @brief The completion of a frame read of a link, as an implementation
/// receives it (erased once).
using link_read_handler_t
    = asio::any_completion_handler<void (std::error_code, link_frame_t)>;

/**
 * @brief The implementation behind a `frame_link` (see the file
 * description for the rules every implementation keeps).
 * @details Every member may be called from any thread.
 */
class frame_link_impl
{
public:
  /// @brief The strand of the link's operations.
  using strand_type = asio::strand<executor_type>;

  frame_link_impl () = default;
  virtual ~frame_link_impl ();
  frame_link_impl (frame_link_impl const &) = delete;
  frame_link_impl &operator= (frame_link_impl const &) = delete;

  /// @brief The executor of the transport underneath.
  virtual executor_type get_executor () const = 0;
  /// @brief The strand every operation runs on.
  virtual strand_type get_strand () const = 0;
  /// @brief The name for logs (the transport's).
  virtual std::string const &name () const = 0;
  /// @brief True while the transport is open.
  virtual bool is_open () const = 0;
  /// @brief Reads the next frame.
  virtual void start_read (link_read_handler_t handler) = 0;
  /// @brief Writes `frame` as one frame (kept alive until the handler ran).
  virtual void start_write (std::shared_ptr<link_frame_t const> frame,
                            status_handler_t handler)
      = 0;
  /// @brief Ends the pending reads and writes with `cancelled`.
  virtual void cancel () LUMEX_NOEXCEPT = 0;
  /// @brief Ends them and closes the transport.
  virtual void close () LUMEX_NOEXCEPT = 0;
};

namespace detail
{
/**
 * @brief The completion of a datagram read handed on as the payload alone;
 * every association of the wrapped handler is forwarded but an erased
 * handler's view allocator (see `LumexErasedHandlers.hpp`).
 */
struct payload_handler_t
{
  link_read_handler_t handler;

  void
  operator() (std::error_code error, datagram::message_t message)
  {
    std::move (handler) (error, std::move (message.payload));
  }
};

/// @brief A `framed_stream` behind `frame_link_impl`.
template <typename Codec> class stream_link_impl final : public frame_link_impl
{
public:
  explicit stream_link_impl (framed::framed_stream<Codec> link)
      : link_ (std::move (link))
  {
  }

  executor_type
  get_executor () const override
  {
    return link_.get_executor ();
  }

  strand_type
  get_strand () const override
  {
    return link_.get_strand ();
  }

  std::string const &
  name () const override
  {
    return link_.name ();
  }

  bool
  is_open () const override
  {
    return link_.is_open ();
  }

  void
  start_read (link_read_handler_t handler) override
  {
    link_.async_read_frame (std::move (handler));
  }

  void
  start_write (std::shared_ptr<link_frame_t const> frame,
               status_handler_t handler) override
  {
    const_bytes_t const payload (frame->data (), frame->size ());
    link_.async_write_frame (payload,
                             asio::consign (std::move (handler), frame));
  }

  void
  cancel () LUMEX_NOEXCEPT override
  {
    link_.cancel ();
  }

  void
  close () LUMEX_NOEXCEPT override
  {
    link_.close ();
  }

private:
  framed::framed_stream<Codec> link_;
};

/// @brief A `framed_datagram` behind `frame_link_impl`, writing with one
/// report ID.
template <typename Codec>
class datagram_link_impl final : public frame_link_impl
{
public:
  datagram_link_impl (framed::framed_datagram<Codec> link,
                      std::uint8_t report_id)
      : link_ (std::move (link)), report_id_ (report_id)
  {
  }

  executor_type
  get_executor () const override
  {
    return link_.get_executor ();
  }

  strand_type
  get_strand () const override
  {
    return link_.get_strand ();
  }

  std::string const &
  name () const override
  {
    return link_.name ();
  }

  bool
  is_open () const override
  {
    return link_.is_open ();
  }

  void
  start_read (link_read_handler_t handler) override
  {
    payload_handler_t wrapped = { std::move (handler) };
    link_.async_read_frame (std::move (wrapped));
  }

  void
  start_write (std::shared_ptr<link_frame_t const> frame,
               status_handler_t handler) override
  {
    const_bytes_t const payload (frame->data (), frame->size ());
    link_.async_write_frame (report_id_, payload,
                             asio::consign (std::move (handler), frame));
  }

  void
  cancel () LUMEX_NOEXCEPT override
  {
    link_.cancel ();
  }

  void
  close () LUMEX_NOEXCEPT override
  {
    link_.close ();
  }

private:
  framed::framed_datagram<Codec> link_;
  std::uint8_t report_id_;
};
} // namespace detail

/// @brief A framed link of any kind (see the file description).
class frame_link
{
  // The initiating functions report the strand of the link (the system
  // executor for an empty link), as the adapters do.
  struct initiate_base
  {
    using executor_type = transport::executor_type;

    explicit initiate_base (executor_type const &executor)
        : executor_ (executor)
    {
    }

    executor_type
    get_executor () const LUMEX_NOEXCEPT
    {
      return executor_;
    }

  private:
    executor_type executor_;
  };

  struct initiate_read : initiate_base
  {
    explicit initiate_read (executor_type const &executor)
        : initiate_base (executor)
    {
    }

    template <typename Handler>
    void
    operator() (Handler &&handler,
                std::shared_ptr<frame_link_impl> const &impl) const
    {
      if (!impl)
        {
          ::lumex::applied::transport::detail::complete_detached (
              std::forward<Handler> (handler),
              make_error_code (error::transport_errc::not_open),
              link_frame_t ());
          return;
        }
      impl->start_read (::lumex::applied::transport::detail::erase_once<
                        link_read_handler_t> (
          std::forward<Handler> (handler),
          ::lumex::applied::transport::detail::operation_mark_ptr ()));
    }
  };

  struct initiate_write : initiate_base
  {
    explicit initiate_write (executor_type const &executor)
        : initiate_base (executor)
    {
    }

    template <typename Handler>
    void
    operator() (Handler &&handler,
                std::shared_ptr<frame_link_impl> const &impl,
                std::shared_ptr<link_frame_t const> const &frame) const
    {
      if (!impl)
        {
          ::lumex::applied::transport::detail::complete_detached (
              std::forward<Handler> (handler),
              make_error_code (error::transport_errc::not_open));
          return;
        }
      impl->start_write (
          frame,
          ::lumex::applied::transport::detail::erase_once<status_handler_t> (
              std::forward<Handler> (handler),
              ::lumex::applied::transport::detail::operation_mark_ptr ()));
    }
  };

public:
  /// @brief The executor type the initiating functions report.
  using executor_type = transport::executor_type;
  /// @brief The strand of the link's operations.
  using strand_type = frame_link_impl::strand_type;

  /// @brief An empty link.
  frame_link () LUMEX_NOEXCEPT;

  /// @brief The frames of `link` (not empty; moved in).
  template <typename Codec>
  explicit frame_link (framed::framed_stream<Codec> link)
      : impl_ (std::make_shared<detail::stream_link_impl<Codec>> (
            std::move (link)))
  {
  }

  /// @brief The frames of `link` (not empty; moved in); every frame is
  /// written in a report with `report_id`.
  template <typename Codec>
  frame_link (framed::framed_datagram<Codec> link, std::uint8_t report_id)
      : impl_ (std::make_shared<detail::datagram_link_impl<Codec>> (
            std::move (link), report_id))
  {
  }

  /// @brief A link over an implementation of one's own (null: empty).
  explicit frame_link (std::shared_ptr<frame_link_impl> impl) LUMEX_NOEXCEPT;

  /// @brief Closes (asynchronously, through the adapter) and returns.
  ~frame_link ();

  frame_link (frame_link const &) = delete;
  frame_link &operator= (frame_link const &) = delete;
  frame_link (frame_link &&other) LUMEX_NOEXCEPT;
  frame_link &operator= (frame_link &&other) LUMEX_NOEXCEPT;

  /// @brief True for an empty link.
  bool
  empty () const LUMEX_NOEXCEPT
  {
    return !impl_;
  }

  /// @brief The executor of the transport (an empty one for an empty link).
  executor_type get_executor () const;

  /// @brief The strand of the link's operations (not for an empty link).
  strand_type get_strand () const;

  /// @brief The name for logs (`""` for an empty link).
  std::string const &name () const;

  /// @brief True while the transport is open.
  bool is_open () const;

  /// @brief Reads the next frame; the token gets
  /// `void (std::error_code, link_frame_t)`.
  template <typename Token>
  auto
  async_read_frame (Token &&token)
      -> decltype (asio::async_initiate<Token,
                                        void (std::error_code, link_frame_t)> (
          std::declval<initiate_read> (), token,
          std::declval<std::shared_ptr<frame_link_impl> &> ()))
  {
    return asio::async_initiate<Token, void (std::error_code, link_frame_t)> (
        initiate_read (initiation_executor ()), token, impl_);
  }

  /// @brief Writes `frame` as one frame; the token gets
  /// `void (std::error_code)`.
  template <typename Token>
  auto
  async_write_frame (link_frame_t frame, Token &&token)
      -> decltype (asio::async_initiate<Token, void (std::error_code)> (
          std::declval<initiate_write> (), token,
          std::declval<std::shared_ptr<frame_link_impl> &> (),
          std::declval<std::shared_ptr<link_frame_t const> const &> ()))
  {
    std::shared_ptr<link_frame_t const> const shared
        = std::make_shared<link_frame_t const> (std::move (frame));
    return asio::async_initiate<Token, void (std::error_code)> (
        initiate_write (initiation_executor ()), token, impl_, shared);
  }

  /// @brief Ends the pending reads and writes with `cancelled`; the link
  /// stays open (any thread).
  void cancel () LUMEX_NOEXCEPT;

  /// @brief Ends them and closes the transport (any thread).
  void close () LUMEX_NOEXCEPT;

  /// @brief The implementation, for the dialogue.
  std::shared_ptr<frame_link_impl> const &
  impl () const LUMEX_NOEXCEPT
  {
    return impl_;
  }

private:
  executor_type initiation_executor () const;

  std::shared_ptr<frame_link_impl> impl_;
};
} // namespace protocol
} // namespace transport
} // namespace applied
} // namespace lumex

namespace asio
{
template <template <typename, typename> class Associator,
          typename DefaultCandidate>
struct associator<
    Associator, lumex::applied::transport::protocol::detail::payload_handler_t,
    DefaultCandidate>
    : Associator<lumex::applied::transport::protocol::link_read_handler_t,
                 DefaultCandidate>
{
  using inner_type = lumex::applied::transport::protocol::link_read_handler_t;
  using wrapper_type
      = lumex::applied::transport::protocol::detail::payload_handler_t;

  static typename Associator<inner_type, DefaultCandidate>::type
  get (wrapper_type const &wrapper) LUMEX_NOEXCEPT
  {
    return Associator<inner_type, DefaultCandidate>::get (wrapper.handler);
  }

  static auto
  get (wrapper_type const &wrapper,
       DefaultCandidate const &candidate) LUMEX_NOEXCEPT
      -> decltype (Associator<inner_type, DefaultCandidate>::get (
          wrapper.handler, candidate))
  {
    return Associator<inner_type, DefaultCandidate>::get (wrapper.handler,
                                                          candidate);
  }
};

template <typename DefaultCandidate>
struct associator<
    associated_allocator,
    lumex::applied::transport::protocol::detail::payload_handler_t,
    DefaultCandidate>
{
  using wrapper_type
      = lumex::applied::transport::protocol::detail::payload_handler_t;
  using shield_type = lumex::applied::transport::detail::shielded_allocator<
      lumex::applied::transport::protocol::link_read_handler_t,
      DefaultCandidate>;
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

#endif // !LUMEX_APPLIED_TRANSPORT_PROTOCOL_LINK_FRAME_LINK_HPP
