#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "lumex/core/unicode/LumexUnicode"
#include "lumex/core/utility/bit/LumexBit.hpp"

namespace
{
using lumex::core::unicode::convert::to_utf8;
using lumex::core::unicode::convert::to_wide;
namespace utf = lumex::core::unicode::utf;

int failures = 0;

// The bytes of a string as hexadecimal, so the output does not depend on the
// encoding of the console.
std::string
hex_bytes (std::string const &text)
{
  std::ostringstream out;
  out << std::hex << std::setfill ('0');
  for (std::size_t i = 0; i < text.size (); ++i)
    {
      if (i != 0)
        out << ' ';
      out << std::setw (2)
          << static_cast<unsigned int> (static_cast<unsigned char> (text[i]));
    }
  return out.str ();
}

void
check (bool condition, char const *what)
{
  std::cout << (condition ? "  ok   " : "  FAIL ") << what << '\n';
  if (!condition)
    ++failures;
}

// A conversion runs twice: the counter sizes the output, the writer fills it.
std::string
utf16_units_to_utf8 (std::vector<std::uint16_t> const &units, bool swap_bytes)
{
  std::uint16_t const *const data = units.empty () ? nullptr : &units[0];
  std::size_t const size
      = swap_bytes ? utf::utf16_decoder<true>::process (data, units.size (), 0,
                                                        utf::utf8_counter ())
                   : utf::utf16_decoder<false>::process (
                         data, units.size (), 0, utf::utf8_counter ());
  std::vector<std::uint8_t> out (size + 1);
  std::uint8_t *const begin = &out[0];
  std::uint8_t *const end
      = swap_bytes
            ? utf::utf16_decoder<true>::process (data, units.size (), begin,
                                                 utf::utf8_writer ())
            : utf::utf16_decoder<false>::process (data, units.size (), begin,
                                                  utf::utf8_writer ());
  return std::string (reinterpret_cast<char const *> (begin),
                      static_cast<std::size_t> (end - begin));
}
} // namespace

int
main ()
{
  std::cout << "=== lumex::unicode: UTF-8, UTF-16, UTF-32 and Latin-1 ===\n\n";

  // "cafe" with an acute accent, the euro sign and an emoji outside the Basic
  // Multilingual Plane (a surrogate pair in UTF-16).
  std::string const text = "caf\xC3\xA9 \xE2\x82\xAC \xF0\x9F\x98\x80";

  std::cout << "--- 1. to_wide and to_utf8 ---\n";
  std::wstring const wide = to_wide (text);
  std::cout << "UTF-8 bytes: " << hex_bytes (text) << '\n';
  std::cout << "wchar_t has " << sizeof (wchar_t) << " bytes, the text takes "
            << wide.size () << " units\n";
  check (wide.size () == (sizeof (wchar_t) == 2 ? 9U : 8U),
         "9 units as UTF-16 (a surrogate pair for the emoji), 8 as UTF-32");
  check (to_utf8 (wide) == text, "to_utf8 (to_wide (text)) == text");

  std::cout << "\n--- 2. Pointer and length keep embedded zeros ---\n";
  std::string const with_zero ("a\0b", 3);
  check (to_wide (with_zero.c_str (), with_zero.size ()).size () == 3,
         "to_wide (ptr, 3) converts three bytes");
  check (to_wide (with_zero.c_str ()).size () == 1,
         "to_wide (ptr) stops at the zero byte");
  check (to_utf8 (std::wstring ()).empty (), "an empty string stays empty");

  std::cout << "\n--- 3. Count, then write ---\n";
  std::vector<std::uint16_t> units;
  units.push_back (0x0063); // c
  units.push_back (0x00E9); // e with an acute accent
  units.push_back (0xD83D); // high surrogate
  units.push_back (0xDE00); // low surrogate: U+1F600
  std::string const converted = utf16_units_to_utf8 (units, false);
  std::cout << "UTF-16 units -> UTF-8: " << hex_bytes (converted) << '\n';
  check (converted == "c\xC3\xA9\xF0\x9F\x98\x80", "four units, seven bytes");

  std::cout << "\n--- 4. The other byte order ---\n";
  std::vector<std::uint16_t> swapped;
  for (std::size_t i = 0; i < units.size (); ++i)
    swapped.push_back (lumex::core::utility::bit::byte_swap (units[i]));
  std::cout << "this machine is "
            << (lumex::core::utility::bit::is_little_endian () ? "little"
                                                               : "big")
            << " endian\n";
  check (utf16_units_to_utf8 (swapped, true) == converted,
         "utf16_decoder<true> reads the swapped units");

  std::cout << "\n--- 5. Latin-1 ---\n";
  std::uint8_t const latin1[] = { 0x63, 0xE9 };
  std::size_t const latin1_size = utf::latin1_decoder::process (
      latin1, sizeof (latin1), 0, utf::utf8_counter ());
  std::cout << "Latin-1 'c' + e with acute accent takes " << latin1_size
            << " bytes in UTF-8\n";
  check (latin1_size == 3, "0xE9 is two bytes in UTF-8");
  std::uint8_t latin1_out[2] = { 0, 0 };
  std::uint8_t *const latin1_end = utf::utf8_decoder::process (
      reinterpret_cast<std::uint8_t const *> ("\xE2\x82\xAC"), 3, latin1_out,
      utf::latin1_writer ());
  check (latin1_end == latin1_out + 1 && latin1_out[0] == '?',
         "the euro sign is not in Latin-1 and becomes '?'");

  std::cout << "\n--- 6. Invalid input is skipped ---\n";
  std::string const broken = "a\x80z\xE2\x82";
  std::wstring const kept = to_wide (broken);
  std::cout << "bytes " << hex_bytes (broken) << " keep " << kept.size ()
            << " characters\n";
  check (kept.size () == 2 && kept[0] == L'a' && kept[1] == L'z',
         "a stray continuation byte and a truncated sequence leave no trace");

  std::cout << "\n=== unicode example "
            << (failures == 0 ? "finished" : "FAILED") << " ===\n";
  return failures == 0 ? 0 : 1;
}
