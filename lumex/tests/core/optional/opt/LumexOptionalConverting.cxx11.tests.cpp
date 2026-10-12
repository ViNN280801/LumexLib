// LumexOptionalConverting.cxx11.tests.cpp
//
// Converting constructors and assignments of optional<T> ([optional.ctor],
// [optional.assign]): from a value U and from optional<U>, with the
// explicit-when-not-implicitly-convertible rules, the constraints, and the
// in-place constructors' constraints. The comments name the mutation of the
// pre-registered list that each test catches.
#include <initializer_list>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/optional/opt/LumexOptional.hpp"

namespace
{
using lumex::core::optional::opt::in_place;
using lumex::core::optional::opt::in_place_t;
using lumex::core::optional::opt::nullopt;
using lumex::core::optional::opt::optional;

// Constructible from int only explicitly.
struct FromIntExplicit
{
  int value;
  explicit FromIntExplicit (int v) : value (v) {}
};

// Implicitly constructible from int.
struct FromIntImplicit
{
  int value;
  FromIntImplicit (int v) : value (v) {} // NOLINT
};

// Constructible from any optional<int> in a way that would swallow one.
struct FromOptionalInt
{
  bool swallowed;
  FromOptionalInt (int) : swallowed (false) {}
  FromOptionalInt (optional<int> const &) : swallowed (true) {}
};

// Assignable from int; used to see assignment through an engaged optional.
struct Counting
{
  int assigned = 0;
  int constructed = 0;
  Counting () = default;
  Counting (int) : constructed (1) {}
  Counting &
  operator= (int)
  {
    ++assigned;
    return *this;
  }
};

struct WithListCtor
{
  std::size_t size;
  char tag;
  WithListCtor (std::initializer_list<int> list, char t)
      : size (list.size ()), tag (t)
  {
  }
};

// Predicates for the traits of the constructors and the assignments.
template <typename Target, typename Source>
using implicitly_converts = std::is_convertible<Source, Target>;
} // namespace

// ---- from a value (M15, M16) ----

// M15: the implicit constructor from U is explicit (or missing).
TEST (LumexOptionalConvertingTest,
      GivenConvertibleValue_WhenCopyInitialized_ThenCompiles)
{
  optional<std::string> from_literal = "abc";
  ASSERT_TRUE (from_literal.has_value ());
  EXPECT_EQ (*from_literal, "abc");
  optional<long> from_int = 5;
  EXPECT_EQ (*from_int, 5L);
  optional<FromIntImplicit> from_implicit = 7;
  EXPECT_EQ (from_implicit->value, 7);
  optional<std::vector<int>> from_braces;
  from_braces = { 1, 2, 3 }; // U defaults to T for a braced list
  ASSERT_TRUE (from_braces.has_value ());
  EXPECT_EQ (from_braces->size (), static_cast<std::size_t> (3));
  static_assert (
      implicitly_converts<optional<std::string>, char const *>::value,
      "M15: implicit from a literal");
  static_assert (implicitly_converts<optional<long>, int>::value,
                 "M15: implicit from int to long");
  static_assert (implicitly_converts<optional<FromIntImplicit>, int>::value,
                 "M15: implicit when T converts implicitly");
}

// M16: the constructor is implicit although T converts only explicitly.
TEST (LumexOptionalConvertingTest,
      GivenExplicitOnlyConversion_WhenTraits_ThenConstructorIsExplicit)
{
  static_assert (std::is_constructible<optional<FromIntExplicit>, int>::value,
                 "direct initialization works");
  static_assert (!implicitly_converts<optional<FromIntExplicit>, int>::value,
                 "M16: but not implicit");
  optional<FromIntExplicit> direct (3);
  EXPECT_EQ (direct->value, 3);
  static_assert (!std::is_constructible<optional<int>, std::string>::value,
                 "no constructor when T is not constructible from U");
  static_assert (!implicitly_converts<optional<int>, std::string>::value, "");
}

TEST (LumexOptionalConvertingTest,
      GivenInPlaceOrOptionalArgument_WhenValueCtor_ThenNotUsed)
{
  // U = in_place_t must not select the value constructor.
  static_assert (
      !std::is_constructible<optional<int>, in_place_t, std::string>::value,
      "M34: in_place requires T constructible from the arguments");
  static_assert (std::is_constructible<optional<int>, in_place_t, int>::value,
                 "");
  static_assert (std::is_constructible<optional<std::string>, in_place_t,
                                       std::size_t, char>::value,
                 "");
  static_assert (!std::is_constructible<optional<FromIntExplicit>, in_place_t,
                                        std::string>::value,
                 "M34");
  static_assert (
      std::is_constructible<optional<WithListCtor>, in_place_t,
                            std::initializer_list<int>, char>::value,
      "initializer_list in-place");
  static_assert (!std::is_constructible<optional<int>, in_place_t,
                                        std::initializer_list<int>>::value,
                 "M34: initializer_list in-place constraint");
  optional<WithListCtor> listed (in_place, { 1, 2, 3 }, 'q');
  EXPECT_EQ (listed->size, static_cast<std::size_t> (3));
  EXPECT_EQ (listed->tag, 'q');
}

