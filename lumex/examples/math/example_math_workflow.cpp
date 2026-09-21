#include <iostream>
#include <vector>

#include "lumex/core/math/LumexMath"

using namespace lumex::core::math::ops;

int
main ()
{
  std::cout
      << "=== Workflow: compare a baseline and a live chromatogram ===\n\n";

  std::vector<double> const baseline = { 0.01, 0.02, 0.80, 0.03, 0.01 };
  std::vector<double> const live = { 0.02, 0.03, 0.79, 0.04, 0.02 };

  std::cout << "baseline_avg=" << avg (baseline) << " live_avg=" << avg (live)
            << '\n';
  std::cout << "peak_distance=" << distance (0.80, 0.79) << '\n';
  std::cout << "rmse=" << rmse (live, baseline) << " live_rms=" << rms (live)
            << '\n';
  return 0;
}
