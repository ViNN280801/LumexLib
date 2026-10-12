// Tests of use_count_settled of split_counter and of ctl_base: it returns
// the count only when ext is zero, so it is the plain count in an ordinary
// program, and it waits (a bounded busy-wait, then yields) while owners are
// in transit. The waiting is observed with two threads: a thread that holds
// ext != 0 until the test releases it.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/tests/core/smart_ptr/LumexSmartPtrTestSupport.hpp"

namespace
{
using namespace smart_ptr_test;
using sp::detail::access;
using sp::detail::ctl_base;
using sp::detail::split_counter;

static_assert (
    noexcept (std::declval<split_counter const &> ().use_count_settled ()),
    "use_count_settled is noexcept");
static_assert (
    noexcept (std::declval<ctl_base const &> ().use_count_settled ()),
    "use_count_settled is noexcept");

TEST (LumexUseCountSettledTest, GivenNoExt_WhenAsked_ThenItIsTheCount)
{
  split_counter counter (3);
  EXPECT_EQ (counter.use_count_settled (), 3);
  counter.add (2);
  EXPECT_EQ (counter.use_count_settled (), 5);
  EXPECT_EQ (counter.use_count_settled (), counter.use_count ());
}

TEST (LumexUseCountSettledTest, GivenAZeroWord_WhenAsked_ThenItIsZeroAtOnce)
{
  split_counter counter (1);
  ASSERT_TRUE (counter.release ());
  EXPECT_EQ (counter.use_count_settled (), 0);
}

TEST (LumexUseCountSettledTest,
      GivenExtPaidBack_WhenAsked_ThenItReturnsWithoutWaiting)
{
  split_counter counter (2);
  counter.transfer_ext (2);
  counter.take_and_settle ();
  EXPECT_FALSE (counter.settle ());
  ASSERT_EQ (split_counter::ext_of (counter.load ()), 0);
  EXPECT_EQ (counter.use_count_settled (), 3)
      << "one unit was counted by its reader, one paid back";
}

TEST (LumexUseCountSettledTest, GivenOwnersInTransit_WhenAsked_ThenItWaits)
{
  split_counter counter (2);
  counter.transfer_ext (1);
  ASSERT_EQ (counter.use_count (), 3);
  std::atomic<bool> started (false);
  std::atomic<bool> done (false);
  std::atomic<long> result (-1);
  std::thread asker (
      [&]
        {
          started.store (true);
          result.store (counter.use_count_settled ());
          done.store (true);
        });
  while (!started.load ())
    std::this_thread::yield ();
  std::this_thread::sleep_for (std::chrono::milliseconds (100));
  EXPECT_FALSE (done.load ())
      << "ext is not zero, so use_count_settled must still wait";
  EXPECT_FALSE (counter.settle ());
  asker.join ();
  EXPECT_TRUE (done.load ());
  EXPECT_EQ (result.load (), 2)
      << "the count once nothing is in transit, not count + ext";
}

TEST (LumexUseCountSettledTest,
      GivenAWaitingAsker_WhenTheReaderCountsItself_ThenTheCountIncludesIt)
{
  split_counter counter (2);
  counter.transfer_ext (1);
  std::atomic<bool> started (false);
  std::atomic<long> result (-1);
  std::thread asker (
      [&]
        {
          started.store (true);
          result.store (counter.use_count_settled ());
        });
  while (!started.load ())
    std::this_thread::yield ();
  std::this_thread::sleep_for (std::chrono::milliseconds (50));
  EXPECT_EQ (result.load (), -1) << "still waiting";
  counter.take_and_settle ();
  asker.join ();
  EXPECT_EQ (result.load (), 3);
}

TEST (LumexUseCountSettledTest,
      GivenABlock_WhenAsked_ThenTheStrongCountIsReturned)
{
  sp::shared_ptr<int> owner (new int (1));
  ctl_base *const block = access::control (owner);
  EXPECT_EQ (block->use_count_settled (), 1);
  sp::shared_ptr<int> copy = owner;
  EXPECT_EQ (block->use_count_settled (), 2);
  block->transfer_strong_ext (1);
  std::atomic<bool> done (false);
  std::thread asker (
      [&]
        {
          EXPECT_EQ (block->use_count_settled (), 2);
          done.store (true);
        });
  std::this_thread::sleep_for (std::chrono::milliseconds (50));
  EXPECT_FALSE (done.load ()) << "the block waits while a unit is in transit";
  block->settle_strong ();
  asker.join ();
}
} // namespace