// ---- from optional<U> (M17, M32) ----

TEST (LumexOptionalConvertingTest,
      GivenOptionalOfConvertible_WhenConverted_ThenImplicitAndCorrect)
{
  optional<int> const full (4);
  optional<int> const none;
  optional<long> widened = full;
  optional<long> widened_none = none;
  ASSERT_TRUE (widened.has_value ());
  EXPECT_EQ (*widened, 4L);
  EXPECT_FALSE (widened_none.has_value ());
  optional<std::string> from_literal_opt = optional<char const *> ("text");
  ASSERT_TRUE (from_literal_opt.has_value ());
  EXPECT_EQ (*from_literal_opt, "text");
  static_assert (
      implicitly_converts<optional<long>, optional<int> const &>::value,
      "M15: implicit from optional<U> const &");
  static_assert (implicitly_converts<optional<long>, optional<int>>::value,
                 "M15: implicit from optional<U> &&");
}

TEST (LumexOptionalConvertingTest,
      GivenOptionalOfExplicitlyConvertible_WhenTraits_ThenExplicit)
{
  static_assert (std::is_constructible<optional<FromIntExplicit>,
                                       optional<int> const &>::value,
                 "constructible from optional<int> const &");
  static_assert (!implicitly_converts<optional<FromIntExplicit>,
                                      optional<int> const &>::value,
                 "M16: explicit from optional<U> const &");
  static_assert (
      std::is_constructible<optional<FromIntExplicit>, optional<int>>::value,
      "");
  static_assert (
      !implicitly_converts<optional<FromIntExplicit>, optional<int>>::value,
      "M16: explicit from optional<U> &&");
  optional<int> const source (6);
  optional<FromIntExplicit> built (source);
  ASSERT_TRUE (built.has_value ());
  EXPECT_EQ (built->value, 6);
  optional<FromIntExplicit> from_empty ((optional<int> ()));
  EXPECT_FALSE (from_empty.has_value ());
}

TEST (LumexOptionalConvertingTest,
      GivenRvalueOptional_WhenConverted_ThenValueMoved)
{
  optional<std::string> source (std::string (40, 'm'));
  optional<std::string> same_type (std::move (source));
  (void)same_type;
  optional<char const *> pointer_source ("pointer");
  optional<std::string> converted (std::move (pointer_source));
  ASSERT_TRUE (converted.has_value ());
  EXPECT_EQ (*converted, "pointer");
  EXPECT_TRUE (pointer_source.has_value ()) << "the source stays engaged";

  optional<std::unique_ptr<int>> owner (std::unique_ptr<int> (new int (9)));
  optional<std::unique_ptr<int const>> const_owner (std::move (owner));
  ASSERT_TRUE (const_owner.has_value ());
  EXPECT_EQ (**const_owner, 9);
  static_assert (
      !std::is_constructible<optional<std::unique_ptr<int const>>,
                             optional<std::unique_ptr<int>> const &>::value,
      "a move-only value cannot be copied from a const optional");
}

// M17: converts-from-any-cvref is not applied: T constructible from
// optional<U> would be converted through the value instead.
TEST (LumexOptionalConvertingTest,
      GivenTConstructibleFromOptionalU_WhenConvertingCtor_ThenNotUsed)
{
  optional<int> const source (1);
  optional<FromOptionalInt> built (source);
  ASSERT_TRUE (built.has_value ());
  EXPECT_TRUE (built->swallowed)
      << "T is built from the whole optional<int>, not from its value";
  optional<int> const none;
  optional<FromOptionalInt> from_none (none);
  ASSERT_TRUE (from_none.has_value ())
      << "the value constructor holds T built from the empty optional";
}

// M32: for T = bool an optional<int> argument is converted by state, not by
// the explicit operator bool.
TEST (LumexOptionalConvertingTest,
      GivenBoolT_WhenOptionalOfOtherType_ThenStateIsConverted)
{
  // Non-const lvalues: without the bool rule the value constructor, which
  // binds a non-const lvalue better, would build a bool from the operator bool
  // of the whole optional and the result would always be engaged.
  optional<int> none;
  optional<bool> from_none (none);
  EXPECT_FALSE (from_none.has_value ()) << "M32";
  optional<int> zero (0);
  optional<bool> from_zero (zero);
  ASSERT_TRUE (from_zero.has_value ());
  EXPECT_FALSE (*from_zero);
  optional<bool> from_optional_bool_value = optional<bool> (true);
  EXPECT_TRUE (*from_optional_bool_value);
}

