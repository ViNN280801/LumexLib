// A client over a session on the mocks of a device simulator: the table and
// the benches of the reconnect tests. The factory of the session gives the
// same mock again, as a device that comes back on the same wire; the clock is
// virtual, for the session, the mock, the device and the dialogue.

#ifndef LUMEX_TESTS_APPLIED_TRANSPORT_PROTOCOL_CLIENT_RECONNECT_SUPPORT_HPP
#define LUMEX_TESTS_APPLIED_TRANSPORT_PROTOCOL_CLIENT_RECONNECT_SUPPORT_HPP

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "lumex/tests/applied/transport/protocol/client/LumexClientTestSupport.hpp"

namespace lumex_tests
{
namespace reconnect_tables
{
namespace dialogue = lumex::applied::transport::protocol::dialogue;

using codec_t = lumex_tests::client_tables::codec_t;

struct gauge_t;
struct gauge_a_t;
struct gauge_b_t;

struct gauge_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;

  template <typename P>
  using dialogue_options
      = dialogue::options<dialogue::identify_with<typename P::WHO_t>,
                          dialogue::layer_set<gauge_a_t, gauge_b_t>,
                          dialogue::default_timeout_ms<100>>;
};

LUMEX_PROTOCOL_LAYOUT (who_t, (text_<8>, u16_le_, u8_, u8_),
                       ("model", "firmware", "layer", "serial"));

// clang-format off
#define GAUGE_ROWS(X, X_AS) \
  X (WHO, 0x01, "Who", (), (), (who_t), (idempotent)) \
  X (READ, 0x10, "Read", (), (), (u16_le_), (idempotent, retries (2))) \
  X (SET, 0x11, "Set", (u8_), ("value"), (u8_), ()) \
  X (WIPE, 0x12, "Wipe", (), (), (u8_), (idempotent, dangerous)) \
  X (FEED, 0x13, "Feed", (), (), (u8_), (stream (100))) \
  X (GO, 0x14, "Go", (), (), (u8_), (enables_stream<FEED_t> ())) \
  X (ORDERED, 0x16, "By order", (), (), (u8_), \
     (correlate (correlation::by_order))) \
  X (SLOW, 0x17, "Long", (), (), (u8_), (idempotent, timeout (10000))) \
  X (STEP, 0x18, "Step", (), (), (two_phase<GO_t, u8_>), (idempotent)) \
  X (FEED_TOO, 0x19, "Feed too", (), (), (u8_), (stream (100))) \
  X (GO_TOO, 0x1A, "Go too", (), (), (u8_), (enables_stream<FEED_TOO_t> ()))
#define GAUGE_A_ROWS(X, X_AS) \
  X (ONLY_A, 0x20, "Only a", (), (), (u8_), (idempotent))
#define GAUGE_B_ROWS(X, X_AS) \
  X (ONLY_B, 0x21, "Only b", (), (), (u8_), (idempotent))
// clang-format on

LUMEX_PROTOCOL_DEFINE (gauge_t, codec_t, gauge_header_t, GAUGE_ROWS);
LUMEX_PROTOCOL_EXTEND (gauge_a_t, gauge_t, GAUGE_A_ROWS);
LUMEX_PROTOCOL_EXTEND (gauge_b_t, gauge_t, GAUGE_B_ROWS);

/// identify_with without layer_set.
struct single_t;

struct single_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;

  template <typename P>
  using dialogue_options
      = dialogue::options<dialogue::identify_with<typename P::WHO_t>,
                          dialogue::default_timeout_ms<100>>;
};

// clang-format off
#define SINGLE_ROWS(X, X_AS) \
  X (WHO, 0x01, "Who", (), (), (who_t), (idempotent)) \
  X (READ, 0x10, "Read", (), (), (u16_le_), (idempotent))
// clang-format on

LUMEX_PROTOCOL_DEFINE (single_t, codec_t, single_header_t, SINGLE_ROWS);

/// The states of the plain gauge.
enum class gauge_state : int
{
  idle = 0,
  busy = 1
};

