// Runs the checks of cmake.consumer_span_no_exceptions_*: span with
// exceptions off (see CMakeLists.txt). Plain C++11, no test framework: the
// build has no exceptions, and gtest needs them.

#include <array>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <utility>
#if defined(_MSC_VER)
#include <stdlib.h> // _set_abort_behavior
#endif

#include "lumex/core/span/LumexSpan"

#if defined(__unix__) || defined(__APPLE__)
#define NOEXC_HAS_FORK 1
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#else
#define NOEXC_HAS_FORK 0
#endif

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#define NOEXC_COMPILER_HAS_EXCEPTIONS 1
#else
#define NOEXC_COMPILER_HAS_EXCEPTIONS 0
#endif

namespace sp = lumex::core::span::view;

// The build really has (or has not) exceptions as the case says.
#if defined(SPAN_NO_EXCEPTIONS_COMPILER)
#if NOEXC_COMPILER_HAS_EXCEPTIONS
#error "the compiler mode must build without exceptions"
#endif
#elif defined(SPAN_NO_EXCEPTIONS_MACRO)
#if !NOEXC_COMPILER_HAS_EXCEPTIONS
#error "the macro mode must build with exceptions on"
#endif
#endif
#if !defined(LUMEX_SPAN_HAS_EXCEPTIONS) || LUMEX_SPAN_HAS_EXCEPTIONS != 0
#error "the library must see the exceptions as off"
#endif

namespace
{
int g_failures = 0;

#define CHECK(cond)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          std::fprintf (stderr, "FAILED line %d: %s\n", __LINE__, #cond);     \
          ++g_failures;                                                       \
        }                                                                     \
    }                                                                         \
  while (0)

// The constexpr tier keeps working: at () of a constant span from C++11.
constexpr int kconstants[3] = { 10, 20, 30 };
constexpr sp::span<int const> kdynamic (kconstants, 3);
constexpr sp::span<int const, 3> kstatic (kconstants);
static_assert (kdynamic.at (1) == 20, "constexpr at () of a dynamic extent");
static_assert (kstatic.at (2) == 30, "constexpr at () of a static extent");

void
check_accessors ()
{
  std::array<int, 3> array = { { 4, 5, 6 } };
  sp::span<int> const dynamic (array.data (), array.size ());
  sp::span<int, 3> const fixed (array);
  CHECK (dynamic.at (0) == 4 && dynamic.at (2) == 6);
  CHECK (fixed.at (1) == 5);
  CHECK (dynamic[1] == 5 && dynamic.front () == 4 && dynamic.back () == 6);
  dynamic.at (1) = 7;
  CHECK (array[1] == 7);
  sp::span<int> const empty;
  CHECK (empty.empty () && empty.size () == 0);
}

// The calls that must terminate. Each one is run in a child process.
enum terminating_case
{
  at_dynamic,
  at_const_elements,
  at_static,
  at_empty,
  at_string_elements,
  at_one_past_the_end,
  terminating_case_count
};

void
run_terminating_case (int which)
{
  std::array<int, 3> array = { { 4, 5, 6 } };
  std::array<std::string, 2> texts = { { std::string ("a text long enough"),
                                         std::string ("another one") } };
  sp::span<int> const dynamic (array.data (), array.size ());
  sp::span<int const> const const_elements (array.data (), array.size ());
  sp::span<int, 3> const fixed (array);
  sp::span<int> const empty;
  sp::span<std::string> const strings (texts.data (), texts.size ());
  int sink = 0;
  switch (which)
    {
    case at_dynamic:
      sink = dynamic.at (99);
      break;
    case at_const_elements:
      sink = const_elements.at (3);
      break;
    case at_static:
      sink = fixed.at (3);
      break;
    case at_empty:
      sink = empty.at (0);
      break;
    case at_string_elements:
      sink = static_cast<int> (strings.at (2).size ());
      break;
    case at_one_past_the_end:
      sink = dynamic.at (dynamic.size ());
      break;
    default:
      break;
    }
  // Reached only when at () returned.
  std::fprintf (stderr, "case %d returned (%d)\n", which, sink);
}

[[noreturn]] void
terminate_handler_exit_42 ()
{
  std::_Exit (42);
}

#if defined(_MSC_VER) && !NOEXC_COMPILER_HAS_EXCEPTIONS
// With _HAS_EXCEPTIONS=0 (the standard library of MSVC without exceptions)
// the terminate handler is not consulted: std::terminate ends in abort, which
// raises SIGABRT before it ends the process. The handler of the signal gives
// the same exit code 42 as the handler of std::terminate, so the check needs
// exactly that code and not the status of a process that died for another
// reason (a /GS cookie, an invalid-parameter fault, CFG: 0xC0000409 too).
extern "C" void
abort_signal_handler (int)
{
  std::_Exit (42);
}
#endif

// The exit code of a child that ran the case: 42 when std::terminate was
// called, 43 when an exception came out instead (the macro mode has them on).
int
run_child (int which)
{
  std::set_terminate (terminate_handler_exit_42);
#if defined(_MSC_VER) && !NOEXC_COMPILER_HAS_EXCEPTIONS
  std::signal (SIGABRT, abort_signal_handler);
  // No message box of the debug runtime in front of the handler.
  _set_abort_behavior (0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
#if NOEXC_COMPILER_HAS_EXCEPTIONS
  try
    {
      run_terminating_case (which);
    }
  catch (...)
    {
      std::_Exit (43);
    }
#else
  run_terminating_case (which);
#endif
  return 0;
}

// Runs the case in a child process and returns its exit code, or -1.
int
exit_code_of_child (char const *self, int which)
{
  std::fflush (nullptr);
#if NOEXC_HAS_FORK
  static_cast<void> (self);
  pid_t const pid = fork ();
  if (pid == 0)
    std::_Exit (run_child (which));
  int status = 0;
  if (pid < 0 || waitpid (pid, &status, 0) != pid || !WIFEXITED (status))
    return -1;
  return WEXITSTATUS (status);
#else
  // cmd.exe drops the outer pair of quotes of the command line, and expands
  // `%name%` in it: a path of the executable with a percent sign fails the run
  // (loudly, as an exit code that is not 42), it does not pass by mistake.
  std::string const command
      = std::string ("\"\"") + self + "\" " + std::to_string (which) + "\"";
  return std::system (command.c_str ());
#endif
}

void
check_terminates (char const *self, int which)
{
  int const code = exit_code_of_child (self, which);
  if (code != 42)
    std::fprintf (stderr,
                  "case %d did not call std::terminate (exit code %d)\n",
                  which, code);
  CHECK (code == 42);
}
} // namespace

int
main (int argc, char **argv)
{
  // A child on a system without fork: the case number is the argument.
  if (argc == 2)
    return run_child (std::atoi (argv[1]));

  check_accessors ();
  for (int which = 0; which < terminating_case_count; ++which)
    check_terminates (argv[0], which);
  if (g_failures != 0)
    {
      std::fprintf (stderr, "%d check(s) failed\n", g_failures);
      return 1;
    }
  std::puts ("span without exceptions: all checks passed");
  return 0;
}
