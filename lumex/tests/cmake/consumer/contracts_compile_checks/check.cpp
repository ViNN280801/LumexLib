// Compile checks of the contracts module, built by
// cmake.contracts_compile_checks. Exactly one of LUMEX_CONTRACTS_GOOD_CASE or
// LUMEX_CONTRACTS_BAD_CASE=<n> is defined; the fixture compiles with warnings
// as errors. The preamble is valid code; a bad case adds one statement that
// the module rejects. LUMEX_CONTRACTS_GOOD_CASE compiles with every semantic
// the fixture names (-DLUMEX_CONTRACTS_SEMANTIC=...), with and without
// LUMEX_CONTRACTS_CATCH_EXCEPTIONS.

#include <string>
#include <type_traits>

#include "lumex/core/contracts/LumexContracts"

namespace contracts = lumex::core::contracts;

namespace
{
struct no_bool
{
  int value;
};

struct explicit_bool
{
  bool value;
  explicit
  operator bool () const
  {
    return value;
  }
};

void
handler (contracts::contract_violation const &)
{
}

#if !LUMEX_CONTRACTS_CATCH_EXCEPTIONS
// The comma form is the C++11 shape of a constexpr function with an
// assertion; a lambda (the catch variant) is not allowed in it before C++17.
constexpr int
checked (int value)
{
  return (LUMEX_CONTRACT_ASSERT_ENFORCE (value >= 0), value);
}

constexpr int
observed (int value)
{
  return (LUMEX_CONTRACT_ASSERT_OBSERVE (value >= 0), value);
}

constexpr int
quick (int value)
{
  return (LUMEX_CONTRACT_ASSERT_QUICK_ENFORCE (value >= 0), value);
}

constexpr int
ignored (int value)
{
  return (LUMEX_CONTRACT_ASSERT_IGNORE (value >= 0), value);
}

constexpr int
plain (int value)
{
  return (LUMEX_CONTRACT_ASSERT (value >= 0), value);
}
#endif

int
valid_uses (int value)
{
  LUMEX_CONTRACT_ASSERT (value > 0);
  LUMEX_CONTRACT_ASSERT (explicit_bool{ true });
  LUMEX_CONTRACT_ASSERT (std::is_same<int, int>::value);
  LUMEX_CONTRACT_ASSERT_OBSERVE (value < 100000);
  LUMEX_CONTRACT_ASSERT_ENFORCE (value > -5000);
  LUMEX_CONTRACT_ASSERT_IGNORE (value < 1000);
  LUMEX_CONTRACT_ASSERT_QUICK_ENFORCE (value > -1000);
  if (value > 5)
    LUMEX_CONTRACT_ASSERT (value > 2);
  else
    LUMEX_CONTRACT_ASSERT (value < 100);
  contracts::set_violation_handler (&handler);
  contracts::scoped_violation_handler const scope (nullptr);
  contracts::source_location const here ("f.cpp", "g", 1, 2);
  contracts::contract_violation const violation (
      "p", contracts::assertion_kind::assert,
      contracts::evaluation_semantic::observe,
      contracts::detection_mode::predicate_false, here);
  return static_cast<int> (violation.location ().line ());
}
} // namespace

#if defined(LUMEX_CONTRACTS_GOOD_CASE)
int
use_valid (int value)
{
  return valid_uses (value);
}
#if !LUMEX_CONTRACTS_CATCH_EXCEPTIONS
static_assert (checked (1) + observed (2) + quick (3) + ignored (-4)
                       + plain (5)
                   == 7,
               "constant evaluation of holding assertions");
#endif

