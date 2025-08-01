#define LUMEX_IMPLEMENTATION

#include "lumex/xml/text/XmlParseResult.hpp"
#include "lumex/xml/utility/XmlCleaner.hpp"

#include "XPathQuery.hpp"

using namespace Lumex::Xml::Utility;
using namespace Lumex::Xml::XPath;
using namespace Lumex::Xml::Text;

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

XPathQueryImpl::XPathQueryImpl() : root(nullptr), alloc(&block, &oom), oom(false)
{
  block.next     = nullptr;
  block.capacity = sizeof(block.data);
}

LUMEX_PUBLIC_API
inline XPathQuery::XPathQuery(char_t const *query, XPathVariableSet *variables)
    : m_impl(nullptr)
{
  XPathQueryImpl *qimpl = XPathQueryImpl::create();

  if(!qimpl) { throw std::bad_alloc(); }
  else
  {
    XmlCleaner<XPathQueryImpl> impl(qimpl, XPathQueryImpl::destroy);

    qimpl->root = impl::xpath_parser::parse(query, variables, &qimpl->alloc, &m_result);

    if(qimpl->root)
    {
      qimpl->root->optimize(&qimpl->alloc);

      m_impl         = impl.release();
      m_result.error = nullptr;
    }
    else
    {
      if(qimpl->oom) throw std::bad_alloc();
      throw xpath_exception(m_result);
    }
  }
}

LUMEX_PUBLIC_API
inline XPathQuery::XPathQuery() : m_impl(nullptr) {}

LUMEX_PUBLIC_API
inline XPathQuery::~XPathQuery()
{
  if(m_impl) XPathQueryImpl::destroy(static_cast<XPathQueryImpl *>(m_impl));
}

LUMEX_PUBLIC_API
inline XPathQuery::XPathQuery(XPathQuery &&rhs) noexcept
{
  m_impl       = rhs.m_impl;
  m_result     = rhs.m_result;
  rhs.m_impl   = nullptr;
  rhs.m_result = XmlParseResult();
}

LUMEX_PUBLIC_API
inline XPathQuery &
XPathQuery::operator=(XPathQuery &&rhs) noexcept
{
  if(this == &rhs) return *this;

  if(m_impl) XPathQueryImpl::destroy(static_cast<XPathQueryImpl *>(m_impl));

  m_impl       = rhs.m_impl;
  m_result     = rhs.m_result;
  rhs.m_impl   = nullptr;
  rhs.m_result = XmlParseResult();

  return *this;
}

LUMEX_PUBLIC_API
inline xpath_value_type
XPathQuery::return_type() const
{
  if(!m_impl) return xpath_type_none;

  return static_cast<XPathQueryImpl *>(m_impl)->root->rettype();
}

LUMEX_PUBLIC_API
inline bool
XPathQuery::evaluate_boolean(XPathNode const &n) const
{
  if(!m_impl) return false;

  impl::xpath_context c(n, 1, 1);
  impl::xpath_stack_data sd;

  bool r = static_cast<XPathQueryImpl *>(m_impl)->root->eval_boolean(c, sd.stack);

  if(sd.oom) throw std::bad_alloc();

  return r;
}

LUMEX_PUBLIC_API
inline double
XPathQuery::evaluate_number(XPathNode const &n) const
{
  if(!m_impl) return impl::gen_nan();

  impl::xpath_context c(n, 1, 1);
  impl::xpath_stack_data sd;

  double r = static_cast<XPathQueryImpl *>(m_impl)->root->eval_number(c, sd.stack);

  if(sd.oom) throw std::bad_alloc();

  return r;
}

LUMEX_PUBLIC_API
inline string_t
XPathQuery::evaluate_string(XPathNode const &n) const
{
  if(!m_impl) return string_t();

  impl::xpath_context c(n, 1, 1);
  impl::xpath_stack_data sd;

  impl::xpath_string r = static_cast<XPathQueryImpl *>(m_impl)->root->eval_string(c, sd.stack);

  if(sd.oom) throw std::bad_alloc();

  return string_t(r.c_str(), r.length());
}

LUMEX_PUBLIC_API
inline size_t
XPathQuery::evaluate_string(char_t *buffer, size_t capacity, XPathNode const &n) const
{
  impl::xpath_context c(n, 1, 1);
  impl::xpath_stack_data sd;

  impl::xpath_string r
    = m_impl ? static_cast<XPathQueryImpl *>(m_impl)->root->eval_string(c, sd.stack) : impl::xpath_string();

  if(sd.oom) throw std::bad_alloc();

  size_t full_size = r.length() + 1;

  if(capacity > 0)
  {
    size_t size = (full_size < capacity) ? full_size : capacity;
    LUMEX_ASSERT(size > 0);

    memcpy(buffer, r.c_str(), (size - 1) * sizeof(char_t));
    buffer[size - 1] = 0;
  }

  return full_size;
}

LUMEX_PUBLIC_API
inline xpath_node_set
XPathQuery::evaluate_node_set(XPathNode const &n) const
{
  impl::xpath_ast_node *root = impl::evaluate_node_set_prepare(static_cast<XPathQueryImpl *>(m_impl));
  if(!root) return xpath_node_set();

  impl::xpath_context c(n, 1, 1);
  impl::xpath_stack_data sd;

  impl::xpath_node_set_raw r = root->eval_node_set(c, sd.stack, impl::nodeset_eval_all);

  if(sd.oom) throw std::bad_alloc();

  return xpath_node_set(r.begin(), r.end(), r.type());
}

LUMEX_PUBLIC_API
inline XPathNode
XPathQuery::evaluate_node(XPathNode const &n) const
{
  impl::xpath_ast_node *root = impl::evaluate_node_set_prepare(static_cast<XPathQueryImpl *>(m_impl));
  if(!root) return XPathNode();

  impl::xpath_context c(n, 1, 1);
  impl::xpath_stack_data sd;

  impl::xpath_node_set_raw r = root->eval_node_set(c, sd.stack, impl::nodeset_eval_first);

  if(sd.oom) throw std::bad_alloc();

  return r.first();
}

LUMEX_PUBLIC_API
inline XmlParseResult const &
XPathQuery::result() const
{
  return m_result;
}

LUMEX_PUBLIC_API
inline static void
unspecified_bool_xpath_query(XPathQuery ***)
{}

LUMEX_PUBLIC_API
inline XPathQuery::
operator XPathQuery::unspecified_bool_type() const
{
  return m_impl ? unspecified_bool_xpath_query : nullptr;
}

LUMEX_PUBLIC_API
inline bool
XPathQuery::operator!() const
{
  return !m_impl;
}
