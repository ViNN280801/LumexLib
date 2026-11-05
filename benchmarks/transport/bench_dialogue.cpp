// bench_dialogue.cpp
// The round trip of one exchange of the dialogue: `client<P>` calls a row,
// the engine frames and writes the request over a SLIP-framed mock stream,
// `device_simulator<P>` decodes it and answers, the engine reads, decodes and
// completes the call. No wait is real: the answer of the simulator is ready at
// once.
//
//   ping6_virtual / block2420_virtual   async call, one thread polling the
//        context; the engine and the simulator run on testing::virtual_clock,
//        so no timer ever fires and the number is the CPU cost of an exchange
//        (clock: virtual). The 2420-byte case is the reply of the large row.
//   ping6_real_timers   the same with the engine on real Asio timers (the
//        request timer is armed and cancelled on the steady clock; clock:
//        real), the simulator still on the virtual clock.
//   ping6_sync_thread / block2420_sync_thread   the synchronous call
//        `client.PING (...)` from the benchmark thread while another thread
//        runs the context (clock: real): the cost includes the hand-off
//        between the two threads.
//   ping6_sync_tcp_loopback   the same synchronous call over a real TCP
//        connection on the loopback interface (clock: real): the client runs
//        on an io_runner with real timers and sockets, and a peer thread of
//        the benchmark answers every request with the sample reply at once.
//
// Falsification: each case is also run with a sleep of 50 microseconds put
// into the handler of the simulator (into the peer thread for the TCP case;
// `*_device_sleep50us`); the benchmark stops (exit code 1) unless that
// variant is at least 40 microseconds slower than its base. Equal work: after
// each case the simulator's request count (the peer's for the TCP case) must
// equal the calls made, the client's statistics must count the same requests
// (the threaded cases too) and no call may end with an error. The loops keep
// the results with optimizer barriers.
//
// Usage: LumexTransportBenchDialogue [output.csv] [--quick] [--filter text]
//        [--list]
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "asio/buffer.hpp"
#include "asio/write.hpp"

#include "bench_transport_common.hpp"
#include "bench_transport_rig.hpp"
#include "bench_transport_tcp.hpp"

namespace
{
namespace bt = bench_transport;
using P = bt::protocol_type;

struct outcome_t
{
  bool done = false;
  std::error_code error;
};

// One async exchange of PING.
void
ping_async (bt::polled_rig_t &rig, outcome_t &out)
{
  out.done = false;
  rig.client ().PING (
      std::uint32_t (7), std::uint8_t (1),
      [&out] (std::error_code error, bt::pr::reply_of<P::PING_t> value)
        {
          bench::keep_object (value);
          out.error = error;
          out.done = true;
        });
  rig.run_until ([&out] () { return out.done; });
  if (out.error)
    bt::die ("an async PING failed");
}

void
block_async (bt::polled_rig_t &rig, outcome_t &out)
{
  out.done = false;
  rig.client ().BLOCK (
      [&out] (std::error_code error, bt::pr::reply_of<P::BLOCK_t> value)
        {
          bench::keep_object (value);
          out.error = error;
          out.done = true;
        });
  rig.run_until ([&out] () { return out.done; });
  if (out.error)
    bt::die ("an async BLOCK failed");
}

void
sleep_in_device (bt::device_type &device, bool block)
{
  if (block)
    device.on<P::BLOCK_t> (
        [] (bt::device_type::request_of<P::BLOCK_t> const &)
          { std::this_thread::sleep_for (std::chrono::microseconds (50)); });
  else
    device.on<P::PING_t> (
        [] (bt::device_type::request_of<P::PING_t> const &)
          { std::this_thread::sleep_for (std::chrono::microseconds (50)); });
}

struct rate_t
{
  bench::timing_t timing;
  std::uint64_t calls;
};

// Times `one (rig)` n times on a polled rig; checks the counts.
template <typename One>
rate_t
polled_case (bench::options_t const &options, bool real_timers, bool sleeping,
             bool block, One one)
{
  bt::polled_rig_t rig (real_timers);
  if (sleeping)
    sleep_in_device (rig.device (), block);
  outcome_t out;
  one (rig, out); // warm up and verify
  std::uint64_t calls = 1;
  bench::timing_t const timed
      = bench::measure (options,
                        [&] (std::uint64_t n)
                          {
                            for (std::uint64_t i = 0; i < n; ++i)
                              one (rig, out);
                            calls += n;
                          });
  rig.settle ();
  if (rig.device ().statistics ().requests != calls)
    bt::die ("the simulator saw another number of requests than were made");
  if (rig.client ().statistics ().requests != calls)
    bt::die ("the client counted another number of requests than were made");
  rate_t rate = { timed, calls };
  return rate;
}

template <typename Call>
rate_t
threaded_case (bench::options_t const &options, bool sleeping, bool block,
               Call call)
{
  bt::threaded_rig_t rig;
  if (sleeping)
    sleep_in_device (rig.device (), block);
  call (rig); // warm up and verify
  std::uint64_t calls = 1;
  bench::timing_t const timed
      = bench::measure (options,
                        [&] (std::uint64_t n)
                          {
                            for (std::uint64_t i = 0; i < n; ++i)
                              call (rig);
                            calls += n;
                          });
  if (rig.device ().statistics ().requests != calls)
    bt::die ("the simulator saw another number of requests than were made");
  if (rig.client ().statistics ().requests != calls)
    bt::die ("the client counted another number of requests than were made");
  rate_t rate = { timed, calls };
  return rate;
}

// A client over a real TCP connection on the loopback interface: the client
// runs on an io_runner (real timers, real sockets); a thread of the benchmark
// reads each request and answers it with the sample reply, after `delay`.
class tcp_rig_t
{
public:
  explicit tcp_rig_t (std::chrono::microseconds delay)
      : runner_ (1), delay_ (delay),
        frame_ (bt::sample_reply_frame<P::PING_t> ())
  {
    client_ = loopback_.connect (runner_, peer_, nullptr);
    answerer_ = std::thread ([this] () { answer (); });
  }

