#include <iostream>

#include "lumex/applied/hardware/LumexHardware"

using namespace lumex::applied::hardware::caps;

int
main ()
{
  std::cout << "=== Workflow: choose a render path from live hardware ===\n\n";

  hardware_info_t const hw = HardwareCapabilities::detect_hardware ();
  std::cout << "cpu=" << hw.cpu_name << " cores=" << hw.cpu_core_count
            << " mem_mb=" << hw.total_memory_mb << '\n';
  std::cout << "software_render="
            << (HardwareCapabilities::should_use_software_rendering () ? "yes"
                                                                       : "no")
            << " old_cpu="
            << (HardwareCapabilities::is_old_cpu (hw.cpu_name) ? "yes" : "no")
            << " generation~"
            << HardwareCapabilities::estimate_cpu_generation (hw.cpu_name)
            << '\n';

  cpu_vectorization_info_t const simd = CPUVectorizationDetector::detect ();
  std::cout << "simd=" << CPUVectorizationDetector::get_summary (simd) << '\n';
  return 0;
}
