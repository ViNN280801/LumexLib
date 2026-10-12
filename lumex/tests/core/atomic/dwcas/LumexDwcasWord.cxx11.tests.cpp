// dwcas_word and dwcas_value_t single-threaded: layout, the five operations
// with every pattern of halves, the value returned by a compare-and-swap,
// every memory order, a const word, the speculative read, the placement and
// the faults the documentation promises (a misaligned word, a word on a
// read-only page).

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

#include <gtest/gtest.h>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
#endif

#include "lumex/tests/core/atomic/dwcas/LumexDwcasTestSupport.hpp"
#include "lumex/tests/support/LumexTestSubprocess.hpp"

#if LUMEX_ATOMIC_HAS_DWCAS

using namespace dwcas_test;
using namespace lumex::core::atomic::dwcas;

static_assert (sizeof (dwcas_word) == 16, "two halves, no padding");
static_assert (alignof (dwcas_word) == 16, "cmpxchg16b needs 16 bytes");
static_assert (sizeof (dwcas_value_t) == 16, "two 64-bit halves");
static_assert (std::is_standard_layout<dwcas_value_t>::value,
               "the value is a plain aggregate");
static_assert (std::is_trivially_copyable<dwcas_value_t>::value,
               "the value is passed in registers");
static_assert (std::is_standard_layout<dwcas_word>::value,
               "the word is standard layout");
static_assert (std::is_trivially_destructible<dwcas_word>::value,
               "a word needs no destructor");
static_assert (!std::is_copy_constructible<dwcas_word>::value, "no copy");
static_assert (!std::is_copy_assignable<dwcas_word>::value, "no assignment");
static_assert (!std::is_move_constructible<dwcas_word>::value, "no move");
static_assert (!std::is_convertible<dwcas_word, dwcas_value_t>::value,
               "a word is not its value");
static_assert (!std::is_convertible<int, dwcas_value_t>::value,
               "a number is not a value");
static_assert (std::is_nothrow_default_constructible<dwcas_word>::value,
               "the constructor is noexcept");
static_assert (noexcept (std::declval<dwcas_word &> ().store (
                   std::declval<dwcas_value_t> ())),
               "store is noexcept");
static_assert (noexcept (std::declval<dwcas_word const &> ().load ()),
               "load is noexcept");
static_assert (
    noexcept (std::declval<dwcas_word &> ().compare_exchange_strong (
        std::declval<dwcas_value_t> (), std::declval<dwcas_value_t> ())),
    "compare_exchange_strong is noexcept");
// The constructors are constexpr: a word with static storage is constant
// initialized and needs no guard.
static_assert ((dwcas_word (), true), "default constructor is constexpr");
static_assert ((dwcas_word (dwcas_value_t{ 1u, 2u }), true),
               "value constructor is constexpr");
static_assert (dwcas_value_t{ 1u, 2u } == dwcas_value_t{ 1u, 2u },
               "equality is constexpr");
static_assert (dwcas_value_t{ 1u, 2u } != dwcas_value_t{ 1u, 3u },
               "inequality is constexpr");

namespace
{
dwcas_word g_static_word (dwcas_value_t{ 0x1111222233334444ull,
                                         0x5555666677778888ull });
dwcas_word const g_const_word (dwcas_value_t{ 7u, 9u });

std::memory_order const k_rmw_orders[]
    = { std::memory_order_relaxed, std::memory_order_consume,
        std::memory_order_acquire, std::memory_order_release,
        std::memory_order_acq_rel, std::memory_order_seq_cst };
std::memory_order const k_load_orders[]
    = { std::memory_order_relaxed, std::memory_order_consume,
        std::memory_order_acquire, std::memory_order_seq_cst };
std::memory_order const k_store_orders[]
    = { std::memory_order_relaxed, std::memory_order_release,
        std::memory_order_seq_cst };
} // namespace

TEST (LumexDwcasWordTest, GivenDefaultWord_WhenLoaded_ThenBothHalvesAreZero)
{
  dwcas_word word;
  dwcas_value_t const value = word.load ();
  EXPECT_EQ (value.lo, 0u);
  EXPECT_EQ (value.hi, 0u);
}

