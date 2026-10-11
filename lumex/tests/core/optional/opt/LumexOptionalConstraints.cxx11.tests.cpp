// LumexOptionalConstraints.cxx11.tests.cpp
//
// Regressions found by review of the rework: a T whose constructor template
// asks about the optional of T (type erasure, std::any), a T whose copy
// operations take a non-const reference, special members of T that the layers
// must respect (trivial copy with a destructor, throwing move assignment),
// ADL swap, or_else on an rvalue, `o = {}`, and an immovable transform result.
// Each test names the defect it catches (O1 ... O12, F1 ... F6).
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#if __cplusplus >= 201703L
#include <any>
#include <optional>
#endif

#include <gtest/gtest.h>

#include "lumex/core/optional/opt/LumexOptional.hpp"

namespace
{
using lumex::core::optional::opt::in_place;
using lumex::core::optional::opt::optional;

// A type-erasure wrapper: a constructor template that accepts anything copy
// constructible, as std::any does.
struct Erased
{
  Erased () = default;
  template <typename F,
            typename std::enable_if<
                !std::is_same<typename std::decay<F>::type, Erased>::value
                    && std::is_copy_constructible<
                        typename std::decay<F>::type>::value,
                int>::type
            = 0>
  Erased (F &&)
  {
  }
};

// Trivial copy construction and assignment, but a destructor that counts.
struct TrivialCopyWithDestructor
{
  static int destructions;
  int value;
  explicit TrivialCopyWithDestructor (int v) : value (v) {}
  ~TrivialCopyWithDestructor () { ++destructions; }
};
int TrivialCopyWithDestructor::destructions = 0;

namespace adl
{
struct Swapped
{
  static int swaps;
  int value;
  explicit Swapped (int v) : value (v) {}
};
int Swapped::swaps = 0;

void
swap (Swapped &a, Swapped &b)
{
  ++Swapped::swaps;
  std::swap (a.value, b.value);
}
} // namespace adl

struct CopyCounter
{
  static int copies;
  CopyCounter () = default;
  CopyCounter (CopyCounter const &) { ++copies; }
  CopyCounter (CopyCounter &&) noexcept {}
  CopyCounter &
  operator= (CopyCounter const &)
  {
    ++copies;
    return *this;
  }
  CopyCounter &
  operator= (CopyCounter &&) noexcept
  {
    return *this;
  }
};
int CopyCounter::copies = 0;

struct ThrowingMoveAssign
{
  ThrowingMoveAssign () = default;
  ThrowingMoveAssign (ThrowingMoveAssign &&) noexcept {}
  ThrowingMoveAssign &
  operator= (ThrowingMoveAssign &&) noexcept (false)
  {
    return *this;
  }
};

// Copy operations that take a non-const reference.
struct NonConstAssign
{
  NonConstAssign () = default;
  NonConstAssign (NonConstAssign const &) {}
  NonConstAssign &
  operator= (NonConstAssign &)
  {
    return *this;
  }
};

struct LegacyCopy
{
  LegacyCopy () = default;
  LegacyCopy (LegacyCopy &) {}
  LegacyCopy &
  operator= (LegacyCopy &)
  {
    return *this;
  }
};
} // namespace

// F1, O11: the constructors from optional<U> and the value constructor do not
// ask T about optional<T>.
TEST (LumexOptionalConstraintsTest,
      GivenErasedT_WhenOptionalOfOptional_ThenCompilesAndCopies)
{
  static_assert (std::is_copy_constructible<optional<Erased>>::value, "F1");
  static_assert (std::is_copy_assignable<optional<Erased>>::value, "F1");
  optional<Erased> first (in_place);
  optional<Erased> second (first);
  second = first;
  optional<Erased> third (std::move (second));
  EXPECT_TRUE (third.has_value ());
  EXPECT_TRUE (first.has_value ());
  // O11: a non-const lvalue optional is copied, not wrapped as a value: the
  // copy of an empty one is empty.
  optional<Erased> empty;
  optional<Erased> empty_copy (empty);
  EXPECT_FALSE (empty_copy.has_value ());
  optional<Erased> empty_target (in_place);
  empty_target = empty;
  EXPECT_FALSE (empty_target.has_value ());
}

#if __cplusplus >= 201703L
TEST (LumexOptionalConstraintsTest, GivenStdAny_WhenOptional_ThenCopies)
{
  optional<std::any> first (in_place, 5);
  optional<std::any> second (first);
  second = first;
  ASSERT_TRUE (second.has_value ());
  EXPECT_EQ (std::any_cast<int> (*second), 5);
  optional<std::any> empty;
  optional<std::any> empty_copy (empty);
  EXPECT_FALSE (empty_copy.has_value ()) << "O11";
  optional<std::any> from_value (std::any (std::string ("x")));
  EXPECT_TRUE (from_value.has_value ());
}
#endif

// F2: T with a non-const copy assignment or copy constructor.
TEST (LumexOptionalConstraintsTest,
      GivenNonConstCopyOperations_WhenOptional_ThenCompiles)
{
  static_assert (std::is_copy_constructible<optional<NonConstAssign>>::value,
                 "F2: const copy constructor works");
  static_assert (!std::is_copy_assignable<optional<NonConstAssign>>::value,
                 "F2: no copy assignment");
  optional<NonConstAssign> value (in_place);
  optional<NonConstAssign> copy (value);
  copy.emplace ();
  EXPECT_TRUE (copy.has_value ());

  static_assert (!std::is_copy_constructible<optional<LegacyCopy>>::value,
                 "F2");
  static_assert (!std::is_copy_assignable<optional<LegacyCopy>>::value, "F2");
  optional<LegacyCopy> legacy (in_place);
  legacy.reset ();
  legacy.emplace ();
  EXPECT_TRUE (legacy.has_value ());
}

