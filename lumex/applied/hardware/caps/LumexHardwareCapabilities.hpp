/**
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy
 * of this software and associated
 * documentation files (the "Software"), to deal
 * in the Software without
 * restriction, including without limitation the rights
 * to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the
 * Software, and to permit persons to whom the Software is
 * furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice
 * and this permission notice shall be included in
 * all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT
 * WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO
 * THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef LUMEX_APPLIED_HARDWARE_CAPS_HARDWARE_CAPABILITIES_HPP
#define LUMEX_APPLIED_HARDWARE_CAPS_HARDWARE_CAPABILITIES_HPP

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
#endif

#include "lumex/LumexExport.hpp"

#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>

#include "lumex/core/utility/macros/LumexConstantMacros.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(_WIN32) || defined(WIN32)
#include <Windows.h>

#include <iphlpapi.h>
#include <wincrypt.h>

#include <vector>

#pragma comment(lib, "iphlpapi.lib")
#else
#include <arpa/inet.h>
#include <fstream>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
namespace hardware
{
namespace caps
{
namespace Constants
{
// Buffer sizes
LUMEX_CONSTEXPR short KCPU_INFO_BUFFER_SIZE
    = 0x40; ///< CPU brand string buffer size (64 bytes)
LUMEX_CONSTEXPR int KLSPCI_BUFFER_SIZE
    = 256; ///< Buffer size for lspci command output

// Hardware detection thresholds
LUMEX_CONSTEXPR uint64_t KMIN_MEMORY_MB
    = 4096; ///< Minimum RAM in MB for hardware rendering (4GB)
LUMEX_CONSTEXPR uint32_t KMIN_CPU_CORES
    = 2; ///< Minimum CPU cores for hardware rendering
LUMEX_CONSTEXPR uint32_t KMIN_CPU_GENERATION
    = 2012; ///< Minimum CPU generation year for hardware rendering
LUMEX_CONSTEXPR uint64_t KHIGH_MEMORY_THRESHOLD_MB
    = 8192; ///< High memory threshold in MB for GPU checks (8GB)
LUMEX_CONSTEXPR uint32_t KHIGH_CPU_CORES
    = 4; ///< High CPU core count threshold for GPU checks

// Memory conversion factors
LUMEX_CONSTEXPR uint64_t KBYTES_TO_MB_FACTOR
    = 1024ULL * 1024ULL; ///< Conversion factor from bytes to megabytes
LUMEX_CONSTEXPR uint64_t KKB_TO_MB_FACTOR
    = 1024ULL; ///< Conversion factor from kilobytes to megabytes

// Frequency conversion
LUMEX_CONSTEXPR uint32_t KGHZ_TO_MHZ_FACTOR
    = 1000; ///< Conversion factor from GHz to MHz

// CPUID function numbers
LUMEX_CONSTEXPR uint32_t KCPUID_EXTENDED_FEATURES
    = 0x80000000; ///< CPUID function for extended feature detection
LUMEX_CONSTEXPR uint32_t KCPUID_BRAND_STRING_1
    = 0x80000002; ///< CPUID function for CPU brand string part 1
LUMEX_CONSTEXPR uint32_t KCPUID_BRAND_STRING_2
    = 0x80000003; ///< CPUID function for CPU brand string part 2
LUMEX_CONSTEXPR uint32_t KCPUID_BRAND_STRING_3
    = 0x80000004; ///< CPUID function for CPU brand string part 3

// CPU brand string memory offsets
LUMEX_CONSTEXPR int KCPU_BRAND_OFFSET_16
    = 16; ///< Memory offset for second part of CPU brand string
LUMEX_CONSTEXPR int KCPU_BRAND_OFFSET_32
    = 32; ///< Memory offset for third part of CPU brand string

// CPU generation years
LUMEX_CONSTEXPR uint32_t KCPU_CORE2_GENERATION
    = 2008; ///< Intel Core 2 Duo/Quad generation year
LUMEX_CONSTEXPR uint32_t KCPU_NEHALEM_GENERATION
    = 2008; ///< Intel Nehalem generation year (1st Gen Core i-series)
LUMEX_CONSTEXPR uint32_t KCPU_SANDY_BRIDGE_GENERATION
    = 2011; ///< Intel Sandy Bridge generation year
LUMEX_CONSTEXPR uint32_t KCPU_IVY_BRIDGE_GENERATION
    = 2012; ///< Intel Ivy Bridge generation year
LUMEX_CONSTEXPR uint32_t KCPU_HASWELL_GENERATION
    = 2013; ///< Intel Haswell generation year
LUMEX_CONSTEXPR uint32_t KCPU_SKYLAKE_GENERATION
    = 2015; ///< Intel Skylake generation year
LUMEX_CONSTEXPR uint32_t KCPU_KABY_LAKE_GENERATION
    = 2017; ///< Intel Kaby Lake generation year (7th Gen)
LUMEX_CONSTEXPR uint32_t KCPU_COFFEE_LAKE_GENERATION
    = 2017; ///< Intel Coffee Lake generation year
LUMEX_CONSTEXPR uint32_t KCPU_COMET_LAKE_GENERATION
    = 2020; ///< Intel Comet Lake generation year
LUMEX_CONSTEXPR uint32_t KCPU_ROCKET_LAKE_GENERATION
    = 2021; ///< Intel Rocket Lake generation year (11th Gen)
LUMEX_CONSTEXPR uint32_t KCPU_ALDER_LAKE_GENERATION
    = 2021; ///< Intel Alder Lake generation year (12th Gen)
LUMEX_CONSTEXPR uint32_t KCPU_RAPTOR_LAKE_GENERATION
    = 2022; ///< Intel Raptor Lake generation year (13th Gen)
LUMEX_CONSTEXPR uint32_t KCPU_METEOR_LAKE_GENERATION
    = 2023; ///< Intel Meteor Lake generation year (14th Gen)
LUMEX_CONSTEXPR uint32_t KCPU_RYZEN_1000_GENERATION
    = 2017; ///< AMD Ryzen 1000 series generation year
LUMEX_CONSTEXPR uint32_t KCPU_RYZEN_2000_GENERATION
    = 2018; ///< AMD Ryzen 2000 series generation year
LUMEX_CONSTEXPR uint32_t KCPU_RYZEN_3000_GENERATION
    = 2019; ///< AMD Ryzen 3000 series generation year
LUMEX_CONSTEXPR uint32_t KCPU_RYZEN_4000_GENERATION
    = 2020; ///< AMD Ryzen 4000 (Renoir) series generation year
LUMEX_CONSTEXPR uint32_t KCPU_RYZEN_5000_GENERATION
    = 2020; ///< AMD Ryzen 5000 series generation year
LUMEX_CONSTEXPR uint32_t KCPU_RYZEN_6000_GENERATION
    = 2022; ///< AMD Ryzen 6000 (Rembrandt) series generation year
LUMEX_CONSTEXPR uint32_t KCPU_RYZEN_7000_GENERATION
    = 2022; ///< AMD Ryzen 7000 series generation year
LUMEX_CONSTEXPR uint32_t KCPU_DEFAULT_GENERATION
    = 2015; ///< Default CPU generation year for unknown processors

// Intel CPU model identifiers
LUMEX_CONSTEXPR char const *KINTEL_CORE2_IDENTIFIER
    = "Core 2"; ///< Intel Core 2 series identifier
LUMEX_CONSTEXPR char const *KINTEL_I3_2_IDENTIFIER
    = "i3-2"; ///< Intel i3 2nd gen identifier
LUMEX_CONSTEXPR char const *KINTEL_I5_2_IDENTIFIER
    = "i5-2"; ///< Intel i5 2nd gen identifier
LUMEX_CONSTEXPR char const *KINTEL_I7_2_IDENTIFIER
    = "i7-2"; ///< Intel i7 2nd gen identifier
LUMEX_CONSTEXPR char const *KINTEL_I3_3_IDENTIFIER
    = "i3-3"; ///< Intel i3 3rd gen identifier
LUMEX_CONSTEXPR char const *KINTEL_I5_3_IDENTIFIER
    = "i5-3"; ///< Intel i5 3rd gen identifier
LUMEX_CONSTEXPR char const *KINTEL_I7_3_IDENTIFIER
    = "i7-3"; ///< Intel i7 3rd gen identifier
LUMEX_CONSTEXPR char const *KINTEL_I3_4_IDENTIFIER
    = "i3-4"; ///< Intel i3 4th gen identifier
LUMEX_CONSTEXPR char const *KINTEL_I5_4_IDENTIFIER
    = "i5-4"; ///< Intel i5 4th gen identifier
LUMEX_CONSTEXPR char const *KINTEL_I7_4_IDENTIFIER
    = "i7-4"; ///< Intel i7 4th gen identifier
LUMEX_CONSTEXPR char const *KINTEL_I3_6_IDENTIFIER
    = "i3-6"; ///< Intel i3 6th gen identifier
LUMEX_CONSTEXPR char const *KINTEL_I5_6_IDENTIFIER
    = "i5-6"; ///< Intel i5 6th gen identifier
LUMEX_CONSTEXPR char const *KINTEL_I7_6_IDENTIFIER
    = "i7-6"; ///< Intel i7 6th gen identifier
LUMEX_CONSTEXPR char const *KINTEL_I3_8_IDENTIFIER
    = "i3-8"; ///< Intel i3 8th gen identifier
LUMEX_CONSTEXPR char const *KINTEL_I5_8_IDENTIFIER
    = "i5-8"; ///< Intel i5 8th gen identifier
LUMEX_CONSTEXPR char const *KINTEL_I7_8_IDENTIFIER
    = "i7-8"; ///< Intel i7 8th gen identifier
LUMEX_CONSTEXPR char const *KINTEL_I3_10_IDENTIFIER
    = "i3-10"; ///< Intel i3 10th gen identifier
LUMEX_CONSTEXPR char const *KINTEL_I5_10_IDENTIFIER
    = "i5-10"; ///< Intel i5 10th gen identifier
LUMEX_CONSTEXPR char const *KINTEL_I7_10_IDENTIFIER
    = "i7-10"; ///< Intel i7 10th gen identifier
LUMEX_CONSTEXPR char const *KINTEL_I3_11_IDENTIFIER
    = "i3-11"; ///< Intel i3 11th gen identifier (Rocket Lake)
LUMEX_CONSTEXPR char const *KINTEL_I5_11_IDENTIFIER
    = "i5-11"; ///< Intel i5 11th gen identifier (Rocket Lake)
LUMEX_CONSTEXPR char const *KINTEL_I7_11_IDENTIFIER
    = "i7-11"; ///< Intel i7 11th gen identifier (Rocket Lake)
LUMEX_CONSTEXPR char const *KINTEL_I3_12_IDENTIFIER
    = "i3-12"; ///< Intel i3 12th gen identifier (Alder Lake)
LUMEX_CONSTEXPR char const *KINTEL_I5_12_IDENTIFIER
    = "i5-12"; ///< Intel i5 12th gen identifier (Alder Lake)
LUMEX_CONSTEXPR char const *KINTEL_I7_12_IDENTIFIER
    = "i7-12"; ///< Intel i7 12th gen identifier (Alder Lake)
LUMEX_CONSTEXPR char const *KINTEL_I9_12_IDENTIFIER
    = "i9-12"; ///< Intel i9 12th gen identifier (Alder Lake)
LUMEX_CONSTEXPR char const *KINTEL_I3_13_IDENTIFIER
    = "i3-13"; ///< Intel i3 13th gen identifier (Raptor Lake)
LUMEX_CONSTEXPR char const *KINTEL_I5_13_IDENTIFIER
    = "i5-13"; ///< Intel i5 13th gen identifier (Raptor Lake)
LUMEX_CONSTEXPR char const *KINTEL_I7_13_IDENTIFIER
    = "i7-13"; ///< Intel i7 13th gen identifier (Raptor Lake)
LUMEX_CONSTEXPR char const *KINTEL_I9_13_IDENTIFIER
    = "i9-13"; ///< Intel i9 13th gen identifier (Raptor Lake)
LUMEX_CONSTEXPR char const *KINTEL_I3_7_IDENTIFIER
    = "i3-7"; ///< Intel i3 7th gen identifier (Kaby Lake)
LUMEX_CONSTEXPR char const *KINTEL_I5_7_IDENTIFIER
    = "i5-7"; ///< Intel i5 7th gen identifier (Kaby Lake)
LUMEX_CONSTEXPR char const *KINTEL_I7_7_IDENTIFIER
    = "i7-7"; ///< Intel i7 7th gen identifier (Kaby Lake)
LUMEX_CONSTEXPR char const *KINTEL_I3_14_IDENTIFIER
    = "i3-14"; ///< Intel i3 14th gen identifier (Meteor Lake)
LUMEX_CONSTEXPR char const *KINTEL_I5_14_IDENTIFIER
    = "i5-14"; ///< Intel i5 14th gen identifier (Meteor Lake)
LUMEX_CONSTEXPR char const *KINTEL_I7_14_IDENTIFIER
    = "i7-14"; ///< Intel i7 14th gen identifier (Meteor Lake)
LUMEX_CONSTEXPR char const *KINTEL_I9_14_IDENTIFIER
    = "i9-14"; ///< Intel i9 14th gen identifier (Meteor Lake)
LUMEX_CONSTEXPR char const *KINTEL_I5_750_IDENTIFIER
    = "i5-750"; ///< Intel Core i5-750 identifier (1st Gen)
LUMEX_CONSTEXPR char const *KINTEL_I7_9_IDENTIFIER
    = "i7-9"; ///< Intel Core i7-9xx identifier (1st Gen)
LUMEX_CONSTEXPR char const *KINTEL_PENTIUM_G_IDENTIFIER
    = "Pentium G"; ///< Intel Pentium G-series identifier (for Ivy Bridge
                   ///< based)

// AMD CPU identifiers
LUMEX_CONSTEXPR char const *KAMD_RYZEN_IDENTIFIER
    = "Ryzen"; ///< AMD Ryzen series identifier
LUMEX_CONSTEXPR char const *KAMD_RYZEN_1000_IDENTIFIER
    = "1000"; ///< AMD Ryzen 1000 series identifier
LUMEX_CONSTEXPR char const *KAMD_RYZEN_2000_IDENTIFIER
    = "2000"; ///< AMD Ryzen 2000 series identifier
LUMEX_CONSTEXPR char const *KAMD_RYZEN_3000_IDENTIFIER
    = "3000"; ///< AMD Ryzen 3000 series identifier
LUMEX_CONSTEXPR char const *KAMD_RYZEN_4000_IDENTIFIER
    = "4000"; ///< AMD Ryzen 4000 series identifier (Renoir)
LUMEX_CONSTEXPR char const *KAMD_RYZEN_5000_IDENTIFIER
    = "5000"; ///< AMD Ryzen 5000 series identifier
LUMEX_CONSTEXPR char const *KAMD_RYZEN_6000_IDENTIFIER
    = "6000"; ///< AMD Ryzen 6000 series identifier (Rembrandt)
LUMEX_CONSTEXPR char const *KAMD_RYZEN_7000_IDENTIFIER
    = "7000"; ///< AMD Ryzen 7000 series identifier

// GPU detection strings
LUMEX_CONSTEXPR char const *KMICROSOFT_GPU_IDENTIFIER
    = "Microsoft"; ///< Microsoft GPU driver identifier
LUMEX_CONSTEXPR char const *KBASIC_GPU_IDENTIFIER
    = "Basic"; ///< Basic render driver identifier
LUMEX_CONSTEXPR char const *KINTEL_GPU_IDENTIFIER
    = "Intel"; ///< Intel integrated graphics identifier
LUMEX_CONSTEXPR char const *KNVIDIA_GPU_IDENTIFIER
    = "NVIDIA"; ///< NVIDIA GPU identifier
LUMEX_CONSTEXPR char const *KAMD_GPU_IDENTIFIER
    = "AMD"; ///< AMD GPU identifier (general)
LUMEX_CONSTEXPR char const *KAMD_RADEON_GPU_IDENTIFIER
    = "Radeon"; ///< AMD Radeon GPU identifier

// Old CPU family identifiers
LUMEX_CONSTEXPR char const *KOLD_CPU_PENTIUM
    = "Pentium"; ///< Intel Pentium series identifier
LUMEX_CONSTEXPR char const *KOLD_CPU_CELERON
    = "Celeron"; ///< Intel Celeron series identifier
LUMEX_CONSTEXPR char const *KOLD_CPU_ATOM
    = "Atom"; ///< Intel Atom series identifier
LUMEX_CONSTEXPR char const *KOLD_CPU_ATHLON64
    = "Athlon 64"; ///< AMD Athlon 64 series identifier
LUMEX_CONSTEXPR char const *KOLD_CPU_PHENOM
    = "Phenom"; ///< AMD Phenom series identifier
LUMEX_CONSTEXPR char const *KOLD_CPU_SEMPRON
    = "Sempron"; ///< AMD Sempron series identifier
LUMEX_CONSTEXPR char const *KAMD_FX_IDENTIFIER
    = "FX-"; ///< AMD FX series identifier
LUMEX_CONSTEXPR char const *KAMD_A_SERIES_IDENTIFIER
    = "A"; ///< AMD A-series APU identifier (e.g., A6, A10)
}

/**
 * @brief Comprehensive hardware information structure containing system
 * specifications
 *
 * This structure encapsulates detailed information about the system's hardware
 * components, including CPU characteristics, memory configuration, and GPU
 * capabilities. It serves as a centralized data container for hardware
 * detection results and is used throughout the framework to make informed
 * decisions about rendering strategies, performance optimizations, and feature
 * availability.
 *
 * @note All frequency values are normalized to MHz for consistency
 * @note Memory values are expressed in megabytes for easier comparison with
 * thresholds
 * @see HardwareCapabilities::detect_hardware() for structure population
 */
