// bench_transport_common.hpp
// What the transport benchmarks share: optimizer barriers, a timing loop that
// reports nanoseconds per operation (median and minimum of several passes),
// the process CPU time, the load average, the result rows and the CSV file.
//
// Every row of the CSV names the clock its benchmark ran on:
//   none     no timer and no wait took part (pure CPU work);
//   virtual  the protocol timers ran on testing::virtual_clock, so no wait
//            took time and the number is the CPU cost of the work;
//   real     the engine ran on real Asio timers and threads, so the number
//            includes what the operating system adds (a thread hand-off).
// The time that is measured is always the wall time of steady_clock (or the
// CPU time of the process where the unit says so).
#ifndef LUMEX_BENCHMARKS_TRANSPORT_COMMON_HPP
#define LUMEX_BENCHMARKS_TRANSPORT_COMMON_HPP

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
#define BENCH_NOINLINE __attribute__ ((noinline))
#define BENCH_ALWAYS_INLINE inline __attribute__ ((always_inline))
#elif defined(_MSC_VER)
#define BENCH_NOINLINE __declspec (noinline)
#define BENCH_ALWAYS_INLINE __forceinline
#else
#define BENCH_NOINLINE
#define BENCH_ALWAYS_INLINE inline
#endif

namespace bench
{
// -- Optimizer barriers (the DoNotOptimize of these benchmarks) --

#if defined(__GNUC__) || defined(__clang__)
/** The object is read from memory here, and memory may change after it. */
template <typename T>
BENCH_ALWAYS_INLINE void
keep_object (T const &object)
{
  asm volatile ("" : : "m"(object) : "memory");
}

/** The scalar is used here, in a register or in memory. */
template <typename T>
BENCH_ALWAYS_INLINE void
keep_value (T const &value)
{
  asm volatile ("" : : "r,m"(value) : "memory");
}

/** The compiler loses what it knows about the value. */
template <typename T>
BENCH_ALWAYS_INLINE T
hide (T value)
{
  asm volatile ("" : "+r"(value));
  return value;
}
#else
static std::uint64_t volatile g_barrier = 0;

template <typename T>
BENCH_ALWAYS_INLINE void
keep_object (T const &object)
{
  _ReadWriteBarrier ();
  g_barrier = g_barrier
              + static_cast<std::uint64_t> (
                  reinterpret_cast<std::uintptr_t> (&object));
}

template <typename T>
BENCH_ALWAYS_INLINE void
keep_value (T const &value)
{
  _ReadWriteBarrier ();
  g_barrier = g_barrier + static_cast<std::uint64_t> (value);
}

template <typename T>
BENCH_ALWAYS_INLINE T
hide (T value)
{
  T volatile copy = value;
  _ReadWriteBarrier ();
  return copy;
}
#endif

// -- Time --

using steady = std::chrono::steady_clock;

inline double
seconds_since (steady::time_point started)
{
  return std::chrono::duration<double> (steady::now () - started).count ();
}

/** The CPU time of the whole process in seconds. */
inline double
process_cpu_seconds ()
{
#if defined(CLOCK_PROCESS_CPUTIME_ID)
  timespec now;
  if (clock_gettime (CLOCK_PROCESS_CPUTIME_ID, &now) == 0)
    return static_cast<double> (now.tv_sec)
           + static_cast<double> (now.tv_nsec) * 1e-9;
#endif
  return static_cast<double> (std::clock ()) / CLOCKS_PER_SEC;
}

/** The one-minute load average of the machine, -1 where it is unknown. */
inline double
load_average ()
{
  std::ifstream in ("/proc/loadavg");
  double load = -1.0;
  if (in)
    in >> load;
  return load;
}

// -- Options of a run --

struct options_t
{
  bool quick = false;
  bool list = false;
  std::string filter;
  std::string output;
  int passes = 7;
  double pass_seconds = 0.05;
};

inline options_t
parse_arguments (int argc, char **argv)
{
  options_t options;
  for (int i = 1; i < argc; ++i)
    {
      std::string const argument = argv[i];
      if (argument == "--quick")
        options.quick = true;
      else if (argument == "--list")
        options.list = true;
      else if (argument == "--filter" && i + 1 < argc)
        options.filter = argv[++i];
      else if (argument.compare (0, 2, "--") != 0)
        options.output = argument;
      else
        {
          std::fprintf (stderr,
                        "usage: %s [output.csv] [--quick] [--filter text] "
                        "[--list]\n",
                        argv[0]);
          std::exit (2);
        }
    }
  if (options.quick)
    {
      options.passes = 3;
      options.pass_seconds = 0.005;
    }
  return options;
}

// -- Results --

struct result_t
{
  std::string benchmark;
  std::string name;
  std::string clock;
  std::string unit;
  double median = 0;
  double minimum = 0;
  int passes = 0;
  std::uint64_t iterations = 0;
  double load = -1;
  std::string note;
};

class reporter_t
{
public:
  explicit reporter_t (std::string benchmark, options_t const &options)
      : benchmark_ (std::move (benchmark)), options_ (options)
  {
  }

  /** Whether the case runs under --filter. */
  bool
  wanted (std::string const &name) const
  {
    if (options_.list)
      std::printf ("%s\n", name.c_str ());
    return !options_.list
           && (options_.filter.empty ()
               || name.find (options_.filter) != std::string::npos);
  }

