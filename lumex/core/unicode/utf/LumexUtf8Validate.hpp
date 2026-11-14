/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/**
 * @file LumexUtf8Validate.hpp
 * @brief Validation of UTF-8: the length of the well-formed prefix of a buffer
 * and a predicate for a whole buffer.
 * @details `utf8_valid_prefix_length` returns how many leading bytes of a
 * buffer are well-formed UTF-8 in the sense of Table 3-7 of the Unicode
 * standard (the shortest form of a scalar value: no overlong form, no
 * surrogate, nothing above U+10FFFF), and `is_valid_utf8` tells whether the
 * whole buffer is. They agree with `utf8_decoder` (`LumexUtf.hpp`), which
 * yields exactly the code points of the well-formed sequences and skips every
 * other byte: the prefix ends at the first byte the decoder would skip, so a
 * buffer is valid exactly when the decoder skips nothing. They only read and
 * allocate nothing, and never read outside `[data, data + size)`. They are
 * not a count made by the decoder, and they are faster than the decoder with
 * `utf8_counter`: the scalar routine reads ASCII eight bytes at a time, and
 * on x86 (SSSE3 or AVX2, chosen at run time by CPUID; the header needs no
 * compiler option to have them) and on AArch64 (NEON, which every AArch64 CPU
 * has) a vector routine checks 16 or 32 bytes at a time by the lookup-table
 * method of J. Keiser and D. Lemire, "Validating UTF-8 In Less Than One
 * Instruction Per Byte", Software: Practice and Experience 51 (2021). Every
 * routine gives exactly the result of the scalar one, the same prefix length
 * and not only the same answer to "valid or not": when a vector routine finds
 * a block invalid, or has no whole block left, the scalar routine continues
 * from the last start of a sequence before that block. A target without such a
 * routine (32-bit ARM, RISC-V, MSVC for ARM64, an unknown compiler) has the
 * scalar one alone, at the speed of the decoder or somewhat above. The
 * overloads for `char` serve text held in `std::string` and `char const *`,
 * and, where the compiler has it (C++20), those for `char8_t` serve
 * `std::u8string` and `u8` literals; a buffer of `char` or `char8_t` is read
 * as bytes.
 *
 * The public functions forward to the routine of the machine in `detail`.
 * `LUMEX_UNICODE_UTF8_VALIDATE_NO_SIMD`, defined before the first include of
 * this header, forces the scalar routine and keeps the vector code (and the
 * intrinsic headers) out of the translation unit. Define it for the whole
 * program in the build system, though a program may mix: every function of
 * the header (the routines, the choice of the routine and the public
 * functions) lives in an inline namespace named after the mode and after the
 * widest instruction set that the options of the unit enable (SSSE3, SSE4,
 * AVX, AVX2, AVX-512, by the predefined macros of `-mavx2`, `-march=native`,
 * `/arch:AVX2` and the like), so units with the macro and units without it,
 * and a `-mavx2` unit beside a plain one, share no function of this header
 * whatever the linker keeps and in whatever order it sees the objects. A CPU
 * without AVX that runs the plain units never runs code that the `-mavx2`
 * unit compiled with VEX encodings. This holds at every optimization level (an
 * always-inline attribute is not what it relies on: cl.exe ignores
 * `__forceinline` at `/Od`). Not covered: the standard library functions that
 * the header calls (the atomic of the dispatch), options that change the code
 * but are in no such macro (`-mbmi2`, `-mpopcnt`, `-msse3`, `/arch:SSE4.2`,
 * the extensions of AArch64), and an instruction set that a unit enables by
 * `#pragma GCC target` or a `target` attribute around the include, for which
 * the compiler defines no macro: the unit then gets the name of its command
 * line and the functions of the header in it are compiled for the wider set.
 * That hazard belongs to every inline function of every header.
 * `LUMEX_UNICODE_UTF8_VALIDATE_SIMD_X86` and
 * `LUMEX_UNICODE_UTF8_VALIDATE_SIMD_NEON` are 1 when this translation unit
 * has the routines of that architecture and 0 otherwise.
 *
 * Header-only, works from C++11.
 */
#ifndef LUMEX_CORE_UNICODE_UTF_UTF8_VALIDATE_HPP
#define LUMEX_CORE_UNICODE_UTF_UTF8_VALIDATE_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
// the pointer of the dispatch (utf8_prefix_slot) is one per shared library
// that includes this header; each copy resolves to the same routine
#if __has_warning("-Wunique-object-duplication")
#pragma clang diagnostic ignored "-Wunique-object-duplication"
#endif
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#endif

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

// Which SIMD routines this build has: x86 (SSSE3 and AVX2, chosen at run time
// by CPUID) with MSVC, clang-cl, GCC and Clang, and AArch64 (NEON, part of the
// baseline) with GCC and Clang. Every other target has the scalar routine
// only, and so has every target when LUMEX_UNICODE_UTF8_VALIDATE_NO_SIMD is
// defined, which also keeps the vector code out of the translation unit.
#if defined(LUMEX_UNICODE_UTF8_VALIDATE_NO_SIMD)
#define LUMEX_UNICODE_UTF8_VALIDATE_SIMD_X86 0
#define LUMEX_UNICODE_UTF8_VALIDATE_SIMD_NEON 0
#elif (defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__)             \
       || defined(__i386__))                                                  \
    && !defined(_M_ARM64EC)                                                   \
    && (defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__))
#define LUMEX_UNICODE_UTF8_VALIDATE_SIMD_X86 1
#define LUMEX_UNICODE_UTF8_VALIDATE_SIMD_NEON 0
#elif defined(__aarch64__) && defined(__ARM_NEON)                             \
    && (defined(__GNUC__) || defined(__clang__))
#define LUMEX_UNICODE_UTF8_VALIDATE_SIMD_X86 0
#define LUMEX_UNICODE_UTF8_VALIDATE_SIMD_NEON 1
#else
#define LUMEX_UNICODE_UTF8_VALIDATE_SIMD_X86 0
#define LUMEX_UNICODE_UTF8_VALIDATE_SIMD_NEON 0
#endif

// The entities of this header live in an inline namespace named after the mode
// (simd, or scalar when the unit has no vector routine) and after the widest
// instruction set that the options of the unit enable, as the compiler tells
// it by the predefined macros: avx512 (__AVX512F__), avx2 (__AVX2__), avx
// (__AVX__), sse4 (__SSE4_1__ or __SSE4_2__), ssse3 (__SSSE3__), and generic
// when none is defined. GCC and Clang define them for -mssse3 .. -mavx512f and
// the -march values that include them; cl.exe and clang-cl define __AVX__,
// __AVX2__ and __AVX512F__ for /arch:AVX, /arch:AVX2 and /arch:AVX512, and
// none for /arch:SSE4.2, /arch:SSE2 and /arch:IA32 (those get generic, as does
// -msse3 on GCC and Clang, and every AArch64 unit whatever its extensions,
// +sve or armv9-a for one). Units of one program built with different options
// of that list (a -mavx2 or /arch:AVX2 unit beside a plain one, or with
// LUMEX_UNICODE_UTF8_VALIDATE_NO_SIMD) then share no function of this header:
// the linker keeps one copy of an inline function whatever options built it,
// and a copy compiled for a wider set (VEX encodings, for one) must not
// replace the copy that a CPU without that set runs. Not covered: options that
// change the code but are in no macro of the list (-mbmi2, -mpopcnt, -mtune),
// and an instruction set that a unit enables by `#pragma GCC target` or
// `#pragma clang attribute push (__attribute__ ((target (...))))` around the
// include: the compiler defines no macro for it (GCC none for the pragma in
// C++), the unit gets the name of its command line, and every function of the
// header in it is compiled for the wider set. That is the hazard of every
// inline function of every header, and this header cannot see it. The names
// are spelled out in full, and not pasted from macros such as `simd` or
// `avx2`, which a program may define.
#if LUMEX_UNICODE_UTF8_VALIDATE_SIMD_X86                                      \
    || LUMEX_UNICODE_UTF8_VALIDATE_SIMD_NEON
