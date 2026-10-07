#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/resource_monitor/LumexResourceMonitor"
#include "lumex/applied/resource_monitor/monitor/detail/LumexWatchLog.hpp"
#include "lumex/core/expected/Expected"
#include "lumex/core/optional/LumexOptional"

using namespace lumex::applied::resource_monitor::monitor::detail;
using namespace lumex::applied::resource_monitor::process;
using lumex::core::expected::result::unexpect;

namespace
{
constexpr std::uint64_t KMEGABYTE = 1024U * 1024U;

process_usage_t
sample_of (process_id_t pid, std::string const &name, std::uint64_t megabytes,
           std::size_t count = 1)
{
  process_usage_t usage;
  usage.pid = pid;
  usage.name = name;
  usage.cpu_percent = 3.14;
  usage.resident_bytes = megabytes * KMEGABYTE;
  usage.process_count = count;
  return usage;
}

process_member_t
member_of (process_id_t pid, std::string const &name, std::uint64_t megabytes,
           bool known_cpu = true)
{
  process_member_t member;
  member.pid = pid;
  member.name = name;
  if (known_cpu)
    member.cpu_percent = 1.3;
  member.resident_bytes = megabytes * KMEGABYTE;
  return member;
}
} // namespace

TEST (LumexWatchLogTest, MegabytesHaveOneDecimal)
{
  EXPECT_EQ (format_megabytes (0), "0.0Mb");
  EXPECT_EQ (format_megabytes (KMEGABYTE), "1.0Mb");
  EXPECT_EQ (format_megabytes (KMEGABYTE + KMEGABYTE / 2), "1.5Mb");
  EXPECT_EQ (format_megabytes (1288490189U), "1228.8Mb");
}

TEST (LumexWatchLogTest, ACpuShareHasOneDecimalAndAMissingOneIsNotAvailable)
{
  EXPECT_EQ (format_cpu_share (3.14), "3.1%");
  EXPECT_EQ (format_cpu_share (0.0), "0.0%");
  EXPECT_EQ (format_cpu_share (100.0), "100.0%");
  EXPECT_EQ (format_cpu_share (nullopt), "n/a");
}

TEST (LumexWatchLogTest, AProcessEntryNamesTheProcessAndItsUsage)
{
  LumexProcessMonitor::result_t const result
      = sample_of (4321, "PeakExpertWeb", 210);
  watch_entry_t const entry = format_pid_entry (4321, result, false);
  EXPECT_EQ (entry.head, "PeakExpertWeb[4321] 3.1% 210.0Mb");
  EXPECT_TRUE (entry.details.empty ());
}

TEST (LumexWatchLogTest, AProcessEntryCountsTheProcessesBelowIt)
{
  LumexProcessMonitor::result_t const result
      = sample_of (4321, "PeakExpertWeb", 1228, 3);
  EXPECT_EQ (format_pid_entry (4321, result, false).head,
             "PeakExpertWeb[4321 +2] 3.1% 1228.0Mb");
}

TEST (LumexWatchLogTest, AProcessWithoutAShareYetIsNotAvailable)
{
  process_usage_t usage = sample_of (7, "app", 5);
  usage.cpu_percent = nullopt;
  EXPECT_EQ (format_pid_entry (7, usage, false).head, "app[7] n/a 5.0Mb");
}

TEST (LumexWatchLogTest, AProcessWithoutANameIsAQuestionMark)
{
  EXPECT_EQ (format_pid_entry (7, sample_of (7, "", 5), false).head,
             "?[7] 3.1% 5.0Mb");
}

TEST (LumexWatchLogTest, AProcessThatCannotBeReadSaysWhy)
{
  EXPECT_EQ (format_pid_entry (777,
                               LumexProcessMonitor::result_t (
                                   unexpect, process_query_error::not_found),
                               false)
                 .head,
             "777 exited");
  EXPECT_EQ (
      format_pid_entry (777,
                        LumexProcessMonitor::result_t (
                            unexpect, process_query_error::access_denied),
                        false)
          .head,
      "777 access denied");
  EXPECT_EQ (format_pid_entry (777,
                               LumexProcessMonitor::result_t (
                                   unexpect, process_query_error::read_failed),
                               false)
                 .head,
             "777 unreadable");
  EXPECT_EQ (format_pid_entry (777,
                               LumexProcessMonitor::result_t (
                                   unexpect, process_query_error::unsupported),
                               false)
                 .head,
             "777 unsupported");
}

