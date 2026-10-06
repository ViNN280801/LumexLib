#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#if defined(__linux__)
#include <csignal>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <gtest/gtest.h>

#include "lumex/applied/resource_monitor/LumexResourceMonitor"

using namespace lumex::applied::resource_monitor::process;

namespace
{
// Not a process ID on Windows (a multiple of 4) nor below Linux's default
// pid_max (4194304).
constexpr process_id_t KNO_SUCH_PROCESS = 4194303;

// Keeps one core busy for a while, so the next sample has a CPU share.
void
burn_cpu (std::chrono::milliseconds duration)
{
  auto const until = std::chrono::steady_clock::now () + duration;
  volatile std::uint64_t sink = 0;
  while (std::chrono::steady_clock::now () < until)
    sink = sink + 1;
}

#if defined(_WIN32)
std::string
own_executable_name ()
{
  // The test binary is named after its target on every platform.
  return "LumexResourceMonitorCxx17Tests.exe";
}
#else
std::string
own_executable_name ()
{
  return std::filesystem::read_symlink ("/proc/self/exe")
      .filename ()
      .string ();
}
#endif

process_usage_t
usage_of (process_id_t pid, double cpu, std::uint64_t resident,
          std::optional<std::uint64_t> private_bytes)
{
  process_usage_t usage;
  usage.pid = pid;
  usage.name = "app";
  if (cpu >= 0.)
    usage.cpu_percent = cpu;
  usage.cpu_time = std::chrono::milliseconds (10);
  usage.logical_processors = 8;
  usage.resident_bytes = resident;
  usage.private_bytes = private_bytes;
  return usage;
}
} // namespace

TEST (LumexProcessMonitorTest, FirstSampleOfAProcessHasNoCpuShareYet)
{
  LumexProcessMonitor monitor;
  LumexProcessMonitor::result_t const usage
      = monitor.sample (LumexProcessMonitor::current_process_id ());
  ASSERT_TRUE (usage.has_value ());
  EXPECT_EQ (usage->pid, LumexProcessMonitor::current_process_id ());
  EXPECT_EQ (usage->name, own_executable_name ());
  EXPECT_FALSE (usage->cpu_percent.has_value ());
  EXPECT_GT (usage->resident_bytes, 0U);
  EXPECT_GE (usage->logical_processors, 1U);
  EXPECT_EQ (usage->process_count, 1U);
}

TEST (LumexProcessMonitorTest, OwnPrivateMemoryIsKnownAndBelowResident)
{
  LumexProcessMonitor monitor;
  LumexProcessMonitor::result_t const usage
      = monitor.sample (LumexProcessMonitor::current_process_id ());
  ASSERT_TRUE (usage.has_value ());
  ASSERT_TRUE (usage->private_bytes.has_value ());
  EXPECT_GT (*usage->private_bytes, 0U);
#if defined(__linux__)
  // RssAnon is part of VmRSS (Windows PrivateUsage is not part of the
  // working set).
  EXPECT_LE (*usage->private_bytes, usage->resident_bytes);
#endif
}

TEST (LumexProcessMonitorTest, SecondSampleHasTheCpuShareOfABusyProcess)
{
  LumexProcessMonitor monitor;
  process_id_t const self = LumexProcessMonitor::current_process_id ();
  LumexProcessMonitor::result_t const first = monitor.sample (self);
  ASSERT_TRUE (first.has_value ());

  burn_cpu (std::chrono::milliseconds (300));

  LumexProcessMonitor::result_t const second = monitor.sample (self);
  ASSERT_TRUE (second.has_value ());
  ASSERT_TRUE (second->cpu_percent.has_value ());
  EXPECT_GT (*second->cpu_percent, 0.);
  EXPECT_LE (*second->cpu_percent, 100.);
  EXPECT_GT (second->cpu_time, first->cpu_time);
}

