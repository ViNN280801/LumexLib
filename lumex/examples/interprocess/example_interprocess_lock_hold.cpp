// Two processes and one resource: run
//   LumexInterprocessExampleLockHold 30
// in one console and the same command in another while the first waits. The
// second process says that the resource is busy and which process holds it.
// Close the first console with the window button or end it in the task
// manager: the system releases the lock of a process that ends in any way,
// and the next run takes it (on Windows after a moment).
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include "lumex/core/interprocess/LumexInterprocess"
#include "lumex/core/utility/process/LumexProcess.hpp"

namespace lk = lumex::core::interprocess::lock;
using lumex::core::filesystem::fs::path;

int
main (int argc, char **argv)
{
  // How long to hold the lock; 0 (the default) takes and releases at once.
  long const seconds = argc > 1 ? std::strtol (argv[1], nullptr, 10) : 0;
  path const file = path ("lumex_interprocess_example_hold") / "device_a.lock";
  std::string const payload
      = "{\"pid\":"
        + std::to_string (lumex::core::utility::process::get_current_pid ())
        + "}";

  auto lock = lk::interprocess_lock::try_acquire (file, payload);
  if (!lock)
    {
      if (lock.error ().kind == lk::lock_error::busy)
        {
          auto const owner = lk::interprocess_lock::read_payload (file);
          std::cout << "busy, held by "
                    << (owner ? owner.value () : std::string ("?")) << "\n";
          return 2;
        }
      std::cout << "cannot use " << file.string () << ": "
                << to_string (lock.error ().kind) << " (system code "
                << lock.error ().system_code << ")\n";
      return 1;
    }

  std::cout << "holding " << file.string () << " as " << payload << " for "
            << seconds << " s\n";
  std::this_thread::sleep_for (std::chrono::seconds (seconds));
  return 0;
}
