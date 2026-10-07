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

namespace
{
// The CPU part of a piece of work: adds the data up for about 150 ms. The sum
// is returned so the compiler cannot drop the loop.
std::uint64_t
add_up (std::vector<std::uint64_t> const &data)
{
  std::uint64_t sum = 0;
  std::chrono::steady_clock::time_point const end
      = std::chrono::steady_clock::now () + std::chrono::milliseconds (150);
  while (std::chrono::steady_clock::now () < end)
    for (std::uint64_t const value : data)
      sum += value;
  return sum;
}
} // namespace

int
main ()
{
  std::cout
      << "=== Workflow: what one piece of work costs this process ===\n\n";

  // The background sampler logs the whole machine while the work runs.
  LumexResourceMonitor::start_if_enabled (std::string ("."),
                                          std::chrono::milliseconds (500));

  // The monitor remembers the CPU time of its last sample, so the sample
  // after the work reports the share used since the sample before it.
  LumexProcessMonitor monitor;
  process_id_t const self = LumexProcessMonitor::current_process_id ();
  LumexProcessMonitor::result_t const before = monitor.sample (self);

  // The work takes memory (32 MiB, written, so it is really in RAM) and CPU.
  // The data stays alive until the second sample, which is what shows it.
  std::vector<std::uint64_t> const data (4U * 1024U * 1024U, 1U);
  std::uint64_t const sum = add_up (data);

  LumexProcessMonitor::result_t const after = monitor.sample (self);

  if (before.has_value () && after.has_value ())
    {
      std::cout << "work result " << sum << "\n";
      std::cout << "resident before " << before->resident_bytes / 1024
                << " KiB, after " << after->resident_bytes / 1024 << " KiB\n";
      if (after->cpu_percent)
        std::cout << "cpu share during the work " << std::fixed
                  << std::setprecision (1) << *after->cpu_percent << "% of "
                  << after->logical_processors << " logical processors\n";
    }
  else
    std::cout << "the process could not be sampled on this platform\n";

  LumexResourceMonitor::stop ();
  std::cout << "\n=== Workflow finished ===\n";
  return 0;
}