#elif LUMEX_CONTRACTS_BAD_CASE == 1
// A predicate that does not convert to bool (enforce).
void
bad ()
{
  LUMEX_CONTRACT_ASSERT_ENFORCE (no_bool{ 1 });
}
#elif LUMEX_CONTRACTS_BAD_CASE == 2
// The same with observe.
void
bad ()
{
  LUMEX_CONTRACT_ASSERT_OBSERVE (no_bool{ 1 });
}
#elif LUMEX_CONTRACTS_BAD_CASE == 3
// The same with quick_enforce.
void
bad ()
{
  LUMEX_CONTRACT_ASSERT_QUICK_ENFORCE (no_bool{ 1 });
}
#elif LUMEX_CONTRACTS_BAD_CASE == 4
// Ignore does not evaluate the predicate but still type-checks it.
void
bad ()
{
  LUMEX_CONTRACT_ASSERT_IGNORE (no_bool{ 1 });
}
#elif LUMEX_CONTRACTS_BAD_CASE == 5
// The plain macro too.
void
bad ()
{
  LUMEX_CONTRACT_ASSERT (std::string ("x"));
}
#elif LUMEX_CONTRACTS_BAD_CASE == 6
// An assertion needs a predicate.
void
bad ()
{
  LUMEX_CONTRACT_ASSERT_ENFORCE ();
}
#elif LUMEX_CONTRACTS_BAD_CASE == 7
// A statement, not a declaration: not at namespace scope.
LUMEX_CONTRACT_ASSERT_ENFORCE (true);
#elif LUMEX_CONTRACTS_BAD_CASE == 8
// A handler takes the violation by const reference, nothing else.
void
bad ()
{
  contracts::set_violation_handler (+[] (int) {});
}
#elif LUMEX_CONTRACTS_BAD_CASE == 9
// A handler is a function pointer: a lambda that captures is not one.
void
bad ()
{
  int count = 0;
  contracts::set_violation_handler (
      [&count] (contracts::contract_violation const &) { ++count; });
}
#elif LUMEX_CONTRACTS_BAD_CASE == 10
// A violation cannot be made from a number.
void
bad ()
{
  contracts::contract_violation const violation (42);
  (void)violation;
}
#elif LUMEX_CONTRACTS_BAD_CASE == 11
// The enumerations are scoped: no implicit conversion to int.
void
bad ()
{
  int const kind = contracts::assertion_kind::assert;
  (void)kind;
}
#elif LUMEX_CONTRACTS_BAD_CASE == 12
// ... and none from int.
void
bad ()
{
  contracts::evaluation_semantic const semantic = 3;
  (void)semantic;
}
#elif LUMEX_CONTRACTS_BAD_CASE == 13
// A scoped handler is not copied.
void
bad ()
{
  contracts::scoped_violation_handler const first (nullptr);
  contracts::scoped_violation_handler const second = first;
  (void)second;
}
#elif LUMEX_CONTRACTS_BAD_CASE == 14
// A failing enforce in constant evaluation does not compile.
#if LUMEX_CONTRACTS_CATCH_EXCEPTIONS
#error "constant evaluation: not part of this variant"
#endif
constexpr int bad_value = checked (-1);
#elif LUMEX_CONTRACTS_BAD_CASE == 15
// ... nor does a failing observe (it would call the handler).
#if LUMEX_CONTRACTS_CATCH_EXCEPTIONS
#error "constant evaluation: not part of this variant"
#endif
constexpr int bad_value = observed (-1);
#elif LUMEX_CONTRACTS_BAD_CASE == 16
// ... nor quick_enforce.
#if LUMEX_CONTRACTS_CATCH_EXCEPTIONS
#error "constant evaluation: not part of this variant"
#endif
constexpr int bad_value = quick (-1);
#elif LUMEX_CONTRACTS_BAD_CASE == 17
// The source_location of the module is a different type from an int.
void
bad ()
{
  contracts::source_location const where = 7;
  (void)where;
}
#elif LUMEX_CONTRACTS_BAD_CASE == 18
// The detection mode of a violation is read, not assigned.
void
bad ()
{
  contracts::contract_violation violation (
      "p", contracts::assertion_kind::assert,
      contracts::evaluation_semantic::observe,
      contracts::detection_mode::predicate_false,
      contracts::source_location ());
  violation.detection_mode ()
      = contracts::detection_mode::evaluation_exception;
}
#else
#error "LUMEX_CONTRACTS_BAD_CASE is not one of the cases"
#endif
