// lumex/tests/core/unicode/LumexUnicodeTestSupport.hpp
//
// What the test files of the unicode module share: reference encoders written
// without the library's policies, a deterministic code point generator, and a
// traits type that records what a decoder reports.
#ifndef LUMEX_TESTS_CORE_UNICODE_HPP
#define LUMEX_TESTS_CORE_UNICODE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace unicode_test
{
// Appends the UTF-8 bytes of a code point, by the definition of the encoding
// (RFC 3629), not by the library's writer.
inline void
append_utf8 (std::vector<std::uint8_t> &out, std::uint32_t cp)
{
  if (cp < 0x80)
    {
      out.push_back (static_cast<std::uint8_t> (cp));
    }
  else if (cp < 0x800)
    {
      out.push_back (static_cast<std::uint8_t> (0xC0 | (cp >> 6)));
      out.push_back (static_cast<std::uint8_t> (0x80 | (cp & 0x3F)));
    }
  else if (cp < 0x10000)
    {
      out.push_back (static_cast<std::uint8_t> (0xE0 | (cp >> 12)));
      out.push_back (static_cast<std::uint8_t> (0x80 | ((cp >> 6) & 0x3F)));
      out.push_back (static_cast<std::uint8_t> (0x80 | (cp & 0x3F)));
    }
  else
    {
      out.push_back (static_cast<std::uint8_t> (0xF0 | (cp >> 18)));
      out.push_back (static_cast<std::uint8_t> (0x80 | ((cp >> 12) & 0x3F)));
      out.push_back (static_cast<std::uint8_t> (0x80 | ((cp >> 6) & 0x3F)));
      out.push_back (static_cast<std::uint8_t> (0x80 | (cp & 0x3F)));
    }
}

// Appends the UTF-16 units of a code point (RFC 2781), in host byte order.
inline void
append_utf16 (std::vector<std::uint16_t> &out, std::uint32_t cp)
{
  if (cp < 0x10000)
    {
      out.push_back (static_cast<std::uint16_t> (cp));
    }
  else
    {
      std::uint32_t const offset = cp - 0x10000;
      out.push_back (static_cast<std::uint16_t> (0xD800 + (offset >> 10)));
      out.push_back (static_cast<std::uint16_t> (0xDC00 + (offset & 0x3FF)));
    }
}

inline std::vector<std::uint8_t>
encode_utf8 (std::vector<std::uint32_t> const &points)
{
  std::vector<std::uint8_t> out;
  for (std::size_t i = 0; i < points.size (); ++i)
    append_utf8 (out, points[i]);
  return out;
}

inline std::vector<std::uint16_t>
encode_utf16 (std::vector<std::uint32_t> const &points)
{
  std::vector<std::uint16_t> out;
  for (std::size_t i = 0; i < points.size (); ++i)
    append_utf16 (out, points[i]);
  return out;
}

// A fixed xorshift generator, so a failure repeats.
class sequence
{
public:
  explicit sequence (std::uint64_t seed) : m_state (seed) {}

  std::uint64_t
  next ()
  {
    m_state ^= m_state << 13;
    m_state ^= m_state >> 7;
    m_state ^= m_state << 17;
    return m_state;
  }

  // A scalar value (never a surrogate), spread over the four UTF-8 lengths.
  std::uint32_t
  code_point ()
  {
    std::uint64_t const roll = next ();
    std::uint32_t const bits = static_cast<std::uint32_t> (roll >> 8);
    switch (roll % 8)
      {
      case 0:
      case 1:
      case 2:
        return bits % 0x80;
      case 3:
      case 4:
        return 0x80 + bits % (0x800 - 0x80);
      case 5:
      case 6:
        {
          std::uint32_t const cp = 0x800 + bits % (0x10000 - 0x800);
          return (cp >= 0xD800 && cp < 0xE000) ? cp + 0x800 : cp;
        }
      default:
        return 0x10000 + bits % (0x110000 - 0x10000);
      }
  }

  std::vector<std::uint32_t>
  code_points (std::size_t count)
  {
    std::vector<std::uint32_t> points;
    for (std::size_t i = 0; i < count; ++i)
      points.push_back (code_point ());
    return points;
  }

private:
  std::uint64_t m_state;
};

// Traits for a decoder run that records every code point and which of
// low () and high () reported it.
struct recorder_t
{
  std::vector<std::uint32_t> all;
  std::vector<std::uint32_t> low_calls;
  std::vector<std::uint32_t> high_calls;
};

struct recording_traits
{
  using value_type = recorder_t *;

  static value_type
  low (value_type result, std::uint32_t cp)
  {
    result->all.push_back (cp);
    result->low_calls.push_back (cp);
    return result;
  }

  static value_type
  high (value_type result, std::uint32_t cp)
  {
    result->all.push_back (cp);
    result->high_calls.push_back (cp);
    return result;
  }
};

template <typename Decoder, typename Unit>
recorder_t
decode_recording (std::vector<Unit> const &units)
{
  recorder_t recorder;
  Unit const *const data = units.empty () ? nullptr : &units[0];
  Decoder::process (data, units.size (), &recorder, recording_traits ());
  return recorder;
}

template <typename Decoder, typename Unit>
recorder_t
decode_recording (Unit const *data, std::size_t size)
{
  recorder_t recorder;
  Decoder::process (data, size, &recorder, recording_traits ());
  return recorder;
}
} // namespace unicode_test

#endif // !LUMEX_TESTS_CORE_UNICODE_HPP
