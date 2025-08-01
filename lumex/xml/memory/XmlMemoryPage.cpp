#include "XmlMemoryPage.hpp"

using namespace Lumex::Xml::Memory;

XmlMemoryPage *
XmlMemoryPage::construct(void *memory)
{
  auto *result       = static_cast<XmlMemoryPage *>(memory);

  result->allocator  = nullptr;
  result->prev       = nullptr;
  result->next       = nullptr;
  result->busy_size  = 0;
  result->freed_size = 0;

  return result;
}
