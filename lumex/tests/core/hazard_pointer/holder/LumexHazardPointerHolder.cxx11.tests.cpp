// Tests of hazard_pointer ([saferecl.hp.holder]): construction, move, swap,
// protect, try_protect and reset_protection, with the retire/clean_up pair as
// the observer of what is protected. A retired object that a holder protects
// must survive a pass; one that nobody protects must not.

#include <atomic>
#include <cstddef>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/tests/core/hazard_pointer/LumexHazardPointerTestSupport.hpp"

namespace
{
using lumex_hp_test::counted_node;
using lumex_hp_test::counters_t;
namespace hp = lumex::core::hazard_pointer;

// Makes an object known to the test, already published in `source`.
counted_node *
publish (counters_t &counters, std::atomic<counted_node *> &source, int value)
{
  counted_node *node = new counted_node (counters, value);
  source.store (node);
  return node;
}

// Removes the object from the source and retires it.
void
remove_and_retire (std::atomic<counted_node *> &source, counted_node *node)
{
  source.store (nullptr);
  node->retire ();
}
} // namespace

// --- the interface
// -----------------------------------------------------------

TEST (LumexHazardPointerHolderTest,
      GivenTheClass_WhenCheckingTraits_ThenItMatchesTheDraft)
{
  static_assert (sizeof (hp::hazard_pointer) == sizeof (void *),
                 "a holder is one pointer");
  static_assert (
      std::is_nothrow_default_constructible<hp::hazard_pointer>::value,
      "default constructor is noexcept");
  static_assert (std::is_nothrow_move_constructible<hp::hazard_pointer>::value,
                 "move constructor is noexcept");
  static_assert (std::is_nothrow_move_assignable<hp::hazard_pointer>::value,
                 "move assignment is noexcept");
  static_assert (!std::is_copy_constructible<hp::hazard_pointer>::value,
                 "not copyable");
  static_assert (!std::is_copy_assignable<hp::hazard_pointer>::value,
                 "not copy assignable");
  static_assert (std::is_nothrow_destructible<hp::hazard_pointer>::value, "");
  static_assert (noexcept (std::declval<hp::hazard_pointer &> ().empty ()),
                 "empty is noexcept");
  static_assert (
      noexcept (std::declval<hp::hazard_pointer &> ().reset_protection ()),
      "reset_protection is noexcept");
  static_assert (
      noexcept (std::declval<hp::hazard_pointer &> ().reset_protection (
          static_cast<counted_node *> (nullptr))),
      "reset_protection (ptr) is noexcept");
  static_assert (noexcept (std::declval<hp::hazard_pointer &> ().swap (
                     std::declval<hp::hazard_pointer &> ())),
                 "swap is noexcept");
  static_assert (noexcept (swap (std::declval<hp::hazard_pointer &> (),
                                 std::declval<hp::hazard_pointer &> ())),
                 "the free swap is noexcept");
  static_assert (noexcept (std::declval<hp::hazard_pointer &> ().protect (
                     std::declval<std::atomic<counted_node *> const &> ())),
                 "protect is noexcept");
  static_assert (noexcept (std::declval<hp::hazard_pointer &> ().try_protect (
                     std::declval<counted_node *&> (),
                     std::declval<std::atomic<counted_node *> const &> ())),
                 "try_protect is noexcept");
  static_assert (std::is_same<decltype (hp::make_hazard_pointer ()),
                              hp::hazard_pointer>::value,
                 "make_hazard_pointer returns a holder");
  SUCCEED ();
}

TEST (LumexHazardPointerHolderTest,
      GivenDefaultConstruction_WhenAskingEmpty_ThenTrue)
{
  hp::hazard_pointer holder;
  EXPECT_TRUE (holder.empty ());
}

TEST (LumexHazardPointerHolderTest,
      GivenMakeHazardPointer_WhenAskingEmpty_ThenFalse)
{
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  EXPECT_FALSE (holder.empty ());
}

TEST (LumexHazardPointerHolderTest,
      GivenAMovedFromHolder_WhenAskingEmpty_ThenTrueAndTheTargetOwnsTheSlot)
{
  hp::hazard_pointer first = hp::make_hazard_pointer ();
  hp::hazard_pointer second (std::move (first));
  EXPECT_TRUE (first.empty ());
  EXPECT_FALSE (second.empty ());
}

