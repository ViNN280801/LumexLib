#ifndef LUMEX_XML_XPATH_MEMORY_BLOCK_HPP
#define LUMEX_XML_XPATH_MEMORY_BLOCK_HPP

#include "lumex/LumexExport.hpp"

#include <array>
#include <cstddef>

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
        /**
         * @brief Represents a block of memory used by XPathAllocator.
         * @details This structure defines a contiguous block of memory that the `XPathAllocator`
         *          uses for its allocations. Each block can link to the next, forming a
         *          singly linked list of memory pages. The union is used to ensure proper
         *          alignment of the data buffer for various data types.
         *
         * @note The `kxpath_memory_page_size` constant (defined in `XPathConstants.hpp`)
         *       determines the fixed size of the data buffer within each block.
         */
        struct LUMEX_API XPathMemoryBlock {
          /// @brief Pointer to the next memory block in the chain. `nullptr` if this is the last block.
          XPathMemoryBlock *next;
          /// @brief The total usable capacity of the `data` array in bytes.
          size_t capacity;

          union {
            /// @brief The raw character array used for general-purpose memory allocation.
            std::array<char, kxpath_memory_page_size> data;
            /// @brief A dummy member used to ensure the `data` array is aligned for `double`s.
            double alignment;
          };
        };
      } // namespace Memory
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_MEMORY_BLOCK_HPP
