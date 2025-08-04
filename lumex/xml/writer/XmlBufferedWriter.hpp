#ifndef LUMEX_XML_WRITER_XML_BUFFERED_WRITER_HPP
#define LUMEX_XML_WRITER_XML_BUFFERED_WRITER_HPP

#include "lumex/LumexExport.hpp"

#include <array>
#include <cstddef>

#include "lumex/xml/types/XmlTypes.hpp"

#include "IXmlWriter.hpp"

using namespace Lumex::Xml::Types;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Writer
    {

      class LUMEX_API XmlBufferedWriter // NOLINT(cppcoreguidelines-special-member-functions)
      {
      public:
        static constexpr short const kBufCapacity8 = 8;
        static constexpr short const kShift5       = 5;
        static constexpr short const kShift6       = 6;

        // utf8 maximum expansion: x4 (-> utf32)
        // utf16 maximum expansion: x2 (-> utf32)
        // utf32 maximum expansion: x1
        enum : std::uint16_t
        {
          bufcapacitybytes = 10240,
          bufcapacity      = bufcapacitybytes / (sizeof(char_t) + 4)
        };

        std::array<char_t, bufcapacity> buffer; // NOLINT(misc-non-private-member-variables-in-classes)

        union {
          std::array<std::uint8_t, static_cast<size_t>(4U * bufcapacity)> data_u8;
          std::array<std::uint16_t, static_cast<size_t>(2U * bufcapacity)> data_u16;
          std::array<std::uint32_t, bufcapacity> data_u32;
          std::array<char_t, bufcapacity> data_char;
        } scratch; // NOLINT(misc-non-private-member-variables-in-classes)

        IXmlWriter &writer;    // NOLINT(misc-non-private-member-variables-in-classes,
                               // cppcoreguidelines-avoid-const-or-ref-data-members)
        size_t bufsize{};      // NOLINT(misc-non-private-member-variables-in-classes)
        xml_encoding encoding; // NOLINT(misc-non-private-member-variables-in-classes)

        XmlBufferedWriter(XmlBufferedWriter const &)            = delete;
        XmlBufferedWriter &operator=(XmlBufferedWriter const &) = delete;

        XmlBufferedWriter(IXmlWriter &writer_, xml_encoding user_encoding);

        size_t flush();

        void flush(char_t const *data, size_t size);

        void write_direct(char_t const *data, size_t length);

        void write_buffer(char_t const *data, size_t length);

        void write_string(char_t const *data);

        void write(char_t d0_);

        void write(char_t d0_, char_t d1_); // NOLINT(bugprone-easily-swappable-parameters)

        void write(char_t d0_, char_t d1_, char_t d2_); // NOLINT(bugprone-easily-swappable-parameters)

        void write(char_t d0_, char_t d1_, char_t d2_, char_t d3_); // NOLINT(bugprone-easily-swappable-parameters)

        void write(char_t d0_, char_t d1_, // NOLINT(bugprone-easily-swappable-parameters)
                   char_t d2_, char_t d3_, char_t d4_);

        void write(char_t d0_, char_t d1_, char_t d2_, // NOLINT(bugprone-easily-swappable-parameters)
                   char_t d3_, char_t d4_, char_t d5_);
      };
    } // namespace Writer
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_WRITER_XML_BUFFERED_WRITER_HPP