struct LUMEX_API hardware_info_t
{
  /**
   * @brief Human-readable CPU brand name and model identifier
   *
   * Contains the complete processor brand string as reported by the system,
   * including manufacturer, model number, and additional identifiers.
   * Used for CPU generation estimation and compatibility checks.
   *
   * @example "Intel(R) Core(TM) i7-8700K CPU @ 3.70GHz"
   * @example "AMD Ryzen 5 3600 6-Core Processor"
   */
#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4251)
#endif
  std::string cpu_name;
#ifdef _WIN32
#pragma warning(pop)
#endif

  /**
   * @brief Total number of logical CPU cores available to the system
   *
   * Represents the count of logical processors including hyperthreading cores.
   * This value directly influences multithreading decisions and workload
   * distribution strategies throughout the framework.
   *
   * @note Includes both physical and logical cores (hyperthreading)
   * @range Typically 1-64 for consumer systems, higher for server hardware
   */
  uint32_t cpu_core_count;

  /**
   * @brief Base CPU clock frequency in megahertz
   *
   * The base operating frequency of the CPU in MHz. This value is used for
   * performance estimation and scheduling decisions. Note that modern CPUs
   * may boost above this frequency under optimal conditions.
   *
   * @unit MHz (Megahertz)
   * @note Does not account for turbo/boost frequencies
   * @range Typically 1000-5000 MHz for modern processors
   */
  uint32_t cpu_frequency_mhz;