#if defined(__AVX512F__)
#define LUMEX_UNICODE_UTF8_VALIDATE_NAMESPACE lumex_utf8_simd_avx512
#elif defined(__AVX2__)
#define LUMEX_UNICODE_UTF8_VALIDATE_NAMESPACE lumex_utf8_simd_avx2
#elif defined(__AVX__)
#define LUMEX_UNICODE_UTF8_VALIDATE_NAMESPACE lumex_utf8_simd_avx
#elif defined(__SSE4_2__)
#define LUMEX_UNICODE_UTF8_VALIDATE_NAMESPACE lumex_utf8_simd_sse4
#elif defined(__SSE4_1__)
#define LUMEX_UNICODE_UTF8_VALIDATE_NAMESPACE lumex_utf8_simd_sse4
#elif defined(__SSSE3__)
#define LUMEX_UNICODE_UTF8_VALIDATE_NAMESPACE lumex_utf8_simd_ssse3
#else
#define LUMEX_UNICODE_UTF8_VALIDATE_NAMESPACE lumex_utf8_simd_generic
#endif
#else
#if defined(__AVX512F__)
#define LUMEX_UNICODE_UTF8_VALIDATE_NAMESPACE lumex_utf8_scalar_avx512
#elif defined(__AVX2__)
#define LUMEX_UNICODE_UTF8_VALIDATE_NAMESPACE lumex_utf8_scalar_avx2
#elif defined(__AVX__)
#define LUMEX_UNICODE_UTF8_VALIDATE_NAMESPACE lumex_utf8_scalar_avx
#elif defined(__SSE4_2__)
#define LUMEX_UNICODE_UTF8_VALIDATE_NAMESPACE lumex_utf8_scalar_sse4
#elif defined(__SSE4_1__)
#define LUMEX_UNICODE_UTF8_VALIDATE_NAMESPACE lumex_utf8_scalar_sse4
#elif defined(__SSSE3__)
#define LUMEX_UNICODE_UTF8_VALIDATE_NAMESPACE lumex_utf8_scalar_ssse3
#else
#define LUMEX_UNICODE_UTF8_VALIDATE_NAMESPACE lumex_utf8_scalar_generic
#endif
#endif

#if LUMEX_UNICODE_UTF8_VALIDATE_SIMD_X86
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#include <immintrin.h>
// The functions that use SSSE3 or AVX2 instructions carry the target, so that
// no /arch or -m option is needed; cl.exe lets every intrinsic be used.
#if defined(__GNUC__) || defined(__clang__)
#define LUMEX_UNICODE_UTF8_VALIDATE_TARGET_SSSE3                              \
  __attribute__ ((target ("ssse3")))
#define LUMEX_UNICODE_UTF8_VALIDATE_TARGET_AVX2                               \
  __attribute__ ((target ("avx2")))
