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

#include "lumex/core/utility/assert/LumexAssert.hpp"

#include "lumex/xml/utility/XmlUtils.hpp"

#include "XPathVariable.hpp"

using namespace lumex::xml::utility;
using namespace lumex::xml::xpath::variable;

LUMEX_PUBLIC_API
XPathVariable::XPathVariable (xpath_value_type type_)
    : m_type (type_), m_next (nullptr)
{
}

LUMEX_PUBLIC_API
char_t const *
XPathVariable::name () const
{
  switch (m_type)
    {
    case xpath_type_node_set:
      return static_cast< // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,
                          // cppcoreguidelines-pro-type-static-cast-downcast)
                 xpath_variable_node_set const *> (this)
          ->name;
    case xpath_type_number:
      return static_cast< // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,
                          // cppcoreguidelines-pro-type-static-cast-downcast)
                 xpath_variable_number const *> (this)
          ->name;
    case xpath_type_string:
      return static_cast< // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,
                          // cppcoreguidelines-pro-type-static-cast-downcast)
                 xpath_variable_string const *> (this)
          ->name;
    case xpath_type_boolean:
      return static_cast< // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,
                          // cppcoreguidelines-pro-type-static-cast-downcast)
                 xpath_variable_boolean const *> (this)
          ->name;

    default:
      LUMEX_ASSERT (false && "Invalid variable type"); // unreachable
      return nullptr;
    }
}

LUMEX_PUBLIC_API
xpath_value_type
XPathVariable::type () const
{
  return m_type;
}

LUMEX_PUBLIC_API
bool
XPathVariable::get_boolean () const
{
  return (m_type == xpath_type_boolean)
             ? static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                   xpath_variable_boolean const *> (this)
                   ->value
             : false;
}

LUMEX_PUBLIC_API
double
XPathVariable::get_number () const
{
  return (m_type == xpath_type_number)
             ? static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                   xpath_variable_number const *> (this)
                   ->value
             : utility::gen_nan ();
}

LUMEX_PUBLIC_API
char_t const *
XPathVariable::get_string () const
{
  char_t const *value
      = (m_type == xpath_type_string)
            ? static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                  xpath_variable_string const *> (this)
                  ->value
            : nullptr;
  return value != nullptr ? value : LUMEX_XML_TEXT ("");
}

LUMEX_PUBLIC_API
XPathNodeSet const &
XPathVariable::get_node_set () const
{
  return (m_type == xpath_type_node_set)
             ? static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                   xpath_variable_node_set const *> (this)
                   ->value
             : xpath::node::dummy_node_set;
}

LUMEX_PUBLIC_API
bool
XPathVariable::set (bool value)
{
  if (m_type != xpath_type_boolean)
    return false;

  static_cast<xpath_variable_boolean *> (this)->value
      = value; // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
  return true;
}

LUMEX_PUBLIC_API
bool
XPathVariable::set (double value)
{
  if (m_type != xpath_type_number)
    return false;

  static_cast<xpath_variable_number *> (this)->value
      = value; // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
  return true;
}

LUMEX_PUBLIC_API
bool
XPathVariable::set (char_t const *value)
{
  if (m_type != xpath_type_string)
    return false;

  auto *var = static_cast<xpath_variable_string *> (
      this); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

  std::size_t size = (utility::strlength (value) + 1) * sizeof (char_t);

  auto *copy // NOLINT(cppcoreguidelines-owning-memory)
      = static_cast<char_t *> (
          malloc (size)); // NOLINT(cppcoreguidelines-no-malloc)
  if (copy == nullptr)
    return false;

  memcpy (copy, value, size);

  // replace old string
  if (var->value != nullptr)
    free (var->value); // NOLINT(cppcoreguidelines-owning-memory,
                       // cppcoreguidelines-no-malloc)
  var->value = copy;

  return true;
}

LUMEX_PUBLIC_API
bool
XPathVariable::set (XPathNodeSet const &value)
{
  if (m_type != xpath_type_node_set)
    return false;

  static_cast<xpath_variable_node_set *> (this)
      ->value // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
      = value;
  return true;
}
