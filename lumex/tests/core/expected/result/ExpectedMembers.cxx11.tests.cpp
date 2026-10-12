// The special member functions of expected<T, E> and expected<void, E> against
// the text of the standard: which of them exist, which are trivial, which are
// noexcept, for every pair of the type zoo of ExpectedMembersSupport.hpp
// ([expected.object.cons], [expected.object.dtor], [expected.object.assign],
// [expected.object.swap], [expected.void.cons], [expected.void.dtor],
// [expected.void.assign], [expected.void.swap]). The conditions are written in
// the support header from the clauses, not from the implementation, so a
// condition that is wrong in expected shows as a pair named in the failure.
// The tests compile from C++11, so every expected suite runs them.

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"
#include "lumex/tests/core/expected/ExpectedMembersSupport.hpp"

namespace
{
using namespace expected_members;
using lumex::core::expected::result::expected;

template <typename T, typename E> using ex = expected<T, E>;
template <typename E> using ex_void = expected<void, E>;

// ---- properties of expected<T, E> --------------------------------------

struct default_constructible
{
  template <typename T, typename E>
  static bool
  actual ()
  {
    return std::is_default_constructible<ex<T, E>>::value;
  }
  template <typename T, typename E>
  static bool
  expected ()
  {
    return std::is_default_constructible<T>::value;
  }
};

struct copy_constructible
{
  template <typename T, typename E>
  static bool
  actual ()
  {
    return std::is_copy_constructible<ex<T, E>>::value;
  }
  template <typename T, typename E>
  static bool
  expected ()
  {
    return copy_constructor_exists<T, E> ();
  }
};

struct trivially_copy_constructible
{
  template <typename T, typename E>
  static bool
  actual ()
  {
    return std::is_trivially_copy_constructible<ex<T, E>>::value;
  }
  template <typename T, typename E>
  static bool
  expected ()
  {
    return copy_constructor_exists<T, E> ()
           && copy_constructor_trivial<T, E> ();
  }
};

struct move_constructible
{
  template <typename T, typename E>
  static bool
  actual ()
  {
    return std::is_move_constructible<ex<T, E>>::value;
  }
  template <typename T, typename E>
  static bool
  expected ()
  {
    // A move constructor that does not take part leaves an rvalue to the
    // copy constructor.
    return move_constructor_exists<T, E> ()
           || copy_constructor_exists<T, E> ();
  }
};

struct trivially_move_constructible
{
  template <typename T, typename E>
  static bool
  actual ()
  {
    return std::is_trivially_move_constructible<ex<T, E>>::value;
  }
  template <typename T, typename E>
  static bool
  expected ()
  {
    return move_constructor_exists<T, E> ()
               ? move_constructor_trivial<T, E> ()
               : copy_constructor_exists<T, E> ()
                     && copy_constructor_trivial<T, E> ();
  }
};

struct nothrow_move_constructible
{
  template <typename T, typename E>
  static bool
  actual ()
  {
    return std::is_nothrow_move_constructible<ex<T, E>>::value;
  }
  template <typename T, typename E>
  static bool
  expected ()
  {
    // [expected.object.cons]/15; the copy constructor has no exception
    // specification, so it is noexcept only when it is trivial.
    return move_constructor_exists<T, E> ()
               ? nothrow_movable<T> () && nothrow_movable<E> ()
               : copy_constructor_exists<T, E> ()
                     && copy_constructor_trivial<T, E> ();
  }
};

struct copy_assignable
{
  template <typename T, typename E>
  static bool
  actual ()
  {
    return std::is_copy_assignable<ex<T, E>>::value;
  }
  template <typename T, typename E>
  static bool
  expected ()
  {
    return copy_assignment_exists<T, E> ();
  }
};

struct trivially_copy_assignable
{
  template <typename T, typename E>
  static bool
  actual ()
  {
    return std::is_trivially_copy_assignable<ex<T, E>>::value;
  }
  template <typename T, typename E>
  static bool
  expected ()
  {
    return copy_assignment_exists<T, E> () && copy_assignment_trivial<T, E> ();
  }
};

struct move_assignable
{
  template <typename T, typename E>
  static bool
  actual ()
  {
    return std::is_move_assignable<ex<T, E>>::value;
  }
  template <typename T, typename E>
  static bool
  expected ()
  {
    return move_assignment_exists<T, E> () || copy_assignment_exists<T, E> ();
  }
};

struct trivially_move_assignable
{
  template <typename T, typename E>
  static bool
  actual ()
  {
    return std::is_trivially_move_assignable<ex<T, E>>::value;
  }
  template <typename T, typename E>
  static bool
  expected ()
  {
    return move_assignment_exists<T, E> ()
               ? move_assignment_trivial<T, E> ()
               : copy_assignment_exists<T, E> ()
                     && copy_assignment_trivial<T, E> ();
  }
};

struct nothrow_move_assignable
{
  template <typename T, typename E>
  static bool
  actual ()
  {
    return std::is_nothrow_move_assignable<ex<T, E>>::value;
  }
  template <typename T, typename E>
  static bool
  expected ()
  {
    // [expected.object.assign]/9; the copy assignment has no exception
    // specification, so it is noexcept only when it is trivial.
    return move_assignment_exists<T, E> ()
               ? move_assignment_nothrow<T, E> ()
               : copy_assignment_exists<T, E> ()
                     && copy_assignment_trivial<T, E> ();
  }
};

struct trivially_destructible
{
  template <typename T, typename E>
  static bool
  actual ()
  {
    return std::is_trivially_destructible<ex<T, E>>::value;
  }
  template <typename T, typename E>
  static bool
  expected ()
  {
    return destructor_trivial<T, E> ();
  }
};

struct trivially_copyable
{
  /// No copy or move is left (a pinned value): the standard says the type is
  /// not trivially copyable (it needs one that is not deleted), the compilers'
  /// trait says it is, so those pairs are not compared.
  template <typename T, typename E>
  static bool
  nothing_left ()
  {
    return !copy_constructor_exists<T, E> ()
           && !move_constructor_exists<T, E> ()
           && !copy_assignment_exists<T, E> ()
           && !move_assignment_exists<T, E> ();
  }
  template <typename T, typename E>
  static bool
  expected ()
  {
    // Every special member function that is not deleted is trivial and the
    // destructor is trivial.
    return (!copy_constructor_exists<T, E> ()
            || copy_constructor_trivial<T, E> ())
           && (!move_constructor_exists<T, E> ()
               || move_constructor_trivial<T, E> ())
           && (!copy_assignment_exists<T, E> ()
               || copy_assignment_trivial<T, E> ())
           && (!move_assignment_exists<T, E> ()
               || move_assignment_trivial<T, E> ())
           && destructor_trivial<T, E> ();
  }
  template <typename T, typename E>
  static bool
  actual ()
  {
    return nothing_left<T, E> () ? expected<T, E> ()
                                 : std::is_trivially_copyable<ex<T, E>>::value;
  }
};

struct swappable_member
{
  template <typename T, typename E>
  static bool
  actual ()
  {
    return has_member_swap<ex<T, E>>::value;
  }
  template <typename T, typename E>
  static bool
  expected ()
  {
    return swap_exists<T, E> ();
  }
};

struct nothrow_swappable_member
{
  template <typename T, typename E>
  static bool
  actual ()
  {
    return member_swap_nothrow<ex<T, E>>::value;
  }
  template <typename T, typename E>
  static bool
  expected ()
  {
    return swap_exists<T, E> () && swap_nothrow<T, E> ();
  }
};

/// [expected.object.general]/1: the value is in the object, not behind a
/// pointer: the object is no bigger than the larger alternative plus the flag
/// and its padding, and no smaller than the larger alternative.
struct object_holds_its_contents
{
  template <typename T, typename E>
  static bool
  actual ()
  {
    std::size_t const larger = (std::max)(sizeof (T), sizeof (E));
    std::size_t const align = (std::max)(alignof (T), alignof (E));
    return sizeof (ex<T, E>) >= larger && sizeof (ex<T, E>) <= larger + align;
  }
  template <typename T, typename E>
  static bool
  expected ()
  {
    return true;
  }
};

// ---- properties of expected<void, E> -----------------------------------

struct void_copy_constructible
{
  template <typename E>
  static bool
  actual_void ()
  {
    return std::is_copy_constructible<ex_void<E>>::value;
  }
  template <typename E>
  static bool
  expected_void ()
  {
    return copyable<E> (); // [expected.void.cons]/5
  }
};

struct void_trivially_copy_constructible
{
  template <typename E>
  static bool
  actual_void ()
  {
    return std::is_trivially_copy_constructible<ex_void<E>>::value;
  }
  template <typename E>
  static bool
  expected_void ()
  {
    return copyable<E> () && std::is_trivially_copy_constructible<E>::value;
  }
};

struct void_move_constructible
{
  template <typename E>
  static bool
  actual_void ()
  {
    return std::is_move_constructible<ex_void<E>>::value;
  }
  template <typename E>
  static bool
  expected_void ()
  {
    return movable<E> () || copyable<E> (); // [expected.void.cons]/7
  }
};

struct void_nothrow_move_constructible
{
  template <typename E>
  static bool
  actual_void ()
  {
    return std::is_nothrow_move_constructible<ex_void<E>>::value;
  }
  template <typename E>
  static bool
  expected_void ()
  {
    return movable<E> ()
               ? nothrow_movable<E> ()
               : copyable<E> ()
                     && std::is_trivially_copy_constructible<E>::value;
  }
};

struct void_copy_assignable
{
  template <typename E>
  static bool
  actual_void ()
  {
    return std::is_copy_assignable<ex_void<E>>::value;
  }
  template <typename E>
  static bool
  expected_void ()
  {
    // [expected.void.assign]/3: no clause about moves, unlike the primary
    // template.
    return std::is_copy_assignable<E>::value && copyable<E> ();
  }
};

struct void_trivially_copy_assignable
{
  template <typename E>
  static bool
  actual_void ()
  {
    return std::is_trivially_copy_assignable<ex_void<E>>::value;
  }
  template <typename E>
  static bool
  expected_void ()
  {
    return std::is_copy_assignable<E>::value && copyable<E> ()
           && std::is_trivially_copy_constructible<E>::value
           && std::is_trivially_copy_assignable<E>::value
           && std::is_trivially_destructible<E>::value;
  }
};

struct void_move_assignable
{
  template <typename E>
  static bool
  actual_void ()
  {
    return std::is_move_assignable<ex_void<E>>::value;
  }
  template <typename E>
  static bool
  expected_void ()
  {
    // [expected.void.assign]/5: a Constraints element, so an rvalue falls back
    // to the copy assignment.
    return (movable<E> () && std::is_move_assignable<E>::value)
           || (std::is_copy_assignable<E>::value && copyable<E> ());
  }
};

struct void_nothrow_move_assignable
{
  template <typename E>
  static bool
  actual_void ()
  {
    return std::is_nothrow_move_assignable<ex_void<E>>::value;
  }
  template <typename E>
  static bool
  expected_void ()
  {
    return (movable<E> () && std::is_move_assignable<E>::value)
               ? nothrow_movable<E> ()
                     && std::is_nothrow_move_assignable<E>::value
               : std::is_copy_assignable<E>::value && copyable<E> ()
                     && std::is_trivially_copy_constructible<E>::value
                     && std::is_trivially_copy_assignable<E>::value
                     && std::is_trivially_destructible<E>::value;
  }
};

struct void_trivially_destructible
{
  template <typename E>
  static bool
  actual_void ()
  {
    return std::is_trivially_destructible<ex_void<E>>::value;
  }
  template <typename E>
  static bool
  expected_void ()
  {
    return std::is_trivially_destructible<E>::value;
  }
};

struct void_swappable
{
  template <typename E>
  static bool
  actual_void ()
  {
    return has_member_swap<ex_void<E>>::value;
  }
  template <typename E>
  static bool
  expected_void ()
  {
    return swappable<E> () && movable<E> (); // [expected.void.swap]/1
  }
};

struct void_nothrow_swappable
{
  template <typename E>
  static bool
  actual_void ()
  {
    return member_swap_nothrow<ex_void<E>>::value;
  }
  template <typename E>
  static bool
  expected_void ()
  {
    return swappable<E> () && movable<E> () && nothrow_movable<E> ()
           && nothrow_swappable<E> (); // [expected.void.swap]/4
  }
};

} // namespace

