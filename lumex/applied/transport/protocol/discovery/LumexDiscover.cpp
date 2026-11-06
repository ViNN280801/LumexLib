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

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexCountingHandler.hpp"
#include "lumex/applied/transport/detail/LumexHandlerKeeper.hpp"
#include "lumex/applied/transport/error/LumexTransportErrc.hpp"
#include "lumex/applied/transport/log/LumexTransportLog.hpp"
#include "lumex/applied/transport/protocol/discovery/LumexDiscover.hpp"
#include "lumex/applied/transport/session/LumexSessionSelectors.hpp"
#include "lumex/core/protocol/error/LumexProtocolErrc.hpp"

#if defined(__clang__)
// The library's clang-cl set: -Wall there is -Weverything (as in the other
// modules' sources).
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#if __has_warning("-Wnrvo")
#pragma clang diagnostic ignored "-Wnrvo"
#endif
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

namespace lumex
{
namespace applied
{
namespace transport
{
namespace protocol
{
namespace
{
namespace tdetail = ::lumex::applied::transport::detail;
using error::transport_errc;
using ::lumex::core::protocol::error::protocol_errc;

class discovery_category_t final : public std::error_category
{
public:
  char const *
  name () const LUMEX_NOEXCEPT override
  {
    return "lumex.discovery";
  }

