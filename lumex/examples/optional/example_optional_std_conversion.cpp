#include <iostream>
#include <optional>

#include "lumex/core/optional/LumexOptional"

int
main ()
{
  std::cout << "=== optional and std::optional (C++17) ===\n\n";

  std::optional<int> standard = 3;
  optional<int> own = standard;  // implicit, copies the value or stays empty
  std::optional<int> back = own; // implicit
  bool const same = (own == standard) // compares like two optionals
                    && !(own != back);
  std::cout << "same=" << (same ? "yes" : "no") << '\n';

  optional<int> empty;
  std::optional<int> standard_empty = empty;
  std::cout << "empty converts to empty="
            << (!standard_empty.has_value () ? "yes" : "no") << '\n';
  return same && !standard_empty.has_value () ? 0 : 1;
}