  /**
   * @brief Total system memory capacity in megabytes
   *
   * The complete amount of physical RAM installed in the system, expressed
   * in megabytes. This value is critical for memory allocation strategies,
   * caching decisions, and determining appropriate buffer sizes.
   *
   * @unit MB (Megabytes)
   * @note Represents physical RAM, not virtual memory
   * @see Constants::KMIN_MEMORY_MB for minimum requirements
   */
  uint64_t total_memory_mb;

  /**
   * @brief Indicates presence of dedicated graphics hardware
   *
   * Boolean flag determining whether the system has a discrete GPU available
   * for hardware-accelerated rendering. When false, the system relies on
   * integrated graphics or software rendering fallbacks.
   *
   * @retval true System has discrete/dedicated GPU available
   * @retval false System uses integrated graphics or software rendering
   * @see gpu_name for specific GPU identification
   */
  bool has_discrete_gpu;

  /**
   * @brief Descriptive name of the primary graphics adapter
   *
   * Contains the brand name and model of the graphics hardware currently
   * being used by the system. Used for driver compatibility checks and
   * rendering capability assessment.
   *
   * @example "NVIDIA GeForce RTX 3080"
   * @example "AMD Radeon RX 6800 XT"
   * @example "Intel UHD Graphics 630"
   * @note May indicate integrated graphics even when discrete GPU is present
   */
#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4251)
#endif
  std::string gpu_name;
#ifdef _WIN32
#pragma warning(pop)
#endif

