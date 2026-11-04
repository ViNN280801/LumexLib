// bench_devices.cpp
// Many devices on one io_runner thread: 16 simulated devices, each behind a
// SLIP-framed mock stream with a `client<P>` that subscribes to the row the
// device streams, all on ONE `io_runner` thread. The loads (the table of
// bench_transport_tables.hpp):
//
//   sized_10   a 2420-byte message at 10 Hz per device (the subscription rate
//              of a spectrum, assumed 10 Hz);
//   sized_25   a 2420-byte message at 25 Hz per device;
//   small_20   a 16-byte message at 20 Hz per device;
//   small_25   a 16-byte message at 25 Hz per device;
//   mixed      6 devices of sized_10, 5 of small_20, 5 of small_25.
//
// Clock: virtual. The simulators, the mocks and the engines run on one
// testing::virtual_clock, which a task on the runner moves in steps of 10 ms
// as fast as the thread can run, so nothing waits and the runner thread is
// busy all the time. What is measured is the wall time that thread needs per
// second of virtual time, as a percentage (`busy_%_of_a_core`): the load the
// same work would put on one core in real time, without the idle wake-ups a
// real timer adds. The CPU time of the process, which the kernel accounts in
// ticks, is in the note as a cross-check. Each load is run
// twice: `*_with_clients` (device, mock, framing, engine, decode, handler) and
// `*_devices_only` (the same devices with a reader that discards the bytes),
// so the cost of the client side is the difference.
//
// Equal work: the handlers count the messages they received; the run stops
// (exit code 1) unless at least 97 % of the messages the rates promise were
// delivered to the handlers, and `*_devices_only` unless the discarding reader
// saw the bytes the devices sent. The numbers of messages are in the note.
//
// Usage: LumexTransportBenchDevices [output.csv] [--quick] [--filter text]
//        [--list]
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <future>
#include <memory>
#include <string>
#include <vector>

#include "bench_transport_common.hpp"
#include "bench_transport_rig.hpp"

namespace
{
namespace bt = bench_transport;
using P = bt::protocol_type;

enum class load_t
{
  sized_10,
  sized_25,
  small_20,
  small_25
};

struct load_info_t
{
  load_t load;
  char const *label;
  int hertz;
};

load_info_t const kLoads[] = {
  { load_t::sized_10, "sized_10", 10 },
  { load_t::sized_25, "sized_25", 25 },
  { load_t::small_20, "small_20", 20 },
  { load_t::small_25, "small_25", 25 },
};

struct scenario_t
{
  char const *name;
  // How many of the 16 devices run each load, in the order of kLoads.
  int counts[4];
};

scenario_t const kScenarios[] = {
  { "sized_10", { 16, 0, 0, 0 } }, { "sized_25", { 0, 16, 0, 0 } },
  { "small_20", { 0, 0, 16, 0 } }, { "small_25", { 0, 0, 0, 16 } },
  { "mixed", { 6, 0, 5, 5 } },
};

// Reads the stream and drops the bytes (the devices-only baseline).
struct raw_reader_t : std::enable_shared_from_this<raw_reader_t>
{
  explicit raw_reader_t (std::shared_ptr<bt::tr::stream::byte_stream> s)
      : stream (std::move (s))
  {
  }

  void
  start ()
  {
    std::shared_ptr<raw_reader_t> const self = shared_from_this ();
    stream->async_read_some (bt::tr::mutable_bytes_t (chunk, sizeof (chunk)),
                             [self] (std::error_code error, std::size_t count)
                               {
                                 if (error)
                                   return;
                                 self->bytes += count;
                                 self->start ();
                               });
  }

  std::shared_ptr<bt::tr::stream::byte_stream> stream;
  std::atomic<std::uint64_t> bytes{ 0 };
  std::uint8_t chunk[4096];
};

// A fleet of devices (and clients) on one runner.
class fleet_t
{
public:
  fleet_t () : runner_ (1) {}

  ~fleet_t () { teardown (); }

  fleet_t (fleet_t const &) = delete;
  fleet_t &operator= (fleet_t const &) = delete;

