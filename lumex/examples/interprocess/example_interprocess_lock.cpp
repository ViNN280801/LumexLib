// interprocess_lock of lumex::interprocess: one lock file per resource, the
// payload that says who holds it, what a second owner gets, and the release.
// A second lock object of the same process counts as a second owner, so one
// process shows everything; example_interprocess_lock_hold shows it with two.
#include <iostream>
#include <string>

#include "lumex/core/filesystem/LumexFilesystem"
#include "lumex/core/interprocess/LumexInterprocess"
#include "lumex/core/utility/process/LumexProcess.hpp"

namespace lk = lumex::core::interprocess::lock;
using lumex::core::filesystem::fs::lumex_filesystem;
using lumex::core::filesystem::fs::path;

int
main ()
{
  // A program keeps its lock files in its own data directory; the example uses
  // a directory next to the working directory. try_acquire creates it.
  path const directory ("lumex_interprocess_example");
  path const file = directory / "device_a.lock";
  std::string const payload
      = "{\"pid\":"
        + std::to_string (lumex::core::utility::process::get_current_pid ())
        + "}";

  auto lock = lk::interprocess_lock::try_acquire (file, payload);
  if (!lock)
    {
      std::cout << "not taken: " << to_string (lock.error ().kind)
                << " (system code " << lock.error ().system_code << ")\n";
      return 1;
    }
  std::cout << "taken: " << file.string () << "\n";
  std::cout << "payload: "
            << lk::interprocess_lock::read_payload (file).value () << "\n";

  // A second owner is refused at once and can tell who holds the resource.
  auto const second = lk::interprocess_lock::try_acquire (file, "second");
  if (!second && second.error ().kind == lk::lock_error::busy)
    std::cout << "second owner: busy, held by "
              << lk::interprocess_lock::read_payload (file).value () << "\n";

  // release () (or the destructor) frees it; the file stays.
  lock.value ().release ();
  std::cout << "released, file still there: " << std::boolalpha
            << lumex_filesystem::is_regular_file (file) << "\n";

  auto const again = lk::interprocess_lock::try_acquire (file, "again");
  std::cout << "taken again: " << again.has_value () << "\n";

  lumex_filesystem::remove_all (directory);
  return again.has_value () ? 0 : 1;
}
