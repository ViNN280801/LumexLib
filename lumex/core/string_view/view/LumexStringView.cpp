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

#define LUMEX_IMPLEMENTATION

#include <algorithm> // std::min, std::swap
#include <stdexcept> // std::out_of_range

#include "LumexStringView.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

// Static member definition
namespace lumex
{
namespace core
{
namespace string_view
{
namespace view
{
LUMEX_PUBLIC_API lumex_string_view::size_type const lumex_string_view::npos;

LUMEX_PUBLIC_API
lumex_string_view::lumex_string_view (char const *str) LUMEX_NOEXCEPT
    : m_data (str),
      m_size (str != nullptr ? std::strlen (str) : 0)
{
}

LUMEX_PUBLIC_API
lumex_string_view::lumex_string_view (std::string const &str) LUMEX_NOEXCEPT
    : m_data (str.data ()),
      m_size (str.size ())
{
}

LUMEX_PUBLIC_API lumex_string_view::const_reverse_iterator
lumex_string_view::rbegin () const LUMEX_NOEXCEPT
{
  return const_reverse_iterator (end ());
}

LUMEX_PUBLIC_API lumex_string_view::const_reverse_iterator
lumex_string_view::crbegin () const LUMEX_NOEXCEPT
{
  return const_reverse_iterator (end ());
}

LUMEX_PUBLIC_API lumex_string_view::const_reverse_iterator
lumex_string_view::rend () const LUMEX_NOEXCEPT
{
  return const_reverse_iterator (begin ());
}

LUMEX_PUBLIC_API lumex_string_view::const_reverse_iterator
lumex_string_view::crend () const LUMEX_NOEXCEPT
{
  return const_reverse_iterator (begin ());
}

// -- Element access --
LUMEX_PUBLIC_API lumex_string_view::const_reference
lumex_string_view::at (size_type idx) const
{
  if (idx >= m_size)
    throw std::out_of_range ("LumexStringView::at() out of range");
  return m_data[idx];
}

// -- Modifiers --
LUMEX_PUBLIC_API void
lumex_string_view::clear () LUMEX_NOEXCEPT
{
  m_data = nullptr;
  m_size = 0;
}

LUMEX_PUBLIC_API void
lumex_string_view::remove_prefix (size_type n) LUMEX_NOEXCEPT
{
  n = std::min (n, m_size);
  m_data += n;
  m_size -= n;
}

LUMEX_PUBLIC_API void
lumex_string_view::remove_suffix (size_type n) LUMEX_NOEXCEPT
{
  n = std::min (n, m_size);
  m_size -= n;
}

LUMEX_PUBLIC_API void
lumex_string_view::swap (lumex_string_view &other) LUMEX_NOEXCEPT
{
  std::swap (m_data, other.m_data);
  std::swap (m_size, other.m_size);
}

// -- Copy out --
LUMEX_PUBLIC_API lumex_string_view::size_type
lumex_string_view::copy (char *dest, size_type count, size_type pos) const
{
  if (pos > m_size)
    throw std::out_of_range ("LumexStringView::copy() pos > size");
  size_type rlen = std::min (count, m_size - pos);
  std::memcpy (dest, m_data + pos, rlen);
  return rlen;
}

// -- Substring --
LUMEX_PUBLIC_API lumex_string_view
lumex_string_view::substr (size_type pos, size_type n) const
{
  if (pos > m_size)
    throw std::out_of_range ("LumexStringView::substr() pos > size");
  n = std::min (n, m_size - pos);
  return lumex_string_view (m_data + pos, n);
}

// -- Comparison --
LUMEX_PUBLIC_API int
lumex_string_view::compare (lumex_string_view other) const LUMEX_NOEXCEPT
{
  int const cmp
      = std::memcmp (m_data, other.m_data, std::min (m_size, other.m_size));
  if (cmp != 0)
    return cmp;
  if (m_size == other.m_size)
    return 0;
  return (m_size < other.m_size) ? -1 : 1;
}

LUMEX_PUBLIC_API int
lumex_string_view::compare (size_type pos, size_type len,
                            lumex_string_view other) const
{
  return substr (pos, len).compare (other);
}

LUMEX_PUBLIC_API int
lumex_string_view::compare (char const *cstr) const
{
  return compare (lumex_string_view (cstr));
}

// -- Starts / ends / contains helpers --
LUMEX_PUBLIC_API bool
lumex_string_view::starts_with (char chr) const LUMEX_NOEXCEPT
{
  return !empty () && front () == chr;
}

LUMEX_PUBLIC_API bool
lumex_string_view::starts_with (lumex_string_view str) const LUMEX_NOEXCEPT
{
  return m_size >= str.m_size
         && std::memcmp (m_data, str.m_data, str.m_size) == 0;
}

LUMEX_PUBLIC_API bool
lumex_string_view::ends_with (char chr) const LUMEX_NOEXCEPT
{
  return !empty () && back () == chr;
}

LUMEX_PUBLIC_API bool
lumex_string_view::ends_with (lumex_string_view str) const LUMEX_NOEXCEPT
{
  return m_size >= str.m_size
         && std::memcmp (m_data + m_size - str.m_size, str.m_data, str.m_size)
                == 0;
}

// -- Find (simple implementations) --
LUMEX_PUBLIC_API lumex_string_view::size_type
lumex_string_view::find (char chr, size_type pos) const LUMEX_NOEXCEPT
{
  if (pos >= m_size)
    return npos;
  auto const *ptr = static_cast<const_pointer> (std::memchr (
      m_data + pos, static_cast<unsigned char> (chr), m_size - pos));
  return ptr != nullptr ? static_cast<size_type> (ptr - m_data) : npos;
}

LUMEX_PUBLIC_API lumex_string_view::size_type
lumex_string_view::find (lumex_string_view str,
                         size_type pos) const LUMEX_NOEXCEPT
{
  if (str.empty ())
    return pos <= m_size ? pos : npos;
  if (str.m_size > m_size || pos > m_size - str.m_size)
    return npos;
  for (size_type i = pos; i <= m_size - str.m_size; ++i)
    {
      if (m_data[i] == str.m_data[0]
          && std::memcmp (m_data + i, str.m_data, str.m_size) == 0)
        {
          return i;
        }
    }
  return npos;
}

LUMEX_PUBLIC_API lumex_string_view::size_type
lumex_string_view::find (char const *cstr, size_type pos,
                         size_type count) const LUMEX_NOEXCEPT
{
  return find (lumex_string_view (cstr, count), pos);
}

LUMEX_PUBLIC_API lumex_string_view::size_type
lumex_string_view::find (char const *cstr, size_type pos) const LUMEX_NOEXCEPT
{
  return find (lumex_string_view (cstr), pos);
}

// -- Reverse find --
LUMEX_PUBLIC_API lumex_string_view::size_type
lumex_string_view::rfind (lumex_string_view str,
                          size_type pos) const LUMEX_NOEXCEPT
{
  if (str.empty ())
    return std::min (pos, m_size);
  if (str.m_size > m_size)
    return npos;
  pos = std::min (pos, m_size - str.m_size);
  for (size_type i = pos + 1; i > 0; --i)
    {
      size_type idx = i - 1;
      if (std::memcmp (m_data + idx, str.m_data, str.m_size) == 0)
        return idx;
    }
  return npos;
}

LUMEX_PUBLIC_API lumex_string_view::size_type
lumex_string_view::rfind (char chr, size_type pos) const LUMEX_NOEXCEPT
{
  if (empty ())
    return npos;
  if (pos >= m_size)
    pos = m_size - 1;
  for (size_type i = pos + 1; i > 0; --i)
    if (m_data[i - 1] == chr)
      return i - 1;
  return npos;
}

LUMEX_PUBLIC_API lumex_string_view::size_type
lumex_string_view::rfind (char const *cstr, size_type pos,
                          size_type count) const LUMEX_NOEXCEPT
{
  return rfind (lumex_string_view (cstr, count), pos);
}

LUMEX_PUBLIC_API lumex_string_view::size_type
lumex_string_view::rfind (char const *cstr, size_type pos) const LUMEX_NOEXCEPT
{
  return rfind (lumex_string_view (cstr), pos);
}

// -- Find first of --
LUMEX_PUBLIC_API lumex_string_view::size_type
lumex_string_view::find_first_of (lumex_string_view str,
                                  size_type pos) const LUMEX_NOEXCEPT
{
  for (size_type i = pos; i < m_size; ++i)
    {
      for (size_type j = 0; j < str.m_size; ++j)
        if (m_data[i] == str.m_data[j])
          return i;
    }
  return npos;
}

LUMEX_PUBLIC_API lumex_string_view::size_type
lumex_string_view::find_first_of (char chr, size_type pos) const LUMEX_NOEXCEPT
{
  return find (chr, pos);
}

LUMEX_PUBLIC_API lumex_string_view::size_type
lumex_string_view::find_first_of (char const *cstr, size_type pos,
                                  size_type count) const LUMEX_NOEXCEPT
{
  return find_first_of (lumex_string_view (cstr, count), pos);
}

LUMEX_PUBLIC_API lumex_string_view::size_type
lumex_string_view::find_first_of (char const *cstr,
                                  size_type pos) const LUMEX_NOEXCEPT
{
  return find_first_of (lumex_string_view (cstr), pos);
}

// -- Find last of --
LUMEX_PUBLIC_API lumex_string_view::size_type
lumex_string_view::find_last_of (lumex_string_view str,
                                 size_type pos) const LUMEX_NOEXCEPT
{
  if (empty () || str.empty ())
    return npos;
  if (pos >= m_size)
    pos = m_size - 1;
  for (size_type i = pos + 1; i > 0; --i)
    {
      size_type idx = i - 1;
      for (size_type j = 0; j < str.m_size; ++j)
        if (m_data[idx] == str.m_data[j])
          return idx;
    }
  return npos;
}

LUMEX_PUBLIC_API lumex_string_view::size_type
lumex_string_view::find_last_of (char chr, size_type pos) const LUMEX_NOEXCEPT
{
  return rfind (chr, pos);
}

LUMEX_PUBLIC_API lumex_string_view::size_type
lumex_string_view::find_last_of (char const *cstr, size_type pos,
                                 size_type count) const LUMEX_NOEXCEPT
{
  return find_last_of (lumex_string_view (cstr, count), pos);
}

LUMEX_PUBLIC_API lumex_string_view::size_type
lumex_string_view::find_last_of (char const *cstr,
                                 size_type pos) const LUMEX_NOEXCEPT
{
  return find_last_of (lumex_string_view (cstr), pos);
}

// -- Find first not of --
LUMEX_PUBLIC_API lumex_string_view::size_type
lumex_string_view::find_first_not_of (lumex_string_view str,
                                      size_type pos) const LUMEX_NOEXCEPT
{
  for (size_type i = pos; i < m_size; ++i)
    {
      bool found = false;
      for (size_type j = 0; j < str.m_size; ++j)
        {
          if (m_data[i] == str.m_data[j])
            {
              found = true;
              break;
            }
        }
      if (!found)
        return i;
    }
  return npos;
}

LUMEX_PUBLIC_API lumex_string_view::size_type
lumex_string_view::find_first_not_of (char chr,
                                      size_type pos) const LUMEX_NOEXCEPT
{
  for (size_type i = pos; i < m_size; ++i)
    if (m_data[i] != chr)
      return i;
  return npos;
}

LUMEX_PUBLIC_API lumex_string_view::size_type
lumex_string_view::find_first_not_of (char const *cstr, size_type pos,
                                      size_type count) const LUMEX_NOEXCEPT
{
  return find_first_not_of (lumex_string_view (cstr, count), pos);
}

LUMEX_PUBLIC_API lumex_string_view::size_type
lumex_string_view::find_first_not_of (char const *cstr,
                                      size_type pos) const LUMEX_NOEXCEPT
{
  return find_first_not_of (lumex_string_view (cstr), pos);
}

// -- Find last not of --
LUMEX_PUBLIC_API lumex_string_view::size_type
lumex_string_view::find_last_not_of (lumex_string_view str,
                                     size_type pos) const LUMEX_NOEXCEPT
{
  if (empty ())
    return npos;
  if (pos >= m_size)
    pos = m_size - 1;
  for (size_type i = pos + 1; i > 0; --i)
    {
      size_type idx = i - 1;
      bool found = false;
      for (size_type j = 0; j < str.m_size; ++j)
        {
          if (m_data[idx] == str.m_data[j])
            {
              found = true;
              break;
            }
        }
      if (!found)
        return idx;
    }
  return npos;
}

LUMEX_PUBLIC_API lumex_string_view::size_type
lumex_string_view::find_last_not_of (char chr,
                                     size_type pos) const LUMEX_NOEXCEPT
{
  if (empty ())
    return npos;
  if (pos >= m_size)
    pos = m_size - 1;
  for (size_type i = pos + 1; i > 0; --i)
    if (m_data[i - 1] != chr)
      return i - 1;
  return npos;
}

LUMEX_PUBLIC_API lumex_string_view::size_type
lumex_string_view::find_last_not_of (char const *cstr, size_type pos,
                                     size_type count) const LUMEX_NOEXCEPT
{
  return find_last_not_of (lumex_string_view (cstr, count), pos);
}

LUMEX_PUBLIC_API lumex_string_view::size_type
lumex_string_view::find_last_not_of (char const *cstr,
                                     size_type pos) const LUMEX_NOEXCEPT
{
  return find_last_not_of (lumex_string_view (cstr), pos);
}

// -- Stream inserter --
LUMEX_PUBLIC_API
std::ostream &
operator<< (std::ostream &ostr, lumex_string_view sview)
{
  return ostr.write (sview.data (),
                     static_cast<std::streamsize> (sview.size ()));
}
} // namespace view
} // namespace string_view
} // namespace core
} // namespace lumex
