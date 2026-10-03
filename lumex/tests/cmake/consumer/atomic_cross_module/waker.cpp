#include <memory>

#include "CrossModule.hpp"

void
waker_publish (atomic_shared_ptr<int> &a, int value)
{
  a.store (std::make_shared<int> (value));
  a.notify_all ();
}

void
waker_hammer (atomic_shared_ptr<int> &a, int rounds)
{
  for (int i = 0; i < rounds; ++i)
    {
      std::shared_ptr<int> expected = a.load ();
      a.compare_exchange_strong (expected, std::make_shared<int> (-i));
    }
}
