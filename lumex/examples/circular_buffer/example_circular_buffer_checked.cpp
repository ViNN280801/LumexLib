#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#include "lumex/core/circular_buffer/CircularBuffer"

using namespace lumex::core::circular_buffer;

int
main ()
{
  std::cout << "=== Checked access, capacity and moved-from buffers ===\n\n";

  // A zero capacity is allowed: the buffer is valid, empty and full, and
  // push_back does nothing.
  circular_buffer<int> none (0);
  none.push_back (1);
  std::cout << "capacity 0: size=" << none.size ()
            << " full=" << (none.full () ? "yes" : "no") << '\n';

  circular_buffer<std::string> log (2);
  // front(), back() and operator[] are unchecked; at(), front_safe() and
  // back_safe() throw std::out_of_range, pop_front()/pop_back() do nothing
  // on an empty buffer.
  try
    {
      log.front_safe ();
    }
  catch (std::out_of_range const &)
    {
      std::cout << "front_safe on empty: out_of_range\n";
    }
  log.pop_front ();
  log.emplace_back ("start");
  log.emplace_back (3U, 'x'); // constructed in place: "xxx"
  std::cout << "at(1)=" << log.at (1) << '\n';
  try
    {
      log.at (2);
    }
  catch (std::out_of_range const &)
    {
      std::cout << "at(2): out_of_range\n";
    }

  // A moved-from buffer has capacity 0 and is valid (push does nothing):
  // assign to it to use it again.
  circular_buffer<std::string> moved (std::move (log));
  std::cout << "moved-from capacity=" << log.capacity ()
            << " size=" << log.size () << '\n';
  log = circular_buffer<std::string> (4);
  log.push_back ("again");
  std::cout << "reused capacity=" << log.capacity ()
            << " front=" << log.front () << '\n';
  return 0;
}
