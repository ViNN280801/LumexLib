// The semantic of the build reaches a consumer through lumex::contracts, and
// behaves as that semantic. Exit codes: 0 all fine, 2 wrong semantic number,
// 3 ignore evaluated or reported, 4 observe wrong, 5 enforce wrong, 6 the
// override of overridden.cpp lost to the build.

#include <cstdio>

#include "lumex/core/contracts/LumexContracts"

namespace contracts = lumex::core::contracts;

namespace
{
int g_calls = 0;

struct violation_error
{
};

void
counting_handler (contracts::contract_violation const &)
{
  ++g_calls;
}

void
throwing_handler (contracts::contract_violation const &)
{
  ++g_calls;
  throw violation_error ();
}
} // namespace

int overridden_semantic ();

int
main ()
{
  int const expected_effective
      = (EXPECTED_REQUESTED == 5 && !LUMEX_CONTRACTS_HAS_NATIVE)
            ? 3
            : EXPECTED_REQUESTED;
  if (LUMEX_CONTRACTS_REQUESTED_SEMANTIC != EXPECTED_REQUESTED
      || LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC != expected_effective)
    {
      std::printf ("semantic %d (effective %d), expected %d\n",
                   LUMEX_CONTRACTS_REQUESTED_SEMANTIC,
                   LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC, expected_effective);
      return 2;
    }

  int evaluations = 0;
  switch (LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC)
    {
    case 1:
      contracts::set_violation_handler (&counting_handler);
      LUMEX_CONTRACT_ASSERT (++evaluations < 0);
      if (evaluations != 0 || g_calls != 0)
        return 3;
      break;
    case 2:
      contracts::set_violation_handler (&counting_handler);
      LUMEX_CONTRACT_ASSERT (++evaluations < 0);
      if (evaluations != 1 || g_calls != 1)
        return 4;
      break;
    case 3:
      contracts::set_violation_handler (&throwing_handler);
      try
        {
          LUMEX_CONTRACT_ASSERT (++evaluations < 0);
          return 5;
        }
      catch (violation_error const &)
        {
          if (evaluations != 1 || g_calls != 1)
            return 5;
        }
      break;
    default:
      // quick_enforce traps and the native keyword is the compiler's: the
      // numbers above are all that can be checked in process.
      break;
    }

  // overridden.cpp defines the semantic itself: observe, whatever the build.
  if (overridden_semantic () != 2)
    return 6;
  std::printf ("contracts_build_semantic: ok\n");
  return 0;
}
