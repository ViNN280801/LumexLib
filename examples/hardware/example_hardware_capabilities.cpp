#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "lumex/applied/hardware/LumexHardwareCapabilities.hpp"

using namespace Lumex::Applied::Hardware;

namespace
{
  // Constants for formatting
  constexpr uint64_t kMBToGBFactor   = 1024ULL; ///< Conversion factor from MB to GB
  constexpr uint32_t kMHzToGHzFactor = 1000U;   ///< Conversion factor from MHz to GHz
  constexpr int kColumnWidth         = 50;      ///< Column width for formatted output
  constexpr int kSeedCount           = 5;       ///< Number of seeds to generate in example
  constexpr int kHexWidth            = 16;      ///< Width for hexadecimal output
  constexpr int kLongCpuNamePrefix   = 1000;    ///< Prefix length for long CPU name test

  /**
   * @brief Helper function to print boolean status with Yes/No
   * @param value Boolean value to print
   * @return String representation ("Yes" or "No")
   */
  inline std::string
  // NOLINTNEXTLINE(readability-identifier-naming)
  _boolToYesNo(bool value)
  {
    return value ? "Yes" : "No";
  }

  /**
   * @brief Helper function to format memory size in human-readable format
   * @param sizeMB Memory size in megabytes
   * @return Formatted string (e.g., "8.0 GB")
   */
  inline std::string
  formatMemorySize(uint64_t sizeMB)
  {
    if(sizeMB >= kMBToGBFactor)
    {
      double sizeGB = static_cast<double>(sizeMB) / static_cast<double>(kMBToGBFactor);
      std::ostringstream oss;
      oss << std::fixed << std::setprecision(1) << sizeGB << " GB";
      return oss.str();
    }
    return std::to_string(sizeMB) + " MB";
  }

  /**
   * @brief Helper function to format CPU frequency in human-readable format
   * @param freqMHz CPU frequency in megahertz
   * @return Formatted string (e.g., "3.7 GHz")
   */
  inline std::string
  formatCPUFrequency(uint32_t freqMHz)
  {
    if(freqMHz >= kMHzToGHzFactor)
    {
      double freqGHz = static_cast<double>(freqMHz) / static_cast<double>(kMHzToGHzFactor);
      std::ostringstream oss;
      oss << std::fixed << std::setprecision(2) << freqGHz << " GHz";
      return oss.str();
    }
    return std::to_string(freqMHz) + " MHz";
  }
}