// === expected<T, E> =======================================================

TEST (ExpectedMembersTest, DefaultConstructor_ExistsIffTheValueHasOne)
{
  EXPECT_EQ (mismatches<default_constructible> (pair_zoo ()), "");
}

TEST (ExpectedMembersTest, CopyConstructor_IsDeletedUnlessBothCopyable)
{
  EXPECT_EQ (mismatches<copy_constructible> (pair_zoo ()), "");
}

TEST (ExpectedMembersTest, CopyConstructor_IsTrivialIffBothAre)
{
  EXPECT_EQ (mismatches<trivially_copy_constructible> (pair_zoo ()), "");
}

TEST (ExpectedMembersTest, MoveConstructor_IsLeftOutUnlessBothMovable)
{
  EXPECT_EQ (mismatches<move_constructible> (pair_zoo ()), "");
}

TEST (ExpectedMembersTest, MoveConstructor_IsTrivialIffBothAre)
{
  EXPECT_EQ (mismatches<trivially_move_constructible> (pair_zoo ()), "");
}

TEST (ExpectedMembersTest, MoveConstructor_IsNoexceptIffBothAre)
{
  EXPECT_EQ (mismatches<nothrow_move_constructible> (pair_zoo ()), "");
}

TEST (ExpectedMembersTest, CopyAssignment_IsDeletedUnlessAllConditionsHold)
{
  EXPECT_EQ (mismatches<copy_assignable> (pair_zoo ()), "");
}

