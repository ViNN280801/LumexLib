// dwcas_word under contention: tearing (writers store pairs whose halves are
// tied, readers assert the tie), lost updates (a counter in both halves),
// exchange as a permutation, the value a failed compare-and-swap returns,
// linearizability of short histories, message passing and mutual exclusion
// built on the word (these are the tests ThreadSanitizer judges on the
// built-in backend), and neighbours that must not disturb each other.
//
// The scenarios are templates over a small adapter, so the same checks run on
// two deliberately broken words (the halves written one after the other, a
// compare-and-swap that is a load followed by a store): the checks must find
// the defect there. A scenario that cannot fail proves nothing.

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/core/atomic/dwcas/LumexDwcasTestSupport.hpp"
#include "lumex/tests/support/LumexTestHistory.hpp"
#include "lumex/tests/support/LumexTestSchedule.hpp"
#include "lumex/tests/support/LumexTestThreads.hpp"

#if LUMEX_ATOMIC_HAS_DWCAS

using namespace dwcas_test;

namespace
{
// --- Adapters ---------------------------------------------------------------

/// The word under test.
class real_word_t
{
public:
  explicit real_word_t (dwcas_value_t initial) : word_ (initial) {}

  dwcas_value_t
  load ()
  {
    return word_.load ();
  }

  void
  store (dwcas_value_t value)
  {
    word_.store (value);
  }

  dwcas_value_t
  exchange (dwcas_value_t value)
  {
    return word_.exchange (value);
  }

  dwcas_value_t
  cas (dwcas_value_t expected, dwcas_value_t desired)
  {
    return word_.compare_exchange_strong (expected, desired);
  }

  dwcas_value_t
  cas_weak (dwcas_value_t expected, dwcas_value_t desired)
  {
    return word_.compare_exchange_weak (expected, desired);
  }

  dwcas_value_t
  guess ()
  {
    return word_.speculative_load ();
  }

private:
  dwcas_word word_;
};

/// A broken word: the two halves are separate atomics written one after the
/// other, so a reader between the two writes sees a pair that never existed.
class torn_word_t
{
public:
  explicit torn_word_t (dwcas_value_t initial)
      : lo_ (initial.lo), hi_ (initial.hi)
  {
  }

  dwcas_value_t
  load ()
  {
    dwcas_value_t value;
    value.lo = lo_.load (std::memory_order_seq_cst);
    value.hi = hi_.load (std::memory_order_seq_cst);
    return value;
  }

  void
  store (dwcas_value_t value)
  {
    lo_.store (value.lo, std::memory_order_seq_cst);
    std::this_thread::yield ();
    hi_.store (value.hi, std::memory_order_seq_cst);
  }

  dwcas_value_t
  exchange (dwcas_value_t value)
  {
    dwcas_value_t const old = load ();
    store (value);
    return old;
  }

  dwcas_value_t
  cas (dwcas_value_t expected, dwcas_value_t desired)
  {
    dwcas_value_t const old = load ();
    if (old == expected)
      store (desired);
    return old;
  }

  dwcas_value_t
  cas_weak (dwcas_value_t expected, dwcas_value_t desired)
  {
    return cas (expected, desired);
  }

  dwcas_value_t
  guess ()
  {
    return load ();
  }

private:
  std::atomic<std::uint64_t> lo_;
  std::atomic<std::uint64_t> hi_;
};

/// A broken compare-and-swap: the comparison and the store are separate
/// steps, so two threads can both succeed from the same value (a lost
/// update). The halves themselves are written together by a real word.
class racy_word_t
{
public:
  explicit racy_word_t (dwcas_value_t initial) : word_ (initial) {}

  dwcas_value_t
  load ()
  {
    return word_.load ();
  }

  void
  store (dwcas_value_t value)
  {
    word_.store (value);
  }

  dwcas_value_t
  exchange (dwcas_value_t value)
  {
    return word_.exchange (value);
  }

  dwcas_value_t
  cas (dwcas_value_t expected, dwcas_value_t desired)
  {
    dwcas_value_t const old = word_.load ();
    if (old == expected)
      {
        std::this_thread::yield ();
        word_.store (desired);
      }
    return old;
  }

  dwcas_value_t
  cas_weak (dwcas_value_t expected, dwcas_value_t desired)
  {
    return cas (expected, desired);
  }