  ~tcp_rig_t ()
  {
    std::error_code error;
    peer_->shutdown (bt::tcp::socket::shutdown_both, error);
    answerer_.join ();
    bt::on_runner (runner_, [this] () { client_.reset (); });
  }

  tcp_rig_t (tcp_rig_t const &) = delete;
  tcp_rig_t &operator= (tcp_rig_t const &) = delete;

  bt::client_type &
  client ()
  {
    return *client_;
  }

  /// The requests the peer has read.
  std::uint64_t
  requests_seen () const
  {
    return seen_.load ();
  }

private:
  void
  answer ()
  {
    std::uint8_t buffer[256];
    std::error_code error;
    for (;;)
      {
        peer_->read_some (asio::buffer (buffer), error);
        if (error)
          return;
        seen_.fetch_add (1);
        if (delay_.count () > 0)
          std::this_thread::sleep_for (delay_);
        asio::write (*peer_, asio::buffer (frame_), error);
        if (error)
          return;
      }
  }

  bt::tr::stream::io_runner runner_;
  bt::loopback_t loopback_;
  std::unique_ptr<bt::tcp::socket> peer_;
  std::unique_ptr<bt::client_type> client_;
  std::chrono::microseconds delay_;
  std::vector<std::uint8_t> frame_;
  std::atomic<std::uint64_t> seen_{ 0 };
  std::thread answerer_;
};

rate_t
tcp_case (bench::options_t const &options, bool sleeping)
{
  tcp_rig_t rig (std::chrono::microseconds (sleeping ? 50 : 0));
  auto call = [&rig] ()
    {
      bt::pr::call_result_t<P::PING_t> const reply
          = rig.client ().PING (std::uint32_t (7), std::uint8_t (1));
      bench::keep_object (reply);
      if (!reply)
        bt::die ("a sync PING over TCP failed");
    };
  call (); // warm up and verify
  std::uint64_t calls = 1;
  bench::timing_t const timed
      = bench::measure (options,
                        [&] (std::uint64_t n)
                          {
                            for (std::uint64_t i = 0; i < n; ++i)
                              call ();
                            calls += n;
                          });
  if (rig.requests_seen () != calls)
    bt::die ("the peer saw another number of requests than were made");
  if (rig.client ().statistics ().requests != calls)
    bt::die ("the client counted another number of requests than were made");
  rate_t rate = { timed, calls };
  return rate;
}

void
ping_sync (bt::threaded_rig_t &rig)
{
  bt::pr::call_result_t<P::PING_t> const reply
      = rig.client ().PING (std::uint32_t (7), std::uint8_t (1));
  bench::keep_object (reply);
  if (!reply)
    bt::die ("a sync PING failed");
}

void
block_sync (bt::threaded_rig_t &rig)
{
  bt::pr::call_result_t<P::BLOCK_t> const reply = rig.client ().BLOCK ();
  bench::keep_object (reply);
  if (!reply)
    bt::die ("a sync BLOCK failed");
}

void
require_slower (char const *name, double base, double slowed)
{
  if (slowed < base + 40000.0)
    {
      std::fprintf (stderr,
                    "bench_dialogue: %s: a sleep of 50 us in the device moved "
                    "the round trip from %.0f ns to %.0f ns only: the "
                    "benchmark does not see the device\n",
                    name, base, slowed);
      std::exit (1);
    }
}
} // namespace

