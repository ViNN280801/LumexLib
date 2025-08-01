#include "lumex/xml/utility/XmlCleaner.hpp"
#include "lumex/xml/xpath/exception/XPathException.hpp"

#include "lumex/xml/xpath/parser/XPathParser.hpp"
#include "lumex/xml/xpath/variable/XPathVariableSet.hpp"

#include "XPathQuery.hpp"

using namespace Lumex::Xml::Utility;
using namespace Lumex::Xml::XPath;

using namespace Lumex::Xml::XPath::Exception;
using namespace Lumex::Xml::XPath::Parser;
using namespace Lumex::Xml::XPath::Variable;

namespace
{
  inline void
  unspecified_bool_xpath_query(XPathQuery *** /* unused */)
  {}

  inline XPathAstNode *
  evaluate_node_set_prepare(XPathQueryImpl *impl)
  {
    if(impl == nullptr) return nullptr;

    if(impl->root->rettype() != xpath_type_node_set)
    {
      XPathParseResult res;
      res.error = "Expression does not evaluate to node set";

      throw XPathException(res);
    }

    return impl->root;
  }
}

XPathQueryImpl *
XPathQueryImpl::create()
{
  void *memory = malloc( // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)
    sizeof(XPathQueryImpl));
  if(memory == nullptr) return nullptr;

  return new(memory) XPathQueryImpl(); // NOLINT(cppcoreguidelines-owning-memory)
}

void
XPathQueryImpl::destroy(XPathQueryImpl *impl)
{
  // free all allocated pages
  impl->alloc.release();

  // free allocator memory (with the first page)
  free(impl); // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)
}

XPathQueryImpl::XPathQueryImpl() : alloc(&block, &oom)
{
  block.next     = nullptr;
  block.capacity = sizeof(block.data); // NOLINT(cppcoreguidelines-pro-type-union-access)
}

inline XPathQuery::XPathQuery(char_t const *query, XPathVariableSet *variables) : m_impl(nullptr)
{
  XPathQueryImpl *qimpl = XPathQueryImpl::create();
  if(qimpl == nullptr) throw std::bad_alloc();

  XmlCleaner<XPathQueryImpl> impl(qimpl, XPathQueryImpl::destroy);

  qimpl->root = XPathParser::parse(query, variables, &qimpl->alloc, &m_result);
  if(qimpl->root != nullptr)
  {
    qimpl->root->optimize(&qimpl->alloc);

    m_impl         = impl.release();
    m_result.error = nullptr;
  }
  else
  {
    if(qimpl->oom) throw std::bad_alloc();
    throw XPathException(m_result);
  }
}

inline XPathQuery::XPathQuery() : m_impl(nullptr) {}

inline XPathQuery::~XPathQuery()
{
  if(m_impl != nullptr) XPathQueryImpl::destroy(static_cast<XPathQueryImpl *>(m_impl));
}

inline XPathQuery::XPathQuery(XPathQuery &&rhs) noexcept : m_impl(rhs.m_impl), m_result(rhs.m_result)
{
  rhs.m_impl   = nullptr;
  rhs.m_result = XPathParseResult();
}

inline XPathQuery &
XPathQuery::operator=(XPathQuery &&rhs) noexcept
{
  if(this == &rhs) return *this;

  if(m_impl != nullptr) XPathQueryImpl::destroy(static_cast<XPathQueryImpl *>(m_impl));

  m_impl       = rhs.m_impl;
  m_result     = rhs.m_result;
  rhs.m_impl   = nullptr;
  rhs.m_result = XPathParseResult();

  return *this;
}

inline xpath_value_type
XPathQuery::return_type() const
{
  if(m_impl == nullptr) return xpath_type_none;

  return static_cast<XPathQueryImpl *>(m_impl)->root->rettype();
}

inline bool
XPathQuery::evaluate_boolean(XPathNode const &n) const
{
  if(m_impl == nullptr) return false;

  XPathContext ctx(n, 1, 1);
  XPathStackData stack_data;

  bool tmp = static_cast<XPathQueryImpl *>(m_impl)->root->eval_boolean(ctx, stack_data.stack);

  if(stack_data.oom) throw std::bad_alloc();

  return tmp;
}

inline double
XPathQuery::evaluate_number(XPathNode const &n) const
{
  if(m_impl == nullptr) return gen_nan();

  XPathContext ctx(n, 1, 1);
  XPathStackData stack_data;

  double tmp = static_cast<XPathQueryImpl *>(m_impl)->root->eval_number(ctx, stack_data.stack);

  if(stack_data.oom) throw std::bad_alloc();

  return tmp;
}

inline string_t
XPathQuery::evaluate_string(XPathNode const &n) const
{
  if(m_impl == nullptr) return {};

  XPathContext ctx(n, 1, 1);
  XPathStackData stack_data;

  XPathString tmp = static_cast<XPathQueryImpl *>(m_impl)->root->eval_string(ctx, stack_data.stack);

  if(stack_data.oom) throw std::bad_alloc();

  return {tmp.c_str(), tmp.length()};
}

inline size_t
XPathQuery::evaluate_string(char_t *buffer, size_t capacity, XPathNode const &n) const
{
  XPathContext ctx(n, 1, 1);
  XPathStackData stack_data;

  XPathString tmp = (m_impl != nullptr)
                      ? static_cast<XPathQueryImpl *>(m_impl)->root->eval_string(ctx, stack_data.stack)
                      : XPathString();

  if(stack_data.oom) throw std::bad_alloc();

  size_t full_size = tmp.length() + 1;

  if(capacity > 0)
  {
    size_t size = (full_size < capacity) ? full_size : capacity;
    LUMEX_ASSERT(size > 0);

    std::memcpy(buffer, tmp.c_str(), (size - 1) * sizeof(char_t));
    buffer[size - 1] = 0;
  }

  return full_size;
}

inline XPathNodeSet
XPathQuery::evaluate_node_set(XPathNode const &n) const
{
  XPathAstNode *root = evaluate_node_set_prepare(static_cast<XPathQueryImpl *>(m_impl));
  if(root == nullptr) return {};

  XPathContext ctx(n, 1, 1);
  XPathStackData stack_data;

  XPathNodeSetRaw node_set_raw = root->eval_node_set(ctx, stack_data.stack, nodeset_eval_all);

  if(stack_data.oom) throw std::bad_alloc();

  return {node_set_raw.begin(), node_set_raw.end(), node_set_raw.type()};
}

inline XPathNode
XPathQuery::evaluate_node(XPathNode const &n) const
{
  XPathAstNode *root = evaluate_node_set_prepare(static_cast<XPathQueryImpl *>(m_impl));
  if(root == nullptr) return {};

  XPathContext ctx(n, 1, 1);
  XPathStackData stack_data;

  XPathNodeSetRaw node_set_raw = root->eval_node_set(ctx, stack_data.stack, nodeset_eval_first);

  if(stack_data.oom) throw std::bad_alloc();

  return node_set_raw.first();
}

inline XPathParseResult const &
XPathQuery::result() const
{
  return m_result;
}

inline XPathQuery::
operator XPathQuery::unspecified_bool_type() const
{
  return (m_impl != nullptr) ? unspecified_bool_xpath_query : nullptr;
}

inline bool
XPathQuery::operator!() const
{
  return m_impl == nullptr;
}