TEST (LumexProcessMonitorTest, EachMonitorKeepsItsOwnPreviousSamples)
{
  process_id_t const self = LumexProcessMonitor::current_process_id ();
  LumexProcessMonitor first;
  ASSERT_TRUE (first.sample (self).has_value ());
  burn_cpu (std::chrono::milliseconds (50));

  LumexProcessMonitor second;
  LumexProcessMonitor::result_t const fresh = second.sample (self);
  ASSERT_TRUE (fresh.has_value ());
  EXPECT_FALSE (fresh->cpu_percent.has_value ());
  LumexProcessMonitor::result_t const again = first.sample (self);
  ASSERT_TRUE (again.has_value ());
  EXPECT_TRUE (again->cpu_percent.has_value ());
}

TEST (LumexProcessMonitorTest, AMovedMonitorKeepsItsPreviousSamples)
{
  process_id_t const self = LumexProcessMonitor::current_process_id ();
  LumexProcessMonitor original;
  ASSERT_TRUE (original.sample (self).has_value ());
  LumexProcessMonitor moved (std::move (original));
  LumexProcessMonitor::result_t const usage = moved.sample (self);
  ASSERT_TRUE (usage.has_value ());
  EXPECT_TRUE (usage->cpu_percent.has_value ());
}

TEST (LumexProcessMonitorTest, AProcessIdNoProcessHasIsNotFound)
{
  LumexProcessMonitor monitor;
  LumexProcessMonitor::result_t const usage
      = monitor.sample (KNO_SUCH_PROCESS);
  ASSERT_FALSE (usage.has_value ());
  EXPECT_EQ (usage.error (), process_query_error::not_found);
}

TEST (LumexProcessMonitorTest, OwnNameFindsThisProcess)
{
  std::vector<process_id_t> const found
      = LumexProcessMonitor::find_by_name (own_executable_name ());
  EXPECT_NE (std::find (found.begin (), found.end (),
                        LumexProcessMonitor::current_process_id ()),
             found.end ());
}

TEST (LumexProcessMonitorTest, SampleByNameGivesOneSamplePerProcess)
{
  LumexProcessMonitor monitor;
  std::vector<process_usage_t> const usages
      = monitor.sample_by_name (own_executable_name ());
  ASSERT_FALSE (usages.empty ());
  bool has_self = false;
  for (process_usage_t const &usage : usages)
    {
      EXPECT_EQ (usage.name, own_executable_name ());
      has_self = has_self
                 || usage.pid == LumexProcessMonitor::current_process_id ();
    }
  EXPECT_TRUE (has_self);
}

TEST (LumexProcessMonitorTest, AnUnknownNameFindsNothing)
{
  LumexProcessMonitor monitor;
  EXPECT_TRUE (
      monitor.sample_by_name ("LumexNoProcessHasThisName12345").empty ());
  EXPECT_TRUE (LumexProcessMonitor::find_by_name ("").empty ());
}

TEST (LumexProcessMonitorTest, ForgetExitedKeepsLiveProcesses)
{
  LumexProcessMonitor monitor;
  process_id_t const self = LumexProcessMonitor::current_process_id ();
  ASSERT_TRUE (monitor.sample (self).has_value ());
  monitor.forget_exited ();
  LumexProcessMonitor::result_t const usage = monitor.sample (self);
  ASSERT_TRUE (usage.has_value ());
  EXPECT_TRUE (usage->cpu_percent.has_value ());
}

#if defined(__linux__)
// A child that is sampled while it runs, then killed and reaped: its ID is
// then not found.
TEST (LumexProcessMonitorTest, AnExitedProcessIsNotFound)
{
  pid_t const child = ::fork ();
  ASSERT_GE (child, 0);
  if (child == 0)
    {
      ::pause ();
      ::_exit (0);
    }
  LumexProcessMonitor monitor;
  process_id_t const pid = static_cast<process_id_t> (child);
  EXPECT_TRUE (monitor.sample (pid).has_value ());

  ::kill (child, SIGKILL);
  int status = 0;
  ::waitpid (child, &status, 0);

  LumexProcessMonitor::result_t const gone = monitor.sample (pid);
  ASSERT_FALSE (gone.has_value ());
  EXPECT_EQ (gone.error (), process_query_error::not_found);
  monitor.forget_exited ();
}

