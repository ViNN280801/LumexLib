#include <iostream>
#include <vector>

#include "lumex/core/generators/LumexGenerators"

using namespace lumex::core::generators::number_generator;

int
main ()
{
  std::cout << "=== NumberGenerator ranges and sequences ===\n\n";

  std::cout << "--- 1. Default d6 ---\n";
  NumberGenerator<int> d6 (1, 6);
  std::cout << "d6=" << d6 () << " again=" << d6.get_number () << '\n';

  std::cout << "\n--- 2. On-the-fly bounds ---\n";
  std::cout << "percent=" << d6 (0, 100) << '\n';

  std::cout << "\n--- 3. Reconfigure bounds ---\n";
  d6.set_bounds (10, 20);
  std::cout << "in_10_20=" << d6 () << '\n';

  std::cout << "\n--- 4. Sequence ---\n";
  std::vector<int> const seq = d6.get_sequence (8, 1, 4);
  std::cout << "sequence=";
  for (std::size_t i = 0; i < seq.size (); ++i)
    {
      std::cout << seq[i];
      if (i + 1U < seq.size ())
        std::cout << ',';
    }
  std::cout << '\n';

  std::cout << "\n--- 5. Normal distribution around a setpoint ---\n";
  NumberGenerator<double> noise (0.0, 1.0, DistributionType::NORMAL);
  noise.set_distribution (DistributionType::NORMAL);
  std::cout << "normal=" << noise (1.0, 0.1, DistributionType::NORMAL) << '\n';

  std::cout << "\n--- 6. Empty sequence ---\n";
  std::cout << "zero_count=" << d6.get_sequence (0).size () << '\n';

  std::cout << "\n=== NumberGenerator example finished ===\n";
  return 0;
}
