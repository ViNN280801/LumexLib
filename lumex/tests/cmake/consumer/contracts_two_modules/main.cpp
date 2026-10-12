// One violation handler for the whole process: the executable installs a
// handler and the library's violations reach it; the library installs its own
// and the executable's violations reach that one. Exit codes: 0 fine, 1..4 the
// numbered check failed.

#include <cstdio>
#include <string>

#include "Module.hpp"

namespace contracts = lumex::core::contracts;

namespace
{
int g_exe_calls = 0;
std::string g_last_function;

void
exe_handler (contracts::contract_violation const &violation)
{
  ++g_exe_calls;
  g_last_function = violation.location ().function_name ();
}
} // namespace

int
main ()
{
  contracts::set_violation_handler (&exe_handler);

  fixture::violate_in_module ();
  if (g_exe_calls != 1
      || g_last_function.find ("violate_in_module") == std::string::npos)
    {
      std::printf ("check 1: %d calls, function '%s'\n", g_exe_calls,
                   g_last_function.c_str ());
      return 1;
    }

  contracts::violation_handler_type const module_handler
      = fixture::install_module_handler ();
  if (contracts::get_violation_handler () != module_handler)
    return 2;

  LUMEX_CONTRACT_ASSERT_OBSERVE (g_exe_calls < 0);
  if (fixture::module_handler_calls () != 1 || g_exe_calls != 1)
    return 3;

  fixture::violate_in_module ();
  if (fixture::module_handler_calls () != 2 || g_exe_calls != 1)
    return 4;

  contracts::set_violation_handler (nullptr);
  std::printf ("contracts_two_modules: one handler for all\n");
  return 0;
}
