#define LUMEX_IMPLEMENTATION

#include <cstdlib>

#include "LumexXmlAllocator.hpp"
#include "LumexXmlMemoryPage.hpp"

#include "lumex/core/utility/LumexAttributes.hpp"
#include "lumex/core/utility/LumexMacros.hpp"

#include "lumex/xml/constants/LumexXmlConstants.hpp"
#include "lumex/xml/types/LumexXmlTypes.hpp"

using namespace Lumex::Xml::Memory;
using namespace Lumex::Xml::Constants;
using namespace Lumex::Xml::Types;

LUMEX_PUBLIC_API
xml_allocator_t::xml_allocator_t(xml_mem_page_t *root) : m_root(root), m_busy_size(root->busy_size) {}

LUMEX_PUBLIC_API
xml_mem_page_t *
xml_allocator_t::allocate_page(size_t data_size)
{
  size_t size  = sizeof(xml_mem_page_t) + data_size;
  void *memory = malloc(size); // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)
  if(memory == nullptr) return nullptr;

  xml_mem_page_t *page = xml_mem_page_t::construct(memory);
  LUMEX_ASSERT(page);

  LUMEX_ASSERT(this == m_root->allocator);
  page->allocator = this;

  return page;
}

LUMEX_PUBLIC_API
void
xml_allocator_t::deallocate_page(xml_mem_page_t *page)
{
  free(page); // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)
}

LUMEX_PUBLIC_API
LUMEX_ATTRIBUTE_NOINLINE void *
xml_allocator_t::allocate_memory_oob(size_t size, xml_mem_page_t *&out_page)
{
  size_t const large_allocation_threshold = kdefault_xml_memory_page_size / 4;

  xml_mem_page_t *page = allocate_page(size <= large_allocation_threshold ? kdefault_xml_memory_page_size : size);
  out_page             = page;

  if(page == nullptr) return nullptr;

  if(size <= large_allocation_threshold)
  {
    m_root->busy_size = m_busy_size;

    page->prev        = m_root;
    m_root->next      = page;
    m_root            = page;

    m_busy_size       = size;
  }
  else
  {
    LUMEX_ASSERT(m_root->prev);

    page->prev         = m_root->prev;
    page->next         = m_root;

    m_root->prev->next = page;
    m_root->prev       = page;

    page->busy_size    = size;
  }

  return reinterpret_cast<char *>(page) + sizeof(xml_mem_page_t);
}

LUMEX_PUBLIC_API
void *
xml_allocator_t::allocate_memory(size_t size, xml_mem_page_t *&out_page)
{
  if(LUMEX_ATTRIBUTE_UNLIKELY_COND(m_busy_size + size > kdefault_xml_memory_page_size))
    return allocate_memory_oob(size, out_page);

  void *buf = reinterpret_cast<char *>(m_root) + sizeof(xml_mem_page_t) + m_busy_size;

  m_busy_size += size;
  out_page = m_root;

  return buf;
}

LUMEX_PUBLIC_API
void *
xml_allocator_t::allocate_object(size_t size, xml_mem_page_t *&out_page)
{
  return allocate_memory(size, out_page);
}

LUMEX_PUBLIC_API
void
xml_allocator_t::deallocate_memory(void *ptr, size_t size, xml_mem_page_t *page)
{
  if(page == m_root) page->busy_size = m_busy_size;

  LUMEX_ASSERT(ptr >= reinterpret_cast<char *>(page) + sizeof(xml_mem_page_t)
               && ptr < reinterpret_cast<char *>(page) + sizeof(xml_mem_page_t) + page->busy_size);
  (void)(ptr == nullptr);

  page->freed_size += size;
  LUMEX_ASSERT(page->freed_size <= page->busy_size);

  if(page->freed_size == page->busy_size)
  {
    if(page->next == nullptr)
    {
      LUMEX_ASSERT(m_root == page);

      page->busy_size  = 0;
      page->freed_size = 0;

      m_busy_size      = 0;
    }
    else
    {
      LUMEX_ASSERT(m_root != page);
      LUMEX_ASSERT(page->prev);

      page->prev->next = page->next;
      page->next->prev = page->prev;

      deallocate_page(page);
    }
  }
}

LUMEX_PUBLIC_API
char_t *
xml_allocator_t::allocate_string(size_t length)
{
  static size_t const max_encoded_offset = (1 << 16) * kxml_memory_block_alignment;
  static_assert(kdefault_xml_memory_page_size <= max_encoded_offset);

  size_t size      = sizeof(xml_mem_str_header_t) + (length * sizeof(char_t));
  size_t full_size = (size + (kxml_memory_block_alignment - 1)) & ~(kxml_memory_block_alignment - 1);

  xml_mem_page_t *page{};
  auto *header = static_cast<xml_mem_str_header_t *>(allocate_memory(full_size, page));

  if(header == nullptr) return nullptr;
  ptrdiff_t page_offset = reinterpret_cast<char *>(header) - reinterpret_cast<char *>(page)
                          - static_cast<ptrdiff_t>(sizeof(xml_mem_page_t));

  LUMEX_ASSERT(page_offset % kxml_memory_block_alignment == 0);
  LUMEX_ASSERT(page_offset >= 0 && static_cast<size_t>(page_offset) < max_encoded_offset);
  header->page_offset = static_cast<uint16_t>(static_cast<size_t>(page_offset) / kxml_memory_block_alignment);

  LUMEX_ASSERT(full_size % kxml_memory_block_alignment == 0);
  LUMEX_ASSERT(full_size < max_encoded_offset || (page->busy_size == full_size && page_offset == 0));
  header->full_size
    = static_cast<uint16_t>(full_size < max_encoded_offset ? full_size / kxml_memory_block_alignment : 0);

  return reinterpret_cast<char_t *>(header + 1);
}

LUMEX_PUBLIC_API
void
xml_allocator_t::deallocate_string(char_t *string)
{
  xml_mem_str_header_t *header = reinterpret_cast<xml_mem_str_header_t *>(string) - 1;
  LUMEX_ASSERT(header);

  size_t page_offset = sizeof(xml_mem_page_t) + (header->page_offset * kxml_memory_block_alignment);
  auto *page         = reinterpret_cast<xml_mem_page_t *>(reinterpret_cast<char *>(header) - page_offset);

  size_t full_size   = header->full_size == 0 ? page->busy_size : header->full_size * kxml_memory_block_alignment;

  deallocate_memory(header, full_size, page);
}
