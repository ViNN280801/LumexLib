// Tests of split_counter, the packed {count:32, ext:32} word of the control
// block: the layout of the word, add / release / transfer_ext / settle, the
// "zero only when both halves are zero" rule, increment-if-nonzero, the
// use_count rule, a randomized comparison against an integer model, and
// threaded checks that exactly one thread sees the word become zero.

#include <atomic>
#include <cstdint>
#include <thread>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/core/smart_ptr/LumexSmartPtrTestSupport.hpp"
#include "lumex/tests/support/LumexTestSchedule.hpp"
#include "lumex/tests/support/LumexTestThreads.hpp"

namespace
{
using namespace smart_ptr_test;
using sp::detail::split_counter;

TEST (LumexSplitCounterTest,
      GivenACounter_WhenCreated_ThenCountIsSetAndExtIsZero)
{
  split_counter counter (1);
  EXPECT_EQ (counter.load (), 1u);
  EXPECT_EQ (split_counter::count_of (counter.load ()), 1);
  EXPECT_EQ (split_counter::ext_of (counter.load ()), 0);
  EXPECT_EQ (counter.use_count (), 1);
  split_counter big (0x7FFFFFFFu);
  EXPECT_EQ (split_counter::count_of (big.load ()), 0x7FFFFFFF);
  static_assert (sizeof (split_counter) == 8, "one 64-bit word");
  static_assert (!std::is_copy_constructible<split_counter>::value, "");
  static_assert (!std::is_copy_assignable<split_counter>::value, "");
}

TEST (LumexSplitCounterTest,
      GivenTheLayout_WhenCountAndExtChange_ThenTheyLiveInTheirHalves)
{
  split_counter counter (1);
  counter.add (4);
  EXPECT_EQ (counter.load (), 5u);
  counter.transfer_ext (3);
  EXPECT_EQ (counter.load (), (std::uint64_t (3) << 32) | 5u);
  EXPECT_EQ (split_counter::count_of (counter.load ()), 5);
  EXPECT_EQ (split_counter::ext_of (counter.load ()), 3);
  EXPECT_EQ (split_counter::ext_unit (), std::uint64_t (1) << 32);
  // ext is two's complement: it can go negative without touching count.
  split_counter negative (2);
  EXPECT_FALSE (negative.settle ());
  EXPECT_EQ (split_counter::count_of (negative.load ()), 2);
  EXPECT_EQ (split_counter::ext_of (negative.load ()), -1);
  EXPECT_EQ (negative.load (), 0xFFFFFFFF00000002ull);
  negative.transfer_ext (1);
  EXPECT_EQ (negative.load (), 2u);
}

TEST (LumexSplitCounterTest,
      GivenReleases_WhenCountReachesZero_ThenOnlyTheLastReturnsTrue)
{
  split_counter counter (3);
  EXPECT_FALSE (counter.release ());
  EXPECT_FALSE (counter.release ());
  EXPECT_TRUE (counter.release ());
  EXPECT_EQ (counter.load (), 0u);
  EXPECT_EQ (counter.use_count (), 0);
}

TEST (LumexSplitCounterTest,
      GivenAnExtDebt_WhenTheCountReachesZero_ThenTheWordIsNotZero)
{
  // The writer transfers two ticks, then drops its own count: not finished.
  split_counter counter (1);
  counter.transfer_ext (2);
  EXPECT_FALSE (counter.release ());
  EXPECT_EQ (split_counter::count_of (counter.load ()), 0);
  EXPECT_EQ (split_counter::ext_of (counter.load ()), 2);
  // The two readers settle; the second one finishes the word.
  EXPECT_FALSE (counter.settle ());
  EXPECT_TRUE (counter.settle ());
  EXPECT_EQ (counter.load (), 0u);
}

TEST (
    LumexSplitCounterTest,
    GivenASettleBeforeTheTransfer_WhenTheWriterCatchesUp_ThenTheWordStaysAlive)
{
  // A reader settles first (ext goes to -1) while the writer still holds the
  // slot count; the transfer then brings ext to 0 and the writer's release
  // finishes.
  split_counter counter (1);
  EXPECT_FALSE (counter.settle ());
  counter.transfer_ext (1);
  EXPECT_EQ (counter.load (), 1u);
  EXPECT_TRUE (counter.release ());
}

TEST (LumexSplitCounterTest,
      GivenTryAdd_WhenTheWordIsZero_ThenItFailsAndStaysZero)
{
  split_counter counter (1);
  EXPECT_TRUE (counter.try_add ());
  EXPECT_EQ (counter.use_count (), 2);
  EXPECT_FALSE (counter.release ());
  EXPECT_TRUE (counter.release ());
  EXPECT_FALSE (counter.try_add ());
  EXPECT_FALSE (counter.try_add ());
  EXPECT_EQ (counter.load (), 0u) << "a failed promotion must not resurrect";
}

TEST (LumexSplitCounterTest,
      GivenCountZeroAndExtPositive_WhenPromoting_ThenTheObjectIsAlive)
{
  split_counter counter (1);
  counter.transfer_ext (1);
  EXPECT_FALSE (counter.release ());
  // The object is alive (a reader pinned it): lock () may take a count.
  EXPECT_EQ (counter.use_count (), 1);
  EXPECT_TRUE (counter.try_add ());
  EXPECT_EQ (split_counter::count_of (counter.load ()), 1);
  EXPECT_FALSE (counter.release ()) << "ext is still owed";
  EXPECT_TRUE (counter.settle ());
}

TEST (LumexSplitCounterTest,
      GivenTheUseCountRule_WhenCountAndExtVary_ThenItFollowsTheSpec)
{
  split_counter counter (5);
  EXPECT_EQ (counter.use_count (), 5);
  counter.transfer_ext (2);
  EXPECT_EQ (counter.use_count (), 5) << "a positive count wins";
  split_counter pinned (1);
  pinned.transfer_ext (4);
  EXPECT_FALSE (pinned.release ());
  EXPECT_EQ (pinned.use_count (), 1) << "count 0, ext != 0 reports one owner";
  split_counter transient (1);
  EXPECT_FALSE (transient.settle ());
  EXPECT_EQ (transient.use_count (), 1) << "negative ext, positive count";
  split_counter gone (1);
  EXPECT_TRUE (gone.release ());
  EXPECT_EQ (gone.use_count (), 0);
}

TEST (LumexSplitCounterTest,
      GivenRandomOperations_WhenComparedWithAModel_ThenTheyAgreeStepByStep)
{
  // The model keeps count and ext as plain integers and says when the word is
  // zero. The sequence respects the protocol's preconditions: a release only
  // while count > 0, a settle only while it is owed or a transfer will come.
  lumex_test::SeededRandom random (
      lumex_test::derive_seed (lumex_test::base_seed (), 11));
  for (int round = 0; round < stress_work (200); ++round)
    {
      split_counter counter (1);
      long count = 1;
      long ext = 0;
      long owed = 0; // settles that are still to come
      bool finished = false;
      for (int step = 0; step < 60 && !finished; ++step)
        {
          switch (random.below (5))
            {
            case 0:
              counter.add (1);
              ++count;
              break;
            case 1:
              if (count > 0)
                {
                  bool const zero = counter.release ();
                  --count;
                  ASSERT_EQ (zero, count == 0 && ext == 0);
                  finished = zero;
                }
              break;
            case 2:
              {
                std::uint32_t const n = random.below (3);
                counter.transfer_ext (n);
                ext += n;
                owed += n;
                break;
              }
            case 3:
              if (owed > 0)
                {
                  bool const zero = counter.settle ();
                  --ext;
                  --owed;
                  ASSERT_EQ (zero, count == 0 && ext == 0);
                  finished = zero;
                }
              break;
            default:
              {
                bool const taken = counter.try_add ();
                ASSERT_EQ (taken, count != 0 || ext != 0);
                if (taken)
                  ++count;
                break;
              }
            }
          if (!finished)
            {
              ASSERT_EQ (split_counter::count_of (counter.load ()), count);
              ASSERT_EQ (split_counter::ext_of (counter.load ()), ext);
            }
        }
    }
}

TEST (LumexSplitCounterTest,
      GivenManyThreads_WhenTheyAddAndRelease_ThenExactlyOneSeesZero)
{
  for (int threads : lumex_test::thread_counts ())
    {
      split_counter counter (static_cast<std::uint32_t> (threads));
      std::atomic<int> zeros (0);
      int const rounds = stress_work (20000);
      std::string error = lumex_test::run_threads (
          threads,
          [&] (int index)
            {
              lumex_test::Schedule schedule (
                  lumex_test::ScheduleKind::yielding,
                  lumex_test::derive_seed (
                      lumex_test::base_seed (),
                      static_cast<std::uint64_t> (index)));
              for (int i = 0; i < rounds; ++i)
                {
                  counter.add (1);
                  if (counter.release ())
                    zeros.fetch_add (1);
                  if ((i & 255) == 0)
                    schedule.point ();
                }
              if (counter.release ())
                zeros.fetch_add (1);
            });
      EXPECT_EQ (error, "");
      EXPECT_EQ (zeros.load (), 1) << threads << " threads";
      EXPECT_EQ (counter.load (), 0u);
    }
}

TEST (LumexSplitCounterTest,
      GivenAWriterAndReaders_WhenTransferAndSettleRace_ThenZeroIsReachedOnce)
{
  // The engine's pattern in miniature: R readers each owe one settle; the
  // writer transfers R and drops its count. Whatever the interleaving, the
  // word becomes zero in exactly one operation and never before all of them
  // have run.
  for (int readers : { 1, 2, 4, 8 })
    {
      for (int round = 0; round < stress_work (200); ++round)
        {
          split_counter counter (1);
          std::atomic<int> zeros (0);
          std::atomic<int> finished_readers (0);
          std::atomic<bool> early (false);
          std::string error = lumex_test::run_threads (
              readers + 1,
              [&] (int index)
                {
                  if (index == 0)
                    {
                      counter.transfer_ext (
                          static_cast<std::uint32_t> (readers));
                      if (counter.release ())
                        {
                          // Only possible when every reader has already
                          // settled.
                          if (finished_readers.load () != readers)
                            early.store (true);
                          zeros.fetch_add (1);
                        }
                    }
                  else
                    {
                      // The reader's settle may come before the transfer: the
                      // word goes negative in ext and stays alive through the
                      // count. Counted before the settle: when a
                      // read-modify-write finds the word zero, every reader
                      // has counted itself.
                      finished_readers.fetch_add (1);
                      bool const zero = counter.settle ();
                      if (zero)
                        {
                          if (finished_readers.load () != readers)
                            early.store (true);
                          zeros.fetch_add (1);
                        }
                    }
                });
          ASSERT_EQ (error, "");
          ASSERT_EQ (zeros.load (), 1);
          ASSERT_FALSE (early.load ());
          ASSERT_EQ (counter.load (), 0u);
        }
    }
}

TEST (
    LumexSplitCounterTest,
    GivenPromotionsRacingTheLastRelease_WhenTheyRun_ThenNoPromotionSucceedsAfterZero)
{
  for (int round = 0; round < stress_work (500); ++round)
    {
      split_counter counter (1);
      std::atomic<int> promoted (0);
      std::atomic<int> zeros (0);
      std::atomic<bool> zero_seen (false);
      std::atomic<bool> late (false);
      std::string error
          = lumex_test::run_threads (4,
                                     [&] (int index)
                                       {
                                         if (index == 0)
                                           {
                                             if (counter.release ())
                                               {
                                                 zero_seen.store (true);
                                                 zeros.fetch_add (1);
                                               }
                                           }
                                         else
                                           {
                                             for (int i = 0; i < 50; ++i)
                                               {
                                                 // A success after the word
                                                 // was declared zero is a
                                                 // resurrection.
                                                 bool const declared_zero
                                                     = zero_seen.load ();
                                                 if (!counter.try_add ())
                                                   continue;
                                                 if (declared_zero)
                                                   late.store (true);
                                                 {
                                                   promoted.fetch_add (1);
                                                   if (counter.release ())
                                                     {
                                                       zero_seen.store (true);
                                                       zeros.fetch_add (1);
                                                     }
                                                 }
                                               }
                                           }
                                       });
      ASSERT_EQ (error, "");
      ASSERT_EQ (zeros.load (), 1);
      ASSERT_FALSE (late.load ());
      ASSERT_EQ (counter.load (), 0u);
    }
}

TEST (LumexSplitCounterTest,
      GivenTheInterface_WhenCheckingTraits_ThenItIsNoexcept)
{
  static_assert (noexcept (std::declval<split_counter &> ().add (1)), "");
  static_assert (noexcept (std::declval<split_counter &> ().release ()), "");
  static_assert (noexcept (std::declval<split_counter &> ().settle ()), "");
  static_assert (noexcept (std::declval<split_counter &> ().transfer_ext (1)),
                 "");
  static_assert (noexcept (std::declval<split_counter &> ().try_add ()), "");
  static_assert (
      noexcept (std::declval<split_counter const &> ().use_count ()), "");
  SUCCEED ();
}
} // namespace
