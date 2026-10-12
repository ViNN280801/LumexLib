#include <algorithm>
#include <iostream>
#include <numeric>

#include "lumex/core/circular_buffer/CircularBuffer"

using namespace lumex::core::circular_buffer;

int
main ()
{
  std::cout << "=== Queue: a producer that outruns its consumer ===\n\n";

  // The producer pushes bursts of three; the consumer takes two per step. The
  // buffer holds four, so from the third step push_back drops the oldest
  // value.
  circular_buffer<int> queue (4);
  int next = 1;
  int consumed = 0;
  for (int step = 0; step < 4; ++step)
    {
      for (int i = 0; i < 3; ++i)
        queue.push_back (next++);
      for (int i = 0; i < 2 && !queue.empty (); ++i)
        {
          consumed += queue.front ();
          queue.pop_front ();
        }
      std::cout << "step " << step << ": size=" << queue.size ()
                << " front=" << queue.front () << " back=" << queue.back ()
                << '\n';
    }
  std::cout << "consumed sum=" << consumed << '\n';

  // Iterators are random access: the standard algorithms work on the logical
  // sequence, oldest first, whatever the position of the ring inside.
  int const sum = std::accumulate (queue.begin (), queue.end (), 0);
  int const largest = *std::max_element (queue.begin (), queue.end ());
  std::cout << "left: sum=" << sum << " largest=" << largest
            << " newest first:";
  for (circular_buffer<int>::const_reverse_iterator it = queue.crbegin ();
       it != queue.crend (); ++it)
    std::cout << ' ' << *it;
  std::cout << '\n';
  return 0;
}
