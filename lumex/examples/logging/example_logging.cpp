#include <iostream>

#include "lumex/applied/logging/LumexLogging"

using namespace lumex::applied::logging::log;

int
main ()
{
  std::cout << "=== LumexLogging console + file ===\n\n";

  LumexLogging::setAppName ("LumexLoggingExample");

  std::cout << "--- 1. Severity helpers ---\n";
  LumexLogging::debug ("example", "debug probe");
  LumexLogging::info ("example", "hello from LumexLogging");
  LumexLogging::success ("example", "self-test passed");
  LumexLogging::warning ("example", "buffer nearly full");
  LumexLogging::error ("example", "retrying serial open");
  LumexLogging::critical ("example", "abort sequence");

  std::cout << "\n--- 2. Logs directory ---\n";
  std::cout << "logs_dir=" << LumexLogging::getLogsDirectory ().string ()
            << '\n';

  std::cout << "\n--- 3. toFile ---\n";
  bool const wrote = LumexLogging::toFile (
      "lumex_logging_example", LumexLogLevel::Info, "example",
      "file sink line", /*appendTimestamp=*/true);
  std::cout << "toFile ok=" << (wrote ? "yes" : "no") << '\n';

  std::cout << "\n=== Logging example finished ===\n";
  return 0;
}
