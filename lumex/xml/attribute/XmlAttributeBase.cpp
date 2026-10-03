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
#include "lumex/xml/node/XmlNodeBase.hpp"
#include "lumex/xml/utility/XmlUtils.hpp"

#include "XmlAttributeBase.hpp"

using namespace lumex::xml::constants;
using namespace lumex::xml::constants::Constants;
using namespace lumex::xml::node;
using namespace lumex::xml::utility;

LUMEX_PUBLIC_API
XmlAttributeBase *
lumex::xml::attribute::allocate_attribute (XmlAllocator &alloc)
{
  XmlMemoryPage *page{};
  void *memory = alloc.allocate_object (sizeof (XmlAttributeBase), page);
  if (memory == nullptr)
    return nullptr;

  return new (memory)
      XmlAttributeBase (page); // NOLINT(cppcoreguidelines-owning-memory)
}

LUMEX_PUBLIC_API
void
lumex::xml::attribute::destroy_attribute (XmlAttributeBase *attr,
                                          XmlAllocator &alloc)
{
  if ((attr->header & kxml_memory_page_name_allocated_mask) != 0)
    alloc.deallocate_string (attr->name);

  if ((attr->header & kxml_memory_page_value_allocated_mask) != 0)
    alloc.deallocate_string (attr->value);

  alloc.deallocate_memory (
      attr, sizeof (XmlAttributeBase),
      LUMEX_XML_GETPAGE (
          attr)); // NOLINT(cppcoreguidelines-pro-type-const-cast)
}

LUMEX_PUBLIC_API
void
lumex::xml::attribute::prepend_attribute (XmlAttributeBase *attr,
                                          XmlNodeBase *node)
{
  XmlAttributeBase *head = node->first_attribute;

  if (head != nullptr)
    {
      attr->prev_attribute_c = head->prev_attribute_c;
      head->prev_attribute_c = attr;
    }
  else
    attr->prev_attribute_c = attr;

  attr->next_attribute = head;
  node->first_attribute = attr;
}

LUMEX_PUBLIC_API
void
lumex::xml::attribute::insert_attribute_after (XmlAttributeBase *attr,
                                               XmlAttributeBase *place,
                                               XmlNodeBase *node)
{
  XmlAttributeBase *next = place->next_attribute;

  if (next != nullptr)
    next->prev_attribute_c = attr;
  else
    node->first_attribute->prev_attribute_c = attr;

  attr->next_attribute = next;
  attr->prev_attribute_c = place;
  place->next_attribute = attr;
}

LUMEX_PUBLIC_API
void
lumex::xml::attribute::insert_attribute_before (XmlAttributeBase *attr,
                                                XmlAttributeBase *place,
                                                XmlNodeBase *node)
{
  XmlAttributeBase *prev = place->prev_attribute_c;

  if (prev->next_attribute != nullptr)
    prev->next_attribute = attr;
  else
    node->first_attribute = attr;

  attr->prev_attribute_c = prev;
  attr->next_attribute = place;
  place->prev_attribute_c = attr;
}

LUMEX_PUBLIC_API
void
lumex::xml::attribute::remove_attribute (XmlAttributeBase *attr,
                                         XmlNodeBase *node)
{
  XmlAttributeBase *next = attr->next_attribute;
  XmlAttributeBase *prev = attr->prev_attribute_c;

  if (next != nullptr)
    next->prev_attribute_c = prev;
  else
    node->first_attribute->prev_attribute_c = prev;

  if (prev->next_attribute != nullptr)
    prev->next_attribute = next;
  else
    node->first_attribute = next;

  attr->prev_attribute_c = nullptr;
  attr->next_attribute = nullptr;
}

LUMEX_PUBLIC_API
void
lumex::xml::attribute::node_copy_attribute (XmlAttributeBase *da_,
                                            XmlAttributeBase *sa_)
{
  XmlAllocator &alloc = get_allocator (da_);
  XmlAllocator *shared_alloc
      = (&alloc == &get_allocator (sa_)) ? &alloc : nullptr;

  lumex::xml::utility::node_copy_string (da_->name, da_->header,
                                         kxml_memory_page_name_allocated_mask,
                                         sa_->name, sa_->header, shared_alloc);
  lumex::xml::utility::node_copy_string (
      da_->value, da_->header, kxml_memory_page_value_allocated_mask,
      sa_->value, sa_->header, shared_alloc);
}
