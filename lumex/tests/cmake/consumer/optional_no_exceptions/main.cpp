// Runs the checks of cmake.consumer_optional_no_exceptions_*: optional with
// exceptions off (see CMakeLists.txt). Plain C++11, no test framework: the
// build has no exceptions, and gtest needs them.

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <utility>

#include "lumex/core/optional/opt/LumexOptional.hpp"

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

namespace lo = lumex::core::optional::opt;

// The build really has (or has not) exceptions as the case says.
#if defined(OPTIONAL_NO_EXCEPTIONS_COMPILER)
#if NOEXC_COMPILER_HAS_EXCEPTIONS
#error "the compiler mode must build without exceptions"
#endif
#elif defined(OPTIONAL_NO_EXCEPTIONS_MACRO)
#if !NOEXC_COMPILER_HAS_EXCEPTIONS
#error "the macro mode must build with exceptions on"
#endif
#endif
#if !defined(LUMEX_OPTIONAL_HAS_EXCEPTIONS)                                   \
    || LUMEX_OPTIONAL_HAS_EXCEPTIONS != 0
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

// The constexpr tiers keep working: value () const & from C++11, value () &
// and value () && from C++14.
constexpr lo::optional<int> kconstant (12);
static_assert (kconstant.value () == 12, "constexpr value () const &");
static_assert (static_cast<lo::optional<int> const &&> (kconstant).value ()
                   == 12,
               "constexpr value () const &&");
#if __cplusplus >= 201402L
constexpr int
constexpr_forms ()
{
  lo::optional<int> number (11);
  number.value () += 1;
  return std::move (number).value ();
}
static_assert (constexpr_forms () == 12, "constexpr value () & and &&");
#endif

void
check_observers ()
{
  lo::optional<int> number (5);
  CHECK (number.has_value ());
  CHECK (number.value () == 5);
  CHECK (std::move (number).value () == 5);
  lo::optional<int> const constant (6);
  CHECK (constant.value () == 6);
  CHECK (std::move (constant).value () == 6);
  lo::optional<std::string> text (std::string ("a text long enough"));
  CHECK (text.value () == "a text long enough");
  text.value () += "!";
  CHECK (*text == "a text long enough!");
  lo::optional<int> empty;
  CHECK (!empty.has_value ());
  CHECK (empty.value_or (3) == 3);
  empty.emplace (8);
  CHECK (empty.value () == 8);
  empty.reset ();
  CHECK (!empty);
}

// The calls that must terminate. Each one is run in a child process.
enum terminating_case
{
  value_lvalue,
  value_rvalue,
  value_const_lvalue,
  value_const_rvalue,
  value_string_lvalue,
  value_string_rvalue,
  value_after_reset,
  terminating_case_count
};

void
run_terminating_case (int which)
{
  lo::optional<int> empty;
  lo::optional<int> const const_empty;
  lo::optional<std::string> text_empty;
  lo::optional<int> reset_one (4);
  reset_one.reset ();
  int sink = 0;
  switch (which)
    {
    case value_lvalue:
      sink = empty.value ();
      break;
    case value_rvalue:
      sink = std::move (empty).value ();
      break;
    case value_const_lvalue:
      sink = const_empty.value ();
      break;
    case value_const_rvalue:
      sink = std::move (const_empty).value ();
      break;
    case value_string_lvalue:
      sink = static_cast<int> (text_empty.value ().size ());
      break;
    case value_string_rvalue:
      sink = static_cast<int> (std::move (text_empty).value ().size ());
      break;
    case value_after_reset:
      sink = reset_one.value ();
      break;
    default:
      break;
    }
  // Reached only when value () returned.
  std::fprintf (stderr, "case %d returned (%d)\n", which, sink);
}

[[noreturn]] void
terminate_handler_exit_42 ()
{
  std::_Exit (42);
}

// The exit code of a child that ran the case: 42 when std::terminate was
// called, 43 when an exception came out instead (the macro mode has them on).
int
run_child (int which)
{
  std::set_terminate (terminate_handler_exit_42);
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

// With _HAS_EXCEPTIONS=0 (the standard library of MSVC without exceptions) the
// terminate handler is not consulted: std::terminate ends the process with the
// fast-fail status 0xC0000409, which is what a call that does not return
// looks like there. Everything else, a normal exit included, is a failure.
bool
is_terminated (int code)
{
#if defined(_MSC_VER) && !NOEXC_COMPILER_HAS_EXCEPTIONS
  return code == 42 || static_cast<unsigned int> (code) == 0xC0000409u;
#else
  return code == 42;
#endif
}

void
check_terminates (char const *self, int which)
{
  int const code = exit_code_of_child (self, which);
  if (!is_terminated (code))
    std::fprintf (stderr,
                  "case %d did not call std::terminate (exit code %d)\n",
                  which, code);
  CHECK (is_terminated (code));
}
} // namespace

int
main (int argc, char **argv)
{
  // A child on a system without fork: the case number is the argument.
  if (argc == 2)
    return run_child (std::atoi (argv[1]));

  check_observers ();
  for (int which = 0; which < terminating_case_count; ++which)
    check_terminates (argv[0], which);
  if (g_failures != 0)
    {
      std::fprintf (stderr, "%d check(s) failed\n", g_failures);
      return 1;
    }
  std::puts ("optional without exceptions: all checks passed");
  return 0;
}
