// Waiting for a lock that another owner holds: try_acquire never waits, so a
// program that may start while the previous run is still closing retries it
// itself, a few times and with a limit. Only `busy` is worth another try; a
// refused directory (`access_denied`) or a broken path (`io_error`) will be
// the same a moment later. One process shows everything: a second lock object
// of the same process is a second owner, so a thread plays the other program
// that holds the resource for a while and then lets it go. The retry loop
// is the whole example: the library keeps no waiting API on purpose, a wait
// belongs to the caller (how long, how often, what to say meanwhile).
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

#include "lumex/core/filesystem/LumexFilesystem"
#include "lumex/core/interprocess/LumexInterprocess"
#include "lumex/core/utility/process/LumexProcess.hpp"

namespace lk = lumex::core::interprocess::lock;
using lumex::core::filesystem::fs::lumex_filesystem;
using lumex::core::filesystem::fs::path;

namespace
{
// Takes the lock of `file`, trying again every `pause` for at most `limit`
// while another owner holds it. `attempts` counts the tries.
lk::interprocess_lock::acquire_result_t
acquire_with_retry (path const &file, std::string const &payload,
                    std::chrono::milliseconds pause,
                    std::chrono::milliseconds limit, int &attempts)
{
  std::chrono::steady_clock::time_point const end
      = std::chrono::steady_clock::now () + limit;
  for (;;)
    {
      ++attempts;
      lk::interprocess_lock::acquire_result_t taken
          = lk::interprocess_lock::try_acquire (file, payload);
      if (taken || taken.error ().kind != lk::lock_error::busy)
        return taken;
      if (std::chrono::steady_clock::now () + pause > end)
        return taken;
      std::this_thread::sleep_for (pause);
    }
}
} // namespace

int
main ()
{
  path const directory ("lumex_interprocess_example_retry");
  path const file = directory / "device_a.lock";
  std::string const payload
      = "{\"pid\":"
        + std::to_string (lumex::core::utility::process::get_current_pid ())
        + "}";

  // The other owner: holds the lock for 300 ms and releases it.
  lk::interprocess_lock::acquire_result_t other
      = lk::interprocess_lock::try_acquire (file, "{\"owner\":\"other\"}");
  if (!other)
    {
      std::cout << "cannot take the first lock: "
                << to_string (other.error ().kind) << '\n';
      return 1;
    }
  std::thread releasing (
      [&other]
        {
          std::this_thread::sleep_for (std::chrono::milliseconds (300));
          other.value ().release ();
        });

  std::cout << "--- 1. Retry while the other owner holds it\n";
  int attempts = 0;
  lk::interprocess_lock::acquire_result_t mine
      = acquire_with_retry (file, payload, std::chrono::milliseconds (50),
                            std::chrono::seconds (5), attempts);
  releasing.join ();
  std::cout << "  taken after " << attempts << " attempts: " << std::boolalpha
            << mine.has_value () << "\n  the file now says: "
            << lk::interprocess_lock::read_payload (file).value () << '\n';
  bool ok = mine.has_value () && attempts > 1;

  std::cout << "\n--- 2. A limit: give up and say who holds it\n";
  int more = 0;
  lk::interprocess_lock::acquire_result_t const late
      = acquire_with_retry (file, "second", std::chrono::milliseconds (20),
                            std::chrono::milliseconds (100), more);
  if (!late && late.error ().kind == lk::lock_error::busy)
    std::cout << "  still busy after " << more << " attempts, held by "
              << lk::interprocess_lock::read_payload (file).value () << '\n';
  ok = ok && !late && late.error ().kind == lk::lock_error::busy;

  std::cout << "\n--- 3. No retry for what a wait does not change\n";
  // A directory in the place of the file is io_error (on both systems).
  path const folder = directory / "folder.lock";
  lumex_filesystem::create_directories (folder);
  int tries = 0;
  lk::interprocess_lock::acquire_result_t const broken
      = acquire_with_retry (folder, "x", std::chrono::milliseconds (20),
                            std::chrono::seconds (5), tries);
  std::cout << "  " << to_string (broken.error ().kind) << " after " << tries
            << " attempt\n";
  ok = ok && tries == 1;

  mine.value ().release ();
  lumex_filesystem::remove_all (directory);
  return ok ? 0 : 1;
}
