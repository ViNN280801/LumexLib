// bench_atomic_smart_ptr.cpp
// Contention benchmark of atomic shared pointers: LumexLib's lock-based
// atomic_shared_ptr built at C++11 and at C++20, LumexLib's default
// selection at C++20, and the standard library's
// std::atomic<std::shared_ptr<T>> where the library has one (libstdc++ 12+,
// the MSVC STL). The method follows the author's benchmark of the libc++
// implementation (llvm-project pull request 194215):
//
//   - load (), store (), exchange () and load () + compare_exchange_strong ()
//     on one object shared by 1, 2, 4, ... threads, and on a private object
//     (uncontended);
//   - every result is divided by std::atomic<std::uint64_t>::
//     compare_exchange_strong timed in the same process, with the same
//     thread count, right before the implementation's block, because raw
//     nanoseconds drift between processes;
//   - the implementations take turns at every thread count, starting with a
//     different one in every repetition, instead of running as blocks.
//
// One run of this program is one repetition; run_benchmark.py runs it many
// times with pauses in between and takes the medians.
//
// Usage: LumexAtomicBenchmark [--csv FILE] [--threads 1,2,4] [--duration-ms N]
//                             [--warmup-ms N] [--repetition N] [--list]
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "bench_atomic_smart_ptr.hpp"

#if defined(LUMEX_ATOMIC_HAS_HAZARD_POINTER)
#include "lumex/core/hazard_pointer/LumexHazardPointer"
#endif

namespace bench = lumex_atomic_bench;

void
lumex_atomic_bench::settle_hazard_domain ()
{
#if defined(LUMEX_ATOMIC_HAS_HAZARD_POINTER)
  lumex::core::hazard_pointer::clean_up ();
#endif
}

namespace
{
char const *const k_operation_names[bench::operation_count]
    = { "load", "store", "exchange", "compare_exchange_strong",
        "load_one_writer" };

#if defined(__cpp_lib_atomic_shared_ptr)
#if defined(_MSC_VER) && !defined(__clang__)
#define LUMEX_ATOMIC_BENCH_STD_LIBRARY                                        \
  "MSVC STL (MSVC " LUMEX_ATOMIC_BENCH_STRINGIFY (_MSC_VER) ")"
#elif defined(_LIBCPP_VERSION)
#define LUMEX_ATOMIC_BENCH_STD_LIBRARY                                        \
  "libc++ " LUMEX_ATOMIC_BENCH_STRINGIFY (_LIBCPP_VERSION)
#elif defined(_GLIBCXX_RELEASE)
#define LUMEX_ATOMIC_BENCH_STD_LIBRARY                                        \
  "libstdc++ " LUMEX_ATOMIC_BENCH_STRINGIFY (_GLIBCXX_RELEASE)
#else
#define LUMEX_ATOMIC_BENCH_STD_LIBRARY "the standard library"
#endif

/** std::atomic<std::shared_ptr<int>> of the standard library in use. */
bench::implementation_t
std_implementation ()
{
  return bench::detail::make_implementation<
      std::atomic<std::shared_ptr<int>>> (
      "std", "std::atomic, " LUMEX_ATOMIC_BENCH_STD_LIBRARY,
      LUMEX_ATOMIC_BENCH_STD_LIBRARY, LUMEX_ATOMIC_BENCH_STANDARD);
}
#endif

std::string
compiler_name ()
{
  std::ostringstream name;
#if defined(_MSC_VER) && !defined(__clang__)
  name << "MSVC " << _MSC_VER;
#elif defined(__clang__)
  name << "Clang " << __clang_major__ << '.' << __clang_minor__;
#elif defined(__GNUC__)
  name << "GCC " << __GNUC__ << '.' << __GNUC_MINOR__;
#else
  name << "unknown";
#endif
  return name.str ();
}

std::string
library_name ()
{
#if defined(_MSC_VER) && !defined(__clang__)
  return "MSVC STL";
#elif defined(_LIBCPP_VERSION)
  std::ostringstream name;
  name << "libc++ " << _LIBCPP_VERSION;
  return name.str ();
#elif defined(_GLIBCXX_RELEASE)
  std::ostringstream name;
  name << "libstdc++ " << _GLIBCXX_RELEASE;
  return name.str ();
#else
  return "unknown";
#endif
}

/** The u64 compare-exchange loop of the libc++ benchmark: the baseline. */
struct baseline_body_t
{
  std::atomic<std::uint64_t> *atom;

