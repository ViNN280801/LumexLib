// Two asio::spawn coroutines on the vendored Boost.Context take turns through
// a steady_timer; each one records its turns in a shared log.

#include <chrono>
#include <cstdio>
#include <exception>
#include <string>

#include <asio.hpp>
#include <asio/spawn.hpp>

int
main ()
{
  asio::io_context io;
  std::string log;
  int failures = 0;

  auto worker = [&] (char name)
    {
      return [&, name] (asio::yield_context yield)
        {
          try
            {
              asio::steady_timer timer (io);
              for (int turn = 0; turn < 3; ++turn)
                {
                  log += name;
                  timer.expires_after (std::chrono::milliseconds (5));
                  timer.async_wait (yield);
                }
            }
          catch (std::exception const &)
            {
              ++failures;
            }
        };
    };

  asio::spawn (io, worker ('a'), asio::detached);
  asio::spawn (io, worker ('b'), asio::detached);
  io.run ();

  std::printf ("log=%s failures=%d\n", log.c_str (), failures);
  // Three turns each, and interleaved: "aaabbb" would mean a coroutine never
  // suspended (no context switch happened).
  int a = 0;
  int b = 0;
  for (std::string::size_type i = 0; i < log.size (); ++i)
    (log[i] == 'a' ? a : b) += 1;
  bool const interleaved
      = log.size () == 6 && log != "aaabbb" && log != "bbbaaa";
  return (failures == 0 && a == 3 && b == 3 && interleaved) ? 0 : 1;
}