// ---- assignment from a value (M18, M19) ----

// M19: assignment from a value rebuilds the engaged value.
TEST (LumexOptionalConvertingTest,
      GivenEngaged_WhenAssignedConvertibleValue_ThenAssignedNotRebuilt)
{
  optional<Counting> value (in_place);
  *value = 0;
  int const before = value->assigned;
  value = 5;
  EXPECT_EQ (value->assigned, before + 1);
  EXPECT_EQ (value->constructed, 0);

  optional<Counting> empty;
  empty = 5;
  ASSERT_TRUE (empty.has_value ());
  EXPECT_EQ (empty->constructed, 1) << "an empty optional constructs";
  EXPECT_EQ (empty->assigned, 0);
}

TEST (LumexOptionalConvertingTest,
      GivenString_WhenAssignedLiteral_ThenCompiles)
{
  optional<std::string> value;
  value = "lit";
  ASSERT_TRUE (value.has_value ());
  EXPECT_EQ (*value, "lit");
  value = "other";
  EXPECT_EQ (*value, "other");
  value = std::string ("moved");
  EXPECT_EQ (*value, "moved");
  value = nullopt;
  EXPECT_FALSE (value.has_value ());
  value = {};
  EXPECT_FALSE (value.has_value ());
}

// M18: the constraints is_assignable / is_constructible are dropped.
TEST (LumexOptionalConvertingTest,
      GivenTraits_WhenAssignFromValue_ThenConstrained)
{
  static_assert (
      std::is_assignable<optional<std::string> &, char const *>::value,
      "assign from a literal");
  static_assert (!std::is_assignable<optional<int> &, std::string>::value,
                 "M18: not constructible from U");
  static_assert (std::is_assignable<optional<long> &, int>::value, "");
  static_assert (!std::is_assignable<optional<FromIntExplicit> &, int>::value
                     == !std::is_assignable<FromIntExplicit &, int>::value,
                 "M18: not assignable from U without T's assignment");
  static_assert (std::is_assignable<optional<int> &, int>::value, "");
  SUCCEED ();
}

TEST (LumexOptionalConvertingTest, GivenOptionalU_WhenAssigned_ThenByState)
{
  optional<long> target;
  optional<int> const full (3);
  optional<int> const none;
  target = full;
  ASSERT_TRUE (target.has_value ());
  EXPECT_EQ (*target, 3L);
  target = optional<int> (8);
  EXPECT_EQ (*target, 8L);
  target = none;
  EXPECT_FALSE (target.has_value ());
  target = optional<int> ();
  EXPECT_FALSE (target.has_value ());

  optional<Counting> counted (in_place);
  optional<int> const source (4);
  counted = source;
  EXPECT_EQ (counted->assigned, 1) << "both engaged: assign";
  optional<Counting> empty_counted;
  empty_counted = source;
  EXPECT_EQ (empty_counted->constructed, 1) << "target empty: construct";
}

TEST (LumexOptionalConvertingTest,
      GivenTraits_WhenAssignFromOptionalU_ThenConstrained)
{
  static_assert (
      std::is_assignable<optional<long> &, optional<int> const &>::value, "");
  static_assert (std::is_assignable<optional<long> &, optional<int>>::value,
                 "");
  static_assert (
      !std::is_assignable<optional<int> &, optional<std::string>>::value,
      "M18: not constructible");
  static_assert (
      !std::is_assignable<optional<std::unique_ptr<int const>> &,
                          optional<std::unique_ptr<int>> const &>::value,
      "a move-only value is not assigned from a const optional");
  static_assert (std::is_assignable<optional<std::unique_ptr<int const>> &,
                                    optional<std::unique_ptr<int>>>::value,
                 "");
  SUCCEED ();
}

TEST (LumexOptionalConvertingTest,
      GivenScalar_WhenAssignedSameType_ThenStillWorks)
{
  optional<int> value;
  value = 4;
  EXPECT_EQ (*value, 4);
  value = 5;
  EXPECT_EQ (*value, 5);
  optional<int *> pointer;
  int target = 0;
  pointer = &target;
  EXPECT_EQ (*pointer, &target);
  pointer = nullptr;
  ASSERT_TRUE (pointer.has_value ());
  EXPECT_EQ (*pointer, nullptr);
}
