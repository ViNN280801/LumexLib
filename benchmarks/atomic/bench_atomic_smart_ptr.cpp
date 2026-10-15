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
//                             [--scenario shared|weak|destroy|store_wait]
//                             [--objects N] [--stores N] [--pin]
//
// Scenario store_wait: one writer times every single store () while the
// other threads load (); the result is the distribution of the time of one
// store, not a rate.
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
    = { "load",
        "store",
        "exchange",
        "compare_exchange_strong",
        "load_one_writer",
        "load_store",
        "weak_load",
        "weak_load_lock" };

/** The row of the writer's stores, taken from the load_one_writer run. */
char const *const k_one_writer_stores = "one_writer_stores";

/** Which rows a run writes. */
enum scenario_t
{
  scenario_shared, ///< the operations on the atomic shared pointers
  scenario_weak,   ///< load and lock () on the atomic weak pointers
  scenario_destroy, ///< the latency from a replacing store to the destructor
  scenario_store_wait ///< the time of every single store of a writer
};

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
  bool threads_given;
  long objects; ///< scenario destroy: replacements per measurement
  long stores;  ///< scenario store_wait: timed stores per measurement
  bool pin;     ///< scenario store_wait: pin every thread to a CPU
  int scenario;
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
         "                            [--repetition N] [--series KEY,KEY] "
         "[--list]\n"
         "                            [--scenario shared|weak|destroy|store_wait]"
         " [--objects N]\n"
         "                            [--stores N] [--pin]\n";
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
  options.threads_given = false;
  options.objects = 20000;
  options.stores = 100000;
  options.pin = false;
  options.scenario = scenario_shared;
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
          options.threads_given = true;
        }
      else if (std::strcmp (argv[i], "--scenario") == 0 && has_value)
        {
          char const *const name = argv[++i];
          if (std::strcmp (name, "shared") == 0)
            options.scenario = scenario_shared;
          else if (std::strcmp (name, "weak") == 0)
            options.scenario = scenario_weak;
          else if (std::strcmp (name, "destroy") == 0)
            options.scenario = scenario_destroy;
          else if (std::strcmp (name, "store_wait") == 0)
            options.scenario = scenario_store_wait;
          else
            return false;
        }
      else if (std::strcmp (argv[i], "--objects") == 0 && has_value)
        {
          if (!parse_positive (argv[++i], options.objects)
              || options.objects == 0)
            return false;
        }
      else if (std::strcmp (argv[i], "--stores") == 0 && has_value)
        {
          if (!parse_positive (argv[++i], options.stores)
              || options.stores == 0)
            return false;
        }
      else if (std::strcmp (argv[i], "--pin") == 0)
        options.pin = true;
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

/** One CSV row. */
void
write_row (std::ostream &csv, long repetition, char const *mode, int threads,
           char const *implementation, char const *operation, double ns,
           unsigned long long operations, unsigned long long successes,
           double seconds, double baseline_ns, double ratio, bool valid,
           double cycles, double instructions)
{
  csv << repetition << ',' << mode << ',' << threads << ',' << implementation
      << ',' << operation << ',' << ns << ',' << operations << ',' << successes
      << ',' << seconds << ',' << baseline_ns << ',' << ratio << ','
      << (valid ? 1 : 0) << ',' << cycles << ',' << instructions << '\n';
}

/** The operations of a block: the first and the one past the last. */
void
operations_of (int scenario, bool contended, int &first, int &last)
{
  if (scenario == scenario_weak)
    {
      first = bench::operation_weak_load;
      last = bench::operation_weak_load_lock + 1;
    }
  else
    {
      first = bench::operation_load;
      // load_store is one thread only: it is the last of the shared ones.
      last = contended ? bench::operation_load_store
                       : bench::operation_load_store + 1;
    }
}

