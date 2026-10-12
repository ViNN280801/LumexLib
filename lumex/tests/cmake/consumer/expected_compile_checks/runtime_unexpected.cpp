// Is there a global `unexpected` of the runtime itself? MSVC's <eh.h>
// declares `void unexpected ()` while `_HAS_CXX23` is 0, and MinGW always
// does, so `using ::unexpected;` - the declaration of the BAD case 3 of
// check.cpp - is valid there and cannot see that the umbrella has no global
// name. This probe compiles exactly when the name exists; the fixture skips
// the case where it does.
//
// The includes are those of check.cpp: they are what pulls <eh.h> in.

#include <cstdlib>
#include <string>
#include <type_traits>

int
main ()
{
  using ::unexpected;
  static_cast<void> (0);
  return 0;
}
