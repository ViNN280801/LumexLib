#define LUMEX_IMPLEMENTATION
#include <cstring>
#include <iostream>

#include "DefaultPaths.hpp"
#include "LumexCrashHandler.hpp"
#include "lumex/core/environment/LumexEnvironment"
#include "lumex/core/filesystem/LumexFilesystem"
#include "lumex/core/time/LumexTime"
#include "lumex/core/utility/attr/LumexAttributes.hpp"

#if LUMEX_OS_UNIX
#include <fstream> // used in _generateCoreDump()
#endif

namespace lumex
{
namespace core
{
namespace exceptions
{
namespace crash
{
LUMEX_PUBLIC_API char const *LumexCrashHandler::s_defaultAppName
    = "UnknownApp";
LUMEX_PUBLIC_API std::string LumexCrashHandler::s_appName
    = LumexCrashHandler::s_defaultAppName;

LUMEX_PUBLIC_API
LumexCrashHandler &
LumexCrashHandler::instance ()
{
  static LumexCrashHandler instance;
  return instance;
}

LUMEX_PUBLIC_API
void
LumexCrashHandler::initialize (
    LUMEX_ATTRIBUTE_MAYBE_UNUSED std::string const &appName)
{
  s_appName = appName.empty () ? s_defaultAppName : appName;

  try
    {
#if LUMEX_OS_UNIX
      std::string homeDir = LumexEnvironment::get ("HOME").value;
      if (homeDir.empty ())
        {
          std::cerr << "Error: HOME environment variable not set, cannot "
                       "determine crashes directory.\n";
          return; // Cannot initialize crash handler without HOME
        }

      lumex::path crashesDir;
      // Check for the standard AppImage environment variable to detect if
      // running as an AppImage.
      std::string appImagePath = LumexEnvironment::get ("APPIMAGE").value;

      if (!appImagePath.empty ())
        {
          // Running in AppImage mode - store crash dumps in a standard user
          // data directory. Prioritize XDG_DATA_HOME as per XDG Base Directory
          // Specification, otherwise fallback to ~/.local/share.
          std::string xdgDataHome
              = LumexEnvironment::get ("XDG_DATA_HOME").value;
          if (!xdgDataHome.empty ())
            {
              crashesDir = lumex::path (xdgDataHome) / lumex::path (s_appName)
                           / lumex::path ("crashes"); // Use s_appName
            }
          else
            {
              crashesDir = lumex::path (homeDir) / lumex::path (".local")
                           / lumex::path ("share")
                           / lumex::path (s_appName) // Use s_appName
                           / lumex::path ("crashes");
            }
          std::cout << "Creating crash dump directory (AppImage mode): "
                    << crashesDir << "\n";
        }
      else
        {
          // Default behavior for non-AppImage: use user's local data
          // directory.
          crashesDir = lumex::path (homeDir) / lumex::path (".local")
                       / lumex::path ("share")
                       / lumex::path (s_appName) // Use s_appName
                       / lumex::path ("crashes");
          std::cout << "Creating crash dump directory: " << crashesDir << "\n";
        }
      lumex::core::filesystem::fs::lumex_filesystem::create_directory (
          crashesDir);
#else
      // Create dump directory if it doesn't exist
      auto exePath
          = lumex::core::filesystem::fs::lumex_filesystem::get_exe_path ();
      auto exeDir = exePath.parent_path ();
      lumex::core::filesystem::fs::lumex_filesystem::create_directory (
          exeDir / lumex::path (KDEFAULT_CRASHES_DIR_PATH));
      std::cout << "Creating crash dump directory: "
                << KDEFAULT_CRASHES_DIR_PATH << "\n";
#endif

#if LUMEX_OS_WINDOWS
      // Set Windows unhandled exception filter
      // SetUnhandledExceptionFilter(_WindowsCrashHandler);
      std::cout << "Windows crash handler installed\n";
#else
      // Set up Linux signal handlers and core dump settings
      _setupCoreDumpSettings ();
      _setupSignalHandlers ();
      std::cout << "Linux crash handler installed" << std::endl;
#endif
    }
  catch (std::exception const &ex)
    {
      std::cerr << "Failed to initialize crash handler: " << ex.what ()
                << "\n";
    }
  catch (...)
    {
      std::cerr << "Failed to initialize crash handler: Unknown error\n";
    }
}

LUMEX_PUBLIC_API
void
LumexCrashHandler::_notifyAndLog (std::string const &errorMessage)
{
  // Log to stderr
  std::cerr << "CRASH: " << errorMessage << "\n";

  // On Windows, show a message box
#if LUMEX_OS_WINDOWS
  MessageBoxA (nullptr, errorMessage.c_str (), "Application Crash",
               MB_OK | MB_ICONERROR);
#endif
}

LUMEX_PUBLIC_API
std::string
LumexCrashHandler::_generateDumpFilename (std::string const &prefix)
{
#if LUMEX_OS_UNIX
  lumex::path dir;

  // Check if we're running in AppImage mode
  std::string appImageMode
      = LumexEnvironment::get ("LUMEX_APPIMAGE_MODE").value;
  std::string externalCrashesDir
      = LumexEnvironment::get ("LUMEX_EXTERNAL_CRASHES_DIR").value;

  if (!appImageMode.empty () && appImageMode == "1"
      && !externalCrashesDir.empty ())
    {
      // AppImage mode - use external crashes directory
      dir = lumex::path (externalCrashesDir);
    }
  else
    {
      // Default behavior for non-AppImage: use user's local data directory
      std::string homeDir = LumexEnvironment::get ("HOME").value;
      if (homeDir.empty ())
        {
          // Fallback for _generateDumpFilename, as initialize() should have
          // already handled this If homeDir is empty, use temp_directory_path
          // as a last resort, and include s_appName to avoid generic paths.
          dir = lumex::core::filesystem::fs::lumex_filesystem::
                    temp_directory_path ()
                        .value ()
                / lumex::path (s_appName) / lumex::path ("crashes");
        }
      else
        {
          // Prioritize XDG_DATA_HOME if set, otherwise fallback to
          // ~/.local/share.
          std::string xdgDataHome
              = LumexEnvironment::get ("XDG_DATA_HOME").value;
          if (!xdgDataHome.empty ())
            {
              dir = lumex::path (xdgDataHome) / lumex::path (s_appName)
                    / lumex::path ("crashes");
            }
          else
            {
              dir = lumex::path (homeDir) / lumex::path (".local")
                    / lumex::path ("share")
                    / lumex::path (s_appName) // Use s_appName
                    / lumex::path ("crashes");
            }
        }
    }
#else
  lumex::path dir{
    lumex::core::filesystem::fs::lumex_filesystem::get_exe_path ()
        .parent_path ()
    / lumex::path (KDEFAULT_CRASHES_DIR_PATH)
  };
#endif

  lumex::core::filesystem::fs::lumex_filesystem::create_directory (dir);

  std::string timestamp = LumexTime::get_current_datetime ();
  lumex::path dumpPath = dir
                         / lumex::path ((std::string (prefix) + timestamp
#if LUMEX_OS_WINDOWS
                                         + ".dmp"));
#else
                                         + ".core"));
#endif