  /// Builds the devices of `scenario` on the runner thread.
  void
  build (scenario_t const &scenario, bool with_clients)
  {
    on_runner (
        [&] ()
          {
            for (int k = 0; k < 4; ++k)
              for (int i = 0; i < scenario.counts[k]; ++i)
                add (kLoads[k].load, with_clients);
          });
    // Let the opens and the first reads settle (nothing streams yet).
    run_steps (10);
  }

  /// Starts every stream and moves the clock by `steps` of 10 ms.
  void
  run_steps (int steps)
  {
    std::promise<void> done;
    std::shared_ptr<step_t> step (new step_t{ this, steps, &done });
    asio::post (runner_.context (), [step] () { (*step) (); });
    done.get_future ().wait ();
  }

  void
  start_streams ()
  {
    on_runner (
        [this] ()
          {
            for (std::size_t i = 0; i < links_.size (); ++i)
              {
                bool started = false;
                switch (loads_[i])
                  {
                  case load_t::sized_10:
                    started = links_[i].device->start_stream<P::SIZED_10_t> ();
                    break;
                  case load_t::sized_25:
                    started = links_[i].device->start_stream<P::SIZED_25_t> ();
                    break;
                  case load_t::small_20:
                    started = links_[i].device->start_stream<P::SMALL_20_t> ();
                    break;
                  case load_t::small_25:
                    started = links_[i].device->start_stream<P::SMALL_25_t> ();
                    break;
                  default:
                    break;
                  }
                if (!started)
                  bt::die ("a stream did not start");
              }
          });
  }

  std::uint64_t
  received () const
  {
    return received_.load ();
  }

  std::uint64_t
  raw_bytes () const
  {
    std::uint64_t total = 0;
    for (std::size_t i = 0; i < readers_.size (); ++i)
      total += readers_[i]->bytes.load ();
    return total;
  }

  void
  teardown ()
  {
    if (links_.empty ())
      return;
    on_runner (
        [this] ()
          {
            for (std::size_t i = 0; i < links_.size (); ++i)
              links_[i].device->stop_streams ();
            for (std::size_t i = 0; i < links_.size (); ++i)
              links_[i].client.reset ();
            readers_.clear ();
          });
    run_steps (5);
    on_runner ([this] () { links_.clear (); });
    run_steps (5);
  }

private:
  struct step_t
  {
    fleet_t *fleet;
    int left;
    std::promise<void> *done;

    void
    operator() ()
    {
      if (left <= 0)
        {
          done->set_value ();
          return;
        }
      fleet->clock_.advance (bt::ms (10));
      --left;
      std::shared_ptr<step_t> again (new step_t (*this));
      asio::post (fleet->runner_.context (), [again] () { (*again) (); });
    }
  };

  template <typename Body>
  void
  on_runner (Body body)
  {
    std::promise<void> done;
    asio::post (runner_.context (),
                [&] ()
                  {
                    body ();
                    done.set_value ();
                  });
    done.get_future ().wait ();
  }

  void
  add (load_t load, bool with_clients)
  {
    bt::link_t link
        = bt::make_link (runner_.get_executor (), clock_, true, with_clients);
    if (with_clients)
      {
        std::atomic<std::uint64_t> *counter = &received_;
        switch (load)
          {
          case load_t::sized_10:
            link.client->on<P::SIZED_10_t> (
                [counter] (bt::pr::reply_of<P::SIZED_10_t> const &value)
                  {
                    bench::keep_object (value);
                    counter->fetch_add (1, std::memory_order_relaxed);
                  });
            break;
          case load_t::sized_25:
            link.client->on<P::SIZED_25_t> (
                [counter] (bt::pr::reply_of<P::SIZED_25_t> const &value)
                  {
                    bench::keep_object (value);
                    counter->fetch_add (1, std::memory_order_relaxed);
                  });
            break;
          case load_t::small_20:
            link.client->on<P::SMALL_20_t> (
                [counter] (bt::pr::reply_of<P::SMALL_20_t> const &value)
                  {
                    bench::keep_object (value);
                    counter->fetch_add (1, std::memory_order_relaxed);
                  });
            break;
          case load_t::small_25:
            link.client->on<P::SMALL_25_t> (
                [counter] (bt::pr::reply_of<P::SMALL_25_t> const &value)
                  {
                    bench::keep_object (value);
                    counter->fetch_add (1, std::memory_order_relaxed);
                  });
            break;
          default:
            break;
          }
      }
    else
      {
        std::shared_ptr<raw_reader_t> reader (new raw_reader_t (link.raw));
        reader->start ();
        readers_.push_back (reader);
      }
    loads_.push_back (load);
    links_.push_back (std::move (link));
  }

