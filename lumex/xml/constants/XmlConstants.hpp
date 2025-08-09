#ifndef LUMEX_XML_CONSTANTS_HPP
#define LUMEX_XML_CONSTANTS_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "lumex/xml/utility/XmlMacros.hpp"

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Constants
    {
      LUMEX_XML_CONSTANT uintptr_t kxml_memory_block_alignment = sizeof(void *); ///< Alignment of memory blocks
      LUMEX_XML_CONSTANT uintptr_t kxml_memory_page_contents_shared_mask = 64;   ///< Mask for shared contents
      LUMEX_XML_CONSTANT uintptr_t kxml_memory_page_name_allocated_mask  = 32;   ///< Mask for allocated name
      LUMEX_XML_CONSTANT uintptr_t kxml_memory_page_value_allocated_mask = 16;   ///< Mask for allocated value
      LUMEX_XML_CONSTANT uintptr_t kxml_memory_page_type_mask            = 15;   ///< Mask for node type

      LUMEX_XML_CONSTANT uintptr_t kxml_memory_page_name_allocated_or_shared_mask ///< Mask for name allocated or shared
        = kxml_memory_page_name_allocated_mask | kxml_memory_page_contents_shared_mask;
      LUMEX_XML_CONSTANT uintptr_t
        kxml_memory_page_value_allocated_or_shared_mask ///< Mask for value allocated or shared
        = kxml_memory_page_value_allocated_mask | kxml_memory_page_contents_shared_mask;

      LUMEX_XML_CONSTANT std::array<unsigned char, 256> kchartype_table
        = {55,  0,   0,   0,   0,   0,   0,   0,   0,   12,  12,  0,   0,   63,  0,   0,   // 0-15
           0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 16-31
           8,   0,   6,   0,   0,   0,   7,   6,   0,   0,   0,   0,   0,   96,  64,  0,   // 32-47
           64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  192, 0,   1,   0,   48,  0,   // 48-63
           0,   192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, // 64-79
           192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 0,   0,   16,  0,   192, // 80-95
           0,   192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, // 96-111
           192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 0,   0,   0,   0,   0,   // 112-127

           192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, // 128+
           192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192,
           192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192,
           192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192,
           192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192,
           192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192,
           192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192};

      LUMEX_XML_CONSTANT std::array<unsigned char, 256> kchartypex_table = {
        3,  3,  3,  3,  3,  3,  3,  3,  3,  2,  2,  3,  3,  2,  3,  3, // 0-15
        3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3, // 16-31
        0,  0,  2,  0,  0,  0,  3,  2,  0,  0,  0,  0,  0,  16, 16, 0, // 32-47
        24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 0,  0,  3,  0,  1,  0, // 48-63

        0,  20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, // 64-79
        20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 0,  0,  0,  0,  20, // 80-95
        0,  20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, // 96-111
        20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 0,  0,  0,  0,  0,  // 112-127

        20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, // 128+
        20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
        20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
        20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
        20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20};

      // ================== Parse flags ==================
      LUMEX_XML_CONSTANT unsigned int kparse_minimal          = 0x0000;
      LUMEX_XML_CONSTANT unsigned int kparse_pi               = 0x0001;
      LUMEX_XML_CONSTANT unsigned int kparse_comments         = 0x0002;
      LUMEX_XML_CONSTANT unsigned int kparse_cdata            = 0x0004;
      LUMEX_XML_CONSTANT unsigned int kparse_ws_pcdata        = 0x0008;
      LUMEX_XML_CONSTANT unsigned int kparse_escapes          = 0x0010;
      LUMEX_XML_CONSTANT unsigned int kparse_eol              = 0x0020;
      LUMEX_XML_CONSTANT unsigned int kparse_wconv_attribute  = 0x0040;
      LUMEX_XML_CONSTANT unsigned int kparse_wnorm_attribute  = 0x0080;
      LUMEX_XML_CONSTANT unsigned int kparse_declaration      = 0x0100;
      LUMEX_XML_CONSTANT unsigned int kparse_doctype          = 0x0200;
      LUMEX_XML_CONSTANT unsigned int kparse_ws_pcdata_single = 0x0400;
      LUMEX_XML_CONSTANT unsigned int kparse_trim_pcdata      = 0x0800;
      LUMEX_XML_CONSTANT unsigned int kparse_fragment         = 0x1000;
      LUMEX_XML_CONSTANT unsigned int kparse_embed_pcdata     = 0x2000;
      LUMEX_XML_CONSTANT unsigned int kparse_merge_pcdata     = 0x4000;
      LUMEX_XML_CONSTANT unsigned int kparse_default
        = kparse_cdata | kparse_escapes | kparse_wconv_attribute | kparse_eol;
      LUMEX_XML_CONSTANT unsigned int kparse_full
        = kparse_default | kparse_pi | kparse_comments | kparse_declaration | kparse_doctype;

      // ================== Formatting flags ==================
      LUMEX_XML_CONSTANT unsigned int kformat_indent                 = 0x01;
      LUMEX_XML_CONSTANT unsigned int kformat_write_bom              = 0x02;
      LUMEX_XML_CONSTANT unsigned int kformat_raw                    = 0x04;
      LUMEX_XML_CONSTANT unsigned int kformat_no_declaration         = 0x08;
      LUMEX_XML_CONSTANT unsigned int kformat_no_escapes             = 0x10;
      LUMEX_XML_CONSTANT unsigned int kformat_save_file_text         = 0x20;
      LUMEX_XML_CONSTANT unsigned int kformat_indent_attributes      = 0x40;
      LUMEX_XML_CONSTANT unsigned int kformat_no_empty_element_tags  = 0x80;
      LUMEX_XML_CONSTANT unsigned int kformat_skip_control_chars     = 0x100;
      LUMEX_XML_CONSTANT unsigned int kformat_attribute_single_quote = 0x200;
      LUMEX_XML_CONSTANT unsigned int kformat_default                = kformat_indent;
      LUMEX_XML_CONSTANT int kdefault_double_precision               = 0x11;
      LUMEX_XML_CONSTANT int kdefault_float_precision                = 0x9;

      // ================== Numeric conversion constants ==================
      LUMEX_XML_CONSTANT unsigned int kDecimalBase        = 10;
      LUMEX_XML_CONSTANT unsigned int kHexadecimalBase    = 16;
      LUMEX_XML_CONSTANT unsigned int kHexCharOffsetLimit = 6; // For 'a' through 'f'
      LUMEX_XML_CONSTANT size_t kSizeOfU8Bytes            = 8; // sizeof(U) == 8

      // ================== String literal lengths ==================
      LUMEX_XML_CONSTANT size_t kTrueStringLength  = 4; // Length of "true"
      LUMEX_XML_CONSTANT size_t kFalseStringLength = 5; // Length of "false"
    } // namespace Constants
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_CONSTANTS_HPP
