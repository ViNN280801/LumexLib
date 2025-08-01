#include "lumex/xml/document/XmlDocumentBase.hpp"
#include "lumex/xml/text/XmlParser.hpp"
#include "lumex/xml/utility/XmlCleaner.hpp"
#include "lumex/xml/utility/XmlUtils.hpp"

#include "XmlParseResult.hpp"

using namespace Lumex::Xml::Utility;
using namespace Lumex::Xml::Document;
using namespace Lumex::Xml::Text;

inline XmlParseResult::XmlParseResult()
    : status(Types::xml_parse_status::status_internal_error), offset(0), encoding(Types::xml_encoding::encoding_auto)
{}

inline XmlParseResult::XmlParseResult(Types::xml_parse_status status)
    : status(status), offset(0), encoding(Types::xml_encoding::encoding_auto)
{}

inline XmlParseResult::
operator bool() const
{
  return status == Types::xml_parse_status::status_ok;
}

inline char const *
XmlParseResult::description() const
{
  switch(status)
  {
  case Types::xml_parse_status::status_ok: return "No error";

  case Types::xml_parse_status::status_file_not_found: return "File was not found";
  case Types::xml_parse_status::status_io_error: return "Error reading from file/stream";
  case Types::xml_parse_status::status_out_of_memory: return "Could not allocate memory";
  case Types::xml_parse_status::status_internal_error: return "Internal error occurred";

  case Types::xml_parse_status::status_unrecognized_tag: return "Could not determine tag type";

  case Types::xml_parse_status::status_bad_pi: return "Error parsing document declaration/processing instruction";
  case Types::xml_parse_status::status_bad_comment: return "Error parsing comment";
  case Types::xml_parse_status::status_bad_cdata: return "Error parsing CDATA section";
  case Types::xml_parse_status::status_bad_doctype: return "Error parsing document type declaration";
  case Types::xml_parse_status::status_bad_pcdata: return "Error parsing PCDATA section";
  case Types::xml_parse_status::status_bad_start_element: return "Error parsing start element tag";
  case Types::xml_parse_status::status_bad_attribute: return "Error parsing element attribute";
  case Types::xml_parse_status::status_bad_end_element: return "Error parsing end element tag";
  case Types::xml_parse_status::status_end_element_mismatch: return "Start-end tags mismatch";

  case Types::xml_parse_status::status_append_invalid_root:
    return "Unable to append nodes: root is not an element or document";

  case Types::xml_parse_status::status_no_document_element: return "No document element found";

  default: return "Unknown error";
  }
}

inline XmlParseResult
load_buffer_impl( // NOLINT(misc-use-internal-linkage)
  Document::XmlDocumentBase *doc, Node::XmlNodeBase *root, void *contents,
  size_t size, // NOLINT(bugprone-easily-swappable-parameters)
  unsigned int options, Types::xml_encoding encoding, bool is_mutable, bool own, Types::char_t **out_buffer)
{
  // check input buffer
  if((contents == nullptr) && (size != 0)) return make_parse_result(Types::xml_parse_status::status_io_error);

  // get actual encoding
  Types::xml_encoding buffer_encoding = Utility::get_buffer_encoding(encoding, contents, size);

  // if convert_buffer below throws bad_alloc, we still need to deallocate contents if we own it
  XmlCleaner<void> contents_guard(own ? contents : nullptr, free);

  // early-out for empty documents to avoid buffer allocation overhead
  if(size == 0)
    return make_parse_result(((options & kparse_fragment) != 0) ? Types::xml_parse_status::status_ok
                                                                : Types::xml_parse_status::status_no_document_element);

  // get private buffer
  char_t *buffer = nullptr;
  size_t length  = 0;

  if(!Utility::convert_buffer(buffer, length, buffer_encoding, contents, size, is_mutable))
    return make_parse_result(Types::xml_parse_status::status_out_of_memory);

  // after this we either deallocate contents (below) or hold on to it via doc->buffer, so we don't need to guard
  // it
  contents_guard.release();

  // delete original buffer if we performed a conversion
  if(own && buffer != contents && (contents != nullptr))
    free(contents); // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)

  // grab onto buffer if it's our buffer, user is responsible for deallocating contents himself
  if(own || buffer != contents) *out_buffer = buffer;

  // store buffer for offset_debug
  doc->buffer = buffer;

  // parse
  XmlParseResult res = XmlParser::parse(buffer, length, doc, root, options);

  // remember encoding
  res.encoding = buffer_encoding;

  return res;
}