  void
  add (std::string const &name, std::string const &clock,
       std::string const &unit, double median, double minimum, int passes,
       std::uint64_t iterations, std::string const &note = std::string ())
  {
    result_t row;
    row.benchmark = benchmark_;
    row.name = name;
    row.clock = clock;
    row.unit = unit;
    row.median = median;
    row.minimum = minimum;
    row.passes = passes;
    row.iterations = iterations;
    row.load = load_average ();
    // A comma would split the CSV field.
    row.note = note;
    std::replace (row.note.begin (), row.note.end (), ',', ';');
    rows_.push_back (row);
    std::printf ("%-44s %-8s %12.2f %-14s min %12.2f  load %.2f  %s\n",
                 name.c_str (), clock.c_str (), median, unit.c_str (), minimum,
                 row.load, note.c_str ());
    std::fflush (stdout);
  }

  std::vector<result_t> const &
  rows () const
  {
    return rows_;
  }

  /** Writes the CSV with the comment lines `meta` (each without '#'). */
  bool
  write (std::string const &path, std::vector<std::string> const &meta) const
  {
    if (path.empty ())
      return true;
    std::ofstream out (path.c_str ());
    if (!out)
      return false;
    for (std::size_t i = 0; i < meta.size (); ++i)
      out << "# " << meta[i] << "\n";
    out << "benchmark,case,clock,unit,median,min,passes,iterations,loadavg,"
           "note\n";
    for (std::size_t i = 0; i < rows_.size (); ++i)
      {
        result_t const &r = rows_[i];
        char numbers[160];
        std::snprintf (numbers, sizeof (numbers), "%.3f,%.3f,%d,%llu,%.2f",
                       r.median, r.minimum, r.passes,
                       static_cast<unsigned long long> (r.iterations), r.load);
        out << r.benchmark << ',' << r.name << ',' << r.clock << ',' << r.unit
            << ',' << numbers << ',' << r.note << "\n";
      }
    return static_cast<bool> (out);
  }

private:
  std::string benchmark_;
  options_t options_;
  std::vector<result_t> rows_;
};

/** The comment lines every CSV starts with. */
inline std::vector<std::string>
machine_comments (char const *benchmark)
{
  std::vector<std::string> lines;
  std::string model = "unknown";
  std::ifstream cpu ("/proc/cpuinfo");
  std::string line;
  while (std::getline (cpu, line))
    if (line.compare (0, 10, "model name") == 0)
      {
        std::size_t const colon = line.find (':');
        if (colon != std::string::npos && colon + 2 <= line.size ())
          model = line.substr (colon + 2);
        break;
      }
  std::string compiler;
#if defined(__clang__)
  compiler = std::string ("Clang ") + __clang_version__;
  compiler = compiler.substr (0, compiler.find (' ', 6));
#elif defined(__GNUC__)
  compiler = std::string ("GCC ") + __VERSION__;
  compiler = compiler.substr (0, compiler.find (' ', 4));
#else
  compiler = "unknown";
#endif
  lines.push_back (std::string ("benchmark=") + benchmark);
  lines.push_back ("machine=\"" + model + "\" logical_cpus="
                   + std::to_string (std::thread::hardware_concurrency ()));
  lines.push_back ("compiler=\"" + compiler + "\" cplusplus="
                   + std::to_string (static_cast<long> (__cplusplus)));
#if defined(NDEBUG)
  lines.push_back ("build=NDEBUG");
#else
  lines.push_back ("build=NOT-NDEBUG (the numbers mean nothing)");
#endif
  return lines;
}

// -- The timing loop --

struct timing_t
{
  double median = 0; // nanoseconds per operation
  double minimum = 0;
  std::uint64_t iterations = 0; // per pass
  int passes = 0;
};

/**
 * Times `body (n)` (it performs n operations): calibrates n so that a pass
 * lasts about `options.pass_seconds`, then runs `options.passes` passes.
 */
template <typename Body>
timing_t
measure (options_t const &options, Body &&body)
{
  std::uint64_t n = 1;
  for (;;)
    {
      steady::time_point const started = steady::now ();
      body (n);
      double const took = seconds_since (started);
      if (took >= options.pass_seconds || n >= (std::uint64_t (1) << 40))
        break;
      double const grow
          = took <= 0 ? 100.0 : options.pass_seconds * 1.3 / took;
      n = std::max<std::uint64_t> (
          n + 1, static_cast<std::uint64_t> (static_cast<double> (n)
                                             * std::min (grow, 100.0)));
    }
  std::vector<double> samples;
  for (int pass = 0; pass < options.passes; ++pass)
    {
      steady::time_point const started = steady::now ();
      body (n);
      samples.push_back (seconds_since (started) * 1e9
                         / static_cast<double> (n));
    }
  std::sort (samples.begin (), samples.end ());
  timing_t out;
  out.median = samples[samples.size () / 2];
  out.minimum = samples.front ();
  out.iterations = n;
  out.passes = options.passes;
  return out;
}

/** Fixed pseudo-random bytes (xorshift), the same on every run. */
inline std::vector<std::uint8_t>
pattern_bytes (std::size_t size, std::uint32_t seed = 0x2545F491u)
{
  std::vector<std::uint8_t> out (size);
  std::uint32_t state = seed;
  for (std::size_t i = 0; i < size; ++i)
    {
      state ^= state << 13;
      state ^= state >> 17;
      state ^= state << 5;
      out[i] = static_cast<std::uint8_t> (state >> 11);
    }
  return out;
}
} // namespace bench

#endif // !LUMEX_BENCHMARKS_TRANSPORT_COMMON_HPP
