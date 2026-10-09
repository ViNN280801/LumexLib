#include "Module.hpp"

namespace
{
int g_module_calls = 0;

void
module_handler (lumex::core::contracts::contract_violation const &)
{
  ++g_module_calls;
}
} // namespace

namespace fixture
{
void
violate_in_module ()
{
  LUMEX_CONTRACT_ASSERT_OBSERVE (g_module_calls < 0);
}

lumex::core::contracts::violation_handler_type
install_module_handler ()
{
  lumex::core::contracts::set_violation_handler (&module_handler);
  return &module_handler;
}

int
module_handler_calls ()
{
  return g_module_calls;
}
} // namespace fixture
