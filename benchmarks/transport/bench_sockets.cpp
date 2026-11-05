// bench_sockets.cpp
// 16 devices on one io_runner thread over REAL sockets and REAL timers: 16
// `client<P>` on one `io_runner` (one thread), each connected over TCP on the
// loopback interface to a peer socket of the benchmark, which writes the
// frames of a device that streams. This is the benchmark whose figure the
// budget "16 devices under 10 % of a core" is judged on; the figures of
// bench_devices.cpp (mock streams on the virtual clock) are only the CPU of
// the work and carry no verdict.
//
// What is measured: the CPU time of the runner thread (the thread that runs
// every socket read, every framing step, every decode and every handler)
// per second of wall time, from CLOCK_THREAD_CPUTIME_ID, read on that thread
// at the start and at the end of a pass. Clock: real. The peer sits on its own
// thread and its time is not counted; it shares the machine with the runner.
//
// Loads (the table of bench_transport_tables.hpp), 16 devices each:
//
//   sized_25_whole   a 2420-byte message at 25 Hz, one write per message;
//   sized_10_whole   a 2420-byte message at 10 Hz (an assumed rate);
//   small_25_whole   a 16-byte message at 25 Hz;
//   small_20_whole   a 16-byte message at 20 Hz;
//   sized_25_chunks64, sized_10_chunks64
//                    the same 2420-byte messages written in chunks of 64
//                    bytes (a USB serial read), the chunks of all devices
//                    sent on a shared tick of 1 ms. These rows carry no
//                    budget: at 25 Hz the stream is saturated (a message
//                    takes 38 ticks, more than its 40 ms period), so the
//                    peer delivers fewer messages than the rate asks.
//
// Equal work: the handlers count the messages they received; a pass stops the
// run (exit code 1) unless a whole-frame load delivered at least 97 % of the
// messages its rate promises (a chunked load at least 80 %: the share it
// delivered is in the note). The note also has the messages per second, the
// CPU time per message and the voluntary context switches of the runner
// thread per second.
//
// Left out: no serial driver or hardware, no requests, keepalive or watchdog
// beside the streams, the handler only counts, the chunks of all devices are
// batched on one tick (so the chunked figures are likely a lower bound), CPU
// is measured and not the wake-up latency, and the peer shares the machine.
//
// Usage: LumexTransportBenchSockets [output.csv] [--quick] [--filter text]
//        [--list]   (Linux only)
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <sys/resource.h>

#include "asio/buffer.hpp"
#include "asio/write.hpp"

#include "bench_transport_common.hpp"
#include "bench_transport_tcp.hpp"

namespace
{
namespace bt = bench_transport;
using P = bt::protocol_type;

int const kDevices = 16;

struct thread_cpu_t
{
  double seconds = 0;
  long voluntary_switches = 0;
};

// The CPU time of the calling thread.
thread_cpu_t
thread_cpu ()
{
  thread_cpu_t now;
  timespec time;
  if (clock_gettime (CLOCK_THREAD_CPUTIME_ID, &time) != 0)
    bt::die ("CLOCK_THREAD_CPUTIME_ID is not available");
  now.seconds = static_cast<double> (time.tv_sec)
                + static_cast<double> (time.tv_nsec) * 1e-9;
  rusage usage;
  if (getrusage (RUSAGE_THREAD, &usage) == 0)
    now.voluntary_switches = usage.ru_nvcsw;
  return now;
}

// The device side: writes `frame` to every socket `hertz` times a second,
// the devices staggered over one period. `chunk` 0 writes a whole frame per
// write; otherwise `chunk` bytes per socket on every tick of 1 ms.
class pacer_t
{
public:
  pacer_t (std::vector<std::unique_ptr<bt::tcp::socket>> &sockets,
           std::vector<std::uint8_t> const &frame, int hertz,
           std::size_t chunk)
      : sockets_ (sockets), frame_ (frame), hertz_ (hertz), chunk_ (chunk)
  {
    thread_ = std::thread ([this] () { run (); });
  }

  ~pacer_t () { stop (); }

  pacer_t (pacer_t const &) = delete;
  pacer_t &operator= (pacer_t const &) = delete;

  void
  stop ()
  {
    stop_ = true;
    if (thread_.joinable ())
      thread_.join ();
  }

  bool
  failed () const
  {
    return failed_;
  }

private:
  void
  run ()
  {
    std::size_t const devices = sockets_.size ();
    std::chrono::microseconds const period (1000000 / hertz_);
    bench::steady::time_point const origin = bench::steady::now ();
    std::vector<std::size_t> at (devices, frame_.size ());
    std::vector<bench::steady::time_point> next (devices);
    for (std::size_t i = 0; i < devices; ++i)
      next[i] = origin
                + period * static_cast<int> (i) / static_cast<int> (devices);
    while (!stop_)
      {
        bench::steady::time_point const now = bench::steady::now ();
        for (std::size_t i = 0; i < devices; ++i)
          {
            if (at[i] >= frame_.size () && now >= next[i])
              {
                at[i] = 0;
                next[i] += period;
              }
            if (at[i] >= frame_.size ())
              continue;
            std::size_t const size
                = chunk_ == 0 ? frame_.size () - at[i]
                              : std::min (chunk_, frame_.size () - at[i]);
            std::error_code error;
            asio::write (*sockets_[i],
                         asio::buffer (frame_.data () + at[i], size), error);
            if (error)
              {
                failed_ = true;
                return;
              }
            at[i] += size;
          }
        std::this_thread::sleep_for (
            std::chrono::microseconds (chunk_ == 0 ? 200 : 1000));
      }
  }

