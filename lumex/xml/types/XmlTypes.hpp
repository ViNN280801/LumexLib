#ifndef LUMEX_XML_TYPES_HPP
#define LUMEX_XML_TYPES_HPP

#include <cstdint>
#include <string>

#include "lumex/xml/utility/XmlMacros.hpp"

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Types
    {
      using char_t   = LUMEX_XML_CHAR;
      using string_t = std::basic_string<char_t>;
#if __cplusplus >= 201703L
      using string_view_t = std::basic_string_view<char_t>;
#endif

      enum chartype_t : std::uint8_t
      {
        ct_parse_pcdata  = 1,  // \0, &, \r, <
        ct_parse_attr    = 2,  // \0, &, \r, ', "
        ct_parse_attr_ws = 4,  // \0, &, \r, ', ", \n, tab
        ct_space         = 8,  // \r, \n, space, tab
        ct_parse_cdata   = 16, // \0, ], >, \r
        ct_parse_comment = 32, // \0, -, >, \r
        ct_symbol        = 64, // Any symbol > 127, a-z, A-Z, 0-9, _, :, -, .
        ct_start_symbol  = 128 // Any symbol > 127, a-z, A-Z, _, :
      };

      enum chartypex_t : std::uint8_t
      {
        ctx_special_pcdata = 1, // Any symbol >= 0 and < 32 (except \t, \r, \n), &, <, >
        ctx_special_attr   = 2, // Any symbol >= 0 and < 32, &, <, ", '
        ctx_start_symbol   = 4, // Any symbol > 127, a-z, A-Z, _
        ctx_digit          = 8, // 0-9
        ctx_symbol         = 16 // Any symbol > 127, a-z, A-Z, 0-9, _, -, .
      };

      struct xml_mem_str_header_t {
        uint16_t page_offset; ///< Offset from page->data
        uint16_t full_size;   ///< 0 if string occupies whole page
      };

      enum xml_node_type : std::uint8_t
      {
        node_null,        ///< Empty (null) node handle
        node_document,    ///< A document tree's absolute root
        node_element,     ///< Element tag, i.e. '<node/>'
        node_pcdata,      ///< Plain character data, i.e. 'text'
        node_cdata,       ///< Character data, i.e. '<![CDATA[text]]>'
        node_comment,     ///< Comment tag, i.e. '<!-- text -->'
        node_pi,          ///< Processing instruction, i.e. '<?name?>'
        node_declaration, ///< Document declaration, i.e. '<?xml version="1.0"?>'
        node_doctype      ///< Document type declaration, i.e. '<!DOCTYPE doc>'
      };

      enum xml_encoding : std::uint8_t
      {
        encoding_auto,     ///< Auto-detect input encoding using BOM or < / <? detection; use UTF8 if BOM is not found
        encoding_utf8,     ///< UTF8 encoding
        encoding_utf16_le, ///< Little-endian UTF16
        encoding_utf16_be, ///< Big-endian UTF16
        encoding_utf16,    ///< UTF16 with native endianness
        encoding_utf32_le, ///< Little-endian UTF32
        encoding_utf32_be, ///< Big-endian UTF32
        encoding_utf32,    ///< UTF32 with native endianness
        encoding_wchar,    ///< The same encoding wchar_t has (either UTF16 or UTF32)
        encoding_latin1    ///< Latin1 encoding
      };

      enum xml_parse_status : std::uint8_t
      {
        status_ok = 0, // No error

        status_file_not_found, // File was not found during load_file()
        status_io_error,       // Error reading from file/stream
        status_out_of_memory,  // Could not allocate memory
        status_internal_error, // Internal error occurred

        status_unrecognized_tag, // Parser could not determine tag type

        status_bad_pi,               // Parsing error occurred while parsing document declaration/processing instruction
        status_bad_comment,          // Parsing error occurred while parsing comment
        status_bad_cdata,            // Parsing error occurred while parsing CDATA section
        status_bad_doctype,          // Parsing error occurred while parsing document type declaration
        status_bad_pcdata,           // Parsing error occurred while parsing PCDATA section
        status_bad_start_element,    // Parsing error occurred while parsing start element tag
        status_bad_attribute,        // Parsing error occurred while parsing element attribute
        status_bad_end_element,      // Parsing error occurred while parsing end element tag
        status_end_element_mismatch, // There was a mismatch of start-end tags (closing tag had incorrect name, some tag
                                     // was not closed or there was an excessive closing tag)

        status_append_invalid_root, // Unable to append nodes since root type is not node_element or node_document
                                    // (exclusive to xml_node::append_buffer)

        status_no_document_element // Parsing resulted in a document without element nodes
      };

      enum xpath_value_type : std::uint8_t
      {
        xpath_type_none,     // Unknown type (query failed to compile)
        xpath_type_node_set, // Node set (xpath_node_set)
        xpath_type_number,   // Number
        xpath_type_string,   // String
        xpath_type_boolean   // Boolean
      };

      struct xml_extra_buffer {
        char_t *buffer;
        xml_extra_buffer *next;
      };

      enum ast_type_t : std::uint8_t
      {
        ast_unknown,
        ast_op_or,                  // left or right
        ast_op_and,                 // left and right
        ast_op_equal,               // left = right
        ast_op_not_equal,           // left != right
        ast_op_less,                // left < right
        ast_op_greater,             // left > right
        ast_op_less_or_equal,       // left <= right
        ast_op_greater_or_equal,    // left >= right
        ast_op_add,                 // left + right
        ast_op_subtract,            // left - right
        ast_op_multiply,            // left * right
        ast_op_divide,              // left / right
        ast_op_mod,                 // left % right
        ast_op_negate,              // left - right
        ast_op_union,               // left | right
        ast_predicate,              // apply predicate to set; next points to next predicate
        ast_filter,                 // select * from left where right
        ast_string_constant,        // string constant
        ast_number_constant,        // number constant
        ast_variable,               // variable
        ast_func_last,              // last()
        ast_func_position,          // position()
        ast_func_count,             // count(left)
        ast_func_id,                // id(left)
        ast_func_local_name_0,      // local-name()
        ast_func_local_name_1,      // local-name(left)
        ast_func_namespace_uri_0,   // namespace-uri()
        ast_func_namespace_uri_1,   // namespace-uri(left)
        ast_func_name_0,            // name()
        ast_func_name_1,            // name(left)
        ast_func_string_0,          // string()
        ast_func_string_1,          // string(left)
        ast_func_concat,            // concat(left, right, siblings)
        ast_func_starts_with,       // starts_with(left, right)
        ast_func_contains,          // contains(left, right)
        ast_func_substring_before,  // substring-before(left, right)
        ast_func_substring_after,   // substring-after(left, right)
        ast_func_substring_2,       // substring(left, right)
        ast_func_substring_3,       // substring(left, right, third)
        ast_func_string_length_0,   // string-length()
        ast_func_string_length_1,   // string-length(left)
        ast_func_normalize_space_0, // normalize-space()
        ast_func_normalize_space_1, // normalize-space(left)
        ast_func_translate,         // translate(left, right, third)
        ast_func_boolean,           // boolean(left)
        ast_func_not,               // not(left)
        ast_func_true,              // true()
        ast_func_false,             // false()
        ast_func_lang,              // lang(left)
        ast_func_number_0,          // number()
        ast_func_number_1,          // number(left)
        ast_func_sum,               // sum(left)
        ast_func_floor,             // floor(left)
        ast_func_ceiling,           // ceiling(left)
        ast_func_round,             // round(left)
        ast_step,                   // process set left with step
        ast_step_root,              // select root node

        ast_opt_translate_table,  // translate(left, right, third) where right/third are constants
        ast_opt_compare_attribute // @name = 'string'
      };

      enum axis_t : std::uint8_t
      {
        axis_ancestor,
        axis_ancestor_or_self,
        axis_attribute,
        axis_child,
        axis_descendant,
        axis_descendant_or_self,
        axis_following,
        axis_following_sibling,
        axis_namespace,
        axis_parent,
        axis_preceding,
        axis_preceding_sibling,
        axis_self
      };

      enum nodetest_t : std::uint8_t
      {
        nodetest_none,
        nodetest_name,
        nodetest_type_node,
        nodetest_type_comment,
        nodetest_type_pi,
        nodetest_type_text,
        nodetest_pi,
        nodetest_all,
        nodetest_all_in_namespace
      };

      enum predicate_t : std::uint8_t
      {
        predicate_default,
        predicate_posinv,
        predicate_constant,
        predicate_constant_one
      };

      enum nodeset_eval_t : std::uint8_t
      {
        nodeset_eval_all,
        nodeset_eval_any,
        nodeset_eval_first
      };

      enum indent_flags_t : std::uint8_t
      {
        indent_newline = 1,
        indent_indent  = 2
      };
    } // namespace Types
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_TYPES_HPP
