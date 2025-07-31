#include "LumexXmlXPathStack.hpp"

using namespace Lumex::Xml::XPath::Memory;

LumexXmlXPathStackData::LumexXmlXPathStackData()
    : result(blocks.data() + 0, std::addressof(oom)), temp(blocks.data() + 1, std::addressof(oom))
{
  blocks[0].next = blocks[1].next = nullptr;
  blocks[0].capacity = blocks[1].capacity = sizeof(blocks[0].data); // NOLINT(cppcoreguidelines-pro-type-union-access)

  stack.result                            = std::addressof(result);
  stack.temp                              = std::addressof(temp);
}

LumexXmlXPathStackData::~LumexXmlXPathStackData()
{
  result.release();
  temp.release();
}