  std::string
  message (int value) const override
  {
    switch (static_cast<discovery_errc> (value))
      {
      case discovery_errc::open_timeout:
        return "the transport did not open in time";
      case discovery_errc::no_response:
        return "nothing answered";
      case discovery_errc::wrong_device:
        return "it is no device of the table";
      case discovery_errc::busy:
        return "something else holds the device";
      default:
        break;
      }
    return "unknown discovery error";
  }
};

bool
is_busy (std::error_code const &error) LUMEX_NOEXCEPT
{
#if defined(_WIN32)
  // Opening a COM port someone holds is an access denied there.
  if (error == std::errc::permission_denied)
    return true;
#endif
  return error == std::errc::device_or_resource_busy;
}

// What an error of the open or the factory says of the candidate.
std::error_code
reason_of_open (std::error_code const &error) LUMEX_NOEXCEPT
{
  if (error::is_transport_error (error, transport_errc::timeout))
    return make_error_code (discovery_errc::open_timeout);
  if (is_busy (error))
    return make_error_code (discovery_errc::busy);
  return error;
}

// Whether a protocol error means a reply that does not fit the table.
bool
reply_does_not_fit (std::error_code const &error) LUMEX_NOEXCEPT
{
  if (error.category ()
      != ::lumex::core::protocol::error::protocol_category ())
    return false;
  switch (static_cast<protocol_errc> (error.value ()))
    {
    case protocol_errc::truncated:
    case protocol_errc::trailing_bytes:
    case protocol_errc::invalid_value:
    case protocol_errc::unknown_frame:
    case protocol_errc::limit_exceeded:
    case protocol_errc::unknown_case:
    case protocol_errc::not_supported:
    case protocol_errc::nak:
    case protocol_errc::refused:
    case protocol_errc::unexpected_reply:
      return true;
    case protocol_errc::access_denied:
    case protocol_errc::duplicate_key:
    case protocol_errc::out_of_limits:
    case protocol_errc::timeout:
    case protocol_errc::retries_exhausted:
    case protocol_errc::not_accepted_in_state:
    case protocol_errc::link_lost:
    case protocol_errc::not_confirmed:
    case protocol_errc::queue_full:
    default:
      return false;
    }
}

// What an error of the identification says of the candidate: it opened, so
// anything the link or the time did is silence, a reply that does not fit is
// another device, and any other error stays as it is.
std::error_code
reason_of_identify (std::error_code const &error) LUMEX_NOEXCEPT
{
  if (error
          == ::lumex::core::protocol::error::make_error_code (
              protocol_errc::timeout)
      || error
             == ::lumex::core::protocol::error::make_error_code (
                 protocol_errc::retries_exhausted)
      || error
             == ::lumex::core::protocol::error::make_error_code (
                 protocol_errc::link_lost)
      || error.category () == error::transport_category ())
    return make_error_code (discovery_errc::no_response);
  if (reply_does_not_fit (error))
    return make_error_code (discovery_errc::wrong_device);
  return error;
}
} // namespace

std::error_category const &
discovery_category () LUMEX_NOEXCEPT
{
  static discovery_category_t const category;
  return category;
}

std::error_code
make_error_code (discovery_errc value) LUMEX_NOEXCEPT
{
  return std::error_code (static_cast<int> (value), discovery_category ());
}

namespace detail
{
namespace
{
/// @brief A discovery under way: everything on its strand.
class discovery_run_t final
    : public tdetail::handler_keeper,
      public std::enable_shared_from_this<discovery_run_t>
{
public:
  discovery_run_t (asio::strand<executor_type> const &strand,
                   std::vector<discovery_candidate_t> candidates,
                   discovery_options_t const &options,
                   discovery_ops_t const &ops, discovery_handler_t handler)
      : strand_ (strand), options_ (options), ops_ (ops),
        handler_ (std::move (handler)), next_ (0), running_ (0),
        parallel_ (options.parallel == 0 ? 1 : options.parallel),
        finished_ (false), enlisted_ (false)
  {
    results_.resize (candidates.size ());
    probes_.resize (candidates.size ());
    for (std::size_t i = 0; i < candidates.size (); ++i)
      results_[i].candidate = std::move (candidates[i]);
  }

  void
  enlist ()
  {
    tdetail::enlist_keeper (enlisted_, strand_, shared_from_this ());
  }

  asio::strand<executor_type> const &
  strand () const LUMEX_NOEXCEPT
  {
    return strand_;
  }

  tdetail::mark_registry &
  marks () LUMEX_NOEXCEPT
  {
    return marks_;
  }

  discovery_handler_t &
  handler () LUMEX_NOEXCEPT
  {
    return handler_;
  }

  void drop_kept_handlers () LUMEX_NOEXCEPT override;

  void fill_slots ();
  void cancel_all ();
  void opened (std::size_t at, std::error_code const &error);
  void out_of_band_written (std::size_t at, std::error_code const &error);
  void pause_ended (std::size_t at, std::error_code const &error);
  void identified (std::size_t at, std::error_code const &error,
                   identity_t const &identity);
  void deadline (std::size_t at, std::error_code const &error);

private:
  enum class phase_t : std::uint8_t
  {
    waiting,
    opening,
    switching,
    identifying,
    over
  };

  struct probe_t
  {
    phase_t phase = phase_t::waiting;
    std::unique_ptr<stream::byte_stream> stream;
    std::unique_ptr<datagram::message_transport> message;
    std::shared_ptr<std::vector<std::uint8_t> const> out_of_band;
    timing::timer deadline;
    timing::timer pause;
    std::shared_ptr<void> dialogue;
  };

  void start_probe (std::size_t at);
  void make_transport (std::size_t at);
  void start_out_of_band (std::size_t at);
  void start_identify (std::size_t at);
  void end_probe (std::size_t at, std::error_code const &reason);
  void finish (std::error_code const &error);

  asio::strand<executor_type> strand_;
  discovery_options_t options_;
  discovery_ops_t ops_;
  discovery_handler_t handler_;
  std::vector<discovery_result_t> results_;
  std::vector<probe_t> probes_;
  std::size_t next_;
  std::size_t running_;
  std::size_t parallel_;
  bool finished_;
  std::atomic<bool> enlisted_;
  tdetail::mark_registry marks_;
};

using run_ptr = std::shared_ptr<discovery_run_t>;

// Each handler a probe gives away runs on the strand of the discovery.
struct probe_step_base_t
{
  using executor_type = asio::strand<transport::executor_type>;

  run_ptr run;
  std::size_t at;

  executor_type
  get_executor () const LUMEX_NOEXCEPT
  {
    return run->strand ();
  }
};

struct opened_t : probe_step_base_t
{
  void
  operator() (std::error_code error)
  {
    run->opened (at, error);
  }
};

struct written_t : probe_step_base_t
{
  void
  operator() (std::error_code error)
  {
    run->out_of_band_written (at, error);
  }

