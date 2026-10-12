// bench_format.cpp
// Throughput of LumexFormat against std::format, std::ostringstream,
// std::snprintf and std::to_string on the same scenarios. Every method
// produces the same text for a scenario (checked before timing), so the
// numbers compare equal work.
//
// Usage: LumexFmtBenchmark [output.csv] [--quick]
// Writes one CSV row per (scenario, method): median / min / max ns per call
// over the repetitions. `plot_results.py` turns the CSV into SVG charts and
// a Markdown table.
#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>
#if __cplusplus >= 202002L && defined(__has_include)
#if __has_include(<format>)
#include <format>
#endif
#endif

#include "lumex/core/fmt/LumexFormat.hpp"

namespace lfmt = lumex::core::fmt;

namespace
{
struct result_t
{
  std::string scenario;
  std::string method;
  double median_ns;
  double min_ns;
  double max_ns;
  long long iterations;
  int repetitions;
};

struct method_t
{
  std::string name;
  std::function<std::string (int)> produce;
};

struct scenario_t
{
  std::string name;
  std::string description;
  std::vector<method_t> methods;
};

/** Keeps the optimizer from dropping the formatted text. */
std::size_t volatile g_sink = 0;

double
value_at (int i)
{
  return 1.0 + i * 0.001;
}

std::string
snprintf_text (char const *pattern, ...)
{
  char buffer[256];
  va_list args;
  va_start (args, pattern);
  int const size = std::vsnprintf (buffer, sizeof (buffer), pattern, args);
  va_end (args);
  return std::string (buffer, static_cast<std::size_t> (size));
}

std::vector<scenario_t>
make_scenarios ()
{
  std::vector<scenario_t> scenarios;

  scenarios.push_back (scenario_t{
      "int",
      "\"{}\" of an int",
      { { "LumexFormat",
          [] (int i) { return lfmt::format ("{}", i * 7919); } },
#if defined(__cpp_lib_format)
        { "std::format", [] (int i) { return std::format ("{}", i * 7919); } },
#endif
        { "ostringstream",
          [] (int i)
            {
              std::ostringstream stream;
              stream << i * 7919;
              return stream.str ();
            } },
        { "snprintf", [] (int i) { return snprintf_text ("%d", i * 7919); } },
        { "to_string", [] (int i) { return std::to_string (i * 7919); } } } });

  scenarios.push_back (
      scenario_t{ "int_hex_padded",
                  "\"{:#010x}\" of an int",
                  { { "LumexFormat", [] (int i)
                        { return lfmt::format ("{:#010x}", i * 7919); } },
#if defined(__cpp_lib_format)
                    { "std::format", [] (int i)
                        { return std::format ("{:#010x}", i * 7919); } },
#endif
                    { "ostringstream",
                      [] (int i)
                        {
                          std::ostringstream stream;
                          stream << "0x" << std::hex << std::setw (8)
                                 << std::setfill ('0') << i * 7919;
                          return stream.str ();
                        } },
                    { "snprintf", [] (int i)
                        { return snprintf_text ("0x%08x", i * 7919); } } } });

  scenarios.push_back (scenario_t{
      "double_fixed",
      "\"{:.3f}\" of a double",
      { { "LumexFormat",
          [] (int i) { return lfmt::format ("{:.3f}", value_at (i)); } },
#if defined(__cpp_lib_format)
        { "std::format",
          [] (int i) { return std::format ("{:.3f}", value_at (i)); } },
#endif
        { "ostringstream",
          [] (int i)
            {
              std::ostringstream stream;
              stream << std::fixed << std::setprecision (3) << value_at (i);
              return stream.str ();
            } },
        { "snprintf",
          [] (int i) { return snprintf_text ("%.3f", value_at (i)); } } } });

  scenarios.push_back (
      scenario_t{ "double_shortest",
                  "\"{}\" of a double (shortest round trip; to_string prints "
                  "6 fixed digits, "
                  "shown for scale)",
                  { { "LumexFormat", [] (int i)
                        { return lfmt::format ("{}", value_at (i)); } },
#if defined(__cpp_lib_format)
                    { "std::format", [] (int i)
                        { return std::format ("{}", value_at (i)); } },
#endif
                    { "to_string", [] (int i)
                        { return std::to_string (value_at (i)); } } } });

  scenarios.push_back (scenario_t{
      "string_padded",
      "\"{:>16}\" of a std::string",
      { { "LumexFormat", [] (int)
            { return lfmt::format ("{:>16}", std::string ("channel")); } },
#if defined(__cpp_lib_format)
        { "std::format", [] (int)
            { return std::format ("{:>16}", std::string ("channel")); } },
#endif
        { "ostringstream",
          [] (int)
            {
              std::ostringstream stream;
              stream << std::setw (16) << std::string ("channel");
              return stream.str ();
            } },
        { "snprintf",
          [] (int) { return snprintf_text ("%16s", "channel"); } } } });

  scenarios.push_back (scenario_t{
      "log_line",
      "\"channel {:>3} flow {:8.3f} ml/min state {}\"",
      { { "LumexFormat",
          [] (int i)
            {
              return lfmt::format (
                  "channel {:>3} flow {:8.3f} ml/min state {}", i % 16,
                  value_at (i), "running");
            } },
#if defined(__cpp_lib_format)
        { "std::format",
          [] (int i)
            {
              return std::format ("channel {:>3} flow {:8.3f} ml/min state {}",
                                  i % 16, value_at (i), "running");
            } },
#endif
        { "ostringstream",
          [] (int i)
            {
              std::ostringstream stream;
              stream << "channel " << std::setw (3) << i % 16 << " flow "
                     << std::setw (8) << std::fixed << std::setprecision (3)
                     << value_at (i) << " ml/min state " << "running";
              return stream.str ();
            } },
        { "snprintf", [] (int i)
            {
              return snprintf_text ("channel %3d flow %8.3f ml/min state %s",
                                    i % 16, value_at (i), "running");
            } } } });

  scenarios.push_back (scenario_t{
      "ten_args",
      "ten \"{}\" fields (ints and strings)",
      { { "LumexFormat",
          [] (int i)
            {
              return lfmt::format ("{} {} {} {} {} {} {} {} {} {}", i, "a",
                                   i + 1, "bb", i + 2, "ccc", i + 3, "dddd",
                                   i + 4, "eeeee");
            } },
#if defined(__cpp_lib_format)
        { "std::format",
          [] (int i)
            {
              return std::format ("{} {} {} {} {} {} {} {} {} {}", i, "a",
                                  i + 1, "bb", i + 2, "ccc", i + 3, "dddd",
                                  i + 4, "eeeee");
            } },
#endif
        { "ostringstream",
          [] (int i)
            {
              std::ostringstream stream;
              stream << i << ' ' << "a" << ' ' << i + 1 << ' ' << "bb" << ' '
                     << i + 2 << ' ' << "ccc" << ' ' << i + 3 << ' ' << "dddd"
                     << ' ' << i + 4 << ' ' << "eeeee";
              return stream.str ();
            } },
        { "snprintf", [] (int i)
            {
              return snprintf_text ("%d %s %d %s %d %s %d %s %d %s", i, "a",
                                    i + 1, "bb", i + 2, "ccc", i + 3, "dddd",
                                    i + 4, "eeeee");
            } } } });

  scenarios.push_back (scenario_t{
      "append_to_buffer",
      "format_to into a reused std::string",
      { { "LumexFormat",
          [] (int i)
            {
              static std::string buffer;
              buffer.clear ();
              lfmt::format_to (std::back_inserter (buffer), "{}:{:.2f};", i,
                               value_at (i));
              return buffer;
            } },
#if defined(__cpp_lib_format)
        { "std::format",
          [] (int i)
            {
              static std::string buffer;
              buffer.clear ();
              std::format_to (std::back_inserter (buffer), "{}:{:.2f};", i,
                              value_at (i));
              return buffer;
            } },
#endif
        { "ostringstream",
          [] (int i)
            {
              static std::ostringstream stream;
              stream.str (std::string ());
              stream << i << ':' << std::fixed << std::setprecision (2)
                     << value_at (i) << ';';
              return stream.str ();
            } },
        { "snprintf", [] (int i)
            { return snprintf_text ("%d:%.2f;", i, value_at (i)); } } } });
  return scenarios;
}

/** Every method of a scenario must print the same text. */
bool
check_same_output (scenario_t const &scenario)
{
  bool same = true;
  for (int i = 0; i < 50; ++i)
    {
      std::string const reference = scenario.methods.front ().produce (i);
      for (method_t const &method : scenario.methods)
        {
          if (scenario.name == "double_shortest" && method.name == "to_string")
            continue; // 6 fixed digits by design, shown for scale
          std::string const text = method.produce (i);
          if (text != reference)
            {
              std::cerr << scenario.name << ": " << method.name << " printed '"
                        << text << "', LumexFormat '" << reference << "'\n";
              same = false;
            }
        }
    }
  return same;
}

result_t
measure (scenario_t const &scenario, method_t const &method,
         long long iterations, int repetitions)
{
  // Warm-up: caches, allocator, first-call costs.
  for (long long i = 0; i < iterations / 10 + 1; ++i)
    g_sink = g_sink + method.produce (static_cast<int> (i)).size ();

  std::vector<double> samples;
  for (int r = 0; r < repetitions; ++r)
    {
      std::chrono::steady_clock::time_point const start
          = std::chrono::steady_clock::now ();
      for (long long i = 0; i < iterations; ++i)
        g_sink = g_sink + method.produce (static_cast<int> (i)).size ();
      std::chrono::duration<double, std::nano> const elapsed
          = std::chrono::steady_clock::now () - start;
      samples.push_back (elapsed.count () / static_cast<double> (iterations));
    }
  std::sort (samples.begin (), samples.end ());
  result_t result;
  result.scenario = scenario.name;
  result.method = method.name;
  result.median_ns = samples[samples.size () / 2];
  result.min_ns = samples.front ();
  result.max_ns = samples.back ();
  result.iterations = iterations;
  result.repetitions = repetitions;
  return result;
}

/** CSV field text with embedded quotes doubled (RFC 4180). */
std::string
csv_quoted (std::string const &text)
{
  std::string result;
  for (char const c : text)
    {
      if (c == '"')
        result += '"';
      result += c;
    }
  return result;
}

std::string
compiler_name ()
{
#if defined(_MSC_VER) && !defined(__clang__)
  return lfmt::format ("MSVC {}", _MSC_VER);
#elif defined(__clang__)
  return lfmt::format ("Clang {}.{}", __clang_major__, __clang_minor__);
#elif defined(__GNUC__)
  return lfmt::format ("GCC {}.{}", __GNUC__, __GNUC_MINOR__);
#else
  return "unknown";
#endif
}
} // namespace

