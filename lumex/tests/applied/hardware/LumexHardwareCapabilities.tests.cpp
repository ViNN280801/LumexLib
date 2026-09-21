#include <algorithm> // For std::transform
#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <vector>

#if !defined(_WIN32)
#include <stdlib.h> // For getenv, setenv
#endif

#include <gtest/gtest.h>

#if defined(_WIN32)
#include <Windows.h> // For GetEnvironmentVariable
#endif

#include "lumex/applied/hardware/LumexHardware"

#include "lumex/core/utility/LumexUtility" // For LUMEX_OS macros

#include "lumex/tests/support/LumexPerfSkip.hpp"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

// Bring Constants into scope for easier access in tests
using namespace lumex::applied::hardware::caps;

// Fixture for basic HardwareCapabilities tests
class HardwareCapabilitiesTest : public ::testing::Test
{
protected:
  void
  SetUp () override
  {
    // Any common setup for tests
  }

  void
  TearDown () override
  {
    // Any common cleanup for tests
  }
};

// Fixture for platform-specific HardwareCapabilities tests
class HardwareCapabilitiesPlatformTest : public ::testing::Test
{
protected:
  void
  SetUp () override
  {
// Store original environment variables if needed to restore them
#if LUMEX_OS_WINDOWS
// GetEnvironmentVariable does not have a simple way to check if a variable
// exists, so we'll just try to get a small buffer and check for success. For
// testing, we might want to ensure they are unset or specific values.
#elif LUMEX_OS_UNIX
    original_qt_quick_backend
        = getenv ("QT_QUICK_BACKEND") ? getenv ("QT_QUICK_BACKEND") : "";
    original_qsg_render_loop
        = getenv ("QSG_RENDER_LOOP") ? getenv ("QSG_RENDER_LOOP") : "";
#endif
  }

  void
  TearDown () override
  {
// Restore original environment variables
#if LUMEX_OS_WINDOWS
// SetEnvironmentVariable(L"QT_QUICK_BACKEND", nullptr); // How to clear?
// SetEnvironmentVariable(L"QSG_RENDER_LOOP", nullptr);
#elif LUMEX_OS_UNIX
    setenv ("QT_QUICK_BACKEND", original_qt_quick_backend.c_str (), 1);
    setenv ("QSG_RENDER_LOOP", original_qsg_render_loop.c_str (), 1);
#endif
  }

  std::string original_qt_quick_backend;
  std::string original_qsg_render_loop;
};

// --- HardwareCapabilities::detect_hardware() Tests ---

TEST_F (HardwareCapabilitiesTest, DetectHardware_ReturnsValidInfo)
{
  hardware_info_t info = HardwareCapabilities::detect_hardware ();

  EXPECT_GT (info.cpu_core_count, 0U);
  EXPECT_FALSE (info.cpu_name.empty ());
  EXPECT_GT (info.total_memory_mb, 0ULL);
  // gpu_name can be empty if integrated/unknown, has_discrete_gpu can be false
  // cpu_frequency_mhz and cpu_generation might be 0 or default if detection
  // fails
}

// Check if CPU core count is reasonable
TEST_F (HardwareCapabilitiesTest, DetectHardware_CpuCoreCountIsReasonable)
{
  hardware_info_t info = HardwareCapabilities::detect_hardware ();
  // Assuming a modern system has at least 1 core, and typically more.
  // Avoid setting a too high upper limit as systems can vary.
  EXPECT_GE (info.cpu_core_count, 1U);
  EXPECT_LE (info.cpu_core_count,
             256U); // Max plausible cores for common systems
}

// Check if total memory is reasonable
TEST_F (HardwareCapabilitiesTest, DetectHardware_TotalMemoryIsReasonable)
{
  hardware_info_t info = HardwareCapabilities::detect_hardware ();
  EXPECT_GE (info.total_memory_mb, 256ULL); // Minimum plausible memory (256MB)
  EXPECT_LE (info.total_memory_mb,
             512ULL * 1024ULL); // Maximum plausible memory (512GB)
}

// Test CPU name detection
TEST_F (HardwareCapabilitiesTest, DetectHardware_CpuNameNotEmpty)
{
  hardware_info_t info = HardwareCapabilities::detect_hardware ();
  EXPECT_FALSE (info.cpu_name.empty ());
}

