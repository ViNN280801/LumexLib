// LumexOptionalSpecialMembers.cxx11.tests.cpp
//
// The special member functions of optional<T> ([optional.ctor],
// [optional.dtor], [optional.assign]): which of them exist, which are
// trivial, what a copy or move assignment does with the contained value and
// in the presence of exceptions, and that a moved-from optional keeps its
// (moved-from) value. Each test names the mutation it catches in the
// comment above it (the pre-registered list of the rework).
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/optional/opt/LumexOptional.hpp"

namespace
{
using lumex::core::optional::opt::in_place;
using lumex::core::optional::opt::nullopt;
using lumex::core::optional::opt::optional;

// Counts constructions, assignments and destructions, and throws on demand.
struct Probe
{
  static int constructions;
  static int copy_assignments;
  static int move_assignments;
  static int destructions;
  static bool throw_on_copy_construct;
  static bool throw_on_copy_assign;

  static void
  reset ()
  {
    constructions = 0;
    copy_assignments = 0;
    move_assignments = 0;
    destructions = 0;
    throw_on_copy_construct = false;
    throw_on_copy_assign = false;
  }

  int id;

  explicit Probe (int value) : id (value) { ++constructions; }
  Probe (Probe const &other) : id (other.id)
  {
    if (throw_on_copy_construct)
      throw std::runtime_error ("copy construct");
    ++constructions;
  }
  Probe (Probe &&other) noexcept : id (other.id)
  {
    other.id = -1;
    ++constructions;
  }
  Probe &
  operator= (Probe const &other)
  {
    if (throw_on_copy_assign)
      throw std::runtime_error ("copy assign");
    ++copy_assignments;
    id = other.id;
    return *this;
  }
  Probe &
  operator= (Probe &&other) noexcept
  {
    ++move_assignments;
    id = other.id;
    other.id = -1;
    return *this;
  }
  ~Probe () { ++destructions; }
};

int Probe::constructions = 0;
int Probe::copy_assignments = 0;
int Probe::move_assignments = 0;
int Probe::destructions = 0;
bool Probe::throw_on_copy_construct = false;
bool Probe::throw_on_copy_assign = false;

struct MoveOnly
{
  MoveOnly () = default;
  MoveOnly (MoveOnly &&) = default;
  MoveOnly &operator= (MoveOnly &&) = default;
};

// Copy constructible, move constructor explicitly deleted: an rvalue optional
// must be copied.
struct CopyOnlyByDeletedMove
{
  CopyOnlyByDeletedMove () = default;
  CopyOnlyByDeletedMove (CopyOnlyByDeletedMove const &) = default;
  CopyOnlyByDeletedMove (CopyOnlyByDeletedMove &&) = delete;
  CopyOnlyByDeletedMove &operator= (CopyOnlyByDeletedMove const &) = default;
};

struct Pod
{
  int a;
  double b;
};

// Trivially copyable but with a destructor that is not trivial.
struct UserDestructor
{
  int value;
  ~UserDestructor () {}
};

struct NoAssign
{
  int const value;
  explicit NoAssign (int v) : value (v) {}
};

struct NothrowNonTrivial
{
  NothrowNonTrivial () noexcept {}
  NothrowNonTrivial (NothrowNonTrivial const &) noexcept {}
  NothrowNonTrivial (NothrowNonTrivial &&) noexcept {}
  NothrowNonTrivial &
  operator= (NothrowNonTrivial const &) noexcept
  {
    return *this;
  }
  NothrowNonTrivial &
  operator= (NothrowNonTrivial &&) noexcept
  {
    return *this;
  }
};
} // namespace

// ---- Triviality (mutations: a layer always written out / always deleted)
// ----

// M9 (trivial destructor lost), M10 (trivial copy constructor lost), M11
// (trivial copy assignment lost), M12 (trivial move operations lost).
TEST (LumexOptionalSpecialMembersTest,
      GivenTriviallyCopyableT_WhenOptional_ThenAllSpecialMembersTrivial)
{
  static_assert (std::is_trivially_destructible<optional<int>>::value,
                 "M9: trivially destructible");
  static_assert (std::is_trivially_copy_constructible<optional<int>>::value,
                 "M10: trivially copy constructible");
  static_assert (std::is_trivially_move_constructible<optional<int>>::value,
                 "M12: trivially move constructible");
  static_assert (std::is_trivially_copy_assignable<optional<int>>::value,
                 "M11: trivially copy assignable");
  static_assert (std::is_trivially_move_assignable<optional<int>>::value,
                 "M12: trivially move assignable");
  static_assert (std::is_trivially_copyable<optional<int>>::value,
                 "trivially copyable");
  static_assert (std::is_trivially_copyable<optional<Pod>>::value,
                 "trivially copyable aggregate");
  static_assert (std::is_trivially_copyable<optional<double>>::value,
                 "trivially copyable double");
  SUCCEED ();
}

