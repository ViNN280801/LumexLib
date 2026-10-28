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
 * @file LumexCountingHandler.hpp
 * @brief `counting_handler`: the wrapper the base classes put around a
 * completion handler to count the result before the handler runs.
 * @details Internal to the transport library. The wrapper is a move-only
 * function object that holds the counters by `shared_ptr` (a handler may
 * outlive the stream) and the handler it wraps; asio::associator
 * specialisations forward every association (executor, allocator,
 * cancellation slot, immediate executor) of the wrapped handler, so the user
 * handler still runs on its own executor and allocator. The base classes wrap
 * the handler the user gave, before it is type-erased.
 *
 * When the handler has a cancellation slot, the wrapper also holds the mark of
 * the pending operation that the transport's slot handler looks at; the mark
 * ends when the handler is called or destroyed. The transport settles the
 * mark when it hands the result on (`mark_registry`): a terminal cancellation
 * requested after that ends nothing, even before the handler ran.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_DETAIL_COUNTING_HANDLER_HPP
#define LUMEX_APPLIED_TRANSPORT_DETAIL_COUNTING_HANDLER_HPP

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/statistics/LumexTransportStatistics.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#if __has_warning("-Wthread-safety-negative")
// With libc++ std::mutex is a capability, and -Wthread-safety-negative asks
// every function that locks a mutex of its own object to promise that its
// caller holds none: a policy for annotated code, not a finding here.
#pragma clang diagnostic ignored "-Wthread-safety-negative"
#endif
#endif

namespace lumex
{
namespace applied
{
namespace transport
{
namespace detail
{
/**
 * @brief The mark of a pending operation whose handler has a cancellation
 * slot: alive from the start until the handler is called or destroyed. The
 * slot handler of the transport holds it weakly, so a signal emitted after
 * the handler ran ends nothing.
 * @details Two bits. `requested`: a terminal cancellation was emitted and its
 * step is on its way to the strand (set by the emitting thread, cleared by
 * the step). `settled`: the transport handed the result on, on the strand
 * (`mark_registry::settle`); a signal emitted from then on ends nothing,
 * although the handler may still wait on its executor. A request that came
 * before the settle still acts: the signal was emitted while the operation
 * was pending.
 */
struct operation_mark
{
  static int const requested = 1;
  static int const settled = 2;

  operation_mark () LUMEX_NOEXCEPT : bits (0) {}

  std::atomic<int> bits;
};

typedef std::shared_ptr<operation_mark> operation_mark_ptr;

/// @brief On the thread that emits: true when this call requested the
/// cancellation of a pending operation (post its step then); false when the
/// operation settled, its handler ran, or a request is already on its way.
inline bool
request_cancellation (std::weak_ptr<operation_mark> const &mark) LUMEX_NOEXCEPT
{
  operation_mark_ptr const live = mark.lock ();
  if (!live)
    return false;
  int bits = live->bits.load (std::memory_order_acquire);
  while ((bits & (operation_mark::requested | operation_mark::settled)) == 0)
    if (live->bits.compare_exchange_weak (
            bits, bits | operation_mark::requested, std::memory_order_acq_rel,
            std::memory_order_acquire))
      return true;
  return false;
}

/// @brief On the strand, in the step a request posted: true when the
/// request acts (the handler has not run); clears the request, so a later
/// signal emitted while the operation is still pending acts again.
inline bool
take_cancellation_request (std::weak_ptr<operation_mark> const &mark)
    LUMEX_NOEXCEPT
{
  operation_mark_ptr const live = mark.lock ();
  if (!live)
    return false;
  return (live->bits.fetch_and (~operation_mark::requested,
                                std::memory_order_acq_rel)
          & operation_mark::requested)
         != 0;
}

/**
 * @brief The marks of the pending operations of one transport (or session),
 * found again by the cancellation slot of their erased handler when the
 * result is handed on.
 * @details The transport only sees the erased handler at the completion; the
 * slot of an erased handler stays where it is while the handler moves, so it
 * names the operation. The slot is only compared, never changed (the user
 * may emit on it at the same time). `watch` runs on the thread that starts
 * the operation, `settle` on the strand: a mutex guards the list.
 */
class mark_registry
{
public:
  /// @brief Remembers `mark` for the operation whose erased handler has
  /// `slot` (nothing without a mark or a slot).
  void
  watch (asio::cancellation_slot const &slot, operation_mark_ptr const &mark)
  {
    if (!mark || !slot.is_connected ())
      return;
    std::lock_guard<std::mutex> const lock (mutex_);
    drop_expired ();
    // Two live operations never share a slot. An equal one is left by an
    // operation whose signal the user destroyed while it was pending (the
    // slot is the signal's memory, now reused): nobody can emit on it, and
    // it must not take the settle of this operation.
    entries_.erase (
        std::remove_if (entries_.begin (), entries_.end (), same_slot (slot)),
        entries_.end ());
    entries_.push_back (entry (slot, mark));
  }