  /**
   * @brief Estimated CPU generation year for capability assessment
   *
   * Approximate year when the CPU architecture was first released, used to
   * determine feature support, instruction set availability, and performance
   * characteristics. This estimation helps make informed decisions about
   * optimization strategies and compatibility requirements.
   *
   * @unit Year (e.g., 2019, 2020, 2021)
   * @note Estimation based on model name parsing and known architecture dates
   * @see Constants CPU generation constants for reference values
   * @see estimate_cpu_generation() for calculation methodology
   */
  uint32_t cpu_generation;
};

/**
 * @brief Module identifier for logging and debugging purposes
 *
 * Static constant string used to identify this module in log outputs,
 * error messages, and debugging information. Helps track the source
 * of hardware detection operations in complex applications.
 */
LUMEX_CONST_STR KMODULE_NAME = "HardwareCapabilities";

/**
 * @brief Hardware detection and capability assessment utility class
 *
 * This static utility class provides comprehensive hardware detection
 * capabilities and intelligent rendering strategy recommendations based on
 * system specifications. It analyzes CPU, memory, and GPU characteristics to
 * determine optimal performance settings and rendering approaches for the
 * current hardware configuration.
 *
 * The class implements cross-platform hardware detection using native APIs and
 * system calls, providing consistent results across Windows, Linux, and macOS
 * platforms. All detection methods are thread-safe and can be called from any
 * context without external synchronization.
 *
 * @note All methods are static and thread-safe
 * @note Detection results are computed fresh on each call (no caching)
 * @warning Some detection methods may require elevated privileges on certain
 * platforms
 *
 * @see hardware_info_t for detailed hardware specification structure
 * @see Constants namespace for threshold values and detection parameters
 *
 * @example
 * @code
 * // Detect current hardware configuration
 * auto hwInfo = HardwareCapabilities::detect_hardware();
 *
 * // Check if software rendering is recommended
 * if (HardwareCapabilities::should_use_software_rendering()) {
 *     // Use software rendering pipeline
 * }
 *
 * // Apply optimal settings based on detected hardware
 * HardwareCapabilities::apply_optimal_rendering_settings();
 * @endcode
 */
