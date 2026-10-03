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

#include "lumex/xml/constants/XmlConstants.hpp"
#include "lumex/xml/node/XmlNode.hpp"
#include "lumex/xml/node/XmlNodeBase.hpp"
#include "lumex/xml/utility/XmlMacros.hpp"
#include "lumex/xml/utility/XmlUtils.hpp"

#include "XmlText.hpp"

using namespace lumex::xml::text;
using namespace lumex::xml::node;
using namespace lumex::xml::utility;
using namespace lumex::xml::constants;
using namespace lumex::xml::constants::Constants;

LUMEX_PUBLIC_API
XmlText::XmlText (XmlNodeBase *root) : m_root (root) {}

LUMEX_PUBLIC_API
XmlNodeBase *
XmlText::_data () const
{
  if ((m_root == nullptr) || node::is_text_node (m_root))
    return m_root;

  // element nodes can have value if parse_embed_pcdata was used
  if (LUMEX_XML_NODETYPE (m_root) == node_element
      && (m_root->value != nullptr))
    return m_root;

  for (XmlNodeBase *node = m_root->first_child; node != nullptr;
       node = node->next_sibling)
    if (node::is_text_node (node))
      return node;

  return nullptr;
}

LUMEX_PUBLIC_API
XmlNodeBase *
XmlText::_data_new ()
{
  XmlNodeBase *data = _data ();
  if (data != nullptr)
    return data;

  return XmlNode (m_root).append_child (node_pcdata).get ();
}

LUMEX_PUBLIC_API
XmlText::XmlText () : m_root (nullptr) {}

inline static void
unspecified_bool_xml_text (
    XmlText *** /*unused*/) // NOLINT(misc-use-anonymous-namespace)
{
}

LUMEX_PUBLIC_API
XmlText::
operator XmlText::unspecified_bool_type () const
{
  return (_data () != nullptr) ? unspecified_bool_xml_text : nullptr;
}

LUMEX_PUBLIC_API
bool
XmlText::operator!() const
{
  return _data () == nullptr;
}

LUMEX_PUBLIC_API
bool
XmlText::empty () const
{
  return _data () == nullptr;
}

LUMEX_PUBLIC_API
char_t const *
XmlText::get () const
{
  XmlNodeBase *data = _data ();
  if (data == nullptr)
    return LUMEX_XML_TEXT ("");
  char_t const *value = data->value;
  return (value != nullptr) ? value : LUMEX_XML_TEXT ("");
}

LUMEX_PUBLIC_API
char_t const *
XmlText::as_string (char_t const *def) const
{
  XmlNodeBase *data = _data ();
  if (data == nullptr)
    return def;
  char_t const *value = data->value;
  return (value != nullptr) ? value : def;
}

LUMEX_PUBLIC_API
int
XmlText::as_int (int def) const
{
  XmlNodeBase *data = _data ();
  if (data == nullptr)
    return def;
  char_t const *value = data->value;
  return (value != nullptr) ? utility::get_value_int (value) : def;
}

LUMEX_PUBLIC_API
unsigned int
XmlText::as_uint (unsigned int def) const
{
  XmlNodeBase *data = _data ();
  if (data == nullptr)
    return def;
  char_t const *value = data->value;
  return (value != nullptr) ? utility::get_value_uint (value) : def;
}

LUMEX_PUBLIC_API
double
XmlText::as_double (double def) const
{
  XmlNodeBase *data = _data ();
  if (data == nullptr)
    return def;
  char_t const *value = data->value;
  return (value != nullptr) ? utility::get_value_double (value) : def;
}

LUMEX_PUBLIC_API
float
XmlText::as_float (float def) const
{
  XmlNodeBase *data = _data ();
  if (data == nullptr)
    return def;
  char_t const *value = data->value;
  return (value != nullptr) ? utility::get_value_float (value) : def;
}

LUMEX_PUBLIC_API
bool
XmlText::as_bool (bool def) const
{
  XmlNodeBase *data = _data ();
  if (data == nullptr)
    return def;
  char_t const *value = data->value;
  return (value != nullptr) ? utility::get_value_bool (value) : def;
}

LUMEX_PUBLIC_API
long long
XmlText::as_llong (long long def) const
{
  XmlNodeBase *data = _data ();
  if (data == nullptr)
    return def;
  char_t const *value = data->value;
  return (value != nullptr) ? utility::get_value_llong (value) : def;
}

LUMEX_PUBLIC_API
unsigned long long
XmlText::as_ullong (unsigned long long def) const
{
  XmlNodeBase *data = _data ();
  if (data == nullptr)
    return def;
  char_t const *value = data->value;
  return (value != nullptr) ? utility::get_value_ullong (value) : def;
}

LUMEX_PUBLIC_API
bool
XmlText::set (char_t const *rhs)
{
  XmlNodeBase *newdata = _data_new ();

  return (newdata != nullptr)
             ? utility::strcpy_insitu (newdata->value, newdata->header,
                                       kxml_memory_page_value_allocated_mask,
                                       rhs, utility::strlength (rhs))
             : false;
}

LUMEX_PUBLIC_API
bool
XmlText::set (char_t const *rhs, std::size_t size)
{
  XmlNodeBase *newdata = _data_new ();

  return (newdata != nullptr)
             ? utility::strcpy_insitu (newdata->value, newdata->header,
                                       kxml_memory_page_value_allocated_mask,
                                       rhs, size)
             : false;
}

LUMEX_PUBLIC_API
bool
XmlText::set (int rhs)
{
  XmlNodeBase *newdata = _data_new ();

  return (newdata != nullptr) ? utility::set_value_integer<unsigned int> (
                                    newdata->value, newdata->header,
                                    kxml_memory_page_value_allocated_mask,
                                    static_cast<unsigned int> (rhs), rhs < 0)
                              : false;
}

