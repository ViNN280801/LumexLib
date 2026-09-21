#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "lumex/applied/logger/LumexLogger"

using namespace lumex::applied::logger::logger;

int
main ()
{
  std::cout
      << "=== Workflow: buffered multi-threaded service diagnostics ===\n\n";

  try
    {
      LumexLogger &logger = LumexLogger::get_instance ();
      LogLevel const originalLevel = logger.get_log_level ();
      bool const originalBuffering = logger.is_buffering_enabled ();
      std::size_t const originalBufferSize = logger.get_buffer_size ();
      bool const originalPreset = logger.is_preset_enabled ();
      std::unordered_set<std::string> const originalComponents
          = logger.get_preset_components ();

      if (!logger.is_logging_enabled ())
        {
          std::cout
              << "Logging is disabled. To produce a real log, create "
                 "'enable_logs' next to this executable before launch.\n";
        }

      std::cout << "--- 1. Capture detailed context in a bounded buffer ---\n";
      logger.set_log_level (LogLevel::LEVEL_TRACE);
      logger.enable_buffering (6);
      logger.enable_preset ({ "NetworkManager", "Detector" });

      logger.debug ("[NetworkManager]: connect requested");
      logger.info ("[Detector]: warm-up started");
      logger.info ("[Sampler]: this non-critical component is filtered");

      std::cout << "--- 2. Log concurrently from worker threads ---\n";
      std::vector<std::thread> workers;
      for (int worker = 0; worker < 3; ++worker)
        {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif
          workers.emplace_back ([&logger, worker] {
            logger.debug ("[NetworkManager]: worker=", worker,
                          " processed one packet");
          });
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
        }
      for (std::thread &worker : workers)
        worker.join ();

      std::cout << "--- 3. Critical message bypasses preset and flushes ---\n";
      logger.warning (
          "[SafetyMonitor]: warning is logged even outside the preset");
      logger.flush ();

      logger_applied_config_view_t const config
          = logger.get_applied_config_view ();
      std::cout << "buffering="
                << (logger.is_buffering_enabled () ? "yes" : "no")
                << " size=" << logger.get_buffer_size ()
                << " configured_trigger="
                << (config.log_buffer_flush_level_configured ? "yes" : "no")
                << " trigger_level="
                << log_level_to_string (config.log_buffer_flush_trigger_level)
                << '\n';

      logger.disable_buffering ();
      if (originalPreset)
        logger.set_preset_components (originalComponents);
      else
        logger.disable_preset ();
      if (originalBuffering)
        logger.enable_buffering (originalBufferSize);
      else
        {
          logger.set_buffer_size (originalBufferSize);
          logger.disable_buffering ();
        }
      logger.set_log_level (originalLevel);
    }
  catch (std::exception const &ex)
    {
      std::cerr << "logger workflow failed: " << ex.what () << '\n';
      return 1;
    }

  std::cout << "\n=== Logger workflow finished ===\n";
  return 0;
}
