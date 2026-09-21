#define LUMEX_IMPLEMENTATION
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <array>
#include <sstream>
#include <vector>

#include "LumexCPUVectorizationCapabilities.hpp"
#include "lumex/applied/logging/LumexLogging"
#include "lumex/core/utility/macros/LumexConstantMacros.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/os/LumexCheckOS.hpp"

#if LUMEX_OS_WINDOWS
#include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
#include <cpuid.h>
#endif

#if defined(__linux__) || defined(__unix__)
#include <fstream>
#include <sstream>
#endif

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace applied
{
namespace hardware
{
namespace caps
{
namespace
{
/**
 * @brief Module identifier for logging and debugging purposes
 */
LUMEX_CONST_STR KMODULE_NAME = "CPUVectorizationDetector";

/**
 * @brief CPUID function numbers for feature detection
 */
namespace CPUIDFunctions
{
LUMEX_CONSTEXPR uint32_t KFEATURE_INFO
    = 0x00000001; ///< Standard feature information
LUMEX_CONSTEXPR uint32_t KEXTENDED_FEATURE
    = 0x00000007; ///< Extended feature information
LUMEX_CONSTEXPR uint32_t KEXTENDED_FEATURE_ECX
    = 0x80000001; ///< Extended feature information (ECX)
} // namespace CPUIDFunctions

/**
 * @brief SIMD width constants
 */
namespace SIMDWidth
{
LUMEX_CONSTEXPR uint32_t KSSE_WIDTH_BITS = 128; ///< SSE register width in bits
LUMEX_CONSTEXPR uint32_t KAVX_WIDTH_BITS = 256; ///< AVX register width in bits
LUMEX_CONSTEXPR uint32_t KAVX512_WIDTH_BITS
    = 512; ///< AVX-512 register width in bits
LUMEX_CONSTEXPR uint32_t KNEON_WIDTH_BITS
    = 128; ///< NEON register width in bits
LUMEX_CONSTEXPR uint32_t KSVE_MAX_WIDTH_BITS
    = 2048; ///< SVE maximum register width in bits
} // namespace SIMDWidth

/**
 * @brief CPUID feature flags (ECX register, function 0x00000001)
 */
namespace CPUIDECXFlags
{
LUMEX_CONSTEXPR uint32_t KSSE3 = (1U << 0);   ///< SSE3 support
LUMEX_CONSTEXPR uint32_t KPCLMUL = (1U << 1); ///< PCLMULQDQ support
LUMEX_CONSTEXPR uint32_t KSSSE3 = (1U << 9);  ///< SSSE3 support
LUMEX_CONSTEXPR uint32_t KFMA = (1U << 12);   ///< FMA3 support
LUMEX_CONSTEXPR uint32_t KCX16 = (1U << 13);  ///< CMPXCHG16B support
LUMEX_CONSTEXPR uint32_t KSSE41 = (1U << 19); ///< SSE4.1 support
LUMEX_CONSTEXPR uint32_t KSSE42 = (1U << 20); ///< SSE4.2 support
LUMEX_CONSTEXPR uint32_t KAVX = (1U << 28);   ///< AVX support
LUMEX_CONSTEXPR uint32_t KF16C = (1U << 29);  ///< F16C support
} // namespace CPUIDECXFlags

/**
 * @brief CPUID feature flags (EDX register, function 0x00000001)
 */
namespace CPUIDEDXFlags
{
LUMEX_CONSTEXPR uint32_t KSSE = (1U << 25);  ///< SSE support
LUMEX_CONSTEXPR uint32_t KSSE2 = (1U << 26); ///< SSE2 support
} // namespace CPUIDEDXFlags

/**
 * @brief CPUID extended feature flags (EBX register, function 0x00000007,
 * subfunction 0)
 */
namespace CPUIDEBXFlags
{
LUMEX_CONSTEXPR uint32_t KAVX2 = (1U << 5);     ///< AVX2 support
LUMEX_CONSTEXPR uint32_t KAVX512F = (1U << 16); ///< AVX-512 Foundation support
LUMEX_CONSTEXPR uint32_t KAVX512DQ = (1U << 17); ///< AVX-512 DQ support
LUMEX_CONSTEXPR uint32_t KAVX512BW = (1U << 30); ///< AVX-512 BW support
LUMEX_CONSTEXPR uint32_t KAVX512VL = (1U << 31); ///< AVX-512 VL support
} // namespace CPUIDEBXFlags

/**
 * @brief CPUID extended feature flags (ECX register, function 0x00000007,
 * subfunction 0)
 */
namespace CPUIDECX7Flags
{
LUMEX_CONSTEXPR uint32_t KAVX512CD = (1U << 28); ///< AVX-512 CD support
} // namespace CPUIDECX7Flags

/**
 * @brief CPUID extended feature flags (EDX register, function 0x80000001)
 */
namespace CPUIDEDXExtFlags
{
LUMEX_CONSTEXPR uint32_t KFMA4 = (1U << 16); ///< FMA4 support (AMD-specific)
} // namespace CPUIDEDXExtFlags

/**
 * @brief ARM feature flags for HWCAP (from getauxval)
 */
namespace ARMHWCAP
{
#if defined(__linux__) && defined(__aarch64__)
LUMEX_CONSTEXPR unsigned long KNEON = (1UL << 1); ///< NEON support (ARMv8)
#elif defined(__linux__) && (defined(__arm__) || defined(__ARM_ARCH))
LUMEX_CONSTEXPR unsigned long KNEON = (1UL << 12); ///< NEON support (ARMv7)
#endif
} // namespace ARMHWCAP

/**
 * @brief ARM feature flags for HWCAP2 (from getauxval)
 */
namespace ARMHWCAP2
{
#if defined(__linux__) && defined(__aarch64__)
LUMEX_CONSTEXPR unsigned long KSVE = (1UL << 0); ///< SVE support
#endif
} // namespace ARMHWCAP2
} // namespace

LUMEX_PUBLIC_API
void
CPUVectorizationDetector::_execute_cpuid (uint32_t function,
                                          std::array<uint32_t, 4> &regs)
{
#if LUMEX_OS_WINDOWS
  int cpuInfo[4] = {
    0
  }; // NOLINT(cppcoreguidelines-avoid-c-arrays, modernize-avoid-c-arrays)
  __cpuid (
      cpuInfo,
      static_cast<int> (
          function)); // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
  regs[0] = static_cast<uint32_t> (cpuInfo[0]);
  regs[1] = static_cast<uint32_t> (cpuInfo[1]);
  regs[2] = static_cast<uint32_t> (cpuInfo[2]);
  regs[3] = static_cast<uint32_t> (cpuInfo[3]);
#elif defined(__GNUC__) || defined(__clang__)
  __cpuid_count (function, 0, regs[0], regs[1], regs[2], regs[3]);
#else
  // Fallback: zero out registers if CPUID is not available
  regs[0] = 0;
  regs[1] = 0;
  regs[2] = 0;
  regs[3] = 0;
#endif
}

LUMEX_PUBLIC_API
bool
CPUVectorizationDetector::_check_os_xsave_xrestore_support ()
{
#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86)                \
    || defined(__i386__)
  std::array<uint32_t, 4> regs = { 0 };
  _execute_cpuid (CPUIDFunctions::KFEATURE_INFO, regs);

  // Check for XSAVE support (bit 26 of ECX)
  LUMEX_CONSTEXPR uint32_t KXSAVE = (1U << 26);
  if ((regs[2] & KXSAVE) == 0)
    return false;

  // Check for OSXSAVE support (bit 27 of ECX)
  LUMEX_CONSTEXPR uint32_t KOSXSAVE = (1U << 27);
  return (regs[2] & KOSXSAVE) != 0;
#else
  // Not x86/x64 architecture
  return false;
#endif
}

LUMEX_PUBLIC_API
void
CPUVectorizationDetector::_detect_x86_x64 (
    cpu_vectorization_info_t &capabilities)
{
  // Initialize all capabilities to false
  capabilities.supports_sse = false;
  capabilities.supports_sse2 = false;
  capabilities.supports_sse3 = false;
  capabilities.supports_ssse3 = false;
  capabilities.supports_sse41 = false;
  capabilities.supports_sse42 = false;
  capabilities.supports_avx = false;
  capabilities.supports_avx2 = false;
  capabilities.supports_avx512f = false;
  capabilities.supports_avx512cd = false;
  capabilities.supports_avx512bw = false;
  capabilities.supports_avx512dq = false;
  capabilities.supports_avx512vl = false;
  capabilities.supports_fma3 = false;
  capabilities.supports_fma4 = false;
  capabilities.supports_neon = false;
  capabilities.supports_sve = false;
  capabilities.max_simd_width_bits = 0;

  // Check if CPUID is available (basic check)
  std::array<uint32_t, 4> regs = { 0 };
  _execute_cpuid (0, regs);
  if (regs[0] == 0 && regs[1] == 0 && regs[2] == 0 && regs[3] == 0)
    return;

  // Get standard feature information (function 0x00000001)
  _execute_cpuid (CPUIDFunctions::KFEATURE_INFO, regs);
  uint32_t eax = regs[0];
  uint32_t ebx = regs[1];
  uint32_t ecx = regs[2];
  uint32_t edx = regs[3];

  // Detect SSE family
  capabilities.supports_sse = (edx & CPUIDEDXFlags::KSSE) != 0;
  capabilities.supports_sse2 = (edx & CPUIDEDXFlags::KSSE2) != 0;
  capabilities.supports_sse3 = (ecx & CPUIDECXFlags::KSSE3) != 0;
  capabilities.supports_ssse3 = (ecx & CPUIDECXFlags::KSSSE3) != 0;
  capabilities.supports_sse41 = (ecx & CPUIDECXFlags::KSSE41) != 0;
  capabilities.supports_sse42 = (ecx & CPUIDECXFlags::KSSE42) != 0;

  // Detect AVX (requires OS support)
  if ((ecx & CPUIDECXFlags::KAVX) != 0 && _check_os_xsave_xrestore_support ())
    {
      capabilities.supports_avx = true;
      capabilities.max_simd_width_bits = SIMDWidth::KAVX_WIDTH_BITS;
    }

  // Detect FMA3
  capabilities.supports_fma3 = (ecx & CPUIDECXFlags::KFMA) != 0;

  // Get extended feature information (function 0x00000007, subfunction 0)
  _execute_cpuid (CPUIDFunctions::KEXTENDED_FEATURE, regs);
  eax = regs[0];
  ebx = regs[1];
  ecx = regs[2];
  edx = regs[3];

  // Check if extended features are supported (eax >= 0)
  if (eax >= 0)
    {
      // Detect AVX2 (requires AVX support)
      if ((ebx & CPUIDEBXFlags::KAVX2) != 0 && capabilities.supports_avx)
        {
          capabilities.supports_avx2 = true;
          capabilities.max_simd_width_bits = SIMDWidth::KAVX_WIDTH_BITS;
        }

      // Detect AVX-512 Foundation (requires AVX2 support)
      if ((ebx & CPUIDEBXFlags::KAVX512F) != 0 && capabilities.supports_avx2)
        {
          capabilities.supports_avx512f = true;
          capabilities.max_simd_width_bits = SIMDWidth::KAVX512_WIDTH_BITS;
        }

      // Detect AVX-512 extensions (require AVX-512 Foundation)
      if (capabilities.supports_avx512f)
        {
          capabilities.supports_avx512cd
              = (ecx & CPUIDECX7Flags::KAVX512CD) != 0;
          capabilities.supports_avx512dq
              = (ebx & CPUIDEBXFlags::KAVX512DQ) != 0;
          capabilities.supports_avx512bw
              = (ebx & CPUIDEBXFlags::KAVX512BW) != 0;
          capabilities.supports_avx512vl
              = (ebx & CPUIDEBXFlags::KAVX512VL) != 0;
        }
    }

  // Get extended feature information (function 0x80000001) for AMD-specific
  // features
  _execute_cpuid (CPUIDFunctions::KEXTENDED_FEATURE_ECX, regs);
  eax = regs[0];
  ebx = regs[1];
  ecx = regs[2];
  edx = regs[3];

  // Detect FMA4 (AMD-specific)
  capabilities.supports_fma4 = (edx & CPUIDEDXExtFlags::KFMA4) != 0;

  // Update max SIMD width if SSE is supported but AVX is not
  if (capabilities.max_simd_width_bits == 0 && capabilities.supports_sse2)
    capabilities.max_simd_width_bits = SIMDWidth::KSSE_WIDTH_BITS;
}

LUMEX_PUBLIC_API
void
CPUVectorizationDetector::_detect_arm (cpu_vectorization_info_t &capabilities)
{
  // Initialize all capabilities to false
  capabilities.supports_sse = false;
  capabilities.supports_sse2 = false;
  capabilities.supports_sse3 = false;
  capabilities.supports_ssse3 = false;
  capabilities.supports_sse41 = false;
  capabilities.supports_sse42 = false;
  capabilities.supports_avx = false;
  capabilities.supports_avx2 = false;
  capabilities.supports_avx512f = false;
  capabilities.supports_avx512cd = false;
  capabilities.supports_avx512bw = false;
  capabilities.supports_avx512dq = false;
  capabilities.supports_avx512vl = false;
  capabilities.supports_fma3 = false;
  capabilities.supports_fma4 = false;
  capabilities.supports_neon = false;
  capabilities.supports_sve = false;
  capabilities.max_simd_width_bits = 0;

#if defined(__linux__)                                                        \
    && (defined(__aarch64__) || defined(__arm__) || defined(__ARM_ARCH))
  // Try getauxval() first (preferred method)
#if defined(__GLIBC__) && __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 16             \
    && defined(AT_HWCAP)
  unsigned long hwcap = 0;
  unsigned long hwcap2 = 0;
// getauxval may not be available on all systems
#if __has_include(<sys/auxv.h>)
  hwcap = getauxval (AT_HWCAP);
#if defined(AT_HWCAP2)
  hwcap2 = getauxval (AT_HWCAP2);
#endif
#endif

  // Detect NEON
#if defined(__aarch64__)
  capabilities.supports_neon = (hwcap & ARMHWCAP::KNEON) != 0;
#elif defined(__arm__) || defined(__ARM_ARCH)
  capabilities.supports_neon = (hwcap & ARMHWCAP::KNEON) != 0;
#endif

  // Detect SVE (ARMv8.2+)
#if defined(__aarch64__)
  capabilities.supports_sve = (hwcap2 & ARMHWCAP2::KSVE) != 0;
#endif

  // Set max SIMD width
  if (capabilities.supports_sve)
    capabilities.max_simd_width_bits
        = SIMDWidth::KSVE_MAX_WIDTH_BITS; // SVE supports up to 2048 bits
  else if (capabilities.supports_neon)
    capabilities.max_simd_width_bits
        = SIMDWidth::KNEON_WIDTH_BITS; // NEON supports 128 bits
#else
  // Fallback: Parse /proc/cpuinfo
  std::ifstream cpuinfo ("/proc/cpuinfo");
  std::string line;
  bool neonFound = false;
  bool sveFound = false;

  while (std::getline (cpuinfo, line))
    {
      // Check for NEON support
      if (line.find ("Features") != std::string::npos)
        {
          if (line.find ("neon") != std::string::npos
              || line.find ("NEON") != std::string::npos)
            neonFound = true;
          if (line.find ("sve") != std::string::npos
              || line.find ("SVE") != std::string::npos)
            sveFound = true;
        }
    }

  capabilities.supports_neon = neonFound;
  capabilities.supports_sve = sveFound;

  // Set max SIMD width
  if (capabilities.supports_sve)
    capabilities.max_simd_width_bits = SIMDWidth::KSVE_MAX_WIDTH_BITS;
  else if (capabilities.supports_neon)
    capabilities.max_simd_width_bits = SIMDWidth::KNEON_WIDTH_BITS;
#endif
#else
  // Not ARM architecture or not Linux
  (void)capabilities; // Suppress unused parameter warning
#endif
}

LUMEX_PUBLIC_API
cpu_vectorization_info_t
CPUVectorizationDetector::detect ()
{
  cpu_vectorization_info_t capabilities{};

  // Detect based on architecture
#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86)                \
    || defined(__i386__)
  _detect_x86_x64 (capabilities);
#elif defined(__aarch64__) || defined(__arm__) || defined(__ARM_ARCH)
  _detect_arm (capabilities);
#else
  // Unknown architecture - leave all capabilities as false
  capabilities.supports_sse = false;
  capabilities.supports_sse2 = false;
  capabilities.supports_sse3 = false;
  capabilities.supports_ssse3 = false;
  capabilities.supports_sse41 = false;
  capabilities.supports_sse42 = false;
  capabilities.supports_avx = false;
  capabilities.supports_avx2 = false;
  capabilities.supports_avx512f = false;
  capabilities.supports_avx512cd = false;
  capabilities.supports_avx512bw = false;
  capabilities.supports_avx512dq = false;
  capabilities.supports_avx512vl = false;
  capabilities.supports_fma3 = false;
  capabilities.supports_fma4 = false;
  capabilities.supports_neon = false;
  capabilities.supports_sve = false;
  capabilities.max_simd_width_bits = 0;
#endif

  // Generate summary string
  capabilities.supported_technologies = get_summary (capabilities);

  // Log detected capabilities
  lumInfo (KMODULE_NAME,
           "Detected CPU vectorization capabilities - Max SIMD width: ",
           std::to_string (capabilities.max_simd_width_bits), " bits, ",
           "Technologies: ",
           capabilities.supported_technologies.empty ()
               ? "None"
               : capabilities.supported_technologies);

  return capabilities;
}

LUMEX_PUBLIC_API
std::string
CPUVectorizationDetector::get_summary (
    cpu_vectorization_info_t const &capabilities)
{
  std::vector<std::string> technologies;

  // Add technologies in order of introduction
  if (capabilities.supports_sse)
    technologies.emplace_back ("SSE");
  if (capabilities.supports_sse2)
    technologies.emplace_back ("SSE2");
  if (capabilities.supports_sse3)
    technologies.emplace_back ("SSE3");
  if (capabilities.supports_ssse3)
    technologies.emplace_back ("SSSE3");
  if (capabilities.supports_sse41)
    technologies.emplace_back ("SSE4.1");
  if (capabilities.supports_sse42)
    technologies.emplace_back ("SSE4.2");
  if (capabilities.supports_avx)
    technologies.emplace_back ("AVX");
  if (capabilities.supports_avx2)
    technologies.emplace_back ("AVX2");
  if (capabilities.supports_fma3)
    technologies.emplace_back ("FMA3");
  if (capabilities.supports_fma4)
    technologies.emplace_back ("FMA4");
  if (capabilities.supports_avx512f)
    technologies.emplace_back ("AVX-512F");
  if (capabilities.supports_avx512cd)
    technologies.emplace_back ("AVX-512CD");
  if (capabilities.supports_avx512bw)
    technologies.emplace_back ("AVX-512BW");
  if (capabilities.supports_avx512dq)
    technologies.emplace_back ("AVX-512DQ");
  if (capabilities.supports_avx512vl)
    technologies.emplace_back ("AVX-512VL");
  if (capabilities.supports_neon)
    technologies.emplace_back ("NEON");
  if (capabilities.supports_sve)
    technologies.emplace_back ("SVE");

  // Join technologies with comma and space
  std::ostringstream oss;
  for (std::size_t i = 0; i < technologies.size (); ++i)
    {
      if (i > 0)
        oss << ", ";
      oss << technologies[i];
    }
  return oss.str ();
}
} // namespace caps
} // namespace hardware
} // namespace applied
} // namespace lumex
