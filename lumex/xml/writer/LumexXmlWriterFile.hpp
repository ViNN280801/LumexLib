#ifndef LUMEXXML_WRITER_HPP
#define LUMEXXML_WRITER_HPP

#include "lumex/LumexExport.hpp"

#include "ILumexXmlWriter.hpp"

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Writer
    {
      class LUMEX_API LumexXmlWriterFile : public ILumexXmlWriter
      {
      public:
        LumexXmlWriterFile(void *file) : file(file) {}
        ~LumexXmlWriterFile() override                            = default;
        LumexXmlWriterFile(LumexXmlWriterFile const &)            = delete;
        LumexXmlWriterFile(LumexXmlWriterFile &&)                 = delete;
        LumexXmlWriterFile &operator=(LumexXmlWriterFile const &) = delete;
        LumexXmlWriterFile &operator=(LumexXmlWriterFile &&)      = delete;

        void write(void const *data, size_t size) override;

      private:
        void *file;
      };
    } // namespace Writer
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEXXML_WRITER_HPP
