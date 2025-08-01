#ifndef LUMEXXML_WRITER_HPP
#define LUMEXXML_WRITER_HPP

#include "IXmlWriter.hpp"

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Writer
    {
      class XmlWriterFile : public IXmlWriter
      {
      public:
        XmlWriterFile(void *file) : file(file) {}
        ~XmlWriterFile() override                       = default;
        XmlWriterFile(XmlWriterFile const &)            = delete;
        XmlWriterFile(XmlWriterFile &&)                 = delete;
        XmlWriterFile &operator=(XmlWriterFile const &) = delete;
        XmlWriterFile &operator=(XmlWriterFile &&)      = delete;

        void write(void const *data, size_t size) override;

      private:
        void *file;
      };
    } // namespace Writer
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEXXML_WRITER_HPP
