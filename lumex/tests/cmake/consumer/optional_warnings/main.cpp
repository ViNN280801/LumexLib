// Every form of optional::value () and the other observers, called in a
// translation unit that a strict consumer compiles (see CMakeLists.txt). Exit
// code 0 when every check held.

#include <cstdio>
#include <string>
#include <utility>

#include "lumex/core/optional/LumexOptional"

namespace lo = lumex::core::optional::opt;

namespace
{
using number_t = lo::optional<int>;
using text_t = lo::optional<std::string>;

// value () of a const lvalue and of a mutable lvalue: the observers whose
// error branch ended in a comma operand (C4702 under cl /W4).
int
read_const (number_t const &number)
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

int
read_const_rvalue (number_t const &&number)
{
  return std::move (number).value ();
}

std::string
read_text (text_t const &text)
{
  return text.value ();
}

std::string &
text_reference (text_t &text)
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
  catch (lo::lumex_bad_optional_access const &)
    {
      return true;
    }
  return false;
}

// The constexpr tiers: value () const & from C++11, value () & from C++14.
constexpr number_t kconstant (12);
constexpr int kconstant_value = kconstant.value ();

#if __cplusplus >= 201402L
constexpr int
constexpr_mutable ()
{
  number_t number (11);
  number.value () = 13;
  return number.value ();
}
constexpr int kmutable_value = constexpr_mutable ();
#endif
} // namespace

int
main ()
{
  int failures = 0;

  number_t const good (42);
  number_t const empty_number;
  number_t mutable_good (7);
  text_t mutable_text (std::string ("text"));
  text_t const text (std::string ("text"));
  text_t const empty_text;

  failures += missed (read_const (good) == 42);
  failures += missed (read_mutable (mutable_good) == 7);
  failures += missed (read_rvalue (number_t (9)) == 9);
  failures += missed (read_const_rvalue (number_t (5)) == 5);
  failures += missed (read_text (text) == "text");
  failures += missed (text_reference (mutable_text) == "text");
  failures += missed (*good == 42);
  failures += missed (good.value_or (0) == 42);
  failures += missed (empty_number.value_or (3) == 3);
  failures += missed (kconstant_value == 12);
#if __cplusplus >= 201402L
  failures += missed (kmutable_value == 13);
#endif

  failures += missed (
      throws ([&empty_number] { return read_const (empty_number); }));
  failures
      += missed (throws ([&empty_text] { return read_text (empty_text); }));
  failures += missed (throws (
      []
        {
          number_t empty_mutable;
          return read_mutable (empty_mutable);
        }));
  failures += missed (throws ([] { return read_rvalue (number_t ()); }));
  failures += missed (throws ([] { return read_const_rvalue (number_t ()); }));

  if (failures != 0)
    {
      std::printf ("optional_warnings: %d check(s) failed\n", failures);
      return 1;
    }
  std::printf ("optional_warnings: every observer compiled and held\n");
  return 0;
}
