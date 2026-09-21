#include <iostream>

#include "lumex/core/time/LumexTime"

using namespace lumex::core::time::clock;
using namespace lumex::core::time::timer;

int
main ()
{
  std::cout << "=== Workflow: stamp a run start and time a dummy loop ===\n\n";

  std::cout << "run_id=" << LumexTime::get_timestamp_ms () << '\n';
  std::cout << "started=" << LumexTime::get_current_datetime () << '\n';
  long long const ms = measure_execution_time ([] () {
    volatile int acc = 0;
    for (int i = 0; i < 5000; ++i)
      acc += i;
    LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (acc);
  });
  std::cout << "prep_ms=" << ms << '\n';
  return 0;
}
