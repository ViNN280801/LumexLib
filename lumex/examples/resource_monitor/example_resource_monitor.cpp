#include <chrono>
#include <iostream>
#include <string>

#include "lumex/applied/resource_monitor/LumexResourceMonitor"

using lumex::applied::resource_monitor::monitor::LumexResourceMonitor;

int
main ()
{
  std::cout << "=== Resource monitor start / stop ===\n\n";

  std::cout << "--- 1. stop is always safe ---\n";
  LumexResourceMonitor::stop ();
  std::cout << "stop() on idle sampler ok\n";

  std::cout << "\n--- 2. startIfEnabled then stop ---\n";
  std::string const dir (".");
  LumexResourceMonitor::startIfEnabled (dir, std::chrono::milliseconds (250));
  std::cout << "startIfEnabled(\".\") returned\n";
  LumexResourceMonitor::stop ();
  std::cout << "stop() after start ok\n";

  std::cout << "\n=== Resource monitor example finished ===\n";
  return 0;
}
