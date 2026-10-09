// A translation unit that asks for observe itself: the macro wins over the
// semantic the build passes.

#define LUMEX_CONTRACTS_SEMANTIC observe

#include "lumex/core/contracts/LumexContracts"

int
overridden_semantic ()
{
  return LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC;
}