// The executable's name wins over the first argument, which a program may set
// to anything: a child runs /bin/sleep under another argv[0].
TEST (LumexProcessMonitorTest, ExecutableNameWinsOverTheFirstArgument)
{
  std::filesystem::path const sleep
      = std::filesystem::canonical ("/bin/sleep");
  std::string const renamed = "renamed-by-test";
  pid_t const child = ::fork ();
  ASSERT_GE (child, 0);
  if (child == 0)
    {
      ::execl (sleep.c_str (), renamed.c_str (), "30",
               static_cast<char *> (nullptr));
      ::_exit (127);
    }
  process_id_t const pid = static_cast<process_id_t> (child);
  std::string const exe_link = "/proc/" + std::to_string (child) + "/exe";
  // Until exec has happened, the child is still a copy of this binary.
  for (int i = 0; i < 200; ++i)
    {
      std::error_code ec;
      if (std::filesystem::read_symlink (exe_link, ec) == sleep)
        break;
      std::this_thread::sleep_for (std::chrono::milliseconds (10));
    }

  LumexProcessMonitor monitor;
  LumexProcessMonitor::result_t const usage = monitor.sample (pid);
  std::vector<process_id_t> const by_exe
      = LumexProcessMonitor::find_by_name (sleep.filename ().string ());
  std::vector<process_id_t> const by_argument
      = LumexProcessMonitor::find_by_name (renamed);
  ::kill (child, SIGKILL);
  int status = 0;
  ::waitpid (child, &status, 0);

  ASSERT_TRUE (usage.has_value ());
  EXPECT_EQ (usage->name, sleep.filename ().string ());
  EXPECT_NE (std::find (by_exe.begin (), by_exe.end (), pid), by_exe.end ());
  EXPECT_EQ (std::find (by_argument.begin (), by_argument.end (), pid),
             by_argument.end ());
}
#endif

TEST (LumexProcessMonitorTest, TotalAddsSamplesUp)
{
  std::vector<process_usage_t> const usages{ usage_of (1, 2.5, 100, 40),
                                             usage_of (2, -1., 300, 60),
                                             usage_of (3, 1.0, 50, 10) };
  process_usage_t const sum = total (usages);
  EXPECT_EQ (sum.pid, 0U);
  EXPECT_EQ (sum.name, "app");
  EXPECT_EQ (sum.process_count, 3U);
  ASSERT_TRUE (sum.cpu_percent.has_value ());
  EXPECT_DOUBLE_EQ (*sum.cpu_percent, 3.5);
  EXPECT_EQ (sum.cpu_time, std::chrono::milliseconds (30));
  EXPECT_EQ (sum.resident_bytes, 450U);
  ASSERT_TRUE (sum.private_bytes.has_value ());
  EXPECT_EQ (*sum.private_bytes, 110U);
}

TEST (LumexProcessMonitorTest, TotalLeavesUnknownValuesUnknown)
{
  std::vector<process_usage_t> usages{ usage_of (1, -1., 100, 40),
                                       usage_of (2, -1., 300, std::nullopt) };
  usages.at (1).name = "other";
  process_usage_t const sum = total (usages);
  EXPECT_FALSE (sum.cpu_percent.has_value ());
  EXPECT_FALSE (sum.private_bytes.has_value ());
  EXPECT_TRUE (sum.name.empty ());
  EXPECT_EQ (sum.resident_bytes, 400U);
}

TEST (LumexProcessMonitorTest, TotalOfNothingIsZero)
{
  process_usage_t const sum = total ({});
  EXPECT_EQ (sum.process_count, 0U);
  EXPECT_EQ (sum.resident_bytes, 0U);
  EXPECT_FALSE (sum.cpu_percent.has_value ());
  EXPECT_TRUE (sum.name.empty ());
}