class LUMEX_API HardwareCapabilities
{
public:
  /**
   * @brief Performs comprehensive system hardware detection and analysis
   *
   * Executes a complete hardware detection routine that gathers detailed
   * information about the system's CPU, memory, and graphics capabilities.
   * This method uses platform-specific APIs and system calls to obtain
   * accurate hardware specifications and populates a hardware_info_t structure
   * with the results.
   *
   * The detection process includes:
   * - CPU identification via CPUID instructions (x86/x64)
   * - Memory capacity detection through system APIs
   * - Graphics adapter enumeration and capability assessment
   * - CPU generation estimation based on model identification
   * - Multi-core configuration analysis
   *
   * @return hardware_info_t Populated structure containing comprehensive
   * hardware details
   *
   * @note Detection is performed fresh on each call - no caching is
   * implemented
   * @note Some detection features may require administrative privileges
   * @note Cross-platform compatibility ensured through conditional compilation
   *
   * @warning On some systems, GPU detection may fail gracefully and report
   * integrated graphics
   * @warning CPU frequency detection accuracy varies by platform and power
   * management settings
   *
   * @see hardware_info_t for detailed structure documentation
   * @see Constants namespace for detection thresholds and parameters
   *
   * @example
   * @code
   * auto hardware = HardwareCapabilities::detect_hardware();
   * std::cout << "CPU: " << hardware.cpu_name << std::endl;
   * std::cout << "Cores: " << hardware.cpu_core_count << std::endl;
   * std::cout << "Memory: " << hardware.total_memory_mb << " MB" << std::endl;
   * @endcode
   */
  static hardware_info_t detect_hardware ();