TEST (
    LumexHazardPointerHolderTest,
    GivenAMoveAssignment_WhenAssigning_ThenTheSourceIsEmptyAndTheTargetOwnsTheSlot)
{
  hp::hazard_pointer first = hp::make_hazard_pointer ();
  hp::hazard_pointer second;
  second = std::move (first);
  EXPECT_TRUE (first.empty ());
  EXPECT_FALSE (second.empty ());
}

TEST (LumexHazardPointerHolderTest,
      GivenASelfMoveAssignment_WhenAssigning_ThenNothingChanges)
{
  counters_t counters;
  std::atomic<counted_node *> source (nullptr);
  counted_node *node = publish (counters, source, 1);
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  ASSERT_EQ (holder.protect (source), node);
  hp::hazard_pointer *alias = &holder;
  holder = std::move (*alias);
  EXPECT_FALSE (holder.empty ());
  remove_and_retire (source, node);
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 0) << "still protected";
  holder.reset_protection ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 1);
}

TEST (
    LumexHazardPointerHolderTest,
    GivenAHolderAssignedOverAProtectingOne_WhenAssigning_ThenTheOldProtectionEnds)
{
  counters_t counters;
  std::atomic<counted_node *> a_source (nullptr);
  std::atomic<counted_node *> b_source (nullptr);
  counted_node *a = publish (counters, a_source, 1);
  counted_node *b = publish (counters, b_source, 2);
  hp::hazard_pointer first = hp::make_hazard_pointer ();
  hp::hazard_pointer second = hp::make_hazard_pointer ();
  ASSERT_EQ (first.protect (a_source), a);
  ASSERT_EQ (second.protect (b_source), b);
  remove_and_retire (a_source, a);
  remove_and_retire (b_source, b);
  second = std::move (first);
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 1) << "b lost its holder, a is kept";
  EXPECT_EQ (a->value, 1);
  second.reset_protection ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 2);
}

TEST (
    LumexHazardPointerHolderTest,
    GivenHolderDestruction_WhenItProtected_ThenTheObjectIsReclaimedAfterwards)
{
  counters_t counters;
  std::atomic<counted_node *> source (nullptr);
  counted_node *node = publish (counters, source, 1);
  {
    hp::hazard_pointer holder = hp::make_hazard_pointer ();
    ASSERT_EQ (holder.protect (source), node);
    remove_and_retire (source, node);
    hp::clean_up ();
    EXPECT_EQ (counters.deleted.load (), 0);
  }
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 1);
}

// --- protect, try_protect, reset_protection ---------------------------------

TEST (LumexHazardPointerHolderTest,
      GivenASource_WhenProtecting_ThenItReturnsTheCurrentValue)
{
  counters_t counters;
  std::atomic<counted_node *> source (nullptr);
  counted_node *node = publish (counters, source, 7);
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  counted_node *got = holder.protect (source);
  EXPECT_EQ (got, node);
  EXPECT_EQ (got->value, 7);
  remove_and_retire (source, node);
  hp::clean_up ();
  holder.reset_protection ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 1);
}

TEST (LumexHazardPointerHolderTest,
      GivenANullSource_WhenProtecting_ThenItReturnsNullAndProtectsNothing)
{
  std::atomic<counted_node *> source (nullptr);
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  EXPECT_EQ (holder.protect (source), nullptr);
}

TEST (LumexHazardPointerHolderTest,
      GivenAConstSource_WhenProtecting_ThenItCompiles)
{
  counters_t counters;
  std::atomic<counted_node *> source (nullptr);
  counted_node *node = publish (counters, source, 1);
  std::atomic<counted_node *> const &view = source;
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  EXPECT_EQ (holder.protect (view), node);
  remove_and_retire (source, node);
  holder.reset_protection ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 1);
}

TEST (LumexHazardPointerHolderTest,
      GivenAConstObjectType_WhenProtecting_ThenItWorks)
{
  counters_t counters;
  counted_node *node = new counted_node (counters, 3);
  std::atomic<counted_node const *> source (node);
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  counted_node const *got = holder.protect (source);
  EXPECT_EQ (got, node);
  source.store (nullptr);
  node->retire ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 0);
  holder.reset_protection ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 1);
}

