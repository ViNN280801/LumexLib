// LumexBit.cxx23.tests.cpp
// byte_swap against std::byteswap (C++23). LumexBit.hpp needs C++20 concepts,
// std::bit_cast, std::ranges and std::is_constant_evaluated, and the
// standard library must have std::byteswap (__cpp_lib_byteswap); otherwise
// the test skips. The suites from C++23 up compile this file together with
// LumexBit.cxx20.tests.cpp.
#include <bit>
#include <cstdint>
#include <version>

#include <gtest/gtest.h>

#include "lumex/core/utility/bit/LumexBit.hpp"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"

TEST (LumexBitTest,
      GivenCpp23Byteswap_WhenCompared_ThenMatchesStandardByteswap)
{
#if LUMEX_HAS_CONCEPTS && LUMEX_HAS_STD_BIT_CAST && LUMEX_HAS_STD_RANGES      \
    && LUMEX_HAS_STD_IS_CONSTANT_EVALUATED && defined(__cpp_lib_byteswap)     \
    && __cpp_lib_byteswap >= 202110L
  using lumex::core::utility::bit::byte_swap;

  std::uint16_t const u16 = 0xBEEF;
  std::uint32_t const u32 = 0xCAFEBABEu;
  std::uint64_t const u64 = 0x0123456789ABCDEFULL;
  EXPECT_EQ (byte_swap (u16), std::byteswap (u16));
  EXPECT_EQ (byte_swap (u32), std::byteswap (u32));
  EXPECT_EQ (byte_swap (u64), std::byteswap (u64));
  EXPECT_EQ (byte_swap (static_cast<std::int32_t> (-99)),
             std::byteswap (static_cast<std::int32_t> (-99)));
#else
  GTEST_SKIP () << "LumexBit.hpp or std::byteswap is not available";
#endif
}
