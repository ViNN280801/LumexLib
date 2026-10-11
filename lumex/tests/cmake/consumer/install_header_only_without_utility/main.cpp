// cmake.install_header_only_without_utility: the seven header-only
// modules compile and work from an install prefix that has no core/utility
// module. Their umbrellas include core/utility headers, which LumexLib
// installs whatever LUMEX_BUILD_UTILITY says. Exit code 0 on success, 1 when
// a call returns an unexpected value.

#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

#include "lumex/core/atomic/LumexAtomic"
#include "lumex/core/generators/LumexGenerators"
#include "lumex/core/math/LumexMath"
#include "lumex/core/optional/LumexOptional"
#include "lumex/core/span/LumexSpan"
#include "lumex/core/string/LumexString"
#include "lumex/core/string_view/LumexStringView"

namespace
{
int failures = 0;

void
check (bool condition, char const *what)
{
  if (!condition)
    {
      std::printf ("failed: %s\n", what);
      ++failures;
    }
}
} // namespace

int
main (int argc, char **argv)
{
  // With --violate: a span with the checks on (the target defines
  // LUMEX_SPAN_CHECKS=1) is indexed out of range and must abort with the
  // message of the span module, with no utility library to link.
  if (argc > 1 && std::strcmp (argv[1], "--violate") == 0)
    {
      int raw[4] = { 1, 2, 3, 4 };
      lumex::core::span::view::span<int> const view (raw, 4);
      volatile std::size_t const index = 4;
      return view[index] == 0 ? 2 : 3; // not reached
    }

  std::vector<int> const numbers = { 1, 2, 3 };
  lumex_string_view const text_view ("key=value");
  check (text_view.find ('=') == 3 && text_view.substr (4) == "value"
             && text_view.contains ("y=v"),
         "string_view: find, substr, contains");
  check (lumex::core::string::utility::stringify ("n=", 2) == "n=2",
         "string: stringify");
  check (lumex::core::string::text::join (numbers, ",") == "1,2,3",
         "string: join");

  check (lumex::core::math::ops::avg (numbers) == 2.0, "math: avg");
  check (LUMEX_MATH_CONSTANTS_PI > 3.14 && LUMEX_MATH_CONSTANTS_PI < 3.15,
         "math: constants");

  lumex::core::generators::number_generator::number_generator<int> dice (1, 6);
  int const roll = dice ();
  check (roll >= 1 && roll <= 6, "generators: NumberGenerator");

  lumex::core::optional::opt::optional<int> value;
  check (!value.has_value (), "optional: empty");
  value.emplace (7);
  check (value.value_or (0) == 7, "optional: emplace");

  lumex::core::atomic::smart_ptr::atomic_shared_ptr<int> cell (
      std::make_shared<int> (1));
  cell.store (std::make_shared<int> (2));
  check (*cell.load () == 2, "atomic: atomic_shared_ptr");

  std::vector<int> const span_source = { 4, 5, 6, 7 };
  lumex::core::span::view::span<int const> const view (span_source);
  check (view.size () == 4 && view.subspan (1, 2).back () == 6,
         "span: subspan");
  check (lumex::core::span::view::as_bytes (view).size () == 4 * sizeof (int),
         "span: as_bytes");

  // The span checks are on and abort a violation.
  if (argc > 0)
    {
      std::string const command = std::string ("\"") + argv[0]
                                  + "\" --violate 2> span_violation.txt";
      int const status = std::system (command.c_str ());
      std::ifstream stream ("span_violation.txt");
      std::ostringstream text;
      text << stream.rdbuf ();
#if defined(_WIN32)
      check (status != 0, "span: a violated check ends the program");
#else
      // The program must die of SIGABRT (std::system gives the wait status; a
      // shell that ran the program as a child reports 128 + SIGABRT).
      check (
          (WIFSIGNALED (status) && WTERMSIG (status) == SIGABRT)
              || (WIFEXITED (status) && WEXITSTATUS (status) == 128 + SIGABRT),
          "span: a violated check aborts the program");
#endif
      check (text.str ().find ("LUMEX_SPAN_CHECKS: index < m_storage.size ()")
                 == 0,
             "span: the message of the failed check");
    }

  std::printf ("header-only modules from the install prefix: %d failure(s)\n",
               failures);
  return failures == 0 ? 0 : 1;
}
