#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "lumex/core/unicode/LumexUnicode"
#include "lumex/core/utility/bit/LumexBit.hpp"

// Workflow: a program reads text files that arrive in different encodings
// (UTF-16 and UTF-32 with a byte order mark, UTF-8, and Latin-1 when the
// caller knows it), turns them into UTF-8 for its log, and writes one log
// line back as UTF-16 big endian.
namespace
{
namespace utf = lumex::core::unicode::utf;
using lumex::core::utility::bit::byte_swap;
using lumex::core::utility::bit::is_little_endian;

enum class file_encoding
{
  utf8,
  utf16_le,
  utf16_be,
  utf32_le,
  utf32_be
};

struct detected_t
{
  file_encoding encoding;
  std::size_t bom_size;
};

int failures = 0;

void
check (bool condition, char const *what)
{
  std::cout << (condition ? "  ok   " : "  FAIL ") << what << '\n';
  if (!condition)
    ++failures;
}

// The byte order mark decides; a file without one is taken as UTF-8.
detected_t
detect (std::vector<std::uint8_t> const &bytes)
{
  std::size_t const n = bytes.size ();
  if (n >= 4 && bytes[0] == 0xFF && bytes[1] == 0xFE && bytes[2] == 0
      && bytes[3] == 0)
    return { file_encoding::utf32_le, 4 };
  if (n >= 4 && bytes[0] == 0 && bytes[1] == 0 && bytes[2] == 0xFE
      && bytes[3] == 0xFF)
    return { file_encoding::utf32_be, 4 };
  if (n >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE)
    return { file_encoding::utf16_le, 2 };
  if (n >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF)
    return { file_encoding::utf16_be, 2 };
  if (n >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
    return { file_encoding::utf8, 3 };
  return { file_encoding::utf8, 0 };
}

// Runs a decoder twice over the units: the counter sizes the UTF-8 text, the
// writer fills it.
template <typename Decoder, typename Unit>
std::string
decode (Unit const *units, std::size_t count)
{
  std::size_t const size
      = Decoder::process (units, count, 0, utf::utf8_counter ());
  std::string text (size, '\0');
  if (size != 0)
    {
      std::uint8_t *const begin = reinterpret_cast<std::uint8_t *> (&text[0]);
      std::uint8_t *const end
          = Decoder::process (units, count, begin, utf::utf8_writer ());
      text.resize (static_cast<std::size_t> (end - begin));
    }
  return text;
}

template <typename Unit>
std::vector<Unit>
load_units (std::vector<std::uint8_t> const &bytes, std::size_t from)
{
  std::vector<Unit> units ((bytes.size () - from) / sizeof (Unit));
  if (!units.empty ())
    std::memcpy (&units[0], &bytes[from], units.size () * sizeof (Unit));
  return units;
}

// Decides on the byte order of this machine: a decoder that swaps reads
// the other order.
std::string
to_utf8_text (std::vector<std::uint8_t> const &bytes)
{
  detected_t const found = detect (bytes);
  bool const little = is_little_endian ();
  switch (found.encoding)
    {
    case file_encoding::utf16_le:
    case file_encoding::utf16_be:
      {
        std::vector<std::uint16_t> const units
            = load_units<std::uint16_t> (bytes, found.bom_size);
        bool const file_is_little = found.encoding == file_encoding::utf16_le;
        return file_is_little == little
                   ? decode<utf::utf16_decoder<false>> (
                         units.empty () ? nullptr : &units[0], units.size ())
                   : decode<utf::utf16_decoder<true>> (
                         units.empty () ? nullptr : &units[0], units.size ());
      }
    case file_encoding::utf32_le:
    case file_encoding::utf32_be:
      {
        std::vector<std::uint32_t> const units
            = load_units<std::uint32_t> (bytes, found.bom_size);
        bool const file_is_little = found.encoding == file_encoding::utf32_le;
        return file_is_little == little
                   ? decode<utf::utf32_decoder<false>> (
                         units.empty () ? nullptr : &units[0], units.size ())
                   : decode<utf::utf32_decoder<true>> (
                         units.empty () ? nullptr : &units[0], units.size ());
      }
    case file_encoding::utf8:
    default:
      return std::string (bytes.begin ()
                              + static_cast<std::ptrdiff_t> (found.bom_size),
                          bytes.end ());
    }
}

// The bytes of a UTF-16 file of the given byte order, with its mark.
std::vector<std::uint8_t>
make_utf16_file (std::u16string const &text, bool big_endian)
{
  std::vector<std::uint8_t> bytes;
  bytes.push_back (big_endian ? 0xFE : 0xFF);
  bytes.push_back (big_endian ? 0xFF : 0xFE);
  for (std::size_t i = 0; i < text.size (); ++i)
    {
      std::uint16_t const unit = static_cast<std::uint16_t> (text[i]);
      bytes.push_back (
          static_cast<std::uint8_t> (big_endian ? unit >> 8 : unit & 0xFF));
      bytes.push_back (
          static_cast<std::uint8_t> (big_endian ? unit & 0xFF : unit >> 8));
    }
  return bytes;
}

// The UTF-8 log line as UTF-16 big endian: the writer stores units in the
// order of this machine, and byte_swap turns them around on a little endian
// one, so the bytes in memory are the bytes of the file.
std::vector<std::uint8_t>
utf8_to_utf16_be (std::string const &text)
{
  std::uint8_t const *const data
      = reinterpret_cast<std::uint8_t const *> (text.c_str ());
  std::size_t const count = utf::utf8_decoder::process (data, text.size (), 0,
                                                        utf::utf16_counter ());
  std::vector<std::uint16_t> units (count + 1);
  std::uint16_t *const end = utf::utf8_decoder::process (
      data, text.size (), &units[0], utf::utf16_writer ());
  units.resize (static_cast<std::size_t> (end - &units[0]));
  if (is_little_endian ())
    for (std::size_t i = 0; i < units.size (); ++i)
      units[i] = byte_swap (units[i]);
  std::vector<std::uint8_t> bytes (units.size () * sizeof (std::uint16_t));
  if (!bytes.empty ())
    std::memcpy (&bytes[0], &units[0], bytes.size ());
  return bytes;
}
} // namespace

int
main ()
{
  std::cout
      << "=== lumex::unicode workflow: files in several encodings ===\n\n";

  // "naive" with a diaeresis and the euro sign.
  std::u16string const sample = u"naïve €";
  std::string const expected_utf8 = "na\xC3\xAF"
                                    "ve \xE2\x82\xAC";

  std::cout << "--- 1. UTF-16 with a byte order mark, both byte orders ---\n";
  check (to_utf8_text (make_utf16_file (sample, false)) == expected_utf8,
         "UTF-16 little endian");
  check (to_utf8_text (make_utf16_file (sample, true)) == expected_utf8,
         "UTF-16 big endian");

  std::cout << "\n--- 2. UTF-32 with a byte order mark ---\n";
  std::vector<std::uint8_t> utf32_le = { 0xFF, 0xFE, 0, 0 };
  std::vector<std::uint8_t> utf32_be = { 0, 0, 0xFE, 0xFF };
  std::u32string const sample32 = U"naïve €";
  for (std::size_t i = 0; i < sample32.size (); ++i)
    {
      std::uint32_t const unit = static_cast<std::uint32_t> (sample32[i]);
      for (int shift = 0; shift < 32; shift += 8)
        utf32_le.push_back (static_cast<std::uint8_t> (unit >> shift));
      for (int shift = 24; shift >= 0; shift -= 8)
        utf32_be.push_back (static_cast<std::uint8_t> (unit >> shift));
    }
  check (to_utf8_text (utf32_le) == expected_utf8, "UTF-32 little endian");
  check (to_utf8_text (utf32_be) == expected_utf8, "UTF-32 big endian");

  std::cout << "\n--- 3. UTF-8 with and without a mark ---\n";
  std::vector<std::uint8_t> plain (expected_utf8.begin (),
                                   expected_utf8.end ());
  std::vector<std::uint8_t> marked = { 0xEF, 0xBB, 0xBF };
  marked.insert (marked.end (), plain.begin (), plain.end ());
  check (to_utf8_text (plain) == expected_utf8, "no mark");
  check (to_utf8_text (marked) == expected_utf8, "the mark is dropped");

  std::cout << "\n--- 4. Latin-1 is decided by the caller ---\n";
  std::vector<std::uint8_t> const latin1 = { 'n', 'a', 0xEF, 'v', 'e' };
  std::string const from_latin1
      = decode<utf::latin1_decoder> (&latin1[0], latin1.size ());
  check (from_latin1
             == "na\xC3\xAF"
                "ve",
         "Latin-1 bytes become UTF-8");

  std::cout << "\n--- 5. A log line as UTF-16 big endian ---\n";
  std::vector<std::uint8_t> const be = utf8_to_utf16_be (expected_utf8);
  std::ostringstream shown;
  shown << std::hex << std::setfill ('0');
  for (std::size_t i = 0; i < be.size (); ++i)
    shown << std::setw (2) << static_cast<unsigned int> (be[i]) << ' ';
  std::cout << "bytes: " << shown.str () << '\n';
  check (be.size () == sample.size () * 2, "two bytes per unit");
  check (be[0] == 0x00 && be[1] == 'n' && be[be.size () - 2] == 0x20
             && be[be.size () - 1] == 0xAC,
         "high byte first, the euro sign is 20 AC");

  std::cout << "\n=== unicode workflow "
            << (failures == 0 ? "finished" : "FAILED") << " ===\n";
  return failures == 0 ? 0 : 1;
}
