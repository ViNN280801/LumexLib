// Fixture of cmake.consumer_string_view_no_exceptions_* (see CMakeLists.txt).
// Exit code 0 on success.
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#include <unistd.h>
#define SV_CAN_FORK 1
#else
#define SV_CAN_FORK 0
#endif

#include "lumex/core/string_view/LumexStringView"

#if defined(SV_WITH_EXCEPTIONS)
#include <stdexcept>
#endif

namespace
{
int failures = 0;

void
check (bool ok, char const *what)
{
  if (!ok)
    {
      std::fprintf (stderr, "string_view no_exceptions: %s failed\n", what);
      ++failures;
    }
}

#if !defined(SV_WITH_EXCEPTIONS) && SV_CAN_FORK
// A child that runs `call` must end by a signal (std::terminate -> abort).
template <class Call>
void
must_terminate (Call call, char const *what)
{
  pid_t const pid = fork ();
  if (pid == 0)
    {
      call ();
      _exit (0); // reaching this line is the failure
    }
  int status = 0;
  waitpid (pid, &status, 0);
  check (WIFSIGNALED (status), what);
}
#endif
} // namespace

int
main ()
{
  lumex_string_view const view ("hello world");
  check (view.find ("wor") == 6 && view.substr (6) == "world"
             && view.compare (6, 5, "world") == 0 && view.contains ("o w")
             && view.at (1) == 'e',
         "queries");
  char buffer[4] = {};
  check (view.copy (buffer, 3, 2) == 3 && buffer[0] == 'l', "copy");
  std::ostringstream out;
  out << '[' << std::setw (8) << std::setfill ('*') << view.substr (0, 3)
      << ']';
  check (out.str () == "[*****hel]", "operator<< with padding");
  std::hash<lumex_string_view> hasher;
  check (hasher (view) == hasher (lumex_string_view ("hello world")), "hash");

#if defined(SV_WITH_EXCEPTIONS)
  bool threw = false;
  try
    {
      (void)view.at (99);
    }
  catch (std::out_of_range const &)
    {
      threw = true;
    }
  check (threw, "at () throws");
  threw = false;
  try
    {
      (void)view.substr (99);
    }
  catch (std::out_of_range const &)
    {
      threw = true;
    }
  check (threw, "substr () throws");
  threw = false;
  try
    {
      (void)view.copy (buffer, 1, 99);
    }
  catch (std::out_of_range const &)
    {
      threw = true;
    }
  check (threw, "copy () throws");
  threw = false;
  try
    {
      (void)view.compare (99, 1, "x");
    }
  catch (std::out_of_range const &)
    {
      threw = true;
    }
  check (threw, "compare (pos, ...) throws");
#elif SV_CAN_FORK
  must_terminate ([&] { (void)view.at (99); }, "at () terminates");
  must_terminate ([&] { (void)view.substr (99); }, "substr () terminates");
  must_terminate ([&] { (void)view.copy (buffer, 1, 99); },
                  "copy () terminates");
  must_terminate ([&] { (void)view.compare (99, 1, "x"); },
                  "compare (pos, ...) terminates");
#endif
  if (failures == 0)
    std::puts ("string_view no_exceptions: ok");
  return failures == 0 ? 0 : 1;
}
