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

#include "lumex/xml/node/XmlNodeBase.hpp"
#include "lumex/xml/utility/XmlUtils.hpp"

#include "XmlAttribute.hpp"

using namespace lumex::xml::node;
using namespace lumex::xml::types;
using namespace lumex::xml::types::Types;
using namespace lumex::xml::utility;
using namespace lumex::xml::attribute;

LUMEX_PUBLIC_API
XmlAttribute::XmlAttribute () : m_attr (nullptr) {}

LUMEX_PUBLIC_API
XmlAttribute::XmlAttribute (XmlAttributeBase *attr) : m_attr (attr) {}

inline static void
unspecified_bool_xml_attribute (
    XmlAttribute *** /*unused*/) // NOLINT(misc-use-anonymous-namespace)
{
}

LUMEX_PUBLIC_API
XmlAttribute::
operator XmlAttribute::unspecified_bool_type () const
{
  return (m_attr != nullptr) ? unspecified_bool_xml_attribute : nullptr;
}

LUMEX_PUBLIC_API
bool
XmlAttribute::operator!() const
{
  return m_attr == nullptr;
}

LUMEX_PUBLIC_API
bool
XmlAttribute::operator== (XmlAttribute const &other) const
{
  return (m_attr == other.m_attr);
}

LUMEX_PUBLIC_API
bool
XmlAttribute::operator!= (XmlAttribute const &other) const
{
  return (m_attr != other.m_attr);
}

LUMEX_PUBLIC_API
bool
XmlAttribute::operator< (XmlAttribute const &other) const
{
  return (m_attr < other.m_attr);
}

LUMEX_PUBLIC_API
bool
XmlAttribute::operator> (XmlAttribute const &other) const
{
  return (m_attr > other.m_attr);
}

LUMEX_PUBLIC_API
bool
XmlAttribute::operator<= (XmlAttribute const &other) const
{
  return (m_attr <= other.m_attr);
}

LUMEX_PUBLIC_API
bool
XmlAttribute::operator>= (XmlAttribute const &other) const
{
  return (m_attr >= other.m_attr);
}

LUMEX_PUBLIC_API
XmlAttribute
XmlAttribute::next_attribute () const
{
  if (m_attr == nullptr)
    return {};
  return XmlAttribute (m_attr->next_attribute);
}

LUMEX_PUBLIC_API
XmlAttribute
XmlAttribute::previous_attribute () const
{
  if (m_attr == nullptr)
    return {};
  XmlAttributeBase *prev = m_attr->prev_attribute_c;
  return (prev->next_attribute != nullptr) ? XmlAttribute (prev)
                                           : XmlAttribute ();
}

LUMEX_PUBLIC_API
char_t const *
XmlAttribute::as_string (char_t const *def) const
{
  if (m_attr == nullptr)
    return def;
  char_t const *value = m_attr->value;
  return (value != nullptr) ? value : def;
}

LUMEX_PUBLIC_API
int
XmlAttribute::as_int (int def) const
{
  if (m_attr == nullptr)
    return def;
  char_t const *value = m_attr->value;
  return (value != nullptr) ? get_value_int (value) : def;
}

LUMEX_PUBLIC_API
unsigned int
XmlAttribute::as_uint (unsigned int def) const
{
  if (m_attr == nullptr)
    return def;
  char_t const *value = m_attr->value;
  return (value != nullptr) ? get_value_uint (value) : def;
}

LUMEX_PUBLIC_API
double
XmlAttribute::as_double (double def) const
{
  if (m_attr == nullptr)
    return def;
  char_t const *value = m_attr->value;
  return (value != nullptr) ? get_value_double (value) : def;
}

LUMEX_PUBLIC_API
float
XmlAttribute::as_float (float def) const
{
  if (m_attr == nullptr)
    return def;
  char_t const *value = m_attr->value;
  return (value != nullptr) ? get_value_float (value) : def;
}

LUMEX_PUBLIC_API
bool
XmlAttribute::as_bool (bool def) const
{
  if (m_attr == nullptr)
    return def;
  char_t const *value = m_attr->value;
  return (value != nullptr) ? get_value_bool (value) : def;
}

LUMEX_PUBLIC_API
long long
XmlAttribute::as_llong (long long def) const
{
  if (m_attr == nullptr)
    return def;
  char_t const *value = m_attr->value;
  return (value != nullptr) ? get_value_llong (value) : def;
}

