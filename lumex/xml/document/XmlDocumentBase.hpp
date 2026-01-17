#ifndef LUMEX_XML_DOCUMENT_BASE_HPP
#define LUMEX_XML_DOCUMENT_BASE_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/core/utility/LumexAssert.hpp"

#include "lumex/xml/memory/XmlAllocator.hpp"
#include "lumex/xml/memory/XmlMemoryPage.hpp"
#include "lumex/xml/node/XmlNodeBase.hpp"

using namespace Lumex::Xml::Node;
using namespace Lumex::Xml::Memory;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Document
    {
      /**
       * @brief Base structure representing an XML document, combining node and allocator functionalities.
       * @details This structure serves as the root node for an XML document tree and simultaneously acts as the primary
       *          memory allocator for all nodes, attributes, and string data within that document. It inherits from
       *          `XmlNodeBase` to participate in the node hierarchy and from `XmlAllocator` to manage memory.
       *          It also holds pointers to the raw buffer if the document was loaded in-place or if extra buffers were
       * used.
       * @note This is an internal structure. `XmlDocument` is the public-facing API.
       * @see XmlDocument
       * @see XmlNodeBase
       * @see XmlAllocator
       */
      struct LUMEX_API XmlDocumentBase : public XmlNodeBase, public XmlAllocator {
        /**
         * @brief Constructs an `XmlDocumentBase` object.
         * @param[in] page A pointer to the `XmlMemoryPage` where this document base object itself resides.
         * @details Initializes the base node (`XmlNodeBase`) with `node_document` type and sets up the allocator
         * (`XmlAllocator`) to manage memory starting from the provided page.
         */
        XmlDocumentBase(XmlMemoryPage *page);

        /**
         * @brief Pointer to the raw buffer used for in-place parsing.
         * @details If the XML document was loaded using an in-place parsing method (e.g., `load_buffer_inplace`),
         *          this pointer holds the original buffer. The document uses this buffer directly for storage.
         * @note This pointer is not managed by the `XmlAllocator` and must be freed manually by the user if ownership
         * was not transferred.
         */
        char_t const *buffer{}; // NOLINT(misc-non-private-member-variables-in-classes)

        /**
         * @brief Head of a linked list of additional memory buffers.
         * @details This pointer points to the first `xml_extra_buffer` in a chain of buffers that might have been
         *          allocated during parsing if the initial buffer or internal pages were insufficient.
         * @note These buffers are explicitly managed and freed by `XmlDocument` during its destruction.
         */
        xml_extra_buffer *extra_buffers{}; // NOLINT(misc-non-private-member-variables-in-classes)
      };

      template <typename Object>
      /**
       * @brief Retrieves a reference to the `XmlDocumentBase` object that owns a given object (node or attribute).
       * @tparam Object The type of the object (e.g., `XmlNodeBase`, `XmlAttributeBase`).
       * @param[in] object A pointer to the object whose owning document is to be retrieved. Must not be `nullptr`.
       * @return A reference to the `XmlDocumentBase` that manages the memory for `object`.
       * @details This function works by accessing the `XmlMemoryPage` associated with `object` and then retrieving
       *          the allocator (which is the `XmlDocumentBase` itself) from that page.
       * @note This is an internal utility function, primarily used for memory management and ownership checks.
       * @throws `LUMEX_ASSERT` if `object` is `nullptr` (debug builds only).
       */
      LUMEX_API inline XmlDocumentBase &
      get_document(Object const *object)
      {
        LUMEX_ASSERT(object);

        return *static_cast<XmlDocumentBase *>(  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
          LUMEX_XML_GETPAGE(object)->allocator); // NOLINT(cppcoreguidelines-pro-type-const-cast)
      }
    } // namespace Document
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_DOCUMENT_BASE_HPP
