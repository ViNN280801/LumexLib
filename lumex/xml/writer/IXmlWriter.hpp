#ifndef ILUMEX_XML_WRITER_HPP
#define ILUMEX_XML_WRITER_HPP

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Writer
    {
      class IXmlWriter // NOLINT(cppcoreguidelines-special-member-functions)
      {
      public:
        virtual ~IXmlWriter();

        virtual void write(void const *data, size_t size) = 0;
      };
    } // namespace Writer
  } // namespace Xml
} // namespace Lumex

#endif // !ILUMEX_XML_WRITER_HPP
