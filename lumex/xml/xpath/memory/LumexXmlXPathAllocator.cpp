#define LUMEX_IMPLEMENTATION

#include "lumex/core/utility/LumexMacros.hpp"

#include "LumexXmlXPathAllocator.hpp"

using namespace Lumex::Xml::XPath::Memory;

LUMEX_PUBLIC_API
LumexXmlXPathAllocator::LumexXmlXPathAllocator(LumexXmlXPathMemoryBlock *root, bool *error)
    : m_root(root), m_error(error)
{}

LUMEX_PUBLIC_API
void *
LumexXmlXPathAllocator::allocate(size_t size)
{
  // round size up to block alignment boundary
  size = (size + kxpath_memory_block_alignment - 1) & ~(kxpath_memory_block_alignment - 1);

  if(m_root_size + size <= m_root->capacity)
  {
    void *buf = &m_root->data[0] + m_root_size; // NOLINT(cppcoreguidelines-pro-type-union-access)
    m_root_size += size;
    return buf;
  }

  // make sure we have at least 1/4th of the page free after allocation to satisfy subsequent allocation
  // requests
  size_t block_capacity_base = sizeof(m_root->data); // NOLINT(cppcoreguidelines-pro-type-union-access)
  size_t block_capacity_req  = size + (block_capacity_base / 4);
  size_t block_capacity      = (block_capacity_base > block_capacity_req) ? block_capacity_base : block_capacity_req;

  size_t block_size          = block_capacity + offsetof(LumexXmlXPathMemoryBlock, data);

  auto *block                                                      // NOLINT(cppcoreguidelines-owning-memory)
    = static_cast<LumexXmlXPathMemoryBlock *>(malloc(block_size)); // NOLINT(cppcoreguidelines-no-malloc)
  if(block == nullptr)
  {
    if(m_error != nullptr) *m_error = true;
    return nullptr;
  }

  block->next     = m_root;
  block->capacity = block_capacity;

  m_root          = block;
  m_root_size     = size;

  return block->data.data(); // NOLINT(cppcoreguidelines-pro-type-union-access)
}

LUMEX_PUBLIC_API
void *
LumexXmlXPathAllocator::reallocate(void *ptr, size_t old_size, size_t new_size)
{
  // round size up to block alignment boundary
  old_size = (old_size + kxpath_memory_block_alignment - 1) & ~(kxpath_memory_block_alignment - 1);
  new_size = (new_size + kxpath_memory_block_alignment - 1) & ~(kxpath_memory_block_alignment - 1);

  // we can only reallocate the last object
  LUMEX_ASSERT(ptr == nullptr
               || static_cast<char *>(ptr) + old_size
                    == &m_root->data[0] + m_root_size); // NOLINT(cppcoreguidelines-pro-type-union-access)

  // try to reallocate the object inplace
  if((ptr != nullptr) && m_root_size - old_size + new_size <= m_root->capacity)
  {
    m_root_size = m_root_size - old_size + new_size;
    return ptr;
  }

  // allocate a new block
  void *result = allocate(new_size);
  if(result == nullptr) return nullptr;

  // we have a new block
  if(ptr != nullptr)
  {
    // copy old data (we only support growing)
    LUMEX_ASSERT(new_size >= old_size);
    memcpy(result, ptr, old_size);

    // free the previous page if it had no other objects
    LUMEX_ASSERT(m_root->data.data() == result); // NOLINT(cppcoreguidelines-pro-type-union-access)
    LUMEX_ASSERT(m_root->next);

    if(m_root->next->data.data() == ptr) // NOLINT(cppcoreguidelines-pro-type-union-access)
    {
      // deallocate the whole page, unless it was the first one
      LumexXmlXPathMemoryBlock *next = m_root->next->next;

      if(next != nullptr)
      {
        free(m_root->next); // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)
        m_root->next = next;
      }
    }
  }

  return result;
}

LUMEX_PUBLIC_API
void
LumexXmlXPathAllocator::revert(LumexXmlXPathAllocator const &state)
{
  // free all new pages
  LumexXmlXPathMemoryBlock *cur = m_root;

  while(cur != state.m_root)
  {
    LumexXmlXPathMemoryBlock *next = cur->next;

    free(cur); // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)

    cur = next;
  }

  // restore state
  m_root      = state.m_root;
  m_root_size = state.m_root_size;
}

LUMEX_PUBLIC_API
void
LumexXmlXPathAllocator::release() const noexcept
{
  LumexXmlXPathMemoryBlock *cur = m_root;
  LUMEX_ASSERT(cur);

  while(cur->next != nullptr)
  {
    LumexXmlXPathMemoryBlock *next = cur->next;

    free(cur); // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)

    cur = next;
  }
}

LumexXmlXPathAllocatorCapture::LumexXmlXPathAllocatorCapture(LumexXmlXPathAllocator *alloc)
    : _target(alloc), _state(*alloc)
{}

LumexXmlXPathAllocatorCapture::~LumexXmlXPathAllocatorCapture() { _target->revert(_state); }
