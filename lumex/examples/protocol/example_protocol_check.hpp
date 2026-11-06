// What an example does with the claims it prints: it checks each one. A claim
// that does not hold is told and makes the exit code of the program 1, so a
// green run of the example in CTest means the library did what the text says.
#ifndef LUMEX_EXAMPLES_PROTOCOL_CHECK_HPP
#define LUMEX_EXAMPLES_PROTOCOL_CHECK_HPP

#include <iostream>

namespace check
{
/// The number of claims that did not hold.
inline int &
failures ()
{
  static int count = 0;
  return count;
}

/// Tells a claim that does not hold (`claim` says what should have been).
inline void
expect (bool holds, char const *claim)
{
  if (holds)
    return;
  std::cout << "  CHECK FAILED: " << claim << '\n';
  ++failures ();
}

/// The exit code of the program: 0 when every claim held.
inline int
exit_code ()
{
  return failures () == 0 ? 0 : 1;
}
} // namespace check

#endif // !LUMEX_EXAMPLES_PROTOCOL_CHECK_HPP
