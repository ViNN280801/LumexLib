// cmake.consumer_atomic_cross_module: before C++20 the lock-based atomic
// smart pointers sleep on a table that lives in a function-local static of an
// inline function. Two shared libraries built with hidden visibility must
// still share that table: a thread that sleeps through one library is woken
// by a notification issued through the other, and lock contention between
// the two libraries never leaves a thread asleep. Exit code 0 on success,
// 3 when a thread is still asleep after the deadline.

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <thread>

#include "CrossModule.hpp"

namespace
{
bool
finished_in_time (std::atomic<bool> const &done, int seconds)
{
  std::chrono::steady_clock::time_point const deadline
      = std::chrono::steady_clock::now () + std::chrono::seconds (seconds);
  while (!done.load () && std::chrono::steady_clock::now () < deadline)
    std::this_thread::sleep_for (std::chrono::milliseconds (5));
  return done.load ();
}
} // namespace

int
main ()
{
  std::printf ("atomic smart pointers: lock-free=%d, std wait=%d\n",
               LUMEX_ATOMIC_SMART_PTR_COMMON_IS_LOCK_FREE,
               LUMEX_HAS_STD_ATOMIC_WAIT);
  atomic_shared_ptr<int> a (std::make_shared<int> (0));

  // 1. Sleep through the sleeper library, wake through the waker library.
  for (int round = 0; round < 20; ++round)
    {
      std::atomic<bool> started (false);
      std::atomic<bool> done (false);
      std::shared_ptr<int> const old = a.load ();
      std::thread sleeper (
          [&a, &started, &done, old]
            {
              started.store (true);
              sleeper_wait (a, old);
              done.store (true);
            });
      while (!started.load ())
        std::this_thread::yield ();
      std::this_thread::sleep_for (std::chrono::milliseconds (20));
      waker_publish (a, round + 1);
      if (!finished_in_time (done, 10))
        {
          std::printf ("round %d: the sleeper was not woken across modules\n",
                       round);
          std::fflush (stdout);
          std::_Exit (3);
        }
      sleeper.join ();
    }

  // 2. Lock contention between the two libraries.
  std::atomic<bool> done (false);
  std::atomic<int> finished (0);
  std::thread contenders[4] = { std::thread (
                                    [&]
                                      {
                                        sleeper_hammer (a, 20000);
                                        finished.fetch_add (1);
                                      }),
                                std::thread (
                                    [&]
                                      {
                                        waker_hammer (a, 20000);
                                        finished.fetch_add (1);
                                      }),
                                std::thread (
                                    [&]
                                      {
                                        sleeper_hammer (a, 20000);
                                        finished.fetch_add (1);
                                      }),
                                std::thread (
                                    [&]
                                      {
                                        waker_hammer (a, 20000);
                                        finished.fetch_add (1);
                                      }) };
  std::thread monitor (
      [&]
        {
          while (finished.load () < 4)
            std::this_thread::sleep_for (std::chrono::milliseconds (5));
          done.store (true);
        });
  if (!finished_in_time (done, 60))
    {
      std::printf ("a contender of the lock was not woken across modules\n");
      std::fflush (stdout);
      std::_Exit (3);
    }
  monitor.join ();
  for (int i = 0; i < 4; ++i)
    contenders[i].join ();
  std::printf ("cross-module wait, notify and lock: OK\n");
  return 0;
}
