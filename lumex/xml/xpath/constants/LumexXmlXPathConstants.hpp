#ifndef LUMEX_XML_XPATH_CONSTANTS_HPP
#define LUMEX_XML_XPATH_CONSTANTS_HPP

#include <cstdint>

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Constants
      {
        static constexpr inline size_t kxpath_memory_page_size = 0x1000; // 4 kb
        static constexpr inline size_t kxpath_ast_depth_limit  = 0x400;  // 1 kb
        static constexpr inline uintptr_t kxpath_memory_block_alignment
          = sizeof(double) > sizeof(void *) ? sizeof(double) : sizeof(void *);
      } // namespace Constants
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_CONSTANTS_HPP
