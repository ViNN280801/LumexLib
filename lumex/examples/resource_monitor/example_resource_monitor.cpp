#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "lumex/applied/resource_monitor/LumexResourceMonitor"

using lumex::applied::resource_monitor::monitor::LumexResourceMonitor;
using lumex::applied::resource_monitor::process::LumexProcessMonitor;
using lumex::applied::resource_monitor::process::process_id_t;
using lumex::applied::resource_monitor::process::process_query_error;
using lumex::applied::resource_monitor::process::process_usage_t;
using lumex::applied::resource_monitor::process::total;

namespace
{
std::string
describe (process_query_error error)
{
  switch (error)
    {
    case process_query_error::not_found:
      return "no such process";
    case process_query_error::access_denied:
      return "access denied";
    case process_query_error::read_failed:
      return "the system refused the query";
    case process_query_error::unsupported:
      return "this platform is not supported";
    }
  return "unknown error";
}

// Keeps one core busy, so the next sample of this process has a CPU share.
void
burn_cpu (std::chrono::milliseconds duration)
{
  std::chrono::steady_clock::time_point const end
      = std::chrono::steady_clock::now () + duration;
  volatile std::uint64_t sink = 0;
  while (std::chrono::steady_clock::now () < end)
    sink = sink + 1;
}

void
print_usage (process_usage_t const &usage)
{
  std::cout << "  pid " << usage.pid << ", name \"" << usage.name << "\"\n";
  std::cout << "  resident " << usage.resident_bytes / 1024 << " KiB";
  if (usage.private_bytes)
    std::cout << ", private " << *usage.private_bytes / 1024 << " KiB";
  std::cout << "\n  cpu time "
            << std::chrono::duration_cast<std::chrono::milliseconds> (
                   usage.cpu_time)
                   .count ()
            << " ms";
  if (usage.cpu_percent)
    std::cout << ", cpu share " << std::fixed << std::setprecision (1)
              << *usage.cpu_percent << "% of " << usage.logical_processors
              << " logical processors";
  else
    std::cout << ", cpu share unknown (a share needs two samples)";
  std::cout << "\n";
}
} // namespace

int
main ()
{
  std::cout << "=== Resource monitor ===\n\n";

  std::cout << "--- 1. stop is always safe ---\n";
  LumexResourceMonitor::stop ();
  std::cout << "stop() on idle sampler ok\n";

  std::cout << "\n--- 2. start_if_enabled then stop ---\n";
  std::string const dir (".");
  LumexResourceMonitor::start_if_enabled (dir,
                                          std::chrono::milliseconds (250));
  std::cout << "start_if_enabled(\".\") returned\n";
  LumexResourceMonitor::stop ();
  std::cout << "stop() after start ok\n";

  std::cout
      << "\n--- 3. One process by ID: two samples give a CPU share ---\n";
  LumexProcessMonitor monitor;
  process_id_t const self = LumexProcessMonitor::current_process_id ();
  LumexProcessMonitor::result_t const first = monitor.sample (self);
  if (!first.has_value ())
    {
      std::cout << "sample failed: " << describe (first.error ()) << "\n";
      std::cout << "\n=== Resource monitor example finished ===\n";
      return 0;
    }
  std::cout << "first sample:\n";
  print_usage (*first);

  burn_cpu (std::chrono::milliseconds (200));

  LumexProcessMonitor::result_t const second = monitor.sample (self);
  if (second.has_value ())
    {
      std::cout << "second sample, after 200 ms of work:\n";
      print_usage (*second);
    }

  std::cout << "\n--- 4. Processes by executable name ---\n";
  std::string const name = first->name;
  std::vector<process_id_t> const ids
      = LumexProcessMonitor::find_by_name (name);
  std::cout << ids.size () << " process(es) named \"" << name
            << "\" (found without sampling them)\n";
  std::vector<process_usage_t> const usages = monitor.sample_by_name (name);
  process_usage_t const sum = total (usages);
  std::cout << "sum of " << sum.process_count
            << " sample(s): " << sum.resident_bytes / 1024
            << " KiB resident\n";

  std::cout << "\n--- 5. A process that does not exist ---\n";
  process_id_t const absent = 0x7FFFFFFEU;
  LumexProcessMonitor::result_t const missing = monitor.sample (absent);
  if (!missing.has_value ())
    std::cout << "sample(" << absent << "): " << describe (missing.error ())
              << "\n";
  else
    std::cout << "sample(" << absent << ") found a process\n";

  std::cout << "\n--- 6. forget_exited ---\n";
  monitor.forget_exited ();
  std::cout << "forget_exited() dropped what was remembered about processes "
               "that are gone\n";

  std::cout << "\n=== Resource monitor example finished ===\n";
  return 0;
}
