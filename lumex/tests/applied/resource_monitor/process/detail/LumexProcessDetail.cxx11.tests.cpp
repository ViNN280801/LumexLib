#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

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
lumex_string_view
with_nuls (char const (&text)[N])
{
  return lumex_string_view (text, N - 1);
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

TEST (LumexProcessDetailTest, StatGivesTheParentProcessId)
{
  // Field 4 follows the state: "... S 1 4242 ..." is parent 1, group 4242.
  EXPECT_EQ (parse_proc_pid_stat (stat_line ("app"))->parent_pid, 1U);

  std::string line = stat_line ("app");
  line.replace (line.find (") S 1 ") + 4, 1, "9001");
  optional<proc_pid_stat_t> const stat = parse_proc_pid_stat (line);
  ASSERT_TRUE (stat.has_value ());
  EXPECT_EQ (stat->parent_pid, 9001U);
  EXPECT_EQ (stat->utime_ticks, 1500U);
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

namespace
{
process_link_t
edge (process_id_t pid, process_id_t parent, std::uint64_t started = 0)
{
  process_link_t value;
  value.pid = pid;
  value.parent_pid = parent;
  value.start_time = started;
  return value;
}

std::vector<process_id_t>
ids (std::initializer_list<process_id_t> list)
{
  return std::vector<process_id_t> (list);
}
} // namespace

TEST (LumexProcessDetailTest,
      DescendantsAreChildrenThenTheirChildrenNearestFirst)
{
  // 10 -> 11, 12; 11 -> 13, 14; 13 -> 15. Other trees and an unrelated 20.
  std::vector<process_link_t> const links
      = { edge (1, 0),   edge (10, 1),  edge (11, 10),
          edge (12, 10), edge (13, 11), edge (14, 11),
          edge (15, 13), edge (20, 1),  edge (21, 20) };
  EXPECT_EQ (descendants_of (10, links), ids ({ 11, 12, 13, 14, 15 }));
  EXPECT_EQ (descendants_of (11, links), ids ({ 13, 14, 15 }));
  EXPECT_EQ (descendants_of (15, links), ids ({}));
  EXPECT_EQ (descendants_of (20, links), ids ({ 21 }));
}

TEST (LumexProcessDetailTest, DescendantsOfAProcessNobodyKnowsAreNone)
{
  EXPECT_TRUE (descendants_of (10, {}).empty ());
  EXPECT_TRUE (descendants_of (10, { edge (11, 5) }).empty ());
}

TEST (LumexProcessDetailTest, DescendantsOfAnUnlistedRootAreStillFound)
{
  // The root is not in the list (it may not be readable), its children are.
  EXPECT_EQ (descendants_of (10, { edge (11, 10), edge (12, 11) }),
             ids ({ 11, 12 }));
}

TEST (LumexProcessDetailTest, AChildOlderThanItsParentIsAStaleLink)
{
  // 11 started before 10: the ID 10 was reused after the real parent exited,
  // so 11 and 12 below it do not belong to 10. 13 started after it and does.
  std::vector<process_link_t> const links
      = { edge (10, 1, 500), edge (11, 10, 100), edge (12, 11, 700),
          edge (13, 10, 600) };
  EXPECT_EQ (descendants_of (10, links), ids ({ 13 }));
}

TEST (LumexProcessDetailTest, AnUnknownStartTimeNeverMakesALinkStale)
{
  std::vector<process_link_t> const links
      = { edge (10, 1, 500), edge (11, 10, 0), edge (12, 11, 0),
          edge (13, 10, 600) };
  EXPECT_EQ (descendants_of (10, links), ids ({ 11, 13, 12 }));
  // The root's own start time unknown: nothing to compare with.
  EXPECT_EQ (descendants_of (10, { edge (10, 1, 0), edge (11, 10, 5) }),
             ids ({ 11 }));
}

TEST (LumexProcessDetailTest, AProcessThatIsItsOwnParentIsNotListedBelowItself)
{
  // Windows' idle process: ID 0, parent 0; the System process 4 is below it.
  std::vector<process_link_t> const links = { edge (0, 0), edge (4, 0) };
  EXPECT_EQ (descendants_of (0, links), ids ({ 4 }));
  EXPECT_TRUE (descendants_of (4, links).empty ());
}

TEST (LumexProcessDetailTest, ALoopInTheLinksEndsAndCountsEachProcessOnce)
{
  // 10 -> 11 -> 12 -> 10 (stale links that close a circle).
  std::vector<process_link_t> const links
      = { edge (10, 12), edge (11, 10), edge (12, 11) };
  EXPECT_EQ (descendants_of (10, links), ids ({ 11, 12 }));
}

TEST (LumexProcessDetailTest, TreesDropARootThatLiesBelowAnotherRoot)
{
  // 10 -> 11 -> 12 and 10 -> 13; 20 alone. Roots 10, 12 and 20: 12 is part
  // of 10.
  std::vector<process_link_t> const links
      = { edge (10, 1), edge (11, 10), edge (12, 11), edge (13, 10),
          edge (20, 1) };
  std::vector<process_tree_t> const trees
      = process_trees (ids ({ 20, 12, 10 }), links);
  ASSERT_EQ (trees.size (), 2U);
  EXPECT_EQ (trees[0].root, 10U);
  EXPECT_EQ (trees[0].descendants, ids ({ 11, 13, 12 }));
  EXPECT_EQ (trees[1].root, 20U);
  EXPECT_TRUE (trees[1].descendants.empty ());
}

TEST (LumexProcessDetailTest, TreesKeepARootBelowANonRootOfTheSameTree)
{
  // 10 (root) -> 11 (not a root) -> 12 (root): 12 is below root 10.
  std::vector<process_link_t> const links
      = { edge (10, 1), edge (11, 10), edge (12, 11) };
  std::vector<process_tree_t> const trees
      = process_trees (ids ({ 12, 10 }), links);
  ASSERT_EQ (trees.size (), 1U);
  EXPECT_EQ (trees[0].root, 10U);
  EXPECT_EQ (trees[0].descendants, ids ({ 11, 12 }));
}

TEST (LumexProcessDetailTest, TreesCountARepeatedRootOnce)
{
  std::vector<process_tree_t> const trees
      = process_trees (ids ({ 10, 10 }), { edge (10, 1), edge (11, 10) });
  ASSERT_EQ (trees.size (), 1U);
  EXPECT_EQ (trees[0].root, 10U);
}

TEST (LumexProcessDetailTest,
      TreesOfTwoRootsInEachOthersBranchesKeepTheSmaller)
{
  // Stale links make 10 and 12 each other's descendants: one survives.
  std::vector<process_link_t> const links
      = { edge (10, 12), edge (11, 10), edge (12, 11) };
  std::vector<process_tree_t> const trees
      = process_trees (ids ({ 12, 10 }), links);
  ASSERT_EQ (trees.size (), 1U);
  EXPECT_EQ (trees[0].root, 10U);
}

TEST (LumexProcessDetailTest, TreesOfNothingAreNothing)
{
  EXPECT_TRUE (process_trees ({}, { edge (10, 1) }).empty ());
  std::vector<process_tree_t> const lone = process_trees (ids ({ 7 }), {});
  ASSERT_EQ (lone.size (), 1U);
  EXPECT_TRUE (lone[0].descendants.empty ());
}