int
main (int argc, char **argv)
{
  bench::options_t const options = bench::parse_arguments (argc, argv);
  bench::reporter_t report ("dialogue", options);

  struct polled_t
  {
    char const *name;
    bool real_timers;
    bool block;
    char const *clock;
  };
  polled_t const polled[] = {
    { "ping6_virtual", false, false, "virtual" },
    { "block2420_virtual", false, true, "virtual" },
    { "ping6_real_timers", true, false, "real" },
  };
  for (polled_t const &c : polled)
    {
      std::string const name = c.name;
      std::string const slow = name + "_device_sleep50us";
      if (!report.wanted (name) && !report.wanted (slow))
        continue;
      auto one = [&c] (bt::polled_rig_t &rig, outcome_t &out)
        {
          if (c.block)
            block_async (rig, out);
          else
            ping_async (rig, out);
        };
      rate_t const base
          = polled_case (options, c.real_timers, false, c.block, one);
      report.add (name, c.clock, "ns/exchange", base.timing.median,
                  base.timing.minimum, base.timing.passes,
                  base.timing.iterations,
                  c.block ? "async, 2420-byte reply" : "async, 6-byte reply");
      rate_t const slowed
          = polled_case (options, c.real_timers, true, c.block, one);
      require_slower (c.name, base.timing.median, slowed.timing.median);
      report.add (slow, c.clock, "ns/exchange", slowed.timing.median,
                  slowed.timing.minimum, slowed.timing.passes,
                  slowed.timing.iterations,
                  "falsifier: sleep_for (50 us) in the simulator handler");
    }

  struct threaded_t
  {
    char const *name;
    bool block;
  };
  threaded_t const threaded[]
      = { { "ping6_sync_thread", false }, { "block2420_sync_thread", true } };
  for (threaded_t const &c : threaded)
    {
      std::string const name = c.name;
      std::string const slow = name + "_device_sleep50us";
      if (!report.wanted (name) && !report.wanted (slow))
        continue;
      auto call = [&c] (bt::threaded_rig_t &rig)
        {
          if (c.block)
            block_sync (rig);
          else
            ping_sync (rig);
        };
      rate_t const base = threaded_case (options, false, c.block, call);
      report.add (name, "real", "ns/exchange", base.timing.median,
                  base.timing.minimum, base.timing.passes,
                  base.timing.iterations,
                  "sync call, the context runs on another thread");
      rate_t const slowed = threaded_case (options, true, c.block, call);
      require_slower (c.name, base.timing.median, slowed.timing.median);
      report.add (slow, "real", "ns/exchange", slowed.timing.median,
                  slowed.timing.minimum, slowed.timing.passes,
                  slowed.timing.iterations,
                  "falsifier: sleep_for (50 us) in the simulator handler");
    }

  {
    std::string const name = "ping6_sync_tcp_loopback";
    std::string const slow = name + "_device_sleep50us";
    if (report.wanted (name) || report.wanted (slow))
      {
        rate_t const base = tcp_case (options, false);
        report.add (name, "real", "ns/exchange", base.timing.median,
                    base.timing.minimum, base.timing.passes,
                    base.timing.iterations,
                    "sync call over TCP on the loopback interface; the client "
                    "on an io_runner with real timers");
        rate_t const slowed = tcp_case (options, true);
        require_slower (name.c_str (), base.timing.median,
                        slowed.timing.median);
        report.add (slow, "real", "ns/exchange", slowed.timing.median,
                    slowed.timing.minimum, slowed.timing.passes,
                    slowed.timing.iterations,
                    "falsifier: sleep_for (50 us) in the peer thread");
      }
  }

  if (!report.write (options.output, bench::machine_comments ("dialogue")))
    bt::die ("cannot write the output");
  return 0;
}
