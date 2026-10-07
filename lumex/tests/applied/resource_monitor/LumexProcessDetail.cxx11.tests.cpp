#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "lumex/applied/resource_monitor/process/detail/LumexProcessDetail.hpp"
#include "lumex/core/optional/LumexOptional"
#include "lumex/core/string_view/LumexStringView"

using namespace lumex::applied::resource_monitor::process::detail;

namespace
{
// Fields 1..22 of /proc/<pid>/stat: utime (14) = 1500, stime (15) = 500,
// starttime (22) = 987654.
std::string
stat_line (std::string const &comm)
{
  return "4242 (" + comm
         + ") S 1 4242 4242 0 -1 4194560 100 0 0 0 1500 500 0 0 20 0 3 0 "
           "987654 123456789 2500 18446744073709551615";
}

// A string literal with embedded NULs as a view of its whole length (the
// terminating NUL left out); a C++11 stand-in for the "..."s literal.
template <std::size_t N>
LumexStringView
with_nuls (char const (&text)[N])
{
  return LumexStringView (text, N - 1);
}
} // namespace

TEST (LumexProcessDetailTest, StatGivesTheCpuTicksAndTheStartTime)
{
  optional<proc_pid_stat_t> const stat
      = parse_proc_pid_stat (stat_line ("PeakExpertHPLCE"));
  ASSERT_TRUE (stat.has_value ());
  EXPECT_EQ (stat->comm, "PeakExpertHPLCE");
  EXPECT_EQ (stat->state, 'S');
  EXPECT_EQ (stat->utime_ticks, 1500U);
  EXPECT_EQ (stat->stime_ticks, 500U);
  EXPECT_EQ (stat->start_ticks, 987654U);
}

TEST (LumexProcessDetailTest, StatGivesTheStateOfAZombie)
{
  std::string line = stat_line ("defunct");
  line.replace (line.find (") S ") + 2, 1, "Z");
  optional<proc_pid_stat_t> const stat = parse_proc_pid_stat (line);
  ASSERT_TRUE (stat.has_value ());
  EXPECT_EQ (stat->state, 'Z');
  EXPECT_EQ (stat->comm, "defunct");
}

TEST (LumexProcessDetailTest, StatCommMayHoldSpacesAndParentheses)
{
  optional<proc_pid_stat_t> const stat
      = parse_proc_pid_stat (stat_line ("my (odd) name) x"));
  ASSERT_TRUE (stat.has_value ());
  EXPECT_EQ (stat->comm, "my (odd) name) x");
  EXPECT_EQ (stat->utime_ticks, 1500U);
  EXPECT_EQ (stat->start_ticks, 987654U);
}

TEST (LumexProcessDetailTest, StatRejectsShortOrBrokenText)
{
  EXPECT_FALSE (parse_proc_pid_stat ("").has_value ());
  EXPECT_FALSE (parse_proc_pid_stat ("4242 (x) S 1 2 3").has_value ());
  EXPECT_FALSE (parse_proc_pid_stat ("4242 x S 1 4242 4242 0 -1 4194560 100 "
                                     "0 0 0 1500 500 0 0 20 0 3 0 987654")
                    .has_value ());
  EXPECT_FALSE (parse_proc_pid_stat ("4242 (x) S 1 4242 4242 0 -1 4194560 "
                                     "100 0 0 0 abc 500 0 0 20 0 3 0 987654")
                    .has_value ());
}

TEST (LumexProcessDetailTest, StatusGivesResidentAndAnonymousBytes)
{
  proc_pid_status_t const status = parse_proc_pid_status (
      "Name:\tPeakExpertWeb\nVmRSS:\t  215040 kB\nRssAnon:\t  102400 kB\n"
      "RssFile:\t  112640 kB\n");
  ASSERT_TRUE (status.resident_bytes.has_value ());
  ASSERT_TRUE (status.anonymous_bytes.has_value ());
  EXPECT_EQ (*status.resident_bytes, std::uint64_t{ 215040 } * 1024U);
  EXPECT_EQ (*status.anonymous_bytes, std::uint64_t{ 102400 } * 1024U);
}

