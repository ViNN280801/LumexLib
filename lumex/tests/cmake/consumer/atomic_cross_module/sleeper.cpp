#include <memory>
#include <utility>

#include "CrossModule.hpp"

void
sleeper_wait (atomic_shared_ptr<int> &a, std::shared_ptr<int> old)
{
  a.wait (std::move (old));
}

void
sleeper_hammer (atomic_shared_ptr<int> &a, int rounds)
{
  for (int i = 0; i < rounds; ++i)
    {
      a.store (std::make_shared<int> (i));
      std::shared_ptr<int> const loaded = a.load ();
      if (!loaded)
        break;
    }
}
