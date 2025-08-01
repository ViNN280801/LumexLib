#include "XmlTreeWalker.hpp"

using namespace Lumex::Xml::Tree;

inline XmlTreeWalker::XmlTreeWalker() : m_depth(0) {}

inline XmlTreeWalker::~XmlTreeWalker() {}

inline int
XmlTreeWalker::depth() const
{
  return m_depth;
}

inline bool
XmlTreeWalker::begin(XmlNode & /*node*/)
{
  return true;
}

inline bool
XmlTreeWalker::end(XmlNode & /*node*/)
{
  return true;
}