TEST (LumexWatchLogTest, TheBreakdownOfAProcessListsEveryMember)
{
  process_usage_t usage = sample_of (10, "shell", 30, 3);
  usage.members.push_back (member_of (10, "shell", 10));
  usage.members.push_back (member_of (11, "python3", 15, false));
  usage.members.push_back (member_of (12, "", 5));

  watch_entry_t const with = format_pid_entry (10, usage, true);
  EXPECT_EQ (with.head, "shell[10 +2] 3.1% 30.0Mb");
  ASSERT_EQ (with.details.size (), 3U);
  EXPECT_EQ (with.details[0], "10 shell 1.3% 10.0Mb");
  EXPECT_EQ (with.details[1], "11 python3 n/a 15.0Mb");
  EXPECT_EQ (with.details[2], "12 ? 1.3% 5.0Mb");

  // Not asked for: the same head, no lines.
  EXPECT_TRUE (format_pid_entry (10, usage, false).details.empty ());
}

TEST (LumexWatchLogTest, ANameWithNoProcessIsNotRunning)
{
  EXPECT_EQ (format_name_entry ("PeakExpertWeb", {}, false).head,
             "PeakExpertWeb[x0] not running");
  EXPECT_TRUE (format_name_entry ("PeakExpertWeb", {}, true).details.empty ());
}

TEST (LumexWatchLogTest, ANameEntryAddsTheSamplesUp)
{
  std::vector<process_usage_t> const usages
      = { sample_of (1, "app", 100), sample_of (2, "app", 50, 2) };
  watch_entry_t const entry = format_name_entry ("app", usages, false);
  // Both shares add up (3.14 + 3.14), the three processes count.
  EXPECT_EQ (entry.head, "app[x3] 6.3% 150.0Mb");
  EXPECT_TRUE (entry.details.empty ());
}

TEST (LumexWatchLogTest, TheBreakdownOfANameListsMembersAndSingleSamples)
{
  process_usage_t tree = sample_of (2, "app", 50, 2);
  tree.members.push_back (member_of (2, "app", 20));
  tree.members.push_back (member_of (3, "helper", 30));
  std::vector<process_usage_t> const usages
      = { sample_of (1, "app", 100), tree };

  watch_entry_t const entry = format_name_entry ("app", usages, true);
  EXPECT_EQ (entry.head, "app[x3] 6.3% 150.0Mb");
  ASSERT_EQ (entry.details.size (), 3U);
  EXPECT_EQ (entry.details[0], "1 app 3.1% 100.0Mb");
  EXPECT_EQ (entry.details[1], "2 app 1.3% 20.0Mb");
  EXPECT_EQ (entry.details[2], "3 helper 1.3% 30.0Mb");
}

TEST (LumexWatchLogTest, OneProcessHasNothingToBreakDown)
{
  std::vector<process_usage_t> const one = { sample_of (1, "app", 100) };
  EXPECT_TRUE (format_name_entry ("app", one, true).details.empty ());
}

TEST (LumexWatchLogTest, TheLineHasTheEntriesAndTheDetailsBelow)
{
  watch_entry_t first;
  first.head = "a[1] 1.0% 1.0Mb";
  first.details = { "1 a 1.0% 1.0Mb", "2 b 0.0% 0.5Mb" };
  watch_entry_t second;
  second.head = "777 exited";
  watch_entry_t third;
  third.head = "c[x2] 2.0% 2.0Mb";
  third.details = { "3 c 1.0% 1.0Mb" };

  EXPECT_EQ (
      compose_log_line ("2026-10-07 14:03:11 37.4%/100%, 5.21Gb/15.60Gb",
                        { first, second, third }),
      "2026-10-07 14:03:11 37.4%/100%, 5.21Gb/15.60Gb | a[1] 1.0% 1.0Mb"
      " | 777 exited | c[x2] 2.0% 2.0Mb\n"
      "    1 a 1.0% 1.0Mb\n"
      "    2 b 0.0% 0.5Mb\n"
      "    3 c 1.0% 1.0Mb\n");
}

TEST (LumexWatchLogTest, WithoutEntriesTheLineIsThePlainOne)
{
  EXPECT_EQ (compose_log_line ("prefix", {}), "prefix\n");
}

TEST (LumexWatchLogTest, CollectingSamplesTheProcessesAndTheNamesInOrder)
{
  // Not a process ID on Windows (a multiple of 4) nor below Linux's default
  // pid_max (4194304).
  process_id_t const none = 4194303;
  process_id_t const self = LumexProcessMonitor::current_process_id ();
  watch_list_t list;
  list.pids.push_back (self);
  list.pids.push_back (none);
  list.names.push_back ("LumexNoProcessHasThisName12345");
  list.include_children = false;

  LumexProcessMonitor monitor;
  std::vector<watch_entry_t> const entries
      = collect_watch_entries (monitor, list);
  ASSERT_EQ (entries.size (), 3U);
  EXPECT_NE (entries[0].head.find ("[" + std::to_string (self) + "] "),
             std::string::npos)
      << entries[0].head;
  EXPECT_EQ (entries[1].head, std::to_string (none) + " exited");
  EXPECT_EQ (entries[2].head,
             "LumexNoProcessHasThisName12345[x0] not running");
}
