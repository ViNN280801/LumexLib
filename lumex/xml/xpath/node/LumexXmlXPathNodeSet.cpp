#define LUMEX_IMPLEMENTATION

#include "lumex/xml/utility/LumexXmlUtils.hpp"
#include "lumex/xml/xpath/utility/LumexXmlXPathUtils.hpp"

#include "LumexXmlXPathNodeSet.hpp"

using namespace Lumex::Xml::XPath::Node;
using namespace Lumex::Xml::XPath::Utility;
using namespace Lumex::Xml::Utility;

LUMEX_PUBLIC_API
inline void
LumexXmlXPathNodeSet::_assign(const_iterator begin_, const_iterator end_, type_t type_)
{
  LUMEX_ASSERT(begin_ <= end_);

  auto size_ = static_cast<size_t>(end_ - begin_);

  // use internal buffer for 0 or 1 elements, heap buffer otherwise
  LumexXmlXPathNode *storage = (size_ <= 1)
                                 ? m_storage.data()
                                 : static_cast<LumexXmlXPathNode *>(
                                     malloc( // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)
                                       size_ * sizeof(LumexXmlXPathNode)));

  if(storage == nullptr) throw std::bad_alloc();

  // deallocate old buffer
  if(m_begin != m_storage.data()) free(m_begin); // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)

  // size check is necessary because for begin_ = end_ = nullptr, memcpy is UB
  if(size_ != 0) memcpy(storage, begin_, size_ * sizeof(LumexXmlXPathNode));

  m_begin = storage;
  m_end   = storage + size_;
  m_type  = type_;
}

LUMEX_PUBLIC_API
inline void
LumexXmlXPathNodeSet::_move(LumexXmlXPathNodeSet &rhs) noexcept
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
inline LumexXmlXPathNodeSet::LumexXmlXPathNodeSet()
    : m_type(type_unsorted), m_begin(m_storage.data()), m_end(m_storage.data())
{}

LUMEX_PUBLIC_API
inline LumexXmlXPathNodeSet::LumexXmlXPathNodeSet(const_iterator begin_, const_iterator end_, type_t type_)
    : m_type(type_unsorted), m_begin(m_storage.data()), m_end(m_storage.data())
{
  _assign(begin_, end_, type_);
}

LUMEX_PUBLIC_API
inline LumexXmlXPathNodeSet::~LumexXmlXPathNodeSet()
{
  if(m_begin != m_storage.data()) free(m_begin); // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)
}

LUMEX_PUBLIC_API
inline LumexXmlXPathNodeSet::LumexXmlXPathNodeSet(LumexXmlXPathNodeSet const &rhs)
    : m_type(type_unsorted), m_begin(m_storage.data()), m_end(m_storage.data())
{
  _assign(rhs.m_begin, rhs.m_end, rhs.m_type);
}

LUMEX_PUBLIC_API
inline LumexXmlXPathNodeSet &
LumexXmlXPathNodeSet::operator=(LumexXmlXPathNodeSet const &rhs)
{
  if(this == &rhs) return *this;

  _assign(rhs.m_begin, rhs.m_end, rhs.m_type);

  return *this;
}

LUMEX_PUBLIC_API
inline LumexXmlXPathNodeSet::LumexXmlXPathNodeSet(LumexXmlXPathNodeSet &&rhs) noexcept
    : m_type(type_unsorted), m_begin(m_storage.data()), m_end(m_storage.data())
{
  _move(rhs);
}

LUMEX_PUBLIC_API
inline LumexXmlXPathNodeSet &
LumexXmlXPathNodeSet::operator=(LumexXmlXPathNodeSet &&rhs) noexcept
{
  if(this == &rhs) return *this;

  if(m_begin != m_storage.data()) free(m_begin); // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)

  _move(rhs);

  return *this;
}

LUMEX_PUBLIC_API
inline LumexXmlXPathNodeSet::type_t
LumexXmlXPathNodeSet::type() const
{
  return m_type;
}

LUMEX_PUBLIC_API
inline size_t
LumexXmlXPathNodeSet::size() const
{
  return m_end - m_begin;
}

LUMEX_PUBLIC_API
inline bool
LumexXmlXPathNodeSet::empty() const
{
  return m_begin == m_end;
}

LUMEX_PUBLIC_API
inline LumexXmlXPathNode const &
LumexXmlXPathNodeSet::operator[](size_t index) const
{
  LUMEX_ASSERT(index < size());
  return m_begin[index];
}

LUMEX_PUBLIC_API
inline LumexXmlXPathNodeSet::const_iterator
LumexXmlXPathNodeSet::begin() const
{
  return m_begin;
}

LUMEX_PUBLIC_API
inline LumexXmlXPathNodeSet::const_iterator
LumexXmlXPathNodeSet::end() const
{
  return m_end;
}

