// A stand-in for the one MSVC intrinsic the msvc_wrapper variant of the dwcas
// tests needs, for compilers that are not MSVC (this directory is put in front
// of the include path only for that variant, and never when the compiler is
// MSVC or clang-cl, which have the real header).
//
// _InterlockedCompareExchange128 (destination, exchange_high, exchange_low,
// comparand_result) is specified by Microsoft: it compares the 16 bytes at
// destination (16-byte aligned) with the pair at comparand_result (the low
// half first), stores {exchange_low, exchange_high} when they are equal and
// returns 1; otherwise it returns 0. In both cases comparand_result receives
// the value that was in memory. Here it is written with lock cmpxchg16b, so
// the wrapper of lumex/core/atomic/dwcas/LumexDwcasBackendMsvc.hpp is run with
// the argument order and the result of the real function.
#pragma once

#if !(defined(__GNUC__) || defined(__clang__))
#error "the intrin.h stand-in is for GCC and Clang"
#endif

inline unsigned char
_InterlockedCompareExchange128 (long long volatile *destination,
                                long long exchange_high,
                                long long exchange_low,
                                long long *comparand_result)
{
  unsigned long long low
      = static_cast<unsigned long long> (comparand_result[0]);
  unsigned long long high
      = static_cast<unsigned long long> (comparand_result[1]);
  unsigned long long const expected_low = low;
  unsigned long long const expected_high = high;
  __asm__ __volatile__ ("lock cmpxchg16b {(%[words])|xmmword ptr [%[words]]}"
                        : "+a"(low), "+d"(high)
                        : [words] "r"(destination), "b"(exchange_low),
                          "c"(exchange_high)
                        : "memory", "cc");
  comparand_result[0] = static_cast<long long> (low);
  comparand_result[1] = static_cast<long long> (high);
  return static_cast<unsigned char> (low == expected_low
                                     && high == expected_high);
}
