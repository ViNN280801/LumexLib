#define LUMEX_IMPLEMENTATION
#include "lumex/xml/memory/XmlMemoryPage.hpp"
#include "lumex/xml/node/XmlNodeBase.hpp"

#include "XmlDocumentBase.hpp"

using namespace Lumex::Xml::Document;
using namespace Lumex::Xml::Memory;
using namespace Lumex::Xml::Node;

LUMEX_PUBLIC_API
XmlDocumentBase::XmlDocumentBase(XmlMemoryPage *page) : XmlNodeBase(page, node_document), XmlAllocator(page) {}
