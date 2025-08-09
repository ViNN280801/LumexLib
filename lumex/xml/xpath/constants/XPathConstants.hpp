#ifndef LUMEX_XML_XPATH_CONSTANTS_HPP
#define LUMEX_XML_XPATH_CONSTANTS_HPP

#include <cstdint>

#include "lumex/xml/utility/XmlMacros.hpp"

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Constants
      {
        LUMEX_XML_CONSTANT size_t kxpath_memory_page_size = 0x1000; // 4 kb
        LUMEX_XML_CONSTANT size_t kxpath_ast_depth_limit  = 0x400;  // 1 kb
        LUMEX_XML_CONSTANT uintptr_t kxpath_memory_block_alignment
          = sizeof(double) > sizeof(void *) ? sizeof(double) : sizeof(void *);
      } // namespace Constants
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_CONSTANTS_HPP