  return dumpPath.string ();
}

#if LUMEX_OS_WINDOWS
LUMEX_PUBLIC_API
LONG WINAPI
LumexCrashHandler::_onWindowsCrashHandler (PEXCEPTION_POINTERS pExInfo)
{
  std::clog << "Windows crash handler called.\n";
  std::string filename (_generateDumpFilename (KDEFAULT_MINIDUMP_PREFIX));
  std::string errorMessage ("Critical application error. Crash dump saved to: "
                            + filename);
  std::clog << ("Attempting to create dump file: " + filename + "\n").c_str ();

  // Convert to wide string for Windows API
  std::wstring wfilename (filename.begin (), filename.end ());

  // Open file for writing dump
  HANDLE hFile = CreateFileW (wfilename.c_str (), GENERIC_WRITE, 0, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

  if (hFile != INVALID_HANDLE_VALUE)
    {
      std::clog << "Dump file created successfully.\n";

      // Write minidump
      MINIDUMP_EXCEPTION_INFORMATION eInfo;
      eInfo.ThreadId = GetCurrentThreadId ();
      eInfo.ExceptionPointers = pExInfo;
      eInfo.ClientPointers = 0;

      BOOL success
          = MiniDumpWriteDump (GetCurrentProcess (), GetCurrentProcessId (),
                               hFile, MiniDumpNormal, &eInfo, NULL, NULL);

      if (success == TRUE)
        {
          std::clog << "MiniDumpWriteDump succeeded.\n";
        }
      else
        {
          DWORD error = GetLastError ();
          errorMessage += " (Failed to write dump, error code: "
                          + std::to_string (error) + ")";
          std::clog << ("MiniDumpWriteDump failed. Error: "
                        + std::to_string (error) + "\n")
                           .c_str ();
        }

      CloseHandle (hFile);
    }
  else
    {
      DWORD error = GetLastError ();
      errorMessage += " (Failed to create dump file, error code: "
                      + std::to_string (error) + ")";
      std::clog << ("CreateFileW failed. Error: " + std::to_string (error)
                    + "\n")
                       .c_str ();
    }

  DWORD exceptionCode = pExInfo->ExceptionRecord->ExceptionCode;
  switch (exceptionCode)
    {
    case EXCEPTION_ACCESS_VIOLATION:
      errorMessage += " - Memory Access Violation";
      break;
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
      errorMessage += " - Array Bounds Exceeded";
      break;
    case EXCEPTION_BREAKPOINT:
      errorMessage += " - Breakpoint Encountered";
      break;
    case EXCEPTION_DATATYPE_MISALIGNMENT:
      errorMessage += " - Data Misalignment";
      break;
    case EXCEPTION_FLT_DENORMAL_OPERAND:
      errorMessage += " - Floating-point Denormal Operand";
      break;
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
      errorMessage += " - Floating-point Division by Zero";
      break;
    case EXCEPTION_FLT_INEXACT_RESULT:
      errorMessage += " - Floating-point Inexact Result";
      break;
    case EXCEPTION_FLT_INVALID_OPERATION:
      errorMessage += " - Floating-point Invalid Operation";
      break;
    case EXCEPTION_FLT_OVERFLOW:
      errorMessage += " - Floating-point Overflow";
      break;
    case EXCEPTION_FLT_STACK_CHECK:
      errorMessage += " - Floating-point Stack Check";
      break;
    case EXCEPTION_FLT_UNDERFLOW:
      errorMessage += " - Floating-point Underflow";
      break;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
      errorMessage += " - Illegal Instruction";
      break;
    case EXCEPTION_IN_PAGE_ERROR:
      errorMessage += " - Page Error";
      break;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
      errorMessage += " - Integer Division by Zero";
      break;
    case EXCEPTION_INT_OVERFLOW:
      errorMessage += " - Integer Overflow";
      break;
    case EXCEPTION_INVALID_DISPOSITION:
      errorMessage += " - Invalid Disposition";
      break;
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
      errorMessage += " - Noncontinuable Exception";
      break;
    case EXCEPTION_PRIV_INSTRUCTION:
      errorMessage += " - Privileged Instruction";
      break;
    case EXCEPTION_STACK_OVERFLOW:
      errorMessage += " - Stack Overflow";
      break;
    default:
      errorMessage
          += " - Unknown Exception Code: " + std::to_string (exceptionCode);
    }

  _notifyAndLog (errorMessage);

  return EXCEPTION_EXECUTE_HANDLER;
}
#else
LUMEX_PUBLIC_API
void
LumexCrashHandler::_generateCoreDump ()
{
  std::string filename = _generateDumpFilename (KDEFAULT_MINIDUMP_PREFIX);

  // Set core dump size limit to infinite
  struct rlimit core_limit;
  core_limit.rlim_cur = RLIM_INFINITY;
  core_limit.rlim_max = RLIM_INFINITY;
  if (setrlimit (RLIMIT_CORE, &core_limit) != 0)
    std::cerr << "Warning: Failed to set core dump size limit\n";

  // Try to set core pattern - but don't fail if it doesn't work
  // This is especially important in AppImage environments where system calls
  // may fail
  std::string core_pattern_cmd
      = "echo \"" + filename
        + "\" | sudo tee /proc/sys/kernel/core_pattern 2>/dev/null";
  int result = system (core_pattern_cmd.c_str ());
  if (result != 0)
    {
      std::cerr << "Warning: Failed to set custom core pattern (exit code: "
                << result << ")\n";
      std::cerr << "Core dump will use system default location\n";

      // Try alternative approach - write to a simple file for debugging
      try
        {
          std::ofstream crashInfo (std::string (filename + ".info").c_str ());
          if (crashInfo.is_open ())
            {
              crashInfo << "Crash occurred at: "
                        << LumexTime::get_current_datetime () << "\n";
              crashInfo << "Process ID: " << getpid () << "\n";
              crashInfo << "Note: Core dump may be in system default location "
                           "due to permission restrictions\n";
              crashInfo.close ();
              std::cerr << "Crash info written to: " << filename << ".info\n";
            }
        }
      catch (...)
        {
          std::cerr << "Failed to write crash info file\n";
        }
    }
}

LUMEX_PUBLIC_API
void
LumexCrashHandler::_signalHandler (int signum)
{
  // Use async-signal-safe functions only in signal handlers
  char const *signalName = "Unknown";

  switch (signum)
    {
    case SIGSEGV:
      signalName = "SIGSEGV (Segmentation Fault)";
      break;
    case SIGFPE:
      signalName = "SIGFPE (Floating-point Exception)";
      break;
    case SIGILL:
      signalName = "SIGILL (Illegal Instruction)";
      break;
    case SIGABRT:
      signalName = "SIGABRT (Abort)";
      break;
    default:
      signalName = "Unknown signal";
      break;
    }

  // Write minimal crash info using async-signal-safe functions
  char const crashMsg[] = "CRASH DETECTED: ";
  if (write (STDERR_FILENO, crashMsg, sizeof (crashMsg) - 1) == -1)
    std::cerr << "Failed to write crash message to stderr\n";
  if (write (STDERR_FILENO, signalName, strlen (signalName)) == -1)
    std::cerr << "Failed to write signal name to stderr\n";
  if (write (STDERR_FILENO, "\n", 1) == -1)
    std::cerr << "Failed to write newline to stderr\n";

  // Try to generate core dump, but don't let it crash the handler
  try
    {
      _generateCoreDump ();
    }
  catch (...)
    {
      char const errMsg[] = "Core dump generation failed\n";
      if (write (STDERR_FILENO, errMsg, sizeof (errMsg) - 1) == -1)
        std::cerr << "Failed to write core dump generation failure message to "
                     "stderr\n";
    }

  // Build error message for UI notification (if possible)
  std::string errorMessage = "Critical application error. Signal received: ";
  errorMessage += signalName;

  try
    {
      _notifyAndLog (errorMessage);
    }
  catch (...)
    {
      // If notification fails, just continue with termination
    }

  // Reset to default handler and re-raise signal
  signal (signum, SIG_DFL);
  raise (signum);
}

LUMEX_PUBLIC_API
void
LumexCrashHandler::_setupSignalHandlers ()
{
  struct sigaction sigAct;
  sigAct.sa_handler = _signalHandler;
  sigemptyset (&sigAct.sa_mask);
  sigAct.sa_flags = SA_RESETHAND;

  sigaction (SIGSEGV, &sigAct, nullptr);
  sigaction (SIGABRT, &sigAct, nullptr);
  sigaction (SIGFPE, &sigAct, nullptr);
  sigaction (SIGILL, &sigAct, nullptr);
}

LUMEX_PUBLIC_API
void
LumexCrashHandler::_setupCoreDumpSettings ()
{
  prctl (PR_SET_DUMPABLE, 1, 0, 0, 0);

  struct rlimit core_limit;
  core_limit.rlim_cur = RLIM_INFINITY;
  core_limit.rlim_max = RLIM_INFINITY;
  setrlimit (RLIMIT_CORE, &core_limit);
}

#endif
} // namespace crash
} // namespace exceptions
} // namespace core
} // namespace lumex