  dwcas_value_t
  guess ()
  {
    return word_.speculative_load ();
  }

private:
  dwcas_word word_;
};

// --- Scenarios --------------------------------------------------------------

/// Writers store, exchange and compare-and-swap pairs with tied halves;
/// readers load and look at what every operation returns. Every value that
/// comes out must be tied.
template <typename Word>
void
tearing_scenario (int threads, int operations, std::uint64_t seed,
                  lumex_test::Verdict &verdict)
{
  Word word (tied (0u));
  std::atomic<long> torn_guesses (0);
  std::string const error = lumex_test::run_threads (
      threads,
      [&] (int index)
        {
          lumex_test::SeededRandom random (lumex_test::derive_seed (
              seed, static_cast<std::uint64_t> (index)));
          bool const writer = threads == 1 || index % 2 == 0;
          bool const reader = threads == 1 || index % 2 == 1;
          std::uint64_t counter = 0;
          for (int i = 0; i < operations; ++i)
            {
              std::uint64_t const token
                  = (static_cast<std::uint64_t> (index + 1) << 40) | ++counter;
              if (writer)
                {
                  switch (i % 4)
                    {
                    case 0:
                      word.store (tied (token));
                      break;
                    case 1:
                      {
                        dwcas_value_t const old = word.exchange (tied (token));
                        verdict.require (is_tied (old),
                                         "exchange returned a torn value "
                                             + describe (old));
                        break;
                      }
                    case 2:
                      {
                        dwcas_value_t const current = word.load ();
                        dwcas_value_t const old
                            = word.cas (current, tied (token));
                        verdict.require (is_tied (old),
                                         "cas returned a torn value "
                                             + describe (old));
                        break;
                      }
                    default:
                      {
                        dwcas_value_t const old
                            = word.cas (tied (1u), tied (token));
                        verdict.require (is_tied (old),
                                         "failing cas returned a torn value "
                                             + describe (old));
                        break;
                      }
                    }
                }
              if (reader)
                {
                  dwcas_value_t const seen = word.load ();
                  verdict.require (is_tied (seen),
                                   "load returned a torn value "
                                       + describe (seen));
                  dwcas_value_t const guess = word.guess ();
                  if (!is_tied (guess))
                    {
                      // A guess may be torn; the compare-and-swap must not
                      // accept a pair the word never held.
                      torn_guesses.fetch_add (1, std::memory_order_relaxed);
                      dwcas_value_t const found = word.cas (guess, guess);
                      verdict.require (!(found == guess),
                                       "cas accepted the torn pair "
                                           + describe (guess));
                    }
                }
              if (random.below (64u) == 0u)
                std::this_thread::yield ();
            }
        });
  verdict.require (error.empty (), error);
}

/// Every thread adds one to a counter kept in both halves (the high half is
/// three times the low one) with a compare-and-swap loop. No update may be
/// lost and no observed pair may break the relation.
template <typename Word>
void
counter_scenario (int threads, int increments, bool weak,
                  lumex_test::Verdict &verdict)
{
  Word word (make_value (0u, 0u));
  std::string const error = lumex_test::run_threads (
      threads,
      [&] (int)
        {
          dwcas_value_t current = word.load ();
          for (int i = 0; i < increments; ++i)
            {
              for (;;)
                {
                  if (current.hi != current.lo * 3u)
                    {
                      verdict.fail ("counter pair broke the relation "
                                    + describe (current));
                      current = word.load ();
                    }
                  dwcas_value_t const next
                      = make_value (current.lo + 1u, (current.lo + 1u) * 3u);
                  dwcas_value_t const found
                      = weak ? word.cas_weak (current, next)
                             : word.cas (current, next);
                  if (found == current)
                    {
                      current = next;
                      break;
                    }
                  current = found;
                }
            }
        });
  verdict.require (error.empty (), error);
  std::uint64_t const total = static_cast<std::uint64_t> (threads)
                              * static_cast<std::uint64_t> (increments);
  dwcas_value_t const final_value = word.load ();
  verdict.require (final_value == make_value (total, total * 3u),
                   "lost updates: expected "
                       + describe (make_value (total, total * 3u)) + ", got "
                       + describe (final_value));
}

/// Every thread exchanges unique tokens in. The values that come out, plus
/// the final value, are exactly the initial value and all tokens, each once.
template <typename Word>
void
exchange_scenario (int threads, int operations, lumex_test::Verdict &verdict)
{
  Word word (tied (0u));
  std::vector<std::vector<std::uint64_t>> outs (
      static_cast<std::size_t> (threads));
  std::string const error = lumex_test::run_threads (
      threads,
      [&] (int index)
        {
          std::vector<std::uint64_t> &mine
              = outs[static_cast<std::size_t> (index)];
          for (int i = 0; i < operations; ++i)
            {
              std::uint64_t const token
                  = static_cast<std::uint64_t> (index) * 1000000u
                    + static_cast<std::uint64_t> (i) + 1u;
              dwcas_value_t const old = word.exchange (tied (token));
              mine.push_back (old.lo);
              if (!is_tied (old))
                verdict.fail ("exchange returned a torn value "
                              + describe (old));
            }
        });
  verdict.require (error.empty (), error);
  std::vector<std::uint64_t> all;
  for (std::size_t i = 0; i < outs.size (); ++i)
    all.insert (all.end (), outs[i].begin (), outs[i].end ());
  all.push_back (word.load ().lo);
  std::vector<std::uint64_t> expected;
  expected.push_back (0u);
  for (int t = 0; t < threads; ++t)
    for (int i = 0; i < operations; ++i)
      expected.push_back (static_cast<std::uint64_t> (t) * 1000000u
                          + static_cast<std::uint64_t> (i) + 1u);
  std::sort (all.begin (), all.end ());
  std::sort (expected.begin (), expected.end ());
  verdict.require (all == expected,
                   "the values that left the word are not the values that "
                   "entered it, each once (a value was lost or duplicated)");
}

/// Compare-and-swap with expected values that usually fail: the value a
/// failure returns must be a value some thread stored.
template <typename Word>
void
failure_value_scenario (int threads, int operations, std::uint64_t seed,
                        lumex_test::Verdict &verdict)
{
  Word word (tied (0u));
  std::uint64_t const bound = static_cast<std::uint64_t> (threads)
                                  * static_cast<std::uint64_t> (operations)
                              + 1u;
  std::string const error = lumex_test::run_threads (
      threads,
      [&] (int index)
        {
          lumex_test::SeededRandom random (lumex_test::derive_seed (
              seed, 1000u + static_cast<std::uint64_t> (index)));
          for (int i = 0; i < operations; ++i)
            {
              std::uint64_t const mine
                  = static_cast<std::uint64_t> (index)
                        * static_cast<std::uint64_t> (operations)
                    + static_cast<std::uint64_t> (i) + 1u;
              dwcas_value_t expected
                  = tied (random.below (static_cast<std::uint32_t> (bound)));
              dwcas_value_t const found = word.cas (expected, tied (mine));
              if (!is_tied (found) || found.lo >= bound)
                verdict.fail ("failure returned a value nobody stored: "
                              + describe (found));
              if (found == expected)
                {
                  // a swap happened: the word now holds mine or a later value
                  dwcas_value_t const now = word.load ();
                  if (!is_tied (now) || now.lo >= bound)
                    verdict.fail ("load after a swap returned "
                                  + describe (now));
                }
            }
        });
  verdict.require (error.empty (), error);
}
} // namespace