TEST (LumexDwcasWordTest, GivenInitialValue_WhenLoaded_ThenHalvesAreNotSwapped)
{
  dwcas_word word (make_value (0x0123456789ABCDEFull, 0xFEDCBA9876543210ull));
  dwcas_value_t const value = word.load ();
  EXPECT_EQ (value.lo, 0x0123456789ABCDEFull);
  EXPECT_EQ (value.hi, 0xFEDCBA9876543210ull);
}

TEST (LumexDwcasWordTest,
      GivenStaticWords_WhenLoaded_ThenTheyHoldTheirInitializer)
{
  EXPECT_EQ (g_static_word.load (),
             make_value (0x1111222233334444ull, 0x5555666677778888ull));
  EXPECT_EQ (g_const_word.load (), make_value (7u, 9u));
}

TEST (LumexDwcasWordTest, GivenConstWord_WhenLoadedAgain_ThenItStaysWritable)
{
  // The load of a const word is a locked write; a const object with mutable
  // members is not placed in read-only memory, so it must not fault.
  for (int i = 0; i < 100; ++i)
    EXPECT_EQ (g_const_word.load (), make_value (7u, 9u));
  dwcas_word const local (make_value (3u, 4u));
  EXPECT_EQ (local.load (), make_value (3u, 4u));
}

TEST (LumexDwcasWordTest,
      GivenEveryPattern_WhenStoredAndLoaded_ThenTheValueRoundTrips)
{
  dwcas_word word;
  for (std::size_t lo = 0; lo < pattern_count (); ++lo)
    for (std::size_t hi = 0; hi < pattern_count (); ++hi)
      {
        dwcas_value_t const wanted = make_value (pattern (lo), pattern (hi));
        word.store (wanted);
        dwcas_value_t const got = word.load ();
        ASSERT_EQ (got.lo, wanted.lo)
            << describe (wanted) << " " << describe (got);
        ASSERT_EQ (got.hi, wanted.hi)
            << describe (wanted) << " " << describe (got);
      }
}

TEST (LumexDwcasWordTest,
      GivenStoresOfOneHalfOnly_WhenLoaded_ThenTheOtherHalfIsKept)
{
  dwcas_word word (make_value (0xAAu, 0xBBu));
  word.store (make_value (0xCCu, 0xBBu));
  EXPECT_EQ (word.load (), make_value (0xCCu, 0xBBu));
  word.store (make_value (0xCCu, 0xDDu));
  EXPECT_EQ (word.load (), make_value (0xCCu, 0xDDu));
}

TEST (LumexDwcasWordTest,
      GivenExchange_WhenCalled_ThenReturnsThePreviousValueAndStoresTheNew)
{
  dwcas_word word (make_value (1u, 2u));
  EXPECT_EQ (word.exchange (make_value (3u, 4u)), make_value (1u, 2u));
  EXPECT_EQ (word.load (), make_value (3u, 4u));
  EXPECT_EQ (word.exchange (make_value (5u, 6u)), make_value (3u, 4u));
  EXPECT_EQ (word.exchange (make_value (5u, 6u)), make_value (5u, 6u));
  EXPECT_EQ (word.load (), make_value (5u, 6u));
}

TEST (
    LumexDwcasWordTest,
    GivenMatchingExpected_WhenStrongCompareExchange_ThenSwapsAndReturnsExpected)
{
  dwcas_word word (make_value (10u, 20u));
  dwcas_value_t const observed = word.compare_exchange_strong (
      make_value (10u, 20u), make_value (30u, 40u));
  EXPECT_EQ (observed, make_value (10u, 20u));
  EXPECT_EQ (word.load (), make_value (30u, 40u));
}

TEST (
    LumexDwcasWordTest,
    GivenOtherLowHalf_WhenStrongCompareExchange_ThenFailsAndReturnsTheCurrentValue)
{
  dwcas_word word (make_value (10u, 20u));
  dwcas_value_t const expected = make_value (11u, 20u);
  dwcas_value_t const observed
      = word.compare_exchange_strong (expected, make_value (30u, 40u));
  EXPECT_EQ (observed, make_value (10u, 20u));
  EXPECT_NE (observed, expected) << "a failure is told by the result";
  EXPECT_EQ (word.load (), make_value (10u, 20u));
}

