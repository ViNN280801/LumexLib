// Stackful coroutines (asio::spawn on the vendored Boost.Context) that migrate
// between the two threads of an io_context: two hundred of them take five
// turns each through a steady_timer. Run under ThreadSanitizer this is what
// BOOST_USE_TSAN is for: without it the sanitizer crashes with a SEGV at the
// end of a coroutine stack (it does not follow swapcontext). Exit code 0 when
// every coroutine finished.

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

#include <asio.hpp>
#include <asio/spawn.hpp>

int
main ()
{
  asio::io_context io;
  asio::executor_work_guard<asio::io_context::executor_type> guard (
      io.get_executor ());
  std::vector<std::thread> threads;
  for (int i = 0; i < 2; ++i)
    threads.emplace_back ([&io] { io.run (); });

  int const coroutines = 200;
  std::atomic<int> done (0);
  for (int c = 0; c < coroutines; ++c)
    asio::spawn (
        io,
        [&io, &done] (asio::yield_context yield)
          {
            asio::steady_timer timer (io);
            for (int turn = 0; turn < 5; ++turn)
              {
                timer.expires_after (std::chrono::microseconds (50));
                timer.async_wait (yield);
              }
            ++done;
          },
        asio::detached);
  while (done.load () < coroutines)
    std::this_thread::sleep_for (std::chrono::milliseconds (5));
  guard.reset ();
  for (std::thread &thread : threads)
    thread.join ();
  std::printf ("coroutines finished: %d\n", done.load ());
  return done.load () == coroutines ? 0 : 1;
}