// --- The tests --------------------------------------------------------------

TEST (LumexDwcasConcurrentTest,
      GivenWritersAndReaders_WhenPairsAreTied_ThenNoOperationReturnsATornValue)
{
  lumex_test::TestWatchdog watchdog ("dwcas tearing");
  std::vector<int> const counts = lumex_test::thread_counts ();
  for (std::size_t c = 0; c < counts.size (); ++c)
    {
      lumex_test::Verdict verdict;
      tearing_scenario<real_word_t> (
          counts[c], lumex_test::scaled (40000),
          lumex_test::derive_seed (lumex_test::base_seed (), 1u, c), verdict);
      EXPECT_TRUE (verdict.ok ())
          << verdict.text () << "\n"
          << lumex_test::replay_text (lumex_test::base_seed (), counts[c]);
    }
}

TEST (LumexDwcasConcurrentTest,
      GivenCasLoopsOnBothHalves_WhenManyThreadsIncrement_ThenNoUpdateIsLost)
{
  lumex_test::TestWatchdog watchdog ("dwcas counter");
  std::vector<int> const counts = lumex_test::thread_counts ();
  for (std::size_t c = 0; c < counts.size (); ++c)
    for (int weak = 0; weak < 2; ++weak)
      {
        lumex_test::Verdict verdict;
        counter_scenario<real_word_t> (counts[c], lumex_test::scaled (20000),
                                       weak != 0, verdict);
        EXPECT_TRUE (verdict.ok ()) << verdict.text () << " (threads "
                                    << counts[c] << ", weak " << weak << ")";
      }
}

