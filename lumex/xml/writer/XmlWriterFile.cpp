#define LUMEX_IMPLEMENTATION

#include <cstdio>

#include "lumex/core/utility/LumexUtility"

#include "XmlWriterFile.hpp"

using namespace Lumex::Xml::Writer;

LUMEX_PUBLIC_API
void
XmlWriterFile::write(void const *data, size_t size)
{
  std::fwrite(data, 1, size, static_cast<FILE *>(file));
}
