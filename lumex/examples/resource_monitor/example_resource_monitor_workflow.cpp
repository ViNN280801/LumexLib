#include <chrono>
#include <iostream>
#include <string>

#include "lumex/applied/resource_monitor/LumexResourceMonitor"

using lumex::applied::resource_monitor::monitor::LumexResourceMonitor;

int
main ()
{
  std::cout << "=== Workflow: sample once around a dummy run ===\n\n";

  LumexResourceMonitor::start_if_enabled (std::string ("."),
                                          std::chrono::milliseconds (500));
  std::cout << "run placeholder\n";
  LumexResourceMonitor::stop ();
  return 0;
}
