// The CPU check of the layer: dwcas_supported () and require_dwcas ().
// The CPUID reading is replaceable (a test seam in Detail), so the answer of
// a CPU without CMPXCHG16B is injected: dwcas_supported () says no and
// require_dwcas () writes its message and aborts, in a child process.

#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <unistd.h>
#endif

#include "lumex/tests/core/atomic/dwcas/LumexDwcasTestSupport.hpp"
#include "lumex/tests/support/LumexTestSubprocess.hpp"
#include "lumex/tests/support/LumexTestThreads.hpp"

#if LUMEX_ATOMIC_HAS_DWCAS

using namespace dwcas_test;
namespace dwcas_detail = lumex::core::atomic::dwcas::Detail;
using lumex::core::atomic::dwcas::dwcas_supported;
using lumex::core::atomic::dwcas::require_dwcas;

namespace
{
std::atomic<int> g_reads (0);

std::uint32_t
cpu_without_the_bit ()
{
  g_reads.fetch_add (1);
  return 0xFFFFFFFFu & ~dwcas_detail::cmpxchg16b_ecx_bit;
}

std::uint32_t
cpu_with_the_bit ()
{
  g_reads.fetch_add (1);
  return dwcas_detail::cmpxchg16b_ecx_bit;
}

std::uint32_t
cpu_with_nothing ()
{
  g_reads.fetch_add (1);
  return 0u;
}

/// Puts a fake CPUID reading in place and restores the real one.
class FakeCpu
{
public:
  explicit FakeCpu (dwcas_detail::cpuid_reader_t reader)
      : saved_ (dwcas_detail::cpuid_reader ())
  {
    g_reads.store (0);
    dwcas_detail::cpuid_reader () = reader;
    dwcas_detail::reset_cpu_probe ();
  }

  ~FakeCpu ()
  {
    dwcas_detail::cpuid_reader () = saved_;
    dwcas_detail::reset_cpu_probe ();
  }

  FakeCpu (FakeCpu const &) = delete;
  FakeCpu &operator= (FakeCpu const &) = delete;

private:
  dwcas_detail::cpuid_reader_t saved_;
};

#if defined(__linux__)
/// True when /proc/cpuinfo lists the cx16 flag, false when it lists flags
/// without it; sets @p known to false when the file cannot be read.
bool
kernel_says_cx16 (bool &known)
{
  std::ifstream info ("/proc/cpuinfo");
  std::string line;
  known = false;
  while (std::getline (info, line))
    {
      if (line.compare (0, 5, "flags") != 0)
        continue;
      known = true;
      std::istringstream words (line);
      std::string word;
      while (words >> word)
        if (word == "cx16")
          return true;
      return false;
    }
  return false;
}
#endif
} // namespace

TEST (LumexDwcasCpuTest,
      GivenRegisterValues_WhenTestedForTheBit_ThenOnlyBit13Counts)
{
  EXPECT_FALSE (dwcas_detail::ecx_has_cmpxchg16b (0u));
  EXPECT_TRUE (dwcas_detail::ecx_has_cmpxchg16b (1u << 13));
  EXPECT_TRUE (dwcas_detail::ecx_has_cmpxchg16b (0xFFFFFFFFu));
  EXPECT_FALSE (dwcas_detail::ecx_has_cmpxchg16b (0xFFFFFFFFu & ~(1u << 13)));
  EXPECT_FALSE (dwcas_detail::ecx_has_cmpxchg16b (1u << 12));
  EXPECT_FALSE (dwcas_detail::ecx_has_cmpxchg16b (1u << 14));
  EXPECT_EQ (dwcas_detail::cmpxchg16b_ecx_bit, 0x2000u);
}

