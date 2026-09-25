// LumexFormatDifferential.tests.cpp
// Differential fuzzing against std::format: a deterministic generator builds
// tens of thousands of random format specifications from the grammar
// [[fill]align][sign][#][0][width][.precision][type] (valid and invalid
// ones) and applies them to edge-case values of every built-in kind. For
// every pair LumexFormat must either produce exactly std::format's text or
// fail exactly when std::format fails. Runs where the standard library has
// <format> (the C++20 suite on MSVC); `?` is C++23 and is not generated.
#if __cplusplus >= 202002L && defined(__has_include)
#if __has_include(<format>)
#include <format>
#endif
#endif

#include <climits>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/fmt/LumexFormat.hpp"

#if defined(__cpp_lib_format) && __cpp_lib_format >= 201907L

namespace fmt = lumex::core::fmt;

namespace
{
/** Small deterministic PRNG (xorshift64*), so failures reproduce. */
class Random
{
public:
  explicit Random (std::uint64_t seed) : _state (seed) {}

  std::uint64_t
  next ()
  {
    _state ^= _state >> 12;
    _state ^= _state << 25;
    _state ^= _state >> 27;
    return _state * 2685821657736338717ULL;
  }

  int
  below (int limit)
  {
    return static_cast<int> (next () % static_cast<std::uint64_t> (limit));
  }

  bool
  chance (int percent)
  {
    return below (100) < percent;
  }

private:
  std::uint64_t _state;
};

/** A random specification; `types` holds the presentation types to draw. */
std::string
random_spec (Random &random, std::string const &types)
{
  static char const *const fills[] = { "*", "0", " ", "x", "\xD0\xB6" };
  static char const aligns[] = { '<', '>', '^' };
  static char const signs[] = { '+', '-', ' ' };
  std::string spec = "{:";
  if (random.chance (35))
    {
      if (random.chance (50))
        spec += fills[random.below (5)];
      spec += aligns[random.below (3)];
    }
  if (random.chance (25))
    spec += signs[random.below (3)];
  if (random.chance (20))
    spec += '#';
  if (random.chance (20))
    spec += '0';
  if (random.chance (50))
    spec += std::to_string (random.below (random.chance (10) ? 60 : 16));
  if (random.chance (30))
    spec += "." + std::to_string (random.below (random.chance (10) ? 40 : 12));
  if (random.chance (70) && !types.empty ())
    spec += types[static_cast<std::size_t> (
        random.below (static_cast<int> (types.size ())))];
  spec += '}';
  return spec;
}

/** Every type letter the grammar allows, so invalid combinations appear. */
std::string const all_types = "bBcdoxXeEfFgGaAsp";

struct outcome_t
{
  bool failed;
  std::string text;
};

template <typename T>
outcome_t
with_std (std::string const &spec, T const &value)
{
  try
    {
      return outcome_t{ false,
                        std::vformat (spec, std::make_format_args (value)) };
    }
  catch (std::format_error const &)
    {
      return outcome_t{ true, std::string () };
    }
}

template <typename T>
outcome_t
with_lumex (std::string const &spec, T const &value)
{
  try
    {
      return outcome_t{ false,
                        fmt::vformat (spec, fmt::make_format_args (value)) };
    }
  catch (fmt::FormatError const &)
    {
      return outcome_t{ true, std::string () };
    }
}

/**
 * Formats `value` with `count` random specifications and reports every
 * difference (at most `max_reports`, so one bug does not flood the log).
 */
template <typename T>
int
compare_random_specs (std::uint64_t seed, T const &value, char const *label,
                      int count, std::string const &types = all_types)
{
  Random random (seed);
  int differences = 0;
  int const max_reports = 5;
  for (int i = 0; i < count; ++i)
    {
      std::string const spec = random_spec (random, types);
      outcome_t const expected = with_std (spec, value);
      outcome_t const actual = with_lumex (spec, value);
      bool const same = expected.failed == actual.failed
                        && (expected.failed || expected.text == actual.text);
      if (!same)
        {
          ++differences;
          if (differences <= max_reports)
            ADD_FAILURE () << label << " spec " << spec << ": std="
                           << (expected.failed ? "<error>"
                                               : "[" + expected.text + "]")
                           << " lumex="
                           << (actual.failed ? "<error>"
                                             : "[" + actual.text + "]");
        }
    }
  return differences;
}
} // namespace