  void
  operator() (std::error_code error, std::size_t)
  {
    run->out_of_band_written (at, error);
  }
};

struct pause_end_t : probe_step_base_t
{
  void
  operator() (std::error_code error)
  {
    run->pause_ended (at, error);
  }
};

struct deadline_end_t : probe_step_base_t
{
  void
  operator() (std::error_code error)
  {
    run->deadline (at, error);
  }
};

struct identified_step_t
{
  run_ptr run;
  std::size_t at;
  std::error_code error;
  identity_t identity;

  void
  operator() ()
  {
    run->identified (at, error, identity);
  }
};

// Strong, as every handler of a probe: with no time for the candidate, the
// exchange of the identification alone keeps the discovery. The end of the
// probe drops the dialogue, and the shutdown drops what the dialogue keeps.
struct identified_t
{
  run_ptr run;
  asio::strand<executor_type> strand;
  std::size_t at;

  void
  operator() (std::error_code const &error, identity_t const &identity) const
  {
    identified_step_t step = { run, at, error, identity };
    asio::post (strand, std::move (step));
  }
};

struct fill_step_t
{
  run_ptr run;

  void
  operator() ()
  {
    run->fill_slots ();
  }
};

struct cancel_step_t
{
  run_ptr run;
  std::weak_ptr<tdetail::operation_mark> mark;

  void
  operator() ()
  {
    if (tdetail::take_cancellation_request (mark))
      run->cancel_all ();
  }
};

// The handler of the slot of the discovery's handler.
class discovery_cancellation_t
{
public:
  discovery_cancellation_t (run_ptr const &run,
                            tdetail::operation_mark_ptr const &mark)
      : run_ (run), mark_ (mark)
  {
  }