#else
#define LUMEX_UNICODE_UTF8_VALIDATE_TARGET_SSSE3
#define LUMEX_UNICODE_UTF8_VALIDATE_TARGET_AVX2
#endif
#elif LUMEX_UNICODE_UTF8_VALIDATE_SIMD_NEON
#include <arm_neon.h>
#endif

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace unicode
{
namespace utf
{
namespace detail
{
inline namespace LUMEX_UNICODE_UTF8_VALIDATE_NAMESPACE
{
/**
 * @brief The portable routine behind `utf8_valid_prefix_length`.
 * @details Not part of the public interface: call `utf8_valid_prefix_length`.
 * @param[in] data Bytes to check; may be null when `size` is 0.
 * @param[in] size Number of bytes.
 * @return The length of the well-formed prefix, in the range `0 .. size`.
 */
inline std::size_t
utf8_valid_prefix_length_scalar (std::uint8_t const *data,
                                 std::size_t size) LUMEX_NOEXCEPT
{
  std::size_t i = 0;

  while (i < size)
    {
      std::uint8_t const lead = data[i];

      // 0xxxxxxx
      if (lead < 0x80)
        {
          ++i;

          while (size - i >= 8)
            {
              std::uint64_t word = 0;
              std::memcpy (&word, data + i, sizeof (word));
              if ((word & 0x8080808080808080ULL) != 0)
                break;
              i += 8;
            }

          // the rest of the word that holds a non-ASCII byte, or of the data
          while (i < size && data[i] < 0x80)
            ++i;

          continue;
        }

      std::size_t const rest = size - i;

      // 110xxxxx 10xxxxxx; C0 and C1 would be overlong forms
      if (lead - 0xC2U < 0x1EU)
        {
          if (rest >= 2 && (data[i + 1] & 0xC0) == 0x80)
            {
              i += 2;
              continue;
            }

          return i;
        }

      // 1110xxxx 10xxxxxx 10xxxxxx; E0 takes A0..BF as the second byte (no
      // overlong form) and ED takes 80..9F (no surrogate)
      if (lead - 0xE0U < 0x10U)
        {
          if (rest >= 3 && ((data[i + 1] ^ 0x80) | (data[i + 2] ^ 0x80)) < 0x40
              && (lead - 0xE1U < 0xCU || lead - 0xEEU < 2U
                  || (lead == 0xE0 ? data[i + 1] >= 0xA0
                                   : data[i + 1] < 0xA0)))
            {
              i += 3;
              continue;
            }

          return i;
        }

      // 11110xxx and three 10xxxxxx; F0 takes 90..BF as the second byte (no
      // overlong form) and F4 takes 80..8F (nothing above U+10FFFF)
      if (lead - 0xF0U < 5U)
        {
          if (rest >= 4
              && ((data[i + 1] ^ 0x80) | (data[i + 2] ^ 0x80)
                  | (data[i + 3] ^ 0x80))
                     < 0x40
              && (lead != 0xF0 || data[i + 1] >= 0x90)
              && (lead != 0xF4 || data[i + 1] < 0x90))
            {
              i += 4;
              continue;
            }

          return i;
        }

      // a continuation byte, or a lead byte F5..FF
      return i;
    }

  return size;
}
// ---------------------------------------------------------------------------
// The SIMD routines
//
// The algorithm is the lookup-table validation of J. Keiser and D. Lemire,
// "Validating UTF-8 In Less Than One Instruction Per Byte", Software:
// Practice and Experience 51 (2021), https://arxiv.org/abs/2010.03090. A pair
// of neighbouring bytes (the previous one and the current one) is ill-formed
// when a class of error holds for the high nibble of the previous byte, its
// low nibble and the high nibble of the current byte; three 16-entry tables,
// one per nibble, carry one bit per class, and the bitwise AND of the three
// lookups is not zero exactly for an ill-formed pair. What a pair cannot
// tell is whether a continuation byte is required (the third and fourth byte
// of a sequence): the bytes two and three places back decide that, and the
// result is compared with the bit for "continuation after continuation".
// The code below is written from that description for this library: the
// classes and the tables follow from Table 3-7 of the Unicode standard.
//
// A block is valid when no byte of its error vector is set. A sequence may
// cross the end of a block: the block that holds its rest sees the bytes in
// front of it through the previous block, and a sequence that is still open
// at the end of the input is left to the scalar routine.
// ---------------------------------------------------------------------------

// Classes of an ill-formed pair of neighbouring bytes, one bit each.

/// A lead byte (C0..FF) not followed by a continuation byte.
LUMEX_CONSTEXPR std::uint8_t const kPairTooShort = 0x01U;
/// An ASCII byte followed by a continuation byte.
LUMEX_CONSTEXPR std::uint8_t const kPairTooLong = 0x02U;
/// E0 followed by 80..9F: a 3-byte overlong form.
LUMEX_CONSTEXPR std::uint8_t const kPairOverlong3 = 0x04U;
/// F4 followed by 90..BF, and F5..FF followed by 90..BF: above U+10FFFF.
LUMEX_CONSTEXPR std::uint8_t const kPairTooLarge = 0x08U;
/// ED followed by A0..BF: a surrogate.
LUMEX_CONSTEXPR std::uint8_t const kPairSurrogate = 0x10U;
/// C0 or C1 followed by a continuation byte: a 2-byte overlong form.
LUMEX_CONSTEXPR std::uint8_t const kPairOverlong2 = 0x20U;
/// F0 followed by 80..8F (a 4-byte overlong form) and F5..FF followed
/// by 80..8F (above U+10FFFF): one bit, since no lead byte is in both.
LUMEX_CONSTEXPR std::uint8_t const kPairLargeOrOverlong4 = 0x40U;
/// A continuation byte followed by a continuation byte: valid only as the
/// third or the fourth byte of a sequence.
LUMEX_CONSTEXPR std::uint8_t const kPairTwoContinuations = 0x80U;

// Constants of the vector routines.

/// The bits of a nibble.
LUMEX_CONSTEXPR std::uint8_t const kNibbleMask = 0x0FU;
/// The shift that brings the high nibble of a byte down.
LUMEX_CONSTEXPR int const kNibbleShift = 4;
/// 0xE0 - 0x80: a saturating subtraction of it leaves bit 7 set in a byte that
/// is E0 or above (the byte two places back of a third byte).
LUMEX_CONSTEXPR std::uint8_t const kThirdByteBias = 0x60U;
/// 0xF0 - 0x80: the same for F0 or above (three places back of a fourth byte).
LUMEX_CONSTEXPR std::uint8_t const kFourthByteBias = 0x70U;
/// Bit 7 of a byte.
LUMEX_CONSTEXPR std::uint8_t const kHighBit = 0x80U;
/// The length of a table.
LUMEX_CONSTEXPR std::size_t const kTableSize = 16U;
/// The bytes that are tested for ASCII together before the blocks are checked.
LUMEX_CONSTEXPR std::size_t const kSimdGroup = 64U;
/// The bytes of a block of the SSSE3 and NEON routines.
LUMEX_CONSTEXPR std::size_t const kNarrowBlock = 16U;
/// The bytes of a block of the AVX2 routine.
LUMEX_CONSTEXPR std::size_t const kWideBlock = 32U;
/// Below this size the AVX2 routine is the SSSE3 one.
LUMEX_CONSTEXPR std::size_t const kWideMinSize = 64U;
/// The most bytes of a sequence that a block can leave open.
LUMEX_CONSTEXPR std::size_t const kMaxOpenBytes = 3U;
/// The bits that tell a continuation byte (10xxxxxx) from the others.
LUMEX_CONSTEXPR std::uint8_t const kContinuationMask = 0xC0U;
LUMEX_CONSTEXPR std::uint8_t const kContinuationTag = 0x80U;

/// The bits of the pair class for the high nibble of the previous byte.
inline unsigned char const *
utf8_table_previous_high () LUMEX_NOEXCEPT
{
  static unsigned char const table[kTableSize] = {
    kPairTooLong,                                         // 0
    kPairTooLong,                                         // 1
    kPairTooLong,                                         // 2
    kPairTooLong,                                         // 3
    kPairTooLong,                                         // 4
    kPairTooLong,                                         // 5
    kPairTooLong,                                         // 6
    kPairTooLong,                                         // 7
    kPairTwoContinuations,                                // 8
    kPairTwoContinuations,                                // 9
    kPairTwoContinuations,                                // A
    kPairTwoContinuations,                                // B
    kPairTooShort | kPairOverlong2,                       // C
    kPairTooShort,                                        // D
    kPairTooShort | kPairOverlong3 | kPairSurrogate,      // E
    kPairTooShort | kPairTooLarge | kPairLargeOrOverlong4 // F
  };
  return table;
}

/// The bits of the pair class for the low nibble of the previous byte.
inline unsigned char const *
utf8_table_previous_low () LUMEX_NOEXCEPT
{
  // the classes that every low nibble keeps, and those of F5..FF
  std::uint8_t const carry
      = kPairTooShort | kPairTooLong | kPairTwoContinuations;
  std::uint8_t const large = carry | kPairTooLarge | kPairLargeOrOverlong4;
  static unsigned char const table[kTableSize] = {
    carry | kPairOverlong3 | kPairOverlong2 | kPairLargeOrOverlong4, // 0
    carry | kPairOverlong2,                                          // 1
    carry,                                                           // 2
    carry,                                                           // 3
    carry | kPairTooLarge,  // 4: F4 takes 80..8F
    large,                  // 5
    large,                  // 6
    large,                  // 7
    large,                  // 8
    large,                  // 9
    large,                  // A
    large,                  // B
    large,                  // C
    large | kPairSurrogate, // D
    large,                  // E
    large                   // F
  };
  return table;
}

/// The bits of the pair class for the high nibble of the current byte.
inline unsigned char const *
utf8_table_current_high () LUMEX_NOEXCEPT
{
  // a continuation byte after what is not a valid lead for it
  std::uint8_t const continuation
      = kPairTooLong | kPairOverlong2 | kPairTwoContinuations;
  static unsigned char const table[kTableSize] = {
    kPairTooShort,                                         // 0
    kPairTooShort,                                         // 1
    kPairTooShort,                                         // 2
    kPairTooShort,                                         // 3
    kPairTooShort,                                         // 4
    kPairTooShort,                                         // 5
    kPairTooShort,                                         // 6
    kPairTooShort,                                         // 7
    continuation | kPairOverlong3 | kPairLargeOrOverlong4, // 8
    continuation | kPairOverlong3 | kPairTooLarge,         // 9
    continuation | kPairSurrogate | kPairTooLarge,         // A
    continuation | kPairSurrogate | kPairTooLarge,         // B
    kPairTooShort,                                         // C
    kPairTooShort,                                         // D
    kPairTooShort,                                         // E
    kPairTooShort                                          // F
  };
  return table;
}

/// For each of the last 16 bytes of a block: the value that a byte must exceed
/// to leave a sequence open at the end of the block (the last byte C0 or
/// above, the one before E0 or above, the one before that F0 or above); a
/// limit of FF for the others, which no byte exceeds.
inline unsigned char const *
utf8_table_open_limit () LUMEX_NOEXCEPT
{
  std::uint8_t const none = 0xFFU;
  std::uint8_t const fourth_lead = 0xEFU;
  std::uint8_t const third_lead = 0xDFU;
  std::uint8_t const second_lead = 0xBFU;
  static unsigned char const table[kTableSize]
      = { none, none, none, none, none, none,        none,       none,
          none, none, none, none, none, fourth_lead, third_lead, second_lead };
  return table;
}

/// The offset at which the scalar routine takes over when a vector routine
/// has found a block invalid or has run out of whole blocks at `offset`.
/// Every byte before `offset` belongs to a well-formed sequence except for
/// those of one that is still open (at most three bytes), so the restart
/// point is the last byte of the three before `offset` that is not a
/// continuation byte (a lead byte or ASCII: the start of a sequence), or
/// `offset` itself when the three are continuation bytes of sequences that
/// ended.
inline std::size_t
utf8_restart_point (std::uint8_t const *data,
                    std::size_t offset) LUMEX_NOEXCEPT
{
  std::size_t back = 1;
  while (back <= kMaxOpenBytes && back <= offset)
    {
      if ((data[offset - back] & kContinuationMask) != kContinuationTag)
        return offset - back;
      ++back;
    }

  return offset;
}

/// The result of a vector routine that stopped at `offset`: the scalar
/// routine from the restart point on.
inline std::size_t
utf8_resume_scalar (std::uint8_t const *data, std::size_t size,
                    std::size_t offset) LUMEX_NOEXCEPT
{
  std::size_t const restart = utf8_restart_point (data, offset);
  return restart
         + utf8_valid_prefix_length_scalar (data + restart, size - restart);
}

#if LUMEX_UNICODE_UTF8_VALIDATE_SIMD_X86

/// The vectors of the SSSE3 routine.
struct utf8_ssse3_lookup
{
  __m128i previous_high;
  __m128i previous_low;
  __m128i current_high;
  __m128i open_limit;
};

LUMEX_UNICODE_UTF8_VALIDATE_TARGET_SSSE3
LUMEX_ATTRIBUTE_ALWAYS_INLINE __m128i
utf8_load_ssse3 (unsigned char const *bytes) LUMEX_NOEXCEPT
{
  return _mm_loadu_si128 (
      static_cast<__m128i const *> (static_cast<void const *> (bytes)));
}

LUMEX_UNICODE_UTF8_VALIDATE_TARGET_SSSE3
LUMEX_ATTRIBUTE_ALWAYS_INLINE utf8_ssse3_lookup
utf8_make_lookup_ssse3 () LUMEX_NOEXCEPT
{
  utf8_ssse3_lookup lookup;
  lookup.previous_high = utf8_load_ssse3 (utf8_table_previous_high ());
  lookup.previous_low = utf8_load_ssse3 (utf8_table_previous_low ());
  lookup.current_high = utf8_load_ssse3 (utf8_table_current_high ());
  lookup.open_limit = utf8_load_ssse3 (utf8_table_open_limit ());
  return lookup;
}

/// The error vector of one block: a byte that is not zero is a byte that
/// cannot follow the bytes before it. `previous` is the block before `input`.
LUMEX_UNICODE_UTF8_VALIDATE_TARGET_SSSE3
LUMEX_ATTRIBUTE_ALWAYS_INLINE __m128i
utf8_check_block_ssse3 (__m128i input, __m128i previous,
                        utf8_ssse3_lookup const &lookup) LUMEX_NOEXCEPT
{
  // the bytes one, two and three places back
  __m128i const back_1 = _mm_alignr_epi8 (input, previous, 15);
  __m128i const back_2 = _mm_alignr_epi8 (input, previous, 14);
  __m128i const back_3 = _mm_alignr_epi8 (input, previous, 13);

  __m128i const nibble = _mm_set1_epi8 (static_cast<char> (kNibbleMask));
  __m128i const high_back_1
      = _mm_and_si128 (_mm_srli_epi16 (back_1, kNibbleShift), nibble);
  __m128i const low_back_1 = _mm_and_si128 (back_1, nibble);
  __m128i const high_input
      = _mm_and_si128 (_mm_srli_epi16 (input, kNibbleShift), nibble);

  __m128i const pair = _mm_and_si128 (
      _mm_shuffle_epi8 (lookup.previous_high, high_back_1),
      _mm_and_si128 (_mm_shuffle_epi8 (lookup.previous_low, low_back_1),
                     _mm_shuffle_epi8 (lookup.current_high, high_input)));

  // bit 7 where the byte must be the third or the fourth of a sequence
  __m128i const third = _mm_subs_epu8 (
      back_2, _mm_set1_epi8 (static_cast<char> (kThirdByteBias)));
  __m128i const fourth = _mm_subs_epu8 (
      back_3, _mm_set1_epi8 (static_cast<char> (kFourthByteBias)));
  __m128i const must_continue
      = _mm_and_si128 (_mm_or_si128 (third, fourth),
                       _mm_set1_epi8 (static_cast<char> (kHighBit)));

  return _mm_xor_si128 (must_continue, pair);
}

LUMEX_UNICODE_UTF8_VALIDATE_TARGET_SSSE3
LUMEX_ATTRIBUTE_ALWAYS_INLINE bool
utf8_is_zero_ssse3 (__m128i value) LUMEX_NOEXCEPT
{
  return _mm_movemask_epi8 (_mm_cmpeq_epi8 (value, _mm_setzero_si128 ()))
         == 0xFFFF;
}

/// The offset at which the SSSE3 routine stops: the start of the group or the
/// block with an error, or the end of the last whole block. Reads only inside
/// `[data, data + size)`.
LUMEX_UNICODE_UTF8_VALIDATE_TARGET_SSSE3
inline std::size_t
utf8_scan_ssse3 (std::uint8_t const *data, std::size_t size) LUMEX_NOEXCEPT
{
  utf8_ssse3_lookup const lookup = utf8_make_lookup_ssse3 ();
  __m128i previous = _mm_setzero_si128 ();
  // not zero when the block before ends inside a sequence
  __m128i previous_open = _mm_setzero_si128 ();
  std::size_t offset = 0;

  for (; size - offset >= kSimdGroup; offset += kSimdGroup)
    {
      __m128i const block_0 = utf8_load_ssse3 (data + offset);
      __m128i const block_1 = utf8_load_ssse3 (data + offset + kNarrowBlock);
      __m128i const block_2
          = utf8_load_ssse3 (data + offset + 2 * kNarrowBlock);
      __m128i const block_3
          = utf8_load_ssse3 (data + offset + 3 * kNarrowBlock);
      __m128i error;

      if (_mm_movemask_epi8 (_mm_or_si128 (_mm_or_si128 (block_0, block_1),
                                           _mm_or_si128 (block_2, block_3)))
          == 0)
        {
          // ASCII only: ill-formed when a sequence is open from before (the
          // pair checks of the other branch see that through `previous`)
          error = previous_open;
          previous_open = _mm_setzero_si128 ();
        }
      else
        {
          error = _mm_or_si128 (
              _mm_or_si128 (utf8_check_block_ssse3 (block_0, previous, lookup),
                            utf8_check_block_ssse3 (block_1, block_0, lookup)),
              _mm_or_si128 (
                  utf8_check_block_ssse3 (block_2, block_1, lookup),
                  utf8_check_block_ssse3 (block_3, block_2, lookup)));
          previous_open = _mm_subs_epu8 (block_3, lookup.open_limit);
        }
      previous = block_3;

      if (!utf8_is_zero_ssse3 (error))
        return offset;
    }

  for (; size - offset >= kNarrowBlock; offset += kNarrowBlock)
    {
      __m128i const block = utf8_load_ssse3 (data + offset);
      __m128i error;

      if (_mm_movemask_epi8 (block) == 0)
        {
          error = previous_open;
          previous_open = _mm_setzero_si128 ();
        }
      else
        {
          error = utf8_check_block_ssse3 (block, previous, lookup);
          previous_open = _mm_subs_epu8 (block, lookup.open_limit);
        }
      previous = block;

      if (!utf8_is_zero_ssse3 (error))
        return offset;
    }

  return offset;
}

/// The routine for CPUs with SSSE3: 16 bytes at a time, ASCII 64 at a time;
/// what the vectors leave goes to the scalar routine.
LUMEX_UNICODE_UTF8_VALIDATE_TARGET_SSSE3
inline std::size_t
utf8_valid_prefix_length_ssse3 (std::uint8_t const *data,
                                std::size_t size) LUMEX_NOEXCEPT
{
  return utf8_resume_scalar (data, size, utf8_scan_ssse3 (data, size));
}

/// The vectors of the AVX2 routine.
struct utf8_avx2_lookup
{
  __m256i previous_high;
  __m256i previous_low;
  __m256i current_high;
  __m256i open_limit;
};

LUMEX_UNICODE_UTF8_VALIDATE_TARGET_AVX2
LUMEX_ATTRIBUTE_ALWAYS_INLINE __m128i
utf8_load_half_avx2 (unsigned char const *bytes) LUMEX_NOEXCEPT
{
  return _mm_loadu_si128 (
      static_cast<__m128i const *> (static_cast<void const *> (bytes)));
}

LUMEX_UNICODE_UTF8_VALIDATE_TARGET_AVX2
LUMEX_ATTRIBUTE_ALWAYS_INLINE __m256i
utf8_load_avx2 (unsigned char const *bytes) LUMEX_NOEXCEPT
{
  return _mm256_loadu_si256 (
      static_cast<__m256i const *> (static_cast<void const *> (bytes)));
}

LUMEX_UNICODE_UTF8_VALIDATE_TARGET_AVX2
LUMEX_ATTRIBUTE_ALWAYS_INLINE utf8_avx2_lookup
utf8_make_lookup_avx2 () LUMEX_NOEXCEPT
{
  // the shuffle works inside each 16-byte lane: the table is in both lanes
  utf8_avx2_lookup lookup;
  lookup.previous_high = _mm256_broadcastsi128_si256 (
      utf8_load_half_avx2 (utf8_table_previous_high ()));
  lookup.previous_low = _mm256_broadcastsi128_si256 (
      utf8_load_half_avx2 (utf8_table_previous_low ()));
  lookup.current_high = _mm256_broadcastsi128_si256 (
      utf8_load_half_avx2 (utf8_table_current_high ()));
  // only the last 16 bytes of a block can leave a sequence open
  lookup.open_limit = _mm256_inserti128_si256 (
      _mm256_set1_epi8 (static_cast<char> (0xFFU)),
      utf8_load_half_avx2 (utf8_table_open_limit ()), 1);
  return lookup;
}

LUMEX_UNICODE_UTF8_VALIDATE_TARGET_AVX2
LUMEX_ATTRIBUTE_ALWAYS_INLINE __m256i
utf8_check_block_avx2 (__m256i input, __m256i previous,
                       utf8_avx2_lookup const &lookup) LUMEX_NOEXCEPT
{
  // alignr works inside the lanes: put the high half of the previous block in
  // front of the low half of this one first
  __m256i const joined = _mm256_permute2x128_si256 (previous, input, 0x21);
  __m256i const back_1 = _mm256_alignr_epi8 (input, joined, 15);
  __m256i const back_2 = _mm256_alignr_epi8 (input, joined, 14);
  __m256i const back_3 = _mm256_alignr_epi8 (input, joined, 13);

  __m256i const nibble = _mm256_set1_epi8 (static_cast<char> (kNibbleMask));
  __m256i const high_back_1
      = _mm256_and_si256 (_mm256_srli_epi16 (back_1, kNibbleShift), nibble);
  __m256i const low_back_1 = _mm256_and_si256 (back_1, nibble);
  __m256i const high_input
      = _mm256_and_si256 (_mm256_srli_epi16 (input, kNibbleShift), nibble);

  __m256i const pair = _mm256_and_si256 (
      _mm256_shuffle_epi8 (lookup.previous_high, high_back_1),
      _mm256_and_si256 (
          _mm256_shuffle_epi8 (lookup.previous_low, low_back_1),
          _mm256_shuffle_epi8 (lookup.current_high, high_input)));

  __m256i const third = _mm256_subs_epu8 (
      back_2, _mm256_set1_epi8 (static_cast<char> (kThirdByteBias)));
  __m256i const fourth = _mm256_subs_epu8 (
      back_3, _mm256_set1_epi8 (static_cast<char> (kFourthByteBias)));
  __m256i const must_continue
      = _mm256_and_si256 (_mm256_or_si256 (third, fourth),
                          _mm256_set1_epi8 (static_cast<char> (kHighBit)));

  return _mm256_xor_si256 (must_continue, pair);
}

LUMEX_UNICODE_UTF8_VALIDATE_TARGET_AVX2
LUMEX_ATTRIBUTE_ALWAYS_INLINE bool
utf8_is_zero_avx2 (__m256i value) LUMEX_NOEXCEPT
{
  return _mm256_testz_si256 (value, value) != 0;
}

/// The offset at which the AVX2 routine stops, as `utf8_scan_ssse3`.
LUMEX_UNICODE_UTF8_VALIDATE_TARGET_AVX2
inline std::size_t
utf8_scan_avx2 (std::uint8_t const *data, std::size_t size) LUMEX_NOEXCEPT
{
  utf8_avx2_lookup const lookup = utf8_make_lookup_avx2 ();
  __m256i previous = _mm256_setzero_si256 ();
  __m256i previous_open = _mm256_setzero_si256 ();
  std::size_t offset = 0;

  for (; size - offset >= kSimdGroup; offset += kSimdGroup)
    {
      __m256i const block_0 = utf8_load_avx2 (data + offset);
      __m256i const block_1 = utf8_load_avx2 (data + offset + kWideBlock);
      __m256i error;

      if (_mm256_movemask_epi8 (_mm256_or_si256 (block_0, block_1)) == 0)
        {
          error = previous_open;
          previous_open = _mm256_setzero_si256 ();
        }
      else
        {
          error = _mm256_or_si256 (
              utf8_check_block_avx2 (block_0, previous, lookup),
              utf8_check_block_avx2 (block_1, block_0, lookup));
          previous_open = _mm256_subs_epu8 (block_1, lookup.open_limit);
        }
      previous = block_1;

      if (!utf8_is_zero_avx2 (error))
        return offset;
    }

  for (; size - offset >= kWideBlock; offset += kWideBlock)
    {
      __m256i const block = utf8_load_avx2 (data + offset);
      __m256i error;

      if (_mm256_movemask_epi8 (block) == 0)
        {
          error = previous_open;
          previous_open = _mm256_setzero_si256 ();
        }
      else
        {
          error = utf8_check_block_avx2 (block, previous, lookup);
          previous_open = _mm256_subs_epu8 (block, lookup.open_limit);
        }
      previous = block;

      if (!utf8_is_zero_avx2 (error))
        return offset;
    }

  return offset;
}

/// The routine for CPUs with AVX2: 32 bytes at a time, ASCII 64 at a time;
/// what the vectors leave goes to the scalar routine. Below `kWideMinSize`
/// bytes it is the SSSE3 routine.
LUMEX_UNICODE_UTF8_VALIDATE_TARGET_AVX2
inline std::size_t
utf8_valid_prefix_length_avx2 (std::uint8_t const *data,
                               std::size_t size) LUMEX_NOEXCEPT
{
  if (size < kWideMinSize)
    return utf8_valid_prefix_length_ssse3 (data, size);

  std::size_t const offset = utf8_scan_avx2 (data, size);
  // leave the AVX state clean for the SSE code that the caller runs next
  _mm256_zeroupper ();
  return utf8_resume_scalar (data, size, offset);
}

/// What CPUID says about the CPU and the operating system.
struct utf8_x86_features
{
  bool ssse3;
  bool avx2;
};

/// Executes CPUID for `leaf` and `subleaf`.
inline void
utf8_cpuid (unsigned int leaf, unsigned int subleaf,
            unsigned int (&registers)[4]) LUMEX_NOEXCEPT
{
#if defined(_MSC_VER)
  int info[4] = { 0, 0, 0, 0 };
  __cpuidex (info, static_cast<int> (leaf), static_cast<int> (subleaf));
  for (std::size_t i = 0; i < 4; ++i)
    registers[i] = static_cast<unsigned int> (info[i]);
#else
  __cpuid_count (leaf, subleaf, registers[0], registers[1], registers[2],
                 registers[3]);
#endif
}

/// The low 32 bits of the extended control register 0: which register states
/// the operating system saves (call it only when CPUID reports OSXSAVE).
inline unsigned int
utf8_xcr0 () LUMEX_NOEXCEPT
{
#if defined(_MSC_VER) && !defined(__clang__)
  return static_cast<unsigned int> (_xgetbv (0));
#else
  unsigned int low = 0;
  unsigned int high = 0;
  __asm__ __volatile__ ("xgetbv" : "=a"(low), "=d"(high) : "c"(0U));
  return low;
#endif
}

/// What the features are, given the answers of the CPU: the highest CPUID
/// leaf, ECX of leaf 1, EBX of leaf 7 and the low bits of XCR0 (zero when the
/// operating system does not report them). SSSE3 is bit 9 of ECX of leaf 1.
/// AVX2 is bit 5 of EBX of leaf 7 and needs the AVX bit (bit 28 of ECX of leaf
/// 1), the operating system to keep the YMM registers (OSXSAVE, bit 27 of ECX
/// of leaf 1, and bits 1 and 2 of XCR0), and SSSE3.
inline utf8_x86_features
utf8_x86_features_from (unsigned int highest_leaf, unsigned int ecx_leaf_1,
                        unsigned int ebx_leaf_7,
                        unsigned int xcr0) LUMEX_NOEXCEPT
{
  unsigned int const ssse3_in_ecx = 1U << 9;
  unsigned int const osxsave_in_ecx = 1U << 27;
  unsigned int const avx_in_ecx = 1U << 28;
  unsigned int const avx2_in_ebx = 1U << 5;
  // XCR0 bits 1 and 2: the operating system keeps the XMM and YMM registers
  unsigned int const xmm_and_ymm_state = 6U;
  unsigned int const structured_leaf = 7U;

  utf8_x86_features features;
  features.ssse3 = highest_leaf >= 1 && (ecx_leaf_1 & ssse3_in_ecx) != 0;
  bool const has_ymm_state
      = (ecx_leaf_1 & osxsave_in_ecx) != 0 && (ecx_leaf_1 & avx_in_ecx) != 0
        && (xcr0 & xmm_and_ymm_state) == xmm_and_ymm_state;
  features.avx2 = features.ssse3 && has_ymm_state
                  && highest_leaf >= structured_leaf
                  && (ebx_leaf_7 & avx2_in_ebx) != 0;
  return features;
}

/// Reads the features by CPUID (not cached: see `utf8_cpu_features`). With
/// GCC and Clang the highest leaf comes from `__get_cpuid_max`, which on a
/// 32-bit x86 first checks that the CPU has CPUID at all (the ID bit of
/// EFLAGS) and reports 0 when it has not, and the CPU then has no feature
/// here. MSVC offers no such call; its 32-bit targets run on CPUs that have
/// CPUID, as every CPU of the SSE generation does.
inline utf8_x86_features
utf8_detect_x86_features () LUMEX_NOEXCEPT
{
  unsigned int const osxsave_in_ecx = 1U << 27;
  unsigned int const structured_leaf = 7U;

  unsigned int registers[4] = { 0, 0, 0, 0 };
#if defined(_MSC_VER)
  utf8_cpuid (0, 0, registers);
  unsigned int const highest_leaf = registers[0];
#else
  unsigned int const highest_leaf = __get_cpuid_max (0, nullptr);
#endif
  if (highest_leaf < 1)
    return utf8_x86_features_from (highest_leaf, 0, 0, 0);

  utf8_cpuid (1, 0, registers);
  unsigned int const ecx_leaf_1 = registers[2];
  // XGETBV faults when the operating system has not enabled it
  unsigned int const xcr0
      = (ecx_leaf_1 & osxsave_in_ecx) != 0 ? utf8_xcr0 () : 0U;

  unsigned int ebx_leaf_7 = 0;
  if (highest_leaf >= structured_leaf)
    {
      utf8_cpuid (structured_leaf, 0, registers);
      ebx_leaf_7 = registers[1];
    }

  return utf8_x86_features_from (highest_leaf, ecx_leaf_1, ebx_leaf_7, xcr0);
}

/// The features of this CPU, read once (thread-safe).
inline utf8_x86_features const &
utf8_cpu_features () LUMEX_NOEXCEPT
{
  static utf8_x86_features const features = utf8_detect_x86_features ();
  return features;
}

/// Whether this CPU can run `utf8_valid_prefix_length_ssse3`.
inline bool
utf8_cpu_has_ssse3 () LUMEX_NOEXCEPT
{
  return utf8_cpu_features ().ssse3;
}

/// Whether this CPU and operating system can run
/// `utf8_valid_prefix_length_avx2`.
inline bool
utf8_cpu_has_avx2 () LUMEX_NOEXCEPT
{
  return utf8_cpu_features ().avx2;
}

#endif // LUMEX_UNICODE_UTF8_VALIDATE_SIMD_X86

#if LUMEX_UNICODE_UTF8_VALIDATE_SIMD_NEON

/// The vectors of the NEON routine.
struct utf8_neon_lookup
{
  uint8x16_t previous_high;
  uint8x16_t previous_low;
  uint8x16_t current_high;
  uint8x16_t open_limit;
  uint8x16_t nibble;
  uint8x16_t third_bias;
  uint8x16_t fourth_bias;
  uint8x16_t high_bit;
};

LUMEX_ATTRIBUTE_ALWAYS_INLINE utf8_neon_lookup
utf8_make_lookup_neon () LUMEX_NOEXCEPT
{
  utf8_neon_lookup lookup;
  lookup.previous_high = vld1q_u8 (utf8_table_previous_high ());
  lookup.previous_low = vld1q_u8 (utf8_table_previous_low ());
  lookup.current_high = vld1q_u8 (utf8_table_current_high ());
  lookup.open_limit = vld1q_u8 (utf8_table_open_limit ());
  lookup.nibble = vdupq_n_u8 (kNibbleMask);
  lookup.third_bias = vdupq_n_u8 (kThirdByteBias);
  lookup.fourth_bias = vdupq_n_u8 (kFourthByteBias);
  lookup.high_bit = vdupq_n_u8 (kHighBit);
  return lookup;
}

/// The error vector of one block, as `utf8_check_block_ssse3`.
LUMEX_ATTRIBUTE_ALWAYS_INLINE uint8x16_t
utf8_check_block_neon (uint8x16_t input, uint8x16_t previous,
                       utf8_neon_lookup const &lookup) LUMEX_NOEXCEPT
{
  uint8x16_t const back_1 = vextq_u8 (previous, input, 15);
  uint8x16_t const back_2 = vextq_u8 (previous, input, 14);
  uint8x16_t const back_3 = vextq_u8 (previous, input, 13);

  uint8x16_t const pair = vandq_u8 (
      vqtbl1q_u8 (lookup.previous_high, vshrq_n_u8 (back_1, 4)),
      vandq_u8 (
          vqtbl1q_u8 (lookup.previous_low, vandq_u8 (back_1, lookup.nibble)),
          vqtbl1q_u8 (lookup.current_high, vshrq_n_u8 (input, 4))));

  uint8x16_t const must_continue
      = vandq_u8 (vorrq_u8 (vqsubq_u8 (back_2, lookup.third_bias),
                            vqsubq_u8 (back_3, lookup.fourth_bias)),
                  lookup.high_bit);

  return veorq_u8 (must_continue, pair);
}

/// The offset at which the NEON routine stops, as `utf8_scan_ssse3`.
inline std::size_t
utf8_scan_neon (std::uint8_t const *data, std::size_t size) LUMEX_NOEXCEPT
{
  utf8_neon_lookup const lookup = utf8_make_lookup_neon ();
  uint8x16_t previous = vdupq_n_u8 (0);
  uint8x16_t previous_open = vdupq_n_u8 (0);
  std::size_t offset = 0;

  for (; size - offset >= kSimdGroup; offset += kSimdGroup)
    {
      uint8x16_t const block_0 = vld1q_u8 (data + offset);
      uint8x16_t const block_1 = vld1q_u8 (data + offset + kNarrowBlock);
      uint8x16_t const block_2 = vld1q_u8 (data + offset + 2 * kNarrowBlock);
      uint8x16_t const block_3 = vld1q_u8 (data + offset + 3 * kNarrowBlock);
      uint8x16_t error;

      if (vmaxvq_u8 (vorrq_u8 (vorrq_u8 (block_0, block_1),
                               vorrq_u8 (block_2, block_3)))
          < kHighBit)
        {
          error = previous_open;
          previous_open = vdupq_n_u8 (0);
        }
      else
        {
          error = vorrq_u8 (
              vorrq_u8 (utf8_check_block_neon (block_0, previous, lookup),
                        utf8_check_block_neon (block_1, block_0, lookup)),
              vorrq_u8 (utf8_check_block_neon (block_2, block_1, lookup),
                        utf8_check_block_neon (block_3, block_2, lookup)));
          previous_open = vqsubq_u8 (block_3, lookup.open_limit);
        }
      previous = block_3;

      if (vmaxvq_u8 (error) != 0)
        return offset;
    }

  for (; size - offset >= kNarrowBlock; offset += kNarrowBlock)
    {
      uint8x16_t const block = vld1q_u8 (data + offset);
      uint8x16_t error;

      if (vmaxvq_u8 (block) < kHighBit)
        {
          error = previous_open;
          previous_open = vdupq_n_u8 (0);
        }
      else
        {
          error = utf8_check_block_neon (block, previous, lookup);
          previous_open = vqsubq_u8 (block, lookup.open_limit);
        }
      previous = block;

      if (vmaxvq_u8 (error) != 0)
        return offset;
    }

  return offset;
}

/// The routine for AArch64, where NEON is part of the baseline: 16 bytes at a
/// time, ASCII 64 at a time; what the vectors leave goes to the scalar
/// routine.
inline std::size_t
utf8_valid_prefix_length_neon (std::uint8_t const *data,
                               std::size_t size) LUMEX_NOEXCEPT
{
  return utf8_resume_scalar (data, size, utf8_scan_neon (data, size));
}

#endif // LUMEX_UNICODE_UTF8_VALIDATE_SIMD_NEON

/// The routines of this header.
enum class utf8_variant : unsigned char
{
  scalar,
  ssse3,
  avx2,
  neon
};

#if LUMEX_UNICODE_UTF8_VALIDATE_SIMD_X86
/// The variant for a CPU with or without SSSE3 and AVX2 (AVX2 implies SSSE3).
inline utf8_variant
utf8_x86_variant_for (bool has_ssse3, bool has_avx2) LUMEX_NOEXCEPT
{
  if (has_ssse3 && has_avx2)
    return utf8_variant::avx2;
  if (has_ssse3)
    return utf8_variant::ssse3;
  return utf8_variant::scalar;
}
#endif

using utf8_prefix_function
    = std::size_t (*) (std::uint8_t const *, std::size_t);

/// The routine of a variant, or the scalar one when this build has none.
inline utf8_prefix_function
utf8_function_of (utf8_variant variant) LUMEX_NOEXCEPT
{
#if LUMEX_UNICODE_UTF8_VALIDATE_SIMD_X86
  if (variant == utf8_variant::avx2)
    return &utf8_valid_prefix_length_avx2;
  if (variant == utf8_variant::ssse3)
    return &utf8_valid_prefix_length_ssse3;
#endif
#if LUMEX_UNICODE_UTF8_VALIDATE_SIMD_NEON
  if (variant == utf8_variant::neon)
    return &utf8_valid_prefix_length_neon;
#endif
  static_cast<void> (variant);
  return &utf8_valid_prefix_length_scalar;
}

/// The variant that the public functions use on this machine.
inline utf8_variant
utf8_selected_variant () LUMEX_NOEXCEPT
{
#if LUMEX_UNICODE_UTF8_VALIDATE_SIMD_X86
  return utf8_x86_variant_for (utf8_cpu_has_ssse3 (), utf8_cpu_has_avx2 ());
#elif LUMEX_UNICODE_UTF8_VALIDATE_SIMD_NEON
  return utf8_variant::neon;
#else
  return utf8_variant::scalar;
#endif
}

#if LUMEX_UNICODE_UTF8_VALIDATE_SIMD_X86                                      \
    || LUMEX_UNICODE_UTF8_VALIDATE_SIMD_NEON

inline std::size_t
utf8_valid_prefix_length_resolve (std::uint8_t const *data,
                                  std::size_t size) LUMEX_NOEXCEPT;

/// The routine in use: the resolver until the first call, then the variant
/// that `utf8_selected_variant` names. Racing first calls store the same
/// value.
inline std::atomic<utf8_prefix_function> &
utf8_prefix_slot () LUMEX_NOEXCEPT
{
  static std::atomic<utf8_prefix_function> slot (
      &utf8_valid_prefix_length_resolve);
  return slot;
}

inline std::size_t
utf8_valid_prefix_length_resolve (std::uint8_t const *data,
                                  std::size_t size) LUMEX_NOEXCEPT
{
  utf8_prefix_function const chosen
      = utf8_function_of (utf8_selected_variant ());
  utf8_prefix_slot ().store (chosen, std::memory_order_relaxed);
  return chosen (data, size);
}

/// The length of the well-formed prefix by the routine of the selected
/// variant. A buffer shorter than a block goes to the scalar routine without
/// looking at the slot. The routines of the variants are called by name (the
/// slot is compared with their addresses): a call through the pointer costs a
/// second indirect branch per call, 0.5 to 1 ns, which is a quarter of the
/// time of a buffer of 16 to 64 bytes.
inline std::size_t
utf8_valid_prefix_length_selected (std::uint8_t const *data,
                                   std::size_t size) LUMEX_NOEXCEPT
{
  if (size < kNarrowBlock)
    return utf8_valid_prefix_length_scalar (data, size);

  utf8_prefix_function const chosen
      = utf8_prefix_slot ().load (std::memory_order_relaxed);
#if LUMEX_UNICODE_UTF8_VALIDATE_SIMD_X86
  if (chosen == &utf8_valid_prefix_length_avx2)
    return utf8_valid_prefix_length_avx2 (data, size);
  if (chosen == &utf8_valid_prefix_length_ssse3)
    return utf8_valid_prefix_length_ssse3 (data, size);
#endif
#if LUMEX_UNICODE_UTF8_VALIDATE_SIMD_NEON
  if (chosen == &utf8_valid_prefix_length_neon)
    return utf8_valid_prefix_length_neon (data, size);
#endif
  // the resolver, on the first call; the scalar routine otherwise
  return chosen (data, size);
}

#else

inline std::size_t
utf8_valid_prefix_length_selected (std::uint8_t const *data,
                                   std::size_t size) LUMEX_NOEXCEPT
{
  return utf8_valid_prefix_length_scalar (data, size);
}

#endif

} // inline namespace
} // namespace detail

inline namespace LUMEX_UNICODE_UTF8_VALIDATE_NAMESPACE
{
/**
 * @brief The length of the longest prefix of a buffer that is well-formed
 * UTF-8.
 * @details The prefix is made of whole well-formed sequences (Table 3-7 of the
 * Unicode standard). The result is `size` when the whole buffer is well formed
 * and otherwise the offset of the first byte that starts no well-formed
 * sequence (a stray continuation byte, `C0`, `C1`, `F5`..`FF`, an overlong
 * form, a surrogate, a value above U+10FFFF, or a sequence cut short by the
 * end of the buffer or by a byte that is not a continuation byte), which is
 * where `utf8_decoder` starts to skip. A U+0000 byte is valid. The buffer is
 * only read, and not past `size`.
 * @param[in] data Bytes to check; may be null when `size` is 0.
 * @param[in] size Number of bytes.
 * @return The length of the well-formed prefix in bytes, `0 .. size`.
 * @note Exception-safety: nothrow. Thread-safety: yes (the CPU features and
 * the choice of the routine are read once, thread-safely, and not changed
 * again).
 */
LUMEX_ATTRIBUTE_NODISCARD (
    "The length of the valid prefix is the only result of the check")
inline std::size_t
utf8_valid_prefix_length (std::uint8_t const *data,
                          std::size_t size) LUMEX_NOEXCEPT
{
  return detail::utf8_valid_prefix_length_selected (data, size);
}

/**
 * @brief The same for a buffer of `char`, which is read as bytes.
 * @param[in] data Characters to check; may be null when `size` is 0.
 * @param[in] size Number of characters.
 * @return The length of the well-formed prefix in characters, `0 .. size`.
 */
LUMEX_ATTRIBUTE_NODISCARD (
    "The length of the valid prefix is the only result of the check")
inline std::size_t
utf8_valid_prefix_length (char const *data, std::size_t size) LUMEX_NOEXCEPT
{
  return detail::utf8_valid_prefix_length_selected (
      reinterpret_cast<std::uint8_t const *> (data), size);
}

#if LUMEX_HAS_CHAR8_T
/**
 * @brief The same for a buffer of `char8_t` (C++20), which is read as bytes.
 * @param[in] data Code units to check; may be null when `size` is 0.
 * @param[in] size Number of code units.
 * @return The length of the well-formed prefix in code units, `0 .. size`.
 */
LUMEX_ATTRIBUTE_NODISCARD (
    "The length of the valid prefix is the only result of the check")
inline std::size_t
utf8_valid_prefix_length (char8_t const *data, std::size_t size) LUMEX_NOEXCEPT
{
  return detail::utf8_valid_prefix_length_selected (
      reinterpret_cast<std::uint8_t const *> (data), size);
}
#endif

/**
 * @brief Tells whether a whole buffer is well-formed UTF-8.
 * @details The same as `utf8_valid_prefix_length (data, size) == size`. An
 * empty buffer is valid.
 * @param[in] data Bytes to check; may be null when `size` is 0.
 * @param[in] size Number of bytes.
 * @return `true` when `utf8_decoder` would skip no byte of the buffer.
 * @note Exception-safety: nothrow. Thread-safety: yes (the CPU features and
 * the choice of the routine are read once, thread-safely, and not changed
 * again).
 */
LUMEX_ATTRIBUTE_NODISCARD ("Whether the buffer is valid UTF-8 is the only "
                           "result of the check")
inline bool
is_valid_utf8 (std::uint8_t const *data, std::size_t size) LUMEX_NOEXCEPT
{
  return utf8_valid_prefix_length (data, size) == size;
}

/**
 * @brief The same for a buffer of `char`, which is read as bytes.
 * @param[in] data Characters to check; may be null when `size` is 0.
 * @param[in] size Number of characters.
 * @return `true` when the whole buffer is well-formed UTF-8.
 */
LUMEX_ATTRIBUTE_NODISCARD ("Whether the buffer is valid UTF-8 is the only "
                           "result of the check")
inline bool
is_valid_utf8 (char const *data, std::size_t size) LUMEX_NOEXCEPT
{
  return utf8_valid_prefix_length (data, size) == size;
}

#if LUMEX_HAS_CHAR8_T
/**
 * @brief The same for a buffer of `char8_t` (C++20), which is read as bytes.
 * @param[in] data Code units to check; may be null when `size` is 0.
 * @param[in] size Number of code units.
 * @return `true` when the whole buffer is well-formed UTF-8.
 */
LUMEX_ATTRIBUTE_NODISCARD ("Whether the buffer is valid UTF-8 is the only "
                           "result of the check")
inline bool
is_valid_utf8 (char8_t const *data, std::size_t size) LUMEX_NOEXCEPT
{
  return utf8_valid_prefix_length (data, size) == size;
}
#endif

} // inline namespace

} // namespace utf
} // namespace unicode
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_UNICODE_UTF_UTF8_VALIDATE_HPP
