#define LUMEX_IMPLEMENTATION
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
LUMEX_PUBLIC_API LumexStringView::size_type const LumexStringView::npos;

LUMEX_PUBLIC_API
LumexStringView::LumexStringView (char const *str) LUMEX_NOEXCEPT
    : m_data (str),
      m_size (str != nullptr ? std::strlen (str) : 0)
{
}

LUMEX_PUBLIC_API
LumexStringView::LumexStringView (std::string const &str) LUMEX_NOEXCEPT
    : m_data (str.data ()),
      m_size (str.size ())
{
}

LUMEX_PUBLIC_API LumexStringView::const_reverse_iterator
LumexStringView::rbegin () const LUMEX_NOEXCEPT
{
  return const_reverse_iterator (end ());
}

LUMEX_PUBLIC_API LumexStringView::const_reverse_iterator
LumexStringView::crbegin () const LUMEX_NOEXCEPT
{
  return const_reverse_iterator (end ());
}

LUMEX_PUBLIC_API LumexStringView::const_reverse_iterator
LumexStringView::rend () const LUMEX_NOEXCEPT
{
  return const_reverse_iterator (begin ());
}

LUMEX_PUBLIC_API LumexStringView::const_reverse_iterator
LumexStringView::crend () const LUMEX_NOEXCEPT
{
  return const_reverse_iterator (begin ());
}

// -- Element access --
LUMEX_PUBLIC_API LumexStringView::const_reference
LumexStringView::at (size_type idx) const
{
  if (idx >= m_size)
    throw std::out_of_range ("LumexStringView::at() out of range");
  return m_data[idx];
}

// -- Modifiers --
LUMEX_PUBLIC_API void
LumexStringView::clear () LUMEX_NOEXCEPT
{
  m_data = nullptr;
  m_size = 0;
}

LUMEX_PUBLIC_API void
LumexStringView::remove_prefix (size_type n) LUMEX_NOEXCEPT
{
  n = std::min (n, m_size);
  m_data += n;
  m_size -= n;
}

LUMEX_PUBLIC_API void
LumexStringView::remove_suffix (size_type n) LUMEX_NOEXCEPT
{
  n = std::min (n, m_size);
  m_size -= n;
}

LUMEX_PUBLIC_API void
LumexStringView::swap (LumexStringView &other) LUMEX_NOEXCEPT
{
  std::swap (m_data, other.m_data);
  std::swap (m_size, other.m_size);
}

// -- Copy out --
LUMEX_PUBLIC_API LumexStringView::size_type
LumexStringView::copy (char *dest, size_type count, size_type pos) const
{
  if (pos > m_size)
    throw std::out_of_range ("LumexStringView::copy() pos > size");
  size_type rlen = std::min (count, m_size - pos);
  std::memcpy (dest, m_data + pos, rlen);
  return rlen;
}

// -- Substring --
LUMEX_PUBLIC_API LumexStringView
LumexStringView::substr (size_type pos, size_type n) const
{
  if (pos > m_size)
    throw std::out_of_range ("LumexStringView::substr() pos > size");
  n = std::min (n, m_size - pos);
  return LumexStringView (m_data + pos, n);
}

// -- Comparison --
LUMEX_PUBLIC_API int
LumexStringView::compare (LumexStringView other) const LUMEX_NOEXCEPT
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
LumexStringView::compare (size_type pos, size_type len,
                          LumexStringView other) const
{
  return substr (pos, len).compare (other);
}

LUMEX_PUBLIC_API int
LumexStringView::compare (char const *cstr) const
{
  return compare (LumexStringView (cstr));
}

// -- Starts / ends / contains helpers --
LUMEX_PUBLIC_API bool
LumexStringView::starts_with (char chr) const LUMEX_NOEXCEPT
{
  return !empty () && front () == chr;
}

LUMEX_PUBLIC_API bool
LumexStringView::starts_with (LumexStringView str) const LUMEX_NOEXCEPT
{
  return m_size >= str.m_size
         && std::memcmp (m_data, str.m_data, str.m_size) == 0;
}

LUMEX_PUBLIC_API bool
LumexStringView::ends_with (char chr) const LUMEX_NOEXCEPT
{
  return !empty () && back () == chr;
}

LUMEX_PUBLIC_API bool
LumexStringView::ends_with (LumexStringView str) const LUMEX_NOEXCEPT
{
  return m_size >= str.m_size
         && std::memcmp (m_data + m_size - str.m_size, str.m_data, str.m_size)
                == 0;
}

// -- Find (simple implementations) --
LUMEX_PUBLIC_API LumexStringView::size_type
LumexStringView::find (char chr, size_type pos) const LUMEX_NOEXCEPT
{
  if (pos >= m_size)
    return npos;
  auto const *ptr = static_cast<const_pointer> (std::memchr (
      m_data + pos, static_cast<unsigned char> (chr), m_size - pos));
  return ptr != nullptr ? static_cast<size_type> (ptr - m_data) : npos;
}

LUMEX_PUBLIC_API LumexStringView::size_type
LumexStringView::find (LumexStringView str, size_type pos) const LUMEX_NOEXCEPT
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