  void
  operator() (asio::cancellation_type_t type)
  {
    if ((type
         & (asio::cancellation_type::terminal
            | asio::cancellation_type::partial
            | asio::cancellation_type::total))
        == asio::cancellation_type::none)
      return;
    run_ptr const alive = run_.lock ();
    if (!alive || !tdetail::request_cancellation (mark_))
      return;
    try
      {
        cancel_step_t step = { alive, mark_ };
        asio::post (alive->strand (), std::move (step));
      }
    catch (...)
      {
        // Out of memory: not cancelled; a later signal may try again.
        tdetail::take_cancellation_request (mark_);
      }
  }

private:
  std::weak_ptr<discovery_run_t> run_;
  std::weak_ptr<tdetail::operation_mark> mark_;
};

// Strand data, but nothing runs the strand any more: the io_context is being
// destroyed. Taken first: what dies may own the last owner of the run.
void
discovery_run_t::drop_kept_handlers () LUMEX_NOEXCEPT
{
  discovery_handler_t const handler (std::move (handler_));
  std::vector<probe_t> probes;
  probes.swap (probes_);
  finished_ = true;
}

void
discovery_run_t::fill_slots ()
{
  if (finished_)
    return;
  while (running_ < parallel_ && next_ < results_.size ())
    start_probe (next_++);
  if (running_ == 0 && next_ == results_.size ())
    finish (std::error_code ());
}

void
discovery_run_t::start_probe (std::size_t at)
{
  ++running_;
  probe_t &probe = probes_[at];
  probe.phase = phase_t::opening;
  try
    {
      if (options_.per_candidate.count () > 0)
        {
          probe.deadline
              = options_.timers.make_timer (executor_type (strand_));
          deadline_end_t ended;
          ended.run = shared_from_this ();
          ended.at = at;
          probe.deadline.async_wait_for (options_.per_candidate,
                                         std::move (ended));
        }
      make_transport (at);
    }
  catch (std::system_error const &failure)
    {
      end_probe (at, failure.code ()
                         ? reason_of_open (failure.code ())
                         : make_error_code (transport_errc::invalid_endpoint));
    }
  catch (...)
    {
      end_probe (at, make_error_code (transport_errc::invalid_endpoint));
    }
}

// A factory that throws is a candidate that is not there.
void
discovery_run_t::make_transport (std::size_t at)
{
  probe_t &probe = probes_[at];
  discovery_candidate_t const &candidate = results_[at].candidate;
  executor_type const executor = strand_.get_inner_executor ();
  session::stream_session::factory_t stream_factory;
  session::message_session::factory_t message_factory;
  switch (candidate.kind)
    {
    case discovery_candidate_t::candidate_kind::serial:
      stream_factory = session::make_serial_factory (candidate.serial);
      break;
    case discovery_candidate_t::candidate_kind::network:
      stream_factory = session::make_tcp_factory (candidate.network);
      break;
    case discovery_candidate_t::candidate_kind::network_datagram:
      message_factory = session::make_udp_factory (candidate.network);
      break;
#if defined(LUMEX_TRANSPORT_WITH_HID)
    case discovery_candidate_t::candidate_kind::hid:
      message_factory = hid::make_hid_factory (candidate.hid);
      break;
#endif
    case discovery_candidate_t::candidate_kind::stream:
      stream_factory = candidate.stream_factory;
      break;
    case discovery_candidate_t::candidate_kind::message:
      message_factory = candidate.message_factory;
      break;
    default:
      break;
    }
  opened_t done;
  done.run = shared_from_this ();
  done.at = at;
  if (stream_factory)
    {
      probe.stream.reset (new stream::byte_stream (stream_factory (executor)));
      probe.stream->async_open (std::move (done));
      return;
    }
  if (message_factory)
    {
      probe.message.reset (
          new datagram::message_transport (message_factory (executor)));
      probe.message->async_open (std::move (done));
      return;
    }
  end_probe (at, ::lumex::core::protocol::error::make_error_code (
                     protocol_errc::not_supported));
}

void
discovery_run_t::opened (std::size_t at, std::error_code const &error)
{
  if (finished_ || probes_[at].phase != phase_t::opening)
    return;
  if (error)
    {
      end_probe (at, reason_of_open (error));
      return;
    }
  start_out_of_band (at);
}

void
discovery_run_t::start_out_of_band (std::size_t at)
{
  probe_t &probe = probes_[at];
  probe.phase = phase_t::switching;
  try
    {
      std::vector<std::uint8_t> bytes;
      if (ops_.out_of_band != nullptr)
        bytes = ops_.out_of_band (results_[at].candidate);
      if (bytes.empty ())
        {
          start_identify (at);
          return;
        }
      written_t done;
      done.run = shared_from_this ();
      done.at = at;
      if (probe.stream)
        {
          probe.out_of_band
              = std::make_shared<std::vector<std::uint8_t> const> (
                  std::move (bytes));
          probe.stream->async_write_all (
              const_bytes_t (probe.out_of_band->data (),
                             probe.out_of_band->size ()),
              std::move (done));
          return;
        }
      datagram::message_t message;
      message.payload = std::move (bytes);
      probe.message->async_send (std::move (message), std::move (done));
    }
  catch (std::system_error const &failure)
    {
      end_probe (at, failure.code ());
    }
  catch (...)
    {
      end_probe (at, make_error_code (std::errc::not_enough_memory));
    }
}

void
discovery_run_t::out_of_band_written (std::size_t at,
                                      std::error_code const &error)
{
  probe_t &probe = probes_[at];
  if (finished_ || probe.phase != phase_t::switching)
    return;
  if (error)
    {
      end_probe (at, error);
      return;
    }
  if (options_.out_of_band_pause.count () <= 0)
    {
      start_identify (at);
      return;
    }
  try
    {
      probe.pause = options_.timers.make_timer (executor_type (strand_));
      pause_end_t ended;
      ended.run = shared_from_this ();
      ended.at = at;
      probe.pause.async_wait_for (options_.out_of_band_pause,
                                  std::move (ended));
    }
  catch (...)
    {
      end_probe (at, make_error_code (std::errc::not_enough_memory));
    }
}

void
discovery_run_t::pause_ended (std::size_t at, std::error_code const &error)
{
  if (error || finished_ || probes_[at].phase != phase_t::switching)
    return;
  start_identify (at);
}

void
discovery_run_t::start_identify (std::size_t at)
{
  probe_t &probe = probes_[at];
  probe.phase = phase_t::identifying;
  try
    {
      discovery_candidate_t const &candidate = results_[at].candidate;
      frame_link link;
      if (probe.stream && ops_.stream_link != nullptr)
        {
          std::unique_ptr<stream::byte_stream> const taken (
              std::move (probe.stream));
          link = ops_.stream_link (candidate, std::move (*taken));
        }
      else if (probe.message && ops_.message_link != nullptr)
        {
          std::unique_ptr<datagram::message_transport> const taken (
              std::move (probe.message));
          link = ops_.message_link (candidate, std::move (*taken));
        }
      if (link.empty ())
        {
          end_probe (at, ::lumex::core::protocol::error::make_error_code (
                             protocol_errc::not_supported));
          return;
        }
      identified_t done = { shared_from_this (), strand_, at };
      probe.dialogue
          = ops_.identify (std::move (link), options_.timers,
                           discovery_identified_t (std::move (done)));
    }
  catch (std::system_error const &failure)
    {
      end_probe (at, failure.code ());
    }
  catch (...)
    {
      end_probe (at, make_error_code (std::errc::not_enough_memory));
    }
}

void
discovery_run_t::identified (std::size_t at, std::error_code const &error,
                             identity_t const &identity)
{
  if (finished_ || probes_[at].phase != phase_t::identifying)
    return;
  results_[at].identity = identity;
  if (!error)
    results_[at].found = true;
  end_probe (at, error ? reason_of_identify (error) : std::error_code ());
}

void
discovery_run_t::deadline (std::size_t at, std::error_code const &error)
{
  if (error || finished_)
    return;
  switch (probes_[at].phase)
    {
    case phase_t::opening:
      end_probe (at, make_error_code (discovery_errc::open_timeout));
      break;
    case phase_t::switching:
    case phase_t::identifying:
      end_probe (at, make_error_code (discovery_errc::no_response));
      break;
    case phase_t::waiting:
    case phase_t::over:
    default:
      break;
    }
}

void
discovery_run_t::end_probe (std::size_t at, std::error_code const &reason)
{
  probe_t &probe = probes_[at];
  if (probe.phase == phase_t::over || probe.phase == phase_t::waiting)
    return;
  probe.phase = phase_t::over;
  results_[at].reason = reason;
  probe.deadline.cancel ();
  probe.pause.cancel ();
  // Each closes as it goes.
  probe.stream.reset ();
  probe.message.reset ();
  probe.dialogue.reset ();
  --running_;
  // Posted: the end of a probe may come inside start_probe, itself inside the
  // loop of fill_slots.
  fill_step_t step = { shared_from_this () };
  asio::post (strand_, std::move (step));
}

void
discovery_run_t::cancel_all ()
{
  if (finished_)
    return;
  std::error_code const cancelled
      = make_error_code (transport_errc::cancelled);
  for (std::size_t i = 0; i < probes_.size (); ++i)
    {
      if (probes_[i].phase == phase_t::waiting)
        {
          probes_[i].phase = phase_t::over;
          results_[i].reason = cancelled;
        }
      else
        end_probe (i, cancelled);
    }
  finish (cancelled);
}

void
discovery_run_t::finish (std::error_code const &error)
{
  finished_ = true;
  marks_.settle (handler_);
  discovery_handler_t handler (std::move (handler_));
  std::vector<discovery_result_t> results;
  results.swap (results_);
  asio::post (strand_,
              asio::append (std::move (handler), error, std::move (results)));
}
} // namespace

void
start_discovery (asio::strand<executor_type> const &strand,
                 std::vector<discovery_candidate_t> candidates,
                 discovery_options_t const &options,
                 discovery_ops_t const &ops, discovery_handler_t handler,
                 tdetail::operation_mark_ptr const &mark)
{
  std::shared_ptr<discovery_run_t> const run
      = std::make_shared<discovery_run_t> (strand, std::move (candidates),
                                           options, ops, std::move (handler));
  run->enlist ();
  if (mark)
    {
      asio::cancellation_slot slot
          = asio::get_associated_cancellation_slot (run->handler ());
      if (slot.is_connected ())
        {
          slot.template emplace<discovery_cancellation_t> (run, mark);
          run->marks ().watch (slot, mark);
        }
    }
  fill_step_t step = { run };
  asio::post (strand, std::move (step));
}
} // namespace detail
} // namespace protocol
} // namespace transport
} // namespace applied
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