  /**
   * @brief Determines whether software rendering should be used instead of
   * hardware acceleration
   *
   * Analyzes the current system's hardware capabilities and makes an
   * intelligent recommendation about whether to use software rendering instead
   * of hardware-accelerated graphics. This decision is based on multiple
   * factors including CPU generation, memory availability, GPU capabilities,
   * and known compatibility issues.
   *
   * The recommendation algorithm considers:
   * - CPU age and performance characteristics
   * - Available system memory for software rendering buffers
   * - GPU driver quality and compatibility
   * - Known problematic hardware configurations
   * - Performance benchmarks for similar systems
   *
   * @return bool Rendering strategy recommendation
   * @retval true Software rendering is recommended for optimal
   * performance/stability
   * @retval false Hardware rendering is recommended and should perform
   * adequately
   *
   * @note Decision is based on current hardware detection results
   * @note Conservative approach - may recommend software rendering for
   * borderline cases
   * @note Does not account for application-specific rendering requirements
   *
   * @see detect_hardware() for underlying hardware analysis
   * @see Constants threshold values for decision criteria
   * @see apply_optimal_rendering_settings() for automatic configuration
   *
   * @example
   * @code
   * if (HardwareCapabilities::should_use_software_rendering()) {
   *     renderer.setSoftwareMode(true);
   *     logger.info("Using software rendering for compatibility");
   * } else {
   *     renderer.enableHardwareAcceleration();
   *     logger.info("Hardware acceleration enabled");
   * }
   * @endcode
   */
  static bool should_use_software_rendering ();

