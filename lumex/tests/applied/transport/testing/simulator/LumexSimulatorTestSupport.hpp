// The bench of the device simulator tests: an io_context the test polls
// itself, a virtual clock, a mock stream (or a mock datagram transport)
// framed with SLIP, the simulator of a table on the mock and a dialogue
// engine on the other side of it, the way client code meets the device. The
// requests go through the engine (the exchange of a row as the generated
// client fills it), so a reply the engine completes is a reply a client
// decodes. No real device, its names, codes or values are in it.
#ifndef LUMEX_TESTS_APPLIED_TRANSPORT_TESTING_SIMULATOR_TEST_SUPPORT_HPP
#define LUMEX_TESTS_APPLIED_TRANSPORT_TESTING_SIMULATOR_TEST_SUPPORT_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/LumexTransport"
#include "lumex/core/framing/LumexFraming"
#include "lumex/core/protocol/LumexProtocol"

namespace lumex_simulator_test
{
namespace tr = lumex::applied::transport;
namespace pr = lumex::applied::transport::protocol;
namespace dialogue = lumex::applied::transport::protocol::dialogue;
namespace fr = lumex::core::framing;
namespace index = lumex::core::protocol::index;
namespace layout = lumex::core::protocol::layout;
namespace perror = lumex::core::protocol::error;
using bytes_t = std::vector<std::uint8_t>;
using ms = std::chrono::milliseconds;
using perror::protocol_errc;
using tr::error::transport_errc;

/// What one exchange ended with.
struct outcome_t
{
  int calls = 0;
  std::error_code error;
  bytes_t frame;
  /// The time of the clock when the handler ran.
  tr::timing::time_point_t at;
};

using outcome_ptr = std::shared_ptr<outcome_t>;

/// The completion of a test exchange: records the outcome.
struct record_t
{
  outcome_ptr out;
  tr::testing::virtual_clock *clock;

  void
  operator() (std::error_code error, pr::reply_frame_t frame)
  {
    ++out->calls;
    out->error = error;
    out->frame = std::move (frame);
    out->at = clock->now ();
  }
};

inline layout::bytes_view_t
view_of (bytes_t const &data)
{
  return layout::bytes_view_t (data.data (), data.size ());
}

namespace sample_detail
{
namespace field = lumex::core::protocol::field;
namespace registry = lumex::core::protocol::registry;
namespace sd = lumex::applied::transport::testing::simulator_detail;

// Picks the entries of the registry keys and values of a sample request.
template <typename Impl> struct pick_t
{
  template <typename Value>
  static void
  keys (Value &, bool)
  {
  }

  template <typename Record, typename Value>
  static void
  values (Record &, Value &)
  {
  }
};

template <typename Sequence, typename... F> struct pick_group_t;

template <std::size_t... I, typename... F>
struct pick_group_t<lumex::core::utility::sequence::index_sequence<I...>, F...>
{
  template <typename Record>
  static void
  keys (Record &record, bool also_write)
  {
    int const done[] = { 0, (pick_t<typename field::impl_of<F>::type>::keys (
                                 field::get<I> (record), also_write),
                             0)... };
    (void)done;
  }

  template <typename Record>
  static void
  values (Record &record)
  {
    int const done[] = { 0, (pick_t<typename field::impl_of<F>::type>::values (
                                 record, field::get<I> (record)),
                             0)... };
    (void)done;
  }
};

template <template <typename...> class List, typename... F>
struct pick_t<field::group_impl<List<F...>>>
{
  using fields = pick_group_t<
      lumex::core::utility::sequence::make_index_sequence<sizeof...(F)>, F...>;

  template <typename Value>
  static void
  keys (Value &value, bool also_write)
  {
    fields::keys (value, also_write);
  }

  template <typename Record, typename Value>
  static void
  values (Record &, Value &value)
  {
    fields::values (value);
  }
};

// The first entry that can be read (and written, for a request that
// writes).
template <typename R, typename F1, typename F2>
struct pick_t<registry::registry_key_<R, F1, F2>>
{
  template <typename Value>
  static void
  keys (Value &key, bool also_write)
  {
    registry::entries_t const entries = R::entries ();
    for (std::size_t n = 0; n < entries.size (); ++n)
      if (!registry::check_access (entries.data ()[n],
                                   registry::access_t::read)
          && (!also_write
              || !registry::check_access (entries.data ()[n],
                                          registry::access_t::write)))
        {
          key = static_cast<typename R::id> (n);
          return;
        }
  }

  template <typename Record, typename Value>
  static void
  values (Record &, Value &)
  {
  }
};

// A value the entry of its key accepts.
template <typename R, std::size_t KeyIndex, typename RawField,
          registry::access_t Needs>
struct pick_t<registry::registry_value_<R, KeyIndex, RawField, Needs>>
{
  template <typename Value>
  static void
  keys (Value &, bool)
  {
  }

