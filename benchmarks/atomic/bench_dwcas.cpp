// bench_dwcas.cpp
// The cost of the 128-bit compare-and-swap of lumex/core/atomic/dwcas against
// a 64-bit compare-and-swap on one shared word, with the same threads, one
// after the other in the same process. A thread repeats "read a guess, try to
// swap in the guess plus one" for the length of the window and counts every
// attempt (a failed attempt is a locked instruction too); the result is the
// wall time per attempt and thread, as the other benchmarks of this directory
// measure, and the ratio of the 128-bit figure to the 64-bit one.
//
//   LumexDwcasBenchmark [--threads 1,2,4,8] [--duration-ms 100]
//                       [--repetitions 5]
//
// The ratios, not the nanoseconds, are the result: a 64-bit and a 128-bit
// attempt contend on the same cache line, so both grow with the thread count
// and the ratio shows what the second half costs. Medians over the
// repetitions; the two measurements of one repetition run in alternating
// order.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "lumex/core/atomic/dwcas/LumexDwcasWord.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"

#if LUMEX_ATOMIC_HAS_DWCAS

namespace
{
using lumex::core::atomic::dwcas::dwcas_value_t;
using lumex::core::atomic::dwcas::dwcas_word;

typedef std::chrono::steady_clock clock_type;

struct options_t
{
  std::vector<int> threads;
  int duration_ms;
  int repetitions;
};

class start_gate_t
{
public:
  explicit start_gate_t (int parties) : parties_ (parties), arrived_ (0) {}

  void
  arrive_and_wait ()
  {
    arrived_.fetch_add (1);
    while (arrived_.load () < parties_)
      std::this_thread::yield ();
  }

private:
  int const parties_;
  std::atomic<int> arrived_;
};

/// One window: @p threads threads spin on @p attempt for @p duration_ms;
/// returns the wall time per attempt and thread in nanoseconds.
template <typename Attempt>
double
measure (int threads, int duration_ms, Attempt attempt)
{
  start_gate_t gate (threads);
  std::atomic<bool> stop (false);
  std::atomic<std::uint64_t> total (0);
  std::vector<std::thread> pool;
  for (int i = 0; i < threads; ++i)
    pool.push_back (std::thread (
        [&]
          {
            gate.arrive_and_wait ();
            std::uint64_t count = 0;
            while (!stop.load (std::memory_order_relaxed))
              {
                for (int k = 0; k < 64; ++k)
                  attempt ();
                count += 64;
              }
            total.fetch_add (count);
          }));
  clock_type::time_point const begin = clock_type::now ();
  std::this_thread::sleep_for (std::chrono::milliseconds (duration_ms));
  stop.store (true);
  for (std::size_t i = 0; i < pool.size (); ++i)
    pool[i].join ();
  double const nanoseconds
      = static_cast<double> (std::chrono::duration_cast<std::chrono::nanoseconds> (
                                 clock_type::now () - begin)
                                 .count ());
  return nanoseconds * threads / static_cast<double> (total.load ());
}

double
measure_64 (int threads, int duration_ms)
{
  std::atomic<std::uint64_t> word (0);
  return measure (threads, duration_ms,
                  [&]
                    {
                      std::uint64_t guess = word.load (std::memory_order_relaxed);
                      word.compare_exchange_strong (guess, guess + 1);
                    });
}

double
measure_128 (int threads, int duration_ms)
{
  dwcas_word word;
  return measure (threads, duration_ms,
                  [&]
                    {
                      dwcas_value_t guess = word.speculative_load ();
                      dwcas_value_t next = guess;
                      ++next.lo;
                      dwcas_value_t const found
                          = word.compare_exchange_strong (guess, next);
                      LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (found);
                    });
}

double
median (std::vector<double> values)
{
  std::sort (values.begin (), values.end ());
  return values[values.size () / 2];
}

bool
parse (int argc, char **argv, options_t &options)
{
  options.duration_ms = 100;
  options.repetitions = 5;
  options.threads.push_back (1);
  options.threads.push_back (2);
  options.threads.push_back (4);
  options.threads.push_back (8);
  for (int i = 1; i < argc; ++i)
    {
      std::string const name = argv[i];
      if (i + 1 >= argc)
        return false;
      std::string const value = argv[++i];
      if (name == "--duration-ms")
        options.duration_ms = std::atoi (value.c_str ());
      else if (name == "--repetitions")
        options.repetitions = std::atoi (value.c_str ());
      else if (name == "--threads")
        {
          options.threads.clear ();
          for (std::size_t at = 0; at < value.size ();)
            {
              int const count = std::atoi (value.c_str () + at);
              if (count > 0)
                options.threads.push_back (count);
              std::size_t const comma = value.find (',', at);
              if (comma == std::string::npos)
                break;
              at = comma + 1;
            }
        }
      else
        return false;
    }
  return options.duration_ms > 0 && options.repetitions > 0
         && !options.threads.empty ();
}
} // namespace

int
main (int argc, char **argv)
{
  options_t options;
  if (!parse (argc, argv, options))
    {
      std::fprintf (stderr,
                    "usage: %s [--threads 1,2,4,8] [--duration-ms 100] "
                    "[--repetitions 5]\n",
                    argv[0]);
      return 2;
    }
  if (!lumex::core::atomic::dwcas::dwcas_supported ())
    {
      std::fprintf (stderr, "this CPU has no CMPXCHG16B\n");
      return 1;
    }
  std::printf ("128-bit compare-and-swap against 64-bit, backend %d, "
               "%d ms windows, median of %d\n",
               LUMEX_DWCAS_BACKEND, options.duration_ms, options.repetitions);
  std::printf ("%8s %14s %14s %8s\n", "threads", "64-bit ns", "128-bit ns",
               "ratio");
  for (std::size_t t = 0; t < options.threads.size (); ++t)
    {
      std::vector<double> narrow, wide, ratio;
      for (int r = 0; r < options.repetitions; ++r)
        {
          double a, b;
          if (r % 2 == 0)
            {
              a = measure_64 (options.threads[t], options.duration_ms);
              b = measure_128 (options.threads[t], options.duration_ms);
            }
          else
            {
              b = measure_128 (options.threads[t], options.duration_ms);
              a = measure_64 (options.threads[t], options.duration_ms);
            }
          narrow.push_back (a);
          wide.push_back (b);
          ratio.push_back (b / a);
        }
      std::printf ("%8d %14.1f %14.1f %8.2f\n", options.threads[t],
                   median (narrow), median (wide), median (ratio));
    }
  return 0;
}

#else

int
main ()
{
  std::fprintf (stderr, "the 128-bit compare-and-swap layer does not exist "
                        "on this target\n");
  return 1;
}

#endif // LUMEX_ATOMIC_HAS_DWCAS