  bt::tr::stream::io_runner runner_;
  bt::tr::testing::virtual_clock clock_;
  std::vector<bt::link_t> links_;
  std::vector<load_t> loads_;
  std::vector<std::shared_ptr<raw_reader_t>> readers_;
  std::atomic<std::uint64_t> received_{ 0 };
};

double
promised_messages (scenario_t const &scenario, double seconds)
{
  double total = 0;
  for (int k = 0; k < 4; ++k)
    total += scenario.counts[k] * kLoads[k].hertz * seconds;
  return total;
}
} // namespace

int
main (int argc, char **argv)
{
  bench::options_t const options = bench::parse_arguments (argc, argv);
  bench::reporter_t report ("devices", options);
  // Virtual seconds per pass.
  int const seconds = options.quick ? 2 : 10;

  for (scenario_t const &scenario : kScenarios)
    for (int with = 1; with >= 0; --with)
      {
        bool const with_clients = with == 1;
        std::string const name
            = std::string (scenario.name)
              + (with_clients ? "_with_clients" : "_devices_only");
        if (!report.wanted (name))
          continue;
        fleet_t fleet;
        fleet.build (scenario, with_clients);
        fleet.start_streams ();
        fleet.run_steps (100); // one virtual second of warm-up
        std::vector<double> percent;
        std::vector<double> cpu_percent;
        std::vector<double> speed;
        std::uint64_t delivered = 0;
        for (int pass = 0; pass < options.passes; ++pass)
          {
            std::uint64_t const before
                = with_clients ? fleet.received () : fleet.raw_bytes ();
            bench::steady::time_point const started = bench::steady::now ();
            double const cpu_before = bench::process_cpu_seconds ();
            fleet.run_steps (seconds * 100);
            double const cpu = bench::process_cpu_seconds () - cpu_before;
            double const wall = bench::seconds_since (started);
            std::uint64_t const got
                = (with_clients ? fleet.received () : fleet.raw_bytes ())
                  - before;
            if (with_clients)
              {
                double const promised = promised_messages (scenario, seconds);
                if (static_cast<double> (got) < promised * 0.97)
                  {
                    std::fprintf (stderr,
                                  "bench_devices: %s: %llu of %.0f messages "
                                  "delivered\n",
                                  name.c_str (),
                                  static_cast<unsigned long long> (got),
                                  promised);
                    return 1;
                  }
              }
            else if (got == 0)
              bt::die ("the discarding reader saw nothing");
            delivered = got;
            percent.push_back (wall / seconds * 100.0);
            cpu_percent.push_back (cpu / seconds * 100.0);
            speed.push_back (seconds / wall);
          }
        std::sort (percent.begin (), percent.end ());
        std::sort (speed.begin (), speed.end ());
        std::sort (cpu_percent.begin (), cpu_percent.end ());
        std::string note
            = std::string (with_clients ? "messages=" : "bytes=")
              + std::to_string (delivered) + " per " + std::to_string (seconds)
              + " virtual s; " + "x"
              + std::to_string (static_cast<long> (speed[speed.size () / 2]))
              + " faster than real time; cpu "
              + std::to_string (cpu_percent[cpu_percent.size () / 2]) + " %";
        report.add (name, "virtual", "busy_%_of_a_core",
                    percent[percent.size () / 2], percent.front (),
                    options.passes, static_cast<std::uint64_t> (seconds),
                    note);
        fleet.teardown ();
      }

  if (!report.write (options.output, bench::machine_comments ("devices")))
    bt::die ("cannot write the output");
  return 0;
}