/** Writes the baseline row and the operation rows of one block. */
bool
run_block (std::ostream &csv, long repetition, bool contended, int threads,
           bench::implementation_t const &implementation, double seconds,
           int scenario)
{
  char const *const mode = contended ? "contended" : "uncontended";
  bench::measurement_t const baseline = measure_baseline (threads, seconds);
  write_row (csv, repetition, mode, threads, implementation.key, "uint64_cas",
             baseline.ns_per_op, baseline.operations, baseline.successes,
             baseline.seconds, baseline.ns_per_op, 1.0, baseline.valid,
             baseline.cycles_per_op, baseline.instructions_per_op);
  bool valid = baseline.valid;
  int first = 0;
  int last = 0;
  operations_of (scenario, contended, first, last);
  for (int operation = first; operation < last; ++operation)
    {
      bench::measurement_t const measurement
          = implementation.measure (operation, threads, seconds, contended);
      double const ratio = baseline.ns_per_op > 0.0
                               ? measurement.ns_per_op / baseline.ns_per_op
                               : 0.0;
      write_row (csv, repetition, mode, threads, implementation.key,
                 k_operation_names[operation], measurement.ns_per_op,
                 measurement.operations, measurement.successes,
                 measurement.seconds, baseline.ns_per_op, ratio,
                 measurement.valid, measurement.cycles_per_op,
                 measurement.instructions_per_op);
      if (operation == bench::operation_load_one_writer)
        {
          // The writer's side of the same run.
          double const writer_ratio
              = baseline.ns_per_op > 0.0
                    ? measurement.writer_ns_per_op / baseline.ns_per_op
                    : 0.0;
          write_row (csv, repetition, mode, threads, implementation.key,
                     k_one_writer_stores, measurement.writer_ns_per_op,
                     measurement.writer_operations, 0u, measurement.seconds,
                     baseline.ns_per_op, writer_ratio, measurement.valid, 0.0,
                     0.0);
        }
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

/** Writes the rows of the destruction-latency scenario of one series. */
bool
run_destroy_block (std::ostream &csv, long repetition, int threads,
                   bench::implementation_t const &implementation, long objects)
{
  bench::latency_result_t const result
      = implementation.latency (threads, objects);
  double const seconds = 0.0;
  // ns_per_op carries the latency in ns (the peak backlog, in objects, for
  // destroy_backlog); operations the replaced objects; successes the ones
  // destroyed before the replacing store returned (destroy_median) or still
  // pending when the loop ended (destroy_backlog).
  write_row (csv, repetition, "destroy", threads, implementation.key,
             "destroy_median", result.median_ns, result.objects,
             result.in_call, seconds, 0.0, 0.0, result.valid, 0.0, 0.0);
  write_row (csv, repetition, "destroy", threads, implementation.key,
             "destroy_p99", result.p99_ns, result.objects, result.in_call,
             seconds, 0.0, 0.0, result.valid, 0.0, 0.0);
  write_row (csv, repetition, "destroy", threads, implementation.key,
             "destroy_max", result.max_ns, result.objects, result.in_call,
             seconds, 0.0, 0.0, result.valid, 0.0, 0.0);
  write_row (csv, repetition, "destroy", threads, implementation.key,
             "destroy_backlog", static_cast<double> (result.peak_backlog),
             result.objects, result.pending_end, seconds, 0.0, 0.0,
             result.valid, 0.0, 0.0);
  if (!result.valid)
    std::cerr << implementation.key << " (destroy, " << threads
              << " threads) destroyed an object twice or not at all\n";
  return result.valid;
}

/**
 * Writes the rows of the store-wait scenario of one series. ns_per_op carries
 * the time of one store in ns (the count of stores over 1 ms, in `successes`,
 * for store_wait_over_1ms; the median cost of an empty pair of clock reads
 * for store_wait_timer; the writer's involuntary context switches during the
 * timed stores, and the stores over 1 ms after which that count had advanced,
 * for store_wait_switches and store_wait_over_1ms_switched); operations the
 * timed stores.
 */
bool
run_store_wait_block (std::ostream &csv, long repetition, int threads,
                      bench::implementation_t const &implementation,
                      long stores, bool pin)
{
  bench::store_wait_result_t const result
      = implementation.store_wait (threads, stores, pin);
  struct row_t
  {
    char const *operation;
    double value;
    unsigned long long successes;
  };
  row_t const rows[]
      = { { "store_wait_timer", result.timer_ns, 0u },
          { "store_wait_median", result.median_ns, 0u },
          { "store_wait_p90", result.p90_ns, 0u },
          { "store_wait_p99", result.p99_ns, 0u },
          { "store_wait_p999", result.p999_ns, 0u },
          { "store_wait_p9999", result.p9999_ns, 0u },
          { "store_wait_max", result.max_ns, 0u },
          { "store_wait_over_1ms", static_cast<double> (result.over_1ms),
            result.over_1ms },
          { "store_wait_switches", static_cast<double> (result.switches),
            result.switches },
          { "store_wait_over_1ms_switched",
            static_cast<double> (result.over_1ms_switched),
            result.over_1ms_switched } };
  bool const valid = result.valid && (!pin || result.pinned);
  for (std::size_t i = 0; i < sizeof (rows) / sizeof (rows[0]); ++i)
    write_row (csv, repetition, "store_wait", threads, implementation.key,
               rows[i].operation, rows[i].value, result.stores,
               rows[i].successes, 0.0, 0.0, 0.0, valid, 0.0, 0.0);
  if (!result.valid)
    std::cerr << implementation.key << " (store_wait, " << threads
              << " threads) left a reference behind\n";
  if (pin && !result.pinned)
    std::cerr << implementation.key << " (store_wait, " << threads
              << " threads) could not pin its threads\n";
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
          bench::boost_implementation (),
          bench::lumex_split_count_cxx11_implementation (),
          bench::lumex_lock_based_lumex_cxx11_implementation (),
          bench::lumex_split_count_implementation (),
          bench::lumex_split_count_r4_implementation (),
          bench::lumex_lock_based_lumex_implementation (),
          bench::lumex_split_count_weak_implementation (),
          bench::lumex_lock_based_lumex_weak_implementation (),
          bench::lumex_lock_based_weak_implementation (),
          bench::lumex_lock_free_weak_implementation (),
          bench::lumex_std_backed_weak_implementation (),
          bench::std_weak_implementation () };
  int const wanted_kind = options.scenario == scenario_weak
                              ? bench::kind_weak
                              : bench::kind_shared;
  for (std::size_t i = 0; i < sizeof (candidates) / sizeof (candidates[0]);
       ++i)
    if (candidates[i].measure != nullptr && candidates[i].kind == wanted_kind
        && (options.scenario != scenario_store_wait
            || candidates[i].store_wait != nullptr)
        && series_selected (options, candidates[i].key))
      implementations.push_back (candidates[i]);

  if (options.list)
    {
      for (std::size_t i = 0; i < implementations.size (); ++i)
        std::cout << implementations[i].key << '|' << implementations[i].label
                  << '|' << implementations[i].path << '|'
                  << implementations[i].standard << '|'
                  << (implementations[i].lock_free ? 1 : 0) << '|'
                  << (implementations[i].kind == bench::kind_weak ? "weak"
                                                                  : "shared")
                  << '\n';
      return 0;
    }

  std::ofstream csv (options.csv.c_str ());
  if (!csv)
    {
      std::cerr << "cannot write " << options.csv << '\n';
      return 2;
    }
  // The latencies of the store-wait scenario are whole nanoseconds up to
  // tens of milliseconds: keep every digit of them.
  csv.precision (options.scenario == scenario_store_wait ? 12 : 6);
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
      << '\n'
      << "# scenario="
      << (options.scenario == scenario_weak        ? "weak"
          : options.scenario == scenario_destroy   ? "destroy"
          : options.scenario == scenario_store_wait ? "store_wait"
                                                   : "shared")
      << '\n';
  if (options.scenario == scenario_store_wait)
    csv << "# stores=" << options.stores << '\n'
        << "# pinned=" << (options.pin ? 1 : 0) << '\n'
        << "# clock=std::chrono::steady_clock\n";
  for (std::size_t i = 0; i < implementations.size (); ++i)
    csv << "# implementation=" << implementations[i].key << '|'
        << implementations[i].label << '|' << implementations[i].path << '|'
        << implementations[i].standard << '|'
        << (implementations[i].lock_free ? 1 : 0) << '\n';
  csv << "repetition,mode,threads,implementation,operation,ns_per_op,"
         "operations,successes,seconds,baseline_ns,ratio,valid,"
         "cycles_per_op,instructions_per_op\n";

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
  if (options.scenario == scenario_destroy)
    {
      // One storing thread alone, then with three readers.
      std::vector<int> counts = options.threads;
      if (!options.threads_given)
        {
          counts.clear ();
          counts.push_back (1);
          counts.push_back (4);
        }
      for (std::size_t t = 0; t < counts.size (); ++t)
        for (std::size_t k = 0; k < count; ++k)
          valid
              = run_destroy_block (csv, options.repetition, counts[t],
                                   implementations[(rotation + t + k) % count],
                                   options.objects)
                && valid;
      csv.flush ();
      return valid && csv ? 0 : 3;
    }
  if (options.scenario == scenario_store_wait)
    {
      // One writer and the readers: two threads and up, the oversubscribed
      // counts of the 20 logical CPUs included.
      std::vector<int> counts = options.threads;
      if (!options.threads_given)
        {
          int const defaults[] = { 2, 4, 8, 12, 20, 40, 60 };
          counts.assign (defaults, defaults + 7);
        }
      for (std::size_t t = 0; t < counts.size (); ++t)
        {
          std::cerr << "repetition " << options.repetition << ": "
                    << counts[t] << " threads\n";
          for (std::size_t k = 0; k < count; ++k)
            valid = run_store_wait_block (
                        csv, options.repetition, counts[t],
                        implementations[(rotation + t + k) % count],
                        options.stores, options.pin)
                    && valid;
        }
      csv.flush ();
      return valid && csv ? 0 : 3;
    }
  for (std::size_t k = 0; k < count; ++k)
    valid = run_block (csv, options.repetition, false, 1,
                       implementations[(rotation + k) % count], seconds,
                       options.scenario)
            && valid;
  for (std::size_t t = 0; t < options.threads.size (); ++t)
    {
      std::cerr << "repetition " << options.repetition << ": "
                << options.threads[t] << " threads\n";
      for (std::size_t k = 0; k < count; ++k)
        valid = run_block (csv, options.repetition, true, options.threads[t],
                           implementations[(rotation + t + k) % count],
                           seconds, options.scenario)
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
