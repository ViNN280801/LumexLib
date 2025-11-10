#ifndef LUMEX_UTILITIES_DEBUG_HPP
#define LUMEX_UTILITIES_DEBUG_HPP

#include "LumexCheckOS.hpp"

#include <cstdio>
#include <string>

#if LUMEX_OS_WINDOWS
  #include <windows.h>

  #include <dbghelp.h>

  #include <mutex>
#else
  #include <array>
  #include <unistd.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
  #if __has_include(<execinfo.h>)
    #include <execinfo.h>
  #endif
  #if __has_include(<dlfcn.h>)
    #include <dlfcn.h>
  #endif
  #if __has_include(<cxxabi.h>)
    #include <cxxabi.h>
  #endif
#endif

#include "lumex/core/string/utility/LumexStringify.hpp"

namespace Lumex
{
  namespace Utility
  {
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay, cppcoreguidelines-avoid-c-arrays,
    // cppcoreguidelines-pro-bounds-constant-array-index, cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)
    namespace Debug
    {
      namespace Detail
      {
#if (defined(__GNUC__) || defined(__clang__)) && LUMEX_OS_LINUX
        // Resolve address to function name using addr2line (POSIX fallback)
        static inline std::string
        resolveAddressWithAddr2line(void *addr, char const *exe_path) noexcept
        {
          try
          {
            // Create pipe for addr2line output
            std::array<char, 512> buffer{};
            std::string result;

            // Build command: addr2line -C -f -e <executable> <address>
            std::string cmd
              = fmt::format("addr2line -C -f -e {} 0x{:X} 2>/dev/null", exe_path, reinterpret_cast<uintptr_t>(addr));

            FILE *pipe = popen(cmd.c_str(), "r"); // NOLINT(cppcoreguidelines-owning-memory)
            if(!pipe) return "";

            // Read function name (first line)
            if(fgets(buffer.data(), buffer.size(), pipe) != nullptr)
            {
              result = buffer.data();
              // Remove newline
              if(!result.empty() && result.back() == '\n') result.pop_back();

              // Read file:line (second line) and append
              if(fgets(buffer.data(), buffer.size(), pipe) != nullptr)
              {
                std::string location = buffer.data();
                if(!location.empty() && location.back() == '\n') location.pop_back();

                // Filter out "??:0" and "??:?"
                if(location != "??:0" && location != "??:?") result += " at " + location;
              }
            }

            pclose(pipe); // NOLINT(cppcoreguidelines-owning-memory)

            // Return empty if addr2line failed (contains "?" or "0x")
            if(result.find("??") != std::string::npos || result.empty()) return "";

            return result;
          }
          catch(...)
          {
            return "";
          }
        }

        // Get executable path for addr2line
        static inline std::string
        getExecutablePath() noexcept
        {
          try
          {
  #if LUMEX_OS_LINUX
            std::array<char, 4096> buffer{};
            ssize_t len = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
            if(len > 0)
            {
              buffer[static_cast<size_t>(len)] = '\0';
              return std::string(buffer.data());
            }
  #endif
            return "";
          }
          catch(...)
          {
            return "";
          }
        }
#endif

#if LUMEX_OS_WINDOWS
        // Thread-safe DbgHelp mutex (Windows requires external synchronization)
        static inline std::mutex &
        getDbgHelpMutex() noexcept
        {
          static std::mutex mtx;
          return mtx;
        }