  void
  operator() (std::atomic<bool> const &stop,
              bench::detail::thread_result_t &out) const
  {
    unsigned long long operations = 0u;
    unsigned long long successes = 0u;
    while (!stop.load (std::memory_order_relaxed))
      {
        std::uint64_t expected = atom->load (std::memory_order_relaxed);
        if (atom->compare_exchange_strong (expected, expected ^ 1u))
          ++successes;
        ++operations;
      }
    out.operations = operations;
    out.successes = successes;
  }
};

bench::measurement_t
measure_baseline (int threads, double seconds)
{
  bench::detail::atomic_holder_t<std::atomic<std::uint64_t>> const holder (1u);
  baseline_body_t const body = { holder.get () };
  return bench::detail::timed_run (threads, seconds, body);
}

/** 1, then every even count up to `limit` (and `limit` when it is odd). */
std::vector<int>
default_thread_counts (int limit)
{
  std::vector<int> counts (1, 1);
  for (int count = 2; count <= limit; count += 2)
    counts.push_back (count);
  if (limit > 1 && limit % 2 != 0)
    counts.push_back (limit);
  return counts;
}

bool
parse_thread_counts (char const *text, std::vector<int> &counts)
{
  counts.clear ();
  std::stringstream stream (text);
  std::string item;
  while (std::getline (stream, item, ','))
    {
      char *end = nullptr;
      long const value = std::strtol (item.c_str (), &end, 10);
      if (end == item.c_str () || *end != '\0' || value < 1 || value > 1024)
        return false;
      counts.push_back (static_cast<int> (value));
    }
  return !counts.empty ();
}

bool
parse_positive (char const *text, long &value)
{
  char *end = nullptr;
  value = std::strtol (text, &end, 10);
  return end != text && *end == '\0' && value >= 0;
}

struct options_t
{
  std::string csv;
  std::vector<int> threads;
  long duration_ms;
  long warmup_ms;
  long repetition;
  bool list;
  std::vector<std::string> series; ///< empty: every series
};

bool
series_selected (options_t const &options, char const *key)
{
  if (options.series.empty ())
    return true;
  for (std::size_t i = 0; i < options.series.size (); ++i)
    if (options.series[i] == key)
      return true;
  return false;
}

void
print_usage ()
{
  std::cerr
      << "usage: LumexAtomicBenchmark [--csv FILE] [--threads 1,2,4]\n"
         "                            [--duration-ms N] [--warmup-ms N]\n"
         "                            [--repetition N] [--series KEY,KEY] [--list]\n";
}

bool
parse_options (int argc, char **argv, options_t &options)
{
  unsigned const cpus = std::thread::hardware_concurrency ();
  options.csv = "atomic_benchmark_run.csv";
  options.threads
      = default_thread_counts (cpus == 0u ? 4 : static_cast<int> (cpus));
  options.duration_ms = 100;
  options.warmup_ms = 300;
  options.repetition = 0;
  options.list = false;
  for (int i = 1; i < argc; ++i)
    {
      bool const has_value = i + 1 < argc;
      if (std::strcmp (argv[i], "--list") == 0)
        options.list = true;
      else if (std::strcmp (argv[i], "--csv") == 0 && has_value)
        options.csv = argv[++i];
      else if (std::strcmp (argv[i], "--threads") == 0 && has_value)
        {
          if (!parse_thread_counts (argv[++i], options.threads))
            return false;
        }
      else if (std::strcmp (argv[i], "--duration-ms") == 0 && has_value)
        {
          if (!parse_positive (argv[++i], options.duration_ms)
              || options.duration_ms == 0)
            return false;
        }
      else if (std::strcmp (argv[i], "--warmup-ms") == 0 && has_value)
        {
          if (!parse_positive (argv[++i], options.warmup_ms))
            return false;
        }
      else if (std::strcmp (argv[i], "--series") == 0 && has_value)
        {
          std::stringstream stream (argv[++i]);
          std::string item;
          while (std::getline (stream, item, ','))
            options.series.push_back (item);
        }
      else if (std::strcmp (argv[i], "--repetition") == 0 && has_value)
        {
          if (!parse_positive (argv[++i], options.repetition))
            return false;
        }
      else
        return false;
    }
  return true;
}

/** Writes the baseline row and the four operation rows of one block. */
bool
run_block (std::ostream &csv, long repetition, bool contended, int threads,
           bench::implementation_t const &implementation, double seconds)
{
  char const *const mode = contended ? "contended" : "uncontended";
  bench::measurement_t const baseline = measure_baseline (threads, seconds);
  csv << repetition << ',' << mode << ',' << threads << ','
      << implementation.key << ",uint64_cas," << baseline.ns_per_op << ','
      << baseline.operations << ',' << baseline.successes << ','
      << baseline.seconds << ',' << baseline.ns_per_op << ",1,"
      << (baseline.valid ? 1 : 0) << '\n';
  bool valid = baseline.valid;
  for (int operation = 0; operation < bench::operation_count; ++operation)
    {
      bench::measurement_t const measurement
          = implementation.measure (operation, threads, seconds, contended);
      double const ratio = baseline.ns_per_op > 0.0
                               ? measurement.ns_per_op / baseline.ns_per_op
                               : 0.0;
      csv << repetition << ',' << mode << ',' << threads << ','
          << implementation.key << ',' << k_operation_names[operation] << ','
          << measurement.ns_per_op << ',' << measurement.operations << ','
          << measurement.successes << ',' << measurement.seconds << ','
          << baseline.ns_per_op << ',' << ratio << ','
          << (measurement.valid ? 1 : 0) << '\n';
      if (!measurement.valid)
        {
          std::cerr << implementation.key << ' '
                    << k_operation_names[operation] << " (" << mode << ", "
                    << threads << " threads) left a reference behind\n";
          valid = false;
        }
    }
  return valid;
}
} // namespace

