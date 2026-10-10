// Tests of ctl_base, the control block, through a test block that records
// dispose () and destroy (): the order and the number of the two finishing
// calls for every sequence of strong and weak operations the pointers and the
// split-count engine can produce (the implicit weak unit, ext transfers and
// settles, release without finishing a still-referenced block), the 32-byte
// layout, the anchor, the type-erased query, and ctl_holder.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/core/smart_ptr/LumexSmartPtrTestSupport.hpp"

namespace
{
using namespace smart_ptr_test;
using sp::detail::ctl_base;
using sp::detail::ctl_holder;

// A block that only records what was asked of it. It lives on the stack of
// the test: destroy () does not free anything.
class RecordingBlock : public ctl_base
{
public:
  RecordingBlock () : disposed (0), destroyed (0), order () {}

  void *
  query (void const *key) noexcept override
  {
    return key == sp::detail::type_id<int> () ? &marker : nullptr;
  }

  int disposed;
  int destroyed;
  std::string order; // "D" for dispose, "Z" for destroy
  int marker = 17;

private:
  void
  dispose () noexcept override
  {
    ++disposed;
    order += 'D';
  }

  void
  destroy () noexcept override
  {
    ++destroyed;
    order += 'Z';
  }
};

TEST (LumexCtlBaseTest,
      GivenTheLayout_WhenMeasured_ThenTheBlockIsThirtyTwoBytes)
{
  if (sizeof (void *) == 8)
    {
      EXPECT_EQ (sizeof (ctl_base), 32u);
    }
  EXPECT_EQ (alignof (ctl_base), alignof (std::uint64_t));
  static_assert (std::is_polymorphic<ctl_base>::value, "");
  static_assert (std::is_abstract<ctl_base>::value, "");
  static_assert (!std::is_copy_constructible<ctl_base>::value, "");
  static_assert (!std::is_copy_assignable<ctl_base>::value, "");
  static_assert (sizeof (sp::detail::split_counter) == 8, "");
}

TEST (LumexCtlBaseTest,
      GivenTheBlocksOfTheModule_WhenMeasured_ThenTheyAddOnlyTheirParts)
{
  if (sizeof (void *) != 8)
    GTEST_SKIP () << "the sizes are checked on a 64-bit target";
  typedef sp::detail::block_allocator<void> default_alloc;
  // Pointer block with an empty deleter: header + pointer.
  EXPECT_EQ (
      (sizeof (sp::detail::ctl_ptr<int *, sp::detail::default_deleter<int>,
                                   default_alloc>)),
      40u);
  // Pointer block with a state-less lambda-like deleter and a pointer deleter.
  EXPECT_EQ (
      (sizeof (
          sp::detail::ctl_ptr<int *, CountingDeleter<int>, default_alloc>)),
      48u);
  // make_shared<int>: header + the int, padded.
  EXPECT_EQ (
      (sizeof (
          sp::detail::ctl_inplace<int, sp::detail::block_allocator<int>>)),
      40u);
  EXPECT_EQ ((sizeof (sp::detail::ctl_holder)), 48u);
}

TEST (LumexCtlBaseTest,
      GivenTheLastStrongOwner_WhenReleased_ThenDisposeThenDestroy)
{
  RecordingBlock block;
  EXPECT_EQ (block.use_count (), 1);
  EXPECT_EQ (block.weak_count (), 0);
  block.release_strong ();
  EXPECT_EQ (block.order, "DZ") << "dispose, then the implicit weak unit goes "
                                   "and the block is destroyed";
  EXPECT_EQ (block.use_count (), 0);
}

TEST (LumexCtlBaseTest,
      GivenSeveralOwners_WhenReleased_ThenDisposeRunsAtTheLast)
{
  RecordingBlock block;
  block.add_strong ();
  block.add_strong ();
  block.release_strong ();
  block.release_strong ();
  EXPECT_EQ (block.order, "");
  EXPECT_EQ (block.use_count (), 1);
  block.release_strong ();
  EXPECT_EQ (block.order, "DZ");
}

TEST (LumexCtlBaseTest,
      GivenAWeakPointer_WhenTheOwnersGo_ThenTheBlockOutlivesTheObject)
{
  RecordingBlock block;
  block.add_weak ();
  block.add_weak ();
  EXPECT_EQ (block.weak_count (), 2);
  block.release_strong ();
  EXPECT_EQ (block.order, "D");
  EXPECT_EQ (block.use_count (), 0);
  EXPECT_EQ (block.weak_count (), 2)
      << "after the object is gone the count is the plain one";
  block.release_weak ();
  EXPECT_EQ (block.order, "D");
  block.release_weak ();
  EXPECT_EQ (block.order, "DZ");
}

TEST (LumexCtlBaseTest,
      GivenAWeakPointerReleasedFirst_WhenOwnersRemain_ThenNothingFinishes)
{
  RecordingBlock block;
  block.add_weak ();
  block.release_weak ();
  EXPECT_EQ (block.order, "");
  EXPECT_EQ (block.weak_count (), 0);
  block.release_strong ();
  EXPECT_EQ (block.order, "DZ");
}

TEST (LumexCtlBaseTest,
      GivenPromotion_WhenTheObjectIsAliveOrGone_ThenOnlyAliveOnesPromote)
{
  RecordingBlock block;
  block.add_weak ();
  EXPECT_TRUE (block.try_add_strong ());
  EXPECT_EQ (block.use_count (), 2);
  block.release_strong ();
  block.release_strong ();
  EXPECT_EQ (block.order, "D");
  EXPECT_FALSE (block.try_add_strong ())
      << "no promotion after the object is gone";
  EXPECT_EQ (block.use_count (), 0);
  block.release_weak ();
  EXPECT_EQ (block.order, "DZ");
}

TEST (
    LumexCtlBaseTest,
    GivenTheEngineProtocol_WhenTheWriterTransfersThenDrops_ThenTheReadersFinishTheBlock)
{
  // The slot count is dropped after the ticks moved to ext; the object lives
  // until the last reader settles.
  RecordingBlock block;
  block.transfer_strong_ext (2);
  block.release_strong (); // the writer drops the slot count
  EXPECT_EQ (block.order, "") << "two readers still owe a settle";
  EXPECT_EQ (block.use_count (), 2)
      << "two owners in transit: count 0 plus ext 2";
  block.settle_strong ();
  EXPECT_EQ (block.order, "");
  block.settle_strong ();
  EXPECT_EQ (block.order, "DZ");
}

TEST (
    LumexCtlBaseTest,
    GivenASettleBeforeTheTransfer_WhenTheWriterIsLate_ThenTheBlockLivesThroughTheCount)
{
  RecordingBlock block;
  block.settle_strong (); // ext = -1, the writer still owns the slot count
  EXPECT_EQ (block.order, "");
  block.transfer_strong_ext (1);
  EXPECT_EQ (block.order, "");
  block.release_strong ();
  EXPECT_EQ (block.order, "DZ");
}

TEST (
    LumexCtlBaseTest,
    GivenAReaderThatCountedItself_WhenTheWriterDrops_ThenTheReaderKeepsTheObject)
{
  // The reader took its own count while pinned, then settled the tick.
  RecordingBlock block;
  block.transfer_strong_ext (1);
  block.add_strong ();     // the reader's own reference
  block.release_strong (); // the writer drops the slot count
  block.settle_strong ();  // the reader pays the tick back
  EXPECT_EQ (block.order, "");
  EXPECT_EQ (block.use_count (), 1);
  block.release_strong ();
  EXPECT_EQ (block.order, "DZ");
}

TEST (
    LumexCtlBaseTest,
    GivenTheWeakLedger_WhenExtIsTransferredAndSettled_ThenTheBlockIsDestroyedAtZero)
{
  RecordingBlock block;
  block.transfer_weak_ext (2);
  block.release_strong ();
  EXPECT_EQ (block.order, "D")
      << "the strong group ended; two weak debts keep the block";
  block.settle_weak ();
  EXPECT_EQ (block.order, "D");
  block.settle_weak ();
  EXPECT_EQ (block.order, "DZ");
}

TEST (LumexCtlBaseTest,
      GivenTheCounters_WhenAccessed_ThenTheyAreTheSamePackedWords)
{
  RecordingBlock block;
  block.transfer_strong_ext (3);
  EXPECT_EQ (
      sp::detail::split_counter::ext_of (block.strong_counter ().load ()), 3);
  EXPECT_EQ (
      sp::detail::split_counter::count_of (block.weak_counter ().load ()), 1)
      << "the implicit weak unit of the strong group";
  ctl_base const &view = block;
  EXPECT_EQ (view.strong_counter ().load (), block.strong_counter ().load ());
  EXPECT_EQ (view.weak_counter ().load (), block.weak_counter ().load ());
  // Finish the block so that the record is tidy.
  block.release_strong ();
  block.settle_strong ();
  block.settle_strong ();
  block.settle_strong ();
}

TEST (LumexCtlBaseTest, GivenTheAnchor_WhenSet_ThenItIsReturned)
{
  RecordingBlock block;
  EXPECT_EQ (block.anchor (), 0u);
  block.set_anchor (0x1234u);
  EXPECT_EQ (block.anchor (), 0x1234u);
  block.release_strong ();
}

TEST (LumexCtlBaseTest,
      GivenTheQuery_WhenAskedForAKey_ThenTheBlockAnswersItsOwn)
{
  RecordingBlock block;
  EXPECT_EQ (block.query (sp::detail::type_id<int> ()), &block.marker);
  EXPECT_EQ (block.query (sp::detail::type_id<double> ()), nullptr);
  block.release_strong ();
}

TEST (LumexCtlBaseTest,
      GivenAHolder_WhenCreated_ThenItOwnsOneReferenceOfTheOwner)
{
  RecordingBlock owner;
  int pointee = 0;
  ctl_holder *holder = ctl_holder::create (&owner, &pointee);
  EXPECT_EQ (owner.use_count (), 2) << "the holder took a strong reference";
  EXPECT_EQ (holder->owner (), &owner);
  EXPECT_EQ (holder->pointer (), &pointee);
  EXPECT_EQ (holder->use_count (), 1);
  holder->release_strong ();
  EXPECT_EQ (owner.use_count (), 1)
      << "disposing the holder drops the owner reference";
  EXPECT_EQ (owner.order, "");
  owner.release_strong ();
  EXPECT_EQ (owner.order, "DZ");
}

TEST (LumexCtlBaseTest,
      GivenAHolderWithWeakReaders_WhenOwnersGo_ThenTheOwnerIsReleasedFirst)
{
  RecordingBlock owner;
  int pointee = 0;
  ctl_holder *holder = ctl_holder::create (&owner, &pointee);
  owner.release_strong (); // the original owner goes; the holder keeps it
  EXPECT_EQ (owner.order, "");
  holder->add_weak ();
  holder->release_strong ();
  EXPECT_EQ (owner.order, "DZ")
      << "the holder's disposal released the last owner reference";
  holder->release_weak (); // frees the holder block
}

TEST (
    LumexCtlBaseTest,
    GivenRealBlocks_WhenSharedPointersWork_ThenTheyFinishThroughThisInterface)
{
  lumex_test::ObjectLedger ledger (2);
  sp::shared_ptr<Probe> p (new Probe (ledger, 1));
  ctl_base *block = sp::detail::access::control (p);
  ASSERT_NE (block, nullptr);
  EXPECT_EQ (block->use_count (), 1);
  EXPECT_EQ (block->anchor (), reinterpret_cast<std::uintptr_t> (p.get ()))
      << "the anchor is the first stored pointer";
  sp::shared_ptr<Probe> q = p;
  EXPECT_EQ (block->use_count (), 2);
  // Pin the block the way the engine does: ext keeps the object alive after
  // every shared_ptr is gone.
  block->transfer_strong_ext (1);
  p.reset ();
  q.reset ();
  EXPECT_EQ (ledger.alive (), 1u) << "a pending ext debt keeps the object";
  EXPECT_EQ (block->use_count (), 1);
  block->settle_strong ();
  EXPECT_TRUE (ledger.balanced ()) << ledger.report ();
}
} // namespace