        // Lazy initialization for SymInitialize (amortize overhead)
        static inline bool
        ensureSymbolsInitialized(HANDLE process) noexcept
        {
          static std::atomic_bool initialized{false};
          static std::once_flag init_flag;

          if(initialized.load(std::memory_order_acquire)) return true;

          std::call_once(init_flag,
                         [process]()
                         {
                           std::lock_guard<std::mutex> lock(getDbgHelpMutex());
                           if(SymInitialize(process, nullptr, TRUE))
                           {
                             SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
                             initialized.store(true, std::memory_order_release);
                           }
                         });

          return initialized.load(std::memory_order_acquire);
        }
#elif defined(__GNUC__) || defined(__clang__)
        // Demangle C++ symbols on POSIX systems
        static inline std::string
        demangleSymbol(char const *mangled) noexcept
        {
          if(!mangled) return "??";

          int status       = -1;
          char *demangled_ = nullptr;

  #if __has_include(<cxxabi.h>)
          demangled_ = abi::__cxa_demangle(mangled, nullptr, nullptr, std::addressof(status));
  #endif
          std::string result = (status == 0 && demangled_) ? demangled_ : mangled;

          if(demangled_) free(demangled_); // NOLINT(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)

          return result;
        }
#endif
      } // namespace Detail