TEST (
    LumexDwcasWordTest,
    GivenOtherHighHalf_WhenStrongCompareExchange_ThenFailsAndReturnsTheCurrentValue)
{
  dwcas_word word (make_value (10u, 20u));
  dwcas_value_t const observed = word.compare_exchange_strong (
      make_value (10u, 21u), make_value (30u, 40u));
  EXPECT_EQ (observed, make_value (10u, 20u));
  EXPECT_EQ (word.load (), make_value (10u, 20u));
}

TEST (
    LumexDwcasWordTest,
    GivenBothHalvesOther_WhenStrongCompareExchange_ThenFailsAndReturnsTheCurrentValue)
{
  dwcas_word word (make_value (0xFFFFFFFFFFFFFFFFull, 0x8000000000000000ull));
  dwcas_value_t const observed = word.compare_exchange_strong (
      make_value (0u, 0u), make_value (1u, 1u));
  EXPECT_EQ (observed,
             make_value (0xFFFFFFFFFFFFFFFFull, 0x8000000000000000ull));
  EXPECT_EQ (word.load (),
             make_value (0xFFFFFFFFFFFFFFFFull, 0x8000000000000000ull));
}

TEST (LumexDwcasWordTest,
      GivenHalvesSwapped_WhenStrongCompareExchange_ThenFails)
{
  // lo and hi are not interchangeable: {1, 2} is not {2, 1}.
  dwcas_word word (make_value (1u, 2u));
  EXPECT_EQ (
      word.compare_exchange_strong (make_value (2u, 1u), make_value (9u, 9u)),
      make_value (1u, 2u));
  EXPECT_EQ (word.load (), make_value (1u, 2u));
}

TEST (LumexDwcasWordTest,
      GivenEqualExpectedAndDesired_WhenCompareExchange_ThenTheValueIsKept)
{
  dwcas_word word (make_value (5u, 6u));
  EXPECT_EQ (
      word.compare_exchange_strong (make_value (5u, 6u), make_value (5u, 6u)),
      make_value (5u, 6u));
  EXPECT_EQ (word.load (), make_value (5u, 6u));
}

TEST (
    LumexDwcasWordTest,
    GivenEveryPairOfPatterns_WhenCompareExchange_ThenTheResultTellsSwapOrFailure)
{
  dwcas_word word;
  for (std::size_t a = 0; a < pattern_count (); ++a)
    for (std::size_t b = 0; b < pattern_count (); ++b)
      {
        dwcas_value_t const current = make_value (pattern (a), pattern (b));
        word.store (current);
        for (std::size_t c = 0; c < pattern_count (); c += 3)
          {
            dwcas_value_t const wrong_low
                = make_value (pattern (c) ^ 0x10u, pattern (b));
            dwcas_value_t const wrong_high
                = make_value (pattern (a), pattern (c) ^ 0x10u);
            ASSERT_EQ (word.compare_exchange_strong (wrong_low, tied (1u)),
                       current);
            ASSERT_EQ (word.compare_exchange_strong (wrong_high, tied (1u)),
                       current);
          }
        dwcas_value_t const next = make_value (pattern (b), pattern (a));
        ASSERT_EQ (word.compare_exchange_strong (current, next), current);
        ASSERT_EQ (word.load (), next);
      }
}

TEST (LumexDwcasWordTest,
      GivenWeakCompareExchange_WhenSingleThreaded_ThenItNeverFailsSpuriously)
{
  dwcas_word word (make_value (0u, 0u));
  for (std::uint64_t i = 0; i < 100000; ++i)
    {
      dwcas_value_t const expected = make_value (i, i * 3u);
      dwcas_value_t const desired = make_value (i + 1u, (i + 1u) * 3u);
      ASSERT_EQ (word.compare_exchange_weak (expected, desired), expected)
          << "iteration " << i;
    }
  EXPECT_EQ (word.load (), make_value (100000u, 300000u));
  EXPECT_EQ (word.compare_exchange_weak (make_value (1u, 1u), tied (2u)),
             make_value (100000u, 300000u));
}