TEST (LumexOptionalSpecialMembersTest,
      GivenNonTrivialT_WhenOptional_ThenNotTrivialAndStillWorks)
{
  static_assert (!std::is_trivially_destructible<optional<std::string>>::value,
                 "a string needs its destructor");
  static_assert (
      !std::is_trivially_copy_constructible<optional<std::string>>::value,
      "a string copy is not trivial");
  static_assert (
      !std::is_trivially_copy_assignable<optional<std::string>>::value,
      "a string assignment is not trivial");
  static_assert (!std::is_trivially_copyable<optional<std::string>>::value,
                 "not trivially copyable");
  static_assert (
      !std::is_trivially_destructible<optional<UserDestructor>>::value,
      "the user destructor is not trivial");
  static_assert (
      !std::is_trivially_copy_assignable<optional<UserDestructor>>::value,
      "the assignment needs a trivial destructor ([optional.assign])");
  SUCCEED ();
}

// ---- Constraints (M1, M2) ----

// M1: the copy constructor is not constrained on T.
TEST (LumexOptionalSpecialMembersTest,
      GivenMoveOnlyT_WhenTraits_ThenCopyConstructorIsDeleted)
{
  using ptr = optional<std::unique_ptr<int>>;
  static_assert (!std::is_copy_constructible<ptr>::value,
                 "M1: copy constructor constrained on T");
  static_assert (std::is_move_constructible<ptr>::value, "move works");
  static_assert (!std::is_copy_assignable<ptr>::value,
                 "M2: copy assignment constrained on T");
  static_assert (std::is_move_assignable<ptr>::value, "move assign works");
  static_assert (!std::is_copy_constructible<optional<MoveOnly>>::value,
                 "M1: custom move-only type");
  SUCCEED ();
}

TEST (LumexOptionalSpecialMembersTest,
      GivenTWithDeletedMove_WhenRvalueOptional_ThenCopied)
{
  static_assert (!std::is_move_constructible<CopyOnlyByDeletedMove>::value,
                 "the fixture has no move constructor");
  using wrapped = optional<CopyOnlyByDeletedMove>;
  static_assert (std::is_copy_constructible<wrapped>::value, "copyable");
  static_assert (std::is_constructible<wrapped, wrapped &&>::value,
                 "an rvalue optional is copied: the move is not viable");
  wrapped source (in_place);
  wrapped copy (std::move (source));
  EXPECT_TRUE (copy.has_value ());
  EXPECT_TRUE (source.has_value ());
}

TEST (LumexOptionalSpecialMembersTest,
      GivenTWithoutAssignment_WhenTraits_ThenOptionalNotAssignable)
{
  static_assert (std::is_copy_constructible<optional<NoAssign>>::value,
                 "copy construction works");
  static_assert (!std::is_copy_assignable<optional<NoAssign>>::value,
                 "M2: no copy assignment without T's");
  static_assert (!std::is_move_assignable<optional<NoAssign>>::value,
                 "M2: nor a move assignment");
  SUCCEED ();
}

// ---- Copy assignment (M3, M4, M5) ----

// M3: both engaged, destroy and rebuild instead of assigning.
TEST (LumexOptionalSpecialMembersTest,
      GivenBothEngaged_WhenCopyAssign_ThenValueIsAssignedNotRebuilt)
{
  Probe::reset ();
  {
    optional<Probe> target (in_place, 1);
    optional<Probe> const source (in_place, 2);
    int const built = Probe::constructions;
    int const destroyed = Probe::destructions;
    target = source;
    EXPECT_EQ (Probe::copy_assignments, 1);
    EXPECT_EQ (Probe::constructions, built) << "no new object";
    EXPECT_EQ (Probe::destructions, destroyed) << "no object destroyed";
    EXPECT_EQ (target->id, 2);
  }
}

TEST (LumexOptionalSpecialMembersTest,
      GivenTargetEmpty_WhenCopyAssign_ThenValueConstructed)
{
  Probe::reset ();
  optional<Probe> target;
  optional<Probe> const source (in_place, 5);
  int const built = Probe::constructions;
  target = source;
  EXPECT_EQ (Probe::constructions, built + 1);
  EXPECT_EQ (Probe::copy_assignments, 0);
  EXPECT_EQ (target->id, 5);
}

TEST (LumexOptionalSpecialMembersTest,
      GivenSourceEmpty_WhenCopyAssign_ThenTargetValueDestroyed)
{
  Probe::reset ();
  optional<Probe> target (in_place, 5);
  optional<Probe> const source;
  int const destroyed = Probe::destructions;
  target = source;
  EXPECT_FALSE (target.has_value ());
  EXPECT_EQ (Probe::destructions, destroyed + 1);
}

