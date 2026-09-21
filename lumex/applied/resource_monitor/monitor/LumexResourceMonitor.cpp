#define LUMEX_IMPLEMENTATION
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>

#include "LumexResourceMonitor.hpp"
#include "lumex/applied/logging/LumexLogging"
#include "lumex/core/time/LumexTime"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#else
#include <sys/sysinfo.h>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

#if defined(__clang__)
#endif

#endif

namespace lumex
{
namespace applied
{
namespace resource_monitor
{
namespace monitor
{
namespace
{
std::mutex g_mutex;
std::thread g_thread;
std::atomic_bool g_stop{ false };
bool g_started{ false };

std::chrono::milliseconds
sanitizePollInterval (std::chrono::milliseconds requested)
{
  if (requested.count () <= 0)
    return std::chrono::milliseconds (
        lumex::applied::resource_monitor::monitor::Constants::
            KDEFAULT_POLL_INTERVAL_MS);
  return requested;
}

std::string
makeLogFilePath (std::string const &logDirectory)
{
  std::error_code ec;
  std::filesystem::create_directories (logDirectory, ec);
  if (ec)
    {
      lumWarning (lumex::applied::resource_monitor::monitor::KMODULE_NAME,
                  "Failed to create log directory '", logDirectory,
                  "': ", ec.message ());
    }
  std::string const timestamp
      = LumexTime::get_current_datetime ("%Y-%m-%d_%H-%M-%S");
  return (std::filesystem::path (logDirectory)
          / ("system_resource_usage_" + timestamp + ".log"))
      .string ();
}

#if defined(_WIN32) || defined(_WIN64)
std::uint64_t
fileTimeToUint64 (FILETIME const &ft)
{
  ULARGE_INTEGER u;
  u.LowPart = ft.dwLowDateTime;
  u.HighPart = ft.dwHighDateTime;
  return u.QuadPart;
}

bool
sampleCpuWin (std::uint64_t &idleOut, std::uint64_t &totalOut)
{
  FILETIME idleFt{};
  FILETIME kernelFt{};
  FILETIME userFt{};
  if (GetSystemTimes (&idleFt, &kernelFt, &userFt) == 0)
    return false;
  std::uint64_t const idle = fileTimeToUint64 (idleFt);
  std::uint64_t const kernel = fileTimeToUint64 (kernelFt);
  std::uint64_t const user = fileTimeToUint64 (userFt);
  idleOut = idle;
  totalOut = idle + kernel + user;
  return true;
}

void
sampleRamWin (double &usedGbOut, double &totalGbOut)
{
  MEMORYSTATUSEX ms{};
  ms.dwLength = sizeof (ms);
  if (GlobalMemoryStatusEx (&ms) == 0)
    {
      usedGbOut = 0.;
      totalGbOut = 0.;
      return;
    }
  std::uint64_t const total = ms.ullTotalPhys;
  std::uint64_t const avail = ms.ullAvailPhys;
  std::uint64_t const used = (total > avail) ? (total - avail) : 0;
  LUMEX_CONSTEXPR double gibi = 1024. * 1024. * 1024.;
  totalGbOut = static_cast<double> (total) / gibi;
  usedGbOut = static_cast<double> (used) / gibi;
}
#else
bool
sampleCpuLinux (std::uint64_t &idleOut, std::uint64_t &totalOut)
{
  std::ifstream statFile ("/proc/stat");
  std::string line;
  if (!std::getline (statFile, line))
    return false;
  if (line.size () < 5 || line.compare (0, 4, "cpu ") != 0)
    return false;
  std::istringstream iss (line);
  std::string cpuLabel;
  iss >> cpuLabel;
  std::uint64_t value{};
  std::uint64_t sum{};
  std::uint64_t idle{};
  int idx = 0;
  while (iss >> value)
    {
      sum += value;
      if (idx == 3)
        idle = value;
      ++idx;
    }
  if (idx < 4)
    return false;
  idleOut = idle;
  totalOut = sum;
  return true;
}

void
sampleRamLinux (double &usedGbOut, double &totalGbOut)
{
  struct sysinfo si{};
  if (sysinfo (&si) != 0)
    {
      usedGbOut = 0.;
      totalGbOut = 0.;
      return;
    }
  unsigned long long const total
      = static_cast<unsigned long long> (si.totalram) * si.mem_unit;
  unsigned long long const freeRam
      = static_cast<unsigned long long> (si.freeram) * si.mem_unit;
  unsigned long long const used = (total > freeRam) ? (total - freeRam) : 0;
  LUMEX_CONSTEXPR double gibi = 1024. * 1024. * 1024.;
  totalGbOut = static_cast<double> (total) / gibi;
  usedGbOut = static_cast<double> (used) / gibi;
}
#endif

void
workerLoop (std::string const &logFilePath, std::chrono::milliseconds interval)
{
  std::ofstream out (logFilePath, std::ios::out | std::ios::app);
  if (!out)
    {
      lumError (lumex::applied::resource_monitor::monitor::KMODULE_NAME,
                "Failed to open log file '", logFilePath, "'");
      return;
    }

#if defined(_WIN32) || defined(_WIN64)
  std::uint64_t idle0{};
  std::uint64_t total0{};
  if (!sampleCpuWin (idle0, total0))
    return;
#else
  std::uint64_t idle0{};
  std::uint64_t total0{};
  if (!sampleCpuLinux (idle0, total0))
    return;
#endif

  while (!g_stop.load (std::memory_order_relaxed))
    {
      std::this_thread::sleep_for (interval);
      if (g_stop.load (std::memory_order_relaxed))
        break;

#if defined(_WIN32) || defined(_WIN64)
      std::uint64_t idle1{};
      std::uint64_t total1{};
      if (!sampleCpuWin (idle1, total1))
        continue;
      double usedRamGb{};
      double totalRamGb{};
      sampleRamWin (usedRamGb, totalRamGb);
#else
      std::uint64_t idle1{};
      std::uint64_t total1{};
      if (!sampleCpuLinux (idle1, total1))
        continue;
      double usedRamGb{};
      double totalRamGb{};
      sampleRamLinux (usedRamGb, totalRamGb);
#endif
      std::uint64_t const did = idle1 - idle0;
      std::uint64_t const dtot = total1 - total0;
      double cpuPct = 0.;
      if (dtot > 0 && did <= dtot)
        cpuPct
            = 100.0
              * (1.0 - static_cast<double> (did) / static_cast<double> (dtot));
      idle0 = idle1;
      total0 = total1;
      if (cpuPct < 0.)
        cpuPct = 0.;
      if (cpuPct > 100.)
        cpuPct = 100.;

      auto const now = std::chrono::system_clock::now ();
      std::time_t const tt = std::chrono::system_clock::to_time_t (now);
      std::tm tmBuf{};
#if defined(_WIN32) || defined(_WIN64)
      localtime_s (&tmBuf, &tt);
#else
      localtime_r (&tt, &tmBuf);
#endif
      out << std::put_time (&tmBuf, "%Y-%m-%d %H:%M:%S") << ' ';
      out.setf (std::ios::fixed);
      out << std::setprecision (1) << cpuPct << "%/100%, "
          << std::setprecision (2) << usedRamGb << "Gb/"
          << std::setprecision (2) << totalRamGb << "Gb\n";
      out.flush ();
    }
}

} // namespace

LUMEX_PUBLIC_API
void
lumex::applied::resource_monitor::monitor::LumexResourceMonitor::
    startIfEnabled (std::string const &logDirectory,
                    std::chrono::milliseconds pollInterval)
{
  std::lock_guard<std::mutex> lock (g_mutex);
  if (g_started)
    {
      lumWarning (KMODULE_NAME, "startIfEnabled() called while the sampler is "
                                "already running; ignoring");
      return;
    }

  std::chrono::milliseconds const interval
      = sanitizePollInterval (pollInterval);
  std::string const path = makeLogFilePath (logDirectory);

  g_stop.store (false, std::memory_order_relaxed);
  g_thread = std::thread ([path, interval] () {
    // Give the host application time to finish initializing before
    // sampling starts, and avoid reporting noise from a
    // thread/instrumentation sanitizer for activity happening during
    // that window.
    std::this_thread::sleep_for (
        std::chrono::milliseconds (Constants::KSTARTUP_GRACE_PERIOD_MS));
    workerLoop (path, interval);
  });
  g_started = true;

  lumInfo (KMODULE_NAME, "Started, logging to '", path, "' every ",
           std::to_string (interval.count ()), " ms");
}

LUMEX_PUBLIC_API
void
lumex::applied::resource_monitor::monitor::LumexResourceMonitor::stop ()
{
  std::lock_guard<std::mutex> lock (g_mutex);
  if (!g_started)
    return;
  g_stop.store (true, std::memory_order_relaxed);
  if (g_thread.joinable ())
    g_thread.join ();
  g_started = false;
  lumInfo (KMODULE_NAME, "Stopped");
}
} // namespace monitor
} // namespace resource_monitor
} // namespace applied
} // namespace lumex
