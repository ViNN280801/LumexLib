#define LUMEX_IMPLEMENTATION

#include "lumex/xml/utility/XmlUtils.hpp"
#include "lumex/xml/xpath/utility/XPathUtils.hpp"

#include "XPathNodeSet.hpp"

using namespace Lumex::Xml::XPath::Node;
using namespace Lumex::Xml::XPath::Utility;
using namespace Lumex::Xml::Utility;

LUMEX_PUBLIC_API
inline void
XPathNodeSet::_assign(const_iterator begin_, const_iterator end_, type_t type_)
{
  LUMEX_ASSERT(begin_ <= end_);

  auto size_ = static_cast<size_t>(end_ - begin_);

  // use internal buffer for 0 or 1 elements, heap buffer otherwise
  XPathNode *storage = (size_ <= 1)
                                 ? m_storage.data()
                                 : static_cast<XPathNode *>(
                                     malloc( // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)
                                       size_ * sizeof(XPathNode)));

  if(storage == nullptr) throw std::bad_alloc();

  // deallocate old buffer
  if(m_begin != m_storage.data()) free(m_begin); // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)

  // size check is necessary because for begin_ = end_ = nullptr, memcpy is UB
  if(size_ != 0) memcpy(storage, begin_, size_ * sizeof(XPathNode));

  m_begin = storage;
  m_end   = storage + size_;
  m_type  = type_;
}

LUMEX_PUBLIC_API
inline void
XPathNodeSet::_move(XPathNodeSet &rhs) noexcept
{
  m_type            = rhs.m_type;
  m_storage.front() = rhs.m_storage.front();
  m_begin           = (rhs.m_begin == rhs.m_storage.data()) ? m_storage.data() : rhs.m_begin;
  m_end             = m_begin + (rhs.m_end - rhs.m_begin);

  rhs.m_type        = type_unsorted;
  rhs.m_begin       = rhs.m_storage.data();
  rhs.m_end         = rhs.m_storage.data();
}

LUMEX_PUBLIC_API
inline XPathNodeSet::XPathNodeSet()
    : m_type(type_unsorted), m_begin(m_storage.data()), m_end(m_storage.data())
{}

LUMEX_PUBLIC_API
inline XPathNodeSet::XPathNodeSet(const_iterator begin_, const_iterator end_, type_t type_)
    : m_type(type_unsorted), m_begin(m_storage.data()), m_end(m_storage.data())
{
  _assign(begin_, end_, type_);
}

LUMEX_PUBLIC_API
inline XPathNodeSet::~XPathNodeSet()
{
  if(m_begin != m_storage.data()) free(m_begin); // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)
}

LUMEX_PUBLIC_API
inline XPathNodeSet::XPathNodeSet(XPathNodeSet const &rhs)
    : m_type(type_unsorted), m_begin(m_storage.data()), m_end(m_storage.data())
{
  _assign(rhs.m_begin, rhs.m_end, rhs.m_type);
}

LUMEX_PUBLIC_API
inline XPathNodeSet &
XPathNodeSet::operator=(XPathNodeSet const &rhs)
{
  if(this == &rhs) return *this;

  _assign(rhs.m_begin, rhs.m_end, rhs.m_type);

  return *this;
}

LUMEX_PUBLIC_API
inline XPathNodeSet::XPathNodeSet(XPathNodeSet &&rhs) noexcept
    : m_type(type_unsorted), m_begin(m_storage.data()), m_end(m_storage.data())
{
  _move(rhs);
}

LUMEX_PUBLIC_API
inline XPathNodeSet &
XPathNodeSet::operator=(XPathNodeSet &&rhs) noexcept
{
  if(this == &rhs) return *this;

  if(m_begin != m_storage.data()) free(m_begin); // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)

  _move(rhs);

  return *this;
}

LUMEX_PUBLIC_API
inline XPathNodeSet::type_t
XPathNodeSet::type() const
{
  return m_type;
}

LUMEX_PUBLIC_API
inline size_t
XPathNodeSet::size() const
{
  return m_end - m_begin;
}

LUMEX_PUBLIC_API
inline bool
XPathNodeSet::empty() const
{
  return m_begin == m_end;
}

LUMEX_PUBLIC_API
inline XPathNode const &
XPathNodeSet::operator[](size_t index) const
{
  LUMEX_ASSERT(index < size());
  return m_begin[index];
}

LUMEX_PUBLIC_API
inline XPathNodeSet::const_iterator
XPathNodeSet::begin() const
{
  return m_begin;
}

