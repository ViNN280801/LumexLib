// Built with exceptions: value () of an empty optional<int> must throw
// lumex_bad_optional_access in every overload, whichever unit the linker meets
// first, and the unit of lib.cpp (no exceptions) must terminate without an
// exception in flight. The calls of lib.cpp run in a child process (fork).
// Exit code 0 on success.
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <utility>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "lumex/core/optional/opt/LumexOptional.hpp"

#include "lib.hpp"

namespace lo = lumex::core::optional::opt;

namespace
{
// 42: std::terminate was called with no exception in flight (value () of the
// unit without exceptions). 44: it was called while an exception was in
// flight, which is the throwing body of the unit with exceptions running in
// the unit without them: the linker kept the wrong copy.
[[noreturn]] void
terminate_handler ()
{
  std::_Exit (std::current_exception () ? 44 : 42);
}

// Whether value () of an empty optional<int> throws, for one overload.
bool
throws_here (int overload)
{
  lo::optional<int> empty;
  lo::optional<int> const const_empty;
  try
    {
      switch (overload)
        {
        case 0:
          return empty.value () == 0;
        case 1:
          return std::move (empty).value () == 0;
        case 2:
          return const_empty.value () == 0;
        default:
          return std::move (const_empty).value () == 0;
        }
    }
  catch (lo::lumex_bad_optional_access const &)
    {
      return true;
    }
}

// The exit code of a child that ran value () of the unit without exceptions.
int
exit_code_of_lib (int overload)
{
  std::fflush (nullptr);
  pid_t const pid = fork ();
  if (pid == 0)
    {
      std::set_terminate (terminate_handler);
      static_cast<void> (lib_value_of_empty (overload));
      std::_Exit (0);
    }
  int status = 0;
  if (pid < 0 || waitpid (pid, &status, 0) != pid || !WIFEXITED (status))
    return -1;
  return WEXITSTATUS (status);
}
} // namespace

int
main ()
{
  int failures = 0;
  for (int overload = 0; overload < 4; ++overload)
    {
      if (!throws_here (overload))
        {
          std::fprintf (
              stderr, "overload %d: the unit with exceptions did not throw\n",
              overload);
          ++failures;
        }
      int const code = exit_code_of_lib (overload);
      if (code != 42)
        {
          std::fprintf (stderr,
                        "overload %d: the unit without exceptions did not "
                        "terminate (exit code %d)\n",
                        overload, code);
          ++failures;
        }
    }
  if (failures == 0)
    std::puts ("optional mixed_exceptions: ok");
  return failures == 0 ? 0 : 1;
}
