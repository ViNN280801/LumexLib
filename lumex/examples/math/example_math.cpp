#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

#include "lumex/core/math/LumexMath"

using namespace lumex::core::math::ops;

int
main ()
{
  std::cout << "=== Math averages, distances, rms, casts ===\n\n";

  std::vector<double> const samples = { 1.0, 2.0, 3.0, 4.0 };
  std::vector<double> const baseline = { 1.1, 1.9, 3.2, 3.8 };

  std::cout << "--- 1. avg ---\n";
  std::cout << "average=" << avg (samples) << '\n';
  std::cout << "average(>2)="
            << avg (samples, [] (double v) { return v > 2.0; }) << '\n';

  std::cout << "\n--- 2. distance / squared_difference ---\n";
  std::cout << "distance(10u, 3u)=" << distance (10u, 3u) << '\n';
  std::cout << "distance(1.5, 4.0)=" << distance (1.5, 4.0) << '\n';
  std::cout << "squared_difference(5, 2)=" << squared_difference (5, 2)
            << '\n';

  std::cout << "\n--- 3. rms / rmse ---\n";
  std::cout << "rms=" << rms (samples) << '\n';
  std::cout << "rmse vs 2.5=" << rmse (samples, 2.5) << '\n';
  std::cout << "rmse vs baseline=" << rmse (samples, baseline) << '\n';

  std::cout << "\n--- 4. is_nan_inf ---\n";
  std::cout << "is_nan_inf(1.0)=" << (is_nan_inf (1.0) ? "yes" : "no") << '\n';
  std::cout << "is_nan_inf(nan)="
            << (is_nan_inf (std::numeric_limits<double>::quiet_NaN ()) ? "yes"
                                                                       : "no")
            << '\n';
  std::cout << "is_nan_inf(inf)="
            << (is_nan_inf (std::numeric_limits<double>::infinity ()) ? "yes"
                                                                      : "no")
            << '\n';

  std::cout << "\n--- 5. checked_narrow_cast ---\n";
  try
    {
      int const fits = checked_narrow_cast<double, int> (42.0, "channel");
      std::cout << "cast 42.0 -> " << fits << '\n';
      LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (
          checked_narrow_cast<double, int> (1.0e20, "overflow"));
    }
  catch (std::out_of_range const &ex)
    {
      std::cout << "narrow_cast rejected: " << ex.what () << '\n';
    }

  std::cout << "\n=== Math example finished ===\n";
  return 0;
}
