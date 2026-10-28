/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy
 * of this software and associated
 * documentation files (the "Software"), to deal
 * in the Software without
 * restriction, including without limitation the rights
 * to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the
 * Software, and to permit persons to whom the Software is
 * furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice
 * and this permission notice shall be included in
 * all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT
 * WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO
 * THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file LumexStringViewTestSupport.hpp
 * @brief Shared helpers of the string view tests.
 * @details Exact-size copies of a text (so that a read past the end is an
 * AddressSanitizer error), a deterministic random source, the enumeration of
 * all short texts over a small alphabet, and the reference results of the
 * searches (the `std::basic_string` members, which have the semantics of the
 * view's).
 */
#ifndef LUMEX_TESTS_CORE_STRING_VIEW_VIEW_TEST_SUPPORT_HPP
#define LUMEX_TESTS_CORE_STRING_VIEW_VIEW_TEST_SUPPORT_HPP

#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <ios>
#include <memory>
#include <string>
#include <vector>

#include "lumex/core/string_view/LumexStringView"

namespace lumex_sv_test
{
using lumex::core::string_view::view::lumex_basic_string_view;

/// The two character types of the views of the library under test.
template <class CharT> struct char_set;

template <> struct char_set<char>
{
  using view_t = lumex_string_view;
  /// 'a', 'b', NUL and a byte with the high bit set (negative as a char).
  static std::vector<char>
  alphabet ()
  {
    return std::vector<char>{ 'a', 'b', '\0', static_cast<char> (0x80) };
  }
};

template <> struct char_set<wchar_t>
{
  using view_t = lumex_wstring_view;
  static std::vector<wchar_t>
  alphabet ()
  {
    return std::vector<wchar_t>{ L'a', L'b', L'\0',
                                 static_cast<wchar_t> (0x20AC) };
  }
};

/// A heap copy of exactly `size` characters (no spare byte: a read past the
/// end is an AddressSanitizer error). A size of 0 gives a null pointer.
template <class CharT> class exact_copy
{
public:
  explicit exact_copy (std::basic_string<CharT> const &text)
      : m_size (text.size ()),
        m_data (text.empty () ? nullptr : new CharT[text.size ()])
  {
    for (std::size_t i = 0; i < m_size; ++i)
      m_data[i] = text[i];
  }
  CharT const *
  data () const
  {
    return m_data.get ();
  }
  std::size_t
  size () const
  {
    return m_size;
  }

private:
  std::size_t m_size;
  std::unique_ptr<CharT[]> m_data;
};

/// Every text of the alphabet up to `max_length` characters, in length order.
template <class CharT>
std::vector<std::basic_string<CharT>>
all_texts (std::vector<CharT> const &alphabet, std::size_t max_length)
{
  std::vector<std::basic_string<CharT>> texts (1);
  std::size_t begin = 0;
  for (std::size_t length = 1; length <= max_length; ++length)
    {
      std::size_t const end = texts.size ();
      for (std::size_t i = begin; i < end; ++i)
        for (std::size_t c = 0; c < alphabet.size (); ++c)
          texts.push_back (texts[i] + alphabet[c]);
      begin = end;
    }
  return texts;
}

/// A fixed-seed generator (xorshift64*): the tests are reproducible.
class random_source
{
public:
  explicit random_source (std::uint64_t seed) : m_state (seed | 1U) {}
  std::uint64_t
  next ()
  {
    m_state ^= m_state >> 12;
    m_state ^= m_state << 25;
    m_state ^= m_state >> 27;
    return m_state * 0x2545F4914F6CDD1DULL;
  }
  std::size_t
  below (std::size_t bound)
  {
    return static_cast<std::size_t> (next () % bound);
  }

private:
  std::uint64_t m_state;
};

/// The positions a search is tried at for a text of `size` characters.
inline std::vector<std::size_t>
positions (std::size_t size)
{
  std::vector<std::size_t> result;
  for (std::size_t pos = 0; pos <= size + 2; ++pos)
    result.push_back (pos);
  result.push_back (static_cast<std::size_t> (-1));
  result.push_back (static_cast<std::size_t> (-1) - 1);
  return result;
}
// Traits without a comparison_category, comparing without regard to case.
struct caseless_traits
{
  using char_type = char;
  using int_type = int;
  using off_type = std::streamoff;
  using pos_type = std::streampos;
  using state_type = std::mbstate_t;
  static constexpr char
  fold (char c) noexcept
  {
    return (c >= 'A' && c <= 'Z') ? static_cast<char> (c - 'A' + 'a') : c;
  }
  static void
  assign (char &r, char const &a) noexcept
  {
    r = a;
  }
  static constexpr bool
  eq (char a, char b) noexcept
  {
    return fold (a) == fold (b);
  }
  static constexpr bool
  lt (char a, char b) noexcept
  {
    return fold (a) < fold (b);
  }
  static int
  compare (char const *a, char const *b, std::size_t n)
  {
    for (std::size_t i = 0; i < n; ++i)
      {
        if (lt (a[i], b[i]))
          return -1;
        if (lt (b[i], a[i]))
          return 1;
      }
    return 0;
  }
  static std::size_t
  length (char const *s)
  {
    return std::char_traits<char>::length (s);
  }
  static char const *
  find (char const *s, std::size_t n, char const &c)
  {
    for (std::size_t i = 0; i < n; ++i)
      if (eq (s[i], c))
        return s + i;
    return nullptr;
  }
  static char *
  move (char *d, char const *s, std::size_t n)
  {
    return std::char_traits<char>::move (d, s, n);
  }
  static char *
  copy (char *d, char const *s, std::size_t n)
  {
    return std::char_traits<char>::copy (d, s, n);
  }
  static char *
  assign (char *d, std::size_t n, char c)
  {
    return std::char_traits<char>::assign (d, n, c);
  }
  static constexpr char
  to_char_type (int_type i) noexcept
  {
    return static_cast<char> (i);
  }
  static constexpr int_type
  to_int_type (char c) noexcept
  {
    return static_cast<unsigned char> (c);
  }
  static constexpr bool
  eq_int_type (int_type a, int_type b) noexcept
  {
    return a == b;
  }
  static constexpr int_type
  eof () noexcept
  {
    return -1;
  }
  static constexpr int_type
  not_eof (int_type i) noexcept
  {
    return i == -1 ? 0 : i;
  }
};

// Traits that count their copy () calls (the view must copy through them).
struct counting_traits : std::char_traits<char>
{
  static int &
  copies ()
  {
    static int count = 0;
    return count;
  }
  static char *
  copy (char *dest, char const *src, std::size_t count)
  {
    ++copies ();
    return std::char_traits<char>::copy (dest, src, count);
  }
};

using counting_view = lumex_basic_string_view<char, counting_traits>;
using caseless_view = lumex_basic_string_view<char, caseless_traits>;
} // namespace lumex_sv_test

#endif // LUMEX_TESTS_CORE_STRING_VIEW_VIEW_TEST_SUPPORT_HPP
