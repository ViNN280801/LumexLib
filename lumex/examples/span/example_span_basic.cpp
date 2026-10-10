#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

#include "lumex/core/span/LumexSpan"

using namespace lumex::core::span::view;

namespace
{
// One parameter type for every contiguous source, in C++11 and later.
std::int64_t
sum (span<std::int32_t const> values)
{
  std::int64_t total = 0;
  for (std::int32_t value : values)
    total += value;
  return total;
}
} // namespace

int
main ()
{
  std::int32_t raw[4] = { 1, 2, 3, 4 };
  std::vector<std::int32_t> vector_values = { 10, 20, 30 };
  std::array<std::int32_t, 2> array_values = { { 100, 200 } };

  std::cout << sum (raw) << ' ' << sum (vector_values) << ' '
            << sum (array_values) << '\n';

  span<std::int32_t> const window (vector_values);
  std::cout << sum (window.subspan (1)) << ' ' << sum (window.first (2)) << ' '
            << as_bytes (window).size () << '\n';
  return 0;
}