TEST (ExpectedMembersTest, CopyAssignment_IsTrivialIffTheSixConditionsHold)
{
  EXPECT_EQ (mismatches<trivially_copy_assignable> (pair_zoo ()), "");
}

TEST (ExpectedMembersTest, MoveAssignment_IsLeftOutUnlessAllConditionsHold)
{
  EXPECT_EQ (mismatches<move_assignable> (pair_zoo ()), "");
}

TEST (ExpectedMembersTest, MoveAssignment_IsTrivialIffTheSixConditionsHold)
{
  EXPECT_EQ (mismatches<trivially_move_assignable> (pair_zoo ()), "");
}

TEST (ExpectedMembersTest, MoveAssignment_IsNoexceptIffTheFourConditionsHold)
{
  EXPECT_EQ (mismatches<nothrow_move_assignable> (pair_zoo ()), "");
}

TEST (ExpectedMembersTest, Destructor_IsTrivialIffBothAre)
{
  EXPECT_EQ (mismatches<trivially_destructible> (pair_zoo ()), "");
}

TEST (ExpectedMembersTest,
      TriviallyCopyable_FollowsTheFourMembersAndTheDestructor)
{
  EXPECT_EQ (mismatches<trivially_copyable> (pair_zoo ()), "");
}

TEST (ExpectedMembersTest, Swap_ExistsAndIsNoexceptAsTheStandardSays)
{
  EXPECT_EQ (mismatches<swappable_member> (pair_zoo ()), "");
  EXPECT_EQ (mismatches<nothrow_swappable_member> (pair_zoo ()), "");
}