      /**
       * @brief Capture current call stack in a formatted string.
       *
       * @details
       * Windows (MSVC):
       * - Uses DbgHelp API (CaptureStackBackTrace + SymFromAddr)
       * - Requires PDB files for function names
       * - Thread-safe through internal mutex
       * - Lazy initialization to minimize overhead
       *
       * Linux/POSIX (GCC/Clang):
       * - Uses GNU backtrace() + backtrace_symbols() (not POSIX, but widely available)
       * - Automatic demangling C++ symbols through __cxa_demangle
       * - Requires compilation with -g and linking with -rdynamic
       * - Fallback on dladdr() when backtrace is not available
       *
       * Performance:
       * - Debug mode: ~150-400 μs on 10 frames
       * - Release with symbols: ~100-250 μs
       * - Release without symbols: ~50-100 μs (only addresses)
       *
       * Graceful Degradation:
       * - Without debug symbols: returns addresses in hex format
       * - On errors: returns "Stack trace unavailable: <reason>"
       * - Thread-safe in all modes
       *
       * @param skip_frames Number of frames to skip (default 1 - itself captureStackTrace)
       * @param max_frames Maximum number of frames to capture (default 16)
       *
       * @return std::string Formatted stack trace, each frame on a new line.
       *         Format: "  #N: function_name (file:line) [0xADDRESS]"
       *
       * @note
       * - Function noexcept - never throws exceptions
       * - Can allocate memory inside (NOT async-signal-safe!)
       * - Optimized for rare calls (logging errors)
       * - DO NOT use in signal handlers or hot paths
       *
       * @warning
       * - Windows: Requires DbgHelp.lib in linking
       * - Linux: Requires -rdynamic for symbol export
       * - Inline functions may be missing in trace at aggressive optimization
       *
       * @example
       * @code
       * void setConfigToDefault() {
       *   std::string trace = Lumex::Utility::Debug::captureStackTrace(1, 5);
       *   lumWarning("Config reset. Call stack:\n", trace);
       *   // Output:
       *   //   #0: initChannelAmqpErrors() at ChromatographicController.cpp:140 [0x7FF6A2B41234]
       *   //   #1: CMChromatographicController::CMChromatographicController() [0x7FF6A2B3F890]
       *   //   #2: ... etc
       * }
       * @endcode
       */
      static inline std::string
      captureStackTrace(int skip_frames = 1, int max_frames = 16) noexcept
      {
        constexpr int kMaxStackFrames = 64;
        std::string result;

        try
        {
#if LUMEX_OS_WINDOWS
          // Windows: DbgHelp API for symbol information
          void *stack[kMaxStackFrames];

          HANDLE process = GetCurrentProcess();

          // Thread-safe symbol initialization (lazy, once)
          if(!Detail::ensureSymbolsInitialized(process))
          {
            // Fallback: only addresses without symbols
            WORD frames
              = CaptureStackBackTrace(static_cast<DWORD>(skip_frames), static_cast<DWORD>(max_frames), stack, nullptr);

            for(WORD i = 0; i < frames; ++i)
              result += stringify("  #", i, ": [0x", reinterpret_cast<uintptr_t>(stack[i]), "]\n");
            return result.empty() ? "  Stack trace unavailable (SymInitialize failed)\n" : result;
          }

          // Capture stack with full symbol resolution
          WORD frames
            = CaptureStackBackTrace(static_cast<DWORD>(skip_frames), static_cast<DWORD>(max_frames), stack, nullptr);

          if(frames == 0) return "  Stack trace empty (no frames captured)\n";

          // Allocate symbol buffer (variable size structure)
          constexpr size_t kSymbolBufferSize = sizeof(SYMBOL_INFO) + (MAX_SYM_NAME * sizeof(TCHAR));
          auto *symbol_buffer                = static_cast<SYMBOL_INFO *>(malloc(kSymbolBufferSize));

          if(symbol_buffer == nullptr) return "  Stack trace unavailable (memory allocation failed)\n";

          symbol_buffer->MaxNameLen   = MAX_SYM_NAME;
          symbol_buffer->SizeOfStruct = sizeof(SYMBOL_INFO);

          // Thread-safe symbol resolution
          std::lock_guard<std::mutex> lock(Detail::getDbgHelpMutex());

          for(WORD i = 0; i < frames; ++i)
          {
            DWORD64 address = reinterpret_cast<DWORD64>(stack[i]);

            // Get function name
            DWORD64 displacement = 0;
            if(SymFromAddr(process, address, &displacement, symbol_buffer))
            {
              // Try to get line number and file name
              IMAGEHLP_LINE64 line{};
              line.SizeOfStruct       = sizeof(IMAGEHLP_LINE64);
              DWORD line_displacement = 0;

              if(SymGetLineFromAddr64(process, address, &line_displacement, &line))
              {
                // Full info: function + file:line + address
                result += stringify("  #", i, ": ", symbol_buffer->Name, " (", line.FileName, ":", line.LineNumber,
                                    ") [0x", address, "]\n");
              }
              else
              {
                // Only function name + address (no line info)
                result += stringify("  #", i, ": ", symbol_buffer->Name, " [0x", address, "]\n");
              }
            }
            else
            {
              // Fallback: only address
              result += stringify("  #", i, ": [0x", address, "]\n");
            }
          }

          free(symbol_buffer); // NOLINT(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)

#elif defined(__GNUC__) || defined(__clang__)
            // Linux/POSIX: backtrace() + dladdr() for symbols
            // NOTE: backtrace() - GNU extension, not in POSIX.1-2024, but widely available

  #if __has_include(<execinfo.h>)
          void *addresses[kMaxStackFrames];

          int frame_count = backtrace(addresses, kMaxStackFrames);

          if(frame_count <= skip_frames) return "  Stack trace empty (insufficient frames)\n";

          int actual_frames = std::min(frame_count - skip_frames, max_frames);

          // Use backtrace_symbols for quick resolution (allocates memory)
          char **symbols = backtrace_symbols(addresses + skip_frames, actual_frames);

          if(!symbols)
          {
            // Fallback: dladdr() for each address
            for(int i = 0; i < actual_frames; ++i)
            {
              Dl_info info;
              if(dladdr(addresses[i + skip_frames], &info))
              {
                std::string demangled = Detail::demangleSymbol(info.dli_sname);
                result += stringify("  #", i, ": ", demangled, " [0x",
                                    reinterpret_cast<uintptr_t>(addresses[i + skip_frames]), "]\n");
              }
              else
              {
                result += stringify("  #", i, ": [0x", reinterpret_cast<uintptr_t>(addresses[i + skip_frames]), "]\n");
              }
            }
            return result;
          }

          // Get executable path for addr2line fallback
          static std::string const exe_path = Detail::getExecutablePath();
          bool const use_addr2line_fallback = !exe_path.empty();

          // Process each frame with demangling
          for(int i = 0; i < actual_frames; ++i)
          {
            Dl_info info;
            bool has_symbol = dladdr(addresses[i + skip_frames], &info) && info.dli_sname;
            if(has_symbol)
            {
              // Demangle C++ symbol
              std::string demangled = Detail::demangleSymbol(info.dli_sname);

              // Calculate offset within function
              ptrdiff_t offset
                = reinterpret_cast<char *>(addresses[i + skip_frames]) - reinterpret_cast<char *>(info.dli_saddr);

              result += stringify("  #", i, ": ", demangled, " +", offset, " [0x",
                                  reinterpret_cast<uintptr_t>(addresses[i + skip_frames]), "]\n");
            }
            else
            {
              // Try addr2line fallback if no symbol info available
              std::string addr2line_result;
              if(use_addr2line_fallback)
                addr2line_result = Detail::resolveAddressWithAddr2line(addresses[i + skip_frames], exe_path.c_str());

              if(!addr2line_result.empty())
              {
                // addr2line succeeded - use its output
                result += stringify("  #", i, ": ", addr2line_result, " [0x",
                                    reinterpret_cast<uintptr_t>(addresses[i + skip_frames]), "]\n");
              }
              else
              {
                // Complete fallback: raw backtrace_symbols output
                result += stringify("  #", i, ": ", symbols[i], " [0x",
                                    reinterpret_cast<uintptr_t>(addresses[i + skip_frames]), "]\n");
              }
            }
          }

          free(symbols); // NOLINT(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)

  #else
          // No backtrace support - return basic info
          result = "  Stack trace unavailable (<execinfo.h> not available)\n";
          result += stringify("  Current function: ", LUMEX_FUNCTION_NAME, "\n");
  #endif

#else
          // Unknown platform
          result = "  Stack trace not supported on this platform\n";
          result += stringify("  Current function: ", LUMEX_FUNCTION_NAME, "\n");
#endif
        }
        catch(std::exception const &exc)
        {
          return stringify("  Stack trace unavailable (exception: ", exc.what(), ")\n");
        }
        catch(...)
        {
          return "  Stack trace unavailable (unknown exception)\n";
        }

        return result.empty() ? "  Stack trace empty\n" : result;
      }