  std::vector<std::unique_ptr<bt::tcp::socket>> &sockets_;
  std::vector<std::uint8_t> const &frame_;
  int hertz_;
  std::size_t chunk_;
  std::atomic<bool> stop_{ false };
  std::atomic<bool> failed_{ false };
  std::thread thread_;
};

// One load: 16 clients of row `Row` on one runner, `passes` passes of
// `seconds` of wall time.
template <typename Row>
void
run_load (bench::reporter_t &report, bench::options_t const &options,
          std::string const &name, int hertz, std::size_t chunk)
{
  if (!report.wanted (name))
    return;
  int const seconds = options.quick ? 2 : 10;
  int const passes = options.quick ? 1 : 3;
  double const required = chunk == 0 ? 0.97 : 0.80;

  bt::tr::stream::io_runner runner (1);
  bt::loopback_t loopback;
  std::atomic<std::uint64_t> received{ 0 };
  std::vector<std::unique_ptr<bt::client_type>> clients;
  std::vector<std::unique_ptr<bt::tcp::socket>> sockets;
  for (int i = 0; i < kDevices; ++i)
    {
      std::unique_ptr<bt::tcp::socket> peer;
      clients.push_back (loopback.connect (
          runner, peer,
          [&received] (bt::client_type &client)
            {
              client.on<Row> (
                  [&received] (bt::pr::reply_of<Row> const &value)
                    {
                      bench::keep_object (value);
                      received.fetch_add (1, std::memory_order_relaxed);
                    });
            }));
      sockets.push_back (std::move (peer));
    }

  std::vector<std::uint8_t> const frame = bt::sample_reply_frame<Row> ();
  std::vector<double> percent;
  std::vector<double> messages_per_second;
  std::vector<double> microseconds_per_message;
  std::vector<double> switches_per_second;
  {
    pacer_t pacer (sockets, frame, hertz, chunk);
    // One second of warm-up, not measured.
    std::this_thread::sleep_for (
        std::chrono::milliseconds (options.quick ? 300 : 1000));
    for (int pass = 0; pass < passes; ++pass)
      {
        thread_cpu_t before;
        thread_cpu_t after;
        std::uint64_t first = 0;
        bt::on_runner (runner,
                       [&] ()
                         {
                           before = thread_cpu ();
                           first = received.load ();
                         });
        bench::steady::time_point const started = bench::steady::now ();
        std::this_thread::sleep_for (std::chrono::seconds (seconds));
        bt::on_runner (runner, [&] () { after = thread_cpu (); });
        double const wall = bench::seconds_since (started);
        std::uint64_t const got = received.load () - first;
        double const promised = kDevices * hertz * wall;
        if (pacer.failed ())
          bt::die ("a peer could not write to its client");
        if (static_cast<double> (got) < promised * required)
          {
            std::fprintf (stderr,
                          "bench_sockets: %s: %llu of %.0f messages "
                          "delivered\n",
                          name.c_str (), static_cast<unsigned long long> (got),
                          promised);
            std::exit (1);
          }
        double const cpu = after.seconds - before.seconds;
        percent.push_back (cpu / wall * 100.0);
        messages_per_second.push_back (static_cast<double> (got) / wall);
        microseconds_per_message.push_back (cpu * 1e6
                                            / static_cast<double> (got));
        switches_per_second.push_back (
            static_cast<double> (after.voluntary_switches
                                 - before.voluntary_switches)
            / wall);
      }
    pacer.stop ();
  }
  bt::on_runner (runner, [&] () { clients.clear (); });

  // The pass whose CPU share is the median gives the figures of the note.
  std::vector<std::size_t> order (percent.size ());
  for (std::size_t i = 0; i < order.size (); ++i)
    order[i] = i;
  std::sort (order.begin (), order.end (),
             [&percent] (std::size_t a, std::size_t b)
               { return percent[a] < percent[b]; });
  std::size_t const at = order[order.size () / 2];
  double const promised_rate = kDevices * hertz;
  char text[240];
  std::snprintf (text, sizeof (text),
                 "%d clients on one io_runner thread over TCP loopback; "
                 "%.1f of %.0f messages/s (%.0f %%); %.1f us of CPU per "
                 "message; %.0f voluntary context switches/s",
                 kDevices, messages_per_second[at], promised_rate,
                 messages_per_second[at] / promised_rate * 100.0,
                 microseconds_per_message[at], switches_per_second[at]);
  report.add (name, "real", "busy_%_of_a_core", percent[at],
              percent[order.front ()], passes,
              static_cast<std::uint64_t> (seconds), text);
}
} // namespace

int
main (int argc, char **argv)
{
  bench::options_t const options = bench::parse_arguments (argc, argv);
  bench::reporter_t report ("sockets", options);

  run_load<P::SIZED_25_t> (report, options, "sized_25_whole", 25, 0);
  run_load<P::SIZED_10_t> (report, options, "sized_10_whole", 10, 0);
  run_load<P::SMALL_25_t> (report, options, "small_25_whole", 25, 0);
  run_load<P::SMALL_20_t> (report, options, "small_20_whole", 20, 0);
  run_load<P::SIZED_25_t> (report, options, "sized_25_chunks64", 25, 64);
  run_load<P::SIZED_10_t> (report, options, "sized_10_chunks64", 10, 64);

  if (!report.write (options.output, bench::machine_comments ("sockets")))
    bt::die ("cannot write the output");
  return 0;
}