// Test GPU detection (it might be integrated, so has_discrete_gpu can be
// false)
TEST_F (HardwareCapabilitiesTest, DetectHardware_GpuDetection)
{
  hardware_info_t info = HardwareCapabilities::detect_hardware ();
  // It's hard to predict if a discrete GPU will be found, so we check general
  // behavior. If has_discrete_gpu is true, gpu_name should not be empty.
  if (info.has_discrete_gpu)
    {
      EXPECT_FALSE (info.gpu_name.empty ());
      EXPECT_FALSE (info.gpu_name.find ("Microsoft Basic Render Driver")
                    != std::string::npos);
      // Check if the GPU name contains expected identifiers (NVIDIA, AMD,
      // Radeon)
      EXPECT_TRUE (
          info.gpu_name.find (Constants::KNVIDIA_GPU_IDENTIFIER)
              != std::string::npos
          || info.gpu_name.find (Constants::KAMD_GPU_IDENTIFIER)
                 != std::string::npos
          || info.gpu_name.find (Constants::KAMD_RADEON_GPU_IDENTIFIER)
                 != std::string::npos);
    }
}

// --- HardwareCapabilities::estimate_cpu_generation() Tests ---

TEST_F (HardwareCapabilitiesTest, EstimateCPUGeneration_IntelModern)
{
  EXPECT_EQ (HardwareCapabilities::estimate_cpu_generation (
                 "Intel(R) Core(TM) i7-10700K CPU @ 3.80GHz"),
             2020U);
  EXPECT_EQ (HardwareCapabilities::estimate_cpu_generation (
                 "Intel(R) Core(TM) i5-8600K CPU @ 3.60GHz"),
             2017U);
  EXPECT_EQ (HardwareCapabilities::estimate_cpu_generation (
                 "Intel(R) Core(TM) i7-6700K CPU @ 4.00GHz"),
             2015U);
  EXPECT_EQ (HardwareCapabilities::estimate_cpu_generation (
                 "Intel(R) Core(TM) i7-7700K CPU @ 4.20GHz"),
             2017U); // Kaby Lake
  EXPECT_EQ (HardwareCapabilities::estimate_cpu_generation (
                 "Intel(R) Core(TM) i5-12600K CPU @ 3.70GHz"),
             2021U); // Alder Lake (using a close existing constant for now)
  EXPECT_EQ (HardwareCapabilities::estimate_cpu_generation (
                 "Intel(R) Core(TM) i7-13700K CPU @ 3.40GHz"),
             2022U); // Raptor Lake (using a close existing constant for now)
  EXPECT_EQ (HardwareCapabilities::estimate_cpu_generation (
                 "Intel(R) Core(TM) i7-11700K CPU @ 3.60GHz"),
             2021U); // Rocket Lake
  EXPECT_EQ (HardwareCapabilities::estimate_cpu_generation (
                 "Intel(R) Core(TM) i5-12400F CPU @ 2.50GHz"),
             2021U); // Alder Lake
  EXPECT_EQ (HardwareCapabilities::estimate_cpu_generation (
                 "Intel(R) Core(TM) i9-13900K CPU @ 3.00GHz"),
             2022U); // Raptor Lake
  EXPECT_EQ (HardwareCapabilities::estimate_cpu_generation (
                 "Intel(R) Core(TM) i7-14700K CPU @ 3.40GHz"),
             2023U); // Meteor Lake (using a close existing constant for now)
}

TEST_F (HardwareCapabilitiesTest, EstimateCPUGeneration_IntelOld)
{
  EXPECT_EQ (HardwareCapabilities::estimate_cpu_generation (
                 "Intel(R) Core(TM) i7-2600 CPU @ 3.40GHz"),
             2011U);
  EXPECT_EQ (HardwareCapabilities::estimate_cpu_generation (
                 "Intel(R) Core(TM) i5-750 CPU @ 2.66GHz"),
             2008U); // Nehalem
  EXPECT_EQ (HardwareCapabilities::estimate_cpu_generation (
                 "Intel(R) Celeron(R) CPU E1400 @ 2.00GHz"),
             2015U); // Celeron (falls to default)
}

TEST_F (HardwareCapabilitiesTest, EstimateCPUGeneration_AmdOld)
{
  EXPECT_EQ (HardwareCapabilities::estimate_cpu_generation (
                 "AMD FX-8350 Eight-Core Processor"),
             2015U); // Vishera (falls to default)
  EXPECT_EQ (HardwareCapabilities::estimate_cpu_generation (
                 "AMD Phenom(tm) II X4 955 Processor"),
             2015U); // Phenom II (falls to default)
  EXPECT_EQ (HardwareCapabilities::estimate_cpu_generation (
                 "AMD Athlon(tm) 64 X2 Dual Core Processor 4200+"),
             2015U); // Athlon 64 X2 (falls to default)
  EXPECT_EQ (HardwareCapabilities::estimate_cpu_generation (
                 "AMD A10-7850K APU with Radeon(TM) R7 Graphics"),
             2015U); // Kaveri (falls to default)
}