      /**
       * @brief Lightweight alternative: captures only the immediate caller.
       *
       * Calling through the LUMEX_CAPTURE_CALLER_INFO() macro ensures correctness on all compilers,
       * since the values are substituted at the call site (inside the calling function).
       */
      static inline std::string
      captureCallerInfoImpl(char const *caller_function, char const *caller_file, int caller_line) noexcept
      {
        try
        {
          std::string file_name = caller_file;
          size_t last_slash     = file_name.find_last_of("/\\");
          if(last_slash != std::string::npos) file_name = file_name.substr(last_slash + 1);

          return stringify(caller_function ? caller_function : "<unknown>", "() at ", file_name, ":", caller_line);
        }
        catch(...)
        {
          return "Caller info unavailable";
        }
      }

      // Macro wrapper to capture caller context portably at the call site
#if defined(__clang__) || defined(__GNUC__)
  #define LUMEX_CAPTURE_CALLER_INFO() \
    ::Lumex::Utility::Debug::captureCallerInfoImpl(__builtin_FUNCTION(), __builtin_FILE(), __builtin_LINE__)
#elif defined(_MSC_VER)
  #define LUMEX_CAPTURE_CALLER_INFO() \
    ::Lumex::Utility::Debug::captureCallerInfoImpl(__FUNCSIG__, __FILE__, __LINE__)
#else
  #define LUMEX_CAPTURE_CALLER_INFO() \
    ::Lumex::Utility::Debug::captureCallerInfoImpl("<unknown>", "<unknown>", 0)
#endif
    } // namespace Debug
    // NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay, cppcoreguidelines-avoid-c-arrays,
    // cppcoreguidelines-pro-bounds-constant-array-index, cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)
  } // namespace Utility
} // namespace Lumex

#endif // !LUMEX_UTILITIES_DEBUG_HPP
