// The numeric traits of LumexMath, built by cmake.hygiene_compile_checks at
// every standard and under the switches that add or remove char8_t and the
// native wchar_t: which types count as numbers must follow the compiler's own
// feature macro (__cpp_char8_t) and the real identity of wchar_t, not the
// language standard.
//   LUMEX_HYGIENE_EXPECT_CHAR8     the compiler must have char8_t (C++17 with
//                                  -fchar8_t or /Zc:char8_t)
//   LUMEX_HYGIENE_EXPECT_NO_CHAR8  it must not (C++20 with -fno-char8_t or
//                                  /Zc:char8_t-)
//   LUMEX_HYGIENE_NO_NATIVE_WCHAR  MSVC /Zc:wchar_t-: wchar_t is unsigned
//                                  short and unsigned short stays numeric

#include <cstdint>
#include <type_traits>

#include "lumex/core/math/LumexMath"

#ifdef LUMEX_HYGIENE_EXPECT_CHAR8
#ifndef __cpp_char8_t
#error "the compiler was asked for char8_t and has none"
#endif
#endif
#ifdef LUMEX_HYGIENE_EXPECT_NO_CHAR8
#ifdef __cpp_char8_t
#error "the compiler was asked to drop char8_t and still has it"
#endif
#endif

namespace
{
using lumex::core::math::ops::traits::is_numeric;

static_assert (is_numeric<short>::value, "short");
static_assert (is_numeric<unsigned short>::value, "unsigned short");
static_assert (is_numeric<int>::value, "int");
static_assert (is_numeric<unsigned>::value, "unsigned");
static_assert (is_numeric<long>::value, "long");
static_assert (is_numeric<unsigned long>::value, "unsigned long");
static_assert (is_numeric<long long>::value, "long long");
static_assert (is_numeric<unsigned long long>::value, "unsigned long long");
static_assert (is_numeric<std::uint16_t>::value, "uint16_t");
static_assert (is_numeric<float const>::value, "float const");
static_assert (is_numeric<double>::value, "double");

static_assert (!is_numeric<bool>::value, "bool");
static_assert (!is_numeric<char>::value, "char");
static_assert (!is_numeric<signed char>::value, "signed char");
static_assert (!is_numeric<unsigned char>::value, "unsigned char");
static_assert (!is_numeric<char16_t>::value, "char16_t");
static_assert (!is_numeric<char32_t>::value, "char32_t");
#ifdef __cpp_char8_t
static_assert (!is_numeric<char8_t>::value, "char8_t");
static_assert (!is_numeric<char8_t const>::value, "char8_t const");
#endif
#ifdef LUMEX_HYGIENE_NO_NATIVE_WCHAR
// The flag took effect: wchar_t is unsigned short, which stays numeric.
static_assert (std::is_same<wchar_t, unsigned short>::value,
               "/Zc:wchar_t- makes wchar_t unsigned short");
#else
static_assert (!is_numeric<wchar_t>::value, "wchar_t");
#endif
} // namespace
