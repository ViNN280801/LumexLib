#include <atomic>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "lumex/core/smart_ptr/LumexSmartPtr"

namespace sp = lumex::core::smart_ptr;

namespace
{
// A cache that does not keep its values alive: it holds weak pointers, so an
// entry disappears once the last user of the value is gone.
class ResourceCache
{
public:
  sp::shared_ptr<std::string>
  get (std::string const &key)
  {
    std::map<std::string, sp::weak_ptr<std::string>>::iterator found
        = entries_.find (key);
    if (found != entries_.end ())
      {
        if (sp::shared_ptr<std::string> alive = found->second.lock ())
          {
            ++hits;
            return alive;
          }
      }
    ++loads;
    sp::shared_ptr<std::string> fresh
        = sp::make_shared<std::string> ("resource:" + key);
    entries_[key] = fresh;
    return fresh;
  }

  int hits = 0;
  int loads = 0;

private:
  std::map<std::string, sp::weak_ptr<std::string>> entries_;
};
} // namespace

int
main ()
{
  std::cout << "=== smart_ptr workflow: a weak cache, then shared use from "
               "threads ===\n\n";

  std::cout << "--- 1. The cache reuses live values only ---\n";
  ResourceCache cache;
  {
    sp::shared_ptr<std::string> first = cache.get ("alpha");
    sp::shared_ptr<std::string> second = cache.get ("alpha");
    std::cout << "same object: " << (first == second ? "yes" : "no")
              << " loads=" << cache.loads << " hits=" << cache.hits << '\n';
  } // both owners are gone: the cached value is destroyed
  sp::shared_ptr<std::string> third = cache.get ("alpha");
  std::cout << "after the owners went: loads=" << cache.loads
            << " (the value was loaded again)\n";

  std::cout << "\n--- 2. Copies of one object on several threads ---\n";
  // Distinct sp::shared_ptr objects that share ownership may be copied and
  // destroyed concurrently; the counters are atomic. Each thread gets its own
  // copy.
  sp::shared_ptr<std::atomic<int>> total
      = sp::make_shared<std::atomic<int>> (0);
  std::vector<std::thread> workers;
  for (int i = 0; i < 4; ++i)
    {
      sp::shared_ptr<std::atomic<int>> mine = total;
      workers.push_back (std::thread (
          [mine]
            {
              for (int n = 0; n < 1000; ++n)
                {
                  sp::shared_ptr<std::atomic<int>> temporary = mine;
                  temporary->fetch_add (1);
                }
            }));
    }
  for (std::size_t i = 0; i < workers.size (); ++i)
    workers[i].join ();
  workers.clear ();
  std::cout << "sum=" << total->load ()
            << " use_count after the join=" << total.use_count () << '\n';
  return 0;
}