LUMEX_PUBLIC_API
inline void
LumexXmlXPathNodeSet::sort(bool reverse)
{
  m_type = Utility::xpath_sort(m_begin, m_end, m_type, reverse);
}

LUMEX_PUBLIC_API
inline LumexXmlXPathNode
LumexXmlXPathNodeSet::first() const
{
  return Utility::xpath_first(m_begin, m_end, m_type);
}

LumexXmlXPathNode *
LumexXmlXPathNodeSetRaw::begin() const
{
  return m_begin;
}

LumexXmlXPathNode *
LumexXmlXPathNodeSetRaw::end() const
{
  return m_end;
}

bool
LumexXmlXPathNodeSetRaw::empty() const
{
  return m_begin == m_end;
}

size_t
LumexXmlXPathNodeSetRaw::size() const
{
  return static_cast<size_t>(m_end - m_begin);
}

LumexXmlXPathNode
LumexXmlXPathNodeSetRaw::first() const
{
  return Xml::XPath::Utility::xpath_first(m_begin, m_end, m_type);
}

LUMEX_ATTRIBUTE_NOINLINE void
LumexXmlXPathNodeSetRaw::push_back_grow(LumexXmlXPathNode const &node, LumexXmlXPathAllocator *alloc)
{
  auto capacity = static_cast<size_t>(m_eos - m_begin);

  // get new capacity (1.5x rule)
  size_t new_capacity = capacity + (capacity / 2) + 1;

  // reallocate the old array or allocate a new one
  auto *data = static_cast<LumexXmlXPathNode *>(
    alloc->reallocate(m_begin, capacity * sizeof(LumexXmlXPathNode), new_capacity * sizeof(LumexXmlXPathNode)));
  if(data == nullptr) return;

  // finalize
  m_begin = data;
  m_end   = data + capacity;
  m_eos   = data + new_capacity;

  // push
  *m_end++ = node;
}

void
LumexXmlXPathNodeSetRaw::push_back(LumexXmlXPathNode const &node, LumexXmlXPathAllocator *alloc)
{
  if(m_end != m_eos)
    *m_end++ = node;
  else
    push_back_grow(node, alloc);
}

void
LumexXmlXPathNodeSetRaw::append(LumexXmlXPathNode const *begin_, LumexXmlXPathNode const *end_,
                                LumexXmlXPathAllocator *alloc)
{
  if(begin_ == end_) return;

  auto size_    = static_cast<size_t>(m_end - m_begin);
  auto capacity = static_cast<size_t>(m_eos - m_begin);
  auto count    = static_cast<size_t>(end_ - begin_);

  if(size_ + count > capacity)
  {
    // reallocate the old array or allocate a new one
    auto *data = static_cast<LumexXmlXPathNode *>(
      alloc->reallocate(m_begin, capacity * sizeof(LumexXmlXPathNode), (size_ + count) * sizeof(LumexXmlXPathNode)));
    if(data == nullptr) return;

    // finalize
    m_begin = data;
    m_end   = data + size_;
    m_eos   = data + size_ + count;
  }

  memcpy(m_end, begin_, count * sizeof(LumexXmlXPathNode));
  m_end += count;
}

void
LumexXmlXPathNodeSetRaw::sort_do()
{
  m_type = Xml::XPath::Utility::xpath_sort(m_begin, m_end, m_type, false);
}

void
LumexXmlXPathNodeSetRaw::truncate(LumexXmlXPathNode *pos)
{
  LUMEX_ASSERT(m_begin <= pos && pos <= m_end);

  m_end = pos;
}

void
LumexXmlXPathNodeSetRaw::remove_duplicates(LumexXmlXPathAllocator *alloc)
{
  if(m_type == LumexXmlXPathNodeSet::type_unsorted && m_end - m_begin > 2)
  {
    LumexXmlXPathAllocatorCapture capture(alloc);

    auto size_       = static_cast<size_t>(m_end - m_begin);
    size_t hash_size = 1;
    while(hash_size < size_ + size_ / 2) hash_size *= 2;

    void const **hash_data = static_cast<void const **>(alloc->allocate(hash_size * sizeof(void *)));
    if(hash_data == nullptr) return;

    memset(hash_data, 0, // NOLINT(bugprone-multi-level-implicit-pointer-conversion)
           hash_size * sizeof(void *));

    LumexXmlXPathNode *write = m_begin;

    for(LumexXmlXPathNode *it = m_begin; it != m_end; ++it)
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

LumexXmlXPathNodeSet::type_t
LumexXmlXPathNodeSetRaw::type() const
{
  return m_type;
}

void
LumexXmlXPathNodeSetRaw::set_type(LumexXmlXPathNodeSet::type_t value)
{
  m_type = value;
}