TEST (LumexFormatDifferentialTest, GivenIntegers_WhenRandomSpecs_ThenSameAsStd)
{
  long long const values[]
      = { 0,   1,       -1,      42,        -42,       127,      128,    255,
          256, INT_MAX, INT_MIN, LLONG_MAX, LLONG_MIN, 0x10FFFF, 1000000 };
  int differences = 0;
  std::uint64_t seed = 1;
  for (long long const value : values)
    {
      differences += compare_random_specs (seed++, value, "long long", 1500);
      differences += compare_random_specs (seed++, static_cast<int> (value),
                                           "int", 500);
    }
  unsigned long long const unsigned_values[]
      = { 0ull, 1ull, 255ull, ULLONG_MAX };
  for (unsigned long long const value : unsigned_values)
    differences
        += compare_random_specs (seed++, value, "unsigned long long", 1500);
  differences += compare_random_specs (
      seed++, static_cast<unsigned char> (200), "unsigned char", 1500);
  differences
      += compare_random_specs (seed++, static_cast<short> (-7), "short", 1500);
  EXPECT_EQ (differences, 0);
}

TEST (LumexFormatDifferentialTest,
      GivenFloatingPoint_WhenRandomSpecs_ThenSameAsStd)
{
  double const values[] = { 0.0,
                            -0.0,
                            1.0,
                            -1.5,
                            0.1,
                            0.5,
                            2.5,
                            1e-7,
                            123456.789,
                            1e21,
                            1e22,
                            9.999999999999999e22,
                            5e-324,
                            2.2250738585072014e-308,
                            1.7976931348623157e308,
                            std::numeric_limits<double>::infinity (),
                            -std::numeric_limits<double>::infinity (),
                            std::numeric_limits<double>::quiet_NaN () };
  int differences = 0;
  std::uint64_t seed = 1000;
  for (double const value : values)
    differences += compare_random_specs (seed++, value, "double", 2000);
  float const float_values[] = { 0.1f, 3.4028235e38f, 1e-45f, 16777217.0f };
  for (float const value : float_values)
    differences += compare_random_specs (seed++, value, "float", 2000);
  long double const long_values[] = { 0.1l, -2.5l, 1e300l };
  for (long double const value : long_values)
    differences += compare_random_specs (seed++, value, "long double", 1000);
  EXPECT_EQ (differences, 0);
}

TEST (LumexFormatDifferentialTest,
      GivenTextAndOthers_WhenRandomSpecs_ThenSameAsStd)
{
  int differences = 0;
  std::uint64_t seed = 5000;
  std::string const strings[] = { "",
                                  "a",
                                  "text",
                                  "\xD1\x82\xD1\x83\xD0\xB4\xD0\xB0",
                                  "\xE4\xB8\xAD\xE5\xBF\x83",
                                  "\xF0\x9F\x90\xB1x",
                                  std::string ("a\0b", 3) };
  for (std::string const &value : strings)
    differences += compare_random_specs (seed++, value, "string", 2000);
  differences
      += compare_random_specs (seed++, "c-string", "char const *", 2000);
  char const chars[] = { 'a', '\0', '\n', '\x7f', '\xff' };
  for (char const value : chars)
    differences += compare_random_specs (seed++, value, "char", 2000);
  differences += compare_random_specs (seed++, true, "bool", 2000);
  differences += compare_random_specs (seed++, false, "bool", 2000);
  differences += compare_random_specs (
      seed++, static_cast<void const *> (&seed), "pointer", 2000);
  differences += compare_random_specs (seed++, nullptr, "nullptr", 2000);
  EXPECT_EQ (differences, 0);
}

#endif
