#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>

#include "lumex/applied/logger/LumexLogger"

using namespace lumex::applied::logger::logger;

int
main ()
{
  std::cout << "=== LumexLogger public API walkthrough ===\n\n";

  try
    {
      LumexLogger &logger = LumexLogger::get_instance ();
      LumexLogger &sameLogger = LumexLogger::get_instance ();

      LogLevel const originalLevel = logger.get_log_level ();
      bool const originalBuffering = logger.is_buffering_enabled ();
      std::size_t const originalBufferSize = logger.get_buffer_size ();
      bool const originalPreset = logger.is_preset_enabled ();
      std::unordered_set<std::string> const originalComponents
          = logger.get_preset_components ();
      std::string const originalTrigger = logger.get_trigger_file_name ();

      std::cout << "--- 1. Value types and helpers ---\n";
      logger_config_t config (LogLevel::LEVEL_DEBUG, FunctionNameMode::NORMAL,
                              true, false, 12, false,
                              { "NetworkManager", "Detector" });
      log_entry_t entry (LogLevel::LEVEL_INFO, "buffered message",
                         std::chrono::system_clock::now ());
      std::cout << "config level=" << log_level_to_string (config.log_level)
                << " function_mode="
                << static_cast<int> (config.func_name_mode)
                << " preset_count=" << config.preset_components.size ()
                << " entry=" << entry.message << '\n';
      std::cout << "stringify="
                << logger_stringify ("value=", 42, " enabled=", true) << '\n';
      std::cout << "clean_name="
                << logger_format_function_name_clean (
                       "bool __cdecl Example::run(int, char const *)")
                << " thread_id=" << logger_format_current_thread_id () << '\n';

      std::cout << "\n--- 2. Singleton and observers ---\n";
      std::cout << "same_instance="
                << (std::addressof (logger) == std::addressof (sameLogger)
                        ? "yes"
                        : "no")
                << '\n';
      std::cout << "enabled=" << (logger.is_logging_enabled () ? "yes" : "no")
                << " level=" << static_cast<int> (logger.get_log_level ())
                << " function_mode="
                << static_cast<int> (logger.get_function_name_mode ())
                << " show_function="
                << (logger.get_function_name_mode () != FunctionNameMode::NONE
                        ? "yes"
                        : "no")
                << " describe_frame="
                << (logger.is_describe_frame_enabled () ? "yes" : "no")
                << '\n';
      std::cout << "trace_enabled="
                << (logger.is_level_enabled (LogLevel::LEVEL_TRACE) ? "yes"
                                                                    : "no")
                << " log_path=" << logger.get_log_file_path ()
                << " trigger=" << logger.get_trigger_file_name () << '\n';

      std::cout << "\n--- 3. Direct logging API ---\n";
      logger.set_log_level (LogLevel::LEVEL_TRACE);
      logger.trace ("trace message ", 1);
      logger.debug ("debug message ", 2);
      logger.info ("info message ", 3);
      logger.success ("success message ", 4);
      logger.warning ("warning message ", 5);
      logger.error ("error message ", 6);
      logger.fatal ("fatal message ", 7);
      logger.log (LogLevel::LEVEL_INFO, "low-level log(level, message)");

      std::cout << "\n--- 4. Buffering API ---\n";
      logger.enable_buffering (8);
      logger.set_buffer_size (4);
      std::cout << "buffering="
                << (logger.is_buffering_enabled () ? "yes" : "no")
                << " size=" << logger.get_buffer_size () << '\n';
      logger.info ("buffered record");
      logger.flush ();
      logger.disable_buffering ();

      std::cout << "\n--- 5. Preset API ---\n";
      logger.enable_preset ({ "NetworkManager", "Detector" });
      logger.set_preset_components ({ "Storage", "Transport" });
      std::cout << "preset=" << (logger.is_preset_enabled () ? "yes" : "no")
                << " components=" << logger.get_preset_components ().size ()
                << '\n';
      logger.disable_preset ();

      std::cout << "\n--- 6. Trigger name and applied config ---\n";
      logger.set_trigger_file_name ("enable_logs.example");
      logger_applied_config_view_t const view
          = logger.get_applied_config_view ();
      std::cout << "trigger=" << logger.get_trigger_file_name ()
                << " timestamped="
                << (view.use_timestamped_logs ? "yes" : "no") << " stacktrace="
                << (view.show_stack_trace_in_messages ? "yes" : "no")
                << " frames=" << view.stack_trace_max_frames << " flush_level="
                << log_level_to_string (view.log_buffer_flush_trigger_level)
                << '\n';

      std::cout << "\n--- 7. Developer hint ---\n";
      logger.print_logger_hint ();

      logger.flush ();

      logger.set_log_level (originalLevel);
      logger.set_trigger_file_name (originalTrigger);
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
    }
  catch (std::exception const &ex)
    {
      std::cerr << "logger example failed: " << ex.what () << '\n';
      return 1;
    }

  std::cout << "\n=== Public API walkthrough finished ===\n";
  return 0;
}
