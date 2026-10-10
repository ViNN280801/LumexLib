// Tests of the family alias pair and of the lock-based engine over the
// module's own pointers (lumex::core::smart_ptr::shared_ptr / weak_ptr).
// The alias lumex::core::smart_ptr::atomic_shared_ptr (and atomic_weak_ptr)
// is the split-count engine where it exists and the lock-based engine
// otherwise; the no_split_count variant of this directory takes the
// lock-based branch. Every expectation is computed from the same macros as
// the alias, so a swapped branch fails at run time under a named test.

#include <cstddef>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/core/smart_ptr/LumexSmartPtr"

#if LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY

namespace
{
namespace sp = ::lumex::core::smart_ptr;
namespace asp = ::lumex::core::atomic::smart_ptr;

/// The object the family pointers own in these tests.
struct Node
{
  explicit Node (int x) : v (x) {}
  int v;
};

/// The lock-based engine over the family, both kinds.
typedef asp::atomic_shared_ptr_lock_based_lumex<Node> lock_based_shared;
typedef asp::atomic_weak_ptr_lock_based_lumex<Node> lock_based_weak;

#if LUMEX_ATOMIC_SMART_PTR_FAMILY_IS_SPLIT_COUNT
/// The engine the family alias must name in this build.
typedef asp::atomic_shared_ptr_lock_free_split_count<Node> family_shared;
typedef asp::atomic_weak_ptr_lock_free_split_count<Node> family_weak;
#else
/// The engine the family alias must name in this build.
typedef lock_based_shared family_shared;
typedef lock_based_weak family_weak;
#endif

TEST (LumexSplitCountFamilyAliasTest,
      GivenTheFamilyMacros_WhenSharedAliasNamed_ThenItIsTheExpectedEngine)
{
  EXPECT_TRUE (
      (std::is_same<sp::atomic_shared_ptr<Node>, family_shared>::value))
      << "the alias follows LUMEX_ATOMIC_SMART_PTR_FAMILY_IS_SPLIT_COUNT";
}

TEST (LumexSplitCountFamilyAliasTest,
      GivenTheFamilyMacros_WhenWeakAliasNamed_ThenItIsTheExpectedEngine)
{
  EXPECT_TRUE ((std::is_same<sp::atomic_weak_ptr<Node>, family_weak>::value))
      << "the alias follows LUMEX_ATOMIC_SMART_PTR_FAMILY_IS_SPLIT_COUNT";
}

#if defined(LUMEX_ATOMIC_SMART_PTR_DISABLE_SPLIT_COUNT)
TEST (LumexSplitCountFamilyAliasTest,
      GivenTheNoSplitCountBuild_WhenAliasesNamed_ThenBothAreLockBased)
{
  EXPECT_EQ (LUMEX_ATOMIC_SMART_PTR_FAMILY_IS_SPLIT_COUNT, 0);
  EXPECT_TRUE (
      (std::is_same<sp::atomic_shared_ptr<Node>, lock_based_shared>::value));
  EXPECT_TRUE (
      (std::is_same<sp::atomic_weak_ptr<Node>, lock_based_weak>::value));
}
#endif

TEST (LumexSplitCountFamilyAliasTest,
      GivenTheLockBasedFamilyEngine_WhenQueried_ThenIsLockFreeIsFalse)
{
  EXPECT_FALSE (lock_based_shared::is_always_lock_free);
  EXPECT_FALSE (lock_based_weak::is_always_lock_free);
  lock_based_shared a;
  lock_based_weak w;
  EXPECT_FALSE (a.is_lock_free ());
  EXPECT_FALSE (w.is_lock_free ());
}

#if LUMEX_ATOMIC_SMART_PTR_HAS_SPLIT_COUNT
TEST (LumexSplitCountFamilyAliasTest,
      GivenTheSplitCountFamilyEngine_WhenQueried_ThenIsLockFreeIsTrue)
{
  EXPECT_TRUE (
      asp::atomic_shared_ptr_lock_free_split_count<Node>::is_always_lock_free);
  EXPECT_TRUE (
      asp::atomic_weak_ptr_lock_free_split_count<Node>::is_always_lock_free);
}
#endif

TEST (LumexSplitCountFamilyAliasTest,
      GivenAValue_WhenLoaded_ThenTheSameObjectIsReturned)
{
  sp::shared_ptr<Node> p = sp::make_shared<Node> (7);
  lock_based_shared a (p);
  sp::shared_ptr<Node> q = a.load ();
  EXPECT_EQ (q.get (), p.get ());
  EXPECT_EQ (p.use_count (), 3) << "p, the stored owner and q";
}

TEST (LumexSplitCountFamilyAliasTest,
      GivenAValue_WhenStored_ThenTheOldOneIsReleased)
{
  sp::shared_ptr<Node> first = sp::make_shared<Node> (1);
  sp::shared_ptr<Node> second = sp::make_shared<Node> (2);
  lock_based_shared a (first);
  EXPECT_EQ (first.use_count (), 2);
  a.store (second);
  EXPECT_EQ (first.use_count (), 1) << "the replaced owner is released";
  EXPECT_EQ (second.use_count (), 2);
  EXPECT_EQ (a.load ()->v, 2);
}

TEST (LumexSplitCountFamilyAliasTest,
      GivenAValue_WhenExchanged_ThenTheOldOneIsReturned)
{
  sp::shared_ptr<Node> first = sp::make_shared<Node> (1);
  sp::shared_ptr<Node> second = sp::make_shared<Node> (2);
  lock_based_shared a (first);
  sp::shared_ptr<Node> old = a.exchange (second);
  EXPECT_EQ (old.get (), first.get ());
  EXPECT_EQ (first.use_count (), 2) << "first and old";
  EXPECT_EQ (second.use_count (), 2) << "second and the stored owner";
  EXPECT_EQ (a.load ().get (), second.get ());
}

TEST (LumexSplitCountFamilyAliasTest,
      GivenAMatchingExpected_WhenCompareExchange_ThenItSwaps)
{
  sp::shared_ptr<Node> first = sp::make_shared<Node> (1);
  sp::shared_ptr<Node> second = sp::make_shared<Node> (2);
  lock_based_shared a (first);
  sp::shared_ptr<Node> expected = first;
  EXPECT_TRUE (a.compare_exchange_strong (expected, second));
  EXPECT_EQ (a.load ().get (), second.get ());
  EXPECT_EQ (first.use_count (), 2) << "first and the expected copy";
  EXPECT_EQ (a.load ().get (), second.get ());
  EXPECT_EQ (second.use_count (), 2) << "second and the stored owner";
}

TEST (LumexSplitCountFamilyAliasTest,
      GivenAnotherExpected_WhenCompareExchange_ThenItFailsAndLoads)
{
  sp::shared_ptr<Node> first = sp::make_shared<Node> (1);
  sp::shared_ptr<Node> second = sp::make_shared<Node> (2);
  sp::shared_ptr<Node> third = sp::make_shared<Node> (3);
  lock_based_shared a (first);
  sp::shared_ptr<Node> expected = second;
  EXPECT_FALSE (a.compare_exchange_strong (expected, third));
  EXPECT_EQ (expected.get (), first.get ())
      << "expected receives the current value";
  EXPECT_EQ (a.load ().get (), first.get ());
}

TEST (LumexSplitCountFamilyAliasTest,
      GivenAliasOfTheSameBlock_WhenCompareExchange_ThenItIsNotEquivalent)
{
  Node other (2);
  sp::shared_ptr<Node> first = sp::make_shared<Node> (1);
  sp::shared_ptr<Node> alias (first, &other);
  sp::shared_ptr<Node> second = sp::make_shared<Node> (3);
  lock_based_shared a (first);
  sp::shared_ptr<Node> expected = alias;
  EXPECT_FALSE (a.compare_exchange_strong (expected, second))
      << "same block, other stored pointer: not equivalent";
  EXPECT_EQ (expected.get (), first.get ());
  EXPECT_EQ (a.load ().get (), first.get ());
}

TEST (LumexSplitCountFamilyAliasTest,
      GivenAnExpiredWeakPointer_WhenCompareExchange_ThenItIsEquivalentExactly)
{
  sp::shared_ptr<Node> owner = sp::make_shared<Node> (3);
  sp::weak_ptr<Node> w (owner);
  lock_based_weak a (w);
  owner.reset ();
  ASSERT_TRUE (w.expired ());
  sp::weak_ptr<Node> expected = w;
  sp::weak_ptr<Node> empty;
  EXPECT_TRUE (a.compare_exchange_strong (expected, empty))
      << "an expired weak pointer is compared without lock ()";
  EXPECT_TRUE (a.load ().expired ());
}

TEST (LumexSplitCountFamilyAliasTest,
      GivenAWeakAliasOfTheSameBlock_WhenCompareExchange_ThenItFails)
{
  Node other (4);
  sp::shared_ptr<Node> owner = sp::make_shared<Node> (3);
  sp::shared_ptr<Node> alias (owner, &other);
  sp::weak_ptr<Node> w (owner);
  sp::weak_ptr<Node> wa (alias);
  sp::weak_ptr<Node> next (owner);
  lock_based_weak a (w);
  sp::weak_ptr<Node> expected = wa;
  EXPECT_FALSE (a.compare_exchange_strong (expected, next))
      << "same block, other stored pointer: not equivalent";
  EXPECT_EQ (expected.lock ().get (), owner.get ());
}
} // namespace

#else

TEST (LumexSplitCountFamilyAliasTest, GivenNoFamily_WhenCompiled_ThenNoAlias)
{
  EXPECT_EQ (LUMEX_ATOMIC_SMART_PTR_HAS_LUMEX_FAMILY, 0);
}

#endif