// M4: a throwing copy assignment of T leaves both engaged and the target
// holds the state T's assignment leaves.
TEST (LumexOptionalSpecialMembersTest,
      GivenBothEngaged_WhenCopyAssignOfTThrows_ThenTargetStaysEngaged)
{
  Probe::reset ();
  optional<Probe> target (in_place, 1);
  optional<Probe> const source (in_place, 2);
  Probe::throw_on_copy_assign = true;
  EXPECT_THROW (target = source, std::runtime_error);
  Probe::throw_on_copy_assign = false;
  EXPECT_TRUE (target.has_value ());
  EXPECT_EQ (target->id, 1);
  EXPECT_TRUE (source.has_value ());
}

// M5: a throwing copy constructor with an empty target leaves it empty.
TEST (LumexOptionalSpecialMembersTest,
      GivenTargetEmpty_WhenCopyConstructorThrows_ThenTargetStaysEmpty)
{
  Probe::reset ();
  optional<Probe> target;
  optional<Probe> const source (in_place, 2);
  Probe::throw_on_copy_construct = true;
  EXPECT_THROW (target = source, std::runtime_error);
  Probe::throw_on_copy_construct = false;
  EXPECT_FALSE (target.has_value ());
  EXPECT_TRUE (source.has_value ());
}

// A throwing copy constructor does not matter when both are engaged: the
// target is assigned, not rebuilt.
TEST (LumexOptionalSpecialMembersTest,
      GivenBothEngaged_WhenCopyConstructorThrows_ThenAssignmentStillWorks)
{
  Probe::reset ();
  optional<Probe> target (in_place, 1);
  optional<Probe> const source (in_place, 2);
  Probe::throw_on_copy_construct = true;
  EXPECT_NO_THROW (target = source);
  Probe::throw_on_copy_construct = false;
  EXPECT_EQ (target->id, 2);
}

TEST (LumexOptionalSpecialMembersTest, GivenSelf_WhenCopyAssign_ThenUnchanged)
{
  optional<std::string> value (std::string (40, 'z'));
  optional<std::string> &alias = value;
  value = alias;
  ASSERT_TRUE (value.has_value ());
  EXPECT_EQ (*value, std::string (40, 'z'));
}

// ---- Moved-from state (M6, M7, M8) ----

// M6: the move constructor destroys the value of the source.
TEST (LumexOptionalSpecialMembersTest,
      GivenEngaged_WhenMoveConstructed_ThenSourceKeepsMovedFromValue)
{
  optional<std::string> source (std::string (40, 'a'));
  optional<std::string> target (std::move (source));
  ASSERT_TRUE (target.has_value ());
  EXPECT_EQ (*target, std::string (40, 'a'));
  EXPECT_TRUE (source.has_value ()) << "M6: the source stays engaged";
  EXPECT_TRUE (source->empty ()) << "its string was moved out";
}

// M7: the move assignment destroys the value of the source.
TEST (LumexOptionalSpecialMembersTest,
      GivenEngaged_WhenMoveAssigned_ThenSourceKeepsMovedFromValue)
{
  optional<std::string> source (std::string (40, 'b'));
  optional<std::string> target (std::string (40, 'c'));
  target = std::move (source);
  ASSERT_TRUE (target.has_value ());
  EXPECT_EQ (*target, std::string (40, 'b'));
  EXPECT_TRUE (source.has_value ()) << "M7: the source stays engaged";

  optional<std::string> empty_target;
  optional<std::string> source2 (std::string (40, 'd'));
  empty_target = std::move (source2);
  EXPECT_TRUE (source2.has_value ()) << "M7: also into an empty target";
  EXPECT_EQ (*empty_target, std::string (40, 'd'));
}

// M8: move assignment between two engaged optionals rebuilds the value.
TEST (LumexOptionalSpecialMembersTest,
      GivenBothEngaged_WhenMoveAssign_ThenValueIsMoveAssigned)
{
  Probe::reset ();
  optional<Probe> target (in_place, 1);
  optional<Probe> source (in_place, 2);
  int const built = Probe::constructions;
  int const destroyed = Probe::destructions;
  target = std::move (source);
  EXPECT_EQ (Probe::move_assignments, 1);
  EXPECT_EQ (Probe::constructions, built);
  EXPECT_EQ (Probe::destructions, destroyed);
  EXPECT_TRUE (source.has_value ());
  EXPECT_EQ (target->id, 2);
}