/// Neither identify_with nor layer_set; a stream and a state.
struct plain_gauge_t;

struct plain_gauge_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;

  template <typename P>
  using dialogue_options
      = dialogue::options<dialogue::default_timeout_ms<100>,
                          dialogue::state_from<typename P::STATE_t, 0>>;
};

// clang-format off
#define PLAIN_GAUGE_ROWS(X, X_AS) \
  X (READ, 0x10, "Read", (), (), (u16_le_), (idempotent)) \
  X (SET, 0x11, "Set", (u8_), ("value"), (u8_), ()) \
  X (STATE, 0x12, "State", (), (), (u8_), ()) \
  X (BUSY, 0x13, "Only when busy", (), (), (u8_), (accepted_in (gauge_state::busy)))
// clang-format on

LUMEX_PROTOCOL_DEFINE (plain_gauge_t, codec_t, plain_gauge_header_t,
                       PLAIN_GAUGE_ROWS);

/// identify_with, with a row of no reader: identity_of_t is not specialized.
struct unread_t;

struct unread_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;

  template <typename P>
  using dialogue_options
      = dialogue::options<dialogue::identify_with<typename P::WHO_t>,
                          dialogue::default_timeout_ms<100>>;
};

// clang-format off
#define UNREAD_ROWS(X, X_AS) \
  X (WHO, 0x01, "Who", (), (), (u8_), ()) \
  X (READ, 0x10, "Read", (), (), (u16_le_), (idempotent))
// clang-format on

LUMEX_PROTOCOL_DEFINE (unread_t, codec_t, unread_header_t, UNREAD_ROWS);

/// A watchdog and a keepalive.
struct watched_t;

struct watched_header_t
{
  using lumex_vocabulary = dialogue::vocabulary_t;
  static LUMEX_CONSTEXPR std::size_t key_width = 1;

  template <typename P>
  using dialogue_options
      = dialogue::options<dialogue::identify_with<typename P::WHO_t>,
                          dialogue::default_timeout_ms<100>,
                          dialogue::keepalive<typename P::BEAT_t, 100>,
                          dialogue::watchdog_ms<300>,
                          dialogue::max_in_flight<2>>;
};

// clang-format off
#define WATCHED_ROWS(X, X_AS) \
  X (WHO, 0x01, "Who", (), (), (who_t), (idempotent, timeout (1000))) \
  X (BEAT, 0x02, "Beat", (), (), (u8_), ()) \
  X (READ, 0x10, "Read", (), (), (u16_le_), (idempotent, timeout (5000)))
// clang-format on

LUMEX_PROTOCOL_DEFINE (watched_t, codec_t, watched_header_t, WATCHED_ROWS);

/// A scenario over the plain table.
// clang-format off
LUMEX_PROTOCOL_SCENARIO (read_then_set_scenario, gauge_t,
                         (send<gauge_t::READ_t> ()),
                         (send<gauge_t::READ_t> ()));
LUMEX_PROTOCOL_SCENARIO (wait_forever_scenario, gauge_t,
                         (expect<gauge_t::FEED_t> ()));
// clang-format on
} // namespace reconnect_tables
} // namespace lumex_tests

