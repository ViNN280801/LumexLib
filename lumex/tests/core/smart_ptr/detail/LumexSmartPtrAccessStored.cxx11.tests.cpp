// Tests of detail::access::stored, the accessor of the stored pointer of a
// weak_ptr (weak_ptr has no get ()): an empty weak pointer stores nullptr, an
// aliasing weak pointer stores the aliased pointer, and an expired weak
// pointer keeps the stored pointer it had.

#include <gtest/gtest.h>

#include "lumex/tests/core/smart_ptr/LumexSmartPtrTestSupport.hpp"

namespace
{
using namespace smart_ptr_test;
using sp::detail::access;

TEST (LumexSmartPtrAccessStoredTest, GivenAnEmptyWeak_WhenAsked_ThenNullptr)
{
  sp::weak_ptr<int> empty;
  EXPECT_EQ (access::stored (empty), nullptr);
}

TEST (LumexSmartPtrAccessStoredTest,
      GivenAnAliasWeak_WhenAsked_ThenTheAliasedPointerIsReturned)
{
  sp::shared_ptr<int> owner (new int (1));
  int other = 0;
  sp::shared_ptr<int> alias (owner, &other);
  sp::weak_ptr<int> weak (alias);
  EXPECT_EQ (access::stored (weak), &other);
  EXPECT_EQ (access::control (weak), access::control (owner));
}

TEST (LumexSmartPtrAccessStoredTest,
      GivenAnExpiredWeak_WhenAsked_ThenItKeepsThePointer)
{
  sp::shared_ptr<int> owner (new int (1));
  int *const stored = owner.get ();
  sp::weak_ptr<int> weak (owner);
  owner.reset ();
  EXPECT_TRUE (weak.expired ());
  EXPECT_EQ (access::stored (weak), stored);
}
} // namespace
