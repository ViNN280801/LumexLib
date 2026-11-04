// bench_logging.cpp
// The cost of a log call of the transport below the threshold: the macro
// LUMEX_TRANSPORT_LOG with a message that is built from a number (a string
// concatenation, as the library writes its lines), measured against
//
//   - eager_below_threshold: a call that builds the message BEFORE it asks
//     whether the record is wanted (the falsification of this benchmark);
//   - bare_atomic_load: an atomic load and a branch, the floor;
//   - enabled_counting_sink: the same call with the record written to a sink
//     that only counts (the cost the fast path saves);
//   - filtered_component: the level passes but the component is denied.
//
// Clock: none. No timer, thread or transport takes part; steady_clock times
// the CPU work.
//
// Falsification: before the measurement, the lazy macro and the eager call
// run 200000 times on a threshold above the record. The benchmark stops
// (exit code 1) unless the eager call built the message every time, the lazy
// one never, and the eager one is at least 5 times slower. The built messages
// are counted in a global the optimizer cannot remove (a volatile counter),
// and the loops keep their inputs with the barriers of
// bench_transport_common.hpp.
//
// Usage: LumexTransportBenchLogging [output.csv] [--quick] [--filter text]
//        [--list]
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "bench_transport_common.hpp"
#include "lumex/applied/transport/log/LumexTransportLog.hpp"

namespace
{
namespace lg = lumex::applied::transport::log;

std::uint64_t volatile g_built = 0;
std::uint64_t volatile g_written = 0;
std::atomic<int> g_level_probe (3);

// The message of a log line: what the library builds with a concatenation.
BENCH_NOINLINE std::string
make_text (std::uint64_t i)
{
  g_built = g_built + 1;
  return "frame " + std::to_string (i) + " len=" + std::to_string (i & 255)
         + " accepted";
}

void
counting_sink (lg::log_record_t const &record)
{
  g_written = g_written + 1;
  bench::keep_object (record);
}

[[noreturn]] void
fail (std::string const &why)
{
  std::fprintf (stderr, "bench_logging: %s\n", why.c_str ());
  std::exit (1);
}

// The macro as the library writes it.
BENCH_NOINLINE void
lazy_loop (std::uint64_t n)
{
  for (std::uint64_t i = 0; i < n; ++i)
    {
      bench::keep_value (i);
      LUMEX_TRANSPORT_LOG (debug, "transport.bench", "instance", make_text (i),
                           lg::log_bytes_t ());
    }
}

// The falsifier: the message is built first.
BENCH_NOINLINE void
eager_loop (std::uint64_t n)
{
  for (std::uint64_t i = 0; i < n; ++i)
    {
      bench::keep_value (i);
      std::string message = make_text (i);
      bench::keep_object (message);
      if (lg::log_enabled (lg::log_level::debug, "transport.bench"))
        lg::log_write (lg::log_level::debug, "transport.bench", "instance",
                       message);
    }
}

BENCH_NOINLINE void
floor_loop (std::uint64_t n)
{
  std::uint64_t taken = 0;
  for (std::uint64_t i = 0; i < n; ++i)
    {
      bench::keep_value (i);
      if (g_level_probe.load (std::memory_order_relaxed) <= 0)
        ++taken;
    }
  bench::keep_value (taken);
}

double
time_loop (void (*loop) (std::uint64_t), std::uint64_t n)
{
  bench::steady::time_point const started = bench::steady::now ();
  loop (n);
  return bench::seconds_since (started) * 1e9 / static_cast<double> (n);
}

void
falsify ()
{
  lg::set_log_sink (&counting_sink);
  lg::set_log_level (lg::log_level::error);
  std::uint64_t const n = 200000;
  g_built = 0;
  double const lazy = time_loop (&lazy_loop, n);
  std::uint64_t const lazy_built = g_built;
  g_built = 0;
  double const eager = time_loop (&eager_loop, n);
  std::uint64_t const eager_built = g_built;
  std::printf (
      "falsification: lazy %.2f ns (built %llu), eager %.2f ns (built "
      "%llu), ratio %.1f\n",
      lazy, static_cast<unsigned long long> (lazy_built), eager,
      static_cast<unsigned long long> (eager_built), eager / lazy);
  if (lazy_built != 0)
    fail ("the lazy call built a message below the threshold");
  if (eager_built != n)
    fail ("the eager call did not build the message every time");
  if (eager < lazy * 5.0)
    fail ("the benchmark does not tell a formatting call from the fast path");
}
} // namespace

int
main (int argc, char **argv)
{
  bench::options_t const options = bench::parse_arguments (argc, argv);
  bench::reporter_t report ("logging", options);
  if (!options.list)
    falsify ();

  lg::set_log_sink (&counting_sink);
  lg::set_component_filter (std::vector<std::string> (),
                            std::vector<std::string> ());

  struct row_t
  {
    char const *name;
    void (*loop) (std::uint64_t);
    lg::log_level level;
    bool deny_component;
    char const *clock;
    bool built_expected;
    char const *note;
  };
  row_t const rows[] = {
    { "lazy_below_threshold", &lazy_loop, lg::log_level::error, false, "none",
      false, "the macro, threshold above the record" },
    { "eager_below_threshold", &eager_loop, lg::log_level::error, false,
      "none", true, "falsifier: formats before the check" },
    { "bare_atomic_load", &floor_loop, lg::log_level::error, false, "none",
      false, "an atomic load and a branch" },
    { "filtered_component", &lazy_loop, lg::log_level::trace, true, "none",
      false, "level passes, component denied" },
    { "enabled_counting_sink", &lazy_loop, lg::log_level::trace, false, "none",
      true, "the record is built and handed to a counting sink" },
  };
  for (row_t const &row : rows)
    {
      if (!report.wanted (row.name))
        continue;
      lg::set_log_level (row.level);
      if (row.deny_component)
        lg::set_component_filter (
            std::vector<std::string> (),
            std::vector<std::string> (1, "transport.bench"));
      else
        lg::set_component_filter (std::vector<std::string> (),
                                  std::vector<std::string> ());
      g_built = 0;
      g_written = 0;
      bench::timing_t const timed
          = bench::measure (options, [&] (std::uint64_t n) { row.loop (n); });
      bool const built = g_built != 0;
      if (built != row.built_expected && row.loop != &floor_loop)
        fail (std::string (row.name) + ": messages built = "
              + std::to_string (g_built) + " contradicts the case");
      std::string note = row.note;
      if (row.level == lg::log_level::trace && !row.deny_component)
        {
          if (g_written == 0)
            fail (std::string (row.name) + ": the sink saw nothing");
          note += "; records=" + std::to_string (g_written);
        }
      report.add (row.name, row.clock, "ns/call", timed.median, timed.minimum,
                  timed.passes, timed.iterations, note);
    }

  lg::set_component_filter (std::vector<std::string> (),
                            std::vector<std::string> ());
  lg::set_log_sink (nullptr);
  lg::set_log_level (lg::log_level::warning);
  if (!report.write (options.output, bench::machine_comments ("logging")))
    fail ("cannot write " + options.output);
  return 0;
}
