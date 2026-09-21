#include <iostream>
#include <string>

#include "lumex/core/exceptions/LumexException"

using namespace lumex::core::exceptions::exception;
using namespace lumex::core::exceptions::stacktrace;

LUMEX_DEFINE_EXCEPTION (ExampleInstrumentError, LumexBaseException)

int
main ()
{
  std::cout << "=== Exceptions, stacktrace, typed errors ===\n\n";

  std::cout << "--- 1. LumexBaseException::what ---\n";
  try
    {
      throw LumexBaseException ("pump pressure out of range");
    }
  catch (LumexBaseException const &ex)
    {
      std::cout << "caught: " << ex.what () << '\n';
      LumexStacktrace const frames = ex.getStackTrace ();
      std::cout << "stack_frames=" << frames.size ()
                << " empty=" << (frames.empty () ? "yes" : "no") << '\n';
    }

  std::cout << "\n--- 2. Derived type via LUMEX_DEFINE_EXCEPTION ---\n";
  try
    {
      throw ExampleInstrumentError (std::string ("valve timeout"));
    }
  catch (ExampleInstrumentError const &ex)
    {
      std::cout << "typed: " << ex.what () << '\n';
      ex.to_stderr ();
    }

  std::cout << "\n--- 3. LumexStacktrace::current ---\n";
  LumexStacktrace const now = LumexStacktrace::current (0);
  std::string const text = to_string (now);
  std::cout << "current_frames=" << now.size ()
            << " to_string_chars=" << text.size () << '\n';

  std::cout << "\n=== Exceptions example finished ===\n";
  return 0;
}
