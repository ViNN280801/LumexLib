#ifndef ILUMEX_XML_IWRITER_HPP
#define ILUMEX_XML_IWRITER_HPP

#include "lumex/LumexExport.hpp"

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Writer
    {
      /**
       * @brief Abstract base interface for writing XML data to various destinations.
       * @details `IXmlWriter` defines a contract for classes that can output raw byte or character data,
       *          serving as a flexible target for XML serialization. Concrete implementations might write
       *          to files, memory buffers, or network streams.
       * @note This interface is crucial for decoupling XML tree serialization logic from output mechanics.
       * @see XmlNode::print()
       * @see XmlDocument::save()
       */
      class LUMEX_API IXmlWriter // NOLINT(cppcoreguidelines-special-member-functions)
      {
      public:
        /**
         * @brief Virtual destructor.
         * @details Ensures proper cleanup of derived `IXmlWriter` implementations.
         */
        virtual ~IXmlWriter() = default;

        /**
         * @brief Writes a block of raw data to the output.
         * @param[in] data A pointer to the data buffer to write. Must not be `nullptr`.
         * @param[in] size The number of bytes to write from the `data` buffer.
         * @details This is a pure virtual function that must be implemented by derived classes.
         *          It is responsible for the actual low-level writing operation.
         */
        virtual void write(void const *data, size_t size) = 0;
      };
    } // namespace Writer
  } // namespace Xml
} // namespace Lumex

#endif // !ILUMEX_XML_IWRITER_HPP