// O1: trivial copy assignment, non-trivial destructor: assigning an empty
// optional to an engaged one must destroy the value. (GCC and Clang also call
// the copy constructor of such a type not trivial, which is why dropping
// `is_trivially_destructible` from the condition of the trivial copy
// assignment changes nothing on them.)
TEST (LumexOptionalConstraintsTest,
      GivenTrivialCopyAndDestructor_WhenAssignedEmpty_ThenValueDestroyed)
{
  using type = TrivialCopyWithDestructor;
  static_assert (std::is_trivially_copy_assignable<type>::value, "fixture");
  static_assert (!std::is_trivially_destructible<type>::value, "fixture");
  static_assert (!std::is_trivially_destructible<optional<type>>::value, "O1");
  static_assert (!std::is_trivially_copy_assignable<optional<type>>::value,
                 "O1: assignment must run the destructor");
  optional<type> target (in_place, 1);
  optional<type> const empty;
  int const before = type::destructions;
  target = empty;
  EXPECT_FALSE (target.has_value ());
  EXPECT_EQ (type::destructions, before + 1);
  optional<type> target2 (in_place, 2);
  int const before2 = type::destructions;
  target2 = optional<type> ();
  EXPECT_EQ (type::destructions, before2 + 1) << "move assignment as well";
}

// O2: swap uses the swap of T found by ADL.
TEST (LumexOptionalConstraintsTest, GivenAdlSwap_WhenSwapped_ThenItIsCalled)
{
  using adl::Swapped;
  Swapped::swaps = 0;
  optional<Swapped> first (in_place, 1);
  optional<Swapped> second (in_place, 2);
  first.swap (second);
  EXPECT_EQ (Swapped::swaps, 1);
  EXPECT_EQ (first->value, 2);
  using lumex::core::optional::opt::swap;
  swap (first, second);
  EXPECT_EQ (Swapped::swaps, 2);
  EXPECT_EQ (first->value, 1);
}

// O3: or_else on an rvalue moves the value.
TEST (LumexOptionalConstraintsTest, GivenRvalue_WhenOrElse_ThenValueMoved)
{
  optional<CopyCounter> engaged (in_place);
  CopyCounter::copies = 0;
  optional<CopyCounter> result
      = std::move (engaged).or_else ([] { return optional<CopyCounter> (); });
  EXPECT_TRUE (result.has_value ());
  EXPECT_EQ (CopyCounter::copies, 0);
  optional<CopyCounter> const kept (in_place);
  CopyCounter::copies = 0;
  optional<CopyCounter> copy
      = kept.or_else ([] { return optional<CopyCounter> (); });
  EXPECT_EQ (CopyCounter::copies, 1) << "a const lvalue is copied once";
}

// O9: the move assignment is noexcept only when T's is.
TEST (LumexOptionalConstraintsTest,
      GivenThrowingMoveAssign_WhenTraits_ThenNotNoexcept)
{
  static_assert (
      std::is_nothrow_move_constructible<optional<ThrowingMoveAssign>>::value,
      "the move constructor does not throw");
  static_assert (
      !std::is_nothrow_move_assignable<optional<ThrowingMoveAssign>>::value,
      "O9");
  static_assert (std::is_nothrow_move_assignable<optional<int>>::value, "");
}

// O12: `o = {}` empties the optional.
TEST (LumexOptionalConstraintsTest, GivenEngaged_WhenAssignedBraces_ThenEmpty)
{
  optional<int> number (5);
  number = {};
  EXPECT_FALSE (number.has_value ()) << "O12";
  optional<std::string> text (std::string ("x"));
  text = {};
  EXPECT_FALSE (text.has_value ());
  optional<int *> pointer (nullptr);
  pointer = {};
  EXPECT_FALSE (pointer.has_value ());
}

#if __cplusplus >= 201703L
namespace
{
struct Immovable
{
  int value;
  explicit Immovable (int v) : value (v) {}
  Immovable (Immovable const &) = delete;
  Immovable (Immovable &&) = delete;
};
} // namespace

// F5: transform builds a result that cannot be moved in place (guaranteed
// elision).
TEST (LumexOptionalConstraintsTest,
      GivenImmovableResult_WhenTransform_ThenBuiltInPlace)
{
  optional<int> value (3);
  auto result = value.transform ([] (int v) { return Immovable (v * 2); });
  static_assert (std::is_same<decltype (result), optional<Immovable>>::value,
                 "F5");
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (result->value, 6);
  optional<int> empty;
  EXPECT_FALSE (
      empty.transform ([] (int v) { return Immovable (v); }).has_value ());
}

// F6: for T = bool a std::optional of another type is not a source.
TEST (LumexOptionalConstraintsTest,
      GivenBoolT_WhenStdOptionalOfInt_ThenNotConstructible)
{
  static_assert (
      !std::is_constructible<optional<bool>, std::optional<int>>::value,
      "F6: no read of the state through explicit operator bool");
  static_assert (
      std::is_constructible<optional<bool>, std::optional<bool>>::value,
      "the std twin of the same T still converts");
  optional<bool> from_std = std::optional<bool> (false);
  ASSERT_TRUE (from_std.has_value ());
  EXPECT_FALSE (*from_std);
  optional<bool> from_empty = std::optional<bool> ();
  EXPECT_FALSE (from_empty.has_value ());
}
#endif
