#define LUMEX_IMPLEMENTATION
#include <algorithm> // std::min, std::swap
#include <stdexcept> // std::out_of_range

#include "LumexWStringView.hpp"
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
LUMEX_PUBLIC_API LumexWStringView::size_type const LumexWStringView::npos;

LUMEX_PUBLIC_API
LumexWStringView::LumexWStringView (wchar_t const *str) LUMEX_NOEXCEPT
    : m_data (str),
      m_size (str != nullptr ? std::wcslen (str) : 0)
{
}

LUMEX_PUBLIC_API
LumexWStringView::LumexWStringView (std::wstring const &str) LUMEX_NOEXCEPT
    : m_data (str.data ()),
      m_size (str.size ())
{
}

LUMEX_PUBLIC_API
LumexWStringView::const_reverse_iterator
LumexWStringView::rbegin () const LUMEX_NOEXCEPT
{
  return const_reverse_iterator (end ());
}

LUMEX_PUBLIC_API
LumexWStringView::const_reverse_iterator
LumexWStringView::crbegin () const LUMEX_NOEXCEPT
{
  return const_reverse_iterator (end ());
}

LUMEX_PUBLIC_API
LumexWStringView::const_reverse_iterator
LumexWStringView::rend () const LUMEX_NOEXCEPT
{
  return const_reverse_iterator (begin ());
}

LUMEX_PUBLIC_API
LumexWStringView::const_reverse_iterator
LumexWStringView::crend () const LUMEX_NOEXCEPT
{
  return const_reverse_iterator (begin ());
}

LUMEX_PUBLIC_API
LumexWStringView::const_reference
LumexWStringView::at (size_type idx) const
{
  if (idx >= m_size)
    throw std::out_of_range ("LumexWStringView::at() out of range");
  return m_data[idx];
}

LUMEX_PUBLIC_API
void
LumexWStringView::clear () LUMEX_NOEXCEPT
{
  m_data = nullptr;
  m_size = 0;
}

LUMEX_PUBLIC_API
void
LumexWStringView::remove_prefix (size_type n) LUMEX_NOEXCEPT
{
  n = std::min (n, m_size);
  m_data += n;
  m_size -= n;
}

LUMEX_PUBLIC_API
void
LumexWStringView::remove_suffix (size_type n) LUMEX_NOEXCEPT
{
  n = std::min (n, m_size);
  m_size -= n;
}

LUMEX_PUBLIC_API
void
LumexWStringView::swap (LumexWStringView &other) LUMEX_NOEXCEPT
{
  std::swap (m_data, other.m_data);
  std::swap (m_size, other.m_size);
}

LUMEX_PUBLIC_API
LumexWStringView::size_type
LumexWStringView::copy (wchar_t *dest, size_type count, size_type pos) const
{
  if (pos > m_size)
    throw std::out_of_range ("LumexWStringView::copy() pos > size");
  size_type rlen = std::min (count, m_size - pos);
  std::wmemcpy (dest, m_data + pos, rlen);
  return rlen;
}

// — Substring —
LUMEX_PUBLIC_API
LumexWStringView
LumexWStringView::substr (size_type pos, size_type n) const
{
  if (pos > m_size)
    throw std::out_of_range ("LumexWStringView::substr() pos > size");
  n = std::min (n, m_size - pos);
  return LumexWStringView (m_data + pos, n);
}

// — Comparison —
LUMEX_PUBLIC_API
int
LumexWStringView::compare (LumexWStringView other) const LUMEX_NOEXCEPT
{
  int const cmp
      = std::wmemcmp (m_data, other.m_data, std::min (m_size, other.m_size));
  if (cmp != 0)
    return cmp;
  if (m_size == other.m_size)
    return 0;
  return (m_size < other.m_size) ? -1 : 1;
}

// convenience overloads
LUMEX_PUBLIC_API
int
LumexWStringView::compare (size_type pos, size_type len,
                           LumexWStringView other) const
{
  return substr (pos, len).compare (other);
}

