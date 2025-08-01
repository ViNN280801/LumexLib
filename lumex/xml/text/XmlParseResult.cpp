#define LUMEX_IMPLEMENTATION

#include "XmlParseResult.hpp"

using namespace Lumex::Xml::Text;

inline XmlParseResult::XmlParseResult()
    : status(Types::xml_parse_status::status_internal_error), offset(0), encoding(Types::xml_encoding::encoding_auto)
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
