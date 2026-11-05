// The harness of the transport matrix: every layer of a device table that
// has a wire form of its own, under every behaviour of the simulator, over
// the mock transport of its wire (a stream, or a datagram transport with a
// report ID), on a virtual clock. A cell is one layer, one transport and one
// behaviour; it runs one script through the generated client over a session
// (a query, a command, a dangerous row, a row of some states and a stream
// window, each where the table has one, and the extra steps of some kinds:
// a burst of calls, a late reply, a cut reply and a hang-up, a block above
// the frame limit), classifies how each call ended and asserts the class
// that `testing::expected_outcome` gives for the row, the counters the
// behaviour must move, the bounds of the queues and a real-time guard. The
// tables are the de-identified ones of lumex/tests/core/protocol/devices,
// referred to as devices A to M and their layers by letter; nothing here names
// a real device, and the framings bound to the layers are generic codecs of
// core/framing with invented parameters.
#ifndef LUMEX_TESTS_APPLIED_TRANSPORT_MATRIX_SUPPORT_HPP
#define LUMEX_TESTS_APPLIED_TRANSPORT_MATRIX_SUPPORT_HPP

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/LumexTransport"
#include "lumex/applied/transport/stream/LumexIoRunner.hpp"
#include "lumex/core/framing/LumexFraming"
#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/applied/transport/testing/simulator/LumexSimulatorTestSupport.hpp"

namespace lumex_matrix_test
{
namespace tr = lumex::applied::transport;
namespace pr = lumex::applied::transport::protocol;
namespace dialogue = lumex::applied::transport::protocol::dialogue;
namespace fr = lumex::core::framing;
namespace index = lumex::core::protocol::index;
namespace layout = lumex::core::protocol::layout;
namespace field = lumex::core::protocol::field;
namespace dsl = lumex::core::protocol::dsl;
namespace ss = lumex::applied::transport::session;
namespace sequence = lumex::core::utility::sequence;
using bytes_t = std::vector<std::uint8_t>;
using ms = std::chrono::milliseconds;
using tr::testing::behavior;
using tr::testing::behavior_kind;
using tr::testing::outcome_class;

// ---------------------------------------------------------------------------
// The wire: a codec that records what it framed
// ---------------------------------------------------------------------------

/// What the codec of the client saw: the payloads it framed, the frames it
/// delivered and the framing errors it reported, by `framing_errc`.
struct wire_record_t
{
  /// The payloads written (the requests, in order).
  std::vector<bytes_t> tx;
  /// The frames delivered (the replies and events, in order).
  std::vector<bytes_t> rx;
  /// Errors by `framing_errc` value (index `value - 1`), other codes last.
  std::uint64_t errors[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
};

/// A snapshot of `wire_record_t`'s counters.
struct wire_counts_t
{
  std::uint64_t tx = 0;
  std::uint64_t rx = 0;
  std::uint64_t errors = 0;
  std::uint64_t bad_checksum = 0;
  std::uint64_t frame_too_large = 0;
  /// The most frames the adapter held for its reader, sampled at each frame.
  std::size_t max_queued_frames = 0;
};

/// The shared record of one bench (the codec moves into the adapter).
class wire_log_t
{
public:
  void
  sent (fr::codec::bytes_view_t payload)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    record_.tx.push_back (bytes_t (payload.begin (), payload.end ()));
  }

  void
  received (fr::codec::bytes_view_t frame, std::error_code error)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    if (queued_)
      max_queued_frames_ = std::max (max_queued_frames_, queued_ ());
    if (!error)
      {
        record_.rx.push_back (bytes_t (frame.begin (), frame.end ()));
        return;
      }
    int at = 7;
    if (error.category () == fr::codec::framing_category ()
        && error.value () >= 1 && error.value () <= 6)
      at = error.value () - 1;
    ++record_.errors[at];
  }

  wire_counts_t
  counts () const
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    wire_counts_t out;
    out.tx = record_.tx.size ();
    out.rx = record_.rx.size ();
    for (std::size_t i = 0; i < 8; ++i)
      out.errors += record_.errors[i];
    out.bad_checksum
        = record_
              .errors[static_cast<int> (fr::codec::framing_errc::bad_checksum)
                      - 1];
    out.frame_too_large
        = record_.errors[static_cast<int> (
                             fr::codec::framing_errc::frame_too_large)
                         - 1];
    out.max_queued_frames = max_queued_frames_;
    return out;
  }

  /// Where the codec reads how many frames wait in its adapter (empty: no
  /// sampling; set it empty before the adapter goes).
  void
  set_queue_probe (std::function<std::size_t ()> probe)
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    queued_ = std::move (probe);
  }

  wire_record_t
  copy () const
  {
    std::lock_guard<std::mutex> const lock (mutex_);
    return record_;
  }

private:
  mutable std::mutex mutex_;
  wire_record_t record_;
  std::function<std::size_t ()> queued_;
  std::size_t max_queued_frames_ = 0;
};

/// `Codec` with every payload, frame and framing error written to a log.
/// Over datagrams the adapter resets the codec after each datagram and
/// counts one that held no frame as `truncated` in its own counters, which a
/// client cannot read through its link: the tap counts it the same way.
template <typename Codec> class recording_codec_t
{
public:
  using options_type = typename Codec::options_type;

  recording_codec_t (Codec codec, std::shared_ptr<wire_log_t> log,
                     bool datagram)
      : codec_ (std::move (codec)), log_ (std::move (log)),
        datagram_ (datagram)
  {
  }

  fr::codec::encode_result_t
  encode (fr::codec::bytes_view_t payload, fr::codec::bytes_t &out) const
  {
    log_->sent (payload);
    return codec_.encode (payload, out);
  }

  template <typename Sink>
  std::size_t
  decode (fr::codec::bytes_view_t chunk, Sink &&sink)
  {
    tap_t<typename std::remove_reference<Sink>::type> tap
        = { log_.get (), &sink, &delivered_ };
    fed_ = fed_ || chunk.size () != 0;
    return codec_.decode (chunk, tap);
  }

  void
  reset () LUMEX_NOEXCEPT
  {
    if (datagram_ && fed_ && !delivered_)
      log_->received (fr::codec::bytes_view_t (),
                      make_error_code (fr::codec::framing_errc::truncated));
    fed_ = false;
    delivered_ = false;
    codec_.reset ();
  }

private:
  template <typename Sink> struct tap_t
  {
    wire_log_t *log;
    Sink *sink;
    bool *delivered;

    void
    operator() (fr::codec::bytes_view_t frame, std::error_code error) const
    {
      *delivered = true;
      log->received (frame, error);
      (*sink) (frame, error);
    }
  };

  Codec codec_;
  std::shared_ptr<wire_log_t> log_;
  bool datagram_;
  bool fed_ = false;
  bool delivered_ = false;
};

/// The largest frame of every codec of the matrix.
LUMEX_CONSTEXPR std::size_t KMAX_FRAME = 4096;

/// The size of the block of `behavior::oversized`: above `KMAX_FRAME`.
LUMEX_CONSTEXPR std::size_t KOVERSIZED = 70000;

/// The bound of the queue of the dialogue in the matrix, small enough for a
/// burst of `KBURST` calls to go above it.
LUMEX_CONSTEXPR std::size_t KQUEUE_BOUND = 4;

/// The calls of the burst step, started at once.
LUMEX_CONSTEXPR std::size_t KBURST = 8;

/// The bound of the frame queue of the framed adapter in the matrix.
LUMEX_CONSTEXPR std::size_t KADAPTER_QUEUE = 4;

using crc16_t
    = fr::stage::crc<lumex::core::crc::parametric::crc16_ibm3740_spec_t>;
using crc32_t
    = fr::stage::crc<lumex::core::crc::parametric::crc32_iso_hdlc_spec_t,
                     fr::codec::byte_order::little>;

/// A wire of the matrix: what binds a layer to its framing. `host ()` is
/// the codec of the client, `device ()` that of the simulator.
struct wire_base_t
{
  /// A datagram transport instead of a stream.
  static LUMEX_CONSTEXPR bool
  datagram () LUMEX_NOEXCEPT
  {
    return false;
  }