LUMEX_PUBLIC_API
int
LumexWStringView::compare (wchar_t const *cstr) const
{
  return compare (LumexWStringView (cstr));
}

// — Starts / ends / contains helpers — (non-standard extensions but useful)
LUMEX_PUBLIC_API
bool
LumexWStringView::starts_with (wchar_t chr) const LUMEX_NOEXCEPT
{
  return !empty () && front () == chr;
}

LUMEX_PUBLIC_API
bool
LumexWStringView::starts_with (LumexWStringView str) const LUMEX_NOEXCEPT
{
  return m_size >= str.m_size
         && std::wmemcmp (m_data, str.m_data, str.m_size) == 0;
}

LUMEX_PUBLIC_API
bool
LumexWStringView::ends_with (wchar_t chr) const LUMEX_NOEXCEPT
{
  return !empty () && back () == chr;
}

LUMEX_PUBLIC_API
bool
LumexWStringView::ends_with (LumexWStringView str) const LUMEX_NOEXCEPT
{
  return m_size >= str.m_size
         && std::wmemcmp (m_data + m_size - str.m_size, str.m_data, str.m_size)
                == 0;
}

// — Find (simple implementations) —
LUMEX_PUBLIC_API
LumexWStringView::size_type
LumexWStringView::find (wchar_t chr, size_type pos) const LUMEX_NOEXCEPT
{
  if (pos >= m_size)
    return npos;
  auto const *ptr = static_cast<const_pointer> (
      std::wmemchr (m_data + pos, chr, m_size - pos));
  return ptr != nullptr ? static_cast<size_type> (ptr - m_data) : npos;
}

LUMEX_PUBLIC_API
LumexWStringView::size_type
LumexWStringView::find (LumexWStringView str,
                        size_type pos) const LUMEX_NOEXCEPT
{
  if (str.empty ())
    return pos <= m_size ? pos : npos;
  if (str.m_size > m_size || pos > m_size - str.m_size)
    return npos;
  for (size_type i = pos; i <= m_size - str.m_size; ++i)
    {
      if (m_data[i] == str.m_data[0]
          && std::wmemcmp (m_data + i, str.m_data, str.m_size) == 0)
        {
          return i;
        }
    }
  return npos;
}

LUMEX_PUBLIC_API
LumexWStringView::size_type
LumexWStringView::find (wchar_t const *cstr, size_type pos,
                        size_type count) const LUMEX_NOEXCEPT
{
  return find (LumexWStringView (cstr, count), pos);
}

LUMEX_PUBLIC_API
LumexWStringView::size_type
LumexWStringView::find (wchar_t const *cstr,
                        size_type pos) const LUMEX_NOEXCEPT
{
  return find (LumexWStringView (cstr), pos);
}

// — Reverse find —
LUMEX_PUBLIC_API
LumexWStringView::size_type
LumexWStringView::rfind (LumexWStringView str,
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
      if (std::wmemcmp (m_data + idx, str.m_data, str.m_size) == 0)
        return idx;
    }
  return npos;
}

LUMEX_PUBLIC_API
LumexWStringView::size_type
LumexWStringView::rfind (wchar_t chr, size_type pos) const LUMEX_NOEXCEPT
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

LUMEX_PUBLIC_API
LumexWStringView::size_type
LumexWStringView::rfind (wchar_t const *cstr, size_type pos,
                         size_type count) const LUMEX_NOEXCEPT
{
  return rfind (LumexWStringView (cstr, count), pos);
}

LUMEX_PUBLIC_API
LumexWStringView::size_type
LumexWStringView::rfind (wchar_t const *cstr,
                         size_type pos) const LUMEX_NOEXCEPT
{
  return rfind (LumexWStringView (cstr), pos);
}

