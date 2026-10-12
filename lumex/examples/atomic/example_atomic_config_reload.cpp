// Workflow: publish configuration snapshots to worker threads.
//
// A reloader thread builds a new immutable configuration and publishes it
// with a compare-exchange loop; workers read the current snapshot without a
// lock of their own and sleep in wait () until the next version is
// announced with notify_all (). The last-used snapshot is remembered in an
// atomic_weak_ptr, which does not keep a retired configuration alive.

#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "lumex/core/atomic/LumexAtomic"

using lumex::core::atomic::smart_ptr::atomic_shared_ptr;
using lumex::core::atomic::smart_ptr::atomic_weak_ptr;

namespace
{
struct Config
{
  int version;
  std::string endpoint;
};

int const final_version = 5;
int const worker_count = 3;
} // namespace

int
main ()
{
  atomic_shared_ptr<Config const> current (
      std::make_shared<Config const> (Config{ 1, "https://primary" }));
  atomic_weak_ptr<Config const> last_used;

  std::vector<int> highest_seen (worker_count, 0);
  std::vector<std::thread> workers;
  for (int w = 0; w < worker_count; ++w)
    workers.push_back (std::thread (
        [&current, &last_used, &highest_seen, w]
          {
            for (;;)
              {
                std::shared_ptr<Config const> const snapshot = current.load ();
                last_used.store (snapshot);
                highest_seen[static_cast<std::size_t> (w)] = snapshot->version;
                if (snapshot->version == final_version)
                  return;
                current.wait (snapshot); // until a newer version is announced
              }
          }));

  std::thread reloader (
      [&current]
        {
          for (int step = 0; step < final_version - 1; ++step)
            {
              std::shared_ptr<Config const> expected = current.load ();
              std::shared_ptr<Config const> desired;
              do
                desired = std::make_shared<Config const> (Config{
                    expected->version + 1, expected->version % 2 == 0
                                               ? "https://primary"
                                               : "https://secondary" });
              while (!current.compare_exchange_weak (expected, desired));
              current.notify_all ();
            }
        });

  reloader.join ();
  for (std::size_t i = 0; i < workers.size (); ++i)
    workers[i].join ();

  int published_version = 0;
  {
    std::shared_ptr<Config const> const final_config = current.load ();
    published_version = final_config->version;
    std::cout << "final version " << final_config->version << " at "
              << final_config->endpoint << '\n';
  }
  for (int w = 0; w < worker_count; ++w)
    std::cout << "worker " << w << " finished on version "
              << highest_seen[static_cast<std::size_t> (w)] << '\n';

  // Retire the configuration. The atomic held the last owner; the weak cache
  // does not keep the configuration alive.
  current.store (nullptr);
  bool const cache_expired = last_used.load ().expired ();
  std::cout << "last-used cache after retirement: "
            << (cache_expired ? "expired" : "still referenced") << '\n';
  return published_version == final_version && cache_expired ? 0 : 1;
}
