// ABA scenarios that need C++17: shared pointers to arrays (an array element
// pointer is an aliasing pointer: same owner, other stored pointer) and
// weak_from_this () of objects whose address is reused.

#include <memory>
#include <new>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/tests/core/atomic/LumexAtomicScenarios.hpp"
#include "lumex/tests/core/atomic/LumexAtomicTestSupport.hpp"
#include "lumex/tests/support/LumexTestLedger.hpp"
#include "lumex/tests/support/LumexTestReusePool.hpp"

using namespace lumex_atomic_test;

namespace
{
struct Selfie : std::enable_shared_from_this<Selfie>
{
  explicit Selfie (lumex_test::ObjectLedger &ledger) : entry (ledger) {}
  lumex_test::LedgerEntry entry;
};

struct SelfieDeleter
{
  lumex_test::ReusePool *pool;

  void
  operator() (Selfie *p) const
  {
    p->~Selfie ();
    pool->release (p, sizeof (Selfie));
  }
};
} // namespace

TEST (
    LumexAtomicSmartPtrAbaCxx17Test,
    GivenSharedPointersToArrays_WhenElementsAndOwnersAreAliased_ThenIdentityIsPointerAndOwner)
{
  LUMEX_ATOMIC_TEST_REQUIRE_PROMPT_DESTRUCTION ();
  // Values: empty; the array x; an element of x (same owner, other pointer);
  // a second array y; the first element of x under the owner y (same pointer
  // as x, other owner).
  std::shared_ptr<int[]> const x (new int[4]);
  std::shared_ptr<int[]> const y (new int[4]);
  std::vector<std::shared_ptr<int[]>> values;
  values.push_back (std::shared_ptr<int[]> ());
  values.push_back (x);
  values.push_back (std::shared_ptr<int[]> (x, x.get () + 1));
  values.push_back (y);
  values.push_back (std::shared_ptr<int[]> (y, x.get ()));
  lumex_test::Verdict verdict;
  scenario::identity_rule_over<EngineUnderTest::shared<int[]>> (values,
                                                                verdict);
  EXPECT_TRUE (verdict.ok ()) << verdict.text ();
  EXPECT_EQ (x.use_count (), 3L)
      << "x, the element alias, and the alias held by the vector";
  values.clear ();
  EXPECT_EQ (x.use_count (), 1L);
  EXPECT_EQ (y.use_count (), 1L);
}

TEST (
    LumexAtomicSmartPtrAbaCxx17Test,
    GivenAnObjectWhoseAddressIsReused_WhenItsWeakFromThisIsCompared_ThenTheOwnerDecides)
{
  // The weak pointer from weak_from_this () of a dead object pins the
  // control block only; the object memory goes back to the pool and the next
  // object takes it. The new object's weak_from_this () shares the object
  // address and not the owner.
  lumex_test::ObjectLedger ledger (8);
  lumex_test::ReusePool object_pool;
  lumex_test::ReusePool control_pool;
  {
    SelfieDeleter const deleter = { &object_pool };
    Selfie *const first_object
        = new (object_pool.allocate (sizeof (Selfie))) Selfie (ledger);
    std::shared_ptr<Selfie> first (
        first_object, deleter,
        lumex_test::ReuseAllocator<char> (control_pool));
    std::weak_ptr<Selfie> const stale = first->weak_from_this ();
    EXPECT_FALSE (stale.expired ());
    atomic_weak_ptr<Selfie> a (stale);
    first.reset ();
    EXPECT_TRUE (stale.expired ());

    Selfie *const second_object
        = new (object_pool.allocate (sizeof (Selfie))) Selfie (ledger);
    ASSERT_EQ (second_object, first_object) << "the address must be reused";
    std::shared_ptr<Selfie> second (
        second_object, deleter,
        lumex_test::ReuseAllocator<char> (control_pool));
    std::weak_ptr<Selfie> const fresh = second->weak_from_this ();
    a.store (fresh);

    std::weak_ptr<Selfie> expected = stale;
    EXPECT_FALSE (a.compare_exchange_strong (expected, stale))
        << "the weak_from_this () of a dead object matched the new object";
    EXPECT_FALSE (expected.owner_before (fresh)
                  || fresh.owner_before (expected));
    EXPECT_TRUE (static_cast<bool> (expected.lock ()));

    a.store (stale);
    std::weak_ptr<Selfie> again = stale;
    EXPECT_TRUE (a.compare_exchange_strong (again, fresh));
  }
  EngineUnderTest::quiesce ();
  EXPECT_TRUE (ledger.balanced ()) << ledger.report ();
  EXPECT_EQ (object_pool.live (), 0L);
  EXPECT_EQ (control_pool.live (), 0L);
}