TEST (LumexOptionalSpecialMembersTest,
      GivenEmptySource_WhenMoved_ThenTargetBecomesEmptyAndSourceStaysEmpty)
{
  optional<std::string> empty;
  optional<std::string> target (std::string (40, 'e'));
  target = std::move (empty);
  EXPECT_FALSE (target.has_value ());
  EXPECT_FALSE (empty.has_value ());
  optional<std::string> built (std::move (empty));
  EXPECT_FALSE (built.has_value ());
}

TEST (LumexOptionalSpecialMembersTest,
      GivenMoveOnlyValue_WhenMovedBetweenOptionals_ThenOwnershipMoves)
{
  optional<std::unique_ptr<int>> source (std::unique_ptr<int> (new int (7)));
  optional<std::unique_ptr<int>> target (std::move (source));
  ASSERT_TRUE (target.has_value ());
  EXPECT_EQ (**target, 7);
  ASSERT_TRUE (source.has_value ());
  EXPECT_EQ (source->get (), nullptr) << "the unique_ptr was moved from";
  optional<std::unique_ptr<int>> third;
  third = std::move (target);
  EXPECT_EQ (**third, 7);
  EXPECT_TRUE (target.has_value ());
}

// ---- noexcept ----

TEST (LumexOptionalSpecialMembersTest,
      GivenNothrowT_WhenSpecialMembers_ThenNoexceptFollowsT)
{
  using opt_nothrow = optional<NothrowNonTrivial>;
  static_assert (std::is_nothrow_move_constructible<opt_nothrow>::value, "");
  static_assert (std::is_nothrow_move_assignable<opt_nothrow>::value, "");
  static_assert (std::is_nothrow_copy_constructible<opt_nothrow>::value, "");
  static_assert (std::is_nothrow_copy_assignable<opt_nothrow>::value, "");
  static_assert (!std::is_nothrow_move_constructible<optional<Probe>>::value
                     == !std::is_nothrow_move_constructible<Probe>::value,
                 "");
  static_assert (
      std::is_nothrow_move_constructible<optional<std::string>>::value,
      "a string moves without throwing");
  static_assert (
      !std::is_nothrow_copy_constructible<optional<std::string>>::value,
      "a string copy may throw");
  static_assert (
      std::is_nothrow_default_constructible<optional<std::string>>::value, "");
  static_assert (std::is_nothrow_constructible<optional<std::string>,
                                               decltype (nullopt)>::value,
                 "");
  SUCCEED ();
}

// ---- Destructor and lifetime ----

TEST (LumexOptionalSpecialMembersTest,
      GivenEngaged_WhenDestroyed_ThenOneDestruction)
{
  Probe::reset ();
  {
    optional<Probe> value (in_place, 1);
    optional<Probe> empty;
    (void)empty;
  }
  EXPECT_EQ (Probe::destructions, 1);
  EXPECT_EQ (Probe::constructions, 1);
}

TEST (LumexOptionalSpecialMembersTest,
      GivenMovedFromEngaged_WhenDestroyed_ThenBothValuesDestroyed)
{
  Probe::reset ();
  {
    optional<Probe> source (in_place, 1);
    optional<Probe> target (std::move (source));
    (void)target;
  }
  // The source still held a (moved-from) Probe: two objects, two destructions.
  EXPECT_EQ (Probe::constructions, 2);
  EXPECT_EQ (Probe::destructions, 2);
}

// M35: the exception derives from std::logic_error (std::bad_optional_access
// exists only from C++17).
TEST (LumexOptionalSpecialMembersTest,
      GivenEmpty_WhenValue_ThenLogicErrorIsThrown)
{
  static_assert (
      std::is_base_of<
          std::logic_error,
          lumex::core::optional::opt::lumex_bad_optional_access>::value,
      "M35: derives from std::logic_error");
  optional<int> empty;
  try
    {
      (void)empty.value ();
      FAIL () << "value () of an empty optional did not throw";
    }
  catch (std::logic_error const &error)
    {
      EXPECT_NE (std::string (error.what ()).find ("Bad optional access"),
                 std::string::npos);
    }
}

// optional<const T> is accepted (as std::optional<const T>): it can be built
// and copied, not assigned.
TEST (LumexOptionalSpecialMembersTest,
      GivenConstT_WhenOptional_ThenCopiedButNotAssigned)
{
  static_assert (std::is_copy_constructible<optional<int const>>::value, "");
  static_assert (!std::is_copy_assignable<optional<int const>>::value, "");
  static_assert (!std::is_move_assignable<optional<int const>>::value, "");
  optional<std::string const> const first (std::string ("const"));
  optional<std::string const> second = first;
  ASSERT_TRUE (second.has_value ());
  EXPECT_EQ (*second, "const");
  second.reset ();
  EXPECT_FALSE (second.has_value ());
  second.emplace ("again");
  EXPECT_EQ (*second, "again");
}
