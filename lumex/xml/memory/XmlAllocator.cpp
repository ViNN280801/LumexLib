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

#include <cstddef>
#include <cstdlib>

#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"

#include "lumex/xml/constants/XmlConstants.hpp"
#include "lumex/xml/types/XmlTypes.hpp"

#include "XmlAllocator.hpp"
#include "XmlMemoryPage.hpp"

using namespace lumex::xml::memory;
using namespace lumex::xml::constants;
using namespace lumex::xml::constants::Constants;
using namespace lumex::xml::types;
using namespace lumex::xml::types::Types;

LUMEX_PUBLIC_API
XmlAllocator::XmlAllocator (XmlMemoryPage *root)
    : m_root (root), m_busy_size (root->busy_size)
{
}

LUMEX_PUBLIC_API
XmlMemoryPage *
XmlAllocator::allocate_page (std::size_t data_size)
{
  std::size_t size = sizeof (XmlMemoryPage) + data_size;
  void *memory = malloc (size); // NOLINT(cppcoreguidelines-owning-memory,
                                // cppcoreguidelines-no-malloc)
  if (memory == nullptr)
    return nullptr;

  XmlMemoryPage *page = XmlMemoryPage::construct (memory);
  LUMEX_ASSERT (page);

  LUMEX_ASSERT (this == m_root->allocator);
  page->allocator = this;

  return page;
}

LUMEX_PUBLIC_API
void
XmlAllocator::deallocate_page (XmlMemoryPage *page)
{
  free (page); // NOLINT(cppcoreguidelines-owning-memory,
               // cppcoreguidelines-no-malloc)
}

LUMEX_PUBLIC_API
LUMEX_ATTRIBUTE_NOINLINE void *
XmlAllocator::allocate_memory_oob (std::size_t size, XmlMemoryPage *&out_page)
{
  std::size_t const large_allocation_threshold
      = kdefault_xml_memory_page_size / 4;

  XmlMemoryPage *page = allocate_page (size <= large_allocation_threshold
                                           ? kdefault_xml_memory_page_size
                                           : size);
  out_page = page;

  if (page == nullptr)
    return nullptr;

  if (size <= large_allocation_threshold)
    {
      m_root->busy_size = m_busy_size;

      page->prev = m_root;
      m_root->next = page;
      m_root = page;

      m_busy_size = size;
    }
  else
    {
      LUMEX_ASSERT (m_root->prev);

      page->prev = m_root->prev;
      page->next = m_root;

      m_root->prev->next = page;
      m_root->prev = page;

      page->busy_size = size;
    }

  return reinterpret_cast<char *> (page) + sizeof (XmlMemoryPage);
}

LUMEX_PUBLIC_API
void *
XmlAllocator::allocate_memory (std::size_t size, XmlMemoryPage *&out_page)
{
  if (LUMEX_ATTRIBUTE_UNLIKELY_COND (m_busy_size + size
                                     > kdefault_xml_memory_page_size))
    return allocate_memory_oob (size, out_page);

  void *buf = reinterpret_cast<char *> (m_root) + sizeof (XmlMemoryPage)
              + m_busy_size;

  m_busy_size += size;
  out_page = m_root;

  return buf;
}

LUMEX_PUBLIC_API
void *
XmlAllocator::allocate_object (std::size_t size, XmlMemoryPage *&out_page)
{
  return allocate_memory (size, out_page);
}

LUMEX_PUBLIC_API
void
XmlAllocator::deallocate_memory (void *ptr, std::size_t size,
                                 XmlMemoryPage *page)
{
  if (page == m_root)
    page->busy_size = m_busy_size;

  LUMEX_ASSERT (ptr >= reinterpret_cast<char *> (page) + sizeof (XmlMemoryPage)
                && ptr < reinterpret_cast<char *> (page)
                             + sizeof (XmlMemoryPage) + page->busy_size);
  (void)(ptr == nullptr);

  page->freed_size += size;
  LUMEX_ASSERT (page->freed_size <= page->busy_size);

  if (page->freed_size == page->busy_size)
    {
      if (page->next == nullptr)
        {
          LUMEX_ASSERT (m_root == page);

          page->busy_size = 0;
          page->freed_size = 0;

          m_busy_size = 0;
        }
      else
        {
          LUMEX_ASSERT (m_root != page);
          LUMEX_ASSERT (page->prev);

          page->prev->next = page->next;
          page->next->prev = page->prev;

          deallocate_page (page);
        }
    }
}

LUMEX_PUBLIC_API
char_t *
XmlAllocator::allocate_string (std::size_t length)
{
  static std::size_t const max_encoded_offset
      = (1 << 16) * kxml_memory_block_alignment;
  LUMEX_STATIC_ASSERT (kdefault_xml_memory_page_size <= max_encoded_offset);

  std::size_t size
      = sizeof (xml_mem_str_header_t) + (length * sizeof (char_t));
  std::size_t full_size = (size + (kxml_memory_block_alignment - 1))
                          & ~(kxml_memory_block_alignment - 1);

  XmlMemoryPage *page{};
  auto *header = static_cast<xml_mem_str_header_t *> (
      allocate_memory (full_size, page));

  if (header == nullptr)
    return nullptr;
  ptrdiff_t page_offset = reinterpret_cast<char *> (header)
                          - reinterpret_cast<char *> (page)
                          - static_cast<ptrdiff_t> (sizeof (XmlMemoryPage));

  LUMEX_ASSERT (static_cast<std::size_t> (page_offset)
                    % kxml_memory_block_alignment
                == 0);
  LUMEX_ASSERT (page_offset >= 0
                && static_cast<std::size_t> (page_offset)
                       < max_encoded_offset);
  header->page_offset = static_cast<uint16_t> (
      static_cast<std::size_t> (page_offset) / kxml_memory_block_alignment);

  LUMEX_ASSERT (full_size % kxml_memory_block_alignment == 0);
  LUMEX_ASSERT (full_size < max_encoded_offset
                || (page->busy_size == full_size && page_offset == 0));
  header->full_size = static_cast<uint16_t> (
      full_size < max_encoded_offset ? full_size / kxml_memory_block_alignment
                                     : 0);

  return reinterpret_cast<char_t *> (header + 1);
}

LUMEX_PUBLIC_API
void
XmlAllocator::deallocate_string (char_t *string)
{
  xml_mem_str_header_t *header
      = reinterpret_cast<xml_mem_str_header_t *> (string) - 1;
  LUMEX_ASSERT (header);

  std::size_t page_offset
      = sizeof (XmlMemoryPage)
        + (header->page_offset * kxml_memory_block_alignment);
  auto *page = reinterpret_cast<XmlMemoryPage *> (
      reinterpret_cast<char *> (header) - page_offset);

  std::size_t full_size
      = header->full_size == 0
            ? page->busy_size
            : header->full_size * kxml_memory_block_alignment;

  deallocate_memory (header, full_size, page);
}
