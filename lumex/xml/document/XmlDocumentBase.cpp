#define LUMEX_IMPLEMENTATION
#include "lumex/xml/memory/XmlMemoryPage.hpp"
#include "lumex/xml/node/XmlNodeBase.hpp"

#include "XmlDocumentBase.hpp"

using namespace lumex::xml::document;
using namespace lumex::xml::memory;
using namespace lumex::xml::node;
using namespace lumex::xml::types;
using namespace lumex::xml::types::Types;

LUMEX_PUBLIC_API
XmlDocumentBase::XmlDocumentBase (XmlMemoryPage *page)
    : XmlNodeBase (page, node_document), XmlAllocator (page)
{
}