  /// @brief On the strand, as the result of the operation of `handler` is
  /// handed on: its mark is settled. The slot is computed from the erased
  /// handler's own cancellation state (an address only: the user's signal
  /// is not read, it may be gone).
  template <typename Handler>
  void
  settle (Handler const &handler) LUMEX_NOEXCEPT
  {
    asio::cancellation_slot const slot
        = asio::get_associated_cancellation_slot (handler);
    if (slot.is_connected ())
      settle_slot (slot);
  }

private:
  typedef std::pair<asio::cancellation_slot, std::weak_ptr<operation_mark>>
      entry;

  struct expired
  {
    bool
    operator() (entry const &item) const LUMEX_NOEXCEPT
    {
      return item.second.expired ();
    }
  };

  struct same_slot
  {
    explicit same_slot (asio::cancellation_slot const &slot) : slot_ (slot) {}

    bool
    operator() (entry const &item) const LUMEX_NOEXCEPT
    {
      return item.first == slot_;
    }

    asio::cancellation_slot slot_;
  };

  void
  drop_expired () LUMEX_NOEXCEPT
  {
    entries_.erase (
        std::remove_if (entries_.begin (), entries_.end (), expired ()),
        entries_.end ());
  }

  void
  settle_slot (asio::cancellation_slot const &slot) LUMEX_NOEXCEPT
  {
    try
      {
        std::lock_guard<std::mutex> const lock (mutex_);
        // A slot freed with its handler may come back for a new operation:
        // only a live mark is the operation's own.
        for (std::vector<entry>::iterator it = entries_.begin ();
             it != entries_.end (); ++it)
          {
            if (!(it->first == slot))
              continue;
            operation_mark_ptr const live = it->second.lock ();
            if (!live)
              continue;
            live->bits.fetch_or (operation_mark::settled,
                                 std::memory_order_acq_rel);
            entries_.erase (it);
            return;
          }
      }
    catch (...)
      {
        // The mutex failed: the mark stays pending until the handler runs.
      }
  }