int
main (int argc, char **argv)
{
  options_t options;
  if (!parse_options (argc, argv, options))
    {
      print_usage ();
      return 2;
    }

  std::vector<bench::implementation_t> implementations;
  bench::implementation_t const candidates[]
      = { bench::lumex_lock_based_cxx11_implementation (),
          bench::lumex_lock_based_implementation (),
          bench::lumex_lock_free_cxx11_implementation (),
          bench::lumex_lock_free_implementation (),
          bench::lumex_lock_free_deferred_implementation (),
          bench::lumex_default_implementation (),
          bench::lumex_std_backed_implementation (),
#if defined(__cpp_lib_atomic_shared_ptr)
          std_implementation (),
#endif
          bench::boost_implementation () };
  for (std::size_t i = 0; i < sizeof (candidates) / sizeof (candidates[0]);
       ++i)
    if (candidates[i].measure != nullptr
        && series_selected (options, candidates[i].key))
      implementations.push_back (candidates[i]);

  if (options.list)
    {
      for (std::size_t i = 0; i < implementations.size (); ++i)
        std::cout << implementations[i].key << '|' << implementations[i].label
                  << '|' << implementations[i].path << '|'
                  << implementations[i].standard << '|'
                  << (implementations[i].lock_free ? 1 : 0) << '\n';
      return 0;
    }

  std::ofstream csv (options.csv.c_str ());
  if (!csv)
    {
      std::cerr << "cannot write " << options.csv << '\n';
      return 2;
    }
  csv.precision (6);
  csv << "# tool=LumexAtomicBenchmark\n"
      << "# compiler=" << compiler_name () << '\n'
      << "# library=" << library_name () << '\n'
#if defined(NDEBUG)
      << "# build=release\n"
#else
      << "# build=debug\n"
#endif
      << "# repetition=" << options.repetition << '\n'
      << "# duration_ms=" << options.duration_ms << '\n'
      << "# hardware_concurrency=" << std::thread::hardware_concurrency ()
      << '\n';
  for (std::size_t i = 0; i < implementations.size (); ++i)
    csv << "# implementation=" << implementations[i].key << '|'
        << implementations[i].label << '|' << implementations[i].path << '|'
        << implementations[i].standard << '|'
        << (implementations[i].lock_free ? 1 : 0) << '\n';
  csv << "repetition,mode,threads,implementation,operation,ns_per_op,"
         "operations,successes,seconds,baseline_ns,ratio,valid\n";

  double const seconds = static_cast<double> (options.duration_ms) / 1000.0;
  int max_threads = 1;
  for (std::size_t i = 0; i < options.threads.size (); ++i)
    if (options.threads[i] > max_threads)
      max_threads = options.threads[i];
  // Wakes every core from its idle state before the first block.
  if (options.warmup_ms > 0)
    {
      bench::measurement_t const warmup = measure_baseline (
          max_threads, static_cast<double> (options.warmup_ms) / 1000.0);
      if (!warmup.valid)
        return 3;
    }

  std::size_t const count = implementations.size ();
  std::size_t const rotation = static_cast<std::size_t> (options.repetition);
  bool valid = true;
  for (std::size_t k = 0; k < count; ++k)
    valid = run_block (csv, options.repetition, false, 1,
                       implementations[(rotation + k) % count], seconds)
            && valid;
  for (std::size_t t = 0; t < options.threads.size (); ++t)
    {
      std::cerr << "repetition " << options.repetition << ": "
                << options.threads[t] << " threads\n";
      for (std::size_t k = 0; k < count; ++k)
        valid
            = run_block (csv, options.repetition, true, options.threads[t],
                         implementations[(rotation + t + k) % count], seconds)
              && valid;
    }
  csv.flush ();
  if (!csv)
    {
      std::cerr << "cannot write " << options.csv << '\n';
      return 2;
    }
  return valid ? 0 : 3;
}