TEST (LumexDwcasWordTest,
      GivenEveryMemoryOrder_WhenOperationsRun_ThenTheResultsDoNotDepend)
{
  dwcas_word word;
  for (std::size_t i = 0; i < sizeof k_store_orders / sizeof k_store_orders[0];
       ++i)
    {
      word.store (tied (i + 1u), k_store_orders[i]);
      EXPECT_EQ (word.load (), tied (i + 1u));
    }
  for (std::size_t i = 0; i < sizeof k_load_orders / sizeof k_load_orders[0];
       ++i)
    {
      word.store (tied (40u + i));
      EXPECT_EQ (word.load (k_load_orders[i]), tied (40u + i));
    }
  for (std::size_t i = 0; i < sizeof k_rmw_orders / sizeof k_rmw_orders[0];
       ++i)
    {
      word.store (tied (100u + i));
      EXPECT_EQ (word.exchange (tied (200u + i), k_rmw_orders[i]),
                 tied (100u + i));
      for (std::size_t f = 0;
           f < sizeof k_load_orders / sizeof k_load_orders[0]; ++f)
        {
          dwcas_value_t const before = word.load ();
          EXPECT_EQ (word.compare_exchange_strong (before, tied (300u + i + f),
                                                   k_rmw_orders[i],
                                                   k_load_orders[f]),
                     before);
          EXPECT_EQ (word.load (), tied (300u + i + f));
          EXPECT_EQ (word.compare_exchange_strong (tied (1u), tied (2u),
                                                   k_rmw_orders[i],
                                                   k_load_orders[f]),
                     tied (300u + i + f));
          EXPECT_EQ (word.compare_exchange_weak (tied (1u), tied (2u),
                                                 k_rmw_orders[i],
                                                 k_load_orders[f]),
                     tied (300u + i + f));
        }
    }
}

TEST (LumexDwcasWordTest,
      GivenQuietWord_WhenSpeculativeLoad_ThenItEqualsTheValue)
{
  dwcas_word word (make_value (0x1234u, 0x5678u));
  EXPECT_EQ (word.speculative_load (), make_value (0x1234u, 0x5678u));
  word.store (make_value (0xFFFFFFFFFFFFFFFFull, 1u));
  EXPECT_EQ (word.speculative_load (), make_value (0xFFFFFFFFFFFFFFFFull, 1u));
  dwcas_word const constant (make_value (8u, 9u));
  EXPECT_EQ (constant.speculative_load (), make_value (8u, 9u));
}

TEST (
    LumexDwcasWordTest,
    GivenSpeculativeGuess_WhenUsedAsExpected_ThenTheCompareExchangeValidatesIt)
{
  dwcas_word word (make_value (1u, 2u));
  dwcas_value_t const guess = word.speculative_load ();
  word.store (make_value (3u, 4u));
  // The guess is out of date: the swap must fail and return the truth.
  EXPECT_EQ (word.compare_exchange_strong (guess, make_value (5u, 6u)),
             make_value (3u, 4u));
  EXPECT_EQ (word.load (), make_value (3u, 4u));
}

TEST (LumexDwcasWordTest,
      GivenWordsInEveryStorage_WhenAddressed_ThenTheyAre16ByteAligned)
{
  dwcas_word local;
  EXPECT_EQ (reinterpret_cast<std::uintptr_t> (&local) % 16u, 0u);
  EXPECT_EQ (reinterpret_cast<std::uintptr_t> (&g_static_word) % 16u, 0u);
  EXPECT_EQ (reinterpret_cast<std::uintptr_t> (&g_const_word) % 16u, 0u);
  dwcas_word locals[5];
  for (std::size_t i = 0; i < 5; ++i)
    EXPECT_EQ (reinterpret_cast<std::uintptr_t> (&locals[i]) % 16u, 0u) << i;
  struct holder_t
  {
    char tag;
    dwcas_word word;
    char tail;
  };
  holder_t holder;
  EXPECT_EQ (reinterpret_cast<std::uintptr_t> (&holder.word) % 16u, 0u);
  EXPECT_EQ (sizeof (holder_t) % 16u, 0u);
}

TEST (LumexDwcasWordTest,
      GivenAlignedStorage_WhenAWordIsPlacedThere_ThenItWorks)
{
  alignas (16) unsigned char storage[48];
  dwcas_word *words[3];
  for (std::size_t i = 0; i < 3; ++i)
    words[i] = new (storage + 16 * i) dwcas_word (tied (i + 1u));
  for (std::size_t i = 0; i < 3; ++i)
    EXPECT_EQ (words[i]->load (), tied (i + 1u));
  EXPECT_EQ (words[1]->exchange (tied (50u)), tied (2u));
  EXPECT_EQ (words[0]->load (), tied (1u)) << "neighbours are not touched";
  EXPECT_EQ (words[2]->load (), tied (3u)) << "neighbours are not touched";
  for (std::size_t i = 0; i < 3; ++i)
    words[i]->~dwcas_word ();
}