  std::mutex mutex_;
  std::vector<entry> entries_;
};

/// @brief A new mark when `handler` has a connected cancellation slot, else
/// none.
template <typename Handler>
operation_mark_ptr
cancellation_mark (Handler const &handler)
{
  if (!asio::get_associated_cancellation_slot (handler).is_connected ())
    return operation_mark_ptr ();
  return std::make_shared<operation_mark> ();
}

/**
 * @brief Counts a completion through `Recorder`, then calls `Handler`.
 * @tparam Handler The wrapped handler (move-only is fine).
 * @tparam Recorder A function object `void (statistics::statistics_counters &,
 * std::error_code const &, Args const &...)` that counts the result.
 */
template <typename Handler, typename Recorder> struct counting_handler
{
  std::shared_ptr<statistics::statistics_counters> counters;
  Recorder recorder;
  Handler handler;
  /// The mark of the pending operation (only with a cancellation slot).
  operation_mark_ptr pending;

  template <typename... Args>
  void
  operator() (std::error_code error, Args &&...args)
  {
    recorder (*counters, error, args...);
    // The operation is over: a signal emitted from now on must not reach the
    // transport. The slot itself is left alone, as Asio's own operations
    // leave it: the user may have destroyed the signal already.
    pending.reset ();
    std::move (handler) (error, std::forward<Args> (args)...);
  }
};

/// @brief Wraps `handler`; the result is move-only like the handler. Call it
/// with the handler as the user gave it: wrapping before the one type erasure
/// (`io_handler_t (count_with (...))`) keeps a single
/// `any_completion_handler`, whose allocator is the user's own. `pending` is
/// `cancellation_mark (handler)` (none: the slot is not watched).
template <typename Recorder, typename Handler>
counting_handler<Handler, Recorder>
count_with (std::shared_ptr<statistics::statistics_counters> const &counters,
            Recorder const &recorder, Handler handler,
            operation_mark_ptr const &pending = operation_mark_ptr ())
{
  counting_handler<Handler, Recorder> wrapped
      = { counters, recorder, std::move (handler), pending };
  return wrapped;
}

/// @brief Counts an open.
struct open_recorder
{
  void
  operator() (statistics::statistics_counters &counters,
              std::error_code const &error) const LUMEX_NOEXCEPT
  {
    counters.add_open (error);
  }
};

/// @brief Counts a read.
struct read_recorder
{
  void
  operator() (statistics::statistics_counters &counters,
              std::error_code const &error,
              std::size_t bytes) const LUMEX_NOEXCEPT
  {
    counters.add_read (error, bytes);
  }
};

/// @brief Counts a write (also the bytes written before an error).
struct write_recorder
{
  void
  operator() (statistics::statistics_counters &counters,
              std::error_code const &error,
              std::size_t bytes) const LUMEX_NOEXCEPT
  {
    counters.add_write (error, bytes);
  }
};

/// @brief Counts a message receive by the payload of the message (any type
/// with a `payload` container).
struct receive_recorder
{
  template <typename Message>
  void
  operator() (statistics::statistics_counters &counters,
              std::error_code const &error,
              Message const &message) const LUMEX_NOEXCEPT
  {
    counters.add_receive (error, message.payload.size ());
  }
};

/**
 * @brief True for the allocator of an already type-erased handler
 * (`asio::any_completion_handler_allocator`): a view into the erased handler.
 * A wrapper that is erased once more must not forward it (see the
 * `associated_allocator` specialisation of `counting_handler`), at any depth
 * (`bind_executor (ex, any_completion_handler (...))` forwards it too).
 */
template <typename Allocator>
struct is_erased_handler_allocator : std::false_type
{
};

template <typename T, typename... Signatures>
struct is_erased_handler_allocator<
    asio::any_completion_handler_allocator<T, Signatures...>> : std::true_type
{
};

/// @brief Counts a message send by the payload size taken at the start.
struct send_recorder
{
  std::size_t bytes;
  void
  operator() (statistics::statistics_counters &counters,
              std::error_code const &error) const LUMEX_NOEXCEPT
  {
    counters.add_send (error, bytes);
  }
};
} // namespace detail
} // namespace transport
} // namespace applied
} // namespace lumex

namespace asio
{
template <template <typename, typename> class Associator, typename Handler,
          typename Recorder, typename DefaultCandidate>
struct associator<
    Associator,
    lumex::applied::transport::detail::counting_handler<Handler, Recorder>,
    DefaultCandidate> : Associator<Handler, DefaultCandidate>
{
  typedef lumex::applied::transport::detail::counting_handler<Handler,
                                                              Recorder>
      wrapper_type;

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

// The allocator of the wrapped handler is forwarded unless it is the view
// allocator of an already type-erased handler (the token was an
// any_completion_handler, or a wrapper of one such as
// bind_executor (ex, any_completion_handler (...))): the outer erasure would
// free its storage through it after the inner erasure is gone. The default
// candidate is returned instead.
template <typename Handler, typename Recorder, typename DefaultCandidate>
struct associator<
    associated_allocator,
    lumex::applied::transport::detail::counting_handler<Handler, Recorder>,
    DefaultCandidate>
{
  typedef lumex::applied::transport::detail::counting_handler<Handler,
                                                              Recorder>
      wrapper_type;
  typedef associated_allocator<Handler, DefaultCandidate> inner_type;
  typedef lumex::applied::transport::detail::is_erased_handler_allocator<
      typename inner_type::type>
      erased;
  typedef typename std::conditional<erased::value, DefaultCandidate,
                                    typename inner_type::type>::type type;

  static type
  get (wrapper_type const &wrapper) LUMEX_NOEXCEPT
  {
    return get_inner (wrapper, DefaultCandidate (), erased ());
  }

  static type
  get (wrapper_type const &wrapper,
       DefaultCandidate const &candidate) LUMEX_NOEXCEPT
  {
    return get_inner (wrapper, candidate, erased ());
  }

private:
  static type
  get_inner (wrapper_type const &, DefaultCandidate const &candidate,
             std::true_type) LUMEX_NOEXCEPT
  {
    return candidate;
  }

  static type
  get_inner (wrapper_type const &wrapper, DefaultCandidate const &candidate,
             std::false_type) LUMEX_NOEXCEPT
  {
    return inner_type::get (wrapper.handler, candidate);
  }
};
} // namespace asio

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_TRANSPORT_DETAIL_COUNTING_HANDLER_HPP
