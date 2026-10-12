#include <iostream>
#include <string>

#include "lumex/core/exceptions/LumexException"

using namespace lumex::core::exceptions::exception;

LUMEX_DEFINE_EXCEPTION (ExampleSequenceAbort, lumex_base_exception)

namespace
{
void
arm_injector ()
{
  throw ExampleSequenceAbort ("injector not ready");
}
}

int
main ()
{
  std::cout << "=== Workflow: abort a sequence and keep the message ===\n\n";
  try
    {
      arm_injector ();
    }
  catch (lumex_base_exception const &ex)
    {
      std::cout << "abort_reason=" << ex.what ()
                << " frames=" << ex.get_stack_trace ().size () << '\n';
    }
  return 0;
}
