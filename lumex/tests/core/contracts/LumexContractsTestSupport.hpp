// What the contracts tests share: a recording violation handler, a predicate
// that counts its evaluations, and a fixture that leaves the process-wide
// handler as it found it (the default one).

#ifndef LUMEX_TESTS_CORE_CONTRACTS_TEST_SUPPORT_HPP
#define LUMEX_TESTS_CORE_CONTRACTS_TEST_SUPPORT_HPP

#include <cstdint>
#include <cstring>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/contracts/LumexContracts"

namespace contracts_test
{
namespace contracts = lumex::core::contracts;

/// What the last recording handler saw.
struct record_t
{
  int calls = 0;
  std::string comment;
  contracts::assertion_kind kind = contracts::assertion_kind::pre;
  contracts::evaluation_semantic semantic
      = contracts::evaluation_semantic::ignore;
  contracts::detection_mode mode = contracts::detection_mode::predicate_false;
  bool terminating = false;
  std::string file;
  std::string function;
  std::uint_least32_t line = 0;
  std::uint_least32_t column = 0;
};

inline record_t &
record ()
{
  static record_t instance;
  return instance;
}

/// Stores the violation in `record ()` and returns.
inline void
recording_handler (contracts::contract_violation const &violation)
{
  record_t &seen = record ();
  ++seen.calls;
  seen.comment = violation.comment ();
  seen.kind = violation.kind ();
  seen.semantic = violation.semantic ();
  seen.mode = violation.detection_mode ();
  seen.terminating = violation.is_terminating ();
  seen.file = violation.location ().file_name ();
  seen.function = violation.location ().function_name ();
  seen.line = violation.location ().line ();
  seen.column = violation.location ().column ();
}

/// The exception that `throwing_handler` throws.
struct violation_error
{
  std::string comment;
};

/// Records the violation, then throws `violation_error`.
inline void
throwing_handler (contracts::contract_violation const &violation)
{
  recording_handler (violation);
  throw violation_error{ violation.comment () };
}

/// A predicate with a side effect: counts the evaluations, returns `result`.
struct probe
{
  int evaluations;
  bool
  operator() (bool result)
  {
    ++evaluations;
    return result;
  }
};

/// True if `text` contains `part`.
inline bool
contains (std::string const &text, char const *part)
{
  return text.find (part) != std::string::npos;
}

/// True if `text` ends with `tail`.
inline bool
ends_with (std::string const &text, char const *tail)
{
  std::size_t const length = std::strlen (tail);
  return text.size () >= length
         && text.compare (text.size () - length, length, tail) == 0;
}

/// Resets the record, installs no handler; restores the default handler.
class fixture : public ::testing::Test
{
protected:
  void
  SetUp () override
  {
    contracts::set_violation_handler (nullptr);
    record () = record_t ();
  }

  void
  TearDown () override
  {
    contracts::set_violation_handler (nullptr);
  }
};
} // namespace contracts_test

#endif // !LUMEX_TESTS_CORE_CONTRACTS_TEST_SUPPORT_HPP
