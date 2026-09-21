#define LUMEX_IMPLEMENTATION

#include <cstdio>

#include "lumex/core/utility/LumexUtility"

#include "XmlWriterFile.hpp"

using namespace lumex::xml::writer;

LUMEX_PUBLIC_API
void
XmlWriterFile::write (void const *data, std::size_t size)
{
  std::fwrite (data, 1, size, static_cast<FILE *> (file));
}