LUMEX_PUBLIC_API
bool
XmlText::set (unsigned int rhs)
{
  XmlNodeBase *newdata = _data_new ();

  return (newdata != nullptr)
             ? utility::set_value_integer<unsigned int> (
                   newdata->value, newdata->header,
                   kxml_memory_page_value_allocated_mask, rhs, false)
             : false;
}

LUMEX_PUBLIC_API
bool
XmlText::set (long rhs)
{
  XmlNodeBase *newdata = _data_new ();

  return (newdata != nullptr) ? utility::set_value_integer<unsigned long> (
                                    newdata->value, newdata->header,
                                    kxml_memory_page_value_allocated_mask,
                                    static_cast<unsigned long> (rhs), rhs < 0)
                              : false;
}

LUMEX_PUBLIC_API
bool
XmlText::set (unsigned long rhs)
{
  XmlNodeBase *newdata = _data_new ();

  return (newdata != nullptr)
             ? utility::set_value_integer<unsigned long> (
                   newdata->value, newdata->header,
                   kxml_memory_page_value_allocated_mask, rhs, false)
             : false;
}

LUMEX_PUBLIC_API
bool
XmlText::set (float rhs)
{
  XmlNodeBase *newdata = _data_new ();

  return (newdata != nullptr) ? utility::set_value_convert (
                                    newdata->value, newdata->header,
                                    kxml_memory_page_value_allocated_mask, rhs,
                                    kdefault_float_precision)
                              : false;
}

LUMEX_PUBLIC_API
bool
XmlText::set (float rhs, int precision)
{
  XmlNodeBase *newdata = _data_new ();

  return (newdata != nullptr)
             ? utility::set_value_convert (
                   newdata->value, newdata->header,
                   kxml_memory_page_value_allocated_mask, rhs, precision)
             : false;
}

LUMEX_PUBLIC_API
bool
XmlText::set (double rhs)
{
  XmlNodeBase *newdata = _data_new ();

  return (newdata != nullptr) ? utility::set_value_convert (
                                    newdata->value, newdata->header,
                                    kxml_memory_page_value_allocated_mask, rhs,
                                    kdefault_double_precision)
                              : false;
}

LUMEX_PUBLIC_API
bool
XmlText::set (double rhs, int precision)
{
  XmlNodeBase *newdata = _data_new ();

  return (newdata != nullptr)
             ? utility::set_value_convert (
                   newdata->value, newdata->header,
                   kxml_memory_page_value_allocated_mask, rhs, precision)
             : false;
}

LUMEX_PUBLIC_API
bool
XmlText::set (bool rhs)
{
  XmlNodeBase *newdata = _data_new ();

  return (newdata != nullptr)
             ? utility::set_value_bool (newdata->value, newdata->header,
                                        kxml_memory_page_value_allocated_mask,
                                        rhs)
             : false;
}

LUMEX_PUBLIC_API
bool
XmlText::set (long long rhs)
{
  XmlNodeBase *newdata = _data_new ();

  return (newdata != nullptr)
             ? utility::set_value_integer<unsigned long long> (
                   newdata->value, newdata->header,
                   kxml_memory_page_value_allocated_mask,
                   static_cast<unsigned long long> (rhs), rhs < 0)
             : false;
}

LUMEX_PUBLIC_API
bool
XmlText::set (unsigned long long rhs)
{
  XmlNodeBase *newdata = _data_new ();

  return (newdata != nullptr)
             ? utility::set_value_integer<unsigned long long> (
                   newdata->value, newdata->header,
                   kxml_memory_page_value_allocated_mask, rhs, false)
             : false;
}

LUMEX_PUBLIC_API
XmlText &
XmlText::operator= (char_t const *rhs)
{
  set (rhs);
  return *this;
}

LUMEX_PUBLIC_API
XmlText &
XmlText::operator= (int rhs)
{
  set (rhs);
  return *this;
}

LUMEX_PUBLIC_API
XmlText &
XmlText::operator= (unsigned int rhs)
{
  set (rhs);
  return *this;
}

LUMEX_PUBLIC_API
XmlText &
XmlText::operator= (long rhs)
{
  set (rhs);
  return *this;
}

LUMEX_PUBLIC_API
XmlText &
XmlText::operator= (unsigned long rhs)
{
  set (rhs);
  return *this;
}

LUMEX_PUBLIC_API
XmlText &
XmlText::operator= (double rhs)
{
  set (rhs);
  return *this;
}

LUMEX_PUBLIC_API
XmlText &
XmlText::operator= (float rhs)
{
  set (rhs);
  return *this;
}

LUMEX_PUBLIC_API
XmlText &
XmlText::operator= (bool rhs)
{
  set (rhs);
  return *this;
}

LUMEX_PUBLIC_API
XmlText &
XmlText::operator= (long long rhs)
{
  set (rhs);
  return *this;
}

LUMEX_PUBLIC_API
XmlText &
XmlText::operator= (unsigned long long rhs)
{
  set (rhs);
  return *this;
}

LUMEX_PUBLIC_API
XmlNode
XmlText::data () const
{
  return XmlNode (_data ());
}

LUMEX_PUBLIC_API
bool
lumex::xml::text::operator&& (XmlText const &lhs,
                              bool rhs) // NOLINT(misc-use-internal-linkage)
{
  return static_cast<bool> (lhs) && rhs;
}

LUMEX_PUBLIC_API
bool
lumex::xml::text::operator|| (XmlText const &lhs,
                              bool rhs) // NOLINT(misc-use-internal-linkage)
{
  return static_cast<bool> (lhs) || rhs;
}
