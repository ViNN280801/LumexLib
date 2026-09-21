#define LUMEX_IMPLEMENTATION
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>

#include "LumexException.hpp"
#include "lumex/core/exceptions/crash/DefaultPaths.hpp"
#include "lumex/core/filesystem/LumexFilesystem"
#include "lumex/core/time/LumexTime"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace exceptions
{
namespace exception
{
LUMEX_PUBLIC_API
LumexBaseException::LumexBaseException (char const *message)
    : m_message (message), m_stacktrace (LumexStacktrace::current (1))
{
}

LUMEX_PUBLIC_API
LumexBaseException::LumexBaseException (std::string const &message)
    : m_message (message), m_stacktrace (LumexStacktrace::current (1))
{
}

LUMEX_PUBLIC_API
LumexBaseException::LumexBaseException (std::string &&message)
    : m_message (std::move (message)),
      m_stacktrace (LumexStacktrace::current (1))
{
}

#if __cplusplus >= 201703L
LUMEX_PUBLIC_API
LumexBaseException::LumexBaseException (std::string_view message)
    : m_message (message), m_stacktrace (LumexStacktrace::current (1))
{
}
#endif

LUMEX_PUBLIC_API
void
LumexBaseException::to_stderr () const LUMEX_NOEXCEPT
{
  std::cerr << "[" << lumDemangle (*this) << "]:" << what () << "\n";
}

LUMEX_PUBLIC_API
void
LumexBaseException::to_crash_report () const
{
  // Single initialization: creating a folder, a file and locking the directory
  static std::once_flag initFlag;
  static std::string s_reportFile;
  static std::mutex s_fileMutex;
  static lumex::path s_crashDir;

  std::call_once (initFlag, [] () {
    // 1. Get the executable path to create a folder
    // 'KDEFAULT_CRASHES_DIR_PATH' in the same directory
    auto exePath
        = lumex::core::filesystem::fs::lumex_filesystem::get_exe_path ();
    auto exeDir = exePath.parent_path ();

    // 2. Create a folder KDEFAULT_CRASHES_DIR_PATH if it doesn't exist
    s_crashDir = exeDir / KDEFAULT_CRASHES_DIR_PATH;
    lumex::core::filesystem::fs::lumex_filesystem::create_directory (
        s_crashDir);

    // 3. Generate a file name once
    auto tsEpoch = LumexTime::get_timestamp_ns ();
    s_reportFile
        = (s_crashDir / ("crash_report_" + tsEpoch + ".txt")).string ();

    // 4. Lock the directory from deletion during operation
    lumex::core::filesystem::fs::lumex_filesystem::lock_directory (s_crashDir);
  });

  // 5. Form the text of the report
  std::ostringstream oss;
  oss << "\n========== Crash Report ==========\n"
      << "Time         : " << LumexTime::get_current_datetime ()
      << "\nMessage    : " << what () << "\nStack trace:\n";
  for (std::size_t i = 0; i < m_stacktrace.size (); ++i)
    {
      auto const &entry = m_stacktrace[i];
      oss << " #" << i << " " << entry.description () << "\n";
    }

  // 6. Append to the file safely
  std::lock_guard<std::mutex> lock (s_fileMutex);
  std::ofstream file (s_reportFile.c_str (), std::ios::app);
  file << oss.str ();
  file.flush (); // Ensure data is written to disk immediately
}

LUMEX_PUBLIC_API
LUMEX_ATTRIBUTE_NOINLINE
LumexStacktrace
LumexException_GetStackTraceTrampoline (int skip_frames)
{
  // This function acts as a trampoline to get a consistent stack trace.
  // It skips its own frame (the trampoline) and then adjusts for the requested
  // skip.
  return LumexStacktrace::current (
      skip_frames + 1); // +1 to skip this trampoline function itself
}
} // namespace exception
} // namespace exceptions
} // namespace core
} // namespace lumex