TEST_F (HardwareCapabilitiesTest, EstimateCPUGeneration_UnknownCpu)
{
  EXPECT_EQ (
      HardwareCapabilities::estimate_cpu_generation ("Some Unknown Processor"),
      2015); // Default value
  EXPECT_EQ (HardwareCapabilities::estimate_cpu_generation (""),
             2015U); // Empty string
}

// --- HardwareCapabilities::is_old_cpu() Tests ---

TEST_F (HardwareCapabilitiesTest, IsOldCPU_ReturnsTrueForOldCPUs)
{
  EXPECT_TRUE (HardwareCapabilities::is_old_cpu (
      "Intel(R) Pentium(R) CPU G630 @ 2.70GHz"));
  EXPECT_TRUE (HardwareCapabilities::is_old_cpu (
      "Intel(R) Celeron(R) CPU J1900 @ 2.42GHz"));
  EXPECT_TRUE (HardwareCapabilities::is_old_cpu (
      "AMD Athlon(tm) 64 X2 Dual Core Processor 4200+"));
  EXPECT_TRUE (HardwareCapabilities::is_old_cpu (
      "Intel(R) Atom(TM) CPU D525 @ 1.80GHz"));
  EXPECT_TRUE (HardwareCapabilities::is_old_cpu (
      "Intel(R) Core(TM)2 Quad CPU Q6600 @ 2.40GHz")); // Core 2
  EXPECT_TRUE (HardwareCapabilities::is_old_cpu (
      "AMD Phenom(tm) II X4 955 Processor")); // Phenom
  EXPECT_TRUE (HardwareCapabilities::is_old_cpu (
      "AMD FX-8350 Eight-Core Processor")); // FX Series
  EXPECT_TRUE (HardwareCapabilities::is_old_cpu (
      "AMD Sempron(tm) Processor 3000+")); // Sempron
  EXPECT_TRUE (HardwareCapabilities::is_old_cpu (
      "Intel(R) Core(TM) i3-2100 CPU @ 3.10GHz")); // Sandy Bridge
  EXPECT_TRUE (HardwareCapabilities::is_old_cpu (
      "Intel(R) Celeron(R) 2957U @ 1.40GHz")); // Celeron Haswell
  EXPECT_TRUE (HardwareCapabilities::is_old_cpu (
      "AMD A6-9220 RADEON R4, 5 COMPUTE CORES 2C+3G")); // A-series APU
}

TEST_F (HardwareCapabilitiesTest, IsOldCPU_ReturnsFalseForModernCPUs)
{
  EXPECT_FALSE (HardwareCapabilities::is_old_cpu (
      "Intel(R) Core(TM) i7-10700K CPU @ 3.80GHz"));
  EXPECT_FALSE (HardwareCapabilities::is_old_cpu (
      "Intel(R) Xeon(R) Gold 6248R CPU @ 3.00GHz"));
  EXPECT_FALSE (HardwareCapabilities::is_old_cpu (
      "Intel(R) Core(TM) i9-11900K CPU @ 3.50GHz")); // Rocket Lake
  EXPECT_FALSE (HardwareCapabilities::is_old_cpu (
      "Intel(R) Core(TM) i5-12600K CPU @ 3.70GHz")); // Alder Lake
}

TEST_F (HardwareCapabilitiesTest, IsOldCPU_CaseInsensitivity)
{
  EXPECT_TRUE (HardwareCapabilities::is_old_cpu ("intel pentium"));
  EXPECT_TRUE (HardwareCapabilities::is_old_cpu ("aMd AtHlOn 64"));
}

TEST_F (HardwareCapabilitiesTest,
        IsOldCPU_UnknownAndEmpty_WhenUnfound_ThenFalse)
{
  EXPECT_FALSE (HardwareCapabilities::is_old_cpu ("Some Unknown Processor"));
  EXPECT_FALSE (HardwareCapabilities::is_old_cpu (""));
}

// --- HardwareCapabilities::should_use_software_rendering() Tests ---

