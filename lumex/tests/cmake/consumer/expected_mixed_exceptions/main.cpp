// Built with exceptions: value () of an expected that holds an error must
// throw bad_expected_access<int> in every form, whichever unit the linker
// meets first; the unit of lib.cpp (no exceptions) must terminate without an
// exception in flight. The calls of lib.cpp run in a child process (fork, or
// this program started again on Windows).
// Exit code 0 on success.
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <utility>

#if defined(_WIN32)
#include <csignal>
#include <string>

#include <stdlib.h> // _set_abort_behavior
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "lumex/core/expected/Expected"

#include "lib.hpp"

namespace lx = lumex::core::expected;

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

// Whether value () of an expected that holds an error throws, per overload.
bool
throws_here (int overload)
{
  lx::result::expected<int, int> error (lx::result::unexpect, 1);
  lx::result::expected<int, int> const const_error (lx::result::unexpect, 1);
  lx::result::expected<void, int> void_error (lx::result::unexpect, 1);
  try
    {
      switch (overload)
        {
        case 0:
          return error.value () == 0;
        case 1:
          return std::move (error).value () == 0;
        case 2:
          return const_error.value () == 0;
        case 3:
          return std::move (const_error).value () == 0;
        case 4:
          static_cast<lx::result::expected<void, int> const &> (void_error)
              .value ();
          return false;
        default:
          std::move (void_error).value ();
          return false;
        }
    }
  catch (lx::error::bad_expected_access<int> const &)
    {
      return true;
    }
}

// What the child does: the call of the unit without exceptions, which must
// terminate. With the standard library of MSVC without exceptions
// (_HAS_EXCEPTIONS=0) std::terminate ends in abort, so the handler of SIGABRT
// is the one that runs there; a call that returns exits with 0.
void
abort_handler (int)
{
  std::_Exit (42);
}

int
run_child (int overload)
{
  std::set_terminate (terminate_handler);
#if defined(_WIN32)
  std::signal (SIGABRT, abort_handler);
  _set_abort_behavior (0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
  static_cast<void> (lib_value_of_error (overload));
  return 0;
}

// The exit code of a child that ran the call of the unit without exceptions.
// POSIX: a forked process. Windows: this program started again with the
// number of the overload (cmd.exe expands %name% in a path: a path of the
// executable with a percent sign fails the run loudly).
int
exit_code_of_lib (char const *self, int overload)
{
  std::fflush (nullptr);
#if defined(_WIN32)
  std::string const command
      = std::string ("\"\"") + self + "\" " + std::to_string (overload) + "\"";
  return std::system (command.c_str ());
#else
  static_cast<void> (self);
  pid_t const pid = fork ();
  if (pid == 0)
    std::_Exit (run_child (overload));
  int status = 0;
  if (pid < 0 || waitpid (pid, &status, 0) != pid || !WIFEXITED (status))
    return -1;
  return WEXITSTATUS (status);
#endif
}
} // namespace

int
main (int argc, char **argv)
{
  // A child on a system without fork: the number of the overload.
  if (argc == 2)
    return run_child (std::atoi (argv[1]));

  int failures = 0;
  for (int overload = 0; overload < 6; ++overload)
    {
      if (!throws_here (overload))
        {
          std::fprintf (
              stderr, "overload %d: the unit with exceptions did not throw\n",
              overload);
          ++failures;
        }
      int const code = exit_code_of_lib (argv[0], overload);
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
    std::puts ("expected mixed_exceptions: ok");
  return failures == 0 ? 0 : 1;
}
