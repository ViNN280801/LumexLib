#include <iostream>
#include <string>

#include "lumex/core/circular_buffer/CircularBuffer"

using namespace lumex::core::circular_buffer;

namespace
{
void
print_buffer (char const *label, CircularBuffer<int> const &buf)
{
  std::cout << label << " size=" << buf.size ()
            << " capacity=" << buf.capacity ()
            << " empty=" << (buf.empty () ? "yes" : "no")
            << " full=" << (buf.full () ? "yes" : "no") << " values=";
  for (CircularBuffer<int>::size_type i = 0; i < buf.size (); ++i)
    {
      std::cout << buf[i];
      if (i + 1U < buf.size ())
        std::cout << ',';
    }
  std::cout << '\n';
}
}

int
main ()
{
  std::cout << "=== CircularBuffer: ring overwrite and iteration ===\n\n";

  std::cout << "--- 1. Fixed capacity, push until full ---\n";
  CircularBuffer<int> samples (4);
  samples.push_back (10);
  samples.push_back (20);
  samples.push_back (30);
  print_buffer ("after 3 pushes", samples);
  samples.push_back (40);
  print_buffer ("full", samples);

  std::cout << "\n--- 2. Push on a full buffer overwrites the oldest ---\n";
  samples.push_back (50);
  print_buffer ("after overwrite", samples);
  std::cout << "front=" << samples.front () << " back=" << samples.back ()
            << '\n';

  std::cout << "\n--- 3. pop_front / pop_back ---\n";
  samples.pop_front ();
  print_buffer ("after pop_front", samples);
  samples.pop_back ();
  print_buffer ("after pop_back", samples);

  std::cout << "\n--- 4. push_front shifts the logical head ---\n";
  samples.push_front (1);
  print_buffer ("after push_front(1)", samples);

  std::cout << "\n--- 5. emplace_back for a non-trivial type ---\n";
  CircularBuffer<std::string> names (3);
  names.emplace_back ("alpha");
  names.emplace_back (std::string ("beta"));
  names.push_back ("gamma");
  names.push_back ("delta");
  std::cout << "names front=" << names.front () << " back=" << names.back ()
            << " size=" << names.size () << '\n';

  std::cout << "\n--- 6. Lexicographical compare and clear ---\n";
  CircularBuffer<int> a (3);
  CircularBuffer<int> b (3);
  a.push_back (1);
  a.push_back (2);
  b.push_back (1);
  b.push_back (2);
  std::cout << "a==b=" << (a == b ? "yes" : "no")
            << " a<b=" << (a < b ? "yes" : "no") << '\n';
  b.push_back (3);
  std::cout << "after b.push_back(3) a==b=" << (a == b ? "yes" : "no") << '\n';
  a.clear ();
  print_buffer ("cleared a", a);

  std::cout << "\n=== CircularBuffer example finished ===\n";
  return 0;
}