TEST (LumexDwcasConcurrentTest,
      GivenUniqueTokens_WhenExchangedByManyThreads_ThenEachLeavesTheWordOnce)
{
  lumex_test::TestWatchdog watchdog ("dwcas exchange");
  std::vector<int> const counts = lumex_test::thread_counts ();
  for (std::size_t c = 0; c < counts.size (); ++c)
    {
      lumex_test::Verdict verdict;
      exchange_scenario<real_word_t> (counts[c], lumex_test::scaled (20000),
                                      verdict);
      EXPECT_TRUE (verdict.ok ())
          << verdict.text () << " (threads " << counts[c] << ")";
    }
}

TEST (LumexDwcasConcurrentTest,
      GivenFailingCompareExchanges_WhenRacing_ThenTheReturnedValueWasStored)
{
  lumex_test::TestWatchdog watchdog ("dwcas failure value");
  std::vector<int> const counts = lumex_test::thread_counts ();
  for (std::size_t c = 0; c < counts.size (); ++c)
    {
      lumex_test::Verdict verdict;
      failure_value_scenario<real_word_t> (
          counts[c], lumex_test::scaled (20000),
          lumex_test::derive_seed (lumex_test::base_seed (), 2u, c), verdict);
      EXPECT_TRUE (verdict.ok ())
          << verdict.text () << " (threads " << counts[c] << ")";
    }
}

TEST (LumexDwcasConcurrentTest,
      GivenAWordWithSeparateHalves_WhenTheTearingScenarioRuns_ThenItIsCaught)
{
  // The negative control: the same checks must see the defect.
  lumex_test::TestWatchdog watchdog ("dwcas tearing control");
  lumex_test::Verdict verdict;
  tearing_scenario<torn_word_t> (4, lumex_test::scaled (20000),
                                 lumex_test::base_seed (), verdict);
  EXPECT_FALSE (verdict.ok ())
      << "a word whose halves are written one after the other must break "
         "the tie";
}

TEST (
    LumexDwcasConcurrentTest,
    GivenANonAtomicCompareExchange_WhenTheCounterScenarioRuns_ThenLostUpdatesAreCaught)
{
  lumex_test::TestWatchdog watchdog ("dwcas counter control");
  lumex_test::Verdict verdict;
  counter_scenario<racy_word_t> (4, lumex_test::scaled (5000), false, verdict);
  EXPECT_FALSE (verdict.ok ())
      << "a compare-and-swap that is a load and a store must lose updates";
}

TEST (
    LumexDwcasConcurrentTest,
    GivenANonAtomicExchange_WhenTheExchangeScenarioRuns_ThenDuplicatesAreCaught)
{
  lumex_test::TestWatchdog watchdog ("dwcas exchange control");
  lumex_test::Verdict verdict;
  exchange_scenario<torn_word_t> (4, lumex_test::scaled (5000), verdict);
  EXPECT_FALSE (verdict.ok ())
      << "an exchange that is a load and a store must duplicate or lose "
         "values";
}

// --- Linearizability --------------------------------------------------------

namespace
{
std::int64_t
id_of (dwcas_value_t const &value)
{
  return static_cast<std::int64_t> (value.lo);
}

/// One round: @p threads threads run @p ops_per_thread random operations on
/// a word that starts as id 0; the history is checked against a sequential
/// register.
template <typename Word>
lumex_test::Linearizability
linearizability_round (int threads, int ops_per_thread, std::uint64_t seed,
                       std::string &report)
{
  Word word (tied (0u));
  lumex_test::HistoryRecorder recorder (threads);
  std::string const error = lumex_test::run_threads (
      threads,
      [&] (int index)
        {
          lumex_test::SeededRandom random (lumex_test::derive_seed (
              seed, static_cast<std::uint64_t> (index)));
          for (int i = 0; i < ops_per_thread; ++i)
            {
              std::uint64_t const argument = random.below (4u);
              std::uint64_t const desired = random.below (4u);
              std::uint64_t const kind = random.below (5u);
              std::uint64_t const invoke = recorder.begin ();
              switch (kind)
                {
                case 0:
                  {
                    dwcas_value_t const value = word.load ();
                    recorder.end (index, lumex_test::register_load, invoke, 0,
                                  0, id_of (value), 0);
                    break;
                  }
                case 1:
                  word.store (tied (argument));
                  recorder.end (index, lumex_test::register_store, invoke,
                                static_cast<std::int64_t> (argument), 0, 0, 0);
                  break;
                case 2:
                  {
                    dwcas_value_t const old = word.exchange (tied (argument));
                    recorder.end (index, lumex_test::register_exchange, invoke,
                                  static_cast<std::int64_t> (argument), 0,
                                  id_of (old), 0);
                    break;
                  }
                default:
                  {
                    bool const weak = kind == 4;
                    dwcas_value_t const found
                        = weak
                              ? word.cas_weak (tied (argument), tied (desired))
                              : word.cas (tied (argument), tied (desired));
                    bool const swapped = found == tied (argument);
                    recorder.end (index,
                                  weak ? lumex_test::register_cas_weak
                                       : lumex_test::register_cas_strong,
                                  invoke, static_cast<std::int64_t> (argument),
                                  static_cast<std::int64_t> (desired),
                                  swapped ? 1 : 0,
                                  swapped ? 0 : id_of (found));
                    break;
                  }
                }
            }
        });
  std::vector<lumex_test::HistoryOp> const history = recorder.history ();
  lumex_test::Linearizability const result
      = lumex_test::is_linearizable<lumex_test::RegisterModel> (history, 0);
  if (!error.empty ())
    report = error;
  else if (result != lumex_test::Linearizability::linearizable)
    report = lumex_test::describe_history<lumex_test::RegisterModel> (history);
  return error.empty () ? result
                        : lumex_test::Linearizability::not_linearizable;
}
} // namespace

