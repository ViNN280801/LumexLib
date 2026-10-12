// Tests of shared_ptr::use_count_settled: the number of owners once no owner
// is in transit. Ordinary programs never have owners in transit, so it equals
// use_count; a thread that holds ext != 0 (as the split-count engine of an
// atomic smart pointer does while a load is in flight) makes it wait.

#include <atomic>
#include <chrono>
#include <thread>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/tests/core/smart_ptr/LumexSmartPtrTestSupport.hpp"

namespace
{
using namespace smart_ptr_test;
using sp::detail::access;

static_assert (noexcept (std::declval<sp::shared_ptr<int> const &> ()
                             .use_count_settled ()),
               "use_count_settled is noexcept");

TEST (LumexSharedPtrUseCountSettledTest,
      GivenAnEmptyPointer_WhenAsked_ThenZero)
{
  sp::shared_ptr<int> empty;
  EXPECT_EQ (empty.use_count_settled (), 0);
}

TEST (LumexSharedPtrUseCountSettledTest,
      GivenOwners_WhenCopiedAndReleased_ThenItFollowsUseCount)
{
  sp::shared_ptr<int> first (new int (1));
  EXPECT_EQ (first.use_count_settled (), 1);
  {
    sp::shared_ptr<int> second = first;
    sp::shared_ptr<int> third = second;
    EXPECT_EQ (first.use_count_settled (), 3);
    EXPECT_EQ (first.use_count_settled (), first.use_count ());
    EXPECT_EQ (third.use_count_settled (), 3);
  }
  EXPECT_EQ (first.use_count_settled (), 1);
}

TEST (LumexSharedPtrUseCountSettledTest,
      GivenAnAlias_WhenAsked_ThenTheOwnersOfTheBlock)
{
  sp::shared_ptr<std::pair<int, int>> owner (new std::pair<int, int> (1, 2));
  sp::shared_ptr<int> alias (owner, &owner->second);
  EXPECT_EQ (alias.use_count_settled (), 2);
  owner.reset ();
  EXPECT_EQ (alias.use_count_settled (), 1);
}

TEST (LumexSharedPtrUseCountSettledTest,
      GivenAnOwnerInTransit_WhenAsked_ThenItWaitsAndThenReportsTheCount)
{
  sp::shared_ptr<int> owner (new int (1));
  sp::shared_ptr<int> other = owner;
  sp::detail::ctl_base *const block = access::control (owner);
  block->transfer_strong_ext (1);
  EXPECT_EQ (owner.use_count (), 3) << "use_count counts the unit in transit";
  std::atomic<bool> started (false);
  std::atomic<bool> done (false);
  std::atomic<long> result (-1);
  std::thread asker (
      [&]
        {
          started.store (true);
          result.store (other.use_count_settled ());
          done.store (true);
        });
  while (!started.load ())
    std::this_thread::yield ();
  std::this_thread::sleep_for (std::chrono::milliseconds (100));
  EXPECT_FALSE (done.load ()) << "a unit is in transit: it must wait";
  block->settle_strong ();
  asker.join ();
  EXPECT_EQ (result.load (), 2) << "the two owners, not count + ext";
}
} // namespace
