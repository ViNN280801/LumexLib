#ifndef LUMEX_XML_DOCUMENT_HPP
#define LUMEX_XML_DOCUMENT_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/core/utility/LumexAttributes.hpp"
#include "lumex/xml/node/XmlNode.hpp"
#include "lumex/xml/writer/IXmlWriter.hpp"

using namespace Lumex::Xml::Node;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Document
    {
      /**
       * @brief Represents an XML document, providing methods for loading, saving, and manipulating XML data.
       * @details This class is the primary entry point for working with XML documents. It inherits from `XmlNode`
       *          to allow treating the document itself as a root node for tree traversal. It manages its own memory
       *          allocation for the XML tree and associated data.
       * @note This class provides a comprehensive set of functions for parsing XML from various sources (files,
       * streams, buffers) and writing to different destinations. It supports different encoding options and
       * parsing/formatting flags.
       * @see XmlNode
       * @see IXmlWriter
       * @see XmlParseResult
       */
      class LUMEX_API XmlDocument : public XmlNode
      {
      public:
        /**
         * @brief Default constructor. Constructs an empty XML document.
         * @details Initializes the document to an empty state, ready to be loaded with XML data or to have nodes
         * created manually.
         * @note The internal memory buffer is initialized, and a root `XmlDocumentBase` node is created.
         */
        XmlDocument();

        /**
         * @brief Destructor. Invalidates all `XmlNode` and `XmlAttribute` handles associated with this document.
         * @details Frees all memory allocated by this document for its XML tree and associated buffers.
         *          Any `XmlNode` or `XmlAttribute` objects previously obtained from this document will become invalid
         *          after the destructor is called.
         * @warning Accessing invalidated handles after the document is destroyed leads to undefined behavior.
         */
        ~XmlDocument();

        // Move semantics support
        /**
         * @brief Move constructor. Transfers ownership of the XML document's resources from `rhs` to this object.
         * @param[in,out] rhs The `XmlDocument` object to move resources from. `rhs` will be left in a valid, empty
         * state.
         * @details After the move, this object will manage the XML tree and memory previously held by `rhs`.
         *          This operation is efficient as it only involves pointer reassignments, not deep copies.
         * @note No-throw guarantee (`noexcept`).
         */
        XmlDocument(XmlDocument &&rhs) noexcept;

        /**
         * @brief Move assignment operator. Transfers ownership of the XML document's resources from `rhs` to this
         * object.
         * @param[in,out] rhs The `XmlDocument` object to move resources from. `rhs` will be left in a valid, empty
         * state.
         * @return A reference to this `XmlDocument` object after the move.
         * @details Any resources currently held by this object are first destroyed, then resources from `rhs` are
         * moved. Self-assignment is handled safely.
         * @note No-throw guarantee (`noexcept`).
         */
        XmlDocument &operator=(XmlDocument &&rhs) noexcept;

        /**
         * @brief Removes all nodes from the document, leaving it in an empty state.
         * @details This method effectively clears the XML tree, deallocating all associated memory and resetting
         *          the document to the same state as if it were newly constructed.
         * @note All `XmlNode` and `XmlAttribute` handles pointing to this document's previous content will become
         * invalid.
         */
        void reset();

        /**
         * @brief Removes all nodes from the document, then copies the entire contents of a specified document.
         * @param[in] proto The `XmlDocument` object whose content will be copied into this document.
         * @details This performs a deep copy of the XML tree from `proto` to this document.
         *          All existing content in this document is first cleared via `reset()`.
         * @note This operation involves memory allocation and copying, and can be relatively expensive for large
         * documents.
         */
        void reset(XmlDocument const &proto);

        /**
         * @brief Loads an XML document from a C-style character stream.
         * @details Parses XML data from the provided input stream (`std::basic_istream<char>`).
         *          The document is reset before loading. Encoding autodetection is performed by default.
         * @param[in,out] stream The input stream containing XML data.
         * @param[in] options A bitmask of `Constants::xml_parse_option` flags controlling the parsing behavior (e.g.,
         * validation, whitespace handling). Defaults to `Constants::kparse_default`.
         * @param[in] encoding The expected character encoding of the stream. Defaults to `encoding_auto` for
         * autodetection.
         * @return An `XmlParseResult` object indicating the success or failure of the parsing operation, along with any
         * error details.
         * @throws `std::ios_base::failure` if stream operations fail and exceptions are enabled on the stream.
         * @note This method copies stream data into an internal buffer. The stream can be closed after this function
         * returns.
         */
        XmlParseResult load(std::basic_istream<char> &stream, unsigned int options = Constants::kparse_default,
                            xml_encoding encoding = encoding_auto);

        /**
         * @brief Loads an XML document from a wide character stream.
         * @details Parses XML data from the provided input stream (`std::basic_istream<wchar_t>`).
         *          The document is reset before loading. This method assumes `encoding_wchar` internally.
         * @param[in,out] stream The input stream containing XML data.
         * @param[in] options A bitmask of `Constants::xml_parse_option` flags controlling the parsing behavior.
         * Defaults to `Constants::kparse_default`.
         * @return An `XmlParseResult` object indicating the success or failure of the parsing operation.
         * @throws `std::ios_base::failure` if stream operations fail and exceptions are enabled on the stream.
         * @note This method copies stream data into an internal buffer. The stream can be closed after this function
         * returns.
         */
        XmlParseResult load(std::basic_istream<wchar_t> &stream, unsigned int options = Constants::kparse_default);

        // (deprecated: use load_string instead) Load document from zero-terminated string. No encoding conversions are
        // applied.
        /**
         * @brief DEPRECATED: Loads an XML document from a zero-terminated C-style string. Use `load_string` instead.
         * @details Parses XML data directly from the provided string. No encoding conversions are applied;
         *          the input `char_t` string is assumed to be in the native character mode of the library (`char` or
         * `wchar_t`). The document is reset before loading.
         * @param[in] contents A pointer to the zero-terminated string containing the XML data. Must remain valid during
         * parsing.
         * @param[in] options A bitmask of `Constants::xml_parse_option` flags. Defaults to `Constants::kparse_default`.
         * @return An `XmlParseResult` object indicating the parsing outcome.
         * @deprecated This function is deprecated. Use `load_string(char_t const*, unsigned int)` for new code.
         * @note This function copies the buffer internally.
         */
        LUMEX_ATTRIBUTE_DEPRECATED XmlParseResult load(char_t const *contents,
                                                       unsigned int options = Constants::kparse_default);

        // Load document from zero-terminated string. No encoding conversions are applied.
        /**
         * @brief Loads an XML document from a zero-terminated C-style string.
         * @details Parses XML data directly from the provided string. No encoding conversions are applied;
         *          the input `char_t` string is assumed to be in the native character mode of the library (`char` or
         * `wchar_t`). The document is reset before loading.
         * @param[in] contents A pointer to the zero-terminated string containing the XML data. Must remain valid during
         * parsing.
         * @param[in] options A bitmask of `Constants::xml_parse_option` flags controlling the parsing behavior.
         * Defaults to `Constants::kparse_default`.
         * @return An `XmlParseResult` object indicating the success or failure of the parsing operation.
         * @note This function copies the buffer internally.
         */
        XmlParseResult load_string(char_t const *contents, unsigned int options = Constants::kparse_default);

        // Load document from file
        /**
         * @brief Loads an XML document from a file specified by a C-style character path.
         * @details Opens the file at `path`, reads its entire content into memory, and then parses it as an XML
         * document. The document is reset before loading. Encoding autodetection is performed by default.
         * @param[in] path The null-terminated C-style string path to the XML file.
         * @param[in] options A bitmask of `Constants::xml_parse_option` flags. Defaults to `Constants::kparse_default`.
         * @param[in] encoding The expected character encoding of the file. Defaults to `encoding_auto` for
         * autodetection.
         * @return An `XmlParseResult` object indicating the success or failure (e.g., file not found, I/O error,
         * parsing error).
         * @note The file is read into an internal buffer and then closed. The buffer is managed by the `XmlDocument`.
         * @warning This function attempts to determine file size and read the entire file. For very large files, this
         * might consume significant memory.
         */
        XmlParseResult load_file(char const *path, unsigned int options = Constants::kparse_default,
                                 xml_encoding encoding = encoding_auto);

        /**
         * @brief Loads an XML document from a file specified by a wide character path.
         * @details Similar to `load_file(char const*, ...)`, but takes a wide character path.
         *          The document is reset before loading. Encoding autodetection is performed by default.
         * @param[in] path The null-terminated wide character string path to the XML file.
         * @param[in] options A bitmask of `Constants::xml_parse_option` flags. Defaults to `Constants::kparse_default`.
         * @param[in] encoding The expected character encoding of the file. Defaults to `encoding_auto` for
         * autodetection.
         * @return An `XmlParseResult` object indicating the success or failure.
         * @note The file is read into an internal buffer and then closed. The buffer is managed by the `XmlDocument`.
         * @warning On some platforms, this might involve internal conversion of the wide path to a multi-byte path.
         */
        XmlParseResult load_file(wchar_t const *path, unsigned int options = Constants::kparse_default,
                                 xml_encoding encoding = encoding_auto);

        // Load document from buffer. Copies/converts the buffer, so it may be deleted or changed after the function
        // returns.
        /**
         * @brief Loads an XML document from a read-only memory buffer.
         * @details Copies the content of the provided `contents` buffer into the document's internal memory
         *          and then parses it. The original `contents` buffer can be safely modified or deleted
         *          after this function returns, as the document owns its copy. The document is reset before loading.
         * @param[in] contents A pointer to the beginning of the memory buffer containing XML data.
         * @param[in] size The size of the buffer in bytes.
         * @param[in] options A bitmask of `Constants::xml_parse_option` flags. Defaults to `Constants::kparse_default`.
         * @param[in] encoding The expected character encoding of the buffer. Defaults to `encoding_auto` for
         * autodetection.
         * @return An `XmlParseResult` object indicating the parsing outcome.
         */
        XmlParseResult load_buffer(void const *contents, size_t size, unsigned int options = Constants::kparse_default,
                                   xml_encoding encoding = encoding_auto);

        // Load document from buffer, using the buffer for in-place parsing (the buffer is modified and used for storage
        // of document data). You should ensure that buffer data will persist throughout the document's lifetime, and
        // free the buffer memory manually once document is destroyed.
        /**
         * @brief Loads an XML document by parsing a provided memory buffer in-place.
         * @details This function directly uses the `contents` buffer for storing the parsed XML data. The buffer will
         * be modified during parsing (e.g., null terminators added). The caller is responsible for ensuring that
         * `contents` remains valid and persists for the entire lifetime of the `XmlDocument` object. The buffer must be
         * freed manually by the caller after the `XmlDocument` is destroyed or reset. The document is reset before
         * loading.
         * @param[in,out] contents A pointer to the beginning of the writable memory buffer containing XML data.
         * @param[in] size The size of the buffer in bytes.
         * @param[in] options A bitmask of `Constants::xml_parse_option` flags. Defaults to `Constants::kparse_default`.
         * @param[in] encoding The expected character encoding of the buffer. Defaults to `encoding_auto` for
         * autodetection.
         * @return An `XmlParseResult` object indicating the parsing outcome.
         * @warning The `contents` buffer is directly used and modified. Do not free or modify it until the
         * `XmlDocument` is destroyed or `reset()`.
         */
        XmlParseResult
        load_buffer_inplace(void *contents, size_t size, unsigned int options = Constants::kparse_default,
                            xml_encoding encoding = encoding_auto);

        // Load document from buffer, using the buffer for in-place parsing (the buffer is modified and used for storage
        // of document data). You should allocate the buffer with pugixml allocation function; document will free the
        // buffer when it is no longer needed (you can't use it anymore).
        /**
         * @brief Loads an XML document by parsing a provided memory buffer in-place, with ownership transfer.
         * @details This function uses the `contents` buffer for in-place parsing and then takes ownership of it.
         *          The `XmlDocument` will manage the lifetime of `contents` and free it when the document is destroyed
         * or reset. The buffer will be modified during parsing. The caller must ensure `contents` was allocated using a
         * compatible allocation function (e.g., `malloc` or `XmlAllocator`'s allocation if exposed). The document is
         * reset before loading.
         * @param[in,out] contents A pointer to the beginning of the writable memory buffer containing XML data. The
         * document takes ownership of this buffer.
         * @param[in] size The size of the buffer in bytes.
         * @param[in] options A bitmask of `Constants::xml_parse_option` flags. Defaults to `Constants::kparse_default`.
         * @param[in] encoding The expected character encoding of the buffer. Defaults to `encoding_auto` for
         * autodetection.
         * @return An `XmlParseResult` object indicating the parsing outcome.
         * @warning Once this function returns successfully, the caller must not access or free `contents` as its
         * ownership has been transferred.
         */
        XmlParseResult
        load_buffer_inplace_own(void *contents, size_t size, unsigned int options = Constants::kparse_default,
                                xml_encoding encoding = encoding_auto);

        // Save XML document to writer (semantics is slightly different from XmlNode::print, see documentation for
        // details).
        /**
         * @brief Saves the XML document to a custom writer interface.
         * @details This method serializes the XML document's content to the provided `IXmlWriter` implementation.
         *          It offers control over indentation, formatting flags, and output encoding.
         *          The function manages writing the XML declaration and BOM if required by the flags.
         * @param[in,out] writer An `IXmlWriter` implementation to which the XML data will be written.
         * @param[in] indent A null-terminated C-style string used for indentation. Defaults to a tab character
         * `LUMEX_XML_TEXT("\t")`.
         * @param[in] flags A bitmask of `Constants::xml_format_option` flags controlling output formatting (e.g.,
         * compact, no declaration, write BOM). Defaults to `Constants::kformat_default`.
         * @param[in] encoding The character encoding for the output. Defaults to `encoding_auto` for automatic
         * selection based on library mode.
         * @note The semantics differ slightly from `XmlNode::print` as this method handles the entire document
         * structure including declarations.
         * @see IXmlWriter
         * @see Constants::xml_format_option
         */
        void save(IXmlWriter &writer, char_t const *indent = LUMEX_XML_TEXT("\t"),
                  unsigned int flags = Constants::kformat_default, xml_encoding encoding = encoding_auto) const;

        // Save XML document to stream (semantics is slightly different from XmlNode::print, see documentation for
        // details).
        /**
         * @brief Saves the XML document to a C-style character output stream.
         * @details Serializes the XML document to the provided `std::basic_ostream<char>`. This is a convenience
         *          wrapper around `save(IXmlWriter&, ...)`, using an internal `XmlWriterStream`.
         * @param[in,out] stream The output stream to which the XML data will be written.
         * @param[in] indent A null-terminated C-style string for indentation. Defaults to `LUMEX_XML_TEXT("\t")`.
         * @param[in] flags A bitmask of `Constants::xml_format_option` flags. Defaults to `Constants::kformat_default`.
         * @param[in] encoding The character encoding for the output. Defaults to `encoding_auto`.
         * @throws `std::ios_base::failure` if stream operations fail and exceptions are enabled on the stream.
         */
        void save(std::basic_ostream<char> &stream, char_t const *indent = LUMEX_XML_TEXT("\t"),
                  unsigned int flags = Constants::kformat_default, xml_encoding encoding = encoding_auto) const;

        /**
         * @brief Saves the XML document to a wide character output stream.
         * @details Serializes the XML document to the provided `std::basic_ostream<wchar_t>`. This is a convenience
         *          wrapper around `save(IXmlWriter&, ...)`, using an internal `XmlWriterStream`.
         *          The output encoding is fixed to `encoding_wchar`.
         * @param[in,out] stream The output stream to which the XML data will be written.
         * @param[in] indent A null-terminated C-style string for indentation. Defaults to `LUMEX_XML_TEXT("\t")`.
         * @param[in] flags A bitmask of `Constants::xml_format_option` flags. Defaults to `Constants::kformat_default`.
         * @throws `std::ios_base::failure` if stream operations fail and exceptions are enabled on the stream.
         */
        void save(std::basic_ostream<wchar_t> &stream, char_t const *indent = LUMEX_XML_TEXT("\t"),
                  unsigned int flags = Constants::kformat_default) const;

        // Save XML to file
        /**
         * @brief Saves the XML document to a file specified by a C-style character path.
         * @details Opens the file at `path` for writing and saves the XML document's content to it.
         *          The file is opened in binary mode (`"wb"`) by default, or text mode (`"w"`) if
         * `Constants::kformat_save_file_text` flag is set.
         * @param[in] path The null-terminated C-style string path to the output XML file.
         * @param[in] indent A null-terminated C-style string for indentation. Defaults to `LUMEX_XML_TEXT("\t")`.
         * @param[in] flags A bitmask of `Constants::xml_format_option` flags. Defaults to `Constants::kformat_default`.
         * @param[in] encoding The character encoding for the output. Defaults to `encoding_auto`.
         * @return `true` if the document was successfully saved to the file, `false` otherwise (e.g., file cannot be
         * opened).
         */
        bool save_file(char const *path, char_t const *indent = LUMEX_XML_TEXT("\t"),
                       unsigned int flags = Constants::kformat_default, xml_encoding encoding = encoding_auto) const;

        /**
         * @brief Saves the XML document to a file specified by a wide character path.
         * @details Opens the file at `path` for writing and saves the XML document's content to it.
         *          The file is opened in binary mode (`L"wb"`) by default, or text mode (`L"w"`) if
         * `Constants::kformat_save_file_text` flag is set.
         * @param[in] path The null-terminated wide character string path to the output XML file.
         * @param[in] indent A null-terminated C-style string for indentation. Defaults to `LUMEX_XML_TEXT("\t")`.
         * @param[in] flags A bitmask of `Constants::xml_format_option` flags. Defaults to `Constants::kformat_default`.
         * @param[in] encoding The character encoding for the output. Defaults to `encoding_auto`.
         * @return `true` if the document was successfully saved to the file, `false` otherwise.
         */
        bool save_file(wchar_t const *path, char_t const *indent = LUMEX_XML_TEXT("\t"),
                       unsigned int flags = Constants::kformat_default, xml_encoding encoding = encoding_auto) const;

        // Get document element
        /**
         * @brief Retrieves the document element (root element) of the XML document.
         * @details The document element is the first child node of type `node_element` found directly under the
         * document's root. If no such element exists (e.g., an empty document or a document with only
         * comments/processing instructions), an empty `XmlNode` is returned.
         * @return An `XmlNode` object representing the document element, or an empty `XmlNode` if not found.
         * @note The document itself is not an element; it contains the document element.
         * @warning Discarding the return value means ignoring the primary content of the XML document.
         */
        LUMEX_ATTRIBUTE_NODISCARD(
          "Ignoring the return value of 'document_element()' means ignoring the main entry point to the XML document "
          "content, which makes it impossible to access and manipulate the document.")
        XmlNode document_element() const;

      private:
        constexpr static size_t const kDefaultMemorySize = 192;

        char_t *m_buffer{};
        char_t m_memory[kDefaultMemorySize]; // NOLINT(cppcoreguidelines-avoid-c-arrays, modernize-avoid-c-arrays)

        // Non-copyable semantics
        XmlDocument(XmlDocument const &);
        XmlDocument &operator=(XmlDocument const &);

        /**
         * @brief Internal helper function to create a new, empty XML document structure.
         * @details This function initializes the internal memory buffer (`m_memory`) and constructs the root
         * `XmlDocumentBase` node within it. It ensures the document is in a valid, usable state after construction or
         * reset.
         * @note This method asserts that `m_root` is `nullptr` before creation to prevent memory leaks if called on an
         * already initialized document.
         * @private
         */
        void _create();
        /**
         * @brief Internal helper function to destroy the XML document's resources.
         * @details This function is responsible for deallocating all dynamically allocated memory associated with the
         * XML tree, including any loaded buffers (`m_buffer`) and extra buffers. It traverses and frees all
         * `XmlMemoryPage` objects except for the initial sentinel page embedded in `m_memory`.
         * @note This method asserts that `m_root` is valid before destruction.
         * @private
         */
        void _destroy();
        /**
         * @brief Internal helper function to move resources from another `XmlDocument` object.
         * @param[in,out] rhs The `XmlDocument` object from which to move resources. Its state will be reset to empty.
         * @details This function reassigns internal pointers (`m_root`, `m_buffer`, `m_memory` related pages)
         *          to transfer ownership of the XML tree and associated memory from `rhs` to the current object.
         *          It also updates parent pointers for child nodes and allocator references in memory pages to reflect
         * the new ownership.
         * @note No-throw guarantee (`noexcept`). This is a low-level move operation used by the move constructor and
         * assignment operator.
         * @private
         */
        void _move(XmlDocument &rhs) noexcept;
      };
    }
  }
}

#endif // !LUMEX_XML_DOCUMENT_HPP
