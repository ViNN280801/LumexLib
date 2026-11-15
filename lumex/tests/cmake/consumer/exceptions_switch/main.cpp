// Checks of cmake.consumer_exceptions_switch_*: the shared switch
// LUMEX_HAS_EXCEPTIONS and the macro of each module (see CMakeLists.txt). The
// build has exceptions in every mode. Exit code 0 when every check held.
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <new>
#include <ostream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <utility>

#if defined(_WIN32)
#include <cstdlib>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "lumex/core/expected/Expected"
#include "lumex/core/optional/opt/LumexOptional.hpp"
#include "lumex/core/smart_ptr/LumexSmartPtr"
#include "lumex/core/smart_ptr/detail/LumexSmartPtrConfig.hpp"
#include "lumex/core/span/LumexSpan"
#include "lumex/core/string_view/LumexStringView"

namespace lx = lumex::core::expected;
namespace lo = lumex::core::optional::opt;
namespace sp = lumex::core::span::view;

// What each module must do in this mode: 1 throws, 0 terminates.
#if defined(SWITCH_DEFAULT)
#define WANT_SHARED 1
#define WANT_EXPECTED 1
#define WANT_OPTIONAL 1
#define WANT_SPAN 1
#define WANT_STRING_VIEW 1
#elif defined(SWITCH_SHARED_ZERO)
#define WANT_SHARED 0
#define WANT_EXPECTED 0
#define WANT_OPTIONAL 0
#define WANT_SPAN 0
#define WANT_STRING_VIEW 0
#elif defined(SWITCH_OVERRIDE_EXPECTED)
#define WANT_SHARED 0
#define WANT_EXPECTED 1
#define WANT_OPTIONAL 0
#define WANT_SPAN 0
#define WANT_STRING_VIEW 0
#elif defined(SWITCH_OVERRIDE_OPTIONAL)
#define WANT_SHARED 0
#define WANT_EXPECTED 0
#define WANT_OPTIONAL 1
#define WANT_SPAN 0
#define WANT_STRING_VIEW 0
#elif defined(SWITCH_OVERRIDE_SPAN)
#define WANT_SHARED 1
#define WANT_EXPECTED 1
#define WANT_OPTIONAL 1
#define WANT_SPAN 0
#define WANT_STRING_VIEW 1
#elif defined(SWITCH_OVERRIDE_STRING_VIEW)
#define WANT_SHARED 0
#define WANT_EXPECTED 0
#define WANT_OPTIONAL 0
#define WANT_SPAN 0
#define WANT_STRING_VIEW 1
#else
#error "a SWITCH_ mode is required"
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

enum module_t
{
  module_expected,
  module_optional,
  module_span,
  module_string_view,
  module_count
};

// The throwing call of one module. Returns normally only when it did not throw
// or terminate (a failure for a module that must not return).
int
call_of (int which)
{
  lx::result::expected<int, int> const error (lx::result::unexpect, 1);
  lo::optional<int> const empty;
  int raw[3] = { 1, 2, 3 };
  sp::span<int> const view (raw, 3);
  lumex_string_view const text ("abc", 3);
  switch (which)
    {
    case module_expected:
      return error.value ();
    case module_optional:
      return empty.value ();
    case module_span:
      return view.at (7);
    default:
      return text.at (7) == 'a' ? 1 : 0;
    }
}

// Whether the call of the module throws the exception of that module.
bool
throws (int which)
{
  try
    {
      static_cast<void> (call_of (which));
    }
  catch (lx::error::bad_expected_access<int> const &)
    {
      return which == module_expected;
    }
  catch (lo::lumex_bad_optional_access const &)
    {
      return which == module_optional;
    }
  catch (std::out_of_range const &)
    {
      return which == module_span || which == module_string_view;
    }
  return false;
}

[[noreturn]] void
terminate_handler ()
{
  std::_Exit (42);
}

int
run_child (int which)
{
  std::set_terminate (terminate_handler);
  static_cast<void> (call_of (which));
  return 0;
}

