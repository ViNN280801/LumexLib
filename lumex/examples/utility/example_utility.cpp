#include <cstdint>
#include <cstring>
#include <iostream>
#include <typeinfo>
#include <vector>

#include "lumex/core/span/LumexSpan"
#include "lumex/core/utility/LumexUtility"

using namespace lumex::core::utility::demangle;
using namespace lumex::core::utility::numeric;
using namespace lumex::core::utility::process;
using lumex::core::span::view::span;
using lumex::core::utility::bit::count_leading_zeros;
using lumex::core::utility::mem::as;

int
main ()
{
  std::cout << "=== Utility: demangle, OS macros, SafeComparator, "
               "process, bit ===\n\n";

  std::cout << "--- 1. lumDemangle / demangle_type_name ---\n";
  std::cout << "demangle(vector<int>)=" << lumDemangle (std::vector<int>)
            << '\n';
  std::cout << "demangle_type_name="
            << demangle_type_name (typeid (std::vector<int>).name ()) << '\n';

  std::cout << "\n--- 2. OS compile-time flags ---\n";
#if defined(LUMEX_OS_WINDOWS)
  std::cout << "LUMEX_OS_WINDOWS=1\n";
#elif defined(LUMEX_OS_LINUX)
  std::cout << "LUMEX_OS_LINUX=1\n";
#else
  std::cout << "other OS\n";
#endif

  std::cout << "\n--- 3. SafeComparator across signed/unsigned ---\n";
  safe_comparator<unsigned char> packet (200);
  int const limit = 300;
  std::cout << "200u8 >= 300=" << (packet.safe_compare (limit) ? "yes" : "no")
            << " 200u8 < 300=" << (packet.safe_less (limit) ? "yes" : "no")
            << " 200u8 <= 200="
            << (packet.safe_less_equal (200) ? "yes" : "no") << '\n';

  std::cout << "\n--- 4. get_current_pid ---\n";
  std::cout << "pid=" << get_current_pid () << '\n';

  std::cout << "\n--- 5. count_leading_zeros ---\n";
  std::uint8_t const narrow = 0x0F;
  std::uint32_t const zero = 0;
  std::uint64_t const high_half = std::uint64_t (1) << 32;
  std::cout << "clz(uint8 0x0F)="
            << static_cast<unsigned> (count_leading_zeros (narrow))
            << " clz(uint32 0)="
            << static_cast<unsigned> (count_leading_zeros (zero))
            << " clz(uint64 1<<32)="
            << static_cast<unsigned> (count_leading_zeros (high_half)) << '\n';

  std::cout << "\n--- 6. byte_swap ---\n";
  std::uint32_t const pattern = 0x12345678U;
  std::cout << std::hex << "byte_swap(0x12345678)=0x"
            << lumex::core::utility::bit::byte_swap (pattern) << std::dec
            << '\n';

  std::cout << "\n--- 7. mem::as: a value out of raw bytes ---\n";
  // The bytes of a frame: a 16-bit and a 32-bit value, read in the byte order
  // of this machine through memcpy, so the offset need not be aligned.
  std::uint16_t const first = 0x1234;
  std::uint32_t const second = 0x0A0B0C0DU;
  unsigned char frame[sizeof (first) + sizeof (second)];
  std::memcpy (frame, &first, sizeof (first));
  std::memcpy (frame + sizeof (first), &second, sizeof (second));
  span<unsigned char const> const bytes (frame);
  // The result is std::optional from C++17 and lumex::optional before it.
  auto const word = as<std::uint16_t> (bytes);
  auto const tail = as<std::uint32_t> (bytes.subspan (sizeof (first)));
  auto const too_short = as<std::uint32_t> (bytes.subspan (4));
  std::cout << std::hex << "u16=0x" << (word.has_value () ? *word : 0)
            << " u32 at offset 2=0x" << (tail.has_value () ? *tail : 0U)
            << std::dec << " u32 at offset 4 present="
            << (too_short.has_value () ? "yes" : "no")
            << " pointer+size matches="
            << (as<std::uint16_t> (frame, sizeof (frame)) == word ? "yes"
                                                                  : "no")
            << '\n';

  std::cout << "\n=== Utility example finished ===\n";
  return 0;
}
