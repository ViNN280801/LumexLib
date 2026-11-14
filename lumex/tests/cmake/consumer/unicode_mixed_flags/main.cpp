// Calls the units of the fixture (see CMakeLists.txt). Exit code 0 when every
// unit selected the routine that it must and every result was the same.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "unit.hpp"

#include "lumex/core/unicode/utf/LumexUtf8Validate.hpp"

namespace detail = lumex::core::unicode::utf::detail;

namespace
{
bool
report (char const *name, unit_result const &result, int expected_variant,
        std::size_t expected_prefix)
{
  bool const ok = result.variant == expected_variant
                  && result.prefix == expected_prefix
                  && result.valid == (expected_prefix == 120 ? 1 : 0);
  std::printf ("%-12s variant %d (wanted %d), prefix %zu (wanted %zu), "
               "valid %d: %s",
               name, result.variant, expected_variant, result.prefix,
               expected_prefix, result.valid, ok ? "ok" : "WRONG");
  std::printf (" |");
  return ok;
}
} // namespace

int
main ()
{
#if LUMEX_UNICODE_UTF8_VALIDATE_SIMD_X86
  // 100 ASCII bytes, 50 bytes of two-byte characters, a byte that no
  // sequence takes, ASCII again: the prefix is 150
  char buffer[200];
  for (std::size_t i = 0; i < sizeof (buffer); ++i)
    buffer[i] = 'a';
  for (std::size_t i = 100; i < 150; i += 2)
    {
      buffer[i] = static_cast<char> (0xC3);
      buffer[i + 1] = static_cast<char> (0xA9);
    }
  buffer[150] = static_cast<char> (0xFF);

  int const best = static_cast<int> (detail::utf8_x86_variant_for (
      detail::utf8_cpu_has_ssse3 (), detail::utf8_cpu_has_avx2 ()));
  int const scalar = static_cast<int> (detail::utf8_variant::scalar);
  int const avx2 = static_cast<int> (detail::utf8_variant::avx2);

  bool ok = true;
  // the long text first, then a valid text of 120 bytes (the 100 ASCII bytes
  // and ten two-byte characters), so that the resolver of every unit runs
  // with the buffer of its own
  ok = report ("no_simd", no_simd_unit (buffer, 200), scalar, 150) && ok;
  ok = report ("plain", plain_unit (buffer, 200), best, 150) && ok;
  ok = report ("plain 120", plain_unit (buffer, 120), best, 120) && ok;
  ok = report ("no_simd 120", no_simd_unit (buffer, 120), scalar, 120) && ok;
  // the unit built for AVX2 runs only on a CPU that has it
  if (detail::utf8_cpu_has_avx2 ())
    {
      ok = report ("isa", isa_unit (buffer, 200), avx2, 150) && ok;
      ok = report ("isa 120", isa_unit (buffer, 120), avx2, 120) && ok;
    }
  // the unit built for AVX2 and the plain unit have each their own SSSE3
  // routine: with one copy for both (a key that names no instruction set) the
  // addresses are equal, on any CPU and without an emulator
  std::uintptr_t const plain_address = plain_unit_ssse3_address ();
  std::uintptr_t const isa_address = isa_unit_ssse3_address ();
  bool const distinct
      = plain_address != 0 && isa_address != 0 && plain_address != isa_address;
  std::printf (
      "SSSE3 routines of the plain unit and of the AVX2 unit are two: "
      "%s |",
      distinct ? "ok" : "WRONG");
  ok = distinct && ok;

#if !defined(_MSC_VER)
  // the model of the CPU that the case runs on, named by the test: a case
  // that is meant for an emulated CPU fails when it runs on the host
  char const *const wanted = std::getenv ("LUMEX_FIXTURE_EXPECT_VARIANT");
  if (wanted != nullptr)
    {
      int wanted_variant = scalar;
      if (std::strcmp (wanted, "avx2") == 0)
        wanted_variant = avx2;
      else if (std::strcmp (wanted, "ssse3") == 0)
        wanted_variant = static_cast<int> (detail::utf8_variant::ssse3);
      bool const as_wanted = best == wanted_variant;
      std::printf ("the CPU gets variant %d, the test wants %s: %s |", best,
                   wanted, as_wanted ? "ok" : "WRONG");
      ok = as_wanted && ok;
    }
#endif
  std::printf ("\n");
  return ok ? 0 : 1;
#else
  std::printf ("no x86 routines in this build\n");
  return 0;
#endif
}