TEST (
    LumexDwcasConcurrentTest,
    GivenShortHistories_WhenCheckedAgainstARegister_ThenEveryOneIsLinearizable)
{
  lumex_test::TestWatchdog watchdog ("dwcas linearizability");
  int const rounds = lumex_test::scaled (1500);
  for (int round = 0; round < rounds; ++round)
    {
      std::string report;
      std::uint64_t const seed = lumex_test::derive_seed (
          lumex_test::base_seed (), 3u, static_cast<std::uint64_t> (round));
      int const threads = 2 + round % 2;
      lumex_test::Linearizability const result
          = linearizability_round<real_word_t> (threads, 4, seed, report);
      ASSERT_EQ (result, lumex_test::Linearizability::linearizable)
          << "round " << round << " history:" << report << "\n"
          << lumex_test::replay_text (seed, threads);
    }
}

TEST (
    LumexDwcasConcurrentTest,
    GivenANonAtomicCompareExchange_WhenHistoriesAreChecked_ThenSomeIsNotLinearizable)
{
  // The negative control of the checker: a word whose compare-and-swap is a
  // load and a store produces histories no register can explain.
  lumex_test::TestWatchdog watchdog ("dwcas linearizability control");
  bool caught = false;
  for (int round = 0; round < 20000 && !caught; ++round)
    {
      std::string report;
      caught
          = linearizability_round<racy_word_t> (
                3, 4,
                lumex_test::derive_seed (lumex_test::base_seed (), 4u,
                                         static_cast<std::uint64_t> (round)),
                report)
            != lumex_test::Linearizability::linearizable;
    }
  EXPECT_TRUE (caught);
}

TEST (LumexDwcasConcurrentTest,
      GivenAnImpossibleHistory_WhenChecked_ThenTheCheckerRejectsIt)
{
  // Sanity of the use of the checker: a load that returns a value nobody
  // stored, after a store, cannot be explained.
  std::vector<lumex_test::HistoryOp> history;
  lumex_test::HistoryOp store
      = { 0, lumex_test::register_store, 1, 0, 0, 0, 1, 2 };
  lumex_test::HistoryOp load
      = { 1, lumex_test::register_load, 0, 0, 2, 0, 3, 4 };
  history.push_back (store);
  history.push_back (load);
  EXPECT_EQ (
      lumex_test::is_linearizable<lumex_test::RegisterModel> (history, 0),
      lumex_test::Linearizability::not_linearizable);
  history[1].res0 = 1;
  EXPECT_EQ (
      lumex_test::is_linearizable<lumex_test::RegisterModel> (history, 0),
      lumex_test::Linearizability::linearizable);
}

// --- Synchronization built on the word --------------------------------------

