#include <iostream>
#include <vector>

#include "lumex/core/generators/LumexGenerators"

using namespace lumex::core::generators::number_generator;

int
main ()
{
  std::cout << "=== Workflow: jitter vial positions for a plate map ===\n\n";

  NumberGenerator<int> rows (1, 8);
  NumberGenerator<int> cols (1, 12);
  std::vector<int> const row_picks = rows.get_sequence (5, 1, 8);
  std::vector<int> const col_picks = cols.get_sequence (5, 1, 12);

  for (std::size_t i = 0; i < row_picks.size () && i < col_picks.size (); ++i)
    {
      char const row = static_cast<char> ('A' + (row_picks[i] - 1));
      std::cout << "vial=" << row << col_picks[i] << '\n';
    }
  return 0;
}