  /// The report ID of a datagram wire.
  static std::uint8_t
  report_id () LUMEX_NOEXCEPT
  {
    return 0;
  }

  /// The fill of `behavior::oversized`: a byte that is no delimiter.
  static std::uint8_t
  oversized_fill () LUMEX_NOEXCEPT
  {
    return 0xAB;
  }

  /// The session values the decoders of the layer need.
  static void
  prepare (index::session_context_t &)
  {
  }

  /// Bytes that end whatever the decoder holds, as the next frame on the
  /// line would (empty: the wire counts noise without one, or never).
  static bytes_t
  boundary ()
  {
    return bytes_t ();
  }

  /// Bytes that announce or make a frame above the limit of the codec, fed
  /// by hand (empty: `KOVERSIZED` bytes of `oversized_fill ()` and the
  /// boundary, see `oversized_block_of`).
  static bytes_t
  oversized_block ()
  {
    return bytes_t ();
  }

  /// Whether the block of `behavior::oversized` reads as one frame above the
  /// limit (`frame_too_large`); false for a codec that reads it as short
  /// frames with a wrong check.
  static LUMEX_CONSTEXPR bool
  oversized_reads_too_large () LUMEX_NOEXCEPT
  {
    return true;
  }

  /// The retries of a row that names none.
  static LUMEX_CONSTEXPR std::size_t
  default_retries () LUMEX_NOEXCEPT
  {
    return 2;
  }

  /// The row the client checks the device with after a reconnect (the
  /// layer names `identify_with`), null for none.
  static char const *
  identify_row () LUMEX_NOEXCEPT
  {
    return nullptr;
  }

  /// The state the `accepted_in` row of the layer is accepted in (-1: the
  /// layer has none).
  static LUMEX_CONSTEXPR std::int64_t
  accepted_state () LUMEX_NOEXCEPT
  {
    return -1;
  }

  /// The period of a reply that ends after the timeout of the query and
  /// within the window of late replies (0: the layer has no window).
  static LUMEX_CONSTEXPR std::int64_t
  late_trickle_ms () LUMEX_NOEXCEPT
  {
    return 0;
  }

  /// What the device needs besides its table (a reply value of its own).
  template <typename Device>
  static void
  prepare_device (Device &)
  {
  }
};

/// SLIP with a CRC-16 after the payload.
struct slip_wire_t : wire_base_t
{
  using codec_type = fr::stage::pipeline<fr::slip::slip_codec,
                                         fr::stage::checksum_stage<crc16_t>>;

  static codec_type
  host ()
  {
    fr::slip::slip_options_t options;
    options.max_frame_size = KMAX_FRAME;
    return codec_type (options);
  }

  static codec_type
  device ()
  {
    return host ();
  }

  static bytes_t
  boundary ()
  {
    return bytes_t (1, 0xC0);
  }
};

/// Text: a start character, the payload, an XOR of it in hexadecimal digits
/// and a carriage return.
struct text_wire_t : wire_base_t
{
  using codec_type = fr::stage::pipeline<
      fr::delimiter::delimiter_codec, fr::stage::prefix_char_stage<'#'>,
      fr::stage::checksum_stage<fr::stage::xor8, fr::stage::ascii_hex_render>>;

  static codec_type
  host ()
  {
    fr::delimiter::delimiter_options_t options;
    options.max_frame_size = KMAX_FRAME;
    return codec_type (options);
  }

  static codec_type
  device ()
  {
    return host ();
  }

  static bytes_t
  boundary ()
  {
    return bytes_t (1, '\r');
  }
};

/// COBS with a CRC-16 after the payload.
struct cobs_wire_t : wire_base_t
{
  using codec_type = fr::stage::pipeline<fr::cobs::cobs_codec,
                                         fr::stage::checksum_stage<crc16_t>>;

  static codec_type
  host ()
  {
    fr::cobs::cobs_options_t options;
    options.max_frame_size = KMAX_FRAME;
    return codec_type (options);
  }

  static codec_type
  device ()
  {
    return host ();
  }

  static bytes_t
  boundary ()
  {
    return bytes_t (1, 0x00);
  }
};

/// A binary header (a two-byte preamble, a two-byte length) and a CRC-32.
/// The preamble is the fill of the oversized block, so that the block reads
/// as a header with a length above the limit (any other fill is line noise
/// the decoder skips without a report).
struct header_wire_t : wire_base_t
{
  using codec_type = fr::header::header_codec<crc32_t>;

  static codec_type
  host ()
  {
    fr::header::header_options_t options;
    options.max_frame_size = KMAX_FRAME;
    options.tx_preamble = bytes_t (2, 0xAB);
    options.rx_preamble = bytes_t (2, 0xAB);
    options.header_size = 4;
    options.length_offset = 2;
    options.length_width = 2;
    return codec_type (options);
  }

  static codec_type
  device ()
  {
    return host ();
  }
};

/// SLIP with a CRC-16 in datagrams of report 1 (a datagram ends what it
/// holds).
struct slip_datagram_wire_t : slip_wire_t
{
  static bytes_t
  boundary ()
  {
    return bytes_t ();
  }

  static LUMEX_CONSTEXPR bool
  datagram () LUMEX_NOEXCEPT
  {
    return true;
  }

  static std::uint8_t
  report_id () LUMEX_NOEXCEPT
  {
    return 1;
  }
};

/// The wire of layer `P`: a device TU specializes it for each of its layers.
template <typename P> struct wire_of;

/// The block `W` feeds by hand to go above its frame limit.
template <typename W>
bytes_t
oversized_block_of ()
{
  bytes_t out = W::oversized_block ();
  if (!out.empty ())
    return out;
  out.assign (KOVERSIZED, W::oversized_fill ());
  bytes_t const end = W::boundary ();
  out.insert (out.end (), end.begin (), end.end ());
  return out;
}

// ---------------------------------------------------------------------------
// The rows of the script
// ---------------------------------------------------------------------------

template <typename Row> struct row_traits_t
{
  using form = dialogue::detail::reply_form_t<Row>;
  using streams = dsl::detail::command_has_option<Row, dsl::kind::stream>;
  // A row of some models or firmware may be refused for a reason of its
  // own: the script leaves it out.
  using of_models = std::integral_constant<
      bool,
      dsl::detail::command_has_option<Row, dsl::kind::model>::value
          || dsl::detail::command_has_option<Row, dsl::kind::firmware>::value>;
  // A row of some states: the script calls it in a state that accepts it
  // (`accepted`), not as a query or a command.
  using of_states = std::integral_constant<
      bool,
      dsl::detail::command_has_option<Row, dsl::kind::accepted_in>::value>;
  using limited
      = std::integral_constant<bool, of_models::value || of_states::value>;
  using dangerous
      = std::integral_constant<bool, (dsl::flags_of (Row::options ())
                                      & dsl::row_flags::dangerous)
                                         != 0>;
  using idempotent
      = std::integral_constant<bool, (dsl::flags_of (Row::options ())
                                      & dsl::row_flags::idempotent)
                                         != 0>;
  using no_fields
      = std::integral_constant<bool, layout::request_of<Row>::type::size::value
                                         == 0>;
};

/// A query: a reply of a field or a layout, no stream.
template <typename Row>
struct is_query_t
    : std::integral_constant<bool, row_traits_t<Row>::form::value == 1
                                       && !row_traits_t<Row>::streams::value
                                       && !row_traits_t<Row>::limited::value
                                       && !row_traits_t<Row>::dangerous::value>
{
};

/// An idempotent query.
template <typename Row>
struct is_idempotent_query_t
    : std::integral_constant<bool, is_query_t<Row>::value
                                       && row_traits_t<Row>::idempotent::value>
{
};

/// A command: no reply, no stream.
template <typename Row>
struct is_command_t
    : std::integral_constant<bool, row_traits_t<Row>::form::value == 0
                                       && !row_traits_t<Row>::streams::value
                                       && !row_traits_t<Row>::limited::value
                                       && !row_traits_t<Row>::dangerous::value>
{
};

/// A command that may not be sent twice.
template <typename Row>
struct is_plain_command_t
    : std::integral_constant<bool,
                             is_command_t<Row>::value
                                 && !row_traits_t<Row>::idempotent::value>
{
};