LUMEX_PUBLIC_API LumexStringView::size_type
LumexStringView::find (char const *cstr, size_type pos,
                       size_type count) const LUMEX_NOEXCEPT
{
  return find (LumexStringView (cstr, count), pos);
}

LUMEX_PUBLIC_API LumexStringView::size_type
LumexStringView::find (char const *cstr, size_type pos) const LUMEX_NOEXCEPT
{
  return find (LumexStringView (cstr), pos);
}

// -- Reverse find --
LUMEX_PUBLIC_API LumexStringView::size_type
LumexStringView::rfind (LumexStringView str,
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

LUMEX_PUBLIC_API LumexStringView::size_type
LumexStringView::rfind (char chr, size_type pos) const LUMEX_NOEXCEPT
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

LUMEX_PUBLIC_API LumexStringView::size_type
LumexStringView::rfind (char const *cstr, size_type pos,
                        size_type count) const LUMEX_NOEXCEPT
{
  return rfind (LumexStringView (cstr, count), pos);
}

LUMEX_PUBLIC_API LumexStringView::size_type
LumexStringView::rfind (char const *cstr, size_type pos) const LUMEX_NOEXCEPT
{
  return rfind (LumexStringView (cstr), pos);
}

// -- Find first of --
LUMEX_PUBLIC_API LumexStringView::size_type
LumexStringView::find_first_of (LumexStringView str,
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

LUMEX_PUBLIC_API LumexStringView::size_type
LumexStringView::find_first_of (char chr, size_type pos) const LUMEX_NOEXCEPT
{
  return find (chr, pos);
}

LUMEX_PUBLIC_API LumexStringView::size_type
LumexStringView::find_first_of (char const *cstr, size_type pos,
                                size_type count) const LUMEX_NOEXCEPT
{
  return find_first_of (LumexStringView (cstr, count), pos);
}

LUMEX_PUBLIC_API LumexStringView::size_type
LumexStringView::find_first_of (char const *cstr,
                                size_type pos) const LUMEX_NOEXCEPT
{
  return find_first_of (LumexStringView (cstr), pos);
}

// -- Find last of --
LUMEX_PUBLIC_API LumexStringView::size_type
LumexStringView::find_last_of (LumexStringView str,
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

LUMEX_PUBLIC_API LumexStringView::size_type
LumexStringView::find_last_of (char chr, size_type pos) const LUMEX_NOEXCEPT
{
  return rfind (chr, pos);
}

LUMEX_PUBLIC_API LumexStringView::size_type
LumexStringView::find_last_of (char const *cstr, size_type pos,
                               size_type count) const LUMEX_NOEXCEPT
{
  return find_last_of (LumexStringView (cstr, count), pos);
}

LUMEX_PUBLIC_API LumexStringView::size_type
LumexStringView::find_last_of (char const *cstr,
                               size_type pos) const LUMEX_NOEXCEPT
{
  return find_last_of (LumexStringView (cstr), pos);
}

// -- Find first not of --
LUMEX_PUBLIC_API LumexStringView::size_type
LumexStringView::find_first_not_of (LumexStringView str,
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

LUMEX_PUBLIC_API LumexStringView::size_type
LumexStringView::find_first_not_of (char chr,
                                    size_type pos) const LUMEX_NOEXCEPT
{
  for (size_type i = pos; i < m_size; ++i)
    if (m_data[i] != chr)
      return i;
  return npos;
}

LUMEX_PUBLIC_API LumexStringView::size_type
LumexStringView::find_first_not_of (char const *cstr, size_type pos,
                                    size_type count) const LUMEX_NOEXCEPT
{
  return find_first_not_of (LumexStringView (cstr, count), pos);
}

LUMEX_PUBLIC_API LumexStringView::size_type
LumexStringView::find_first_not_of (char const *cstr,
                                    size_type pos) const LUMEX_NOEXCEPT
{
  return find_first_not_of (LumexStringView (cstr), pos);
}

// -- Find last not of --
LUMEX_PUBLIC_API LumexStringView::size_type
LumexStringView::find_last_not_of (LumexStringView str,
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

LUMEX_PUBLIC_API LumexStringView::size_type
LumexStringView::find_last_not_of (char chr,
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

LUMEX_PUBLIC_API LumexStringView::size_type
LumexStringView::find_last_not_of (char const *cstr, size_type pos,
                                   size_type count) const LUMEX_NOEXCEPT
{
  return find_last_not_of (LumexStringView (cstr, count), pos);
}

LUMEX_PUBLIC_API LumexStringView::size_type
LumexStringView::find_last_not_of (char const *cstr,
                                   size_type pos) const LUMEX_NOEXCEPT
{
  return find_last_not_of (LumexStringView (cstr), pos);
}

// -- Stream inserter --
LUMEX_PUBLIC_API
std::ostream &
operator<< (std::ostream &ostr, LumexStringView sview)
{
  return ostr.write (sview.data (),
                     static_cast<std::streamsize> (sview.size ()));
}
} // namespace view
} // namespace string_view
} // namespace core
} // namespace lumex
