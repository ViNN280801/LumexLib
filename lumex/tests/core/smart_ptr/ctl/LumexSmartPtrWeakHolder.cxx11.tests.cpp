// Tests of ctl_weak_holder, the block of an out-of-window alias of a weak
// pointer: creation takes one weak reference of the owner's block (never a
// strong one), disposing the holder drops it, the block keeps the memory of
// the owner but not its object, and the holder block is 8-byte aligned so
// that bit 0 of its address stays free for the holder flag of the engine.

#include <cstdint>

#include <gtest/gtest.h>

#include "lumex/tests/core/smart_ptr/LumexSmartPtrTestSupport.hpp"

namespace
{
using namespace smart_ptr_test;
using sp::detail::access;
using sp::detail::ctl_base;
using sp::detail::ctl_weak_holder;

TEST (LumexWeakHolderTest,
      GivenAnOwner_WhenCreated_ThenItTakesOneWeakReference)
{
  sp::shared_ptr<int> owner (new int (1));
  ctl_base *block = access::control (owner);
  long const weak_before = block->weak_count ();
  int pointee = 0;
  ctl_weak_holder *holder = ctl_weak_holder::create (block, &pointee);
  EXPECT_EQ (block->weak_count (), weak_before + 1)
      << "the holder took a weak reference";
  EXPECT_EQ (block->use_count (), 1)
      << "a weak reference does not change the number of owners";
  holder->release_strong ();
}

TEST (LumexWeakHolderTest, GivenAHolder_WhenAsked_ThenOwnerAndPointerAreKept)
{
  sp::shared_ptr<int> owner (new int (1));
  ctl_base *block = access::control (owner);
  int pointee = 0;
  ctl_weak_holder *holder = ctl_weak_holder::create (block, &pointee);
  EXPECT_EQ (holder->owner (), block);
  EXPECT_EQ (holder->pointer (), &pointee);
  EXPECT_EQ (holder->use_count (), 1);
  holder->release_strong ();
}

TEST (LumexWeakHolderTest,
      GivenAHolder_WhenDisposed_ThenTheWeakReferenceIsDropped)
{
  sp::shared_ptr<int> owner (new int (1));
  ctl_base *block = access::control (owner);
  int pointee = 0;
  ctl_weak_holder *holder = ctl_weak_holder::create (block, &pointee);
  ASSERT_EQ (block->weak_count (), 1);
  holder->release_strong ();
  EXPECT_EQ (block->weak_count (), 0)
      << "disposing the holder drops the weak reference of the owner";
  EXPECT_EQ (block->use_count (), 1) << "the owner itself is untouched";
}

TEST (LumexWeakHolderTest,
      GivenAHolderOfAnOwner_WhenTheObjectGoes_ThenTheBlockWaitsForTheHolder)
{
  lumex_test::ObjectLedger ledger (2);
  AllocStats stats;
  sp::shared_ptr<Probe> owner = sp::allocate_shared<Probe> (
      CountingAllocator<Probe> (&stats), ledger, 1);
  ctl_base *block = access::control (owner);
  int pointee = 0;
  ctl_weak_holder *holder = ctl_weak_holder::create (block, &pointee);
  owner.reset ();
  EXPECT_EQ (ledger.alive (), 0u) << "the object is destroyed at the reset";
  EXPECT_EQ (stats.deallocations.load (), 0)
      << "the block stays while the holder holds its weak reference";
  holder->release_strong ();
  EXPECT_EQ (stats.deallocations.load (), 1)
      << "the block is freed when the holder drops the last weak reference";
  EXPECT_TRUE (ledger.balanced ()) << ledger.report ();
}

TEST (LumexWeakHolderTest, GivenTheHolderBlock_WhenAligned_ThenBitZeroIsFree)
{
  static_assert (alignof (ctl_weak_holder) >= 2,
                 "bit 0 of the address is the holder flag");
  sp::shared_ptr<int> owner (new int (1));
  int pointee = 0;
  ctl_weak_holder *holder
      = ctl_weak_holder::create (access::control (owner), &pointee);
  std::uintptr_t const address = reinterpret_cast<std::uintptr_t> (holder);
  EXPECT_EQ (address & std::uintptr_t (1), 0u);
  EXPECT_EQ (address % alignof (ctl_weak_holder), 0u);
  holder->release_strong ();
}
} // namespace