/// A dangerous row (called with `confirm`).
template <typename Row>
struct is_dangerous_t
    : std::integral_constant<bool, row_traits_t<Row>::dangerous::value
                                       && row_traits_t<Row>::form::value != 2
                                       && !row_traits_t<Row>::streams::value
                                       && !row_traits_t<Row>::limited::value>
{
};

/// A row accepted in some states only.
template <typename Row>
struct is_accepted_t
    : std::integral_constant<bool, row_traits_t<Row>::of_states::value
                                       && !row_traits_t<Row>::of_models::value
                                       && row_traits_t<Row>::form::value != 2
                                       && !row_traits_t<Row>::streams::value
                                       && !row_traits_t<Row>::dangerous::value>
{
};

/// A row the device sends by itself.
template <typename Row>
struct is_stream_t
    : std::integral_constant<bool, row_traits_t<Row>::streams::value
                                       && row_traits_t<Row>::form::value == 1
                                       && !row_traits_t<Row>::limited::value
                                       && row_traits_t<Row>::no_fields::value>
{
};

/// The first row of `List` that `Pred` accepts, `void` for none.
template <template <typename> class Pred, typename List> struct first_of_t;

template <template <typename> class Pred>
struct first_of_t<Pred, dsl::type_list<>>
{
  using type = void;
};

template <template <typename> class Pred, typename Head, typename... Tail>
struct first_of_t<Pred, dsl::type_list<Head, Tail...>>
{
  using type = typename std::conditional<
      Pred<Head>::value, Head,
      typename first_of_t<Pred, dsl::type_list<Tail...>>::type>::type;
};

/// `First` unless it is `void`, then `Second`.
template <typename First, typename Second> struct either_t
{
  using type = First;
};

template <typename Second> struct either_t<void, Second>
{
  using type = Second;
};

/// The rows of the script of layer `P`.
template <typename P> struct script_rows_t
{
  using rows = typename dsl::commands_of<P>::type;
  using query =
      typename either_t<typename first_of_t<is_idempotent_query_t, rows>::type,
                        typename first_of_t<is_query_t, rows>::type>::type;
  using command =
      typename either_t<typename first_of_t<is_plain_command_t, rows>::type,
                        typename first_of_t<is_command_t, rows>::type>::type;
  using dangerous = typename first_of_t<is_dangerous_t, rows>::type;
  using accepted = typename first_of_t<is_accepted_t, rows>::type;
  using stream = typename first_of_t<is_stream_t, rows>::type;
  /// The row of the burst step: the query, else the command.
  using burst = typename either_t<query, command>::type;
};

// ---------------------------------------------------------------------------
// Driving the context: polled on this thread, or run by a runner of threads
// ---------------------------------------------------------------------------

/// Runs the io_context of a bench. Polled (the matrix): one thread, `poll`
/// and the clock wait by wait (`testing::run_until`). Threaded (the subset
/// for the thread sanitizer): an `io_runner` of four threads, the clock moved
/// on a runner thread from one wait to the next once the handlers settled:
/// the progress of the bench (a sum of its counters and the waits of the
/// clock) stayed the same over several short pauses, so that a reply on its
/// way is not overtaken by the time of a timeout.
class driver_t
{
public:
  explicit driver_t (bool threaded);
  ~driver_t ();

  driver_t (driver_t const &) = delete;
  driver_t &operator= (driver_t const &) = delete;

  tr::executor_type executor ();

  bool threaded () const LUMEX_NOEXCEPT;

  /// Runs and moves the clock until `done` or `budget` of virtual time.
  bool run_until (tr::testing::virtual_clock &clock,
                  std::function<bool ()> const &done, ms budget);

  /// Stops the threads (threaded); the context stays.
  void stop ();

  /// What tells a threaded driver that the handlers still move (empty: the
  /// waits of the clock only).
  void set_progress (std::function<std::uint64_t ()> progress);

  /// Where a threaded driver moves the clock (by default any runner thread).
  /// The strand of the mock: a wait the clock takes holds a copy of its
  /// timer's executor, and the mock's timers are dropped on that strand, so
  /// the last copy of an executor goes on one strand. Asio frees the shared
  /// target of an executor after a release decrement and an acquire fence,
  /// which the thread sanitizer does not see when two threads hold copies.
  void set_clock_executor (tr::executor_type executor);

private:
  tr::executor_type clock_executor_;
  std::function<std::uint64_t ()> progress_;
  std::unique_ptr<asio::io_context> context_;
  std::unique_ptr<tr::stream::io_runner> runner_;
};

// ---------------------------------------------------------------------------
// Outcomes
// ---------------------------------------------------------------------------

/// How one call ended, as the test saw it.
struct call_state_t
{
  std::mutex mutex;
  int calls = 0;
  std::error_code error;
  /// The value the call returned (a `pr::reply_of<Row>`).
  std::shared_ptr<void const> value;
};

/// The completion of a call of any row.
struct call_done_t
{
  std::shared_ptr<call_state_t> state;

  template <typename Value>
  void
  operator() (std::error_code error, Value const &value)
  {
    std::shared_ptr<void const> const kept
        = std::make_shared<Value const> (value);
    std::lock_guard<std::mutex> const lock (state->mutex);
    ++state->calls;
    state->error = error;
    state->value = kept;
  }
};

/// The messages of a stream the client delivered.
struct stream_state_t
{
  std::mutex mutex;
  std::vector<tr::timing::time_point_t> times;
};

struct stream_seen_t
{
  std::shared_ptr<stream_state_t> state;
  tr::testing::virtual_clock *clock;

  template <typename Message>
  void
  operator() (Message const &)
  {
    tr::timing::time_point_t const now = clock->now ();
    std::lock_guard<std::mutex> const lock (state->mutex);
    state->times.push_back (now);
  }
};

/// The counters a step compares before and after.
struct snapshot_t
{
  pr::dialogue_statistics_t engine;
  wire_counts_t wire;
  ss::session_statistics_t session;
  tr::testing::simulator_statistics_t device;
  /// The reads of the mock stream (zero-length ones too; 0 for datagrams).
  std::uint64_t link_reads = 0;
};

/// What a step observed.
struct observed_t
{
  bool ended = false;
  std::error_code error;
  snapshot_t before;
  snapshot_t after;
};

/// The class of what a step observed (see the .cpp for the rules); `hang`
/// when the call did not end.
enum class seen_class : std::uint8_t
{
  hang,
  other_error,
  success,
  success_after_retries,
  timeout,
  retries_exhausted,
  link_lost,
  protocol_error,
  dropped_counted
};

char const *seen_class_name (seen_class seen);

seen_class classify_call (observed_t const &observed);

seen_class classify_stream (observed_t const &observed,
                            std::size_t messages_late_half);

/// True when `seen` is what `expected` asks for under `kind` (the
/// allowances of echo and malicious are in the .cpp).
bool matches (behavior_kind kind, outcome_class expected, seen_class seen,
              bool stream);

/// The behaviour a cell of `kind` runs, with the matrix's parameters.
behavior matrix_behavior (behavior_kind kind, std::uint8_t oversized_fill);

/// The behaviour of the stream window of a cell of `kind`: that of the
/// calls, but frozen at once (`frozen (0)`, `partial_then_frozen (0, 2)`):
/// the messages of a stream are no replies the freeze counts.
behavior stream_behavior (behavior_kind kind, std::uint8_t oversized_fill);

/// The kinds a wire takes: every kind on a stream; on a datagram wire the
/// kinds without a datagram meaning (`zero_read`) and `fragmented` (a piece
/// of a frame in a datagram is no frame) are left out.
std::vector<behavior_kind> kinds_for (bool datagram);

/// The checks of the counters a behaviour must move (or must not), for a
/// step that asked for `expected`; failures are added to the test.
void check_counters (behavior_kind kind, outcome_class expected,
                     observed_t const &observed, std::size_t attempts,
                     std::string const &where);

/// What `on_reconnect` of the client heard.
struct reconnect_state_t
{
  std::mutex mutex;
  int calls = 0;
  std::error_code error;
  std::string model;
};

struct reconnect_seen_t
{
  std::shared_ptr<reconnect_state_t> state;

