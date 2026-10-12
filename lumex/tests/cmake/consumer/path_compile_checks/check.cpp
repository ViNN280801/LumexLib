// Compile checks of lumex::path, built by cmake.path_compile_checks. Exactly
// one of LUMEX_PATH_GOOD_CASE or LUMEX_PATH_BAD_CASE=<n> is defined; the
// fixture compiles with warnings as errors. Since 2.0.0.0 a lumex path
// converts to std::string only explicitly (an implicit conversion made
// `std_path = lumex_path;` ambiguous on POSIX and, with libstdc++ 8, every
// direct initialization of a std::filesystem::path from a lumex path), so
// every bad case is an implicit conversion to std::string that must not
// compile at any standard; the good case holds the explicit forms, the
// conversions that stay implicit and, from C++17, the three ways to make a
// std::filesystem::path from a lumex path.

#include <string>
#include <utility>

#include "lumex/core/filesystem/LumexFilesystem"

using lumex::core::filesystem::fs::path;

namespace
{
std::size_t
take_string (std::string const &text)
{
  return text.size ();
}

std::size_t
take_string_by_value (std::string text)
{
  return text.size ();
}

std::string
give_string (path const &value)
{
#if defined(LUMEX_PATH_BAD_CASE) && LUMEX_PATH_BAD_CASE == 5
  // A return statement is a copy initialization.
  return value;
#else
  return value.string ();
#endif
}

std::size_t
take_path (path const &value)
{
  return value.string ().size ();
}
} // namespace

int
main ()
{
  path const value ("dir/file.txt");
  std::size_t used = 0;
#if defined(LUMEX_PATH_GOOD_CASE)
  // The explicit forms.
  std::string const by_member = value.string ();
  std::string const by_constructor (value);
  std::string const by_cast = static_cast<std::string> (value);
  std::string const by_braces{ value };
  used += take_string (value.string ());
  used += take_string (std::string (value));
  used += take_string_by_value (static_cast<std::string> (value));
  used += give_string (value).size ();
  used += by_member.size () + by_constructor.size () + by_cast.size ()
          + by_braces.size ();
  // What converts to a path implicitly still does.
  path const from_string = std::string ("a/b");
  path const from_literal = "a/b";
  path assigned;
  assigned = std::string ("c");
  assigned = "d";
  used += take_path (std::string ("a/b")) + take_path ("a/b");
  used += from_string.string ().size () + from_literal.string ().size ()
          + assigned.string ().size ();
  used += (value == std::string ("dir/file.txt")) ? 1U : 0U;
  used += (value / "x").string ().size ();
#if LUMEX_HAS_STD_PATH_CONVERSION
  // The three ways to make a standard path from a lumex path, and the
  // assignment: ambiguous on POSIX (and, with libstdc++ 8, the direct
  // initialization) while the conversion to std::string was implicit.
  std::filesystem::path const copied = value;
  std::filesystem::path const direct (value);
  std::filesystem::path const braced{ value };
  std::filesystem::path assigned_std;
  assigned_std = value;
  used += copied.string ().size () + direct.string ().size ()
          + braced.string ().size () + assigned_std.string ().size ();
#endif
#elif LUMEX_PATH_BAD_CASE == 1
  // A copy initialization of a string.
  std::string const text = value;
  used += text.size ();
#elif LUMEX_PATH_BAD_CASE == 2
  // An argument of a parameter that takes a constant reference to a string.
  used += take_string (value);
#elif LUMEX_PATH_BAD_CASE == 3
  // An argument of a parameter that takes a string by value.
  used += take_string_by_value (value);
#elif LUMEX_PATH_BAD_CASE == 4
  // An assignment to a string.
  std::string text;
  text = value;
  used += text.size ();
#elif LUMEX_PATH_BAD_CASE == 5
  // The return statement of give_string above.
  used += give_string (value).size ();
#elif LUMEX_PATH_BAD_CASE == 6
  // Appending to a string.
  std::string text ("x");
  text += value;
  used += text.size ();
#elif LUMEX_PATH_BAD_CASE == 7
  // A conditional expression with a string: one operand converts to the other
  // only implicitly.
  std::string const other ("x");
  std::string const text = used == 0 ? value : other;
  used += text.size ();
#else
#error "LUMEX_PATH_BAD_CASE is not one of the cases of check.cpp"
#endif
  return used == 12345 ? 1 : 0;
}
