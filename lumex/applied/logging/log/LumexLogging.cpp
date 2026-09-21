#define LUMEX_IMPLEMENTATION
#include "LumexLogging.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#include "lumex/core/environment/LumexEnvironment"
#include "lumex/core/filesystem/LumexFilesystem"
#include "lumex/core/time/LumexTime"
#include "lumex/core/utility/LumexUtility"

#if LUMEX_OS_WINDOWS
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include <fstream>
#include <iomanip>
#include <iostream>

namespace lumex
{
namespace applied
{
namespace logging
{
namespace log
{
LUMEX_PUBLIC_API std::mutex LumexLogging::s_mutex;
LUMEX_PUBLIC_API lumex::path LumexLogging::s_logsDirectory;
LUMEX_PUBLIC_API std::string LumexLogging::s_launchTimestamp;
LUMEX_PUBLIC_API std::string LumexLogging::s_appName;

LUMEX_PUBLIC_API
void
LumexLogging::_log (LumexLogLevel level, std::string const &moduleName,
                    std::string const &msg)
{
  {
    std::lock_guard<std::mutex> lock (s_mutex);

    // [timestamp] | LEVEL | module : message
    if (level >= LumexLogLevel::Debug && level <= LumexLogLevel::Warning)
      {
        std::clog << _level_to_color (level) << "["
                  << LumexTime::get_current_datetime () << "] "
                  << "|" << std::setw (KLOG_WIDTH) << _level_to_string (level)
                  << "| " << moduleName << " : " << msg << "\033[0m"
                  << "\n";
      }
    else
      {
        std::cerr << _level_to_color (level) << "["
                  << LumexTime::get_current_datetime () << "] "
                  << "|" << std::setw (KLOG_WIDTH) << _level_to_string (level)
                  << "| " << moduleName << " : " << msg << "\033[0m"
                  << "\n";
      }
  } // unlock mutex, because `toFile` locking it => avoid deadlock
  toFile (KDEFAULT_LOG_FILE_NAME, level, moduleName.c_str (), msg.c_str ());
}

LUMEX_PUBLIC_API
char const *
LumexLogging::_level_to_string (LumexLogLevel level) LUMEX_NOEXCEPT
{
  switch (level)
    {
    case LumexLogLevel::Debug:
      return "DEBUG";
    case LumexLogLevel::Info:
      return "INFO";
    case LumexLogLevel::Success:
      return "SUCCESS";
    case LumexLogLevel::Warning:
      return "WARNING";
    case LumexLogLevel::Error:
      return "ERROR";
    case LumexLogLevel::Critical:
      return "CRITICAL";
    default:
      return "UNKNOWN";
    }
}

LUMEX_PUBLIC_API
char const *
LumexLogging::_level_to_color (LumexLogLevel level) LUMEX_NOEXCEPT
{
  switch (level)
    {
    case LumexLogLevel::Debug:
      return "\033[36m"; // Cyan
    case LumexLogLevel::Info:
      return "\033[37m"; // White
    case LumexLogLevel::Success:
      return "\033[32m"; // Green
    case LumexLogLevel::Warning:
      return "\033[33m"; // Yellow
    case LumexLogLevel::Error:
      return "\033[31m"; // Red
    case LumexLogLevel::Critical:
      return "\033[41m"; // Red BG
    default:
      return "\033[0m";
    }
}

LUMEX_PUBLIC_API
lumex::path
LumexLogging::getLogsDirectory ()
{
  try
    {
#if LUMEX_OS_UNIX
      std::string homeDir = LumexEnvironment::get ("HOME").value;
      if (homeDir.empty ())
        {
          std::cerr << "Error: HOME environment variable not set, cannot "
                       "determine logs directory."
                    << "\n";
          return lumex::core::filesystem::fs::lumex_filesystem::current_path ()
              .value_or (lumex::core::filesystem::fs::lumex_filesystem::
                             temp_directory_path ()
                                 .value ()); // Fallback, but likely to fail
                                             // permissions
        }

      lumex::path logsDir;
      // Check for the standard AppImage environment variable to detect if
      // running as an AppImage.
      std::string appImagePath = LumexEnvironment::get ("APPIMAGE").value;

      if (!appImagePath.empty ())
        {
          // Running in AppImage mode - store logs in a standard user data
          // directory. Prioritize XDG_DATA_HOME as per XDG Base Directory
          // Specification, otherwise fallback to ~/.local/share.
          std::string xdgDataHome
              = LumexEnvironment::get ("XDG_DATA_HOME").value;
          if (!xdgDataHome.empty ())
            {
              if (s_appName.empty ())
                logsDir = lumex::path (xdgDataHome) / lumex::path ("logs");
              else
                logsDir = lumex::path (xdgDataHome) / lumex::path (s_appName)
                          / lumex::path ("logs");
            }
          else if (s_appName.empty ())
            {
              logsDir = lumex::path (homeDir) / lumex::path (".local")
                        / lumex::path ("share") / lumex::path ("logs");
            }
          else
            {
              logsDir = lumex::path (homeDir) / lumex::path (".local")
                        / lumex::path ("share") / lumex::path (s_appName)
                        / lumex::path ("logs");
            }
        }
      else
        {
          // Default behavior for non-AppImage: use user's local data
          // directory.
          if (s_appName.empty ())
            {
              logsDir = lumex::path (homeDir) / lumex::path (".local")
                        / lumex::path ("share") / lumex::path ("logs");
            }
          else
            {
              logsDir = lumex::path (homeDir) / lumex::path (".local")
                        / lumex::path ("share") / lumex::path (s_appName)
                        / lumex::path ("logs");
            }
        }
      lumex::core::filesystem::fs::lumex_filesystem::create_directories (
          logsDir);
      return logsDir;
#else
      auto exePath
          = lumex::core::filesystem::fs::lumex_filesystem::get_exe_path ();
      auto logsDir = exePath.parent_path () / lumex::path ("logs");
      if (!s_appName.empty ())
        logsDir = logsDir / lumex::path (s_appName);
      lumex::core::filesystem::fs::lumex_filesystem::create_directories (
          logsDir);
      return logsDir;
#endif
    }
  catch (std::exception const &e)
    {
      std::cerr << "Error initializing logs directory: " << e.what () << "\n";
      return lumex::core::filesystem::fs::lumex_filesystem::current_path ()
          .value_or (lumex::core::filesystem::fs::lumex_filesystem::
                         temp_directory_path ()
                             .value ());
    }
}

LUMEX_PUBLIC_API
void
LumexLogging::setAppName (std::string const &appName)
{
  s_appName = appName;
}

LUMEX_PUBLIC_API
bool
LumexLogging::toFile (char const *filename, LumexLogLevel level,
                      char const *moduleName, char const *msg,
                      bool appendTimestamp)
{
  try
    {
      std::lock_guard<std::mutex> lock (s_mutex);
      std::string logPath = getLogsDirectory ();
      std::string fullFilename = std::string (filename);

      if (appendTimestamp)
        {
          if (s_launchTimestamp.empty ())
            s_launchTimestamp = LumexTime::get_timestamp_ns ();

          fullFilename = fullFilename + "_" + s_launchTimestamp + ".log";
        }
      else
        {
          fullFilename = fullFilename + ".log";
        }

      lumex::path logFile = lumex::path (logPath) / lumex::path (fullFilename);
      std::ofstream file (logFile.c_str (), std::ios::app);
      if (!file.is_open ())
        {
          std::cerr << "Failed to open log file: " << logFile << "\n";
          return false;
        }

      file << "[" << LumexTime::get_current_datetime () << "] "
           << "|" << std::setw (KLOG_WIDTH) << _level_to_string (level) << "| "
           << moduleName << " : " << msg << "\n";
      file.flush ();
      bool const ok = static_cast<bool> (file);
      file.close ();
      return ok;
    }
  catch (std::exception const &e)
    {
      std::cerr << "Error writing to log file: " << e.what () << "\n";
      return false;
    }
}
} // namespace log
} // namespace logging
} // namespace applied
} // namespace lumex
