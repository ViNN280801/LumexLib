// Compile checks of the 128-bit compare-and-swap layer, built by
// cmake.dwcas_compile_checks. Exactly one of LUMEX_DWCAS_GOOD_CASE,
// LUMEX_DWCAS_GOOD_DISABLED_CASE, LUMEX_DWCAS_BAD_CASE=<n> or
// LUMEX_DWCAS_BAD_DISABLED_CASE=<n> is defined; the fixture compiles with
// warnings as errors (and the strict flags of a consumer).
//
// The BAD cases are the misuse the types are meant to stop: ignoring the
// result of an operation that returns the observed value, copying or
// assigning a word, passing a number where a value is wanted, writing through
// a const word, a layout that would unalign the word. The disabled cases name
// the layer when LUMEX_ATOMIC_DISABLE_DWCAS removed it.

#include <cstdint>

#include "lumex/core/atomic/LumexAtomic"

#if defined(LUMEX_DWCAS_GOOD_CASE)

using lumex::core::atomic::dwcas::dwcas_supported;
using lumex::core::atomic::dwcas::dwcas_value_t;
using lumex::core::atomic::dwcas::dwcas_word;
using lumex::core::atomic::dwcas::require_dwcas;

int
main ()
{
  static_assert (LUMEX_ATOMIC_HAS_DWCAS == 1, "x86-64 has the layer");
  static_assert (LUMEX_DWCAS_BACKEND != 0, "a backend is chosen");
  require_dwcas ();
  dwcas_word word;
  dwcas_word const constant (dwcas_value_t{ 1u, 2u });
  dwcas_value_t value = { 3u, 4u };
  int used = 0;
  word.store (value);
  word.store (value, std::memory_order_release);
  value = word.load ();
  value = word.load (std::memory_order_acquire);
  value = constant.load ();
  value = word.exchange (value);
  value = word.exchange (value, std::memory_order_acq_rel);
  value = word.compare_exchange_strong (value, value);
  value = word.compare_exchange_strong (
      value, value, std::memory_order_acq_rel, std::memory_order_acquire);
  value = word.compare_exchange_weak (value, value);
  value = word.compare_exchange_weak (value, value, std::memory_order_release,
                                      std::memory_order_relaxed);
  value = word.speculative_load ();
  value = constant.speculative_load ();
  used += word.is_lock_free () ? 1 : 0;
  used += dwcas_supported () ? 1 : 0;
  used += value == dwcas_value_t () ? 1 : 0;
  used += value != dwcas_value_t () ? 1 : 0;
  return used < 0 ? 1 : 0;
}

#elif defined(LUMEX_DWCAS_GOOD_DISABLED_CASE)

// With LUMEX_ATOMIC_DISABLE_DWCAS the layer is not declared and the module
// umbrella still compiles; the macros say so.
static_assert (LUMEX_ATOMIC_HAS_DWCAS == 0, "disabled");
static_assert (LUMEX_DWCAS_BACKEND == LUMEX_DWCAS_BACKEND_NONE, "no backend");

int
main ()
{
#if LUMEX_ATOMIC_HAS_DWCAS
  return 1;
#else
  return 0;
#endif
}

#elif defined(LUMEX_DWCAS_BAD_CASE)

using lumex::core::atomic::dwcas::dwcas_value_t;
using lumex::core::atomic::dwcas::dwcas_word;

#if LUMEX_DWCAS_BAD_CASE == 18
// A packed layout around the word: every compiler refuses or warns.
struct __attribute__ ((packed)) packed_holder_t
{
  char tag;
  dwcas_word word;
};
packed_holder_t g_packed;
#endif

int
main ()
{
  dwcas_word word;
  dwcas_word const constant;
  dwcas_value_t value = { 1u, 2u };
  int used = 0;
#if LUMEX_DWCAS_BAD_CASE == 1
  word.load (); // discarded: nodiscard
#elif LUMEX_DWCAS_BAD_CASE == 2
  word.exchange (value); // discarded: nodiscard
#elif LUMEX_DWCAS_BAD_CASE == 3
  word.compare_exchange_strong (value, value); // discarded: nodiscard
#elif LUMEX_DWCAS_BAD_CASE == 4
  word.compare_exchange_weak (value, value); // discarded: nodiscard
#elif LUMEX_DWCAS_BAD_CASE == 5
  dwcas_word copy (word); // no copy
  used += copy.is_lock_free () ? 1 : 0;
#elif LUMEX_DWCAS_BAD_CASE == 6
  dwcas_word other;
  other = word; // no assignment
#elif LUMEX_DWCAS_BAD_CASE == 7
  word.store (5); // a number is not a value
#elif LUMEX_DWCAS_BAD_CASE == 8
  word.store (std::uint64_t (5)); // one half is not a value
#elif LUMEX_DWCAS_BAD_CASE == 9
  used += word.compare_exchange_strong (1, 2) == value ? 1 : 0;
#elif LUMEX_DWCAS_BAD_CASE == 10
  dwcas_value_t taken = word; // a word is not its value
  used += taken == value ? 1 : 0;
#elif LUMEX_DWCAS_BAD_CASE == 11
  value = word.load (1); // an int is not a memory order
#elif LUMEX_DWCAS_BAD_CASE == 12
  dwcas_word two_numbers (1, 2); // the constructor takes one value
  used += two_numbers.is_lock_free () ? 1 : 0;
#elif LUMEX_DWCAS_BAD_CASE == 13
  dwcas_value_t three = { 1u, 2u, 3u }; // two halves only
  used += three == value ? 1 : 0;
#elif LUMEX_DWCAS_BAD_CASE == 14
  constant.store (value); // store of a const word
#elif LUMEX_DWCAS_BAD_CASE == 15
  value = constant.exchange (value); // exchange of a const word
#elif LUMEX_DWCAS_BAD_CASE == 16
  used += word == value ? 1 : 0; // a word is not comparable with a value
#elif LUMEX_DWCAS_BAD_CASE == 17
  value = word.compare_exchange_strong (value, value, 3); // not an order
#elif LUMEX_DWCAS_BAD_CASE == 18
  used += g_packed.tag;
#else
#error "unknown LUMEX_DWCAS_BAD_CASE"
#endif
  used += constant.is_lock_free () ? 1 : 0;
  used += value.lo == 0u ? 1 : 0;
  return used < 0 ? 1 : 0;
}

#elif defined(LUMEX_DWCAS_BAD_DISABLED_CASE)

int
main ()
{
#if LUMEX_DWCAS_BAD_DISABLED_CASE == 1
  lumex::core::atomic::dwcas::dwcas_word word;
#elif LUMEX_DWCAS_BAD_DISABLED_CASE == 2
  lumex::core::atomic::dwcas::dwcas_value_t value;
#elif LUMEX_DWCAS_BAD_DISABLED_CASE == 3
  return lumex::core::atomic::dwcas::dwcas_supported () ? 1 : 0;
#elif LUMEX_DWCAS_BAD_DISABLED_CASE == 4
  lumex::core::atomic::dwcas::require_dwcas ();
#elif LUMEX_DWCAS_BAD_DISABLED_CASE == 5
  return lumex::core::atomic::dwcas::Detail::cmpxchg16b_ecx_bit;
#else
#error "unknown LUMEX_DWCAS_BAD_DISABLED_CASE"
#endif
  return 0;
}

#else
#error "define one of the LUMEX_DWCAS_*_CASE macros"
#endif
