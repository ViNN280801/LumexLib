// expected next to std::expected (C++23): the same T and E convert both ways
// without a cast. Below C++23 the standard library has no std::expected and
// the program says so.
#include <iostream>
#include <string>

// The standard check comes first: below C++23 the MSVC STL warns that
// <expected> is empty (STL4038).
#if __cplusplus > 202002L && __has_include(<expected>)
#include <expected>
#endif

#include "lumex/core/expected/Expected"

#if LUMEX_EXPECTED_HAS_STD_CONVERSION

namespace ex = lumex::core::expected::result;

using lumex_result_t = ex::expected<int, std::string>;
using std_result_t = std::expected<int, std::string>;

// A function of a library that uses std::expected.
std_result_t
parse_with_std (char const digit)
{
  if (digit < '0' || digit > '9')
    return std::unexpected<std::string> ("not a digit");
  return digit - '0';
}

// A function that takes this library's expected.
int
twice (lumex_result_t const &parsed)
{
  return parsed.has_value () ? parsed.value () * 2 : -1;
}

int
main ()
{
  // std::expected to expected: implicit, the value or the error is copied
  // (moved from an rvalue).
  std::cout << twice (parse_with_std ('4')) << '\n'; // 8
  std::cout << twice (parse_with_std ('x')) << '\n'; // -1

  // expected to std::expected: implicit as well.
  lumex_result_t const mine = ex::failure (std::string ("kept"));
  std_result_t const theirs = mine;
  std::cout << theirs.error () << '\n'; // kept
  return 0;
}

#else

int
main ()
{
  std::cout << "std::expected is not available (C++23 is needed)\n";
  return 0;
}

#endif