LUMEX_PUBLIC_API
inline XPathNodeSet::const_iterator
XPathNodeSet::end() const
{
  return m_end;
}

LUMEX_PUBLIC_API
inline void
XPathNodeSet::sort(bool reverse)
{
  m_type = Utility::xpath_sort(m_begin, m_end, m_type, reverse);
}

LUMEX_PUBLIC_API
inline XPathNode
XPathNodeSet::first() const
{
  return Utility::xpath_first(m_begin, m_end, m_type);
}

XPathNode *
XPathNodeSetRaw::begin() const
{
  return m_begin;
}

XPathNode *
XPathNodeSetRaw::end() const
{
  return m_end;
}

bool
XPathNodeSetRaw::empty() const
{
  return m_begin == m_end;
}

size_t
XPathNodeSetRaw::size() const
{
  return static_cast<size_t>(m_end - m_begin);
}

XPathNode
XPathNodeSetRaw::first() const
{
  return Xml::XPath::Utility::xpath_first(m_begin, m_end, m_type);
}

LUMEX_ATTRIBUTE_NOINLINE void
XPathNodeSetRaw::push_back_grow(XPathNode const &node, XPathAllocator *alloc)
{
  auto capacity = static_cast<size_t>(m_eos - m_begin);

  // get new capacity (1.5x rule)
  size_t new_capacity = capacity + (capacity / 2) + 1;

  // reallocate the old array or allocate a new one
  auto *data = static_cast<XPathNode *>(
    alloc->reallocate(m_begin, capacity * sizeof(XPathNode), new_capacity * sizeof(XPathNode)));
  if(data == nullptr) return;

  // finalize
  m_begin = data;
  m_end   = data + capacity;
  m_eos   = data + new_capacity;

  // push
  *m_end++ = node;
}

void
XPathNodeSetRaw::push_back(XPathNode const &node, XPathAllocator *alloc)
{
  if(m_end != m_eos)
    *m_end++ = node;
  else
    push_back_grow(node, alloc);
}

void
XPathNodeSetRaw::append(XPathNode const *begin_, XPathNode const *end_,
                                XPathAllocator *alloc)
{
  if(begin_ == end_) return;

  auto size_    = static_cast<size_t>(m_end - m_begin);
  auto capacity = static_cast<size_t>(m_eos - m_begin);
  auto count    = static_cast<size_t>(end_ - begin_);

  if(size_ + count > capacity)
  {
    // reallocate the old array or allocate a new one
    auto *data = static_cast<XPathNode *>(
      alloc->reallocate(m_begin, capacity * sizeof(XPathNode), (size_ + count) * sizeof(XPathNode)));
    if(data == nullptr) return;

    // finalize
    m_begin = data;
    m_end   = data + size_;
    m_eos   = data + size_ + count;
  }

  memcpy(m_end, begin_, count * sizeof(XPathNode));
  m_end += count;
}

void
XPathNodeSetRaw::sort_do()
{
  m_type = Xml::XPath::Utility::xpath_sort(m_begin, m_end, m_type, false);
}

void
XPathNodeSetRaw::truncate(XPathNode *pos)
{
  LUMEX_ASSERT(m_begin <= pos && pos <= m_end);

  m_end = pos;
}

void
XPathNodeSetRaw::remove_duplicates(XPathAllocator *alloc)
{
  if(m_type == XPathNodeSet::type_unsorted && m_end - m_begin > 2)
  {
    XPathAllocatorCapture capture(alloc);

    auto size_       = static_cast<size_t>(m_end - m_begin);
    size_t hash_size = 1;
    while(hash_size < size_ + size_ / 2) hash_size *= 2;

    void const **hash_data = static_cast<void const **>(alloc->allocate(hash_size * sizeof(void *)));
    if(hash_data == nullptr) return;

    memset(hash_data, 0, // NOLINT(bugprone-multi-level-implicit-pointer-conversion)
           hash_size * sizeof(void *));

    XPathNode *write = m_begin;

    for(XPathNode *it = m_begin; it != m_end; ++it)
    {
      void const *attr = it->attribute().get();
      void const *node = it->node().get();
      void const *key  = attr != nullptr ? attr : node;

      if(key != nullptr && Xml::Utility::hash_insert(hash_data, hash_size, key)) *write++ = *it;
    }

    m_end = write;
  }
  else { m_end = std::unique(m_begin, m_end); }
}

XPathNodeSet::type_t
XPathNodeSetRaw::type() const
{
  return m_type;
}

void
XPathNodeSetRaw::set_type(XPathNodeSet::type_t value)
{
  m_type = value;
}