LUMEX_PUBLIC_API
unsigned long long
XmlAttribute::as_ullong (unsigned long long def) const
{
  if (m_attr == nullptr)
    return def;
  char_t const *value = m_attr->value;
  return (value != nullptr) ? get_value_ullong (value) : def;
}

LUMEX_PUBLIC_API
bool
XmlAttribute::empty () const
{
  return m_attr == nullptr;
}

LUMEX_PUBLIC_API
char_t const *
XmlAttribute::name () const
{
  if (m_attr == nullptr)
    return "";
  char_t const *name = m_attr->name;
  return (name != nullptr) ? name : "";
}

LUMEX_PUBLIC_API
char_t const *
XmlAttribute::value () const
{
  if (m_attr == nullptr)
    return "";
  char_t const *value = m_attr->value;
  return (value != nullptr) ? value : "";
}

LUMEX_PUBLIC_API
std::size_t
XmlAttribute::hash_value () const
{
  return reinterpret_cast<uintptr_t> (m_attr) / sizeof (XmlAttributeBase);
}

LUMEX_PUBLIC_API
XmlAttributeBase *
XmlAttribute::get () const
{
  return m_attr;
}

LUMEX_PUBLIC_API
void
XmlAttribute::set (XmlAttributeBase *attr)
{
  m_attr = attr;
}

LUMEX_PUBLIC_API
XmlAttribute &
XmlAttribute::operator= (char_t const *rhs)
{
  set_value (rhs);
  return *this;
}

LUMEX_PUBLIC_API
XmlAttribute &
XmlAttribute::operator= (int rhs)
{
  set_value (rhs);
  return *this;
}

LUMEX_PUBLIC_API
XmlAttribute &
XmlAttribute::operator= (unsigned int rhs)
{
  set_value (rhs);
  return *this;
}

LUMEX_PUBLIC_API
XmlAttribute &
XmlAttribute::operator= (long rhs)
{
  set_value (rhs);
  return *this;
}

LUMEX_PUBLIC_API
XmlAttribute &
XmlAttribute::operator= (unsigned long rhs)
{
  set_value (rhs);
  return *this;
}

LUMEX_PUBLIC_API
XmlAttribute &
XmlAttribute::operator= (double rhs)
{
  set_value (rhs);
  return *this;
}

LUMEX_PUBLIC_API
XmlAttribute &
XmlAttribute::operator= (float rhs)
{
  set_value (rhs);
  return *this;
}

LUMEX_PUBLIC_API
XmlAttribute &
XmlAttribute::operator= (bool rhs)
{
  set_value (rhs);
  return *this;
}

LUMEX_PUBLIC_API
XmlAttribute &
XmlAttribute::operator= (long long rhs)
{
  set_value (rhs);
  return *this;
}

LUMEX_PUBLIC_API
XmlAttribute &
XmlAttribute::operator= (unsigned long long rhs)
{
  set_value (rhs);
  return *this;
}

LUMEX_PUBLIC_API
bool
XmlAttribute::set_name (char_t const *rhs)
{
  if (m_attr == nullptr)
    return false;

  return strcpy_insitu (m_attr->name, m_attr->header,
                        kxml_memory_page_name_allocated_mask, rhs,
                        strlength (rhs));
}

LUMEX_PUBLIC_API
bool
XmlAttribute::set_name (char_t const *rhs, std::size_t size)
{
  if (m_attr == nullptr)
    return false;

  return strcpy_insitu (m_attr->name, m_attr->header,
                        kxml_memory_page_name_allocated_mask, rhs, size);
}

LUMEX_PUBLIC_API
bool
XmlAttribute::set_value (char_t const *rhs)
{
  if (m_attr == nullptr)
    return false;

  return strcpy_insitu (m_attr->value, m_attr->header,
                        kxml_memory_page_value_allocated_mask, rhs,
                        strlength (rhs));
}

LUMEX_PUBLIC_API
bool
XmlAttribute::set_value (char_t const *rhs, std::size_t size)
{
  if (m_attr == nullptr)
    return false;

  return strcpy_insitu (m_attr->value, m_attr->header,
                        kxml_memory_page_value_allocated_mask, rhs, size);
}