  /**
   * @brief Automatically configures optimal rendering settings based on
   * detected hardware
   *
   * Performs comprehensive hardware analysis and automatically applies the
   * most appropriate rendering configuration for the current system. This
   * method combines hardware detection with intelligent decision-making to
   * optimize performance, stability, and visual quality based on the available
   * system resources.
   *
   * The optimization process includes:
   * - Rendering pipeline selection (hardware vs software)
   * - Memory buffer size configuration
   * - Multi-threading optimization based on CPU cores
   * - Graphics quality preset selection
   * - Fallback mechanism configuration for problematic drivers
   *
   * Settings are applied globally to the rendering subsystem and persist until
   * the next call to this method or manual override by application code.
   *
   * @note Changes are applied immediately to the global rendering context
   * @note Previous settings are overwritten without backup
   * @note Thread-safe and can be called from any context
   *
   * @warning May cause brief rendering interruption during settings
   * application
   * @warning Some settings changes may require graphics context recreation
   *
   * @see should_use_software_rendering() for rendering strategy determination
   * @see detect_hardware() for underlying hardware analysis
   *
   * @example
   * @code
   * // Apply optimal settings at application startup
   * HardwareCapabilities::apply_optimal_rendering_settings();
   *
   * // Settings are now configured automatically based on hardware
   * // No further configuration needed unless manual override required
   * @endcode
   */
  static void apply_optimal_rendering_settings ();

