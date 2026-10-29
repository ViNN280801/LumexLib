// Compile checks of the transport's timer seam, built by
// cmake.timing_compile_checks. Exactly one of LUMEX_TIMING_GOOD_CASE or
// LUMEX_TIMING_BAD_CASE=<n> is defined; the fixture compiles with warnings
// as errors. Every bad case mixes the seam with std::chrono::steady_clock
// (its time points are of timing::timer_clock) or calls a device_watcher
// constructor that no longer exists (see CMakeLists.txt for the list).

#include <chrono>
#include <system_error>
#include <vector>

#include "lumex/applied/transport/session/LumexDeviceWatcher.hpp"
#include "lumex/applied/transport/testing/LumexVirtualClock.hpp"
#include "lumex/applied/transport/timing/LumexTimerSource.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"

namespace
{
namespace tr = lumex::applied::transport;
using ms = std::chrono::milliseconds;

void
on_wait (std::error_code const &)
{
}

std::vector<tr::session::serial_port_info_t>
no_ports ()
{
  return std::vector<tr::session::serial_port_info_t> ();
}
} // namespace

int
main ()
{
  asio::io_context context;
  tr::timing::timer timer = tr::timing::steady_timer_source ().make_timer (
      context.get_executor ());
#if defined(LUMEX_TIMING_GOOD_CASE)
  timer.async_wait_until (timer.now () + ms (10), on_wait);
  tr::timing::time_point_t const at = tr::timing::timer_clock::now ();
  tr::testing::virtual_clock clock (tr::timing::time_point_t () + ms (5));
  tr::session::device_watcher_options_t options;
  options.period = ms (5);
  options.enumeration = no_ports;
  tr::session::device_watcher watcher (context.get_executor (), options);
  tr::session::device_watcher defaults (context.get_executor ());
  tr::session::device_watcher braces (context.get_executor (), {});
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (at);
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (clock);
#elif LUMEX_TIMING_BAD_CASE == 1
  timer.async_wait_until (std::chrono::steady_clock::now () + ms (10),
                          on_wait);
#elif LUMEX_TIMING_BAD_CASE == 2
  tr::timing::time_point_t const at = std::chrono::steady_clock::now ();
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (at);
#elif LUMEX_TIMING_BAD_CASE == 3
  tr::testing::virtual_clock clock (std::chrono::steady_clock::now ());
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (clock);
#elif LUMEX_TIMING_BAD_CASE == 4
  bool const late = timer.now () < std::chrono::steady_clock::now ();
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (late);
#elif LUMEX_TIMING_BAD_CASE == 5
  tr::session::device_watcher watcher (context.get_executor (), ms (5));
#elif LUMEX_TIMING_BAD_CASE == 6
  tr::session::device_watcher watcher (context.get_executor (), ms (5),
                                       no_ports);
#endif
  return 0;
}
