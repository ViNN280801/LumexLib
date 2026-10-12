// What the dwcas test files share: the names under test, the value helpers
// and the expectations about the backend.
//
// Every test file wraps its tests in `#if LUMEX_ATOMIC_HAS_DWCAS` and has a
// skipped placeholder test otherwise, so the directory also builds (and
// reports "skipped") on a target without the layer.
#ifndef LUMEX_TESTS_CORE_ATOMIC_DWCAS_TEST_SUPPORT_HPP
#define LUMEX_TESTS_CORE_ATOMIC_DWCAS_TEST_SUPPORT_HPP

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/atomic/dwcas/LumexDwcasWord.hpp"
#include "lumex/tests/support/LumexTestConfig.hpp"

#if LUMEX_ATOMIC_HAS_DWCAS

namespace dwcas_test
{
using lumex::core::atomic::dwcas::dwcas_value_t;
using lumex::core::atomic::dwcas::dwcas_word;

/// A value from its two halves.
inline dwcas_value_t
make_value (std::uint64_t lo, std::uint64_t hi)
{
  dwcas_value_t value = { lo, hi };
  return value;
}

/// A value whose halves are tied: `hi` is the complement of `lo`. A read
/// that combines the halves of two different tied values breaks the tie
/// (unless the two values are equal), so a torn read is detected by looking
/// at one value.
inline dwcas_value_t
tied (std::uint64_t seed)
{
  return make_value (seed, ~seed);
}

/// True when the halves of @p value are tied.
inline bool
is_tied (dwcas_value_t const &value)
{
  return value.hi == ~value.lo;
}

/// Readable form of a value, for failure messages.
inline std::string
describe (dwcas_value_t const &value)
{
  char buffer[64];
  std::snprintf (buffer, sizeof buffer, "{%016llx,%016llx}",
                 static_cast<unsigned long long> (value.lo),
                 static_cast<unsigned long long> (value.hi));
  return buffer;
}

/// True when the words of the backend in use are plain machine instructions
/// (the assembly backend and the MSVC wrapper): a 16-byte load writes, a
/// misaligned word faults.
inline bool
backend_is_hardware ()
{
  return LUMEX_DWCAS_BACKEND != LUMEX_DWCAS_BACKEND_BUILTIN;
}

/// Halves that exercise every bit position and every combination.
inline std::size_t
pattern_count ()
{
  return 10;
}

/// The @p index-th pattern of `pattern_count ()`.
inline std::uint64_t
pattern (std::size_t index)
{
  static std::uint64_t const patterns[]
      = { 0x0000000000000000ull, 0x0000000000000001ull, 0x0000000000000002ull,
          0x8000000000000000ull, 0xFFFFFFFFFFFFFFFFull, 0x00000000FFFFFFFFull,
          0xFFFFFFFF00000000ull, 0x5555555555555555ull, 0xAAAAAAAAAAAAAAAAull,
          0x0123456789ABCDEFull };
  return patterns[index];
}
} // namespace dwcas_test

#endif // LUMEX_ATOMIC_HAS_DWCAS

#endif // !LUMEX_TESTS_CORE_ATOMIC_DWCAS_TEST_SUPPORT_HPP