// The exit code of a child that ran the call: 42 when it terminated.
int
exit_code_of_child (char const *self, int which)
{
  std::fflush (nullptr);
#if defined(_WIN32)
  std::string const command
      = std::string ("\"\"") + self + "\" " + std::to_string (which) + "\"";
  return std::system (command.c_str ());
#else
  static_cast<void> (self);
  pid_t const pid = fork ();
  if (pid == 0)
    std::_Exit (run_child (which));
  int status = 0;
  if (pid < 0 || waitpid (pid, &status, 0) != pid || !WIFEXITED (status))
    return -1;
  return WEXITSTATUS (status);
#endif
}

// A stream buffer that throws on every write.
class throwing_buffer : public std::streambuf
{
protected:
  int_type
  overflow (int_type) override
  {
    throw 5;
  }

  std::streamsize
  xsputn (char const *, std::streamsize) override
  {
    throw 5;
  }
};

// The try block of the inserter of the string view follows the compiler, not
// the switch: with exceptions on (every mode here) an exception of the buffer
// does not leave the inserter, the stream goes bad.
void
check_inserter_try ()
{
  throwing_buffer buffer;
  std::ostream out (&buffer);
  lumex_string_view const text ("abc", 3);
  bool escaped = false;
  try
    {
      out << text;
    }
  catch (...)
    {
      escaped = true;
    }
  CHECK (!escaped);
  CHECK (out.bad ());
}

// An allocator that cannot allocate.
template <typename T> struct failing_allocator
{
  using value_type = T;

  failing_allocator () {}
  template <typename U> failing_allocator (failing_allocator<U> const &) {}

  T *
  allocate (std::size_t)
  {
    throw std::bad_alloc ();
  }

  void
  deallocate (T *, std::size_t)
  {
  }
};

template <typename T, typename U>
bool
operator== (failing_allocator<T> const &, failing_allocator<U> const &)
{
  return true;
}

template <typename T, typename U>
bool
operator!= (failing_allocator<T> const &, failing_allocator<U> const &)
{
  return false;
}

struct counting_deleter
{
  int *count;

  void
  operator() (int *) const
  {
    ++*count;
  }
};

// The try blocks of smart_ptr follow the compiler, not the switch: when the
// control block cannot be allocated the deleter is called with the pointer
// and the exception reaches the caller, whatever LUMEX_HAS_EXCEPTIONS says.
void
check_smart_ptr_try ()
{
  int deleted = 0;
  int object = 0;
  bool threw = false;
  try
    {
      counting_deleter const deleter = { &deleted };
      lumex::core::smart_ptr::shared_ptr<int> const owner (
          &object, deleter, failing_allocator<int> ());
    }
  catch (std::bad_alloc const &)
    {
      threw = true;
    }
  CHECK (threw);
  CHECK (deleted == 1);
}

void
check_module (char const *self, int which, int want_throw, int macro)
{
  CHECK (macro == want_throw);
  if (want_throw)
    CHECK (throws (which));
  else
    {
      int const code = exit_code_of_child (self, which);
      if (code != 42)
        std::fprintf (stderr, "module %d did not terminate (exit code %d)\n",
                      which, code);
      CHECK (code == 42);
    }
}
} // namespace

int
main (int argc, char **argv)
{
  // A child on Windows: the number of the module.
  if (argc == 2)
    return run_child (std::atoi (argv[1]));

  // The compiler has exceptions in every mode.
  CHECK (LUMEX_COMPILER_HAS_EXCEPTIONS == 1);
  CHECK (LUMEX_HAS_EXCEPTIONS == WANT_SHARED);
  // smart_ptr follows the compiler, not the switch.
  CHECK (LUMEX_SMART_PTR_HAS_EXCEPTIONS == 1);
  check_inserter_try ();
  check_smart_ptr_try ();

  check_module (argv[0], module_expected, WANT_EXPECTED,
                LUMEX_EXPECTED_HAS_EXCEPTIONS);
  check_module (argv[0], module_optional, WANT_OPTIONAL,
                LUMEX_OPTIONAL_HAS_EXCEPTIONS);
  check_module (argv[0], module_span, WANT_SPAN, LUMEX_SPAN_HAS_EXCEPTIONS);
  check_module (argv[0], module_string_view, WANT_STRING_VIEW,
                LUMEX_STRING_VIEW_HAS_EXCEPTIONS);

  if (g_failures != 0)
    {
      std::fprintf (stderr, "%d check(s) failed\n", g_failures);
      return 1;
    }
  std::puts ("exceptions_switch: all checks passed");
  return 0;
}