  template <typename Record, typename Value>
  static void
  values (Record &record, Value &value)
  {
    registry::entries_t const entries = R::entries ();
    std::size_t const at
        = static_cast<std::size_t> (field::get<KeyIndex> (record));
    if (at < entries.size ())
      sd::set_raw (value, entries.data ()[at],
                   sd::default_raw (entries.data ()[at]));
  }
};
} // namespace sample_detail

/**
 * The sample request of `Row` (every field its first valid value) with the
 * registry entries a device can answer: a request that names an entry the
 * reply of the row would have to report must name one that can be read (and
 * written, for a row that writes), or the device refuses it. The one place
 * where the tests choose the entry of a sample request.
 */
template <typename Row>
typename layout::request_of<Row>::type
readable_sample_request ()
{
  using fields = typename Row::fields_type;
  typename layout::request_of<Row>::type request
      = lumex::core::protocol::field::sample_value<fields> ();
  using impl = typename lumex::core::protocol::field::impl_of<fields>::type;
  bool const also_write
      = tr::testing::simulator_detail::registry_walk_t<impl>::has_value::value;
  sample_detail::pick_t<impl>::keys (request, also_write);
  sample_detail::pick_t<impl>::values (request, request);
  return request;
}

/// The row pointer of a row type of the index of `P`, null for void.
template <typename P, typename Row> struct row_ptr_t
{
  static index::row_t const *
  get ()
  {
    return index::protocol_index<P> ()->find_name (Row::identifier ());
  }
};

template <typename P> struct row_ptr_t<P, void>
{
  static index::row_t const *
  get ()
  {
    return nullptr;
  }
};

/// The bench for layer `P` (see the file description).
template <typename P> class bench_t
{
public:
  using device_type = tr::testing::device_simulator<P>;

  /// A stream bench: the device behind a mock stream.
  explicit bench_t (tr::testing::simulator_options_t device_options
                    = tr::testing::simulator_options_t (),
                    pr::dialogue_options_t engine_options
                    = pr::dialogue_options_t ())
  {
    tr::testing::mock_options_t mock_options;
    mock_options.timers = clock.source ();
    tr::testing::mock_stream mock = tr::testing::make_mock_stream (
        context.get_executor (), mock_options);
    stream_control.reset (new tr::testing::mock_stream_control (mock.control));
    bool opened = false;
    mock.stream.async_open (
        [&opened] (std::error_code error)
          {
            EXPECT_FALSE (error);
            opened = true;
          });
    context.poll ();
    context.restart ();
    EXPECT_TRUE (opened);
    device.reset (new device_type (mock.control, fr::slip::slip_codec (),
                                   clock, device_options));
    prepare (engine_options);
    engine.reset (new pr::dialogue_engine (
        pr::frame_link (tr::framed::framed_stream<fr::slip::slip_codec> (
            std::move (mock.stream), fr::slip::slip_codec ())),
        index_of (), engine_options));
    settle ();
  }

  /// A datagram bench: the device behind a mock datagram transport, every
  /// report of the requests and of the replies numbered `report_id`.
  bench_t (std::uint8_t report_id,
           tr::testing::simulator_options_t device_options
           = tr::testing::simulator_options_t (),
           pr::dialogue_options_t engine_options = pr::dialogue_options_t ())
  {
    tr::testing::mock_options_t mock_options;
    mock_options.timers = clock.source ();
    tr::testing::mock_datagram mock = tr::testing::make_mock_datagram (
        context.get_executor (), mock_options);
    datagram_control.reset (
        new tr::testing::mock_datagram_control (mock.control));
    mock.transport.async_open ([] (std::error_code) {});
    context.poll ();
    context.restart ();
    device.reset (new device_type (mock.control, fr::slip::slip_codec (),
                                   report_id, clock, device_options));
    prepare (engine_options);
    engine.reset (new pr::dialogue_engine (
        pr::frame_link (
            tr::framed::framed_datagram<fr::slip::slip_codec> (
                std::move (mock.transport), fr::slip::slip_codec ()),
            report_id),
        index_of (), engine_options));
    settle ();
  }

  ~bench_t ()
  {
    engine.reset ();
    settle ();
    device.reset ();
    settle ();
  }

  bench_t (bench_t const &) = delete;
  bench_t &operator= (bench_t const &) = delete;

  static index::protocol_index_t const &
  index_of ()
  {
    return *index::protocol_index<P> ();
  }

  template <typename Row>
  static index::row_t const *
  row_of ()
  {
    return row_ptr_t<P, Row>::get ();
  }

  /// The exchange of `Row` with `args`, as the generated client fills it.
  template <typename Row, typename... Args>
  pr::exchange_t
  make_exchange (Args const &...args) const
  {
    using traits = dialogue::row_traits_t<P, Row>;
    pr::exchange_t out;
    layout::encode_result_t const encoded
        = layout::encode_message<P, Row> (out.message, args...);
    EXPECT_TRUE (encoded.has_value ());
    out.row = row_of<Row> ();
    out.how = traits::how ();
    out.reply_row = row_ptr_t<P, typename traits::reply_row>::get ();
    out.matcher = traits::matcher ();
    out.reader = dialogue::reply_reader_of<P, Row> ();
    return out;
  }

  /// The exchange of `Row` with the request `record`, encoded with the
  /// session values `context` (null: none).
  template <typename Row>
  pr::exchange_t
  make_exchange_of (
      typename layout::request_of<Row>::type const &record,
      lumex::core::protocol::field::decode_context_t const *context) const
  {
    using traits = dialogue::row_traits_t<P, Row>;
    pr::exchange_t out;
    layout::encode_result_t const encoded
        = layout::encode_message_record<P, Row> (out.message, record, context);
    EXPECT_TRUE (encoded.has_value ());
    out.row = row_of<Row> ();
    out.how = traits::how ();
    out.reply_row = row_ptr_t<P, typename traits::reply_row>::get ();
    out.matcher = traits::matcher ();
    out.reader = dialogue::reply_reader_of<P, Row> ();
    return out;
  }

  /// Starts `request` and returns where its outcome will be.
  outcome_ptr
  start_exchange (pr::exchange_t request)
  {
    outcome_ptr const out = std::make_shared<outcome_t> ();
    record_t handler = { out, &clock };
    engine->async_exchange (std::move (request), std::move (handler));
    return out;
  }

  /// Sends the request of `Row` and runs until it ended (or `budget` of
  /// virtual time passed).
  template <typename Row, typename... Args>
  outcome_ptr
  call (Args const &...args)
  {
    outcome_ptr const out = start_exchange (make_exchange<Row> (args...));
    EXPECT_TRUE (run_until ([&] () { return out->calls != 0; }));
    return out;
  }

  /// Runs the ready handlers without moving the clock.
  void
  settle ()
  {
    tr::testing::run_until (context, clock, [] () { return false; }, ms (0));
  }

  /// Runs and moves the clock until `done` or `budget` passed.
  bool
  run_until (std::function<bool ()> const &done, ms budget = ms (60000))
  {
    return tr::testing::run_until (context, clock, done, budget);
  }

  /// Moves the clock by `by` and runs what became ready.
  void
  advance (ms by)
  {
    tr::testing::run_until (context, clock, [] () { return false; }, by);
  }

  /// Writes `payload` as one frame, whatever it says: the exchange of
  /// `Row` (a row without a reply, so that it ends with the write) with its
  /// message replaced.
  template <typename Row>
  outcome_ptr
  send_payload (bytes_t const &payload)
  {
    pr::exchange_t request = make_exchange<Row> ();
    request.message = payload;
    outcome_ptr const out = start_exchange (request);
    EXPECT_TRUE (run_until ([&] () { return out->calls != 0; }));
    return out;
  }

  /// The frame of `payload` as the codec of the bench writes it.
  static bytes_t
  framed (bytes_t const &payload)
  {
    bytes_t wire;
    fr::slip::slip_codec const codec;
    EXPECT_TRUE (codec.encode (view_of (payload), wire).has_value ());
    return wire;
  }

  /// The messages of `Row` the engine's subscription saw.
  template <typename Row>
  pr::subscription_t
  subscribe (std::vector<bytes_t> &into)
  {
    return engine->subscribe (row_of<Row> (),
                              [&into] (pr::reply_frame_t const &frame)
                                { into.push_back (frame); });
  }

  asio::io_context context;
  tr::testing::virtual_clock clock;
  std::unique_ptr<tr::testing::mock_stream_control> stream_control;
  std::unique_ptr<tr::testing::mock_datagram_control> datagram_control;
  std::unique_ptr<device_type> device;
  std::unique_ptr<pr::dialogue_engine> engine;

private:
  void
  prepare (pr::dialogue_options_t &options)
  {
    options.timers = clock.source ();
    if (options.name == "dialogue")
      options.name = "lab";
    if (options.defaults.timeout_ms == 1000)
      options.defaults.timeout_ms = 200;
  }
};
} // namespace lumex_simulator_test

#endif // !LUMEX_TESTS_APPLIED_TRANSPORT_TESTING_SIMULATOR_TEST_SUPPORT_HPP
