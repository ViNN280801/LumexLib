#include <cstdint>
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "lumex/applied/resource_monitor/monitor/detail/LumexProcFs.hpp"

using namespace lumex::applied::resource_monitor::monitor::detail;

namespace
{
// A /proc/stat as Linux 2.6.33+ writes it: user nice system idle iowait irq
// softirq steal guest guest_nice, then the per-CPU lines.
constexpr char const *KFULL_PROC_STAT = "cpu  100 20 30 400 50 6 7 8 9 10\n"
                                        "cpu0 50 10 15 200 25 3 3 4 4 5\n"
                                        "intr 12345 0 0\n";

constexpr char const *KMEMINFO = "MemTotal:       32615420 kB\n"
                                 "MemFree:         1048576 kB\n"
                                 "MemAvailable:   20971520 kB\n"
                                 "Buffers:          524288 kB\n"
                                 "Cached:         18874368 kB\n";
} // namespace

TEST (LumexProcFsTest, ProcStatCountsIowaitAsIdle)
{
  std::optional<proc_stat_cpu_t> const cpu
      = parse_proc_stat_cpu (KFULL_PROC_STAT);
  ASSERT_TRUE (cpu.has_value ());
  EXPECT_EQ (cpu->idle, 400U + 50U);
}

TEST (LumexProcFsTest, ProcStatTotalLeavesGuestTimeOut)
{
  // guest and guest_nice are already part of user and nice.
  std::optional<proc_stat_cpu_t> const cpu
      = parse_proc_stat_cpu (KFULL_PROC_STAT);
  ASSERT_TRUE (cpu.has_value ());
  EXPECT_EQ (cpu->total, 100U + 20U + 30U + 400U + 50U + 6U + 7U + 8U);
}

TEST (LumexProcFsTest, ProcStatOfAnOldKernelWithFourFields)
{
  std::optional<proc_stat_cpu_t> const cpu
      = parse_proc_stat_cpu ("cpu  1 2 3 4\n");
  ASSERT_TRUE (cpu.has_value ());
  EXPECT_EQ (cpu->idle, 4U);
  EXPECT_EQ (cpu->total, 10U);
}

TEST (LumexProcFsTest, ProcStatWithoutTheLastNewline)
{
  std::optional<proc_stat_cpu_t> const cpu
      = parse_proc_stat_cpu ("cpu  1 2 3 4 5");
  ASSERT_TRUE (cpu.has_value ());
  EXPECT_EQ (cpu->idle, 9U);
  EXPECT_EQ (cpu->total, 15U);
}

TEST (LumexProcFsTest, ProcStatRejectsTooFewFields)
{
  EXPECT_FALSE (parse_proc_stat_cpu ("cpu  1 2 3\n").has_value ());
  EXPECT_FALSE (parse_proc_stat_cpu ("cpu  1 2 x 4\n").has_value ());
}

TEST (LumexProcFsTest, ProcStatRejectsAFirstLineThatIsNotTheAggregate)
{
  EXPECT_FALSE (parse_proc_stat_cpu ("").has_value ());
  EXPECT_FALSE (parse_proc_stat_cpu ("\ncpu  1 2 3 4\n").has_value ());
  EXPECT_FALSE (parse_proc_stat_cpu ("cpu0 1 2 3 4\n").has_value ());
  EXPECT_FALSE (parse_proc_stat_cpu ("intr 1 2 3 4\n").has_value ());
}

TEST (LumexProcFsTest, MeminfoGivesTotalAndAvailableInBytes)
{
  std::optional<proc_meminfo_t> const memory = parse_proc_meminfo (KMEMINFO);
  ASSERT_TRUE (memory.has_value ());
  EXPECT_EQ (memory->total_bytes, std::uint64_t{ 32615420 } * 1024U);
  EXPECT_EQ (memory->available_bytes, std::uint64_t{ 20971520 } * 1024U);
}

TEST (LumexProcFsTest, MeminfoWithoutMemAvailableGivesNothing)
{
  // Kernels before 3.14: the sampler falls back to sysinfo ().
  EXPECT_FALSE (
      parse_proc_meminfo ("MemTotal: 1000 kB\nMemFree: 10 kB\n").has_value ());
  EXPECT_FALSE (parse_proc_meminfo ("MemAvailable: 10 kB\n").has_value ());
  EXPECT_FALSE (parse_proc_meminfo ("").has_value ());
}

TEST (LumexProcFsTest, MeminfoMatchesWholeKeysOnly)
{
  std::optional<proc_meminfo_t> const memory
      = parse_proc_meminfo ("MemTotalX:  1 kB\n"
                            "MemAvailableSoon: 2 kB\n"
                            "MemAvailable: 300 kB\n"
                            "MemTotal: 400 kB");
  ASSERT_TRUE (memory.has_value ());
  EXPECT_EQ (memory->total_bytes, 400U * 1024U);
  EXPECT_EQ (memory->available_bytes, 300U * 1024U);
}

TEST (LumexProcFsTest, ReadWholeFileOfAMissingFileGivesNothing)
{
  EXPECT_FALSE (
      read_whole_file ("this/file/does/not/exist/LumexProcFs").has_value ());
}

#if defined(__linux__)
// The parsers against this machine's own files (Linux only; the Windows
// sampler reads no /proc).
TEST (LumexProcFsTest, LiveProcStatAndMeminfoParse)
{
  std::optional<std::string> const stat = read_whole_file ("/proc/stat");
  ASSERT_TRUE (stat.has_value ());
  std::optional<proc_stat_cpu_t> const cpu = parse_proc_stat_cpu (*stat);
  ASSERT_TRUE (cpu.has_value ());
  EXPECT_GT (cpu->total, cpu->idle);

  std::optional<std::string> const meminfo = read_whole_file ("/proc/meminfo");
  ASSERT_TRUE (meminfo.has_value ());
  std::optional<proc_meminfo_t> const memory = parse_proc_meminfo (*meminfo);
  ASSERT_TRUE (memory.has_value ());
  EXPECT_GT (memory->total_bytes, 0U);
  EXPECT_LE (memory->available_bytes, memory->total_bytes);
}
#endif
