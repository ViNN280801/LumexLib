#ifndef LUMEX_CPU_VECTORIZATION_CAPABILITIES_HPP
#define LUMEX_CPU_VECTORIZATION_CAPABILITIES_HPP

#include "lumex/LumexExport.hpp"

#include <array>
#include <cstdint>
#include <string>

#if defined(_WIN32) || defined(WIN32)
  #include <windows.h>
#elif defined(__linux__) || defined(__unix__)
  #include <sys/auxv.h>
  #include <unistd.h>
#endif

#define LUMEX_CPU_VECTORIZATION_INFO_ALIGNMENT 64

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Applied
  {
    namespace Hardware
    {
      /**
       * @brief Comprehensive CPU vectorization capabilities structure
       *
       * This structure encapsulates detailed information about CPU vectorization instruction sets
       * and SIMD capabilities. It provides a unified interface for querying supported vectorization
       * technologies across different CPU architectures (x86/x64, ARM, etc.).
       *
       * The structure is designed to be architecture-agnostic, providing consistent access patterns
       * regardless of the underlying CPU architecture. Detection is performed using platform-specific
       * mechanisms (CPUID for x86/x64, getauxval/proc for ARM).
       *
       * @note All boolean flags indicate runtime support, not compile-time availability
       * @note Detection results are computed fresh on each call (no caching)
       * @note Thread-safe - can be queried from any thread context
       *
       * @see CPUVectorizationDetector::detect() for structure population
       * @see CPUVectorizationDetector class for detection methods
       *
       * @example
       * @code
       * auto caps = CPUVectorizationDetector::detect();
       * if (caps.m_supportsAVX2) {
       *     // Use AVX2 optimized code path
       * } else if (caps.m_supportsAVX) {
       *     // Use AVX optimized code path
       * } else if (caps.m_supportsSSE42) {
       *     // Use SSE4.2 optimized code path
       * }
       * @endcode
       */
      struct alignas(LUMEX_CPU_VECTORIZATION_INFO_ALIGNMENT) LUMEX_API
        cpu_vectorization_info_t { // NOLINT(cppcoreguidelines-pro-type-member-init,
                                   // misc-non-private-member-variables-in-classes, readability-identifier-naming)
        /**
         * @brief SSE (Streaming SIMD Extensions) support
         *
         * Indicates support for the original SSE instruction set (SSE1), introduced with
         * Intel Pentium III and AMD Athlon XP processors. Provides 128-bit SIMD operations
         * for floating-point and integer data types.
         *
         * @retval true CPU supports SSE instructions
         * @retval false CPU does not support SSE instructions
         * @note Present on virtually all x86/x64 processors since ~1999
         */
        bool m_supportsSSE;

        /**
         * @brief SSE2 (Streaming SIMD Extensions 2) support
         *
         * Indicates support for SSE2 instruction set, introduced with Intel Pentium 4
         * and AMD Athlon 64 processors. Extends SSE with double-precision floating-point
         * and additional integer operations.
         *
         * @retval true CPU supports SSE2 instructions
         * @retval false CPU does not support SSE2 instructions
         * @note Present on all x64 processors and most x86 processors since ~2001
         */
        bool m_supportsSSE2;

        /**
         * @brief SSE3 (Streaming SIMD Extensions 3) support
         *
         * Indicates support for SSE3 instruction set, introduced with Intel Pentium 4
         * Prescott and AMD Athlon 64 processors. Adds horizontal operations and additional
         * floating-point instructions.
         *
         * @retval true CPU supports SSE3 instructions
         * @retval false CPU does not support SSE3 instructions
         * @note Present on most processors since ~2004
         */
        bool m_supportsSSE3;

        /**
         * @brief SSSE3 (Supplemental SSE3) support
         *
         * Indicates support for SSSE3 instruction set, introduced with Intel Core 2
         * and AMD processors. Adds shuffle and arithmetic instructions for improved
         * performance in multimedia and cryptographic applications.
         *
         * @retval true CPU supports SSSE3 instructions
         * @retval false CPU does not support SSSE3 instructions
         * @note Present on most processors since ~2006
         */
        bool m_supportsSSSE3;

        /**
         * @brief SSE4.1 support
         *
         * Indicates support for SSE4.1 instruction set, introduced with Intel Core 2
         * Penryn processors. Adds dot product, rounding, and blending operations for
         * improved multimedia and scientific computing performance.
         *
         * @retval true CPU supports SSE4.1 instructions
         * @retval false CPU does not support SSE4.1 instructions
         * @note Present on Intel processors since ~2007, AMD processors since ~2011
         */
        bool m_supportsSSE41;

        /**
         * @brief SSE4.2 support
         *
         * Indicates support for SSE4.2 instruction set, introduced with Intel Core i7
         * Nehalem processors. Adds string processing, CRC32, and POPCNT instructions
         * for improved text processing and data manipulation performance.
         *
         * @retval true CPU supports SSE4.2 instructions
         * @retval false CPU does not support SSE4.2 instructions
         * @note Present on Intel processors since ~2008, AMD processors since ~2011
         */
        bool m_supportsSSE42;

        /**
         * @brief AVX (Advanced Vector Extensions) support
         *
         * Indicates support for AVX instruction set, introduced with Intel Sandy Bridge
         * and AMD Bulldozer processors. Provides 256-bit SIMD operations for floating-point
         * data types, doubling the width compared to SSE.
         *
         * @retval true CPU supports AVX instructions
         * @retval false CPU does not support AVX instructions
         * @note Present on processors since ~2011
         * @note Requires OS support for XSAVE/XRSTOR instructions
         */
        bool m_supportsAVX;

        /**
         * @brief AVX2 support
         *
         * Indicates support for AVX2 instruction set, introduced with Intel Haswell
         * and AMD Excavator processors. Extends AVX with 256-bit integer operations,
         * gather operations, and FMA (Fused Multiply-Add) support.
         *
         * @retval true CPU supports AVX2 instructions
         * @retval false CPU does not support AVX2 instructions
         * @note Present on processors since ~2013
         * @note Requires AVX support as prerequisite
         */
        bool m_supportsAVX2;

        /**
         * @brief AVX-512 Foundation support
         *
         * Indicates support for AVX-512 Foundation instruction set, introduced with
         * Intel Xeon Phi and Skylake-X processors. Provides 512-bit SIMD operations
         * with mask registers and enhanced floating-point capabilities.
         *
         * @retval true CPU supports AVX-512 Foundation instructions
         * @retval false CPU does not support AVX-512 Foundation instructions
         * @note Present on high-end processors since ~2016
         * @note Requires AVX2 support as prerequisite
         * @note May be disabled by microcode updates on some processors
         */
        bool m_supportsAVX512F;

        /**
         * @brief AVX-512 CD (Conflict Detection) support
         *
         * Indicates support for AVX-512 Conflict Detection instructions, which provide
         * efficient conflict detection in parallel algorithms and hash table operations.
         *
         * @retval true CPU supports AVX-512 CD instructions
         * @retval false CPU does not support AVX-512 CD instructions
         * @note Requires AVX-512 Foundation support
         */
        bool m_supportsAVX512CD;

        /**
         * @brief AVX-512 BW (Byte and Word) support
         *
         * Indicates support for AVX-512 Byte and Word instructions, which provide
         * enhanced 8-bit and 16-bit integer operations for improved performance in
         * multimedia and data processing applications.
         *
         * @retval true CPU supports AVX-512 BW instructions
         * @retval false CPU does not support AVX-512 BW instructions
         * @note Requires AVX-512 Foundation support
         */
        bool m_supportsAVX512BW;

        /**
         * @brief AVX-512 DQ (Doubleword and Quadword) support
         *
         * Indicates support for AVX-512 Doubleword and Quadword instructions, which
         * provide enhanced 32-bit and 64-bit integer operations.
         *
         * @retval true CPU supports AVX-512 DQ instructions
         * @retval false CPU does not support AVX-512 DQ instructions
         * @note Requires AVX-512 Foundation support
         */
        bool m_supportsAVX512DQ;

        /**
         * @brief AVX-512 VL (Vector Length) support
         *
         * Indicates support for AVX-512 Vector Length extensions, which allow AVX-512
         * instructions to operate on 128-bit and 256-bit registers in addition to
         * 512-bit registers.
         *
         * @retval true CPU supports AVX-512 VL instructions
         * @retval false CPU does not support AVX-512 VL instructions
         * @note Requires AVX-512 Foundation support
         */
        bool m_supportsAVX512VL;

        /**
         * @brief FMA3 (Fused Multiply-Add 3-operand) support
         *
         * Indicates support for FMA3 instruction set, which provides single-round
         * floating-point multiply-add operations for improved accuracy and performance
         * in scientific computing and signal processing.
         *
         * @retval true CPU supports FMA3 instructions
         * @retval false CPU does not support FMA3 instructions
         * @note Present on Intel Haswell and AMD Bulldozer processors and later
         * @note Often bundled with AVX2 support
         */
        bool m_supportsFMA3;

        /**
         * @brief FMA4 (Fused Multiply-Add 4-operand) support
         *
         * Indicates support for FMA4 instruction set, an AMD-specific variant of FMA
         * with 4-operand instructions. Provides similar functionality to FMA3 but with
         * different instruction encoding.
         *
         * @retval true CPU supports FMA4 instructions
         * @retval false CPU does not support FMA4 instructions
         * @note AMD-specific, present on AMD Bulldozer and Piledriver processors
         * @note Not supported on Intel processors
         */
        bool m_supportsFMA4;

        /**
         * @brief NEON support (ARM Advanced SIMD)
         *
         * Indicates support for ARM NEON instruction set, which provides 64-bit and
         * 128-bit SIMD operations for ARM processors. Equivalent to SSE/AVX on x86/x64
         * architectures.
         *
         * @retval true CPU supports NEON instructions
         * @retval false CPU does not support NEON instructions
         * @note Present on most ARMv7-A and ARMv8-A processors
         * @note x86/x64 processors always return false
         */
        bool m_supportsNEON;

        /**
         * @brief SVE (Scalable Vector Extension) support
         *
         * Indicates support for ARM SVE instruction set, which provides scalable
         * vector operations with variable-length vectors (128-2048 bits). Introduced
         * with ARMv8.2-A architecture.
         *
         * @retval true CPU supports SVE instructions
         * @retval false CPU does not support SVE instructions
         * @note Present on ARMv8.2-A and later processors (e.g., Fujitsu A64FX, ARM Neoverse)
         * @note x86/x64 processors always return false
         */
        bool m_supportsSVE;

        /**
         * @brief Maximum SIMD register width in bits
         *
         * Indicates the maximum SIMD register width supported by the CPU. This value
         * helps determine the optimal vectorization strategy and buffer alignment
         * requirements.
         *
         * @unit Bits (e.g., 128, 256, 512)
         * @note For x86/x64: 128 (SSE), 256 (AVX), or 512 (AVX-512)
         * @note For ARM: 128 (NEON) or variable (SVE)
         * @note Used for memory alignment and buffer size calculations
         */
        uint32_t m_maxSIMDWidthBits;

        /**
         * @brief Human-readable summary of supported vectorization technologies
         *
         * Contains a comma-separated list of supported vectorization instruction sets,
         * formatted for display in logs, diagnostics, or user interfaces. Provides
         * a quick overview of CPU capabilities.
         *
         * @example "SSE, SSE2, SSE3, SSSE3, SSE4.1, SSE4.2, AVX, AVX2, FMA3"
         * @example "NEON, SVE"
         * @note Automatically generated during detection
         * @note Empty string if no vectorization support detected
         */
#ifdef _WIN32
  #pragma warning(push)
  #pragma warning(disable : 4251)
#endif
        std::string m_supportedTechnologies;
#ifdef _WIN32
  #pragma warning(pop)
#endif
      };

      /**
       * @brief CPU vectorization capabilities detection utility class
       *
       * This static utility class provides comprehensive CPU vectorization detection
       * capabilities across different architectures (x86/x64, ARM, etc.). It uses
       * platform-specific mechanisms to detect supported SIMD instruction sets and
       * provides a unified interface for querying vectorization capabilities.
       *
       * The class implements cross-platform detection using:
       * - CPUID instruction for x86/x64 architectures (Windows, Linux, macOS)
       * - getauxval() and /proc/cpuinfo for ARM architectures (Linux)
       * - Architecture-specific feature detection mechanisms
       *
       * All detection methods are thread-safe and can be called from any context
       * without external synchronization. Detection results are computed fresh on
       * each call (no caching is implemented).
       *
       * @note All methods are static and thread-safe
       * @note Detection is performed fresh on each call (no caching)
       * @note Cross-platform compatibility ensured through conditional compilation
       * @warning Detection accuracy depends on OS support for feature reporting
       *
       * @see cpu_vectorization_info_t for detailed capability structure
       *
       * @example
       * @code
       * // Detect CPU vectorization capabilities
       * auto caps = CPUVectorizationDetector::detect();
       *
       * // Check for specific instruction set support
       * if (caps.m_supportsAVX2) {
       *     // Use AVX2 optimized code path
       *     useAVX2OptimizedFunction();
       * } else if (caps.m_supportsAVX) {
       *     // Use AVX optimized code path
       *     useAVXOptimizedFunction();
       * } else {
       *     // Fall back to scalar code
       *     useScalarFunction();
       * }
       *
       * // Get summary of supported technologies
       * std::cout << "Supported: " << caps.m_supportedTechnologies << std::endl;
       * @endcode
       */
      class LUMEX_API CPUVectorizationDetector
      {
      public:
        /**
         * @brief Performs comprehensive CPU vectorization capabilities detection
         *
         * Executes a complete vectorization detection routine that identifies all
         * supported SIMD instruction sets on the current CPU. This method uses
         * architecture-specific detection mechanisms (CPUID for x86/x64, getauxval
         * for ARM) to determine runtime support for various vectorization technologies.
         *
         * The detection process includes:
         * - x86/x64: CPUID-based detection for SSE, AVX, AVX2, AVX-512 variants
         * - ARM: getauxval() and /proc/cpuinfo parsing for NEON, SVE support
         * - Cross-platform: Maximum SIMD width calculation
         * - Summary generation: Human-readable technology list
         *
         * @return cpu_vectorization_info_t Populated structure containing all
         *         detected vectorization capabilities
         *
         * @note Detection is performed fresh on each call - no caching is implemented
         * @note Some features may require OS support (e.g., AVX requires XSAVE support)
         * @note Cross-platform compatibility ensured through conditional compilation
         *
         * @warning On some systems, certain features may be disabled by microcode
         * @warning Detection accuracy depends on OS and CPU feature reporting mechanisms
         *
         * @see cpu_vectorization_info_t for detailed structure documentation
         *
         * @example
         * @code
         * auto caps = CPUVectorizationDetector::detect();
         * std::cout << "AVX2: " << (caps.m_supportsAVX2 ? "Yes" : "No") << std::endl;
         * std::cout << "Max SIMD width: " << caps.m_maxSIMDWidthBits << " bits" << std::endl;
         * std::cout << "Technologies: " << caps.m_supportedTechnologies << std::endl;
         * @endcode
         */
        static cpu_vectorization_info_t detect();

        /**
         * @brief Gets a human-readable summary of supported vectorization technologies
         *
         * Generates a comma-separated string listing all supported vectorization
         * instruction sets. This method provides a convenient way to display CPU
         * capabilities in logs, diagnostics, or user interfaces.
         *
         * @param capabilities Vectorization capabilities structure to summarize
         * @return std::string Comma-separated list of supported technologies
         *
         * @note Empty string is returned if no vectorization support is detected
         * @note Technologies are listed in order of introduction (SSE -> AVX -> AVX-512)
         *
         * @example
         * @code
         * auto caps = CPUVectorizationDetector::detect();
         * std::string summary = CPUVectorizationDetector::getSummary(caps);
         * // Output: "SSE, SSE2, SSE3, SSSE3, SSE4.1, SSE4.2, AVX, AVX2, FMA3"
         * @endcode
         */
        static std::string getSummary(cpu_vectorization_info_t const &capabilities);

      private:
        /**
         * @brief Detects x86/x64 vectorization capabilities using CPUID
         *
         * Internal method that uses CPUID instruction to detect supported SIMD
         * instruction sets on x86/x64 architectures. This method is called
         * automatically by detect() when running on x86/x64 processors.
         *
         * @param[out] capabilities Structure to populate with detection results
         *
         * @note Only called on x86/x64 architectures
         * @note Requires CPUID instruction support (present on all x86/x64 CPUs)
         * @note OS support for XSAVE/XRSTOR may be required for AVX detection
         */
        static void _detect_x86_x64(cpu_vectorization_info_t &capabilities);

        /**
         * @brief Detects ARM vectorization capabilities using system calls
         *
         * Internal method that uses getauxval() and /proc/cpuinfo parsing to detect
         * supported SIMD instruction sets on ARM architectures. This method is called
         * automatically by detect() when running on ARM processors.
         *
         * @param[out] capabilities Structure to populate with detection results
         *
         * @note Only called on ARM architectures
         * @note Falls back to /proc/cpuinfo parsing if getauxval() is unavailable
         * @note May require reading /proc/cpuinfo on older Linux systems
         */
        static void _detect_arm(cpu_vectorization_info_t &capabilities);

        /**
         * @brief Executes CPUID instruction and returns results
         *
         * Platform-specific wrapper for CPUID instruction execution. Handles
         * differences between Windows (__cpuid) and Unix (inline assembly)
         * implementations.
         *
         * @param[in] function CPUID function number to execute
         * @param[out] regs Array of 4 integers to store CPUID results (EAX, EBX, ECX, EDX)
         *
         * @note Only available on x86/x64 architectures
         * @note Thread-safe - CPUID is a serializing instruction
         */
        static void
        _execute_cpuid(uint32_t function, std::array<uint32_t, 4> &regs); // NOLINT(modernize-avoid-c-arrays)

        /**
         * @brief Checks OS support for XSAVE/XRSTOR instructions
         *
         * Verifies that the operating system supports saving and restoring extended
         * CPU state, which is required for AVX and later instruction sets. This
         * check is necessary because hardware support alone is insufficient - the
         * OS must also support the extended state management.
         *
         * @return bool OS support status
         * @retval true OS supports XSAVE/XRSTOR (AVX can be used)
         * @retval false OS does not support XSAVE/XRSTOR (AVX cannot be used)
         *
         * @note Only relevant for x86/x64 architectures
         * @note Modern operating systems (Windows 7+, Linux 2.6.30+) support this
         */
        static bool _check_os_xsave_xrestore_support();
      };
    } // namespace Hardware
  } // namespace Applied
} // namespace Lumex

#endif // !LUMEX_CPU_VECTORIZATION_CAPABILITIES_HPP