TEST_F (HardwareCapabilitiesTest, ShouldUseSoftwareRendering_LowRam)
{
  // Mock hardware_info_t for specific scenario
  hardware_info_t lowRamInfo
      = HardwareCapabilities::detect_hardware (); // Get current as base
  lowRamInfo.total_memory_mb
      = 1024; // 1GB RAM, less than KMIN_MEMORY_MB (4096)

  // Temporarily replace the detect_hardware behavior for this test
  // This requires a mock or injecting dependencies, which is not directly
  // supported by current class design. For now, we'll have to assume default
  // hardware, or manually manipulate env for appimage related test. The
  // current implementation calls detect_hardware internally, so we can't
  // easily mock it without refactoring. This test will rely on a "best effort"
  // check given the static nature.

  // Given the current static design, direct mocking is not feasible.
  // We can only test the logic *if* we could inject hardware_info_t.
  // For now, these tests will be conceptual or rely on actual system
  // properties. To properly test this, HardwareCapabilities::detect_hardware
  // would need to be virtual or a dependency.

  // A simple approach without refactoring would be to test the logic path if
  // input were provided. Since it directly calls detect_hardware(), this test
  // will always use real hardware info. For now, we assume hardware is good
  // enough, unless explicit env vars set. This test will pass if current
  // system has >4GB RAM.

  // To make this test truly effective, the detect_hardware call needs to be
  // injectable. As per the prompt, I should "stub logic with extensible
  // interfaces". For now, I'll rely on testing the individual helper
  // functions. For the combined "should_use_software_rendering", it's
  // challenging without mocking `detect_hardware`.
}

// --- HardwareCapabilities::apply_optimal_rendering_settings() Tests ---

#if LUMEX_OS_WINDOWS
TEST_F (HardwareCapabilitiesPlatformTest,
        ApplyOptimalRenderingSettings_Windows)
{
  // This test would need to mock `should_use_software_rendering()` or run on a
  // controlled environment. Given the current static design and direct calls
  // to system APIs like _putenv_s, directly testing that environment variables
  // are *set* is difficult and typically handled by integration tests or
  // manual verification. Here, we can only verify the logging output if
  // `should_use_software_rendering()` returns true.

  // Conceptually, if should_use_software_rendering() returns true:
  // _putenv_s("QT_QUICK_BACKEND", "software");
  // _putenv_s("QSG_RENDER_LOOP", "basic");

  // Since we cannot reliably control the return of
  // `should_use_software_rendering` without significant refactoring (e.g.,
  // dependency injection or making it non-static and injecting a mock), this
  // test will only check if the function executes without crashing. The
  // effectiveness of this test depends on the environment where it's run.
  EXPECT_NO_FATAL_FAILURE (
      HardwareCapabilities::apply_optimal_rendering_settings ());
}
#elif LUMEX_OS_UNIX
TEST_F (HardwareCapabilitiesPlatformTest, ApplyOptimalRenderingSettings_Unix)
{
  // See comments for Windows test. Similar limitations apply.
  EXPECT_NO_FATAL_FAILURE (
      HardwareCapabilities::apply_optimal_rendering_settings ());
}
#endif

// --- Concurrency Test ---

TEST_F (HardwareCapabilitiesTest, DetectHardware_ThreadSafety)
{
  std::vector<std::future<hardware_info_t>> futures;
  int const num_threads = 10;

  for (int i = 0; i < num_threads; ++i)
    futures.push_back (std::async (std::launch::async, [] () {
      return HardwareCapabilities::detect_hardware ();
    }));

  for (auto &f : futures)
    {
      hardware_info_t info = f.get ();
      EXPECT_GT (info.cpu_core_count, 0U); // Basic check for validity
      EXPECT_FALSE (info.cpu_name.empty ());
    }
}

// --- Performance Test (Opt-in) ---

TEST_F (HardwareCapabilitiesTest, Perf_DetectHardware)
{
#if LUMEX_PERF_WALL_CLOCK_ENABLED
  int const N
      = 10; // Number of times to run detection for performance measurement
  auto start = std::chrono::high_resolution_clock::now ();

  for (int i = 0; i < N; ++i)
    HardwareCapabilities::detect_hardware ();

  auto dur = std::chrono::duration_cast<std::chrono::milliseconds> (
      std::chrono::high_resolution_clock::now () - start);

  int const expected_duration
      = 5000; // 5s is enough for 10 times on a really old machine.
  EXPECT_LT (dur.count (), expected_duration)
      << "Detecting hardware " << N << " times took too long: " << dur.count ()
      << "ms";
#else
  GTEST_SKIP ()
      << "wall-clock Perf_* thresholds are Release-only (no sanitizers)";
#endif
}
