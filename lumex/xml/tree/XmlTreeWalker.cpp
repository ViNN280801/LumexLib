#define LUMEX_IMPLEMENTATION

#include "XmlTreeWalker.hpp"

using namespace Lumex::Xml::Tree;

LUMEX_PUBLIC_API
inline XmlTreeWalker::XmlTreeWalker() : m_depth(0) {}

LUMEX_PUBLIC_API
inline XmlTreeWalker::~XmlTreeWalker() {}

LUMEX_PUBLIC_API
inline int
XmlTreeWalker::depth() const
{
  return m_depth;
}

LUMEX_PUBLIC_API
inline bool
XmlTreeWalker::begin(XmlNode & /*node*/)
{
  return true;
}

LUMEX_PUBLIC_API
inline bool
XmlTreeWalker::end(XmlNode & /*node*/)
{
  return true;
}