TEST (ExpectedMembersTest, Object_HoldsItsContentsWithoutAllocation)
{
  EXPECT_EQ (mismatches<object_holds_its_contents> (pair_zoo ()), "");
}

TEST (ExpectedMembersTest, Value_LiesInsideTheObject)
{
  expected<std::string, int> const holding_value (
      std::string ("in the object"));
  expected<int, std::string> const holding_error (
      lumex::core::expected::result::unexpect, std::string ("in the object"));

  char const *const value_begin
      = reinterpret_cast<char const *> (std::addressof (*holding_value));
  char const *const error_begin = reinterpret_cast<char const *> (
      std::addressof (holding_error.error ()));
  char const *const first = reinterpret_cast<char const *> (&holding_value);
  char const *const second = reinterpret_cast<char const *> (&holding_error);

  EXPECT_GE (value_begin, first);
  EXPECT_LE (value_begin + sizeof (std::string),
             first + sizeof (holding_value));
  EXPECT_GE (error_begin, second);
  EXPECT_LE (error_begin + sizeof (std::string),
             second + sizeof (holding_error));
}

// === expected<void, E> ====================================================

TEST (ExpectedVoidMembersTest,
      CopyConstructor_IsDeletedUnlessTheErrorIsCopyable)
{
  EXPECT_EQ (void_mismatches<void_copy_constructible> (void_zoo ()), "");
}

TEST (ExpectedVoidMembersTest, CopyConstructor_IsTrivialIffTheErrorIs)
{
  EXPECT_EQ (void_mismatches<void_trivially_copy_constructible> (void_zoo ()),
             "");
}

TEST (ExpectedVoidMembersTest,
      MoveConstructor_IsLeftOutUnlessTheErrorIsMovable)
{
  EXPECT_EQ (void_mismatches<void_move_constructible> (void_zoo ()), "");
}

TEST (ExpectedVoidMembersTest, MoveConstructor_IsNoexceptIffTheErrorIs)
{
  EXPECT_EQ (void_mismatches<void_nothrow_move_constructible> (void_zoo ()),
             "");
}

