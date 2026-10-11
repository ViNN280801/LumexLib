#include <algorithm>
#include <iostream>
#include <vector>

#include "lumex/core/circular_buffer/CircularBuffer"

using namespace lumex::core::circular_buffer;

namespace
{
void
print_buffer (char const *label, circular_buffer<int> const &buf)
{
  std::cout << label << ": capacity=" << buf.capacity () << " values=";
  for (circular_buffer<int>::const_iterator it = buf.begin ();
       it != buf.end (); ++it)
    std::cout << (it == buf.begin () ? "" : ",") << *it;
  std::cout << '\n';
}
}

int
main ()
{
  std::cout << "=== Editing a ring: construction, insert, erase, capacity "
               "===\n\n";

  // A list gives a full buffer whose capacity is the size of the list.
  circular_buffer<int> ring{ 1, 2, 3, 4 };
  print_buffer ("from a list", ring);
  ring.push_back (5); // full: overwrites the oldest element
  print_buffer ("push_back 5", ring);

  // The result of an insert is the last capacity () elements of the sequence
  // "before the position, the new elements, from the position".
  ring.insert (ring.begin () + 2, 9);
  print_buffer ("insert 9 before index 2", ring);
  ring.erase (ring.begin () + 1, ring.begin () + 3);
  print_buffer ("erase [1, 3)", ring);

  ring.set_capacity (6); // keeps the content; a shrink drops the newest
  ring.resize (4, 8);    // appends copies of 8
  print_buffer ("set_capacity 6, resize 4", ring);
  ring.rset_capacity (3); // a shrink drops the oldest
  print_buffer ("rset_capacity 3", ring);

  // With a capacity, a range longer than the capacity keeps its last part.
  std::vector<int> const more = { 7, 8, 9, 10 };
  circular_buffer<int> tail (2, more.begin (), more.end ());
  print_buffer ("capacity 2 from {7,8,9,10}", tail);

  // The iterators are random access; iterator and const_iterator compare.
  circular_buffer<int>::iterator first = ring.begin ();
  circular_buffer<int>::const_iterator third = ring.cbegin () + 2;
  std::cout << "first[1]=" << first[1] << " first<third=" << (first < third)
            << " third-first=" << (third - first) << '\n';

  // linearize () makes the content one contiguous run for a C API.
  std::sort (ring.begin (), ring.end ());
  int const *const data = ring.linearize ();
  std::cout << "sorted, linearized=" << ring.is_linearized ()
            << " data[0]=" << data[0] << " data[2]=" << data[2] << '\n';
  return 0;
}
