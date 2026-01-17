#include <iostream>
#include <string>

#include "lumex/applied/hardware/LumexCPUVectorizationCapabilities.hpp"

using namespace Lumex::Applied::Hardware;

namespace
{
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
}

int
main()
{
  std::cout << "=== CPU Vectorization Capabilities Detection Example ===\n\n";

  // Detect CPU vectorization capabilities
  std::cout << "Detecting CPU vectorization capabilities...\n";
  cpu_vectorization_info_t caps = CPUVectorizationDetector::detect();

  std::cout << "\n--- Detection Results ---\n\n";

  // Display maximum SIMD width
  std::cout << "Maximum SIMD Register Width: " << caps.m_maxSIMDWidthBits << " bits\n\n";

  // Display SSE family support
  std::cout << "SSE Family Support:\n";
  std::cout << "  SSE:     " << _boolToYesNo(caps.m_supportsSSE) << "\n";
  std::cout << "  SSE2:    " << _boolToYesNo(caps.m_supportsSSE2) << "\n";
  std::cout << "  SSE3:    " << _boolToYesNo(caps.m_supportsSSE3) << "\n";
  std::cout << "  SSSE3:   " << _boolToYesNo(caps.m_supportsSSSE3) << "\n";
  std::cout << "  SSE4.1:  " << _boolToYesNo(caps.m_supportsSSE41) << "\n";
  std::cout << "  SSE4.2:  " << _boolToYesNo(caps.m_supportsSSE42) << "\n\n";

  // Display AVX family support
  std::cout << "AVX Family Support:\n";
  std::cout << "  AVX:     " << _boolToYesNo(caps.m_supportsAVX) << "\n";
  std::cout << "  AVX2:    " << _boolToYesNo(caps.m_supportsAVX2) << "\n\n";

  // Display AVX-512 support
  std::cout << "AVX-512 Family Support:\n";
  std::cout << "  AVX-512 Foundation: " << _boolToYesNo(caps.m_supportsAVX512F) << "\n";
  std::cout << "  AVX-512 CD:        " << _boolToYesNo(caps.m_supportsAVX512CD) << "\n";
  std::cout << "  AVX-512 BW:        " << _boolToYesNo(caps.m_supportsAVX512BW) << "\n";
  std::cout << "  AVX-512 DQ:        " << _boolToYesNo(caps.m_supportsAVX512DQ) << "\n";
  std::cout << "  AVX-512 VL:        " << _boolToYesNo(caps.m_supportsAVX512VL) << "\n\n";

  // Display FMA support
  std::cout << "FMA Support:\n";
  std::cout << "  FMA3:   " << _boolToYesNo(caps.m_supportsFMA3) << "\n";
  std::cout << "  FMA4:   " << _boolToYesNo(caps.m_supportsFMA4) << "\n\n";

  // Display ARM support
  std::cout << "ARM SIMD Support:\n";
  std::cout << "  NEON:   " << _boolToYesNo(caps.m_supportsNEON) << "\n";
  std::cout << "  SVE:    " << _boolToYesNo(caps.m_supportsSVE) << "\n\n";

  // Display summary
  std::cout << "--- Summary ---\n";
  std::string summary = CPUVectorizationDetector::getSummary(caps);
  if(summary.empty())
    std::cout << "No vectorization technologies detected.\n";
  else
    std::cout << "Supported Technologies: " << summary << "\n";
  std::cout << "\n";

  // Example: Runtime code path selection
  std::cout << "--- Runtime Code Path Selection Example ---\n";
  std::cout << "Recommended optimization level: ";

  if(caps.m_supportsAVX512F)
  {
    std::cout << "AVX-512 (Highest performance)\n";
    std::cout << "  -> Use AVX-512 optimized functions for maximum performance\n";
  }
  else if(caps.m_supportsAVX2)
  {
    std::cout << "AVX2 (High performance)\n";
    std::cout << "  -> Use AVX2 optimized functions for high performance\n";
  }
  else if(caps.m_supportsAVX)
  {
    std::cout << "AVX (Medium performance)\n";
    std::cout << "  -> Use AVX optimized functions for medium performance\n";
  }
  else if(caps.m_supportsSSE42)
  {
    std::cout << "SSE4.2 (Good performance)\n";
    std::cout << "  -> Use SSE4.2 optimized functions for good performance\n";
  }
  else if(caps.m_supportsSSE41)
  {
    std::cout << "SSE4.1 (Basic performance)\n";
    std::cout << "  -> Use SSE4.1 optimized functions for basic performance\n";
  }
  else if(caps.m_supportsSSE2)
  {
    std::cout << "SSE2 (Minimal optimization)\n";
    std::cout << "  -> Use SSE2 optimized functions for minimal optimization\n";
  }
  else if(caps.m_supportsNEON)
  {
    std::cout << "NEON (ARM optimization)\n";
    std::cout << "  -> Use NEON optimized functions for ARM processors\n";
  }
  else
  {
    std::cout << "Scalar (No SIMD optimization)\n";
    std::cout << "  -> Use scalar (non-vectorized) code\n";
  }

  std::cout << "\n";

  // Example: Check for specific feature combinations
  std::cout << "--- Feature Combination Examples ---\n";
  if(caps.m_supportsAVX2 && caps.m_supportsFMA3)
    std::cout << "✓ AVX2 + FMA3: Can use fused multiply-add operations with 256-bit vectors\n";
  if(caps.m_supportsAVX512F && caps.m_supportsAVX512VL)
    std::cout << "✓ AVX-512F + AVX-512VL: Can use AVX-512 instructions on 128/256-bit vectors\n";
  if(caps.m_supportsSSE42 && !caps.m_supportsAVX)
    std::cout << "✓ SSE4.2 only: CPU supports SSE4.2 but not AVX (older processor)\n";

  std::cout << "\n=== Example completed successfully ===\n";

  return EXIT_SUCCESS;
}

/*
========================= EXAMPLE OUTPUT =========================
=== CPU Vectorization Capabilities Detection Example ===

Detecting CPU vectorization capabilities...
[17.01.2026_18:46:05] |    INFO| CPUVectorizationDetector : Detected CPU vectorization capabilities - Max SIMD width:
256 bits, Technologies: SSE, SSE2, SSE3, SSSE3, SSE4.1, SSE4.2, AVX, AVX2, FMA3

--- Detection Results ---

Maximum SIMD Register Width: 256 bits

SSE Family Support:
  SSE:     Yes
  SSE2:    Yes
  SSE3:    Yes
  SSSE3:   Yes
  SSE4.1:  Yes
  SSE4.2:  Yes

AVX Family Support:
  AVX:     Yes
  AVX2:    Yes

AVX-512 Family Support:
  AVX-512 Foundation: No
  AVX-512 CD:        No
  AVX-512 BW:        No
  AVX-512 DQ:        No
  AVX-512 VL:        No

FMA Support:
  FMA3:   Yes
  FMA4:   No

ARM SIMD Support:
  NEON:   No
  SVE:    No

--- Summary ---
Supported Technologies: SSE, SSE2, SSE3, SSSE3, SSE4.1, SSE4.2, AVX, AVX2, FMA3

--- Runtime Code Path Selection Example ---
Recommended optimization level: AVX2 (High performance)
  -> Use AVX2 optimized functions for high performance

--- Feature Combination Examples ---
тЬУ AVX2 + FMA3: Can use fused multiply-add operations with 256-bit vectors

=== Example completed successfully ===
==================================================================
*/