TEST (ExpectedVoidMembersTest,
      CopyAssignment_NeedsOnlyTheErrorToBeCopyAssignable)
{
  EXPECT_EQ (void_mismatches<void_copy_assignable> (void_zoo ()), "");
}

TEST (ExpectedVoidMembersTest,
      CopyAssignment_IsTrivialIffTheThreeConditionsHold)
{
  EXPECT_EQ (void_mismatches<void_trivially_copy_assignable> (void_zoo ()),
             "");
}

TEST (ExpectedVoidMembersTest, MoveAssignment_IsLeftOutNotDeleted)
{
  EXPECT_EQ (void_mismatches<void_move_assignable> (void_zoo ()), "");
}

TEST (ExpectedVoidMembersTest,
      MoveAssignment_IsNoexceptIffTheTwoConditionsHold)
{
  EXPECT_EQ (void_mismatches<void_nothrow_move_assignable> (void_zoo ()), "");
}

TEST (ExpectedVoidMembersTest, Destructor_IsTrivialIffTheErrorIs)
{
  EXPECT_EQ (void_mismatches<void_trivially_destructible> (void_zoo ()), "");
}

TEST (ExpectedVoidMembersTest, Swap_ExistsAndIsNoexceptAsTheStandardSays)
{
  EXPECT_EQ (void_mismatches<void_swappable> (void_zoo ()), "");
  EXPECT_EQ (void_mismatches<void_nothrow_swappable> (void_zoo ()), "");
}

// === The cases the questions asked, spelled out ===========================

TEST (ExpectedMembersTest, NonCopyableContents_MakeTheExpectedNonCopyable)
{
  // The finding behind the layering: with a C++11 copy constructor written
  // in the class, is_copy_constructible was true for these.
  static_assert (
      !std::is_copy_constructible<expected<move_only_t, int>>::value,
      "a move-only value");
  static_assert (
      !std::is_copy_constructible<expected<int, move_only_t>>::value,
      "a move-only error");
  static_assert (
      !std::is_copy_constructible<expected<void, move_only_t>>::value,
      "a move-only error of void");
  static_assert (!std::is_copy_assignable<expected<move_only_t, int>>::value,
                 "a move-only value");
  static_assert (!std::is_copy_assignable<expected<int, move_only_t>>::value,
                 "a move-only error");
  static_assert (!std::is_copy_assignable<expected<void, move_only_t>>::value,
                 "a move-only error of void");
  static_assert (std::is_move_constructible<expected<move_only_t, int>>::value,
                 "but movable");
  static_assert (std::is_move_assignable<expected<move_only_t, int>>::value,
                 "but movable");
  static_assert (
      std::is_move_constructible<expected<void, move_only_t>>::value,
      "but movable");
  static_assert (!std::is_copy_constructible<expected<pinned_t, int>>::value,
                 "a value that cannot be copied or moved");
  static_assert (!std::is_move_constructible<expected<pinned_t, int>>::value,
                 "a value that cannot be copied or moved");
  SUCCEED ();
}

TEST (ExpectedMembersTest, TrivialContents_MakeTheExpectedTrivial)
{
  static_assert (std::is_trivially_copyable<expected<int, int>>::value,
                 "int and int");
  static_assert (std::is_trivially_copyable<expected<trivial_t, int>>::value,
                 "a trivial struct");
  static_assert (std::is_trivially_copyable<expected<void, int>>::value,
                 "void and int");
  static_assert (std::is_trivially_destructible<expected<void, int>>::value,
                 "void and int");
  static_assert (
      std::is_trivially_copy_constructible<expected<int, int>>::value,
      "int and int");
  static_assert (
      std::is_trivially_move_constructible<expected<int, int>>::value,
      "int and int");
  static_assert (std::is_trivially_copy_assignable<expected<int, int>>::value,
                 "int and int");
  static_assert (std::is_trivially_move_assignable<expected<int, int>>::value,
                 "int and int");
  static_assert (
      !std::is_trivially_copyable<expected<std::string, int>>::value,
      "a std::string value");
  static_assert (
      !std::is_trivially_destructible<expected<std::string, int>>::value,
      "a std::string value");
  static_assert (
      !std::is_trivially_copy_constructible<expected<int, std::string>>::value,
      "a std::string error");
  SUCCEED ();
}
