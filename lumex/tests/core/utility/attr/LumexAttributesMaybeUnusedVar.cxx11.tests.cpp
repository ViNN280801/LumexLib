// LumexAttributesMaybeUnusedVar.cxx11.tests.cpp
// LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR of LumexAttributes.hpp in every standard.
// The macro discards a value on purpose. On GCC below C++17 it is a comma
// with an overloaded operator, so that a result of a warn_unused_result
// function counts as used (GCC reports a result that is only cast to void);
// elsewhere it is a cast to void. Either way it is a void expression that
// evaluates its argument once, whatever the argument is. The warning itself is
// checked by cmake.hygiene_compile_checks, which compiles with warnings as
// errors; the tests here pin what the expression does at run time.
#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/utility/attr/LumexAttributes.hpp"

namespace
{
int
count_call (int &calls)
{
  return ++calls;
}

LUMEX_ATTRIBUTE_NODISCARD ("the result is the point of the call")
int
count_nodiscard_call (int &calls)
{
  return ++calls;
}

LUMEX_ATTRIBUTE_NODISCARD ("the result is the point of the call")
std::string
make_text_counting (int &calls)
{
  ++calls;
  return std::string (3, 'x');
}

LUMEX_ATTRIBUTE_NODISCARD ("the result is the point of the call")
std::unique_ptr<int>
make_owner_counting (int &calls)
{
  ++calls;
  return std::unique_ptr<int> (new int (7));
}

void
count_void_call (int &calls)
{
  ++calls;
}

int
add_two (int first, int second)
{
  return first + second;
}

struct bit_fields_t
{
  unsigned flag : 1;
  unsigned rest : 7;
};

enum class colour_t
{
  red,
  green
};

union number_t
{
  int integer;
  float real;
};

struct movable_only_t
{
  movable_only_t () : value (3) {}
  movable_only_t (movable_only_t &&) = default;
  movable_only_t (movable_only_t const &) = delete;
  int value;
  int
  member () const
  {
    return value;
  }
};

// The idiom that expands a pack for its side effects: an array of unknown
// bound in a template, which no reference can bind to before C++20.
template <typename... Args>
std::size_t
count_with_array (Args &&...args)
{
  int expanded[] = { 0, (static_cast<void> (args), 0)... };
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (expanded);
  return sizeof...(Args);
}
} // namespace

TEST (LumexAttributesMaybeUnusedVarTest,
      GivenACallWithSideEffects_WhenDiscarded_ThenItRunsOnce)
{
  // Arrange
  int calls = 0;

  // Act
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (count_call (calls));

  // Assert
  EXPECT_EQ (1, calls);
}

TEST (LumexAttributesMaybeUnusedVarTest,
      GivenANodiscardResult_WhenDiscarded_ThenItRunsOnce)
{
  // Arrange
  int calls = 0;

  // Act
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (count_nodiscard_call (calls));
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (make_text_counting (calls));
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (make_owner_counting (calls));

  // Assert
  EXPECT_EQ (3, calls);
}

TEST (LumexAttributesMaybeUnusedVarTest,
      GivenAVoidExpression_WhenDiscarded_ThenItRunsOnce)
{
  // Arrange
  int calls = 0;

  // Act
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (count_void_call (calls));

  // Assert
  EXPECT_EQ (1, calls);
}

TEST (LumexAttributesMaybeUnusedVarTest,
      GivenCommasInTheArgument_WhenDiscarded_ThenTheyStayOneArgument)
{
  // Arrange
  int calls = 0;

  // Act: template arguments and call arguments carry commas of their own, and
  // a comma operator in parentheses evaluates both operands.
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (std::pair<int, int> (add_two (1, 2), 4));
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR ((count_call (calls), count_call (calls)));

  // Assert
  EXPECT_EQ (2, calls);
}

TEST (LumexAttributesMaybeUnusedVarTest,
      GivenNamesAndOtherLvalues_WhenDiscarded_ThenTheyAreLeftAsTheyWere)
{
  // Arrange
  int variable = 5;
  int const constant = 6;
  int array[3] = { 1, 2, 3 };
  std::string text ("keep");
  bit_fields_t bits = { 1U, 5U };

  // Act
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (variable);
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (constant);
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (array);
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (array[1]);
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (text);
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (add_two);
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (bits.flag);
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (&variable);
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (sizeof (variable));
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (0);

  // Assert
  EXPECT_EQ (5, variable);
  EXPECT_EQ (3, array[2]);
  EXPECT_EQ ("keep", text);
  EXPECT_EQ (1U, bits.flag);
  EXPECT_EQ (5U, bits.rest);
}

TEST (LumexAttributesMaybeUnusedVarTest,
      GivenOtherKindsOfValues_WhenDiscarded_ThenEachOneCompiles)
{
  // Arrange
  colour_t colour = colour_t::green;
  number_t number;
  number.integer = 4;
  movable_only_t movable;
  volatile int volatile_value = 8;
  int (movable_only_t::*member_function) () const = &movable_only_t::member;
  int movable_only_t::*member_object = &movable_only_t::value;
  void (*function_pointer) (int &) = &count_void_call;

  // Act: an enumerator, a union, a class that cannot be copied (an lvalue and
  // a temporary), a volatile object, pointers to members and to a function,
  // and the null pointer constant.
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (colour);
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (colour_t::red);
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (number);
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (movable);
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (movable_only_t ());
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (volatile_value);
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (member_function);
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (member_object);
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (function_pointer);
  LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (nullptr);

  // Assert
  EXPECT_EQ (3, movable.value);
  EXPECT_EQ (8, volatile_value);
  EXPECT_EQ (4, number.integer);
}

TEST (LumexAttributesMaybeUnusedVarTest,
      GivenAnArrayOfUnknownBoundInATemplate_WhenDiscarded_ThenItCompiles)
{
  EXPECT_EQ (0U, count_with_array ());
  EXPECT_EQ (3U, count_with_array (1, 2.5, "text"));
}

TEST (LumexAttributesMaybeUnusedVarTest,
      GivenTheMacro_WhenUsedAsAnExpression_ThenItIsVoid)
{
  // The macro is a statement in the library; as an operand it must still be
  // void, as the cast was.
  static_assert (
      std::is_void<decltype (LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (1))>::value,
      "LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR is a void expression");
  static_assert (std::is_void<decltype (LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (
                     count_void_call))>::value,
                 "LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR is a void expression");

  // Unevaluated, the call inside does not run.
  int calls = 0;
  static_assert (std::is_void<decltype (LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (
                     count_nodiscard_call (calls)))>::value,
                 "LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR is a void expression");
  EXPECT_EQ (0, calls);
}

TEST (LumexAttributesMaybeUnusedVarTest,
      GivenAStatementContext_WhenTheMacroIsTheOnlyStatement_ThenItCompiles)
{
  // Arrange
  int calls = 0;
  bool const flag = true;

  // Act: as the body of an if / else without braces and in a for loop head.
  if (flag)
    LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (count_nodiscard_call (calls));
  else
    LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (count_nodiscard_call (calls));
  for (LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (count_call (calls)); calls < 3;
       LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (count_call (calls)))
    {
    }

  // Assert
  EXPECT_EQ (3, calls);
}
