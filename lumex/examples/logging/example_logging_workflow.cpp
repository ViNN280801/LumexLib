#include <iostream>

#include "lumex/applied/logging/LumexLogging"

using namespace lumex::applied::logging::log;

int
main ()
{
  std::cout << "=== Workflow: module-tagged run log ===\n\n";

  LumexLogging::set_app_name ("LumexLoggingWorkflow");
  LumexLogging::info ("pump", "flow set to ", 1.0, " ml/min");
  LumexLogging::info ("detector", "wavelength=", 254, " nm");
  LumexLogging::warning ("autosampler", "vial 12 missing");
  std::cout << "logs_dir=" << LumexLogging::get_logs_directory ().string ()
            << '\n';
  return 0;
}
