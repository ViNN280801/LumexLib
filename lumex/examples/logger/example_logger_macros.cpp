#include <iostream>
#include <stdexcept>
#include <string>

#include "lumex/applied/logger/LumexLogger"

using namespace lumex::applied::logger::logger;

int
main ()
{
  std::cout << "=== LumexLogger macro catalog ===\n\n";

  try
    {
      LumexLogger &logger = LumexLogger::get_instance ();
      LogLevel const originalLevel = logger.get_log_level ();
      logger.set_log_level (LogLevel::LEVEL_TRACE);

      int object = 42;
      std::string const text = "macro message";

      std::cout << "--- 1. Generic LOGGER_LOG_* layer ---\n";
      LOGGER_LOG_IF (LogLevel::LEVEL_INFO, "generic conditional");
      LOGGER_LOG_TRACE ("generic trace");
      LOGGER_LOG_DEBUG ("generic debug");
      LOGGER_LOG_INFO ("generic info");
      LOGGER_LOG_SUCCESS ("generic success");
      LOGGER_LOG_WARNING ("generic warning");
      LOGGER_LOG_ERROR ("generic error");
      LOGGER_LOG_FATAL ("generic fatal");

      std::cout << "--- 2. Lumex LUMEX_LOG_* layer ---\n";
      LUMEX_LOG_TRACE ("lumex trace");
      LUMEX_LOG_DEBUG ("lumex debug");
      LUMEX_LOG_INFO ("lumex info");
      LUMEX_LOG_SUCCESS ("lumex success");
      LUMEX_LOG_WARNING ("lumex warning");
      LUMEX_LOG_ERROR ("lumex error");
      LUMEX_LOG_FATAL ("lumex fatal");

      std::cout << "--- 3. Address formatting and address macros ---\n";
      std::cout << "generic_address=" << LOGGER_ADDRESS_TO_STRING (&object)
                << " lumex_address=" << LUMEX_ADDRESS_TO_STRING (&object)
                << '\n';
      LOGGER_LOG_ADDRESS_TRACE (std::string ("generic address trace: "),
                                &object);
      LOGGER_LOG_ADDRESS_DEBUG (std::string ("generic address debug: "),
                                &object);
      LOGGER_LOG_ADDRESS_INFO (std::string ("generic address info: "),
                               &object);
      LOGGER_LOG_ADDRESS_SUCCESS (std::string ("generic address success: "),
                                  &object);
      LOGGER_LOG_ADDRESS_WARNING (std::string ("generic address warning: "),
                                  &object);
      LOGGER_LOG_ADDRESS_ERROR (std::string ("generic address error: "),
                                &object);
      LOGGER_LOG_ADDRESS_FATAL (std::string ("generic address fatal: "),
                                &object);
      LUMEX_LOG_ADDRESS_TRACE (std::string ("lumex address trace: "), &object);
      LUMEX_LOG_ADDRESS_DEBUG (std::string ("lumex address debug: "), &object);
      LUMEX_LOG_ADDRESS_INFO (std::string ("lumex address info: "), &object);
      LUMEX_LOG_ADDRESS_SUCCESS (std::string ("lumex address success: "),
                                 &object);
      LUMEX_LOG_ADDRESS_WARNING (std::string ("lumex address warning: "),
                                 &object);
      LUMEX_LOG_ADDRESS_ERROR (std::string ("lumex address error: "), &object);
      LUMEX_LOG_ADDRESS_FATAL (std::string ("lumex address fatal: "), &object);

      std::cout << "--- 4. Object macros ---\n";
      LOGGER_LOG_OBJECT_TRACE (&object, text);
      LOGGER_LOG_OBJECT_DEBUG (&object, text);
      LOGGER_LOG_OBJECT_INFO (&object, text);
      LOGGER_LOG_OBJECT_SUCCESS (&object, text);
      LOGGER_LOG_OBJECT_WARNING (&object, text);
      LOGGER_LOG_OBJECT_ERROR (&object, text);
      LOGGER_LOG_OBJECT_FATAL (&object, text);
      LUMEX_LOG_OBJECT_TRACE (&object, text);
      LUMEX_LOG_OBJECT_DEBUG (&object, text);
      LUMEX_LOG_OBJECT_INFO (&object, text);
      LUMEX_LOG_OBJECT_SUCCESS (&object, text);
      LUMEX_LOG_OBJECT_WARNING (&object, text);
      LUMEX_LOG_OBJECT_ERROR (&object, text);
      LUMEX_LOG_OBJECT_FATAL (&object, text);

      std::cout << "--- 5. DESCRIBE_FRAME-gated macros ---\n";
      LOGGER_LOG_IF_DESCRIBE_FRAME (LogLevel::LEVEL_INFO,
                                    "generic conditional frame");
      LOGGER_LOG_TRACE_DESCRIBE_FRAME ("generic frame trace");
      LOGGER_LOG_DEBUG_DESCRIBE_FRAME ("generic frame debug");
      LOGGER_LOG_INFO_DESCRIBE_FRAME ("generic frame info");
      LOGGER_LOG_SUCCESS_DESCRIBE_FRAME ("generic frame success");
      LOGGER_LOG_WARNING_DESCRIBE_FRAME ("generic frame warning");
      LOGGER_LOG_ERROR_DESCRIBE_FRAME ("generic frame error");
      LOGGER_LOG_FATAL_DESCRIBE_FRAME ("generic frame fatal");
      LUMEX_LOG_TRACE_DESCRIBE_FRAME ("lumex frame trace");
      LUMEX_LOG_DEBUG_DESCRIBE_FRAME ("lumex frame debug");
      LUMEX_LOG_INFO_DESCRIBE_FRAME ("lumex frame info");
      LUMEX_LOG_SUCCESS_DESCRIBE_FRAME ("lumex frame success");
      LUMEX_LOG_WARNING_DESCRIBE_FRAME ("lumex frame warning");
      LUMEX_LOG_ERROR_DESCRIBE_FRAME ("lumex frame error");
      LUMEX_LOG_FATAL_DESCRIBE_FRAME ("lumex frame fatal");

      logger.flush ();
      logger.set_log_level (originalLevel);
    }
  catch (std::exception const &ex)
    {
      std::cerr << "logger macro example failed: " << ex.what () << '\n';
      return 1;
    }

  std::cout << "\n=== Macro catalog finished ===\n";
  return 0;
}