TEST (LumexProcessDetailTest, StatusOfAnOldKernelHasNoAnonymousBytes)
{
  proc_pid_status_t const status
      = parse_proc_pid_status ("VmRSS:\t  2048 kB\nVmData:\t  1024 kB\n");
  ASSERT_TRUE (status.resident_bytes.has_value ());
  EXPECT_FALSE (status.anonymous_bytes.has_value ());
}

TEST (LumexProcessDetailTest, StatusOfAKernelThreadHasNoResidentBytes)
{
  proc_pid_status_t const status
      = parse_proc_pid_status ("Name:\tkthreadd\nState:\tS (sleeping)\n");
  EXPECT_FALSE (status.resident_bytes.has_value ());
  EXPECT_FALSE (status.anonymous_bytes.has_value ());
}

TEST (LumexProcessDetailTest, ExecutableNameIsTheLastPathPart)
{
  EXPECT_EQ (executable_name ("/opt/app/PeakExpertWeb"), "PeakExpertWeb");
  EXPECT_EQ (executable_name ("/usr/bin/python3.12 (deleted)"), "python3.12");
  EXPECT_EQ (executable_name ("plain"), "plain");
  EXPECT_EQ (executable_name (""), "");
}

TEST (LumexProcessDetailTest, FirstArgumentNameOfACommandLine)
{
  EXPECT_EQ (
      first_argument_name (with_nuls ("/usr/bin/electron\0--type=gpu\0")),
      optional<std::string> ("electron"));
  EXPECT_EQ (first_argument_name (with_nuls ("sleep\0"
                                             "30\0")),
             optional<std::string> ("sleep"));
  EXPECT_FALSE (first_argument_name ("").has_value ());
  EXPECT_FALSE (first_argument_name (with_nuls ("\0")).has_value ());
  EXPECT_FALSE (first_argument_name (with_nuls ("/usr/bin/\0")).has_value ());
}

// The Linux rule: exact names; a 15-character comm stands for any longer name
// that starts with it, but only when comm is all that could be read.
TEST (LumexProcessDetailTest, LinuxNamesMatchExactly)
{
  EXPECT_TRUE (linux_name_matches ("PeakExpertWeb", "PeakExpertWeb", false));
  EXPECT_FALSE (linux_name_matches ("PeakExpertWeb", "peakexpertweb", false));
  EXPECT_FALSE (linux_name_matches ("PeakExpert", "PeakExpertWeb", false));
  EXPECT_FALSE (linux_name_matches ("", "", false));
}

TEST (LumexProcessDetailTest, LinuxCutCommMatchesALongerName)
{
  EXPECT_TRUE (
      linux_name_matches ("PeakExpertHPLCEngine", "PeakExpertHPLCE", true));
  // Not when the name came from the executable: it is the whole name then.
  EXPECT_FALSE (
      linux_name_matches ("PeakExpertHPLCEngine", "PeakExpertHPLCE", false));
  // Not for a comm shorter than the cut, nor for another prefix.
  EXPECT_FALSE (linux_name_matches ("PeakExpertWebX", "PeakExpertWeb", true));
  EXPECT_FALSE (
      linux_name_matches ("PeakExpertCEEngine", "PeakExpertHPLCE", true));
}

// The Windows rule: no ASCII case, with or without ".exe".
TEST (LumexProcessDetailTest, WindowsNamesIgnoreCaseAndExe)
{
  EXPECT_TRUE (windows_name_matches ("PeakExpertWeb", "PeakExpertWeb.exe"));
  EXPECT_TRUE (
      windows_name_matches ("peakexpertweb.EXE", "PeakExpertWeb.exe"));
  EXPECT_TRUE (windows_name_matches ("PEAKEXPERTWEB", "peakexpertweb"));
  EXPECT_FALSE (windows_name_matches ("PeakExpert", "PeakExpertWeb.exe"));
  EXPECT_FALSE (
      windows_name_matches ("PeakExpertWeb.ex", "PeakExpertWeb.exe"));
  EXPECT_FALSE (windows_name_matches ("", "a.exe"));
  EXPECT_FALSE (windows_name_matches (".exe", "a.exe"));
}
