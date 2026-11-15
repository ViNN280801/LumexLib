// The order of the constructions, destructions and assignments of expected,
// as [expected.object.assign] (reinit-expected), [expected.object.swap]
// (Table 72) and [expected.void.assign] / [expected.void.swap] write them: a
// probe type logs every operation, so a test compares the whole sequence of
// the standard with the one that happened, and whether the object kept its old
// contents when a construction threw. The tests compile from C++11, so every
// expected suite runs them.

#include <cstdlib>
#include <exception>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"
#include "lumex/tests/core/expected/ExpectedProbe.hpp"

namespace
{
using lumex::core::expected::error::unexpected;
using lumex::core::expected::result::expected;
using lumex::core::expected::result::in_place;
using lumex::core::expected::result::unexpect;

using namespace expected_probe;

class ExpectedReinitTest : public ::testing::Test
{
protected:
  void
  SetUp () override
  {
    the_log ().clear ();
    fail_on ().clear ();
  }

  void
  TearDown () override
  {
    fail_on ().clear ();
  }

  /// The log since the last call, as one line; the log is emptied.
  static std::string
  taken ()
  {
    std::string const text = joined (the_log ());
    the_log ().clear ();
    return text;
  }
};

constexpr int kOld = 1;
constexpr int kNew = 2;

// === the copy assignment, [expected.object.assign]/2 ======================

TEST_F (ExpectedReinitTest, CopyAssign_BothValues_AssignsTheValue)
{
  expected<v_nothrow_t, e_nothrow_t> uut (in_place, kOld);
  expected<v_nothrow_t, e_nothrow_t> const other (in_place, kNew);
  taken ();

  uut = other;

  EXPECT_EQ (taken (), "V:copy-assign(2)");
  ASSERT_TRUE (uut.has_value ());
  EXPECT_EQ (uut->id, kNew);
}

TEST_F (ExpectedReinitTest, CopyAssign_BothErrors_AssignsTheError)
{
  expected<v_nothrow_t, e_nothrow_t> uut (unexpect, kOld);
  expected<v_nothrow_t, e_nothrow_t> const other (unexpect, kNew);
  taken ();

  uut = other;

  EXPECT_EQ (taken (), "E:copy-assign(2)");
  ASSERT_FALSE (uut.has_value ());
  EXPECT_EQ (uut.error ().id, kNew);
}

TEST_F (ExpectedReinitTest,
        CopyAssign_ValueFromError_ConstructsDirectlyWhenTheCopyCannotThrow)
{
  expected<v_nothrow_t, e_nothrow_t> uut (in_place, kOld);
  expected<v_nothrow_t, e_nothrow_t> const other (unexpect, kNew);
  taken ();

  uut = other;

  // Branch 1 of reinit-expected: destroy the old object, build the new one.
  EXPECT_EQ (taken (), "V:dtor(1) E:copy-ctor(2)");
  ASSERT_FALSE (uut.has_value ());
  EXPECT_EQ (uut.error ().id, kNew);
}

TEST_F (ExpectedReinitTest,
        CopyAssign_ValueFromError_BuildsATemporaryWhenOnlyTheMoveCannotThrow)
{
  expected<v_nothrow_t, e_copy_throws_t> uut (in_place, kOld);
  expected<v_nothrow_t, e_copy_throws_t> const other (unexpect, kNew);
  taken ();

  uut = other;

  // Branch 2: the copy may throw, so it goes into a temporary first, and the
  // temporary (whose move cannot throw) into the place of the old object.
  EXPECT_EQ (taken (), "E:copy-ctor(2) V:dtor(1) E:move-ctor(2) E:dtor(-1)");
  ASSERT_FALSE (uut.has_value ());
  EXPECT_EQ (uut.error ().id, kNew);
}

TEST_F (ExpectedReinitTest,
        CopyAssign_ValueFromError_KeepsTheOldValueWhenTheTemporaryThrows)
{
  expected<v_nothrow_t, e_copy_throws_t> uut (in_place, kOld);
  expected<v_nothrow_t, e_copy_throws_t> const other (unexpect, kNew);
  taken ();
  fail_on () = "E:copy-ctor";

  EXPECT_THROW (uut = other, injected_failure);

  EXPECT_EQ (taken (), "E:copy-ctor!");
  ASSERT_TRUE (uut.has_value ());
  EXPECT_EQ (uut->id, kOld) << "the value was never touched";
}

TEST_F (ExpectedReinitTest,
        CopyAssign_ValueFromError_SavesTheOldValueWhenNothingIsNothrow)
{
  expected<v_nothrow_t, e_all_throw_t> uut (in_place, kOld);
  expected<v_nothrow_t, e_all_throw_t> const other (unexpect, kNew);
  taken ();

  uut = other;

  // Branch 3: neither constructor of the new object is safe, so the old
  // object goes into a temporary, is destroyed, and the new one is built.
  EXPECT_EQ (taken (), "V:move-ctor(1) V:dtor(-1) E:copy-ctor(2) V:dtor(1)");
  ASSERT_FALSE (uut.has_value ());
  EXPECT_EQ (uut.error ().id, kNew);
}

TEST_F (ExpectedReinitTest,
        CopyAssign_ValueFromError_PutsTheOldValueBackWhenTheNewErrorThrows)
{
  expected<v_nothrow_t, e_all_throw_t> uut (in_place, kOld);
  expected<v_nothrow_t, e_all_throw_t> const other (unexpect, kNew);
  taken ();
  fail_on () = "E:copy-ctor";

  EXPECT_THROW (uut = other, injected_failure);

  EXPECT_EQ (
      taken (),
      "V:move-ctor(1) V:dtor(-1) E:copy-ctor! V:move-ctor(1) V:dtor(-1)");
  ASSERT_TRUE (uut.has_value ()) << "the strong guarantee";
  EXPECT_EQ (uut->id, kOld);
}

TEST_F (ExpectedReinitTest, CopyAssign_ErrorFromValue_IsTheMirrorImage)
{
  expected<v_copy_throws_t, e_nothrow_t> uut (unexpect, kOld);
  expected<v_copy_throws_t, e_nothrow_t> const other (in_place, kNew);
  taken ();

  uut = other;

  // The value is built (copy may throw), so branch 2 with V as the new object.
  EXPECT_EQ (taken (), "V:copy-ctor(2) E:dtor(1) V:move-ctor(2) V:dtor(-1)");
  ASSERT_TRUE (uut.has_value ());
  EXPECT_EQ (uut->id, kNew);
}

TEST_F (ExpectedReinitTest,
        CopyAssign_ErrorFromValue_PutsTheOldErrorBackWhenTheNewValueThrows)
{
  expected<v_all_throw_t, e_nothrow_t> uut (unexpect, kOld);
  expected<v_all_throw_t, e_nothrow_t> const other (in_place, kNew);
  taken ();
  fail_on () = "V:copy-ctor";

  EXPECT_THROW (uut = other, injected_failure);

  EXPECT_EQ (
      taken (),
      "E:move-ctor(1) E:dtor(-1) V:copy-ctor! E:move-ctor(1) E:dtor(-1)");
  ASSERT_FALSE (uut.has_value ());
  EXPECT_EQ (uut.error ().id, kOld);
}

// === the move assignment, [expected.object.assign]/7 ======================

TEST_F (ExpectedReinitTest, MoveAssign_BothValues_MoveAssignsTheValue)
{
  expected<v_nothrow_t, e_nothrow_t> uut (in_place, kOld);
  expected<v_nothrow_t, e_nothrow_t> other (in_place, kNew);
  taken ();

  uut = std::move (other);

  EXPECT_EQ (taken (), "V:move-assign(2)");
  EXPECT_EQ (uut->id, kNew);
  EXPECT_TRUE (other.has_value ())
      << "has_value () of the source is unchanged";
}

TEST_F (ExpectedReinitTest, MoveAssign_ErrorFromValue_ConstructsByMove)
{
  expected<v_nothrow_t, e_nothrow_t> uut (unexpect, kOld);
  expected<v_nothrow_t, e_nothrow_t> other (in_place, kNew);
  taken ();

  uut = std::move (other);

  EXPECT_EQ (taken (), "E:dtor(1) V:move-ctor(2)");
  ASSERT_TRUE (uut.has_value ());
  EXPECT_EQ (uut->id, kNew);
}

TEST_F (ExpectedReinitTest, MoveAssign_ValueFromError_ConstructsByMove)
{
  expected<v_nothrow_t, e_nothrow_t> uut (in_place, kOld);
  expected<v_nothrow_t, e_nothrow_t> other (unexpect, kNew);
  taken ();

  uut = std::move (other);

  EXPECT_EQ (taken (), "V:dtor(1) E:move-ctor(2)");
  ASSERT_FALSE (uut.has_value ());
  EXPECT_EQ (uut.error ().id, kNew);
  EXPECT_FALSE (other.has_value ());
}

// === assignment from a value and from an unexpected ========================

TEST_F (ExpectedReinitTest, AssignValue_ToAValue_AssignsIt)
{
  expected<v_nothrow_t, e_nothrow_t> uut (in_place, kOld);
  v_nothrow_t source (kNew);
  taken ();

  uut = source;

  EXPECT_EQ (taken (), "V:copy-assign(2)");
}

TEST_F (ExpectedReinitTest, AssignValue_ToAnError_ReplacesItByReinit)
{
  expected<v_nothrow_t, e_nothrow_t> uut (unexpect, kOld);
  v_nothrow_t source (kNew);
  taken ();

  uut = source;

  EXPECT_EQ (taken (), "E:dtor(1) V:copy-ctor(2)");
  ASSERT_TRUE (uut.has_value ());
  EXPECT_EQ (uut->id, kNew);
}

TEST_F (ExpectedReinitTest, AssignValue_ToAnError_KeepsTheErrorWhenItThrows)
{
  expected<v_all_throw_t, e_nothrow_t> uut (unexpect, kOld);
  v_all_throw_t source (kNew);
  taken ();
  fail_on () = "V:copy-ctor";

  EXPECT_THROW (uut = source, injected_failure);

  EXPECT_FALSE (uut.has_value ());
  EXPECT_EQ (uut.error ().id, kOld);
}

TEST_F (ExpectedReinitTest, AssignUnexpected_ToAValue_ReplacesItByReinit)
{
  expected<v_nothrow_t, e_nothrow_t> uut (in_place, kOld);
  unexpected<e_nothrow_t> const source (in_place, kNew);
  taken ();

  uut = source;

  EXPECT_EQ (taken (), "V:dtor(1) E:copy-ctor(2)");
  ASSERT_FALSE (uut.has_value ());
}

TEST_F (ExpectedReinitTest, AssignUnexpected_ToAnError_AssignsIt)
{
  expected<v_nothrow_t, e_nothrow_t> uut (unexpect, kOld);
  unexpected<e_nothrow_t> source (in_place, kNew);
  taken ();

  uut = std::move (source);

  EXPECT_EQ (taken (), "E:move-assign(2)");
}

TEST_F (ExpectedReinitTest,
        AssignUnexpected_ToAValue_KeepsTheValueWhenItThrows)
{
  expected<v_nothrow_t, e_all_throw_t> uut (in_place, kOld);
  unexpected<e_all_throw_t> const source (in_place, kNew);
  taken ();
  fail_on () = "E:copy-ctor";

  EXPECT_THROW (uut = source, injected_failure);

  ASSERT_TRUE (uut.has_value ());
  EXPECT_EQ (uut->id, kOld);
}

// === emplace, [expected.object.assign]/19 ==================================

TEST_F (ExpectedReinitTest, Emplace_OverAValue_DestroysItAndBuildsTheNewOne)
{
  expected<v_nothrow_t, e_nothrow_t> uut (in_place, kOld);
  taken ();

  v_nothrow_t &built = uut.emplace (kNew);

  EXPECT_EQ (taken (), "V:dtor(1) V:ctor(2)");
  EXPECT_EQ (&built, &*uut) << "the reference is to the new value";
  EXPECT_EQ (built.id, kNew);
}

TEST_F (ExpectedReinitTest, Emplace_OverAnError_DestroysItAndBuildsTheValue)
{
  expected<v_nothrow_t, e_nothrow_t> uut (unexpect, kOld);
  taken ();

  uut.emplace (v_nothrow_t (kNew));

  EXPECT_EQ (taken (), "V:ctor(2) E:dtor(1) V:move-ctor(2) V:dtor(-1)");
  ASSERT_TRUE (uut.has_value ());
  EXPECT_EQ (uut->id, kNew);
}

TEST_F (ExpectedReinitTest, EmplaceError_OverAValue_IsReinitOfTheError)
{
  expected<v_all_throw_t, e_all_throw_t> uut (in_place, kOld);
  taken ();
  fail_on () = "E:ctor";

  EXPECT_THROW (uut.emplace_error (kNew), injected_failure);

  ASSERT_TRUE (uut.has_value ()) << "the old value is put back";
  EXPECT_EQ (uut->id, kOld);
}

// === swap, [expected.object.swap] Table 72 =================================

TEST_F (ExpectedReinitTest, Swap_TwoValues_SwapsThem)
{
  expected<v_nothrow_t, e_nothrow_t> a (in_place, kOld);
  expected<v_nothrow_t, e_nothrow_t> b (in_place, kNew);
  taken ();

  a.swap (b);

  EXPECT_EQ (taken (), "V:swap(1,2)");
  EXPECT_EQ (a->id, kNew);
  EXPECT_EQ (b->id, kOld);
}

TEST_F (ExpectedReinitTest, Swap_TwoErrors_SwapsThem)
{
  expected<v_nothrow_t, e_nothrow_t> a (unexpect, kOld);
  expected<v_nothrow_t, e_nothrow_t> b (unexpect, kNew);
  taken ();

  a.swap (b);

  EXPECT_EQ (taken (), "E:swap(1,2)");
}

TEST_F (ExpectedReinitTest, Swap_ValueWithError_ParksTheErrorWhenItIsNothrow)
{
  expected<v_nothrow_t, e_nothrow_t> a (in_place, kOld);
  expected<v_nothrow_t, e_nothrow_t> b (unexpect, kNew);
  taken ();

  a.swap (b);

  // E is nothrow move constructible: the error is parked, the value moved to
  // the other object, the error moved back.
  EXPECT_EQ (taken (), "E:move-ctor(2) E:dtor(-1) V:move-ctor(1) V:dtor(-1) "
                       "E:move-ctor(2) E:dtor(-1)");
  EXPECT_FALSE (a.has_value ());
  EXPECT_EQ (a.error ().id, kNew);
  ASSERT_TRUE (b.has_value ());
  EXPECT_EQ (b->id, kOld);
}

TEST_F (ExpectedReinitTest, Swap_ErrorWithValue_CallsTheOtherWay)
{
  expected<v_nothrow_t, e_nothrow_t> a (unexpect, kOld);
  expected<v_nothrow_t, e_nothrow_t> b (in_place, kNew);
  taken ();

  a.swap (b);

  ASSERT_TRUE (a.has_value ());
  EXPECT_EQ (a->id, kNew);
  EXPECT_FALSE (b.has_value ());
  EXPECT_EQ (b.error ().id, kOld);
}

TEST_F (ExpectedReinitTest,
        Swap_ValueWithError_ParksTheValueWhenTheErrorMayThrow)
{
  expected<v_nothrow_t, e_all_throw_t> a (in_place, kOld);
  expected<v_nothrow_t, e_all_throw_t> b (unexpect, kNew);
  taken ();

  a.swap (b);

  // E may throw when moved: the value is parked instead.
  EXPECT_EQ (taken (), "V:move-ctor(1) V:dtor(-1) E:move-ctor(2) E:dtor(-1) "
                       "V:move-ctor(1) V:dtor(-1)");
  EXPECT_FALSE (a.has_value ());
  EXPECT_EQ (a.error ().id, kNew);
  ASSERT_TRUE (b.has_value ());
  EXPECT_EQ (b->id, kOld);
}

TEST_F (ExpectedReinitTest,
        Swap_ValueWithError_RestoresBothWhenTheValueMoveThrows)
{
  expected<v_all_throw_t, e_nothrow_t> a (in_place, kOld);
  expected<v_all_throw_t, e_nothrow_t> b (unexpect, kNew);
  taken ();
  fail_on () = "V:move-ctor";

  EXPECT_THROW (a.swap (b), injected_failure);

  ASSERT_TRUE (a.has_value ());
  EXPECT_EQ (a->id, kOld);
  ASSERT_FALSE (b.has_value ()) << "the error that was parked is back";
  EXPECT_EQ (b.error ().id, kNew);
}

TEST_F (ExpectedReinitTest,
        Swap_ValueWithError_RestoresBothWhenTheErrorMoveThrows)
{
  expected<v_nothrow_t, e_all_throw_t> a (in_place, kOld);
  expected<v_nothrow_t, e_all_throw_t> b (unexpect, kNew);
  taken ();
  fail_on () = "E:move-ctor";

  EXPECT_THROW (a.swap (b), injected_failure);

  ASSERT_TRUE (a.has_value ()) << "the value that was parked is back";
  EXPECT_EQ (a->id, kOld);
  ASSERT_FALSE (b.has_value ());
  EXPECT_EQ (b.error ().id, kNew);
}

// === expected<void, E> =====================================================

TEST_F (ExpectedReinitTest, Void_AssignErrorToSuccess_ConstructsTheErrorOnce)
{
  expected<void, e_copy_throws_t> uut;
  expected<void, e_copy_throws_t> const other (unexpect, kNew);
  taken ();

  uut = other;

  // [expected.void.assign]/1.2: construct_at (unex, rhs.unex), no temporary.
  EXPECT_EQ (taken (), "E:copy-ctor(2)");
  ASSERT_FALSE (uut.has_value ());
}

TEST_F (ExpectedReinitTest,
        Void_AssignErrorToSuccess_KeepsTheSuccessWhenItThrows)
{
  expected<void, e_copy_throws_t> uut;
  expected<void, e_copy_throws_t> const other (unexpect, kNew);
  taken ();
  fail_on () = "E:copy-ctor";

  EXPECT_THROW (uut = other, injected_failure);

  EXPECT_TRUE (uut.has_value ());
}

TEST_F (ExpectedReinitTest, Void_AssignSuccessToError_DestroysTheError)
{
  expected<void, e_nothrow_t> uut (unexpect, kOld);
  expected<void, e_nothrow_t> const other;
  taken ();

  uut = other;

  EXPECT_EQ (taken (), "E:dtor(1)");
  EXPECT_TRUE (uut.has_value ());
}

TEST_F (ExpectedReinitTest, Void_Swap_SuccessWithError_MovesTheErrorOnce)
{
  expected<void, e_copy_throws_t> a;
  expected<void, e_copy_throws_t> b (unexpect, kNew);
  taken ();

  a.swap (b);

  // [expected.void.swap]: construct_at (unex, move (rhs.unex)); destroy.
  EXPECT_EQ (taken (), "E:move-ctor(2) E:dtor(-1)");
  EXPECT_FALSE (a.has_value ());
  EXPECT_TRUE (b.has_value ());
}

TEST_F (ExpectedReinitTest, Void_Emplace_DestroysTheError)
{
  expected<void, e_nothrow_t> uut (unexpect, kOld);
  taken ();

  uut.emplace ();

  EXPECT_EQ (taken (), "E:dtor(1)");
  EXPECT_TRUE (uut.has_value ());
}

// === destruction ============================================================

TEST_F (ExpectedReinitTest, Destructor_DestroysOnlyTheActiveAlternative)
{
  {
    expected<v_nothrow_t, e_nothrow_t> holding_value (in_place, kOld);
    expected<v_nothrow_t, e_nothrow_t> holding_error (unexpect, kNew);
    taken ();
  }

  EXPECT_EQ (taken (), "E:dtor(2) V:dtor(1)");
}

// === a restore that throws too =============================================
// The assignments and swap put the old contents back with a move that cannot
// throw (their constraints see to it). emplace_error is constrained by
// is_constructible alone: when the construction of the new error throws and
// the move that puts the old contents back throws as well, the destructor of
// the guard (restore_guard, noexcept) calls std::terminate. Before the guard
// the second exception was caught and one object was destroyed twice
// (undefined behavior). The tests pin the terminate: the child process
// installs a handler that ends it with exit code 42, so a program that is
// killed some other way, or one that lets the exception out, does not pass.

/// What the next constructions do: the first `succeed` of them work, the next
/// `fail` throw, the rest work again.
struct plan_t
{
  int succeed;
  int fail;
};

plan_t &
plan ()
{
  static plan_t current = { 0, 0 };
  return current;
}

void
spend ()
{
  if (plan ().succeed > 0)
    {
      --plan ().succeed;
      return;
    }
  if (plan ().fail > 0)
    {
      --plan ().fail;
      throw injected_failure ();
    }
}

/// Every constructor may throw, as `plan ()` says.
template <char Name> struct fragile_t
{
  int id;

  explicit fragile_t (int v) noexcept (false) : id (v) { spend (); }
  fragile_t (fragile_t const &other) noexcept (false) : id (other.id)
  {
    spend ();
  }
  fragile_t (fragile_t &&other) noexcept (false) : id (other.id) { spend (); }
};

using fragile_value_t = fragile_t<'V'>;
using fragile_error_t = fragile_t<'E'>;

constexpr int kExitOnTerminate = 42;

[[noreturn]] void
exit_on_terminate ()
{
  std::_Exit (kExitOnTerminate);
}

// The old contents are moved aside (the first construction), the new error
// throws (the second), the old contents are moved back (the third) and throw
// as well: the plan is one success and two failures.
void
emplace_error_over_an_error ()
{
  std::set_terminate (exit_on_terminate);
  expected<int, fragile_error_t> uut (unexpect, kOld);
  plan () = plan_t{ 1, 2 };
  uut.emplace_error (kNew);
}

void
emplace_error_over_a_value ()
{
  std::set_terminate (exit_on_terminate);
  expected<fragile_value_t, fragile_error_t> uut (in_place, kOld);
  plan () = plan_t{ 1, 2 };
  uut.emplace_error (kNew);
}

void
void_emplace_error_over_an_error ()
{
  std::set_terminate (exit_on_terminate);
  expected<void, fragile_error_t> uut (unexpect, kOld);
  plan () = plan_t{ 1, 2 };
  uut.emplace_error (kNew);
}

TEST (ExpectedReinitDeathTest,
      EmplaceError_OverAnError_WhenTheRestoreThrowsToo_Terminates)
{
#if GTEST_HAS_DEATH_TEST
  GTEST_FLAG_SET (death_test_style, "threadsafe");
  EXPECT_EXIT (emplace_error_over_an_error (),
               ::testing::ExitedWithCode (kExitOnTerminate), "");
#else
  GTEST_SKIP () << "death tests are not supported on this platform";
#endif
}

TEST (ExpectedReinitDeathTest,
      EmplaceError_OverAValue_WhenTheRestoreThrowsToo_Terminates)
{
#if GTEST_HAS_DEATH_TEST
  GTEST_FLAG_SET (death_test_style, "threadsafe");
  EXPECT_EXIT (emplace_error_over_a_value (),
               ::testing::ExitedWithCode (kExitOnTerminate), "");
#else
  GTEST_SKIP () << "death tests are not supported on this platform";
#endif
}

TEST (ExpectedReinitDeathTest,
      VoidEmplaceError_OverAnError_WhenTheRestoreThrowsToo_Terminates)
{
#if GTEST_HAS_DEATH_TEST
  GTEST_FLAG_SET (death_test_style, "threadsafe");
  EXPECT_EXIT (void_emplace_error_over_an_error (),
               ::testing::ExitedWithCode (kExitOnTerminate), "");
#else
  GTEST_SKIP () << "death tests are not supported on this platform";
#endif
}

// With only the new error throwing, the restore works and the exception
// reaches the caller with the old contents back.
TEST_F (ExpectedReinitTest,
        EmplaceError_OverAnError_WhenOnlyTheNewErrorThrows_KeepsTheOldError)
{
  expected<int, fragile_error_t> uut (unexpect, kOld);
  plan () = plan_t{ 1, 1 };

  EXPECT_THROW (uut.emplace_error (kNew), injected_failure);
  plan () = plan_t{ 0, 0 };

  ASSERT_FALSE (uut.has_value ());
  EXPECT_EQ (uut.error ().id, kOld);
}

} // namespace
