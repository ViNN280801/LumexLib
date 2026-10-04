#include <cstdint>
#include <iostream>
#include <typeinfo>
#include <vector>

#include "lumex/core/utility/LumexUtility"

using namespace lumex::core::utility::demangle;
using namespace lumex::core::utility::numeric;
using namespace lumex::core::utility::process;
using lumex::core::utility::bit::count_leading_zeros;

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
  SafeComparator<unsigned char> packet (200);
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
#if LUMEX_HAS_CONCEPTS && LUMEX_HAS_STD_BIT_CAST && LUMEX_HAS_STD_RANGES      \
    && LUMEX_HAS_STD_IS_CONSTANT_EVALUATED
  std::uint32_t const pattern = 0x12345678U;
  std::cout << std::hex << "byte_swap(0x12345678)=0x"
            << lumex::core::utility::bit::byte_swap (pattern) << std::dec
            << '\n';
#else
  std::cout << "byte_swap needs C++20\n";
#endif

  std::cout << "\n=== Utility example finished ===\n";
  return 0;
}
