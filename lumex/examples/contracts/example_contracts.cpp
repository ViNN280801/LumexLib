#include <cstdio>
#include <iostream>
#include <string>
#include <type_traits>

#include "lumex/core/contracts/LumexContracts"

// The names of the module live in lumex::core::contracts; only the
// LUMEX_CONTRACT_ASSERT* and LUMEX_CONTRACTS_* macros are global.
namespace contracts = lumex::core::contracts;

namespace
{
// The last violation a handler saw.
struct last_violation_t
{
  int count = 0;
  std::string comment;
  std::string function;
  unsigned line = 0;
  contracts::evaluation_semantic semantic
      = contracts::evaluation_semantic::ignore;
};

last_violation_t g_last;

// A handler is a plain function: it receives a contract_violation.
void
remember (contracts::contract_violation const &violation)
{
  ++g_last.count;
  g_last.comment = violation.comment ();
  g_last.function = violation.location ().function_name ();
  g_last.line = violation.location ().line ();
  g_last.semantic = violation.semantic ();
}

struct violation_error
{
  std::string comment;
};

// A handler may throw: the exception leaves the violated assertion.
void
throw_it (contracts::contract_violation const &violation)
{
  throw violation_error{ violation.comment () };
}

int
divide (int numerator, int denominator)
{
  // Where C++26 writes contract_assert (denominator != 0);
  LUMEX_CONTRACT_ASSERT (denominator != 0);
  return numerator / denominator;
}
} // namespace

int
main ()
{
  std::cout << "=== contracts: LUMEX_CONTRACT_ASSERT from C++11 ===\n\n";

  std::cout << "--- 1. The semantic in force ---\n";
  // Chosen by the CMake option LUMEX_CONTRACTS_SEMANTIC (default ENFORCE) or,
  // per translation unit, by defining LUMEX_CONTRACTS_SEMANTIC before the
  // include. 1 ignore, 2 observe, 3 enforce, 4 quick_enforce, 5 the
  // compiler's own contract_assert.
  std::cout << "semantic number: " << LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC
            << "\nthe compiler has contract_assert (P2900): "
            << (LUMEX_CONTRACTS_HAS_NATIVE ? "yes" : "no") << '\n';

  std::cout << "\n--- 2. A holding assertion costs nothing visible ---\n";
  std::cout << "divide (12, 4) = " << divide (12, 4) << '\n';

  std::cout << "\n--- 3. A handler sees the violation ---\n";
  // The explicit-semantic macros pick the behavior whatever the build chose.
  // observe: report to the handler, then carry on.
  contracts::set_violation_handler (&remember);
  int evaluations = 0;
  LUMEX_CONTRACT_ASSERT_OBSERVE (++evaluations < 0);
  std::cout << "the predicate ran " << evaluations << " time(s)\n"
            << "violations seen: " << g_last.count << '\n'
            << "comment:         " << g_last.comment << '\n'
            << "function:        " << g_last.function << '\n'
            << "line:            " << g_last.line << '\n'
            << "semantic:        " << contracts::to_string (g_last.semantic)
            << '\n';

  std::cout << "\n--- 4. ignore does not evaluate the predicate ---\n";
  evaluations = 0;
  LUMEX_CONTRACT_ASSERT_IGNORE (++evaluations < 0);
  std::cout << "evaluations: " << evaluations << " (still type-checked)\n";

  std::cout
      << "\n--- 5. Commas of template arguments need no parentheses ---\n";
  LUMEX_CONTRACT_ASSERT_OBSERVE (std::is_same<int, long>::value);
  std::cout << "comment: " << g_last.comment << '\n';

  std::cout << "\n--- 6. A handler that throws turns a violation into an "
               "exception ---\n";
  {
    contracts::scoped_violation_handler const scope (&throw_it);
    try
      {
        divide (1, 0);
      }
    catch (violation_error const &error)
      {
        std::cout << "caught: " << error.comment << '\n';
      }
  } // the previous handler is back
  std::cout << "previous handler restored: "
            << (contracts::get_violation_handler () == &remember ? "yes"
                                                                 : "no")
            << '\n';

  std::cout << "\n--- 7. The default handler prints one line ---\n";
  contracts::set_violation_handler (nullptr);
  {
    // Not a failed assertion of this program: a report made by hand.
    contracts::contract_violation const violation (
        "demo > 0", contracts::assertion_kind::assert,
        contracts::evaluation_semantic::observe,
        contracts::detection_mode::predicate_false,
        contracts::source_location ("example.cpp", "int demo ()", 1, 1));
    contracts::invoke_default_violation_handler (violation);
  }

  std::cout << "\nOK\n";
  return 0;
}