int
main (int argc, char **argv)
{
  std::string output = "format_benchmark.csv";
  bool quick = false;
  for (int i = 1; i < argc; ++i)
    {
      if (std::strcmp (argv[i], "--quick") == 0)
        quick = true;
      else
        output = argv[i];
    }
  long long const iterations = quick ? 20000 : 200000;
  int const repetitions = quick ? 3 : 15;

  std::vector<scenario_t> const scenarios = make_scenarios ();
  bool all_same = true;
  for (scenario_t const &scenario : scenarios)
    all_same = check_same_output (scenario) && all_same;
  if (!all_same)
    {
      std::cerr << "methods disagree on the output; numbers would compare "
                   "different work\n";
      return 1;
    }

  std::ofstream csv (output.c_str ());
  csv << "# compiler=" << compiler_name () << " cplusplus=" << __cplusplus
#if defined(NDEBUG)
      << " build=release"
#else
      << " build=debug"
#endif
      << "\n";
  csv << "scenario,description,method,median_ns,min_ns,max_ns,iterations,"
         "repetitions\n";
  for (scenario_t const &scenario : scenarios)
    for (method_t const &method : scenario.methods)
      {
        result_t const result
            = measure (scenario, method, iterations, repetitions);
        csv << lfmt::format ("{},\"{}\",{},{:.2f},{:.2f},{:.2f},{},{}\n",
                             result.scenario,
                             csv_quoted (scenario.description), result.method,
                             result.median_ns, result.min_ns, result.max_ns,
                             result.iterations, result.repetitions);
        std::cout << lfmt::format ("{:<18} {:<22} {:>9.1f} ns/call\n",
                                   result.scenario, result.method,
                                   result.median_ns);
      }
  std::cout << "written " << output << " (sink " << g_sink << ")\n";
  return 0;
}