TEST (LumexDwcasCpuTest,
      GivenThisMachine_WhenAskedForSupport_ThenTheKernelAgrees)
{
#if defined(__linux__)
  bool known = false;
  bool const kernel = kernel_says_cx16 (known);
  if (!known)
    GTEST_SKIP () << "/proc/cpuinfo has no flags line";
  EXPECT_EQ (dwcas_supported (), kernel)
      << "the layer and /proc/cpuinfo (cx16) must agree";
  EXPECT_EQ (
      dwcas_detail::ecx_has_cmpxchg16b (dwcas_detail::read_cpuid_leaf1_ecx ()),
      kernel);
#else
  SUCCEED ();
#endif
}

TEST (LumexDwcasCpuTest, GivenSupportedCpu_WhenRequired_ThenItReturns)
{
  if (!dwcas_supported ())
    GTEST_SKIP () << "this CPU has no CMPXCHG16B";
  require_dwcas ();
  require_dwcas ();
  SUCCEED ();
}

TEST (LumexDwcasCpuTest,
      GivenFakeCpuWithTheBit_WhenAsked_ThenSupportedAndReadOnce)
{
  FakeCpu fake (&cpu_with_the_bit);
  EXPECT_TRUE (dwcas_supported ());
  for (int i = 0; i < 1000; ++i)
    EXPECT_TRUE (dwcas_supported ());
  EXPECT_EQ (g_reads.load (), 1) << "the answer is cached";
  require_dwcas ();
  EXPECT_EQ (g_reads.load (), 1);
}

TEST (LumexDwcasCpuTest,
      GivenFakeCpuWithoutTheBit_WhenAsked_ThenNotSupportedAndReadOnce)
{
  FakeCpu fake (&cpu_without_the_bit);
  EXPECT_FALSE (dwcas_supported ());
  for (int i = 0; i < 1000; ++i)
    EXPECT_FALSE (dwcas_supported ());
  EXPECT_EQ (g_reads.load (), 1) << "the negative answer is cached too";
}

TEST (LumexDwcasCpuTest, GivenFakeCpuWithNoLeaf_WhenAsked_ThenNotSupported)
{
  FakeCpu fake (&cpu_with_nothing);
  EXPECT_FALSE (dwcas_supported ());
}

TEST (LumexDwcasCpuTest, GivenResetProbe_WhenAskedAgain_ThenTheCpuIsReadAgain)
{
  FakeCpu fake (&cpu_with_the_bit);
  EXPECT_TRUE (dwcas_supported ());
  dwcas_detail::cpuid_reader () = &cpu_without_the_bit;
  EXPECT_TRUE (dwcas_supported ()) << "still the cached answer";
  dwcas_detail::reset_cpu_probe ();
  EXPECT_FALSE (dwcas_supported ()) << "read again after the reset";
}

TEST (LumexDwcasCpuTest,
      GivenThreadsRacingOnTheFirstCall_WhenAsked_ThenAllGetTheSameAnswer)
{
  for (int round = 0; round < lumex_test::scaled (200); ++round)
    {
      FakeCpu fake (round % 2 == 0 ? &cpu_with_the_bit : &cpu_without_the_bit);
      bool const expected = round % 2 == 0;
      std::atomic<int> wrong (0);
      std::string const error
          = lumex_test::run_threads (8,
                                     [&] (int)
                                       {
                                         if (dwcas_supported () != expected)
                                           wrong.fetch_add (1);
                                       });
      ASSERT_TRUE (error.empty ()) << error;
      ASSERT_EQ (wrong.load (), 0) << "round " << round;
      ASSERT_GE (g_reads.load (), 1);
      ASSERT_LE (g_reads.load (), 8) << "at most one read per racing thread";
    }
}

TEST (LumexDwcasCpuTest,
      GivenTheMessage_WhenRead_ThenItNamesTheInstructionAndTheSwitch)
{
  std::string const message = dwcas_detail::missing_cpu_message ();
  EXPECT_NE (message.find ("CMPXCHG16B"), std::string::npos);
  EXPECT_NE (message.find ("LUMEX_ATOMIC_DISABLE_DWCAS"), std::string::npos);
  EXPECT_NE (message.find ("lumex::core::atomic::dwcas"), std::string::npos);
  ASSERT_FALSE (message.empty ());
  EXPECT_EQ (message[message.size () - 1], '\n') << "one complete line";
  EXPECT_EQ (message.find ('\n'), message.size () - 1) << "exactly one line";
}

