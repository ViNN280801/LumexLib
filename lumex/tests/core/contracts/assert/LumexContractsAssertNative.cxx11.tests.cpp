// The real contract_assert of C++26 (P2900), mocked: no installed compiler has
// it, so this translation unit says the compiler does (the feature macro
// __cpp_contracts of P2900, 202502L) and defines contract_assert as a macro
// that records its use. LUMEX_CONTRACT_ASSERT must then expand to it when the
// semantic is p2900, and only then.

#define __cpp_contracts 202502L
#define LUMEX_CONTRACTS_SEMANTIC p2900

#include <string>

#include <gtest/gtest.h>

namespace native_mock
{
struct log_t
{
  int calls = 0;
  std::string text;
  bool value = false;
};

inline log_t &
log ()
{
  static log_t instance;
  return instance;
}

inline void
contract_assert_used (char const *text, bool value)
{
  ++log ().calls;
  log ().text = text;
  log ().value = value;
}
} // namespace native_mock

// The keyword is a statement in C++26; the mock is one too.
#define contract_assert(...)                                                  \
  ::native_mock::contract_assert_used (#__VA_ARGS__,                          \
                                       static_cast<bool> ((__VA_ARGS__)))

#include "lumex/core/contracts/LumexContracts"
#include "lumex/tests/core/contracts/LumexContractsTestSupport.hpp"

namespace
{
using namespace contracts_test;

static_assert (LUMEX_CONTRACTS_HAS_NATIVE == 1, "P2900 is announced");
static_assert (LUMEX_CONTRACTS_REQUESTED_SEMANTIC == 5, "p2900 was asked");
static_assert (LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC == 5, "the native keyword");

class ContractsAssertNative : public fixture
{
protected:
  void
  SetUp () override
  {
    fixture::SetUp ();
    native_mock::log () = native_mock::log_t ();
    contracts::set_violation_handler (&recording_handler);
  }
};

TEST_F (ContractsAssertNative, PlainMacroIsTheKeyword)
{
  int const value = 5;
  LUMEX_CONTRACT_ASSERT (value > 3);
  EXPECT_EQ (native_mock::log ().calls, 1);
  EXPECT_EQ (native_mock::log ().text, "value > 3");
  EXPECT_TRUE (native_mock::log ().value);
  EXPECT_EQ (record ().calls, 0);
}

TEST_F (ContractsAssertNative, KeywordGetsTheWholePredicateWithItsCommas)
{
  LUMEX_CONTRACT_ASSERT (std::is_same<int, long>::value);
  EXPECT_EQ (native_mock::log ().calls, 1);
  EXPECT_EQ (native_mock::log ().text, "std::is_same<int, long>::value");
  EXPECT_FALSE (native_mock::log ().value);
  // The keyword reports to the compiler's handler, not to this library's.
  EXPECT_EQ (record ().calls, 0);
}

TEST_F (ContractsAssertNative, P2900MacroIsTheKeywordToo)
{
  LUMEX_CONTRACT_ASSERT_P2900 (1 == 1);
  EXPECT_EQ (native_mock::log ().calls, 1);
  EXPECT_EQ (native_mock::log ().text, "1 == 1");
}

TEST_F (ContractsAssertNative, TheEmulatedMacrosStayEmulated)
{
  LUMEX_CONTRACT_ASSERT_OBSERVE (false);
  EXPECT_EQ (native_mock::log ().calls, 0);
  EXPECT_EQ (record ().calls, 1);
  LUMEX_CONTRACT_ASSERT_IGNORE (false);
  EXPECT_EQ (record ().calls, 1);
}
} // namespace