  void
  operator() (std::error_code const &error,
              pr::identity_t const &identity) const
  {
    std::lock_guard<std::mutex> const lock (state->mutex);
    ++state->calls;
    state->error = error;
    state->model = identity.model;
  }
};

/// The debug lines of the dialogue while it lives, when `enabled` (the
/// other levels are dropped, so that a cell writes nothing).
class log_guard_t
{
public:
  explicit log_guard_t (bool enabled);
  ~log_guard_t ();

  log_guard_t (log_guard_t const &) = delete;
  log_guard_t &operator= (log_guard_t const &) = delete;

  /// The debug lines of the dialogue that begin with `prefix` ("TX ").
  static std::vector<std::string> lines (std::string const &prefix);

private:
  tr::log::log_level level_;
  tr::log::log_sink_t sink_;
};

// ---------------------------------------------------------------------------
// The bench of a cell
// ---------------------------------------------------------------------------

/// A session that retries on the virtual clock without jitter.
ss::session_options_t session_options (tr::testing::virtual_clock &clock);

/// The client options of the matrix: the table's, on the virtual clock,
/// `W::default_retries ()` for a row that names none, a queue of
/// `KQUEUE_BOUND`.
template <typename P, typename W>
pr::client_options_t
client_options (tr::testing::virtual_clock &clock)
{
  pr::client_options_t options = pr::client_options_for<P> ();
  options.dialogue.name = "matrix";
  options.dialogue.timers = clock.source ();
  options.dialogue.defaults.retries
      = static_cast<std::uint32_t> (W::default_retries ());
  options.dialogue.queue_bound = KQUEUE_BOUND;
  return options;
}

/// A framed adapter behind `frame_link_impl` as the library puts it there
/// (`frame_link (framed_stream)`, `frame_link (framed_datagram, report)`),
/// which also tells how many decoded frames wait in the adapter.
template <typename Framed>
class observed_link_t final : public pr::frame_link_impl
{
public:
  observed_link_t (Framed link, std::uint8_t report_id)
      : link_ (std::move (link)), report_id_ (report_id)
  {
  }

  tr::executor_type
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
  start_read (pr::link_read_handler_t handler) override
  {
    read (link_, std::move (handler));
  }