TEST (LumexDwcasConcurrentTest,
      GivenAReleaseStore_WhenAnAcquireLoadSeesIt_ThenThePayloadIsVisible)
{
  // Message passing. The payload is plain memory, so the verdict on the
  // orders is ThreadSanitizer's (the built-in backend): a data race on the
  // payload is reported if store/load did not order it.
  lumex_test::TestWatchdog watchdog ("dwcas message passing");
  int const rounds = lumex_test::scaled (2000);
  int payload[16];
  dwcas_word flag (tied (0u));
  lumex_test::StartBarrier barrier (2);
  std::atomic<int> wrong (0);
  std::thread reader (
      [&]
        {
          for (int round = 1; round <= rounds; ++round)
            {
              barrier.arrive_and_wait ();
              std::uint64_t const wanted = static_cast<std::uint64_t> (round);
              dwcas_value_t seen;
              do
                seen = flag.load (std::memory_order_acquire);
              while (seen.lo != wanted);
              for (int i = 0; i < 16; ++i)
                if (payload[i] != round * 16 + i)
                  wrong.fetch_add (1);
              barrier.arrive_and_wait ();
            }
        });
  for (int round = 1; round <= rounds; ++round)
    {
      for (int i = 0; i < 16; ++i)
        payload[i] = round * 16 + i;
      barrier.arrive_and_wait ();
      flag.store (tied (static_cast<std::uint64_t> (round)),
                  std::memory_order_release);
      barrier.arrive_and_wait ();
    }
  reader.join ();
  EXPECT_EQ (wrong.load (), 0);
}

TEST (LumexDwcasConcurrentTest,
      GivenASpinLockOfTheWord_WhenThreadsCountInside_ThenTheyExcludeEachOther)
{
  lumex_test::TestWatchdog watchdog ("dwcas spin lock");
  std::vector<int> const counts = lumex_test::thread_counts ();
  for (std::size_t c = 0; c < counts.size (); ++c)
    {
      dwcas_word lock (make_value (0u, 0u));
      long counter = 0; // plain: protected by the lock
      int const rounds = lumex_test::scaled (5000);
      std::string const error = lumex_test::run_threads (
          counts[c],
          [&] (int)
            {
              for (int i = 0; i < rounds; ++i)
                {
                  while (!(lock.compare_exchange_strong (
                               make_value (0u, 0u), make_value (1u, 1u),
                               std::memory_order_acquire,
                               std::memory_order_relaxed)
                           == make_value (0u, 0u)))
                    std::this_thread::yield ();
                  ++counter;
                  lock.store (make_value (0u, 0u), std::memory_order_release);
                }
            });
      EXPECT_TRUE (error.empty ()) << error;
      EXPECT_EQ (counter, static_cast<long> (counts[c]) * rounds)
          << "threads " << counts[c];
    }
}

TEST (
    LumexDwcasConcurrentTest,
    GivenTwoAdjacentWords_WhenHammeredSeparately_ThenTheyDoNotDisturbEachOther)
{
  lumex_test::TestWatchdog watchdog ("dwcas neighbours");
  dwcas_word words[2];
  words[0].store (make_value (0u, 0u));
  words[1].store (make_value (0u, 0u));
  int const rounds = lumex_test::scaled (50000);
  lumex_test::Verdict verdict;
  std::string const error = lumex_test::run_threads (
      4,
      [&] (int index)
        {
          dwcas_word &mine = words[index % 2];
          // Two threads share each word and add one to its counter through a
          // compare-and-swap; the high half stays twice the low one.
          for (int i = 0; i < rounds; ++i)
            {
              dwcas_value_t current = mine.load ();
              for (;;)
                {
                  if ((current.lo & 0xF000000000000000ull) != 0u
                      || current.hi != current.lo * 2u)
                    verdict.fail ("word " + std::to_string (index % 2)
                                  + " was disturbed: " + describe (current));
                  dwcas_value_t const next
                      = make_value (current.lo + 1u, (current.lo + 1u) * 2u);
                  dwcas_value_t const found
                      = mine.compare_exchange_strong (current, next);
                  if (found == current)
                    break;
                  current = found;
                }
            }
        });
  EXPECT_TRUE (error.empty ()) << error;
  EXPECT_TRUE (verdict.ok ()) << verdict.text ();
  EXPECT_EQ (words[0].load (),
             make_value (2u * static_cast<std::uint64_t> (rounds),
                         4u * static_cast<std::uint64_t> (rounds)));
  EXPECT_EQ (words[1].load (),
             make_value (2u * static_cast<std::uint64_t> (rounds),
                         4u * static_cast<std::uint64_t> (rounds)));
}

#else

TEST (LumexDwcasConcurrentTest,
      GivenTargetWithoutTheLayer_WhenBuilt_ThenThereIsNothingToRun)
{
  GTEST_SKIP () << "LUMEX_ATOMIC_HAS_DWCAS is 0 on this target";
}

#endif // LUMEX_ATOMIC_HAS_DWCAS
