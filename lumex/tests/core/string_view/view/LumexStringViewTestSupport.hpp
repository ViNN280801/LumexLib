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
#include <memory>
#include <string>
#include <vector>

#include "lumex/core/string_view/LumexStringView"

namespace lumex_sv_test
{
/// The two character types of the views of the library under test.
template <class CharT> struct char_set;

template <> struct char_set<char>
{
  typedef lumex_string_view view_t;
  /// 'a', 'b', NUL and a byte with the high bit set (negative as a char).
  static std::vector<char>
  alphabet ()
  {
    return std::vector<char>{ 'a', 'b', '\0', static_cast<char> (0x80) };
  }
};

template <> struct char_set<wchar_t>
{
  typedef lumex_wstring_view view_t;
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
} // namespace lumex_sv_test

#endif // LUMEX_TESTS_CORE_STRING_VIEW_VIEW_TEST_SUPPORT_HPP