// — Find first of —
LUMEX_PUBLIC_API
LumexWStringView::size_type
LumexWStringView::find_first_of (LumexWStringView str,
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

LUMEX_PUBLIC_API
LumexWStringView::size_type
LumexWStringView::find_first_of (wchar_t chr,
                                 size_type pos) const LUMEX_NOEXCEPT
{
  return find (chr, pos);
}

LUMEX_PUBLIC_API
LumexWStringView::size_type
LumexWStringView::find_first_of (wchar_t const *cstr, size_type pos,
                                 size_type count) const LUMEX_NOEXCEPT
{
  return find_first_of (LumexWStringView (cstr, count), pos);
}

LUMEX_PUBLIC_API
LumexWStringView::size_type
LumexWStringView::find_first_of (wchar_t const *cstr,
                                 size_type pos) const LUMEX_NOEXCEPT
{
  return find_first_of (LumexWStringView (cstr), pos);
}

// — Find last of —
LUMEX_PUBLIC_API
LumexWStringView::size_type
LumexWStringView::find_last_of (LumexWStringView str,
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

LUMEX_PUBLIC_API
LumexWStringView::size_type
LumexWStringView::find_last_of (wchar_t chr,
                                size_type pos) const LUMEX_NOEXCEPT
{
  return rfind (chr, pos);
}

LUMEX_PUBLIC_API
LumexWStringView::size_type
LumexWStringView::find_last_of (wchar_t const *cstr, size_type pos,
                                size_type count) const LUMEX_NOEXCEPT
{
  return find_last_of (LumexWStringView (cstr, count), pos);
}

LUMEX_PUBLIC_API
LumexWStringView::size_type
LumexWStringView::find_last_of (wchar_t const *cstr,
                                size_type pos) const LUMEX_NOEXCEPT
{
  return find_last_of (LumexWStringView (cstr), pos);
}

// — Find first not of —
LUMEX_PUBLIC_API
LumexWStringView::size_type
LumexWStringView::find_first_not_of (LumexWStringView str,
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

LUMEX_PUBLIC_API
LumexWStringView::size_type
LumexWStringView::find_first_not_of (wchar_t chr,
                                     size_type pos) const LUMEX_NOEXCEPT
{
  for (size_type i = pos; i < m_size; ++i)
    if (m_data[i] != chr)
      return i;
  return npos;
}

LUMEX_PUBLIC_API
LumexWStringView::size_type
LumexWStringView::find_first_not_of (wchar_t const *cstr, size_type pos,
                                     size_type count) const LUMEX_NOEXCEPT
{
  return find_first_not_of (LumexWStringView (cstr, count), pos);
}

LUMEX_PUBLIC_API
LumexWStringView::size_type
LumexWStringView::find_first_not_of (wchar_t const *cstr,
                                     size_type pos) const LUMEX_NOEXCEPT
{
  return find_first_not_of (LumexWStringView (cstr), pos);
}

// — Find last not of —
LUMEX_PUBLIC_API
LumexWStringView::size_type
LumexWStringView::find_last_not_of (LumexWStringView str,
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

LUMEX_PUBLIC_API
LumexWStringView::size_type
LumexWStringView::find_last_not_of (wchar_t chr,
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

LUMEX_PUBLIC_API
LumexWStringView::size_type
LumexWStringView::find_last_not_of (wchar_t const *cstr, size_type pos,
                                    size_type count) const LUMEX_NOEXCEPT
{
  return find_last_not_of (LumexWStringView (cstr, count), pos);
}

LUMEX_PUBLIC_API
LumexWStringView::size_type
LumexWStringView::find_last_not_of (wchar_t const *cstr,
                                    size_type pos) const LUMEX_NOEXCEPT
{
  return find_last_not_of (LumexWStringView (cstr), pos);
}

LUMEX_PUBLIC_API
std::wostream &
operator<< (std::wostream &wostr, LumexWStringView wsview)
{
  return wostr.write (wsview.data (),
                      static_cast<std::streamsize> (wsview.size ()));
}
} // namespace view
} // namespace string_view
} // namespace core
} // namespace lumex
