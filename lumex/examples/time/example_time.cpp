#include <iostream>
#include <sstream>

#include "lumex/core/time/LumexTime"

using namespace lumex::core::time::clock;
using namespace lumex::core::time::timer;

int
main ()
{
  std::cout << "=== Time stamps and measurement ===\n\n";

  std::cout << "--- 1. Formatted now ---\n";
  std::cout << "datetime=" << LumexTime::get_current_datetime () << '\n';
  std::cout << "custom="
            << LumexTime::get_current_datetime ("%Y-%m-%d %H:%M:%S") << '\n';

  std::cout << "\n--- 2. Epoch stamps ---\n";
  std::cout << "ns=" << LumexTime::get_timestamp_ns () << '\n';
  std::cout << "ms=" << LumexTime::get_timestamp_ms () << '\n';
  std::cout << "s=" << LumexTime::get_timestamp_s () << '\n';

  std::cout << "\n--- 3. measure_execution_time ---\n";
  long long const ms = measure_execution_time ([] () {
    volatile int x = 0;
    for (int i = 0; i < 10000; ++i)
      x += i;
    LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (x);
  });
  std::cout << "loop_ms=" << ms << '\n';

  std::cout << "\n--- 4. measure_time without env gate ---\n";
  std::ostringstream report;
  bool const reported = measure_time (
      [] () {
        volatile int y = 1;
        LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (y);
      },
      "noop", report, /*need_to_gate_via_env=*/false);
  std::cout << "reported=" << (reported ? "yes" : "no")
            << " text_chars=" << report.str ().size () << '\n';

  std::cout << "\n--- 5. LUMEX_MEASURE_TIME (env-gated; still runs) ---\n";
  LUMEX_MEASURE_TIME (LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (0), "example no-op");

  std::cout << "\n=== Time example finished ===\n";
  return 0;
}