namespace lumex
{
namespace applied
{
namespace transport
{
namespace protocol
{
template <> struct identity_of_t<lumex_tests::reconnect_tables::gauge_t>
{
  static std::error_code
  read (reply_of<lumex_tests::reconnect_tables::gauge_t::WHO_t> const &reply,
        identity_t &identity)
  {
    namespace fl = lumex::core::protocol::field;
    identity.model
        = std::string (fl::get<0> (reply).chars, fl::get<0> (reply).size ());
    identity.firmware = fl::get<1> (reply);
    int const layer = fl::get<2> (reply);
    identity.layer
        = layer == 1 ? "gauge_a_t" : (layer == 2 ? "gauge_b_t" : "");
    identity.serial_number = std::to_string (int (fl::get<3> (reply)));
    return std::error_code ();
  }
};

template <> struct identity_of_t<lumex_tests::reconnect_tables::single_t>
{
  static std::error_code
  read (reply_of<lumex_tests::reconnect_tables::single_t::WHO_t> const &reply,
        identity_t &identity)
  {
    namespace fl = lumex::core::protocol::field;
    identity.model
        = std::string (fl::get<0> (reply).chars, fl::get<0> (reply).size ());
    identity.firmware = fl::get<1> (reply);
    return std::error_code ();
  }
};

template <> struct identity_of_t<lumex_tests::reconnect_tables::watched_t>
{
  static std::error_code
  read (reply_of<lumex_tests::reconnect_tables::watched_t::WHO_t> const &reply,
        identity_t &identity)
  {
    namespace fl = lumex::core::protocol::field;
    identity.model
        = std::string (fl::get<0> (reply).chars, fl::get<0> (reply).size ());
    return std::error_code ();
  }
};
} // namespace protocol
} // namespace transport
} // namespace applied
} // namespace lumex

