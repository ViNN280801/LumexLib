#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>
#if defined(__has_include)
#if __has_include(<span>)
#include <span>
#endif
#endif

#include "lumex/core/span/LumexSpan"

using namespace lumex::core::span::view;

#if LUMEX_SPAN_HAS_STD_SPAN
namespace
{
// An interface that takes the standard type ...
std::int64_t
total (std::span<std::int32_t const> values)
{
  std::int64_t sum = 0;
  for (std::int32_t value : values)
    sum += value;
  return sum;
}

// ... and one that takes the library's.
std::size_t
count_bytes (span<byte const> bytes)
{
  return bytes.size ();
}
} // namespace
#endif

int
main ()
{
#if LUMEX_SPAN_HAS_STD_SPAN
  std::vector<std::int32_t> values = { 4, 8, 15, 16, 23, 42 };
  span<std::int32_t> own (values);

  std::int64_t const sum = total (own); // span to std::span: implicit
  std::span<std::int32_t> standard (values);
  span<std::int32_t const> back = standard; // std::span to span: implicit
  std::cout << "sum=" << sum << " back=" << back.size () << '\n';

  // The byte views convert to the std::byte spans and back.
  std::span<std::byte const> raw = as_bytes (own);
  std::cout << "std::span<std::byte const> size=" << raw.size ()
            << " count_bytes=" << count_bytes (std::as_bytes (standard))
            << '\n';

  // A static extent follows the standard's rules: implicit to a dynamic span,
  // explicit from a dynamic one.
  span<std::int32_t, 6> fixed (values.data (), 6);
  std::span<std::int32_t> loose = fixed;
  std::span<std::int32_t, 6> exact (own);
  std::cout << "loose=" << loose.size () << " exact=" << exact.size () << '\n';
#else
  std::cout << "no std::span in this build (C++20 with <span> is needed)\n";
#endif
  return 0;
}
