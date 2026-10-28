// Every observer of expected, value () of a const object included, called in a
// translation unit that a strict consumer compiles (see CMakeLists.txt). Exit
// code 0 when every check held.

#include <cstdio>
#include <string>
#include <system_error>
#include <utility>

#include "lumex/core/expected/Expected"

namespace lx = lumex::core::expected;
using lx::result::expected;

namespace
{
using number_t = expected<int, std::error_code>;
using text_t = expected<std::string, std::error_code>;
using flag_t = expected<void, std::error_code>;

std::error_code
failure ()
{
  return std::make_error_code (std::errc::invalid_argument);
}

// value () of a const lvalue: the observer whose error branch ended in a
// comma operand (C4702 under cl /W4).
int
read_const (number_t const &number)
{
  return number.value ();
}

int
read_const_rvalue (number_t const &&number)
{
  return number.value ();
}

int
read_mutable (number_t &number)
{
  return number.value ();
}

int
read_rvalue (number_t &&number)
{
  return std::move (number).value ();
}

std::string
read_text (text_t const &text)
{
  return text.value ();
}

// 0 when `ok`, else 1. A call, so that a strict compiler does not fold a
// comparison of the constants above into a conditional with a dead arm.
int
missed (bool ok)
{
  return ok ? 0 : 1;
}

template <typename Observer>
bool
throws (Observer const &observer)
{
  try
    {
      static_cast<void> (observer ());
    }
  catch (lx::error::bad_expected_access<std::error_code> const &)
    {
      return true;
    }
  return false;
}
} // namespace

int
main ()
{
  int failures = 0;

  number_t const good (42);
  number_t const bad (lx::result::unexpect, failure ());
  number_t mutable_good (7);
  text_t const text (std::string ("text"));
  text_t const bad_text (lx::result::unexpect, failure ());
  flag_t const flag;
  flag_t const bad_flag (lx::result::unexpect, failure ());

  failures += missed (read_const (good) == 42);
  failures += missed (read_const_rvalue (number_t (5)) == 5);
  failures += missed (read_mutable (mutable_good) == 7);
  failures += missed (read_rvalue (number_t (9)) == 9);
  failures += missed (read_text (text) == "text");
  failures += missed (*good == 42);
  failures += missed (good.value_or (0) == 42);
  failures += missed (bad.value_or (3) == 3);
  failures += missed (bad.error () == failure ());

  failures += missed (throws ([&bad] { return read_const (bad); }));
  failures += missed (throws ([&bad_text] { return read_text (bad_text); }));
  failures += missed (throws (
      [&bad_flag]
        {
          bad_flag.value ();
          return 0;
        }));
  flag.value ();

  if (failures != 0)
    {
      std::printf ("expected_warnings: %d check(s) failed\n", failures);
      return 1;
    }
  std::printf ("expected_warnings: every observer compiled and held\n");
  return 0;
}
