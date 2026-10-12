// LUMEX_CONTRACT_ASSERT: the table of the four emulated semantics (how often
// the predicate is evaluated, whether the handler is called, whether the
// program continues), the contents of the violation, the text of the
// predicate, commas in template arguments, statement forms.

#include <cstdint>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/contracts/LumexContracts"
#include "lumex/tests/core/contracts/LumexContractsTestSupport.hpp"

namespace
{
using namespace contracts_test;

class ContractsAssert : public fixture
{
protected:
  void
  SetUp () override
  {
    fixture::SetUp ();
    contracts::set_violation_handler (&recording_handler);
  }
};

// --- true predicate: evaluated once (never with ignore), nothing reported ---

TEST_F (ContractsAssert, IgnoreNeverEvaluatesThePredicate)
{
  probe holds{ 0 };
  LUMEX_CONTRACT_ASSERT_IGNORE (holds (true));
  LUMEX_CONTRACT_ASSERT_IGNORE (holds (false));
  EXPECT_EQ (holds.evaluations, 0);
  EXPECT_EQ (record ().calls, 0);
}

TEST_F (ContractsAssert, ObserveEvaluatesOnceAndStaysQuietWhenTrue)
{
  probe holds{ 0 };
  LUMEX_CONTRACT_ASSERT_OBSERVE (holds (true));
  EXPECT_EQ (holds.evaluations, 1);
  EXPECT_EQ (record ().calls, 0);
}

TEST_F (ContractsAssert, EnforceEvaluatesOnceAndStaysQuietWhenTrue)
{
  probe holds{ 0 };
  LUMEX_CONTRACT_ASSERT_ENFORCE (holds (true));
  EXPECT_EQ (holds.evaluations, 1);
  EXPECT_EQ (record ().calls, 0);
}

TEST_F (ContractsAssert, QuickEnforceEvaluatesOnceAndStaysQuietWhenTrue)
{
  probe holds{ 0 };
  LUMEX_CONTRACT_ASSERT_QUICK_ENFORCE (holds (true));
  EXPECT_EQ (holds.evaluations, 1);
  EXPECT_EQ (record ().calls, 0);
}

// --- false predicate -------------------------------------------------------

TEST_F (ContractsAssert, IgnoreReportsNothingWhenFalse)
{
  bool reached = false;
  LUMEX_CONTRACT_ASSERT_IGNORE (false);
  reached = true;
  EXPECT_TRUE (reached);
  EXPECT_EQ (record ().calls, 0);
}

TEST_F (ContractsAssert, ObserveReportsAndContinues)
{
  probe holds{ 0 };
  bool reached = false;
  LUMEX_CONTRACT_ASSERT_OBSERVE (holds (false));
  reached = true;
  EXPECT_TRUE (reached);
  EXPECT_EQ (holds.evaluations, 1);
  EXPECT_EQ (record ().calls, 1);
  EXPECT_EQ (record ().semantic, contracts::evaluation_semantic::observe);
  EXPECT_FALSE (record ().terminating);
}

TEST_F (ContractsAssert, ObserveReportsEveryViolation)
{
  for (int count = 0; count < 5; ++count)
    LUMEX_CONTRACT_ASSERT_OBSERVE (count < 0);
  EXPECT_EQ (record ().calls, 5);
}

TEST_F (ContractsAssert, EnforceCallsTheHandlerAndLetsItsExceptionOut)
{
  // With a handler that returns, enforce aborts (see the death tests). A
  // handler that throws is how a program turns a violation into an exception.
  contracts::set_violation_handler (&throwing_handler);
  probe holds{ 0 };
  bool reached = false;
  EXPECT_THROW (
      {
        LUMEX_CONTRACT_ASSERT_ENFORCE (holds (false));
        reached = true;
      },
      violation_error);
  EXPECT_FALSE (reached);
  EXPECT_EQ (holds.evaluations, 1);
  EXPECT_EQ (record ().calls, 1);
  EXPECT_EQ (record ().semantic, contracts::evaluation_semantic::enforce);
  EXPECT_TRUE (record ().terminating);
}

// --- the violation ----------------------------------------------------------

std::uint_least32_t
report_a_false_predicate ()
{
  int const value = 3;
  std::uint_least32_t const before = __LINE__;
  LUMEX_CONTRACT_ASSERT_OBSERVE (value == 4);
  return before + 1;
}

TEST_F (ContractsAssert, ViolationNamesThePredicateAndThePlace)
{
  std::uint_least32_t const line = report_a_false_predicate ();
  ASSERT_EQ (record ().calls, 1);
  EXPECT_EQ (record ().comment, "value == 4");
  EXPECT_EQ (record ().kind, contracts::assertion_kind::assert);
  EXPECT_EQ (record ().mode, contracts::detection_mode::predicate_false);
  EXPECT_TRUE (
      ends_with (record ().file, "LumexContractsAssert.cxx11.tests.cpp"))
      << record ().file;
  EXPECT_TRUE (contains (record ().function, "report_a_false_predicate"))
      << record ().function;
  EXPECT_EQ (record ().line, line);
#if LUMEX_CONTRACTS_HAS_BUILTIN_COLUMN
  EXPECT_GT (record ().column, 0u);
#endif
}

#define CONTRACTS_TEST_LIMIT 4
#define CONTRACTS_TEST_PLUS(a, b) ((a) + (b))

TEST_F (ContractsAssert, CommentIsTheTextAsWrittenNotMacroExpanded)
{
  int const value = 9;
  LUMEX_CONTRACT_ASSERT_OBSERVE (value < CONTRACTS_TEST_LIMIT);
  EXPECT_EQ (record ().comment, "value < CONTRACTS_TEST_LIMIT");
  LUMEX_CONTRACT_ASSERT_OBSERVE (CONTRACTS_TEST_PLUS (value, 1) == 0);
  EXPECT_EQ (record ().comment, "CONTRACTS_TEST_PLUS (value, 1) == 0");
  EXPECT_EQ (record ().calls, 2);
}

TEST_F (ContractsAssert, CommasOfTemplateArgumentsStayInThePredicate)
{
  LUMEX_CONTRACT_ASSERT_OBSERVE (std::is_same<int, long>::value);
  EXPECT_EQ (record ().calls, 1);
  EXPECT_EQ (record ().comment, "std::is_same<int, long>::value");
  LUMEX_CONTRACT_ASSERT_OBSERVE (std::is_same<int, int>::value);
  EXPECT_EQ (record ().calls, 1);
}

TEST_F (ContractsAssert, ACommaOperatorIsOnePredicate)
{
  int counter = 0;
  LUMEX_CONTRACT_ASSERT_OBSERVE (++counter, counter == 5);
  EXPECT_EQ (counter, 1);
  EXPECT_EQ (record ().calls, 1);
}

// --- contextual conversion to bool ------------------------------------------

struct explicit_bool
{
  bool value;
  explicit
  operator bool () const
  {
    return value;
  }
};

TEST_F (ContractsAssert, AnExplicitBoolConversionIsEnough)
{
  LUMEX_CONTRACT_ASSERT_OBSERVE (explicit_bool{ true });
  EXPECT_EQ (record ().calls, 0);
  LUMEX_CONTRACT_ASSERT_OBSERVE (explicit_bool{ false });
  EXPECT_EQ (record ().calls, 1);
}

TEST_F (ContractsAssert, PointersAndIntegersConvert)
{
  int object = 0;
  int *pointer = &object;
  int *null_pointer = nullptr;
  LUMEX_CONTRACT_ASSERT_OBSERVE (pointer);
  LUMEX_CONTRACT_ASSERT_OBSERVE (object + 1);
  EXPECT_EQ (record ().calls, 0);
  LUMEX_CONTRACT_ASSERT_OBSERVE (null_pointer);
  LUMEX_CONTRACT_ASSERT_OBSERVE (object);
  EXPECT_EQ (record ().calls, 2);
}

// --- statement forms --------------------------------------------------------

int
branch (bool condition, bool holds)
{
  // No braces: the macro is one expression statement.
  if (condition)
    LUMEX_CONTRACT_ASSERT_OBSERVE (holds);
  else
    LUMEX_CONTRACT_ASSERT_OBSERVE (!holds);
  return record ().calls;
}

TEST_F (ContractsAssert, WorksInIfElseWithoutBraces)
{
  EXPECT_EQ (branch (true, true), 0);
  EXPECT_EQ (branch (true, false), 1);
  EXPECT_EQ (branch (false, true), 2);
  EXPECT_EQ (branch (false, false), 2);
}

TEST_F (ContractsAssert, WorksInALoopBodyAndALambda)
{
  int sum = 0;
  for (int index = 0; index < 3; ++index)
    LUMEX_CONTRACT_ASSERT_OBSERVE (index >= 0), sum += index;
  EXPECT_EQ (sum, 3);
  auto check = [] (int value) { LUMEX_CONTRACT_ASSERT_OBSERVE (value > 0); };
  check (1);
  EXPECT_EQ (record ().calls, 0);
  check (-1);
  EXPECT_EQ (record ().calls, 1);
  EXPECT_TRUE (contains (record ().function, "operator()")
               || contains (record ().function, "lambda")
               || contains (record ().function, "ContractsAssert"))
      << record ().function;
}

TEST_F (ContractsAssert, TheExpressionIsOfTypeVoid)
{
  EXPECT_TRUE (
      (std::is_void<decltype (LUMEX_CONTRACT_ASSERT_OBSERVE (true))>::value));
  EXPECT_TRUE (
      (std::is_void<decltype (LUMEX_CONTRACT_ASSERT_ENFORCE (true))>::value));
  EXPECT_TRUE ((std::is_void<decltype (LUMEX_CONTRACT_ASSERT_QUICK_ENFORCE (
                    true))>::value));
  EXPECT_TRUE (
      (std::is_void<decltype (LUMEX_CONTRACT_ASSERT_IGNORE (true))>::value));
}

// --- the handler is read when the violation happens ------------------------

TEST_F (ContractsAssert, ReportGoesToTheHandlerInstalledAtThatMoment)
{
  LUMEX_CONTRACT_ASSERT_OBSERVE (false);
  EXPECT_EQ (record ().calls, 1);
  contracts::scoped_violation_handler const scope (&throwing_handler);
  EXPECT_THROW (LUMEX_CONTRACT_ASSERT_OBSERVE (false), violation_error);
  EXPECT_EQ (record ().calls, 2);
}

TEST_F (ContractsAssert, ObserveWithoutAHandlerPrintsAndContinues)
{
  contracts::set_violation_handler (nullptr);
  testing::internal::CaptureStderr ();
  LUMEX_CONTRACT_ASSERT_OBSERVE (1 + 1 == 3);
  std::string const text = testing::internal::GetCapturedStderr ();
  EXPECT_TRUE (contains (text, "1 + 1 == 3")) << text;
  EXPECT_TRUE (contains (text, "semantic: observe")) << text;
  EXPECT_TRUE (contains (text, "LumexContractsAssert.cxx11.tests.cpp"))
      << text;
}

// --- the configuration macros -----------------------------------------------

TEST (ContractsAssertConfig,
      TheBuildDefaultIsEnforceUnlessTheBuildSaysOtherwise)
{
#if defined(LUMEX_CONTRACTS_SEMANTIC)
  GTEST_SKIP () << "this translation unit overrides the semantic";
#elif defined(LUMEX_CONTRACTS_BUILD_SEMANTIC)
  // The test targets link lumex::contracts, which passes the CMake option.
  static_assert (LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC >= 1
                     && LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC <= 5,
                 "a valid semantic");
  SUCCEED ();
#else
  static_assert (LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC == 3,
                 "enforce by default");
  SUCCEED ();
#endif
}

TEST (ContractsAssertConfig,
      NativeContractAssertNeedsP2900NotTheWithdrawnContracts)
{
  // __cpp_contracts 201906L is the withdrawn contracts of the C++20 draft
  // (GCC -fcontracts): no contract_assert, so it must not count.
#if defined(__cpp_contracts) && __cpp_contracts < 202502L
  EXPECT_EQ (LUMEX_CONTRACTS_HAS_NATIVE, 0);
#elif !defined(__cpp_contracts)
  EXPECT_EQ (LUMEX_CONTRACTS_HAS_NATIVE, 0);
#else
  EXPECT_EQ (LUMEX_CONTRACTS_HAS_NATIVE, 1);
#endif
}
} // namespace