#if defined(__unix__) || defined(__APPLE__)
namespace
{
/// Points stderr of the child at a file; returns false when it cannot.
bool
redirect_stderr_to (char const *path)
{
  int const fd = open (path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd < 0)
    return false;
  return dup2 (fd, 2) == 2;
}

std::string
read_file (std::string const &path)
{
  std::ifstream in (path.c_str ());
  std::ostringstream text;
  text << in.rdbuf ();
  return text.str ();
}

std::string
scratch_path (char const *tag)
{
  return std::string ("/tmp/lumex_dwcas_cpu_") + tag + "_"
         + std::to_string (static_cast<long> (getpid ())) + ".txt";
}
} // namespace

TEST (LumexDwcasCpuTest,
      GivenFakeCpuWithoutTheBit_WhenRequired_ThenTheChildAbortsWithTheMessage)
{
  if (!lumex_test::subprocess_available ())
    GTEST_SKIP () << "no fork on this platform";
  std::string const path = scratch_path ("abort");
  lumex_test::ChildResult const result = lumex_test::run_in_child (
      [&]
        {
          if (!redirect_stderr_to (path.c_str ()))
            return 3;
          FakeCpu fake (&cpu_without_the_bit);
          require_dwcas ();
          return 0; // not reached: the call aborts
        });
  std::string const written = read_file (path);
  std::remove (path.c_str ());
  EXPECT_EQ (result.end, lumex_test::ChildEnd::signaled)
      << lumex_test::describe_child (result);
  EXPECT_EQ (result.code, SIGABRT);
  EXPECT_EQ (written, std::string (dwcas_detail::missing_cpu_message ()))
      << "the message and nothing else goes to stderr";
}

TEST (
    LumexDwcasCpuTest,
    GivenFakeCpuWithTheBit_WhenRequired_ThenTheChildExitsCleanlyWithoutOutput)
{
  if (!lumex_test::subprocess_available ())
    GTEST_SKIP () << "no fork on this platform";
  std::string const path = scratch_path ("clean");
  lumex_test::ChildResult const result = lumex_test::run_in_child (
      [&]
        {
          if (!redirect_stderr_to (path.c_str ()))
            return 3;
          FakeCpu fake (&cpu_with_the_bit);
          require_dwcas ();
          return 0;
        });
  std::string const written = read_file (path);
  std::remove (path.c_str ());
  EXPECT_EQ (result.end, lumex_test::ChildEnd::clean)
      << lumex_test::describe_child (result);
  EXPECT_TRUE (written.empty ()) << written;
}

TEST (LumexDwcasCpuTest,
      GivenDirectTermination_WhenCalledInAChild_ThenItAbortsAfterTheMessage)
{
  if (!lumex_test::subprocess_available ())
    GTEST_SKIP () << "no fork on this platform";
  std::string const path = scratch_path ("direct");
  lumex_test::ChildResult const result = lumex_test::run_in_child (
      [&]
        {
          if (!redirect_stderr_to (path.c_str ()))
            return 3;
          dwcas_detail::terminate_without_cmpxchg16b ();
        });
  std::string const written = read_file (path);
  std::remove (path.c_str ());
  EXPECT_EQ (result.end, lumex_test::ChildEnd::signaled);
  EXPECT_EQ (written, std::string (dwcas_detail::missing_cpu_message ()));
}
#endif

#else

TEST (LumexDwcasCpuTest,
      GivenTargetWithoutTheLayer_WhenBuilt_ThenThereIsNothingToRun)
{
  GTEST_SKIP () << "LUMEX_ATOMIC_HAS_DWCAS is 0 on this target";
}

#endif // LUMEX_ATOMIC_HAS_DWCAS
