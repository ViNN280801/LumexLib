#include <iostream>
#include <string>

#include "lumex/core/circular_buffer/CircularBuffer"

using namespace lumex::core::circular_buffer::buffer;

int
main ()
{
  std::cout << "=== Workflow: keep the last N instrument readings ===\n\n";

  CircularBuffer<double> window (5);
  double const incoming[] = { 0.12, 0.15, 0.14, 0.91, 0.16, 0.17, 0.18 };
  for (double const sample : incoming)
    {
      window.push_back (sample);
      double sum = 0.0;
      for (CircularBuffer<double>::size_type i = 0; i < window.size (); ++i)
        sum += window[i];
      double const mean = sum / static_cast<double> (window.size ());
      std::cout << "sample=" << sample << " window=";
      for (CircularBuffer<double>::size_type i = 0; i < window.size (); ++i)
        {
          std::cout << window[i];
          if (i + 1U < window.size ())
            std::cout << ',';
        }
      std::cout << " mean=" << mean
                << " full=" << (window.full () ? "yes" : "no") << '\n';
    }

  std::cout << "oldest=" << window.front () << " newest=" << window.back ()
            << '\n';
  return 0;
}