TEST (LumexHazardPointerHolderTest,
      GivenTheValueStillThere_WhenTryProtecting_ThenTrueAndProtected)
{
  counters_t counters;
  std::atomic<counted_node *> source (nullptr);
  counted_node *node = publish (counters, source, 1);
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  counted_node *ptr = node;
  EXPECT_TRUE (holder.try_protect (ptr, source));
  EXPECT_EQ (ptr, node);
  remove_and_retire (source, node);
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 0);
  holder.reset_protection ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 1);
}

TEST (
    LumexHazardPointerHolderTest,
    GivenAChangedSource_WhenTryProtecting_ThenFalsePtrUpdatedAndNothingProtected)
{
  counters_t counters;
  std::atomic<counted_node *> source (nullptr);
  counted_node *stale = publish (counters, source, 1);
  counted_node *fresh = new counted_node (counters, 2);
  source.store (fresh);
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  counted_node *ptr = stale;
  EXPECT_FALSE (holder.try_protect (ptr, source));
  EXPECT_EQ (ptr, fresh) << "ptr receives the current value";
  stale->retire ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 1)
      << "the failed attempt did not leave the stale object protected";
  remove_and_retire (source, fresh);
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 2);
}

TEST (LumexHazardPointerHolderTest,
      GivenANullPtr_WhenTryProtectingANullSource_ThenTrue)
{
  std::atomic<counted_node *> source (nullptr);
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  counted_node *ptr = nullptr;
  EXPECT_TRUE (holder.try_protect (ptr, source));
  EXPECT_EQ (ptr, nullptr);
}

TEST (
    LumexHazardPointerHolderTest,
    GivenAProtectedObject_WhenProtectingAnotherWithTheSameHolder_ThenTheFirstIsReleased)
{
  counters_t counters;
  std::atomic<counted_node *> a_source (nullptr);
  std::atomic<counted_node *> b_source (nullptr);
  counted_node *a = publish (counters, a_source, 1);
  counted_node *b = publish (counters, b_source, 2);
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  ASSERT_EQ (holder.protect (a_source), a);
  ASSERT_EQ (holder.protect (b_source), b);
  remove_and_retire (a_source, a);
  remove_and_retire (b_source, b);
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 1) << "a was released, b is kept";
  EXPECT_EQ (b->value, 2);
  holder.reset_protection ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 2);
}

TEST (LumexHazardPointerHolderTest,
      GivenResetProtectionWithAPointer_WhenReclaiming_ThenThatObjectIsKept)
{
  counters_t counters;
  std::atomic<counted_node *> source (nullptr);
  counted_node *node = publish (counters, source, 1);
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  holder.reset_protection (node);
  remove_and_retire (source, node);
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 0);
  holder.reset_protection (node);
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 0) << "the same pointer again";
  holder.reset_protection (static_cast<counted_node *> (nullptr));
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 1) << "a null pointer ends it";
}

TEST (LumexHazardPointerHolderTest,
      GivenResetProtectionWithConstPointer_WhenReclaiming_ThenThatObjectIsKept)
{
  counters_t counters;
  std::atomic<counted_node *> source (nullptr);
  counted_node *node = publish (counters, source, 1);
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  counted_node const *view = node;
  holder.reset_protection (view);
  remove_and_retire (source, node);
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 0);
  holder.reset_protection ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 1);
}

TEST (
    LumexHazardPointerHolderTest,
    GivenHandOverHandTraversal_WhenTwoHoldersAlternate_ThenEveryVisitedNodeIsSafe)
{
  // Two holders walk a three-node list; the node under the cursor is always
  // protected by one of them while the other takes the next.
  struct list_node : hp::hazard_pointer_obj_base<list_node>
  {
    std::atomic<list_node *> next;
    int value;
    explicit list_node (int v) : next (nullptr), value (v) {}
  };
  list_node *third = new list_node (3);
  list_node *second = new list_node (2);
  list_node *first = new list_node (1);
  first->next.store (second);
  second->next.store (third);
  std::atomic<list_node *> head (first);

  hp::hazard_pointer current = hp::make_hazard_pointer ();
  hp::hazard_pointer upcoming = hp::make_hazard_pointer ();
  int sum = 0;
  list_node *node = current.protect (head);
  while (node != nullptr)
    {
      sum += node->value;
      list_node *next = upcoming.protect (node->next);
      swap (current, upcoming);
      node = next;
    }
  EXPECT_EQ (sum, 6);
  current.reset_protection ();
  upcoming.reset_protection ();
  first->retire ();
  second->retire ();
  third->retire ();
  hp::clean_up ();
}