LUMEX_PUBLIC_API
bool
XmlAttribute::set_value (int rhs)
{
  if (m_attr == nullptr)
    return false;

  return set_value_integer<unsigned int> (
      m_attr->value, m_attr->header, kxml_memory_page_value_allocated_mask,
      static_cast<unsigned int> (rhs), rhs < 0);
}

LUMEX_PUBLIC_API
bool
XmlAttribute::set_value (unsigned int rhs)
{
  if (m_attr == nullptr)
    return false;

  return set_value_integer<unsigned int> (
      m_attr->value, m_attr->header, kxml_memory_page_value_allocated_mask,
      rhs, false);
}

LUMEX_PUBLIC_API
bool
XmlAttribute::set_value (long rhs)
{
  if (m_attr == nullptr)
    return false;

  return set_value_integer<unsigned long> (
      m_attr->value, m_attr->header, kxml_memory_page_value_allocated_mask,
      static_cast<unsigned long> (rhs), rhs < 0);
}

LUMEX_PUBLIC_API
bool
XmlAttribute::set_value (unsigned long rhs)
{
  if (m_attr == nullptr)
    return false;

  return set_value_integer<unsigned long> (
      m_attr->value, m_attr->header, kxml_memory_page_value_allocated_mask,
      rhs, false);
}

LUMEX_PUBLIC_API
bool
XmlAttribute::set_value (double rhs)
{
  if (m_attr == nullptr)
    return false;

  return set_value_convert (m_attr->value, m_attr->header,
                            kxml_memory_page_value_allocated_mask, rhs,
                            kdefault_double_precision);
}

LUMEX_PUBLIC_API
bool
XmlAttribute::set_value (double rhs, int precision)
{
  if (m_attr == nullptr)
    return false;

  return set_value_convert (m_attr->value, m_attr->header,
                            kxml_memory_page_value_allocated_mask, rhs,
                            precision);
}

LUMEX_PUBLIC_API
bool
XmlAttribute::set_value (float rhs)
{
  if (m_attr == nullptr)
    return false;

  return set_value_convert (m_attr->value, m_attr->header,
                            kxml_memory_page_value_allocated_mask, rhs,
                            kdefault_float_precision);
}

LUMEX_PUBLIC_API
bool
XmlAttribute::set_value (float rhs, int precision)
{
  if (m_attr == nullptr)
    return false;

  return set_value_convert (m_attr->value, m_attr->header,
                            kxml_memory_page_value_allocated_mask, rhs,
                            precision);
}

LUMEX_PUBLIC_API
bool
XmlAttribute::set_value (bool rhs)
{
  if (m_attr == nullptr)
    return false;

  return set_value_bool (m_attr->value, m_attr->header,
                         kxml_memory_page_value_allocated_mask, rhs);
}

LUMEX_PUBLIC_API
bool
XmlAttribute::set_value (long long rhs)
{
  if (m_attr == nullptr)
    return false;

  return set_value_integer<unsigned long long> (
      m_attr->value, m_attr->header, kxml_memory_page_value_allocated_mask,
      static_cast<unsigned long long> (rhs), rhs < 0);
}

LUMEX_PUBLIC_API
bool
XmlAttribute::set_value (unsigned long long rhs)
{
  if (m_attr == nullptr)
    return false;

  return set_value_integer<unsigned long long> (
      m_attr->value, m_attr->header, kxml_memory_page_value_allocated_mask,
      rhs, false);
}

LUMEX_PUBLIC_API
bool
lumex::xml::utility::is_attribute_of (attribute::XmlAttributeBase *attr,
                                      XmlNodeBase *node)
{
  for (XmlAttributeBase *attribute = node->first_attribute;
       attribute != nullptr; attribute = attribute->next_attribute)
    if (attribute == attr)
      return true;

  return false;
}

LUMEX_PUBLIC_API
bool
lumex::xml::attribute::operator&& (
    XmlAttribute const &lhs, bool rhs) // NOLINT(misc-use-internal-linkage)
{
  return static_cast<bool> (lhs) && rhs;
}

LUMEX_PUBLIC_API
bool
lumex::xml::attribute::operator|| (
    XmlAttribute const &lhs, bool rhs) // NOLINT(misc-use-internal-linkage)
{
  return static_cast<bool> (lhs) || rhs;
}
