#ifndef LUMEXXML_WRITER_STREAM_HPP
#define LUMEXXML_WRITER_STREAM_HPP

#include <iostream>

#include "IXmlWriter.hpp"

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Writer
    {
      class XmlWriterStream : public IXmlWriter
      {
      public:
        XmlWriterStream(std::basic_ostream<char> &stream);
        XmlWriterStream(std::basic_ostream<wchar_t> &stream);

        void write(void const *data, size_t size) override;

      private:
        std::basic_ostream<char> *narrow_stream;
        std::basic_ostream<wchar_t> *wide_stream;
      };
    } // namespace Writer
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEXXML_WRITER_STREAM_HPP