namespace lumex_client_test
{
namespace rt = lumex_tests::reconnect_tables;
namespace ss = tr::session;
using gauge = rt::gauge_t;
using gauge_a = rt::gauge_a_t;
using gauge_b = rt::gauge_b_t;

/// What the identity row of a gauge says.
template <typename Value = pr::reply_of<gauge::WHO_t>>
inline Value
gauge_who (char const *model, int firmware, int layer, int serial = 1)
{
  Value who;
  std::string const text (model);
  field::get<0> (who).assign (field::text_view (text.data (), text.size ()));
  field::get<1> (who) = static_cast<std::uint16_t> (firmware);
  field::get<2> (who) = static_cast<std::uint8_t> (layer);
  field::get<3> (who) = static_cast<std::uint8_t> (serial);
  return who;
}

/// A session that retries on the virtual clock without jitter (the cycle ends
/// after `max_attempts` failed attempts; 0: never).
inline ss::session_options_t
bench_session_options (tr::testing::virtual_clock &clock,
                       std::uint32_t max_attempts = 0)
{
  ss::session_options_t options;
  options.name = "bench";
  options.policy.initial_delay = ms (100);
  options.policy.max_delay = ms (1000);
  options.policy.max_attempts = max_attempts;
  options.policy.jitter = 0;
  options.open_timeout = ms (1000);
  options.timers = clock.source ();
  return options;
}

/// A device of layer `Device` and a client of layer `Client` over a stream
/// session; the test polls the context itself.
template <typename Device, typename Client = Device> class stream_bench_t
{
public:
  using client_type = pr::client<Client>;

  explicit stream_bench_t (bool open_first = true)
      : stream_bench_t (pr::client_options_for<Client> (), open_first)
  {
  }

  explicit stream_bench_t (pr::client_options_t options,
                           bool open_first = true,
                           std::uint32_t max_attempts = 0)
  {
    tr::testing::mock_options_t mock_options;
    mock_options.timers = clock.source ();
    mock.reset (new tr::testing::mock_stream (tr::testing::make_mock_stream (
        context.get_executor (), mock_options)));
    std::shared_ptr<tr::stream::stream_impl> const impl = mock->stream.impl ();
    session.reset (new ss::stream_session (
        context.get_executor (), [impl] (tr::executor_type const &)
          { return tr::stream::byte_stream (impl); },
        bench_session_options (clock, max_attempts)));
    device.reset (new tr::testing::device_simulator<Device> (
        mock->control, fr::slip::slip_codec (), clock));
    if (open_first)
      {
        session->start ();
        settle ();
      }
    options.dialogue.timers = clock.source ();
    client.reset (new client_type (
        pr::frame_link (tr::framed::framed_stream<fr::slip::slip_codec> (
            session->as_byte_stream (), fr::slip::slip_codec ())),
        options));
    settle ();
    // The link counts as established after the delay of the policy.
    advance (ms (200));
  }

  ~stream_bench_t ()
  {
    client.reset ();
    settle ();
    session.reset ();
    settle ();
    device.reset ();
    settle ();
    mock.reset ();
    settle ();
  }

  stream_bench_t (stream_bench_t const &) = delete;
  stream_bench_t &operator= (stream_bench_t const &) = delete;

  void
  settle ()
  {
    tr::testing::run_until (context, clock, [] () { return false; }, ms (0));
  }

  void
  advance (ms by)
  {
    tr::testing::run_until (context, clock, [] () { return false; }, by);
  }

  bool
  run_until (std::function<bool ()> const &done, ms budget = ms (60000))
  {
    return tr::testing::run_until (context, clock, done, budget);
  }

  /// Hangs up, and runs what is ready: the session starts again at once.
  void
  lose ()
  {
    mock->control.disconnect ();
    settle ();
  }

  /// Identifies, and waits for the end.
  std::shared_ptr<outcome_t<pr::identity_t>>
  identify ()
  {
    auto const out = std::make_shared<outcome_t<pr::identity_t>> ();
    record_t<pr::identity_t> done = { out };
    client->async_identify (done);
    EXPECT_TRUE (run_until ([&] () { return out->calls != 0; }));
    settle ();
    return out;
  }

  std::uint64_t
  requests () const
  {
    return device->statistics ().requests;
  }

  asio::io_context context;
  tr::testing::virtual_clock clock;
  std::unique_ptr<tr::testing::mock_stream> mock;
  std::unique_ptr<ss::stream_session> session;
  std::unique_ptr<tr::testing::device_simulator<Device>> device;
  std::unique_ptr<client_type> client;
};

/// The same over a message session, on report 1.
template <typename Device, typename Client = Device> class datagram_bench_t
{
public:
  using client_type = pr::client<Client>;

  explicit datagram_bench_t (std::uint32_t max_attempts = 0)
  {
    tr::testing::mock_options_t mock_options;
    mock_options.timers = clock.source ();
    mock.reset (
        new tr::testing::mock_datagram (tr::testing::make_mock_datagram (
            context.get_executor (), mock_options)));
    std::shared_ptr<tr::datagram::message_impl> const impl
        = mock->transport.impl ();
    session.reset (new ss::message_session (
        context.get_executor (), [impl] (tr::executor_type const &)
          { return tr::datagram::message_transport (impl); },
        bench_session_options (clock, max_attempts)));
    device.reset (new tr::testing::device_simulator<Device> (
        mock->control, fr::slip::slip_codec (), 1, clock));
    session->start ();
    settle ();
    pr::client_options_t options = pr::client_options_for<Client> ();
    options.dialogue.timers = clock.source ();
    client.reset (new client_type (
        pr::frame_link (
            tr::framed::framed_datagram<fr::slip::slip_codec> (
                session->as_message_transport (), fr::slip::slip_codec ()),
            1),
        options));
    settle ();
    advance (ms (200));
  }

  ~datagram_bench_t ()
  {
    client.reset ();
    settle ();
    session.reset ();
    settle ();
    device.reset ();
    settle ();
    mock.reset ();
    settle ();
  }

  datagram_bench_t (datagram_bench_t const &) = delete;
  datagram_bench_t &operator= (datagram_bench_t const &) = delete;

  void
  settle ()
  {
    tr::testing::run_until (context, clock, [] () { return false; }, ms (0));
  }

  void
  advance (ms by)
  {
    tr::testing::run_until (context, clock, [] () { return false; }, by);
  }

  bool
  run_until (std::function<bool ()> const &done, ms budget = ms (60000))
  {
    return tr::testing::run_until (context, clock, done, budget);
  }

  void
  lose ()
  {
    mock->control.disconnect ();
    settle ();
  }

  asio::io_context context;
  tr::testing::virtual_clock clock;
  std::unique_ptr<tr::testing::mock_datagram> mock;
  std::unique_ptr<ss::message_session> session;
  std::unique_ptr<tr::testing::device_simulator<Device>> device;
  std::unique_ptr<client_type> client;
};
} // namespace lumex_client_test

#endif // !LUMEX_TESTS_APPLIED_TRANSPORT_PROTOCOL_CLIENT_RECONNECT_SUPPORT_HPP
