#ifndef LUMEX_XML_DOCUMENT_HPP
#define LUMEX_XML_DOCUMENT_HPP

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
      class XmlDocument : public XmlNode
      {
      public:
        // Default constructor, makes empty document
        XmlDocument();

        // Destructor, invalidates all node/attribute handles to this document
        ~XmlDocument();

        // Move semantics support
        XmlDocument(XmlDocument &&rhs) noexcept;

        XmlDocument &operator=(XmlDocument &&rhs) noexcept;

        // Removes all nodes, leaving the empty document
        void reset();

        // Removes all nodes, then copies the entire contents of the specified document
        void reset(XmlDocument const &proto);

        // Load document from stream.
        XmlParseResult load(std::basic_istream<char> &stream, unsigned int options = Constants::kparse_default,
                            xml_encoding encoding = encoding_auto);

        XmlParseResult load(std::basic_istream<wchar_t> &stream, unsigned int options = Constants::kparse_default);

        // (deprecated: use load_string instead) Load document from zero-terminated string. No encoding conversions are
        // applied.
        LUMEX_ATTRIBUTE_DEPRECATED XmlParseResult load(char_t const *contents,
                                                       unsigned int options = Constants::kparse_default);

        // Load document from zero-terminated string. No encoding conversions are applied.
        XmlParseResult load_string(char_t const *contents, unsigned int options = Constants::kparse_default);

        // Load document from file
        XmlParseResult load_file(char const *path, unsigned int options = Constants::kparse_default,
                                 xml_encoding encoding = encoding_auto);

        XmlParseResult load_file(wchar_t const *path, unsigned int options = Constants::kparse_default,
                                 xml_encoding encoding = encoding_auto);

        // Load document from buffer. Copies/converts the buffer, so it may be deleted or changed after the function
        // returns.
        XmlParseResult loadm_buffer(void const *contents, size_t size, unsigned int options = Constants::kparse_default,
                                    xml_encoding encoding = encoding_auto);

        // Load document from buffer, using the buffer for in-place parsing (the buffer is modified and used for storage
        // of document data). You should ensure that buffer data will persist throughout the document's lifetime, and
        // free the buffer memory manually once document is destroyed.
        XmlParseResult
        loadm_buffer_inplace(void *contents, size_t size, unsigned int options = Constants::kparse_default,
                             xml_encoding encoding = encoding_auto);

        // Load document from buffer, using the buffer for in-place parsing (the buffer is modified and used for storage
        // of document data). You should allocate the buffer with pugixml allocation function; document will free the
        // buffer when it is no longer needed (you can't use it anymore).
        XmlParseResult
        loadm_buffer_inplace_own(void *contents, size_t size, unsigned int options = Constants::kparse_default,
                                 xml_encoding encoding = encoding_auto);

        // Save XML document to writer (semantics is slightly different from XmlNode::print, see documentation for
        // details).
        void save(IXmlWriter &writer, char_t const *indent = LUMEX_XML_TEXT("\t"),
                  unsigned int flags = Constants::kformat_default, xml_encoding encoding = encoding_auto) const;

        // Save XML document to stream (semantics is slightly different from XmlNode::print, see documentation for
        // details).
        void save(std::basic_ostream<char> &stream, char_t const *indent = LUMEX_XML_TEXT("\t"),
                  unsigned int flags = Constants::kformat_default, xml_encoding encoding = encoding_auto) const;

        void save(std::basic_ostream<wchar_t> &stream, char_t const *indent = LUMEX_XML_TEXT("\t"),
                  unsigned int flags = Constants::kformat_default) const;

        // Save XML to file
        bool save_file(char const *path, char_t const *indent = LUMEX_XML_TEXT("\t"),
                       unsigned int flags = Constants::kformat_default, xml_encoding encoding = encoding_auto) const;

        bool save_file(wchar_t const *path, char_t const *indent = LUMEX_XML_TEXT("\t"),
                       unsigned int flags = Constants::kformat_default, xml_encoding encoding = encoding_auto) const;

        // Get document element
        LUMEX_ATTRIBUTE_NODISCARD(
          "Ignoring the return value of 'document_element()' means ignoring the main entry point to the XML document "
          "content, which makes it impossible to access and manipulate the document.")
        XmlNode document_element() const;

      private:
        constexpr static size_t const kDefaultMemorySize = 192;

        char_t *m_buffer{};
        std::array<char, kDefaultMemorySize> m_memory{};

        // Non-copyable semantics
        XmlDocument(XmlDocument const &);
        XmlDocument &operator=(XmlDocument const &);

        void _create();
        void _destroy();
        void _move(XmlDocument &rhs) noexcept;
      };
    }
  }
}

#endif // !LUMEX_XML_DOCUMENT_HPP
