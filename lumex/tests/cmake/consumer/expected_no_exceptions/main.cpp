// Runs the checks of cmake.consumer_expected_no_exceptions_*: expected with
// exceptions off (see CMakeLists.txt). Plain C++11, no test framework: the
// build has no exceptions, and gtest needs them.

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <utility>

#include "lumex/core/expected/Expected"

#if defined(__unix__) || defined(__APPLE__)
#define NOEXC_HAS_FORK 1
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#else
#define NOEXC_HAS_FORK 0
#endif

namespace lx = lumex::core::expected;
using lx::result::expected;

// The build really has (or has not) exceptions as the case says.
#if defined(EXPECTED_NO_EXCEPTIONS_COMPILER)
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#error "the compiler mode must build without exceptions"
#endif
#elif defined(EXPECTED_NO_EXCEPTIONS_MACRO)
#if !defined(__cpp_exceptions) && !defined(__EXCEPTIONS)                      \
    && !defined(_CPPUNWIND)
#error "the macro mode must build with exceptions on"
#endif
#endif
#if LUMEX_EXPECTED_HAS_EXCEPTIONS != 0
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

// A type whose copy and move constructors may throw: the paths of expected
// that keep the old contents (reinit, swap) are the ones with a try block
// when exceptions are on.
struct fragile
{
  int v;
  explicit fragile (int x) noexcept : v (x) {}
  fragile (fragile const &o) noexcept (false) : v (o.v) {}
  fragile (fragile &&o) noexcept (false) : v (o.v) { o.v = -1; }
  fragile &
  operator= (fragile const &o) noexcept (false)
  {
    v = o.v;
    return *this;
  }
  fragile &
  operator= (fragile &&o) noexcept (false)
  {
    v = o.v;
    o.v = -1;
    return *this;
  }
};

void
check_observers ()
{
  expected<int, int> ok (5);
  CHECK (ok.has_value ());
  CHECK (ok.value () == 5);
  CHECK (std::move (ok).value () == 5);
  expected<int, int> const cok (6);
  CHECK (cok.value () == 6);
  CHECK (std::move (cok).value () == 6);
  expected<void, int> v;
  v.value ();
  expected<int, int> bad (lx::result::unexpect, 7);
  CHECK (!bad.has_value ());
  CHECK (bad.error () == 7);
  CHECK (bad.value_or (3) == 3);
}

void
check_assign_and_emplace ()
{
  // The value may throw when built: switching to it keeps the old error
  // (reinit-expected), with a try block when exceptions are on.
  expected<fragile, int> a (fragile (1));
  expected<fragile, int> b (lx::result::unexpect, 2);
  a = b;
  CHECK (!a.has_value () && a.error () == 2);
  a = expected<fragile, int> (fragile (3));
  CHECK (a.has_value () && a.value ().v == 3);
  a = b;
  CHECK (!a.has_value () && a.error () == 2);
  a = expected<fragile, int> (fragile (4));
  CHECK (a.has_value () && a.value ().v == 4);
  a.emplace (5);
  CHECK (a.has_value () && a.value ().v == 5);
  a.emplace_error (6);
  CHECK (!a.has_value () && a.error () == 6);
  // The error may throw when built.
  expected<int, fragile> c (7);
  expected<int, fragile> d (lx::result::unexpect, fragile (8));
  c = d;
  CHECK (!c.has_value () && c.error ().v == 8);
  c = expected<int, fragile> (9);
  CHECK (c.has_value () && c.value () == 9);
  c.emplace_error (fragile (10));
  CHECK (!c.has_value () && c.error ().v == 10);
  expected<void, fragile> v;
  v.emplace_error (fragile (11));
  CHECK (!v.has_value () && v.error ().v == 11);
  v.emplace ();
  CHECK (v.has_value ());
  v = expected<void, fragile> (lx::result::unexpect, fragile (12));
  CHECK (!v.has_value () && v.error ().v == 12);
}

void
check_swap ()
{
  // E may throw when moved: the value is parked (mode 2).
  expected<int, fragile> a (1);
  expected<int, fragile> b (lx::result::unexpect, fragile (2));
  a.swap (b);
  CHECK (!a.has_value () && a.error ().v == 2);
  CHECK (b.has_value () && b.value () == 1);
  a.swap (b);
  CHECK (a.has_value () && a.value () == 1);
  CHECK (!b.has_value () && b.error ().v == 2);
  // E is nothrow move constructible: the error is parked (mode 1).
  expected<fragile, int> c (fragile (3));
  expected<fragile, int> d (lx::result::unexpect, 4);
  c.swap (d);
  CHECK (!c.has_value () && c.error () == 4);
  CHECK (d.has_value () && d.value ().v == 3);
  // void
  expected<void, fragile> e;
  expected<void, fragile> f (lx::result::unexpect, fragile (7));
  e.swap (f);
  CHECK (!e.has_value () && e.error ().v == 7);
  CHECK (f.has_value ());
  // same alternative
  expected<int, int> g (1), h (2);
  g.swap (h);
  CHECK (g.value () == 2 && h.value () == 1);
}

// The calls that must terminate. Each one is run in a child process.
enum terminating_case
{
  value_lvalue,
  value_rvalue,
  value_const_lvalue,
  value_const_rvalue,
  value_string_error,
  void_value_lvalue,
  void_value_rvalue,
  terminating_case_count
};

void
run_terminating_case (int which)
{
  expected<int, int> bad (lx::result::unexpect, 1);
  expected<int, int> const cbad (lx::result::unexpect, 1);
  expected<int, std::string> sbad (lx::result::unexpect,
                                   std::string ("an error long enough"));
  expected<void, int> vbad (lx::result::unexpect, 1);
  int sink = 0;
  switch (which)
    {
    case value_lvalue:
      sink = bad.value ();
      break;
    case value_rvalue:
      sink = std::move (bad).value ();
      break;
    case value_const_lvalue:
      sink = cbad.value ();
      break;
    case value_const_rvalue:
      sink = std::move (cbad).value ();
      break;
    case value_string_error:
      sink = sbad.value ();
      break;
    case void_value_lvalue:
      vbad.value ();
      break;
    case void_value_rvalue:
      std::move (vbad).value ();
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

#if NOEXC_HAS_FORK
void
check_terminates (int which)
{
  std::fflush (nullptr);
  pid_t const pid = fork ();
  if (pid == 0)
    {
      std::set_terminate (terminate_handler_exit_42);
      run_terminating_case (which);
      std::_Exit (0);
    }
  int status = 0;
  CHECK (pid > 0);
  CHECK (waitpid (pid, &status, 0) == pid);
  bool const terminated = WIFEXITED (status) && WEXITSTATUS (status) == 42;
  if (!terminated)
    std::fprintf (stderr, "case %d did not call std::terminate (status %d)\n",
                  which, status);
  CHECK (terminated);
}
#endif
} // namespace

int
main ()
{
  check_observers ();
  check_assign_and_emplace ();
  check_swap ();
#if NOEXC_HAS_FORK
  for (int which = 0; which < terminating_case_count; ++which)
    check_terminates (which);
#endif
  if (g_failures != 0)
    {
      std::fprintf (stderr, "%d check(s) failed\n", g_failures);
      return 1;
    }
  std::puts ("expected without exceptions: all checks passed");
  return 0;
}
