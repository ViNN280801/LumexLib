// Tests of weak_ptr::use_count_settled: the number of owners of the object
// once no owner is in transit, 0 when the weak pointer is empty or the
// object is gone. The waiting is shown with a thread that holds ext != 0.

#include <atomic>
#include <chrono>
#include <thread>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/tests/core/smart_ptr/LumexSmartPtrTestSupport.hpp"

namespace
{
using namespace smart_ptr_test;
using sp::detail::access;

static_assert (
    noexcept (std::declval<sp::weak_ptr<int> const &> ().use_count_settled ()),
    "use_count_settled is noexcept");

TEST (LumexWeakPtrUseCountSettledTest,
      GivenAnEmptyWeakPointer_WhenAsked_ThenZero)
{
  sp::weak_ptr<int> empty;
  EXPECT_EQ (empty.use_count_settled (), 0);
}

TEST (LumexWeakPtrUseCountSettledTest,
      GivenOwners_WhenTheyGo_ThenItFollowsUseCountToZero)
{
  sp::shared_ptr<int> first (new int (1));
  sp::weak_ptr<int> weak (first);
  EXPECT_EQ (weak.use_count_settled (), 1);
  sp::shared_ptr<int> second = first;
  EXPECT_EQ (weak.use_count_settled (), 2);
  EXPECT_EQ (weak.use_count_settled (), weak.use_count ());
  first.reset ();
  second.reset ();
  EXPECT_EQ (weak.use_count_settled (), 0) << "the object is gone";
  EXPECT_TRUE (weak.expired ());
}

TEST (LumexWeakPtrUseCountSettledTest,
      GivenAnOwnerInTransit_WhenAsked_ThenItWaitsAndThenReportsTheCount)
{
  sp::shared_ptr<int> owner (new int (1));
  sp::weak_ptr<int> weak (owner);
  sp::detail::ctl_base *const block = access::control (owner);
  block->transfer_strong_ext (1);
  EXPECT_EQ (weak.use_count (), 2);
  std::atomic<bool> started (false);
  std::atomic<bool> done (false);
  std::atomic<long> result (-1);
  std::thread asker (
      [&]
        {
          started.store (true);
          result.store (weak.use_count_settled ());
          done.store (true);
        });
  while (!started.load ())
    std::this_thread::yield ();
  std::this_thread::sleep_for (std::chrono::milliseconds (100));
  EXPECT_FALSE (done.load ()) << "a unit is in transit: it must wait";
  block->settle_strong ();
  asker.join ();
  EXPECT_EQ (result.load (), 1) << "the one owner, not count + ext";
}
} // namespace