  void
  start_write (std::shared_ptr<pr::link_frame_t const> frame,
               tr::status_handler_t handler) override
  {
    write (link_, report_id_, std::move (frame), std::move (handler));
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

  void
  drop_connection () LUMEX_NOEXCEPT override
  {
    pr::detail::drop_connection_of (link_);
  }

  void
  reject_connection () LUMEX_NOEXCEPT override
  {
    pr::detail::reject_connection_of (link_);
  }

  bool
  reconnects () const LUMEX_NOEXCEPT override
  {
    return link_.transport ().impl ()->reconnects ();
  }

  void
  watch_connection (tr::connection_handler_t handler) override
  {
    link_.transport ().impl ()->watch_connection (std::move (handler));
  }

  /// The decoded frames that wait for a reader (any thread).
  std::size_t
  queued_frames () const LUMEX_NOEXCEPT
  {
    return link_.queued_frames ();
  }

private:
  template <typename Codec>
  static void
  read (tr::framed::framed_stream<Codec> &link,
        pr::link_read_handler_t handler)
  {
    link.async_read_frame (std::move (handler));
  }

  template <typename Codec>
  static void
  read (tr::framed::framed_datagram<Codec> &link,
        pr::link_read_handler_t handler)
  {
    pr::detail::payload_handler_t wrapped = { std::move (handler) };
    link.async_read_frame (std::move (wrapped));
  }

  template <typename Codec>
  static void
  write (tr::framed::framed_stream<Codec> &link, std::uint8_t,
         std::shared_ptr<pr::link_frame_t const> frame,
         tr::status_handler_t handler)
  {
    tr::const_bytes_t const payload (frame->data (), frame->size ());
    link.async_write_frame (payload,
                            asio::consign (std::move (handler), frame));
  }

  template <typename Codec>
  static void
  write (tr::framed::framed_datagram<Codec> &link, std::uint8_t report_id,
         std::shared_ptr<pr::link_frame_t const> frame,
         tr::status_handler_t handler)
  {
    tr::const_bytes_t const payload (frame->data (), frame->size ());
    link.async_write_frame (report_id, payload,
                            asio::consign (std::move (handler), frame));
  }

  Framed link_;
  std::uint8_t report_id_;
};

/// The options of the framed adapter of the matrix.
inline tr::framed::framed_options_t
adapter_options ()
{
  tr::framed::framed_options_t options;
  options.queue_bound = KADAPTER_QUEUE;
  return options;
}

/// A link over `link` whose adapter `log` samples at each frame.
template <typename Framed>
pr::frame_link
observed_link (Framed link, std::uint8_t report_id,
               std::shared_ptr<wire_log_t> const &log)
{
  std::shared_ptr<observed_link_t<Framed>> const impl
      = std::make_shared<observed_link_t<Framed>> (std::move (link),
                                                   report_id);
  observed_link_t<Framed> const *const raw = impl.get ();
  log->set_queue_probe ([raw] () { return raw->queued_frames (); });
  return pr::frame_link (std::shared_ptr<pr::frame_link_impl> (impl));
}

template <typename P, typename W, bool Datagram = W::datagram ()>
struct transport_of_t;

/// A stream: a mock stream behind a stream session.
template <typename P, typename W> struct transport_of_t<P, W, false>
{
  using codec_type = recording_codec_t<typename W::codec_type>;

  void
  make (tr::executor_type const &executor, tr::testing::virtual_clock &clock)
  {
    tr::testing::mock_options_t options;
    options.timers = clock.source ();
    mock.reset (new tr::testing::mock_stream (
        tr::testing::make_mock_stream (executor, options)));
    std::shared_ptr<tr::stream::stream_impl> const impl = mock->stream.impl ();
    session.reset (new ss::stream_session (
        executor, [impl] (tr::executor_type const &)
          { return tr::stream::byte_stream (impl); },
        session_options (clock)));
    device.reset (new tr::testing::device_simulator<P> (mock->control,
                                                        W::device (), clock));
  }

  pr::frame_link
  link (std::shared_ptr<wire_log_t> const &log)
  {
    return observed_link (tr::framed::framed_stream<codec_type> (
                              session->as_byte_stream (),
                              codec_type (W::host (), log, false),
                              adapter_options ()),
                          0, log);
  }

  /// The reads of the mock so far (zero-length ones too).
  std::uint64_t
  reads () const
  {
    return mock->stream.statistics ().reads;
  }

  /// The device hangs up now.
  void
  hang_up ()
  {
    mock->control.disconnect ();
  }

  /// Writes stay pending until `release_writes`.
  void
  hold_writes ()
  {
    mock->control.hold_writes ();
  }

  void
  release_writes ()
  {
    mock->control.release_writes ();
  }

  /// Bytes reach the client as if the device sent them.
  void
  feed (bytes_t const &bytes)
  {
    mock->control.feed (bytes);
  }

  void
  release ()
  {
    session.reset ();
  }

  std::unique_ptr<tr::testing::mock_stream> mock;
  std::unique_ptr<ss::stream_session> session;
  std::unique_ptr<tr::testing::device_simulator<P>> device;
};

/// A datagram transport: a mock behind a message session, one report ID.
template <typename P, typename W> struct transport_of_t<P, W, true>
{
  using codec_type = recording_codec_t<typename W::codec_type>;

  void
  make (tr::executor_type const &executor, tr::testing::virtual_clock &clock)
  {
    tr::testing::mock_options_t options;
    options.timers = clock.source ();
    mock.reset (new tr::testing::mock_datagram (
        tr::testing::make_mock_datagram (executor, options)));
    std::shared_ptr<tr::datagram::message_impl> const impl
        = mock->transport.impl ();
    session.reset (new ss::message_session (
        executor, [impl] (tr::executor_type const &)
          { return tr::datagram::message_transport (impl); },
        session_options (clock)));
    device.reset (new tr::testing::device_simulator<P> (
        mock->control, W::device (), W::report_id (), clock));
  }

  pr::frame_link
  link (std::shared_ptr<wire_log_t> const &log)
  {
    return observed_link (tr::framed::framed_datagram<codec_type> (
                              session->as_message_transport (),
                              codec_type (W::host (), log, true),
                              adapter_options ()),
                          W::report_id (), log);
  }

  /// No reads of a stream.
  std::uint64_t
  reads () const
  {
    return 0;
  }

  void
  hang_up ()
  {
    mock->control.disconnect ();
  }

  /// Sends stay pending until `release_writes`.
  void
  hold_writes ()
  {
    mock->control.hold_sends ();
  }

  void
  release_writes ()
  {
    mock->control.release_sends ();
  }

  /// A datagram ends what it holds: nothing to feed.
  void
  feed (bytes_t const &)
  {
  }

  void
  release ()
  {
    session.reset ();
  }

  std::unique_ptr<tr::testing::mock_datagram> mock;
  std::unique_ptr<ss::message_session> session;
  std::unique_ptr<tr::testing::device_simulator<P>> device;
};

/// The bench of one cell: the driver, the clock, the mock, the session, the
/// simulator and the client of layer `P` on the wire `W`.
template <typename P, typename W = wire_of<P>> class bench_t
{
public:
  explicit bench_t (bool threaded)
      : driver (threaded), log (std::make_shared<wire_log_t> ()),
        reconnects (std::make_shared<reconnect_state_t> ())
  {
    wire.make (driver.executor (), clock);
    W::prepare (wire.device->session ());
    W::prepare (session_values);
    W::prepare_device (*wire.device);
    wire.session->start ();
    settle ();
    client.reset (
        new pr::client<P> (wire.link (log), client_options<P, W> (clock)));
    client->set_session (session_values);
    reconnect_seen_t const seen = { reconnects };
    client->on_reconnect (seen);
    driver.set_progress ([this] () { return progress (); });
    driver.set_clock_executor (wire.mock->control.get_strand ());
    settle ();
    // The link counts as established after the first delay of the policy.
    advance (ms (200));
  }

  ~bench_t ()
  {
    driver.set_progress (std::function<std::uint64_t ()> ());
    driver.set_clock_executor (tr::executor_type ());
    log->set_queue_probe (std::function<std::size_t ()> ());
    client->on_reconnect (nullptr);
    client.reset ();
    settle ();
    wire.release ();
    settle ();
    wire.device.reset ();
    settle ();
    wire.mock.reset ();
    settle ();
    driver.stop ();
  }

  bench_t (bench_t const &) = delete;
  bench_t &operator= (bench_t const &) = delete;

  void
  settle ()
  {
    driver.run_until (clock, [] () { return false; }, ms (0));
  }

  void
  advance (ms by)
  {
    driver.run_until (clock, [] () { return false; }, by);
  }

  bool
  run_until (std::function<bool ()> const &done, ms budget = ms (60000))
  {
    return driver.run_until (clock, done, budget);
  }

  snapshot_t
  snapshot () const
  {
    snapshot_t out;
    out.engine = client->statistics ();
    out.wire = log->counts ();
    out.session = wire.session->session_statistics ();
    out.device = wire.device->statistics ();
    out.link_reads = wire.reads ();
    return out;
  }

  tr::testing::device_simulator<P> &
  device ()
  {
    return *wire.device;
  }

  /// A sum that grows with every step of a handler chain of the bench.
  std::uint64_t
  progress () const
  {
    snapshot_t const now = snapshot ();
    return now.wire.tx + now.wire.rx + now.wire.errors + now.engine.requests
           + now.engine.replies + now.engine.timeouts + now.engine.reconnects
           + now.engine.unexpected_replies + now.engine.late_replies
           + now.engine.unknown_frames + now.engine.events
           + now.device.requests + now.device.replies
           + now.device.stream_messages + now.device.not_processed
           + now.session.attempts + now.session.failed_attempts;
  }

  driver_t driver;
  tr::testing::virtual_clock clock;
  std::shared_ptr<wire_log_t> log;
  std::shared_ptr<reconnect_state_t> reconnects;
  index::session_context_t session_values;
  transport_of_t<P, W> wire;
  std::unique_ptr<pr::client<P>> client;
};

// ---------------------------------------------------------------------------
// The steps of the script
// ---------------------------------------------------------------------------

/// Calls the method of `Row` with the sample of its request (and `confirm`
/// for a dangerous row).
template <typename P, typename Row, bool Dangerous, typename Indices>
struct sample_call_t;

template <typename P, typename Row, std::size_t... I>
struct sample_call_t<P, Row, false, sequence::index_sequence<I...>>
{
  template <typename Record>
  static void
  run (pr::client<P> &client, Record const &request, call_done_t handler)
  {
    client.template lumex_invoke<Row> (field::get<I> (request)..., handler);
  }
};

template <typename P, typename Row, std::size_t... I>
struct sample_call_t<P, Row, true, sequence::index_sequence<I...>>
{
  template <typename Record>
  static void
  run (pr::client<P> &client, Record const &request, call_done_t handler)
  {
    client.template lumex_invoke<Row> (pr::confirm, field::get<I> (request)...,
                                       handler);
  }
};

/// The shape of `Row` for `expected_outcome`.
template <typename P, typename Row>
tr::testing::row_shape_t
shape_of (pr::client_options_t const &options)
{
  index::row_t const *const row
      = index::protocol_index<P> ()->find_name (Row::identifier ());
  tr::testing::row_shape_t shape;
  shape.retries = options.dialogue.defaults.retries;
  if (row != nullptr && row->entry->options.has_retries)
    shape.retries = row->entry->options.retries;
  shape.idempotent = row_traits_t<Row>::idempotent::value;
  shape.reply = row_traits_t<Row>::form::value == 0
                    ? tr::testing::reply_shape::none
                    : tr::testing::reply_shape::one;
  shape.reconnects = true;
  shape.watchdog = false;
  return shape;
}

/// Starts the call of `Row` and returns where it ends.
template <typename P, typename W, typename Row>
std::shared_ptr<call_state_t>
start_call (bench_t<P, W> &bench)
{
  using record = typename layout::request_of<Row>::type;
  record const request = lumex_simulator_test::readable_sample_request<Row> ();
  std::shared_ptr<call_state_t> const state
      = std::make_shared<call_state_t> ();
  call_done_t done = { state };
  sample_call_t<
      P, Row, row_traits_t<Row>::dangerous::value,
      sequence::make_index_sequence<record::size::value>>::run (*bench.client,
                                                                request, done);
  return state;
}

inline bool
ended (std::shared_ptr<call_state_t> const &state)
{
  std::lock_guard<std::mutex> const lock (state->mutex);
  return state->calls != 0;
}

inline std::error_code
error_of (std::shared_ptr<call_state_t> const &state)
{
  std::lock_guard<std::mutex> const lock (state->mutex);
  return state->error;
}

/// A step's behaviour went away: the device answers whole again and the
/// link had the time to come back.
template <typename P, typename W>
void
restore (bench_t<P, W> &bench)
{
  bench.device ().set_behavior (behavior::perfect ());
  bench.advance (ms (1500));
}

/// Checks that a copy of the reply that came after the call was counted
/// before the next call, and that the next call of the row waits for its
/// own reply (one sent later than the copy that was dropped).
template <typename P, typename W, typename Row>
void
check_no_reply_kept (bench_t<P, W> &bench, std::string const &where)
{
  pr::dialogue_statistics_t const counted = bench.client->statistics ();
  EXPECT_GE (counted.late_replies + counted.unexpected_replies, 1u)
      << where << ": the copy of the reply was not counted";
  // The next reply trickles: a call that a kept copy answered would end at
  // once.
  bench.device ().set_behavior (behavior::slow_trickle (ms (2)));
  tr::timing::time_point_t const started = bench.clock.now ();
  std::shared_ptr<call_state_t> const next = start_call<P, W, Row> (bench);
  bool const done = bench.run_until ([&next] () { return ended (next); });
  tr::timing::time_point_t const finished = bench.clock.now ();
  EXPECT_TRUE (done) << where << ": the next call did not end";
  std::error_code const error = error_of (next);
  EXPECT_FALSE (error) << where << ": the next call ended with "
                       << error.message ();
  EXPECT_GT (finished, started)
      << where << ": the next call ended before its reply could come";
  restore (bench);
}

/// Checks that a call that succeeded under `echo` returned the value of the
/// echoed request read as its reply: the frame the device sent is the
/// request, and the value is what that frame reads as.
template <typename P, typename W, typename Row>
void
check_echo_value (bench_t<P, W> &bench, call_state_t &state,
                  std::string const &where)
{
  using value_type = pr::reply_of<Row>;
  wire_record_t const record = bench.log->copy ();
  ASSERT_FALSE (record.tx.empty ()) << where << ": no request";
  ASSERT_FALSE (record.rx.empty ()) << where << ": no frame";
  bytes_t const &request = record.tx.back ();
  EXPECT_EQ (record.rx.back (), request)
      << where << ": the frame that answered is not the echo";
  field::decode_context_t const context
      = bench.session_values.decode_context ();
  layout::decode_result_t<value_type> const decoded
      = layout::decode_reply_message<P, Row> (
          layout::bytes_view_t (request.data (), request.size ()), &context);
  ASSERT_TRUE (static_cast<bool> (decoded))
      << where << ": the echo does not read as the reply";
  std::lock_guard<std::mutex> const lock (state.mutex);
  ASSERT_TRUE (static_cast<bool> (state.value)) << where << ": no value";
  EXPECT_TRUE (*std::static_pointer_cast<value_type const> (state.value)
               == *decoded)
      << where << ": the value is not what the device sent";
}

/// Whether `kind` cuts the link (the step then waits for it to come back
/// before it reads the counters).
inline bool
cuts_link (behavior_kind kind)
{
  return kind == behavior_kind::disconnecting
         || kind == behavior_kind::works_then_disconnects
         || kind == behavior_kind::cable_unplugged;
}

/// Whether `kind` puts bytes on the line besides a reply.
inline bool
adds_noise (behavior_kind kind)
{
  return kind == behavior_kind::flood || kind == behavior_kind::garbage
         || kind == behavior_kind::malicious;
}

/// Whether `kind` answers some replies before it freezes.
inline bool
freezes (behavior_kind kind)
{
  return kind == behavior_kind::frozen
         || kind == behavior_kind::partial_then_frozen;
}

/// The reads of the mock a frame may take under `zero_read` (the empty reads
/// in front of it, the one that carries it, slack), and those a step takes
/// besides.
LUMEX_CONSTEXPR std::uint64_t KREADS_PER_FRAME = 8;

/// Whether the step of the cell is a known gap (see `known_gap`).
bool is_known_gap (std::string const &cell, std::string const &step);

/// Compares what a step saw with what it expected, or with the pinned
/// class of a known gap (see `known_gap`); true for a known gap.
bool check_class (std::string const &cell, std::string const &step,
                  char const *row, behavior_kind kind, outcome_class expected,
                  seen_class seen, std::error_code const &error, bool stream);

/// One call of `Row` under `shaped`, classified and checked.
/// `cable_unplugged` unplugs while the call is in flight (its write held):
/// a call made while the cable is out ends at once with `link_lost` by the
/// rules of the engine, which is not what the behaviour describes. Under
/// `frozen` and `partial_then_frozen` a call of a row with a reply goes
/// first and succeeds (the reply before the freeze).
template <typename P, typename W, typename Row>
void
run_step (bench_t<P, W> &bench, behavior_kind kind, behavior const &shaped,
          std::string const &cell, std::string const &step)
{
  std::string const where = cell + " " + step;
  pr::client_options_t const options = client_options<P, W> (bench.clock);
  tr::testing::row_shape_t const shape = shape_of<P, Row> (options);
  outcome_class const expected = tr::testing::expected_outcome (kind, shape);
  bool const replies = shape.reply != tr::testing::reply_shape::none;
  int reconnects_before = 0;
  {
    std::lock_guard<std::mutex> const lock (bench.reconnects->mutex);
    reconnects_before = bench.reconnects->calls;
  }
  observed_t observed;
  std::shared_ptr<call_state_t> state;
  if (kind == behavior_kind::cable_unplugged)
    {
      observed.before = bench.snapshot ();
      bench.wire.hold_writes ();
      state = start_call<P, W, Row> (bench);
      bench.settle ();
      bench.device ().set_behavior (shaped);
      bench.settle ();
      bench.wire.release_writes ();
    }
  else
    {
      bench.device ().set_behavior (shaped);
      bench.settle ();
      if (replies && freezes (kind))
        {
          std::shared_ptr<call_state_t> const first
              = start_call<P, W, Row> (bench);
          EXPECT_TRUE (bench.run_until ([&first] () { return ended (first); }))
              << where << ": the call before the freeze did not end";
          std::error_code const error = error_of (first);
          EXPECT_TRUE (!error || is_known_gap (cell, step))
              << where << ": the call before the freeze ("
              << Row::identifier () << ") ended with " << error.message ();
        }
      observed.before = bench.snapshot ();
      state = start_call<P, W, Row> (bench);
    }
  observed.ended = bench.run_until ([&state] () { return ended (state); });
  // Noise in front of no reply is counted when the line goes on: the bytes
  // of the next frame end it.
  if (adds_noise (kind) && !replies)
    {
      bench.advance (ms (10));
      bench.wire.feed (W::boundary ());
    }
  // What follows the reply (an echo, copies, noise) is counted too, and a
  // cut link comes back.
  bench.advance (cuts_link (kind) ? ms (1500) : ms (100));
  observed.error = error_of (state);
  observed.after = bench.snapshot ();
  std::string const named = where + " (" + Row::identifier () + ")";
  seen_class const seen = classify_call (observed);
  bool const gap = check_class (cell, step, Row::identifier (), kind, expected,
                                seen, observed.error, false);
  if (!gap)
    check_counters (kind, expected, observed, 1 + shape.retries, named);
  if (kind == behavior_kind::echo && seen == seen_class::success && replies)
    check_echo_value<P, W, Row> (bench, *state, named);
  if (kind == behavior_kind::zero_read)
    {
      // Zero-length reads do not spin.
      EXPECT_LE (observed.after.link_reads - observed.before.link_reads,
                 KREADS_PER_FRAME
                     * (1 + observed.after.wire.rx - observed.before.wire.rx))
          << named << ": reads";
    }
  if (cuts_link (kind) && W::identify_row () != nullptr)
    {
      // The device was checked on the new connection (the identity reads
      // from the reply of the row of identify_with).
      std::lock_guard<std::mutex> const lock (bench.reconnects->mutex);
      EXPECT_GT (bench.reconnects->calls, reconnects_before)
          << named << ": no check after the reconnect";
      EXPECT_FALSE (bench.reconnects->error)
          << named << ": the check after the reconnect failed with "
          << bench.reconnects->error.message ();
      EXPECT_FALSE (bench.reconnects->model.empty ())
          << named << ": the check after the reconnect read no identity";
    }
  if (kind == behavior_kind::repeating && replies
      && seen == seen_class::dropped_counted)
    check_no_reply_kept<P, W, Row> (bench, where);
  else
    restore (bench);
}

/// The row of `accepted_in` in the state that accepts it, on both sides,
/// under `shaped`.
template <typename P, typename W, typename Row>
void
run_accepted (bench_t<P, W> &bench, behavior_kind kind, behavior const &shaped,
              std::string const &cell)
{
  bench.client->engine ().set_state (W::accepted_state ());
  bench.device ().set_state (W::accepted_state ());
  run_step<P, W, Row> (bench, kind, shaped, cell, "accepted");
}

/// The row of `accepted_in` in another state: the client refuses it and
/// sends nothing.
template <typename P, typename W, typename Row>
void
run_refused (bench_t<P, W> &bench, std::string const &where)
{
  bench.client->engine ().set_state (W::accepted_state () + 1);
  snapshot_t const before = bench.snapshot ();
  std::shared_ptr<call_state_t> const state = start_call<P, W, Row> (bench);
  EXPECT_TRUE (bench.run_until ([&state] () { return ended (state); }))
      << where << ": the refused call did not end";
  bench.advance (ms (100));
  snapshot_t const after = bench.snapshot ();
  std::error_code const error = error_of (state);
  EXPECT_TRUE (
      error
      == lumex::core::protocol::error::protocol_errc::not_accepted_in_state)
      << where << ": a row outside its states ended with " << error.message ();
  EXPECT_EQ (after.wire.tx, before.wire.tx)
      << where << ": a row outside its states was sent";
  EXPECT_EQ (after.engine.state_refusals - before.engine.state_refusals, 1u)
      << where << ": the refusal was not counted";
  bench.client->engine ().set_state (W::accepted_state ());
}

/// A query whose reply ends after its timeout, within the window of late
/// replies of a `by_order` layer: the call times out and the reply is
/// dropped as a late one, not taken for the reply of the next request.
template <typename P, typename W, typename Row>
void
run_late (bench_t<P, W> &bench, std::string const &where)
{
  bench.device ().set_behavior (
      behavior::slow_trickle (ms (W::late_trickle_ms ())));
  bench.settle ();
  snapshot_t const before = bench.snapshot ();
  std::shared_ptr<call_state_t> const state = start_call<P, W, Row> (bench);
  EXPECT_TRUE (bench.run_until ([&state] () { return ended (state); }))
      << where << ": the call did not end";
  bench.advance (ms (1000));
  snapshot_t const after = bench.snapshot ();
  std::error_code const error = error_of (state);
  EXPECT_TRUE (
      error == lumex::core::protocol::error::protocol_errc::timeout
      || error
             == lumex::core::protocol::error::protocol_errc::retries_exhausted)
      << where << ": ended with " << error.message ();
  EXPECT_GE (after.engine.late_replies - before.engine.late_replies, 1u)
      << where << ": the late reply was not counted as late";
  EXPECT_EQ (after.engine.unexpected_replies, before.engine.unexpected_replies)
      << where << ": the late reply was counted as unexpected";
  restore (bench);
}

/// A reply cut in the middle, then the device hangs up: the part that waits
/// in the decoder is not carried over to the new connection, where the next
/// call is answered at the first attempt without a framing error.
template <typename P, typename W, typename Row>
void
run_close (bench_t<P, W> &bench, std::string const &where)
{
  snapshot_t const start = bench.snapshot ();
  bench.device ().set_behavior (behavior::partial_then_frozen (0, 2));
  bench.settle ();
  std::shared_ptr<call_state_t> const cut = start_call<P, W, Row> (bench);
  EXPECT_TRUE (bench.run_until ([&cut] () { return ended (cut); }))
      << where << ": the call of the cut reply did not end";
  bench.wire.hang_up ();
  bench.device ().set_behavior (behavior::perfect ());
  bench.advance (ms (1500));
  snapshot_t const before = bench.snapshot ();
  std::shared_ptr<call_state_t> const next = start_call<P, W, Row> (bench);
  EXPECT_TRUE (bench.run_until ([&next] () { return ended (next); }))
      << where << ": the call after the reconnect did not end";
  bench.advance (ms (100));
  snapshot_t const after = bench.snapshot ();
  std::error_code const error = error_of (next);
  EXPECT_FALSE (error) << where << ": the call after the reconnect ended with "
                       << error.message ();
  EXPECT_GT (before.engine.reconnects, start.engine.reconnects)
      << where << ": the hang-up did not reconnect";
  EXPECT_EQ (after.engine.retries, before.engine.retries)
      << where << ": the call after the reconnect needed a retry";
  EXPECT_EQ (after.wire.errors, before.wire.errors)
      << where << ": the cut reply glued to the next frame";
  restore (bench);
}

/// `KBURST` calls of `Row` at once under `shaped`: the dialogue holds
/// `KQUEUE_BOUND` of them in its queue behind the one in flight and refuses
/// the others with `queue_full`; every call ends.
template <typename P, typename W, typename Row>
void
run_burst (bench_t<P, W> &bench, behavior const &shaped,
           std::string const &where)
{
  bench.device ().set_behavior (shaped);
  bench.settle ();
  pr::dialogue_statistics_t const before = bench.client->statistics ();
  std::vector<std::shared_ptr<call_state_t>> calls;
  for (std::size_t i = 0; i < KBURST; ++i)
    calls.push_back (start_call<P, W, Row> (bench));
  bool const done = bench.run_until (
      [&calls] ()
        {
          for (std::size_t i = 0; i < calls.size (); ++i)
            if (!ended (calls[i]))
              return false;
          return true;
        },
      ms (600000));
  EXPECT_TRUE (done) << where << ": a call of the burst did not end";
  std::size_t refused = 0;
  for (std::size_t i = 0; i < calls.size (); ++i)
    if (error_of (calls[i])
        == lumex::core::protocol::error::protocol_errc::queue_full)
      ++refused;
  pr::dialogue_statistics_t const after = bench.client->statistics ();
  // One call in flight (no table names max_in_flight), the bound waiting.
  EXPECT_EQ (refused, KBURST - KQUEUE_BOUND - 1)
      << where << ": calls refused with queue_full";
  EXPECT_EQ (after.queue_refusals - before.queue_refusals, refused)
      << where << ": queue refusals";
  EXPECT_EQ (after.max_queued, KQUEUE_BOUND) << where << ": max_queued";
  restore (bench);
}

/// Runs `f (static_cast<Row *> (nullptr))` when the table has the row.
template <typename Row> struct if_row_t
{
  template <typename F>
  static void
  run (F const &f)
  {
    f (static_cast<Row *> (nullptr));
  }
};

template <> struct if_row_t<void>
{
  template <typename F>
  static void
  run (F const &)
  {
  }
};

/// What a runner does with its row.
enum class step_kind : std::uint8_t
{
  call,
  stream,
  accepted,
  refused,
  late,
  close,
  burst
};

/// Runs one step of the script with the row it is given.
template <typename P, typename W> struct step_runner_t
{
  bench_t<P, W> *bench;
  step_kind what;
  behavior_kind kind;
  behavior const *shaped;
  std::string cell;
  std::string step;

  template <typename Row> void operator() (Row *) const;
};

/// The stream window of `Row`: the stream runs whole for three intervals
/// (the client watches it), then for two virtual seconds under `shaped`.
template <typename P, typename W, typename Row>
void
run_stream (bench_t<P, W> &bench, behavior_kind kind, behavior const &shaped,
            std::string const &cell, std::string const &step)
{
  std::string const where = cell + " " + step;
  index::row_t const *const row
      = index::protocol_index<P> ()->find_name (Row::identifier ());
  ms const interval (row->entry->options.stream_interval_ms);
  tr::testing::row_shape_t shape;
  shape.reply = tr::testing::reply_shape::stream;
  shape.reconnects = true;
  outcome_class const expected = tr::testing::expected_outcome (kind, shape);
  std::shared_ptr<stream_state_t> const seen_messages
      = std::make_shared<stream_state_t> ();
  stream_seen_t seen = { seen_messages, &bench.clock };
  pr::subscription_t const subscription
      = bench.client->template on<Row> (seen);
  EXPECT_NE (subscription.id, 0u) << where << ": subscription refused";
  EXPECT_TRUE (bench.device ().template start_stream<Row> ());
  bench.advance (interval * 3);
  observed_t observed;
  observed.ended = true;
  observed.before = bench.snapshot ();
  std::size_t base = 0;
  {
    std::lock_guard<std::mutex> const lock (seen_messages->mutex);
    base = seen_messages->times.size ();
  }
  EXPECT_GE (base, 1u) << where << ": the stream did not run whole";
  tr::timing::time_point_t const half = bench.clock.now () + ms (1000);
  bench.device ().set_behavior (shaped);
  bench.advance (ms (2000));
  observed.after = bench.snapshot ();
  std::size_t late_half = 0;
  {
    std::lock_guard<std::mutex> const lock (seen_messages->mutex);
    for (std::size_t i = base; i < seen_messages->times.size (); ++i)
      if (seen_messages->times[i] >= half)
        ++late_half;
  }
  bench.device ().template stop_stream<Row> ();
  bench.client->unsubscribe (subscription);
  seen_class const seen_as = classify_stream (observed, late_half);
  check_class (cell, step, Row::identifier (), kind, expected, seen_as,
               std::error_code (), true);
  restore (bench);
}

template <typename P, typename W>
template <typename Row>
void
step_runner_t<P, W>::operator() (Row *) const
{
  std::string const where = cell + " " + step;
  switch (what)
    {
    case step_kind::call:
      run_step<P, W, Row> (*bench, kind, *shaped, cell, step);
      break;
    case step_kind::stream:
      run_stream<P, W, Row> (*bench, kind, *shaped, cell, step);
      break;
    case step_kind::accepted:
      run_accepted<P, W, Row> (*bench, kind, *shaped, cell);
      break;
    case step_kind::refused:
      run_refused<P, W, Row> (*bench, where);
      break;
    case step_kind::late:
      run_late<P, W, Row> (*bench, where);
      break;
    case step_kind::close:
      run_close<P, W, Row> (*bench, where);
      break;
    case step_kind::burst:
      run_burst<P, W, Row> (*bench, *shaped, where);
      break;
    }
}

/// Whether the script of `P` has a row with a reply.
template <typename P>
struct script_has_reply_t
    : std::integral_constant<
          bool,
          !std::is_same<typename script_rows_t<P>::query, void>::value
              || !std::is_same<typename script_rows_t<P>::stream, void>::value>
{
};

/// Every debug line of a frame is the describe line of the index, in both
/// directions (a cell with a perfect device).
template <typename P, typename W>
void
check_describe (bench_t<P, W> &bench, std::string const &where)
{
  wire_record_t const record = bench.log->copy ();
  index::protocol_index_t const &where_index = *index::protocol_index<P> ();
  std::vector<std::string> const tx = log_guard_t::lines ("TX ");
  std::vector<std::string> const rx = log_guard_t::lines ("RX ");
  ASSERT_EQ (tx.size (), record.tx.size ()) << where << ": TX lines";
  ASSERT_EQ (rx.size (), record.rx.size ()) << where << ": RX lines";
  for (std::size_t i = 0; i < tx.size (); ++i)
    EXPECT_EQ (tx[i], where_index.describe (
                          layout::bytes_view_t (record.tx[i].data (),
                                                record.tx[i].size ()),
                          index::frame_direction::tx, &bench.session_values))
        << where;
  for (std::size_t i = 0; i < rx.size (); ++i)
    EXPECT_EQ (rx[i], where_index.describe (
                          layout::bytes_view_t (record.rx[i].data (),
                                                record.rx[i].size ()),
                          index::frame_direction::rx, &bench.session_values))
        << where;
}

/// The oversized cell: the block of the behaviour (in the steps) and a
/// block fed by hand that goes above the limit of the codec as the codec
/// reads it, on every stream layer, those without a reply too.
template <typename P, typename W>
void
check_oversized (bench_t<P, W> &bench, wire_counts_t const &before,
                 std::string const &where)
{
  wire_counts_t const steps = bench.log->counts ();
  // A block is dropped once: a later block meets a decoder that is still
  // dropping (no frame boundary came between), so the count is over the
  // steps.
  if (script_has_reply_t<P>::value)
    {
      if (W::oversized_reads_too_large ())
        EXPECT_GE (steps.frame_too_large - before.frame_too_large, 1u)
            << where << ": frame_too_large not counted";
      else
        EXPECT_GE (steps.errors - before.errors, 1u)
            << where << ": the block was not counted";
    }
  if (W::datagram ())
    return;
  // The boundary first: the decoder may still drop the block of the last
  // step.
  bench.wire.feed (W::boundary ());
  bench.wire.feed (oversized_block_of<W> ());
  bench.advance (ms (100));
  EXPECT_GE (bench.log->counts ().frame_too_large - steps.frame_too_large, 1u)
      << where << ": a frame above the limit, fed by hand, was not counted";
  restore (bench);
}

/// One cell: layer `P` on the wire `W` under `kind`.
template <typename P, typename W>
void
run_cell (behavior_kind kind, bool threaded, std::string const &where)
{
  using rows = script_rows_t<P>;
  std::chrono::steady_clock::time_point const started
      = std::chrono::steady_clock::now ();
  {
    log_guard_t const log (kind == behavior_kind::perfect && !threaded);
    bench_t<P, W> bench (threaded);
    behavior const shaped = matrix_behavior (kind, W::oversized_fill ());
    wire_counts_t const at_start = bench.log->counts ();
    step_runner_t<P, W> runner
        = { &bench, step_kind::call, kind, &shaped, where, "query" };
    if_row_t<typename rows::query>::run (runner);
    runner.step = "command";
    if_row_t<typename rows::command>::run (runner);
    runner.step = "dangerous";
    if_row_t<typename rows::dangerous>::run (runner);
    runner.step = "accepted";
    runner.what = step_kind::accepted;
    if_row_t<typename rows::accepted>::run (runner);
    runner.step = "stream";
    runner.what = step_kind::stream;
    behavior const streamed = stream_behavior (kind, W::oversized_fill ());
    runner.shaped = &streamed;
    if_row_t<typename rows::stream>::run (runner);
    if (kind == behavior_kind::oversized)
      check_oversized (bench, at_start, where);
    if (kind == behavior_kind::perfect && !threaded)
      {
        runner.step = "refused";
        runner.what = step_kind::refused;
        if_row_t<typename rows::accepted>::run (runner);
      }
    if (kind == behavior_kind::slow_trickle && W::late_trickle_ms () > 0)
      {
        runner.step = "late";
        runner.what = step_kind::late;
        if_row_t<typename rows::query>::run (runner);
      }
    if (kind == behavior_kind::partial_then_frozen && !W::datagram ())
      {
        runner.step = "close";
        runner.what = step_kind::close;
        if_row_t<typename rows::query>::run (runner);
      }
    if (!threaded
        && (kind == behavior_kind::perfect || kind == behavior_kind::silent
            || kind == behavior_kind::flood))
      {
        runner.step = "burst";
        runner.what = step_kind::burst;
        if_row_t<typename rows::burst>::run (runner);
      }
    pr::dialogue_statistics_t const counted = bench.client->statistics ();
    EXPECT_LE (counted.max_queued, KQUEUE_BOUND)
        << where << ": the queue of the dialogue went above its bound";
    EXPECT_LE (bench.log->counts ().max_queued_frames, KADAPTER_QUEUE)
        << where << ": the queue of the adapter went above its bound";
    if (kind == behavior_kind::perfect && !threaded)
      check_describe (bench, where);
  }
  std::chrono::steady_clock::duration const took
      = std::chrono::steady_clock::now () - started;
  EXPECT_LT (took, std::chrono::seconds (5))
      << where << ": the cell took longer than the real-time guard";
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

/// A cell to register: its gtest suite and name, and its body.
struct cell_t
{
  std::string suite;
  std::string name;
  std::function<void ()> body;
};

/// Registers `cells` as tests (at static initialization) and keeps their
/// names for the check of the matrix itself.
void register_cells (std::vector<cell_t> const &cells);

/// The names of every registered cell, `suite.name`.
std::vector<std::string> registered_cells ();

/// The cells of layer `P` on the wire `W` (the layer `layer` of device
/// `device`, letters only), one per kind its wire takes; `threaded` names
/// them in the suite `threads` and runs them on a runner of four threads.
template <typename P, typename W = wire_of<P>>
std::vector<cell_t>
cells_of (char const *device, char const *layer, bool threaded = false)
{
  std::vector<cell_t> out;
  bool const datagram = W::datagram ();
  std::vector<behavior_kind> const kinds = kinds_for (datagram);
  for (std::size_t i = 0; i < kinds.size (); ++i)
    {
      std::string const tail = std::string (layer) + "."
                               + (datagram ? "datagram" : "stream") + "."
                               + tr::testing::behavior_kind_name (kinds[i]);
      cell_t cell;
      cell.suite = threaded ? std::string ("threads") : std::string (device);
      cell.name = threaded ? std::string (device) + "." + tail : tail;
      behavior_kind const kind = kinds[i];
      std::string const where = std::string (device) + "." + tail;
      cell.body = [kind, threaded, where] ()
        { run_cell<P, W> (kind, threaded, where); };
      out.push_back (cell);
    }
  return out;
}

/// Joins lists of cells.
inline std::vector<cell_t>
join (std::vector<cell_t> a, std::vector<cell_t> const &b)
{
  a.insert (a.end (), b.begin (), b.end ());
  return a;
}
} // namespace lumex_matrix_test

#endif // !LUMEX_TESTS_APPLIED_TRANSPORT_MATRIX_SUPPORT_HPP
