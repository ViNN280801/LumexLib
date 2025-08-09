#ifndef LUMEX_XML_PARSE_RESULT_HPP
#define LUMEX_XML_PARSE_RESULT_HPP

#include <cstddef>

#include "lumex/LumexExport.hpp"

#include "lumex/core/utility/LumexAttributes.hpp"

#include "lumex/xml/types/XmlTypes.hpp"

using namespace Lumex::Xml;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Document
    {
      class XmlDocumentBase;
    }

    namespace Node
    {
      class XmlNodeBase;
    }

    namespace Text
    {
      /**
       * @brief Represents the result of an XML parsing operation, including status, error offset, and encoding.
       * @details This structure provides comprehensive feedback on whether a parsing operation succeeded or failed,
       *          and if it failed, details about the error type and its location within the input data.
       */
      struct LUMEX_API XmlParseResult {
        /// @brief Parsing status (see `xml_parse_status` enumeration for possible values).
        Types::xml_parse_status status; // NOLINT(misc-non-private-member-variables-in-classes)

        /// @brief Last parsed offset in `char_t` units from the start of the input data.
        ///        Indicates the position of the error if `status` is not `status_ok`.
        ptrdiff_t offset; // NOLINT(misc-non-private-member-variables-in-classes)

        /// @brief The detected or specified character encoding of the source document.
        Types::xml_encoding encoding; // NOLINT(misc-non-private-member-variables-in-classes)

        /**
         * @brief Default constructor. Initializes the object to a failed state (`status_internal_error`).
         */
        XmlParseResult();

        /**
         * @brief Constructs an `XmlParseResult` with a specified status.
         * @param[in] status The parsing status to set for this result.
         */
        XmlParseResult(Types::xml_parse_status status);

        /**
         * @brief Conversion operator to `bool`.
         * @details Allows `XmlParseResult` objects to be used in boolean contexts (e.g., `if (result)`).
         *          It evaluates to `true` if `status` is `status_ok`, and `false` otherwise.
         * @return `true` if parsing was successful, `false` otherwise.
         */
        operator bool() const;

        /**
         * @brief Retrieves a human-readable description of the parsing status.
         * @return A null-terminated C-style string describing the parsing status.
         */
        LUMEX_ATTRIBUTE_NODISCARD("The returned value is the same as the one returned by the description() method; "
                                  "discarding it negates the purpose of the getter.")
        char const *description() const;
      };

      /**
       * @brief Helper function to create an `XmlParseResult` object.
       * @param[in] status The parsing status.
       * @param[in] offset The offset in the input data where the status occurred. Defaults to 0.
       * @return An `XmlParseResult` object initialized with the provided status and offset.
       */
      LUMEX_API
      inline XmlParseResult
      make_parse_result(Types::xml_parse_status status, ptrdiff_t offset = 0)
      {
        XmlParseResult result;
        result.status = status;
        result.offset = offset;

        return result;
      }

      /**
       * @brief Internal implementation for loading an XML document from a buffer.
       * @param[in,out] doc The `XmlDocumentBase` object to populate with parsed data.
       * @param[in,out] root The root `XmlNodeBase` where the parsed document structure will be attached.
       * @param[in] contents A pointer to the raw input data buffer.
       * @param[in] size The size of the input buffer in bytes.
       * @param[in] options A bitmask of `xml_parse_option` flags controlling parsing behavior.
       * @param[in] encoding The expected character encoding of the buffer.
       * @param[in] is_mutable `true` if the input buffer can be modified by the parser, `false` otherwise.
       * @param[in] own `true` if the parser should take ownership of `contents` and deallocate it, `false` otherwise.
       * @param[out] out_buffer A pointer to a `char_t*` that will receive the pointer to the internal buffer used for
       * parsing.
       * @return An `XmlParseResult` object indicating the parsing outcome.
       * @details This function handles buffer conversion, BOM skipping, memory ownership, and dispatches to the main
       * XML parsing logic. It is an internal utility function called by `XmlDocument::load`.
       */
      LUMEX_API
      XmlParseResult load_buffer_impl(Document::XmlDocumentBase *doc, Node::XmlNodeBase *root, void *contents,
                                      size_t size, // NOLINT(bugprone-easily-swappable-parameters)
                                      unsigned int options, Types::xml_encoding encoding, bool is_mutable, bool own,
                                      Types::char_t **out_buffer);
    } // namespace Text
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_PARSE_RESULT_HPP
