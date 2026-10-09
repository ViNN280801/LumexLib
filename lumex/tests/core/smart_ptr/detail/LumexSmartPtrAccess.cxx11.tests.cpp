// Tests of detail::access, the documented friend that the split-count engine
// uses: control, adopt, adopt_weak, detach, detach_weak and weak_count. They
// move counts without touching the counters, so every test ends with the
// counts the caller still owes written out.

#include <atomic>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/tests/core/smart_ptr/LumexSmartPtrTestSupport.hpp"

namespace
{
using namespace smart_ptr_test;
using sp::detail::access;
using sp::detail::ctl_base;

TEST (LumexSmartPtrAccessTest,
      GivenControl_WhenAskedForEmptyAndOwningPointers_ThenItIsNullOrTheBlock)
{
  sp::shared_ptr<int> empty;
  EXPECT_EQ (access::control (empty), nullptr);
  sp::weak_ptr<int> empty_weak;
  EXPECT_EQ (access::control (empty_weak), nullptr);
  sp::shared_ptr<int> p (new int (1));
  ctl_base *block = access::control (p);
  ASSERT_NE (block, nullptr);
  sp::weak_ptr<int> w (p);
  EXPECT_EQ (access::control (w), block);
  sp::shared_ptr<int> alias (p, new int (0));
  EXPECT_EQ (access::control (alias), block);
  delete alias.get ();
  sp::shared_ptr<int> alias_of_empty (sp::shared_ptr<int> (), p.get ());
  EXPECT_EQ (access::control (alias_of_empty), nullptr);
}

TEST (LumexSmartPtrAccessTest,
      GivenDetachAndAdopt_WhenCountsMove_ThenNothingIsCountedTwice)
{
  lumex_test::ObjectLedger ledger (2);
  sp::shared_ptr<Probe> p (new Probe (ledger, 1));
  Probe *stored = nullptr;
  ctl_base *block = access::detach (p, stored);
  EXPECT_EQ (p.use_count (), 0);
  EXPECT_EQ (p.get (), nullptr);
  ASSERT_NE (block, nullptr);
  EXPECT_EQ (stored->value, 1);
  EXPECT_EQ (block->use_count (), 1) << "the count moved out with the block";
  EXPECT_EQ (ledger.alive (), 1u);
  sp::shared_ptr<Probe> again = access::adopt<Probe> (block, stored);
  EXPECT_EQ (again.use_count (), 1);
  EXPECT_EQ (again.get (), stored);
  again.reset ();
  EXPECT_TRUE (ledger.balanced ()) << ledger.report ();
}

TEST (LumexSmartPtrAccessTest,
      GivenDetachOfAnAlias_WhenAdopted_ThenThePointerIsTheAliasOne)
{
  lumex_test::ObjectLedger ledger (2);
  sp::shared_ptr<Probe> owner (new Probe (ledger, 4));
  sp::shared_ptr<int> alias (owner, &owner->value);
  owner.reset ();
  int *stored = nullptr;
  ctl_base *block = access::detach (alias, stored);
  ASSERT_NE (block, nullptr);
  EXPECT_EQ (*stored, 4);
  sp::shared_ptr<int> back = access::adopt<int> (block, stored);
  EXPECT_EQ (back.use_count (), 1);
  EXPECT_EQ (*back, 4);
}

TEST (
    LumexSmartPtrAccessTest,
    GivenDetachOfAnEmptyPointer_WhenCalled_ThenTheBlockIsNullAndThePointerIsKept)
{
  int value = 3;
  sp::shared_ptr<int> alias_of_empty (sp::shared_ptr<int> (), &value);
  int *stored = nullptr;
  ctl_base *block = access::detach (alias_of_empty, stored);
  EXPECT_EQ (block, nullptr);
  EXPECT_EQ (stored, &value);
  EXPECT_EQ (alias_of_empty.get (), nullptr);
  sp::shared_ptr<int> rebuilt = access::adopt<int> (nullptr, stored);
  EXPECT_EQ (rebuilt.use_count (), 0);
  EXPECT_EQ (rebuilt.get (), &value);
}

TEST (
    LumexSmartPtrAccessTest,
    GivenAdoptWithAnExtraCount_WhenThePointerDies_ThenItDropsExactlyThatCount)
{
  lumex_test::ObjectLedger ledger (2);
  sp::shared_ptr<Probe> p (new Probe (ledger, 1));
  ctl_base *block = access::control (p);
  block->add_strong (); // a count the caller owns
  {
    sp::shared_ptr<Probe> adopted = access::adopt<Probe> (block, p.get ());
    EXPECT_EQ (p.use_count (), 2);
  }
  EXPECT_EQ (p.use_count (), 1);
  EXPECT_EQ (ledger.alive (), 1u);
}

TEST (LumexSmartPtrAccessTest,
      GivenWeakAdoptAndDetach_WhenCountsMove_ThenTheLedgerIsExact)
{
  sp::shared_ptr<int> p (new int (1));
  sp::weak_ptr<int> w (p);
  EXPECT_EQ (access::weak_count (p), 1);
  int *stored = nullptr;
  ctl_base *block = access::detach_weak (w, stored);
  EXPECT_EQ (w.use_count (), 0);
  EXPECT_EQ (access::weak_count (p), 1)
      << "the weak count moved with the block";
  ASSERT_NE (block, nullptr);
  sp::weak_ptr<int> again = access::adopt_weak<int> (block, stored);
  EXPECT_EQ (again.use_count (), 1);
  EXPECT_EQ (access::weak_count (p), 1);
  again.reset ();
  EXPECT_EQ (access::weak_count (p), 0);
}

TEST (LumexSmartPtrAccessTest,
      GivenWeakCount_WhenAskedForEmptyPointers_ThenZero)
{
  sp::shared_ptr<int> empty;
  sp::weak_ptr<int> empty_weak;
  EXPECT_EQ (access::weak_count (empty), 0);
  EXPECT_EQ (access::weak_count (empty_weak), 0);
}

TEST (LumexSmartPtrAccessTest,
      GivenTheInterface_WhenCheckingTraits_ThenItIsNoexcept)
{
  static_assert (noexcept (access::control (
                     std::declval<sp::shared_ptr<int> const &> ())),
                 "");
  static_assert (noexcept (access::adopt<int> (nullptr, nullptr)), "");
  static_assert (noexcept (access::adopt_weak<int> (nullptr, nullptr)), "");
  static_assert (!std::is_default_constructible<access>::value, "");
  SUCCEED ();
}
} // namespace
