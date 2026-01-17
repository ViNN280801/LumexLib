#define LUMEX_IMPLEMENTATION
#ifndef NOMINMAX
  #define NOMINMAX
#endif
#include <array>
#include <sstream>
#include <vector>

#include "LumexCPUVectorizationCapabilities.hpp"
#include "lumex/applied/logging/LumexLogging"
#include "lumex/core/utility/LumexCheckOS.hpp"

#if LUMEX_OS_WINDOWS
  #include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
  #include <cpuid.h>
#endif

#if defined(__linux__) || defined(__unix__)
  #include <fstream>
  #include <sstream>
#endif

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Applied
  {
    namespace Hardware
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
          constexpr uint32_t KFEATURE_INFO         = 0x00000001; ///< Standard feature information
          constexpr uint32_t KEXTENDED_FEATURE     = 0x00000007; ///< Extended feature information
          constexpr uint32_t KEXTENDED_FEATURE_ECX = 0x80000001; ///< Extended feature information (ECX)
        } // namespace CPUIDFunctions

        /**
         * @brief SIMD width constants
         */
        namespace SIMDWidth
        {
          constexpr uint32_t KSSE_WIDTH_BITS     = 128;  ///< SSE register width in bits
          constexpr uint32_t KAVX_WIDTH_BITS     = 256;  ///< AVX register width in bits
          constexpr uint32_t KAVX512_WIDTH_BITS  = 512;  ///< AVX-512 register width in bits
          constexpr uint32_t KNEON_WIDTH_BITS    = 128;  ///< NEON register width in bits
          constexpr uint32_t KSVE_MAX_WIDTH_BITS = 2048; ///< SVE maximum register width in bits
        } // namespace SIMDWidth

        /**
         * @brief CPUID feature flags (ECX register, function 0x00000001)
         */
        namespace CPUIDECXFlags
        {
          constexpr uint32_t KSSE3   = (1U << 0);  ///< SSE3 support
          constexpr uint32_t KPCLMUL = (1U << 1);  ///< PCLMULQDQ support
          constexpr uint32_t KSSSE3  = (1U << 9);  ///< SSSE3 support
          constexpr uint32_t KFMA    = (1U << 12); ///< FMA3 support
          constexpr uint32_t KCX16   = (1U << 13); ///< CMPXCHG16B support
          constexpr uint32_t KSSE41  = (1U << 19); ///< SSE4.1 support
          constexpr uint32_t KSSE42  = (1U << 20); ///< SSE4.2 support
          constexpr uint32_t KAVX    = (1U << 28); ///< AVX support
          constexpr uint32_t KF16C   = (1U << 29); ///< F16C support
        } // namespace CPUIDECXFlags

        /**
         * @brief CPUID feature flags (EDX register, function 0x00000001)
         */
        namespace CPUIDEDXFlags
        {
          constexpr uint32_t KSSE  = (1U << 25); ///< SSE support
          constexpr uint32_t KSSE2 = (1U << 26); ///< SSE2 support
        } // namespace CPUIDEDXFlags

        /**
         * @brief CPUID extended feature flags (EBX register, function 0x00000007, subfunction 0)
         */
        namespace CPUIDEBXFlags
        {
          constexpr uint32_t KAVX2     = (1U << 5);  ///< AVX2 support
          constexpr uint32_t KAVX512F  = (1U << 16); ///< AVX-512 Foundation support
          constexpr uint32_t KAVX512DQ = (1U << 17); ///< AVX-512 DQ support
          constexpr uint32_t KAVX512BW = (1U << 30); ///< AVX-512 BW support
          constexpr uint32_t KAVX512VL = (1U << 31); ///< AVX-512 VL support
        } // namespace CPUIDEBXFlags

        /**
         * @brief CPUID extended feature flags (ECX register, function 0x00000007, subfunction 0)
         */
        namespace CPUIDECX7Flags
        {
          constexpr uint32_t KAVX512CD = (1U << 28); ///< AVX-512 CD support
        } // namespace CPUIDECX7Flags

        /**
         * @brief CPUID extended feature flags (EDX register, function 0x80000001)
         */
        namespace CPUIDEDXExtFlags
        {
          constexpr uint32_t KFMA4 = (1U << 16); ///< FMA4 support (AMD-specific)
        } // namespace CPUIDEDXExtFlags

        /**
         * @brief ARM feature flags for HWCAP (from getauxval)
         */
        namespace ARMHWCAP
        {
#if defined(__linux__) && defined(__aarch64__)
          constexpr unsigned long KNEON = (1UL << 1); ///< NEON support (ARMv8)
#elif defined(__linux__) && (defined(__arm__) || defined(__ARM_ARCH))
          constexpr unsigned long KNEON = (1UL << 12); ///< NEON support (ARMv7)
#endif
        } // namespace ARMHWCAP

        /**
         * @brief ARM feature flags for HWCAP2 (from getauxval)
         */
        namespace ARMHWCAP2
        {
#if defined(__linux__) && defined(__aarch64__)
          constexpr unsigned long KSVE = (1UL << 0); ///< SVE support
#endif
        } // namespace ARMHWCAP2
      } // namespace

      LUMEX_PUBLIC_API
      void
      CPUVectorizationDetector::_execute_cpuid(uint32_t function, std::array<uint32_t, 4> &regs)
      {
#if LUMEX_OS_WINDOWS
        int cpuInfo[4] = {0}; // NOLINT(cppcoreguidelines-avoid-c-arrays, modernize-avoid-c-arrays)
        __cpuid(cpuInfo, static_cast<int>(function)); // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        regs[0] = static_cast<uint32_t>(cpuInfo[0]);
        regs[1] = static_cast<uint32_t>(cpuInfo[1]);
        regs[2] = static_cast<uint32_t>(cpuInfo[2]);
        regs[3] = static_cast<uint32_t>(cpuInfo[3]);
#elif defined(__GNUC__) || defined(__clang__)
        __cpuid_count(function, 0, regs[0], regs[1], regs[2], regs[3]);
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
      CPUVectorizationDetector::_check_os_xsave_xrestore_support()
      {
#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
        std::array<uint32_t, 4> regs = {0};
        _execute_cpuid(CPUIDFunctions::KFEATURE_INFO, regs);

        // Check for XSAVE support (bit 26 of ECX)
        constexpr uint32_t KXSAVE = (1U << 26);
        if((regs[2] & KXSAVE) == 0) return false;

        // Check for OSXSAVE support (bit 27 of ECX)
        constexpr uint32_t KOSXSAVE = (1U << 27);
        return (regs[2] & KOSXSAVE) != 0;
#else
        // Not x86/x64 architecture
        return false;
#endif
      }

      LUMEX_PUBLIC_API
      void
      CPUVectorizationDetector::_detect_x86_x64(cpu_vectorization_info_t &capabilities)
      {
        // Initialize all capabilities to false
        capabilities.m_supportsSSE      = false;
        capabilities.m_supportsSSE2     = false;
        capabilities.m_supportsSSE3     = false;
        capabilities.m_supportsSSSE3    = false;
        capabilities.m_supportsSSE41    = false;
        capabilities.m_supportsSSE42    = false;
        capabilities.m_supportsAVX      = false;
        capabilities.m_supportsAVX2     = false;
        capabilities.m_supportsAVX512F  = false;
        capabilities.m_supportsAVX512CD = false;
        capabilities.m_supportsAVX512BW = false;
        capabilities.m_supportsAVX512DQ = false;
        capabilities.m_supportsAVX512VL = false;
        capabilities.m_supportsFMA3     = false;
        capabilities.m_supportsFMA4     = false;
        capabilities.m_supportsNEON     = false;
        capabilities.m_supportsSVE      = false;
        capabilities.m_maxSIMDWidthBits = 0;

        // Check if CPUID is available (basic check)
        std::array<uint32_t, 4> regs = {0};
        _execute_cpuid(0, regs);
        if(regs[0] == 0 && regs[1] == 0 && regs[2] == 0 && regs[3] == 0) return;

        // Get standard feature information (function 0x00000001)
        _execute_cpuid(CPUIDFunctions::KFEATURE_INFO, regs);
        uint32_t eax = regs[0];
        uint32_t ebx = regs[1];
        uint32_t ecx = regs[2];
        uint32_t edx = regs[3];

        // Detect SSE family
        capabilities.m_supportsSSE   = (edx & CPUIDEDXFlags::KSSE) != 0;
        capabilities.m_supportsSSE2  = (edx & CPUIDEDXFlags::KSSE2) != 0;
        capabilities.m_supportsSSE3  = (ecx & CPUIDECXFlags::KSSE3) != 0;
        capabilities.m_supportsSSSE3 = (ecx & CPUIDECXFlags::KSSSE3) != 0;
        capabilities.m_supportsSSE41 = (ecx & CPUIDECXFlags::KSSE41) != 0;
        capabilities.m_supportsSSE42 = (ecx & CPUIDECXFlags::KSSE42) != 0;

        // Detect AVX (requires OS support)
        if((ecx & CPUIDECXFlags::KAVX) != 0 && _check_os_xsave_xrestore_support())
        {
          capabilities.m_supportsAVX      = true;
          capabilities.m_maxSIMDWidthBits = SIMDWidth::KAVX_WIDTH_BITS;
        }

        // Detect FMA3
        capabilities.m_supportsFMA3 = (ecx & CPUIDECXFlags::KFMA) != 0;

        // Get extended feature information (function 0x00000007, subfunction 0)
        _execute_cpuid(CPUIDFunctions::KEXTENDED_FEATURE, regs);
        eax = regs[0];
        ebx = regs[1];
        ecx = regs[2];
        edx = regs[3];

        // Check if extended features are supported (eax >= 0)
        if(eax >= 0)
        {
          // Detect AVX2 (requires AVX support)
          if((ebx & CPUIDEBXFlags::KAVX2) != 0 && capabilities.m_supportsAVX)
          {
            capabilities.m_supportsAVX2     = true;
            capabilities.m_maxSIMDWidthBits = SIMDWidth::KAVX_WIDTH_BITS;
          }

          // Detect AVX-512 Foundation (requires AVX2 support)
          if((ebx & CPUIDEBXFlags::KAVX512F) != 0 && capabilities.m_supportsAVX2)
          {
            capabilities.m_supportsAVX512F  = true;
            capabilities.m_maxSIMDWidthBits = SIMDWidth::KAVX512_WIDTH_BITS;
          }

          // Detect AVX-512 extensions (require AVX-512 Foundation)
          if(capabilities.m_supportsAVX512F)
          {
            capabilities.m_supportsAVX512CD = (ecx & CPUIDECX7Flags::KAVX512CD) != 0;
            capabilities.m_supportsAVX512DQ = (ebx & CPUIDEBXFlags::KAVX512DQ) != 0;
            capabilities.m_supportsAVX512BW = (ebx & CPUIDEBXFlags::KAVX512BW) != 0;
            capabilities.m_supportsAVX512VL = (ebx & CPUIDEBXFlags::KAVX512VL) != 0;
          }
        }

        // Get extended feature information (function 0x80000001) for AMD-specific features
        _execute_cpuid(CPUIDFunctions::KEXTENDED_FEATURE_ECX, regs);
        eax = regs[0];
        ebx = regs[1];
        ecx = regs[2];
        edx = regs[3];

        // Detect FMA4 (AMD-specific)
        capabilities.m_supportsFMA4 = (edx & CPUIDEDXExtFlags::KFMA4) != 0;

        // Update max SIMD width if SSE is supported but AVX is not
        if(capabilities.m_maxSIMDWidthBits == 0 && capabilities.m_supportsSSE2)
          capabilities.m_maxSIMDWidthBits = SIMDWidth::KSSE_WIDTH_BITS;
      }

      LUMEX_PUBLIC_API
      void
      CPUVectorizationDetector::_detect_arm(cpu_vectorization_info_t &capabilities)
      {
        // Initialize all capabilities to false
        capabilities.m_supportsSSE      = false;
        capabilities.m_supportsSSE2     = false;
        capabilities.m_supportsSSE3     = false;
        capabilities.m_supportsSSSE3    = false;
        capabilities.m_supportsSSE41    = false;
        capabilities.m_supportsSSE42    = false;
        capabilities.m_supportsAVX      = false;
        capabilities.m_supportsAVX2     = false;
        capabilities.m_supportsAVX512F  = false;
        capabilities.m_supportsAVX512CD = false;
        capabilities.m_supportsAVX512BW = false;
        capabilities.m_supportsAVX512DQ = false;
        capabilities.m_supportsAVX512VL = false;
        capabilities.m_supportsFMA3     = false;
        capabilities.m_supportsFMA4     = false;
        capabilities.m_supportsNEON     = false;
        capabilities.m_supportsSVE      = false;
        capabilities.m_maxSIMDWidthBits = 0;

#if defined(__linux__) && (defined(__aarch64__) || defined(__arm__) || defined(__ARM_ARCH))
        // Try getauxval() first (preferred method)
  #if defined(__GLIBC__) && __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 16 && defined(AT_HWCAP)
        unsigned long hwcap  = 0;
        unsigned long hwcap2 = 0;
    // getauxval may not be available on all systems
    #if __has_include(<sys/auxv.h>)
        hwcap = getauxval(AT_HWCAP);
      #if defined(AT_HWCAP2)
        hwcap2 = getauxval(AT_HWCAP2);
      #endif
    #endif

          // Detect NEON
    #if defined(__aarch64__)
        capabilities.m_supportsNEON = (hwcap & ARMHWCAP::KNEON) != 0;
    #elif defined(__arm__) || defined(__ARM_ARCH)
        capabilities.m_supportsNEON = (hwcap & ARMHWCAP::KNEON) != 0;
    #endif

          // Detect SVE (ARMv8.2+)
    #if defined(__aarch64__)
        capabilities.m_supportsSVE = (hwcap2 & ARMHWCAP2::KSVE) != 0;
    #endif

        // Set max SIMD width
        if(capabilities.m_supportsSVE)
          capabilities.m_maxSIMDWidthBits = SIMDWidth::KSVE_MAX_WIDTH_BITS; // SVE supports up to 2048 bits
        else if(capabilities.m_supportsNEON)
          capabilities.m_maxSIMDWidthBits = SIMDWidth::KNEON_WIDTH_BITS; // NEON supports 128 bits
  #else
        // Fallback: Parse /proc/cpuinfo
        std::ifstream cpuinfo("/proc/cpuinfo");
        std::string line;
        bool neonFound = false;
        bool sveFound  = false;

        while(std::getline(cpuinfo, line))
        {
          // Check for NEON support
          if(line.find("Features") != std::string::npos)
          {
            if(line.find("neon") != std::string::npos || line.find("NEON") != std::string::npos) neonFound = true;
            if(line.find("sve") != std::string::npos || line.find("SVE") != std::string::npos) sveFound = true;
          }
        }

        capabilities.m_supportsNEON = neonFound;
        capabilities.m_supportsSVE  = sveFound;

        // Set max SIMD width
        if(capabilities.m_supportsSVE)
          capabilities.m_maxSIMDWidthBits = SIMDWidth::KSVE_MAX_WIDTH_BITS;
        else if(capabilities.m_supportsNEON)
          capabilities.m_maxSIMDWidthBits = SIMDWidth::KNEON_WIDTH_BITS;
  #endif
#else
        // Not ARM architecture or not Linux
        (void)capabilities; // Suppress unused parameter warning
#endif
      }

      LUMEX_PUBLIC_API
      cpu_vectorization_info_t
      CPUVectorizationDetector::detect()
      {
        cpu_vectorization_info_t capabilities{};

        // Detect based on architecture
#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
        _detect_x86_x64(capabilities);
#elif defined(__aarch64__) || defined(__arm__) || defined(__ARM_ARCH)
        _detect_arm(capabilities);
#else
        // Unknown architecture - leave all capabilities as false
        capabilities.m_supportsSSE      = false;
        capabilities.m_supportsSSE2     = false;
        capabilities.m_supportsSSE3     = false;
        capabilities.m_supportsSSSE3    = false;
        capabilities.m_supportsSSE41    = false;
        capabilities.m_supportsSSE42    = false;
        capabilities.m_supportsAVX      = false;
        capabilities.m_supportsAVX2     = false;
        capabilities.m_supportsAVX512F  = false;
        capabilities.m_supportsAVX512CD = false;
        capabilities.m_supportsAVX512BW = false;
        capabilities.m_supportsAVX512DQ = false;
        capabilities.m_supportsAVX512VL = false;
        capabilities.m_supportsFMA3     = false;
        capabilities.m_supportsFMA4     = false;
        capabilities.m_supportsNEON     = false;
        capabilities.m_supportsSVE      = false;
        capabilities.m_maxSIMDWidthBits = 0;
#endif

        // Generate summary string
        capabilities.m_supportedTechnologies = getSummary(capabilities);

        // Log detected capabilities
        lumInfo(KMODULE_NAME, "Detected CPU vectorization capabilities - Max SIMD width: ",
                std::to_string(capabilities.m_maxSIMDWidthBits), " bits, ", "Technologies: ",
                capabilities.m_supportedTechnologies.empty() ? "None" : capabilities.m_supportedTechnologies);

        return capabilities;
      }

      LUMEX_PUBLIC_API
      std::string
      CPUVectorizationDetector::getSummary(cpu_vectorization_info_t const &capabilities)
      {
        std::vector<std::string> technologies;

        // Add technologies in order of introduction
        if(capabilities.m_supportsSSE) technologies.emplace_back("SSE");
        if(capabilities.m_supportsSSE2) technologies.emplace_back("SSE2");
        if(capabilities.m_supportsSSE3) technologies.emplace_back("SSE3");
        if(capabilities.m_supportsSSSE3) technologies.emplace_back("SSSE3");
        if(capabilities.m_supportsSSE41) technologies.emplace_back("SSE4.1");
        if(capabilities.m_supportsSSE42) technologies.emplace_back("SSE4.2");
        if(capabilities.m_supportsAVX) technologies.emplace_back("AVX");
        if(capabilities.m_supportsAVX2) technologies.emplace_back("AVX2");
        if(capabilities.m_supportsFMA3) technologies.emplace_back("FMA3");
        if(capabilities.m_supportsFMA4) technologies.emplace_back("FMA4");
        if(capabilities.m_supportsAVX512F) technologies.emplace_back("AVX-512F");
        if(capabilities.m_supportsAVX512CD) technologies.emplace_back("AVX-512CD");
        if(capabilities.m_supportsAVX512BW) technologies.emplace_back("AVX-512BW");
        if(capabilities.m_supportsAVX512DQ) technologies.emplace_back("AVX-512DQ");
        if(capabilities.m_supportsAVX512VL) technologies.emplace_back("AVX-512VL");
        if(capabilities.m_supportsNEON) technologies.emplace_back("NEON");
        if(capabilities.m_supportsSVE) technologies.emplace_back("SVE");

        // Join technologies with comma and space
        std::ostringstream oss;
        for(size_t i = 0; i < technologies.size(); ++i)
        {
          if(i > 0) oss << ", ";
          oss << technologies[i];
        }
        return oss.str();
      }
    } // namespace Hardware
  } // namespace Applied
} // namespace Lumex