  /**
   * @brief Estimates CPU generation year based on processor model name
   * analysis
   *
   * Analyzes the CPU brand string to determine the approximate
   * generation/release year of the processor architecture. This estimation is
   * performed by parsing known model identifiers, architecture names, and
   * generation markers from both Intel and AMD processor naming schemes.
   *
   * The estimation algorithm recognizes:
   * - Intel Core series generations (i3/i5/i7 model numbers)
   * - AMD Ryzen series generations (1000/2000/3000/5000/7000 series)
   * - Legacy processor families (Pentium, Athlon, etc.)
   * - Architecture codenames where available
   *
   * @param cpu_name CPU brand string to analyze for generation markers
   * @return uint32_t Estimated generation year (e.g., 2019, 2020, 2021)
   *
   * @note Returns default year for unrecognized processors
   * @note Estimation accuracy depends on standard manufacturer naming
   * conventions
   * @note Used internally by detect_hardware() for capability assessment
   *
   * @see Constants CPU generation year constants for reference values
   * @see is_old_cpu() for complementary age assessment
   */
  static uint32_t estimate_cpu_generation (std::string const &cpu_name);

  /**
   * @brief Determines if CPU belongs to legacy/outdated processor family
   *
   * Analyzes the CPU name to identify processors that are considered legacy
   * or outdated for modern computing requirements. This assessment is used
   * to make conservative rendering decisions and apply appropriate
   * compatibility measures for older hardware.
   *
   * Legacy processor identification includes:
   * - Intel Pentium and Celeron series
   * - Intel Atom processors (low-power variants)
   * - AMD Athlon 64 and Sempron series
   * - AMD Phenom series processors
   * - Other discontinued or low-performance processor lines
   *
   * @param cpu_name CPU brand string to evaluate for legacy status
   * @return bool Legacy processor assessment result
   * @retval true CPU is identified as legacy/outdated hardware
   * @retval false CPU is modern enough for standard operation
   *
   * @note Used in conjunction with generation estimation for comprehensive
   * assessment
   * @note Conservative approach - may flag some capable processors as legacy
   * @note Primarily used for rendering strategy decisions
   *
   * @see estimate_cpu_generation() for complementary age assessment
   * @see Constants old CPU family identifiers for recognition patterns
   */
  static bool is_old_cpu (std::string const &cpu_name);
};

/**
 * @brief Get the MAC address of the first active network interface.
 * @details Cross-platform function to get the MAC address of the main network
 * interface. On Windows, it uses GetAdaptersInfo, on Unix systems -
 * getifaddrs.
 * @return String with the MAC address in the format "XX:XX:XX:XX:XX:XX" or
 * empty string on error.
 * @note The function returns the first found active interface.
 *       If there are no network interfaces, it returns an empty string.
 * @threadsafe The function is thread-safe, but may be slow due to system
 * calls.
 */
LUMEX_PUBLIC_API
std::string get_mac_address ();

/**
 * @brief Generates a cryptographically strong seed for unique identification.
 * @details Uses hardware RNGs where available: CryptGenRandom on Windows,
 *          getentropy() or /dev/urandom on POSIX, std::random_device as
 *          fallback.
 * @return 64-bit random value suitable as a seed.
 * @note Intended for unique identifiers; call once per session.
 * @threadsafe The function is thread-safe.
 */
LUMEX_PUBLIC_API
std::uint64_t generate_cryptographic_seed ();
} // namespace caps
} // namespace hardware
} // namespace applied
} // namespace lumex

using lumex::applied::hardware::caps::hardware_info_t;
using lumex::applied::hardware::caps::HardwareCapabilities;

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_APPLIED_HARDWARE_CAPS_HARDWARE_CAPABILITIES_HPP