// --- swap
// ----------------------------------------------------------------------

TEST (LumexHazardPointerHolderTest,
      GivenTwoProtectingHolders_WhenSwapped_ThenBothObjectsStayProtected)
{
  counters_t counters;
  std::atomic<counted_node *> a_source (nullptr);
  std::atomic<counted_node *> b_source (nullptr);
  counted_node *a = publish (counters, a_source, 1);
  counted_node *b = publish (counters, b_source, 2);
  hp::hazard_pointer first = hp::make_hazard_pointer ();
  hp::hazard_pointer second = hp::make_hazard_pointer ();
  ASSERT_EQ (first.protect (a_source), a);
  ASSERT_EQ (second.protect (b_source), b);
  first.swap (second);
  remove_and_retire (a_source, a);
  remove_and_retire (b_source, b);
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 0) << "swap ended a protection";
  first.reset_protection ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 1) << "first now holds b";
  EXPECT_EQ (a->value, 1);
  second.reset_protection ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 2);
}

TEST (LumexHazardPointerHolderTest,
      GivenTheFreeSwap_WhenSwappingWithAnEmptyHolder_ThenTheProtectionMoves)
{
  counters_t counters;
  std::atomic<counted_node *> source (nullptr);
  counted_node *node = publish (counters, source, 1);
  hp::hazard_pointer full = hp::make_hazard_pointer ();
  hp::hazard_pointer empty_one;
  ASSERT_EQ (full.protect (source), node);
  swap (full, empty_one);
  EXPECT_TRUE (full.empty ());
  EXPECT_FALSE (empty_one.empty ());
  remove_and_retire (source, node);
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 0);
  empty_one.reset_protection ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 1);
}

// --- retired and protected
// ---------------------------------------------------

TEST (
    LumexHazardPointerHolderTest,
    GivenManyProtectedObjects_WhenCleaningUp_ThenEveryOneSurvivesAndIsReclaimedWhenItsHolderEnds)
{
  // One object per holder, enough holders for several blocks of slots: a
  // pass that skips any slot position loses an object.
  counters_t counters;
  std::size_t const count = 150;
  std::vector<std::atomic<counted_node *>> sources (count);
  std::vector<counted_node *> nodes (count);
  std::vector<hp::hazard_pointer> holders;
  for (std::size_t i = 0; i < count; ++i)
    {
      holders.push_back (hp::make_hazard_pointer ());
    }
  for (std::size_t i = 0; i < count; ++i)
    {
      nodes[i] = publish (counters, sources[i], static_cast<int> (i));
      ASSERT_EQ (holders[i].protect (sources[i]), nodes[i]);
    }
  for (std::size_t i = 0; i < count; ++i)
    {
      remove_and_retire (sources[i], nodes[i]);
    }
  hp::clean_up ();
  ASSERT_EQ (counters.deleted.load (), 0) << "all protected";
  for (std::size_t i = 0; i < count; ++i)
    {
      EXPECT_EQ (nodes[i]->value, static_cast<int> (i));
    }
  for (std::size_t i = 0; i < count; ++i)
    {
      holders[i].reset_protection ();
      hp::clean_up ();
      EXPECT_EQ (counters.deleted.load (), static_cast<int> (i + 1))
          << "object " << i << " goes with its holder";
    }
}

TEST (
    LumexHazardPointerHolderTest,
    GivenFewRetiredObjectsAndManySlots_WhenCleaningUp_ThenOnlyTheUnprotectedGo)
{
  // Batches of at most eight nodes are matched against the slots one by one.
  counters_t counters;
  std::atomic<counted_node *> kept_source (nullptr);
  std::atomic<counted_node *> gone_source (nullptr);
  counted_node *kept = publish (counters, kept_source, 1);
  counted_node *gone = publish (counters, gone_source, 2);
  std::vector<hp::hazard_pointer> filler;
  for (int i = 0; i < 100; ++i)
    {
      filler.push_back (hp::make_hazard_pointer ());
    }
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  ASSERT_EQ (holder.protect (kept_source), kept);
  remove_and_retire (kept_source, kept);
  remove_and_retire (gone_source, gone);
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 1);
  EXPECT_EQ (kept->value, 1);
  holder.reset_protection ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 2);
}
