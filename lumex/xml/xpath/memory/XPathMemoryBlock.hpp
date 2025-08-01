#ifndef LUMEX_XML_XPATH_MEMORY_BLOCK_HPP
#define LUMEX_XML_XPATH_MEMORY_BLOCK_HPP

#include <array>

#include "lumex/xml/xpath/constants/XPathConstants.hpp"

using namespace Lumex::Xml::XPath::Constants;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Memory
      {
        struct XPathMemoryBlock {
          XPathMemoryBlock *next;
          size_t capacity;

          union {
            std::array<char, kxpath_memory_page_size> data;
            double alignment;
          };
        };
      } // namespace Memory
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_MEMORY_BLOCK_HPP