int
main()
{
  std::cout << "=== Hardware Capabilities Detection - Comprehensive Example ===\n\n";

  // ============================================================================
  // Example 1: Basic Hardware Detection
  // ============================================================================
  std::cout << "--- Example 1: Basic Hardware Detection ---\n";
  std::cout << "Detecting system hardware...\n\n";

  HardwareInfo hwInfo = HardwareCapabilities::detectHardware();

  std::cout << "Detected Hardware Information:\n";
  std::cout << "  CPU Name:        " << hwInfo.cpuName << "\n";
  std::cout << "  CPU Cores:       " << hwInfo.cpuCoreCount << "\n";
  std::cout << "  CPU Frequency:   " << formatCPUFrequency(hwInfo.cpuFrequencyMHz) << "\n";
  std::cout << "  Total Memory:    " << formatMemorySize(hwInfo.totalMemoryMB) << "\n";
  std::cout << "  Has Discrete GPU: " << _boolToYesNo(hwInfo.hasDiscreteGPU) << "\n";
  std::cout << "  GPU Name:        " << (hwInfo.gpuName.empty() ? "N/A" : hwInfo.gpuName) << "\n";
  std::cout << "  CPU Generation:  ~" << hwInfo.cpuGeneration << "\n";
  std::cout << "\n";

  // ============================================================================
  // Example 2: CPU Generation Estimation
  // ============================================================================
  std::cout << "--- Example 2: CPU Generation Estimation ---\n";
  std::cout << "Testing CPU generation estimation with various CPU names:\n\n";

  // Test Intel CPUs
  std::vector<std::string> testCPUNames = {
    "Intel(R) Core(TM) i7-8700K CPU @ 3.70GHz",    // Coffee Lake (8th Gen) - 2017
    "Intel(R) Core(TM) i5-10400 CPU @ 2.90GHz",    // Comet Lake (10th Gen) - 2020
    "Intel(R) Core(TM) i7-11700K CPU @ 3.60GHz",   // Rocket Lake (11th Gen) - 2021
    "Intel(R) Core(TM) i9-12900K CPU @ 3.20GHz",   // Alder Lake (12th Gen) - 2021
    "Intel(R) Core(TM) i7-13700K CPU @ 3.40GHz",   // Raptor Lake (13th Gen) - 2022
    "Intel(R) Core(TM) i5-14400 CPU @ 2.50GHz",    // Meteor Lake (14th Gen) - 2023
    "Intel(R) Core(TM) i5-6500 CPU @ 3.20GHz",     // Skylake (6th Gen) - 2015
    "Intel(R) Core(TM) i7-7700K CPU @ 4.20GHz",    // Kaby Lake (7th Gen) - 2017
    "Intel(R) Core(TM) i3-3220 CPU @ 3.30GHz",     // Ivy Bridge (3rd Gen) - 2012
    "Intel(R) Core(TM) i5-2500K CPU @ 3.30GHz",    // Sandy Bridge (2nd Gen) - 2011
    "Intel(R) Core(TM) i7-4770K CPU @ 3.50GHz",    // Haswell (4th Gen) - 2013
    "Intel(R) Core(TM)2 Quad CPU Q6600 @ 2.40GHz", // Core 2 Quad - 2008
    "Intel(R) Pentium(R) CPU G2020 @ 2.90GHz",     // Pentium G (Ivy Bridge) - 2012
    "AMD Ryzen 5 3600 6-Core Processor",           // Ryzen 3000 - 2019
    "AMD Ryzen 7 2700X Eight-Core Processor",      // Ryzen 2000 - 2018
    "AMD Ryzen 9 5900X 12-Core Processor",         // Ryzen 5000 - 2020
    "AMD Ryzen 7 7800X3D 8-Core Processor",        // Ryzen 7000 - 2022
    "AMD Ryzen 5 5600G with Radeon Graphics",      // Ryzen 5000 APU - 2020
    "Unknown CPU Model XYZ-12345",                 // Unknown - should return default
  };

  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization, bugprone-easily-swappable-parameters)
  for(auto const &cpuName : testCPUNames)
  {
    uint32_t generation = HardwareCapabilities::estimateCPUGeneration(cpuName);
    std::cout << "  " << std::setw(kColumnWidth) << std::left << cpuName << " -> Generation: ~" << generation << "\n";
  }
  std::cout << "\n";

  // ============================================================================
  // Example 3: Old CPU Detection
  // ============================================================================
  std::cout << "--- Example 3: Old CPU Detection ---\n";
  std::cout << "Testing old CPU detection with various CPU names:\n\n";

  std::vector<std::string> oldCPUTestNames = {
    "Intel(R) Pentium(R) CPU G4400 @ 3.30GHz",        // Pentium - should be old
    "Intel(R) Celeron(R) CPU N3350 @ 1.10GHz",        // Celeron - should be old
    "Intel(R) Atom(TM) CPU N270 @ 1.60GHz",           // Atom - should be old
    "AMD Athlon(tm) 64 X2 Dual Core Processor 5000+", // Athlon 64 - should be old
    "AMD Phenom(tm) II X4 955 Processor",             // Phenom - should be old
    "AMD Sempron(tm) Processor 140",                  // Sempron - should be old
    "AMD FX(tm)-8350 Eight-Core Processor",           // FX series - should be old
    "AMD A10-7850K APU with Radeon R7 Graphics",      // A-series - should be old
    "Intel(R) Core(TM) i3-2100 CPU @ 3.10GHz",        // Sandy Bridge i3 - should be old
    "Intel(R) Core(TM) i7-8700K CPU @ 3.70GHz",       // Coffee Lake - should NOT be old
    "AMD Ryzen 5 3600 6-Core Processor",              // Ryzen 3000 - should NOT be old
  };

  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization, bugprone-easily-swappable-parameters)
  for(auto const &cpuName : oldCPUTestNames)
  {
    bool isOld = HardwareCapabilities::isOldCPU(cpuName);
    std::cout << "  " << std::setw(kColumnWidth) << std::left << cpuName << " -> Is Old: " << _boolToYesNo(isOld)
              << "\n";
  }
  std::cout << "\n";

  // ============================================================================
  // Example 4: Software Rendering Recommendation
  // ============================================================================
  std::cout << "--- Example 4: Software Rendering Recommendation ---\n";
  std::cout << "Checking if software rendering is recommended for current hardware...\n\n";

  bool useSoftware = HardwareCapabilities::shouldUseSoftwareRendering();
  std::cout << "Recommendation: " << (useSoftware ? "Use Software Rendering" : "Use Hardware Rendering") << "\n";
  std::cout << "\n";

  // Display threshold values for reference
  std::cout << "Threshold Values Used:\n";
  std::cout << "  Minimum Memory:        " << formatMemorySize(Constants::KMIN_MEMORY_MB) << "\n";
  std::cout << "  Minimum CPU Cores:     " << Constants::KMIN_CPU_CORES << "\n";
  std::cout << "  Minimum CPU Generation: " << Constants::KMIN_CPU_GENERATION << "\n";
  std::cout << "  High Memory Threshold: " << formatMemorySize(Constants::KHIGH_MEMORY_THRESHOLD_MB) << "\n";
  std::cout << "  High CPU Cores:        " << Constants::KHIGH_CPU_CORES << "\n";
  std::cout << "\n";

  // ============================================================================
  // Example 5: Apply Optimal Rendering Settings
  // ============================================================================
  std::cout << "--- Example 5: Apply Optimal Rendering Settings ---\n";
  std::cout << "Applying optimal rendering settings based on detected hardware...\n\n";

  HardwareCapabilities::applyOptimalRenderingSettings();

  std::cout << "Optimal rendering settings have been applied.\n";
  std::cout << "Note: This sets environment variables for Qt rendering backend.\n";
  std::cout << "\n";

  // ============================================================================
  // Example 6: MAC Address Detection
  // ============================================================================
  std::cout << "--- Example 6: MAC Address Detection ---\n";
  std::cout << "Retrieving MAC address of first active network interface...\n\n";

  std::string macAddress = getMacAddress();
  if(macAddress.empty())
    std::cout << "MAC Address: Not available (no active network interface found)\n";
  else
    std::cout << "MAC Address: " << macAddress << "\n";
  std::cout << "\n";

  // ============================================================================
  // Example 7: Cryptographic Seed Generation
  // ============================================================================
  std::cout << "--- Example 7: Cryptographic Seed Generation ---\n";
  std::cout << "Generating cryptographically secure random seeds...\n\n";

  for(int i = 0; i < kSeedCount; ++i)
  {
    std::uint64_t seed = generateCryptographicSeed();
    std::cout << "  Seed " << (i + 1) << ": 0x" << std::hex << std::setw(kHexWidth) << std::setfill('0') << seed
              << std::dec << " (" << seed
              << ")\n"; // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
  }
  std::cout << "\n";

  // ============================================================================
  // Example 8: Edge Cases and Validation
  // ============================================================================
  std::cout << "--- Example 8: Edge Cases and Validation ---\n";
  std::cout << "Testing edge cases and validation scenarios:\n\n";

  // Test with empty CPU name
  std::cout << "Testing with empty CPU name:\n";
  uint32_t genEmpty = HardwareCapabilities::estimateCPUGeneration("");
  bool isOldEmpty   = HardwareCapabilities::isOldCPU("");
  std::cout << "  Empty CPU name -> Generation: ~" << genEmpty << ", Is Old: " << _boolToYesNo(isOldEmpty) << "\n";

  // Test with very long CPU name
  std::string longCpuName(static_cast<size_t>(kLongCpuNamePrefix), 'A');
  longCpuName += "Intel(R) Core(TM) i7-8700K";
  uint32_t genLong = HardwareCapabilities::estimateCPUGeneration(longCpuName);
  std::cout << "  Very long CPU name -> Generation: ~" << genLong << "\n";

  // Test with case variations
  std::cout << "Testing case-insensitive detection:\n";
  std::string lowerCase = "intel(r) core(tm) i7-8700k cpu @ 3.70ghz";
  std::string upperCase = "INTEL(R) CORE(TM) I7-8700K CPU @ 3.70GHZ";
  uint32_t genLower     = HardwareCapabilities::estimateCPUGeneration(lowerCase);
  uint32_t genUpper     = HardwareCapabilities::estimateCPUGeneration(upperCase);
  bool isOldLower       = HardwareCapabilities::isOldCPU(lowerCase);
  bool isOldUpper       = HardwareCapabilities::isOldCPU(upperCase);
  std::cout << "  Lowercase -> Generation: ~" << genLower << ", Is Old: " << _boolToYesNo(isOldLower) << "\n";
  std::cout << "  Uppercase -> Generation: ~" << genUpper << ", Is Old: " << _boolToYesNo(isOldUpper) << "\n";
  std::cout << "\n";

  // ============================================================================
  // Example 9: Constants Verification
  // ============================================================================
  std::cout << "--- Example 9: Constants Verification ---\n";
  std::cout << "Displaying all hardware detection constants:\n\n";

  std::cout << "Buffer Sizes:\n";
  std::cout << "  CPU Info Buffer Size: " << Constants::KCPU_INFO_BUFFER_SIZE << " bytes\n";
  std::cout << "  lspci Buffer Size:    " << Constants::KLSPCI_BUFFER_SIZE << " bytes\n";
  std::cout << "\n";

  std::cout << "Hardware Detection Thresholds:\n";
  std::cout << "  Minimum Memory:        " << Constants::KMIN_MEMORY_MB << " MB\n";
  std::cout << "  Minimum CPU Cores:     " << Constants::KMIN_CPU_CORES << "\n";
  std::cout << "  Minimum CPU Generation: " << Constants::KMIN_CPU_GENERATION << "\n";
  std::cout << "  High Memory Threshold: " << Constants::KHIGH_MEMORY_THRESHOLD_MB << " MB\n";
  std::cout << "  High CPU Cores:        " << Constants::KHIGH_CPU_CORES << "\n";
  std::cout << "\n";

  std::cout << "Conversion Factors:\n";
  std::cout << "  Bytes to MB: " << Constants::KBYTES_TO_MB_FACTOR << "\n";
  std::cout << "  KB to MB:    " << Constants::KKB_TO_MB_FACTOR << "\n";
  std::cout << "  GHz to MHz:  " << Constants::KGHZ_TO_MHZ_FACTOR << "\n";
  std::cout << "\n";

  // ============================================================================
  // Example 10: Real-World Usage Scenario
  // ============================================================================
  std::cout << "--- Example 10: Real-World Usage Scenario ---\n";
  std::cout << "Simulating application startup hardware check:\n\n";

  // Simulate application startup
  std::cout << "[Application Startup]\n";
  std::cout << "1. Detecting hardware...\n";
  HardwareInfo startupHw = HardwareCapabilities::detectHardware();

  std::cout << "2. Analyzing hardware capabilities...\n";
  bool needsSoftwareRendering = HardwareCapabilities::shouldUseSoftwareRendering();

  std::cout << "3. Applying optimal settings...\n";
  HardwareCapabilities::applyOptimalRenderingSettings();

  std::cout << "4. Hardware check complete.\n";
  std::cout << "   Decision: " << (needsSoftwareRendering ? "Software Rendering" : "Hardware Rendering") << "\n";
  std::cout << "   System meets minimum requirements: " << _boolToYesNo(!needsSoftwareRendering) << "\n";
  std::cout << "\n";

  // ============================================================================
  // Summary
  // ============================================================================
  std::cout << "=== Summary ===\n";
  std::cout << "All hardware capabilities functions have been demonstrated:\n";
  std::cout << "  ✓ HardwareCapabilities::detectHardware()\n";
  std::cout << "  ✓ HardwareCapabilities::estimateCPUGeneration()\n";
  std::cout << "  ✓ HardwareCapabilities::isOldCPU()\n";
  std::cout << "  ✓ HardwareCapabilities::shouldUseSoftwareRendering()\n";
  std::cout << "  ✓ HardwareCapabilities::applyOptimalRenderingSettings()\n";
  std::cout << "  ✓ getMacAddress()\n";
  std::cout << "  ✓ generateCryptographicSeed()\n";
  std::cout << "\n";
  std::cout << "=== Example completed successfully ===\n";

  return EXIT_SUCCESS;
}
