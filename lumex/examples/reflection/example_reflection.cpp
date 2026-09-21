#include <cstdint>
#include <iostream>
#include <string>

#include "lumex/core/reflection/LumexReflection"

using namespace lumex::core::reflection::var_info;

DEFINE_REFLECTED_ENUM (ExampleRunState, std::uint8_t, (Idle), (Injecting, 5),
                       (Running), (Done))

int
main ()
{
  std::cout << "=== Reflection: LUMEX_VARINFO and reflected enums ===\n\n";

  std::cout << "--- 1. LUMEX_VARINFO on locals ---\n";
  int channel = 2;
  double flow = 1.25;
  std::string instrument = "HPLC-01";
  std::cout << LUMEX_VARINFO (channel) << '\n';
  std::cout << LUMEX_VARINFO (flow) << '\n';
  std::cout << LUMEX_VARINFO (instrument) << '\n';

  std::cout << "\n--- 2. Reflected enum toString / values ---\n";
  std::cout << "Idle=" << toString (ExampleRunState::Idle)
            << " Injecting=" << toString (ExampleRunState::Injecting)
            << " numeric_injecting="
            << static_cast<int> (ExampleRunState::Injecting) << '\n';
  std::cout << "size=" << ExampleRunStateSize
            << " first=" << toString (ExampleRunStateFirst)
            << " last=" << toString (ExampleRunStateLast) << '\n';
  for (ExampleRunState const state : ExampleRunStateValues)
    std::cout << "  " << toString (state) << '\n';

  std::cout << "\n--- 3. Unknown enumerator ---\n";
  std::cout << "unknown=" << toString (static_cast<ExampleRunState> (99))
            << '\n';

  std::cout << "\n=== Reflection example finished ===\n";
  return 0;
}
