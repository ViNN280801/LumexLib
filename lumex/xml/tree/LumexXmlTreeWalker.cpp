#define LUMEX_IMPLEMENTATION

#include "LumexXmlTreeWalker.hpp"

using namespace Lumex::Xml::Tree;

LUMEX_PUBLIC_API
inline LumexXmlTreeWalker::LumexXmlTreeWalker() : m_depth(0) {}

LUMEX_PUBLIC_API
inline LumexXmlTreeWalker::~LumexXmlTreeWalker() {}

LUMEX_PUBLIC_API
inline int
LumexXmlTreeWalker::depth() const
{
  return m_depth;
}

LUMEX_PUBLIC_API
inline bool
LumexXmlTreeWalker::begin(LumexXmlNode & /*node*/)
{
  return true;
}

LUMEX_PUBLIC_API
inline bool
LumexXmlTreeWalker::end(LumexXmlNode & /*node*/)
{
  return true;
}
