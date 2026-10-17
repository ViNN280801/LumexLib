// Built with exceptions: at () must throw std::out_of_range and operator<<
// must set the badbit, whichever unit the linker meets first.
// Exit code 0 on success.
#include <cstdio>
#include <ostream>
#include <stdexcept>
#include <streambuf>

#include "lumex/core/string_view/LumexStringView"
#include "lumex/core/utility/attr/LumexAttributes.hpp"

#include "lib.hpp"

namespace
{
// A stream buffer that refuses every character: the inserter must set the
// badbit and, with exceptions enabled, catch whatever the buffer throws.
struct failing_buf : std::streambuf
{
  int_type
  overflow (int_type) override
  {
    throw std::runtime_error ("buffer");
  }
  std::streamsize
  xsputn (char const *, std::streamsize) override
  {
    throw std::runtime_error ("buffer");
  }
};
} // namespace

int
main (int argc, char **)
{
  using lumex::core::string_view::view::lumex_string_view;
  int failures = 0;
  lumex_string_view const view ("abc");
  if (argc > 5) // never taken: only keeps the call of the other unit linked
    {
      failing_buf unused;
      std::ostream sink (&unused);
      lib_print (sink, view);
    }
  if (lib_first (view) != 'a')
    {
      std::fputs ("lib_first failed\n", stderr);
      ++failures;
    }
  bool threw = false;
  try
    {
      LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (view.at (99));
    }
  catch (std::out_of_range const &)
    {
      threw = true;
    }
  if (!threw)
    {
      std::fputs ("at () did not throw std::out_of_range\n", stderr);
      ++failures;
    }
  failing_buf buf;
  std::ostream out (&buf);
  try
    {
      out << view;
    }
  catch (...)
    {
      std::fputs ("an exception escaped operator<<\n", stderr);
      ++failures;
    }
  if (!out.bad ())
    {
      std::fputs ("operator<< did not set the badbit\n", stderr);
      ++failures;
    }
  if (failures == 0)
    std::puts ("string_view mixed_exceptions: ok");
  return failures == 0 ? 0 : 1;
}
