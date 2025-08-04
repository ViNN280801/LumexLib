#define LUMEX_IMPLEMENTATION

#include "lumex/core/utility/LumexAssert.hpp"

#include "XmlWriterStream.hpp"

using namespace Lumex::Xml::Writer;

LUMEX_PUBLIC_API
XmlWriterStream::XmlWriterStream(std::basic_ostream<char> &stream) : narrow_stream(&stream), wide_stream(nullptr) {}

LUMEX_PUBLIC_API
XmlWriterStream::XmlWriterStream(std::basic_ostream<wchar_t> &stream) : narrow_stream(nullptr), wide_stream(&stream) {}

LUMEX_PUBLIC_API
void
XmlWriterStream::write(void const *data, size_t size)
{
  if(narrow_stream != nullptr)
  {
    LUMEX_ASSERT(!wide_stream);
    narrow_stream->write(reinterpret_cast<char const *>(data), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                         static_cast<std::streamsize>(size));
  }
  else
  {
    LUMEX_ASSERT(wide_stream);
    LUMEX_ASSERT(size % sizeof(wchar_t) == 0);

    wide_stream->write(reinterpret_cast<wchar_t const *>(data), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                       static_cast<std::streamsize>(size / sizeof(wchar_t)));
  }
}
