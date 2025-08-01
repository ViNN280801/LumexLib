#ifndef LUMEX_XML_CONSTANTS_HPP
#define LUMEX_XML_CONSTANTS_HPP

#include <array>
#include <cstdint>

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Constants
    {
      static constexpr inline uintptr_t const kxml_memory_block_alignment
        = sizeof(void *); ///< Alignment of memory blocks
      static constexpr inline uintptr_t const kxml_memory_page_contents_shared_mask = 64; ///< Mask for shared contents
      static constexpr inline uintptr_t const kxml_memory_page_name_allocated_mask  = 32; ///< Mask for allocated name
      static constexpr inline uintptr_t const kxml_memory_page_value_allocated_mask = 16; ///< Mask for allocated value
      static constexpr inline uintptr_t const kxml_memory_page_type_mask            = 15; ///< Mask for node type

      static constexpr inline uintptr_t const
        kxml_memory_page_name_allocated_or_shared_mask ///< Mask for name allocated or shared
        = kxml_memory_page_name_allocated_mask | kxml_memory_page_contents_shared_mask;
      static constexpr inline uintptr_t const
        kxml_memory_page_value_allocated_or_shared_mask ///< Mask for value allocated or shared
        = kxml_memory_page_value_allocated_mask | kxml_memory_page_contents_shared_mask;

      static constexpr inline std::array<unsigned char, 256> const kchartype_table
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

      static constexpr inline std::array<unsigned char, 256> const kchartypex_table = {
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
      static constexpr inline unsigned int const kparse_minimal          = 0x0000;
      static constexpr inline unsigned int const kparse_pi               = 0x0001;
      static constexpr inline unsigned int const kparse_comments         = 0x0002;
      static constexpr inline unsigned int const kparse_cdata            = 0x0004;
      static constexpr inline unsigned int const kparse_ws_pcdata        = 0x0008;
      static constexpr inline unsigned int const kparse_escapes          = 0x0010;
      static constexpr inline unsigned int const kparse_eol              = 0x0020;
      static constexpr inline unsigned int const kparse_wconv_attribute  = 0x0040;
      static constexpr inline unsigned int const kparse_wnorm_attribute  = 0x0080;
      static constexpr inline unsigned int const kparse_declaration      = 0x0100;
      static constexpr inline unsigned int const kparse_doctype          = 0x0200;
      static constexpr inline unsigned int const kparse_ws_pcdata_single = 0x0400;
      static constexpr inline unsigned int const kparse_trim_pcdata      = 0x0800;
      static constexpr inline unsigned int const kparse_fragment         = 0x1000;
      static constexpr inline unsigned int const kparse_embed_pcdata     = 0x2000;
      static constexpr inline unsigned int const kparse_merge_pcdata     = 0x4000;
      static constexpr inline unsigned int const kparse_default
        = kparse_cdata | kparse_escapes | kparse_wconv_attribute | kparse_eol;
      static constexpr inline unsigned int const kparse_full
        = kparse_default | kparse_pi | kparse_comments | kparse_declaration | kparse_doctype;

      // ================== Formatting flags ==================
      static constexpr inline unsigned int const kformat_indent                 = 0x01;
      static constexpr inline unsigned int const kformat_write_bom              = 0x02;
      static constexpr inline unsigned int const kformat_raw                    = 0x04;
      static constexpr inline unsigned int const kformat_no_declaration         = 0x08;
      static constexpr inline unsigned int const kformat_no_escapes             = 0x10;
      static constexpr inline unsigned int const kformat_save_file_text         = 0x20;
      static constexpr inline unsigned int const kformat_indent_attributes      = 0x40;
      static constexpr inline unsigned int const kformat_no_empty_element_tags  = 0x80;
      static constexpr inline unsigned int const kformat_skip_control_chars     = 0x100;
      static constexpr inline unsigned int const kformat_attribute_single_quote = 0x200;
      static constexpr inline unsigned int const kformat_default                = kformat_indent;
      static constexpr inline int const kdefault_double_precision               = 0x11;
      static constexpr inline int const kdefault_float_precision                = 0x9;

      // ================== Numeric conversion constants ==================
      static constexpr inline unsigned int const kDecimalBase        = 10;
      static constexpr inline unsigned int const kHexadecimalBase    = 16;
      static constexpr inline unsigned int const kHexCharOffsetLimit = 6; // For 'a' through 'f'
      static constexpr inline size_t const kSizeOfU8Bytes            = 8; // sizeof(U) == 8

      // ================== String literal lengths ==================
      static constexpr inline size_t const kTrueStringLength  = 4; // Length of "true"
      static constexpr inline size_t const kFalseStringLength = 5; // Length of "false"
    } // namespace Constants
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_CONSTANTS_HPP