TEST (LumexDwcasWordTest,
      GivenMisalignedWord_WhenLoaded_ThenTheCpuFaultsInTheChild)
{
  if (!backend_is_hardware ())
    GTEST_SKIP () << "the built-in backend goes through libatomic";
  if (!lumex_test::subprocess_available ())
    GTEST_SKIP () << "no fork on this platform";
  lumex_test::ChildResult const result = lumex_test::run_in_child (
      []
        {
          alignas (16) unsigned char storage[48] = {};
          dwcas_word *word
              = new (storage + 8) dwcas_word (make_value (1u, 2u));
          dwcas_value_t const value = word->load ();
          return value.lo == 1u ? 0 : 2;
        });
  EXPECT_TRUE (lumex_test::child_was_caught (result))
      << "cmpxchg16b on an address that is not 16-byte aligned must fault, "
         "the child "
      << lumex_test::describe_child (result);
}

#if defined(__unix__) || defined(__APPLE__)
TEST (LumexDwcasWordTest,
      GivenWordOnReadOnlyPage_WhenLoaded_ThenTheHardwareBackendFaults)
{
  if (!backend_is_hardware ())
    GTEST_SKIP () << "the built-in backend may read without writing";
  if (!lumex_test::subprocess_available ())
    GTEST_SKIP () << "no fork on this platform";
  lumex_test::ChildResult const speculative = lumex_test::run_in_child (
      []
        {
          long const page = sysconf (_SC_PAGESIZE);
          void *memory = mmap (nullptr, static_cast<std::size_t> (page),
                               PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
          if (memory == MAP_FAILED)
            return 3;
          dwcas_word *word = new (memory) dwcas_word (make_value (4u, 5u));
          if (mprotect (memory, static_cast<std::size_t> (page), PROT_READ)
              != 0)
            return 4;
          // A read that does not write is fine on a read-only page.
          return word->speculative_load () == make_value (4u, 5u) ? 0 : 2;
        });
  EXPECT_EQ (speculative.end, lumex_test::ChildEnd::clean)
      << lumex_test::describe_child (speculative);
  lumex_test::ChildResult const loading = lumex_test::run_in_child (
      []
        {
          long const page = sysconf (_SC_PAGESIZE);
          void *memory = mmap (nullptr, static_cast<std::size_t> (page),
                               PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
          if (memory == MAP_FAILED)
            return 3;
          dwcas_word *word = new (memory) dwcas_word (make_value (4u, 5u));
          if (mprotect (memory, static_cast<std::size_t> (page), PROT_READ)
              != 0)
            return 4;
          dwcas_value_t const value = word->load ();
          return value.lo == 4u ? 0 : 2;
        });
  EXPECT_TRUE (lumex_test::child_was_caught (loading))
      << "a 16-byte load is a locked write and must fault on a read-only "
         "page, the child "
      << lumex_test::describe_child (loading);
}
#endif

TEST (LumexDwcasValueTest, GivenValues_WhenCompared_ThenBothHalvesCount)
{
  EXPECT_TRUE (make_value (1u, 2u) == make_value (1u, 2u));
  EXPECT_FALSE (make_value (1u, 2u) != make_value (1u, 2u));
  EXPECT_FALSE (make_value (1u, 2u) == make_value (1u, 3u));
  EXPECT_FALSE (make_value (1u, 2u) == make_value (0u, 2u));
  EXPECT_TRUE (make_value (1u, 2u) != make_value (1u, 3u));
  EXPECT_TRUE (make_value (1u, 2u) != make_value (2u, 1u));
  EXPECT_TRUE (make_value (0u, 0u) == dwcas_value_t ());
}

TEST (LumexDwcasWordTest, GivenWord_WhenAskedIfLockFree_ThenYes)
{
  dwcas_word word;
  EXPECT_TRUE (word.is_lock_free ());
}

#else

TEST (LumexDwcasWordTest,
      GivenTargetWithoutTheLayer_WhenBuilt_ThenThereIsNothingToRun)
{
  GTEST_SKIP () << "LUMEX_ATOMIC_HAS_DWCAS is 0 on this target";
}

#endif // LUMEX_ATOMIC_HAS_DWCAS
