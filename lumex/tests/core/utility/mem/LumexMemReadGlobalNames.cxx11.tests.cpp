// LumexMemRead.hpp declares nothing at global scope. A program that already
// has global declarations named optional, nullopt, make_optional,
// lumex_bad_optional_access (the optional module puts them at global scope
// only through its umbrella, which the header must not include), span,
// dynamic_extent, byte, as_bytes, as_writable_bytes and to_integer (the span
// module declares them in lumex::core::span::view) must still compile when it
// includes this header, which uses the optional and the span of the library
// and which the utility umbrella includes. A global declaration of any of
// those names in LumexLib makes this file fail to compile. The declarations
// below stand in for that program, so they carry the names they must not
// clash with rather than this library's naming rules. The header is included
// on its own: the utility umbrella includes <windows.h> on Windows, whose
// global byte would clash with the declaration below.
#include <cstddef>
#include <cstdint>

#include <gtest/gtest.h>

#include "lumex/core/utility/mem/LumexMemRead.hpp"

// The program's declarations.
template <typename T> struct optional
{
  T value;
  bool engaged;
};

static const int nullopt = 1;

template <typename T>
optional<T>
make_optional (T value)
{
  optional<T> result;
  result.value = value;
  result.engaged = true;
  return result;
}

struct lumex_bad_optional_access
{
};

template <typename T> struct span
{
  T *first;
  std::size_t count;
};

static const std::size_t dynamic_extent = 7;

enum class byte
{
  low,
  high
};

static int
as_bytes (int value)
{
  return value + 1;
}

static int
as_writable_bytes (int value)
{
  return value + 2;
}

static int
to_integer (int value)
{
  return value + 3;
}

TEST (LumexMemReadGlobalNamesTest,
      GivenProgramWithOwnGlobalNames_WhenIncludeMemRead_ThenBothWork)
{
  optional<int> const mine = make_optional (4);
  EXPECT_TRUE (mine.engaged);
  EXPECT_EQ (mine.value, 4);
  EXPECT_EQ (nullopt, 1);
  EXPECT_EQ (dynamic_extent, 7u);
  EXPECT_EQ (as_bytes (1) + as_writable_bytes (1) + to_integer (1), 9);
  span<int> const view = { nullptr, 0 };
  EXPECT_EQ (view.count, 0u);
  EXPECT_EQ (static_cast<int> (byte::high), 1);

  std::uint32_t const value = 0x01020304u;
  auto const result
      = lumex::core::utility::mem::as<std::uint32_t> (&value, sizeof (value));
  ASSERT_TRUE (result.has_value ());
  EXPECT_EQ (*result, value);
}
