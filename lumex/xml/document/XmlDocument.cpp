#include <iostream>

#include "lumex/xml/node/XmlNode.hpp"
#include "lumex/xml/utility/XmlCleaner.hpp"
#include "lumex/xml/utility/XmlUtils.hpp"
#include "lumex/xml/writer/XmlWriterFile.hpp"
#include "lumex/xml/writer/XmlWriterStream.hpp"

#include "XmlDocument.hpp"
#include "XmlDocumentBase.hpp"

using namespace Lumex::Xml::Utility;
using namespace Lumex::Xml::Document;
using namespace Lumex::Xml::Writer;
using namespace Lumex::Xml::Node;

namespace
{
  template <typename T>
  inline xml_parse_status
  load_stream_data_seek(std::basic_istream<T> &stream, void **out_buffer, size_t *out_size)
  {
    // get length of remaining data in stream
    typename std::basic_istream<T>::pos_type pos = stream.tellg();
    stream.seekg(0, std::ios::end);
    std::streamoff length = stream.tellg() - pos;
    stream.seekg(pos);

    if(stream.fail() || pos < 0) return status_io_error;

    // guard against huge files
    auto read_length = static_cast<size_t>(length);

    if(static_cast<std::streamsize>(read_length) != length || length < 0) return status_out_of_memory;

    size_t max_suffix_size = sizeof(char_t);

    // read stream data into memory (guard against stream exceptions with buffer holder)
    Utility::XmlCleaner<void> buffer(malloc( // NOLINT(cppcoreguidelines-no-malloc)
                                       (read_length * sizeof(T)) + max_suffix_size),
                                     free);
    if(!buffer.data) return status_out_of_memory;

    stream.read(static_cast<T *>(buffer.data), static_cast<std::streamsize>(read_length));

    // read may set failbit | eofbit in case gcount() is less than read_length (i.e. line ending conversion), so check
    // for other I/O errors
    if(stream.bad() || (!stream.eof() && stream.fail())) return status_io_error;

    // return buffer
    size_t actual_length = static_cast<size_t>(stream.gcount());
    LUMEX_ASSERT(actual_length <= read_length);

    *out_buffer = buffer.release();
    *out_size   = actual_length * sizeof(T);

    return status_ok;
  }
}

template <typename T> struct xml_stream_chunk {
  static xml_stream_chunk *
  create()
  {
    void *memory                          // NOLINT(cppcoreguidelines-owning-memory)
      = malloc(sizeof(xml_stream_chunk)); // NOLINT(cppcoreguidelines-no-malloc)
    if(memory == nullptr) return nullptr;

    return new(memory) xml_stream_chunk(); // NOLINT(cppcoreguidelines-owning-memory)
  }

  static void
  destroy(xml_stream_chunk *chunk)
  {
    // free chunk chain
    while(chunk)
    {
      xml_stream_chunk *next_ = chunk->next;

      free(chunk); // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)

      chunk = next_;
    }
  }

  xml_stream_chunk() : next(nullptr) {}

  xml_stream_chunk *next; // NOLINT(misc-non-private-member-variables-in-classes)
  size_t size{};          // NOLINT(misc-non-private-member-variables-in-classes)
  T data[                 // NOLINT(misc-non-private-member-variables-in-classes, cppcoreguidelines-avoid-c-arrays,
                          // modernize-avoid-c-arrays)
    Memory::kdefault_xml_memory_page_size / sizeof(T)];
};

namespace
{
  template <typename T>
  inline xml_parse_status
  load_stream_data_noseek(std::basic_istream<T> &stream, void **out_buffer, size_t *out_size)
  {
    XmlCleaner<xml_stream_chunk<T>> chunks(nullptr, xml_stream_chunk<T>::destroy);

    // read file to a chunk list
    size_t total              = 0;
    xml_stream_chunk<T> *last = nullptr;

    while(!stream.eof())
    {
      // allocate new chunk
      xml_stream_chunk<T> *chunk = xml_stream_chunk<T>::create();
      if(!chunk) return status_out_of_memory;

      // append chunk to list
      if(last)
        last = last->next = chunk;
      else
        chunks.data = last = chunk;

      // read data to chunk
      stream.read(chunk->data, // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
                  static_cast<std::streamsize>(sizeof(chunk->data) / sizeof(T)));
      chunk->size = static_cast<size_t>(stream.gcount()) * sizeof(T);

      // read may set failbit | eofbit in case gcount() is less than read length, so check for other I/O errors
      if(stream.bad() || (!stream.eof() && stream.fail())) return status_io_error;

      // guard against huge files (chunk size is small enough to make this overflow check work)
      if(total + chunk->size < total) return status_out_of_memory;
      total += chunk->size;
    }

    size_t max_suffix_size = sizeof(char_t);

    // copy chunk list to a contiguous buffer
    char *buffer = static_cast<char *>(malloc( // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc )
      total + max_suffix_size));
    if(!buffer) return status_out_of_memory;

    char *write = buffer;

    for(xml_stream_chunk<T> *chunk = chunks.data; chunk; chunk = chunk->next)
    {
      LUMEX_ASSERT(write + chunk->size <= buffer + total);
      memcpy(write, chunk->data, chunk->size); // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
      write += chunk->size;
    }

    LUMEX_ASSERT(write == buffer + total);

    // return buffer
    *out_buffer = buffer;
    *out_size   = total;

    return status_ok;
  }

  inline size_t
  zero_terminate_buffer(void *buffer, size_t size, xml_encoding encoding)
  {
#ifdef LUMEX_XML_WCHAR_MODE
    xml_encoding wchar_encoding = get_wchar_encoding();

    if(encoding == wchar_encoding || Utility::need_endian_swap_utf(encoding, wchar_encoding))
    {
      size_t length                         = size / sizeof(char_t);

      static_cast<char_t *>(buffer)[length] = 0;
      return (length + 1) * sizeof(char_t);
    }
#else
    if(encoding == encoding_utf8)
    {
      static_cast<char *>(buffer)[size] = 0;
      return size + 1;
    }
#endif

    return size;
  }

  template <typename T>
  inline XmlParseResult
  load_stream_impl(XmlDocumentBase *doc, std::basic_istream<T> &stream, unsigned int options, xml_encoding encoding,
                   char_t **out_buffer)
  {
    void *buffer            = nullptr;
    size_t size             = 0;
    xml_parse_status status = status_ok;

    // if stream has an error bit set, bail out (otherwise tellg() can fail and we'll clear error bits)
    if(stream.fail()) return make_parse_result(status_io_error);

    // load stream to memory (using seek-based implementation if possible, since it's faster and takes less memory)
    if(stream.tellg() < 0)
    {
      stream.clear(); // clear error flags that could be set by a failing tellg
      status = load_stream_data_noseek(stream, &buffer, &size);
    }
    else
      status = load_stream_data_seek(stream, &buffer, &size);

    if(status != status_ok) return make_parse_result(status);

    xml_encoding real_encoding = get_buffer_encoding(encoding, buffer, size);

    return load_buffer_impl(doc, doc, buffer, zero_terminate_buffer(buffer, size, real_encoding), options,
                            real_encoding, true, true, out_buffer);
  }
}

inline XmlDocument::XmlDocument() { _create(); }

inline XmlDocument::~XmlDocument() { _destroy(); }

inline XmlDocument::XmlDocument(XmlDocument &&rhs) noexcept
{
  _create();
  _move(rhs);
}

inline XmlDocument &
XmlDocument::operator=(XmlDocument &&rhs) noexcept
{
  if(this == &rhs) return *this;

  _destroy();
  _create();
  _move(rhs);

  return *this;
}

inline void
XmlDocument::reset()
{
  _destroy();
  _create();
}

inline void
XmlDocument::reset(XmlDocument const &proto)
{
  reset();
  node_copy_tree(m_root, proto.m_root);
}

inline void
XmlDocument::_create()
{
  LUMEX_ASSERT(!m_root);

  size_t const page_offset = 0;

  // initialize sentinel page
  static_assert(sizeof(XmlMemoryPage) + sizeof(XmlDocumentBase) + page_offset <= sizeof(m_memory));

  // prepare page structure
  XmlMemoryPage *page = XmlMemoryPage::construct(m_memory.data());
  LUMEX_ASSERT(page);

  page->busy_size = Memory::kdefault_xml_memory_page_size;

  // allocate new root
  m_root                 = new( // NOLINT(cppcoreguidelines-owning-memory)
    reinterpret_cast<char *>(page) + sizeof(XmlMemoryPage) + page_offset) XmlDocumentBase(page);
  m_root->prev_sibling_c = m_root;

  // setup sentinel page
  page->allocator = static_cast<XmlDocumentBase *>(m_root); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

  // verify the document allocation
  LUMEX_ASSERT(reinterpret_cast<char *>(m_root) + sizeof(XmlDocumentBase) <= m_memory.data() + sizeof(m_memory));
}

inline void
XmlDocument::_destroy()
{
  LUMEX_ASSERT(m_root);

  // destroy static storage
  if(m_buffer != nullptr)
  {
    free(m_buffer); // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)
    m_buffer = nullptr;
  }

  // destroy extra buffers (note: no need to destroy linked list nodes, they're allocated using document allocator)
  for(xml_extra_buffer *extra = static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                                  XmlDocumentBase *>(m_root)
                                  ->extra_buffers;
      extra != nullptr; extra = extra->next)
    if(extra->buffer != nullptr)
      free(extra->buffer); // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)

  // destroy dynamic storage, leave sentinel page (it's in static memory)
  XmlMemoryPage *root_page = LUMEX_XML_GETPAGE(m_root); // NOLINT(cppcoreguidelines-pro-type-const-cast)
  LUMEX_ASSERT(root_page && !root_page->prev);
  LUMEX_ASSERT(reinterpret_cast<char *>(root_page) >= m_memory.data()
               && reinterpret_cast<char *>(root_page) < m_memory.data() + sizeof(m_memory));

  for(XmlMemoryPage *page = root_page->next; page != nullptr;)
  {
    XmlMemoryPage *next = page->next;
    XmlAllocator::deallocate_page(page);

    page = next;
  }

  m_root = nullptr;
}

inline void
XmlDocument::_move(XmlDocument &rhs) noexcept
{
  auto *doc   = static_cast<XmlDocumentBase *>(m_root);     // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
  auto *other = static_cast<XmlDocumentBase *>(rhs.m_root); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

  // save first child pointer for later; this needs hash access
  XmlNodeBase *other_first_child = other->first_child;

  // move allocation state
  // note that other->m_root may point to the embedded document page, in which case we should keep original (empty)
  // state
  if(other->m_root != LUMEX_XML_GETPAGE(other)) // NOLINT(cppcoreguidelines-pro-type-const-cast)
  {
    doc->m_root      = other->m_root;
    doc->m_busy_size = other->m_busy_size;
  }

  // move buffer state
  doc->buffer        = other->buffer;
  doc->extra_buffers = other->extra_buffers;
  m_buffer           = rhs.m_buffer;

  // move page structure
  XmlMemoryPage *doc_page = LUMEX_XML_GETPAGE(doc); // NOLINT(cppcoreguidelines-pro-type-const-cast)
  LUMEX_ASSERT(doc_page && !doc_page->prev && !doc_page->next);

  XmlMemoryPage *other_page = LUMEX_XML_GETPAGE(other); // NOLINT(cppcoreguidelines-pro-type-const-cast)
  LUMEX_ASSERT(other_page && !other_page->prev);

  // relink pages since root page is embedded into XmlDocument
  if(XmlMemoryPage *page = other_page->next)
  {
    LUMEX_ASSERT(page->prev == other_page);

    page->prev       = doc_page;

    doc_page->next   = page;
    other_page->next = nullptr;
  }

  // make sure pages point to the correct document state
  for(XmlMemoryPage *page = doc_page->next; page != nullptr; page = page->next)
  {
    LUMEX_ASSERT(page->allocator == other);

    page->allocator = doc;
  }

  // move tree structure
  LUMEX_ASSERT(!doc->first_child);

  doc->first_child = other_first_child;

  for(XmlNodeBase *node = other_first_child; node != nullptr; node = node->next_sibling)
  {
    LUMEX_ASSERT(node->parent == other);
    node->parent = doc;
  }

  // reset other document
  new(other) XmlDocumentBase(LUMEX_XML_GETPAGE(other)); // NOLINT(cppcoreguidelines-pro-type-const-cast)
  rhs.m_buffer = nullptr;
}

inline XmlParseResult
XmlDocument::load(std::basic_istream<char> &stream, unsigned int options, xml_encoding encoding)
{
  reset();

  return load_stream_impl(static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                            XmlDocumentBase *>(m_root),
                          stream, options, encoding, &m_buffer);
}

inline XmlParseResult
XmlDocument::load(std::basic_istream<wchar_t> &stream, unsigned int options)
{
  reset();

  return load_stream_impl(static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                            XmlDocumentBase *>(m_root),
                          stream, options, encoding_wchar, &m_buffer);
}

inline XmlParseResult
XmlDocument::load_string(char_t const *contents, unsigned int options)
{
  // Force native encoding (skip autodetection)
#ifdef LUMEX_XML_WCHAR_MODE
  xml_encoding encoding = encoding_wchar;
#else
  xml_encoding encoding = encoding_utf8;
#endif

  return loadm_buffer(contents, Utility::strlength(contents) * sizeof(char_t), options, encoding);
}

inline XmlParseResult
XmlDocument::load(char_t const *contents, unsigned int options)
{
  return load_string(contents, options);
}

namespace
{
  template <typename T>
  inline xml_parse_status
  convert_file_size(T length, size_t &out_result)
  {
    // check for I/O errors
    if(length < 0) return status_io_error;

    // check for overflow
    auto result = static_cast<size_t>(length);

    if(static_cast<T>(result) != length) return status_out_of_memory;

    out_result = result;
    return status_ok;
  }

  inline xml_parse_status
  get_file_size(FILE *file, size_t &out_result)
  {
#if defined(__linux__) || defined(__APPLE__)
    // this simultaneously retrieves the file size and file mode (to guard against loading non-files)
    struct stat st;
    if(fstat(fileno(file), &st) != 0) return status_io_error;

    // anything that's not a regular file doesn't have a coherent length
    if(!S_ISREG(st.st_mode)) return status_io_error;

    xml_parse_status status = convert_file_size(st.st_size, out_result);
#elif defined(LUMEX_XML_MSVC_CRT_VERSION) && LUMEX_XML_MSVC_CRT_VERSION >= 1400
    // there are 64-bit versions of fseek/ftell, let's use them
    _fseeki64(file, 0, SEEK_END);
    __int64 length = _ftelli64(file);
    _fseeki64(file, 0, SEEK_SET);

    xml_parse_status status = convert_file_size(length, out_result);
#elif defined(__MINGW32__) && !defined(__NO_MINGW_LFS)                                                                 \
  && (!defined(__STRICT_ANSI__) || defined(__MINGW64_VERSION_MAJOR))
    // there are 64-bit versions of fseek/ftell, let's use them
    fseeko64(file, 0, SEEK_END);
    off64_t length = ftello64(file);
    fseeko64(file, 0, SEEK_SET);

    xml_parse_status status = convert_file_size(length, out_result);
#else
    // if this is a 32-bit OS, long is enough; if this is a unix system, long is 64-bit, which is enough; otherwise we
    // can't do anything anyway.
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    xml_parse_status status = convert_file_size(length, out_result);
#endif

    return status;
  }

  inline XmlParseResult
  load_file_impl(XmlDocumentBase *doc, FILE *file, unsigned int options, xml_encoding encoding, char_t **out_buffer)
  {
    if(file == nullptr) return make_parse_result(status_file_not_found);

    // get file size (can result in I/O errors)
    size_t size                  = 0;
    xml_parse_status size_status = get_file_size(file, size);
    if(size_status != status_ok) return make_parse_result(size_status);

    size_t max_suffix_size = sizeof(char_t);

    // allocate buffer for the whole file
    char *contents = static_cast<char *>(malloc( // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)
      size + max_suffix_size));
    if(contents == nullptr) return make_parse_result(status_out_of_memory);

    // read file in memory
    size_t read_size = fread(contents, 1, size, file);

    if(read_size != size)
    {
      free(contents); // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)
      return make_parse_result(status_io_error);
    }

    xml_encoding real_encoding = get_buffer_encoding(encoding, contents, size);

    return load_buffer_impl(doc, doc, contents, zero_terminate_buffer(contents, size, real_encoding), options,
                            real_encoding, true, true, out_buffer);
  }

  inline void
  close_file(FILE *file)
  {
    fclose(file); // NOLINT(cppcoreguidelines-owning-memory)
  }

#if (defined(__MINGW32__) && (!defined(__STRICT_ANSI__) || defined(__MINGW64_VERSION_MAJOR)))
  inline FILE *
  open_file_wide(const wchar_t *path, const wchar_t *mode)
  {
  #if defined(MSVC) && MSVC >= 1400
    FILE *file = nullptr;
    return _wfopen_s(&file, path, mode) == 0 ? file : nullptr;
  #else
    return _wfopen(path, mode);
  #endif
  }
#else
  inline char *
  convert_path_heap(const wchar_t *str)
  {
    LUMEX_ASSERT(str);

    // first pass: get length in utf8 characters
    size_t length = strlength_wide(str);
    size_t size   = as_utf8_begin(str, length);

    // allocate resulting string
    char *result                               // NOLINT(cppcoreguidelines-owning-memory)
      = static_cast<char *>(malloc(size + 1)); // NOLINT(cppcoreguidelines-no-malloc)
    if(result == nullptr) return nullptr;

    // second pass: convert to utf8
    as_utf8_end(result, size, str, length);

    // zero-terminate
    result[size] = 0;

    return result;
  }

  inline FILE *
  open_file_wide(wchar_t const *path, wchar_t const *mode) // NOLINT(bugprone-easily-swappable-parameters)
  {
    // there is no standard function to open wide paths, so our best bet is to try utf8 path
    std::unique_ptr<char, decltype(&std::free)> path_utf8(convert_path_heap(path), &std::free);
    if(!path_utf8) return nullptr;

    // convert mode to ASCII (we mirror _wfopen interface)
    std::array<char, 4> mode_ascii = {0};
    for(size_t i = 0; mode[i] != 0; ++i) mode_ascii.at(i) = static_cast<char>(mode[i]);

    FILE *result = nullptr;

  #ifdef _WIN32
    // Use fopen_s on Windows for secure file opening
    if(fopen_s(&result, path_utf8.get(), mode_ascii.data()) != 0) result = nullptr; // Ensure result is null on error
  #else
    // Use standard fopen on other platforms
    result = std::fopen(path_utf8.get(), mode_ascii);
  #endif

    return result;
  }
#endif

  inline FILE *
  open_file(const char *path, const char *mode)
  {
#if defined(LUMEX_XML_MSVC_CRT_VERSION) && LUMEX_XML_MSVC_CRT_VERSION >= 1400
    FILE *file = nullptr;
    return fopen_s(&file, path, mode) == 0 ? file : nullptr;
#else
    return fopen(path, mode);
#endif
  }

  inline bool
  save_file_impl(XmlDocument const &doc, FILE *file, char_t const *indent, unsigned int flags, xml_encoding encoding)
  {
    if(file == nullptr) return false;

    XmlWriterFile writer(file);
    doc.save(writer, indent, flags, encoding);

    return fflush(file) == 0 && ferror(file) == 0;
  }
}

inline XmlParseResult
XmlDocument::load_file(char const *path_, unsigned int options, xml_encoding encoding)
{
  reset();

  XmlCleaner<FILE> file(open_file(path_, "rb"), close_file);

  return load_file_impl(static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                          XmlDocumentBase *>(m_root),
                        file.data, options, encoding, &m_buffer);
}

inline XmlParseResult
XmlDocument::load_file(wchar_t const *path_, unsigned int options, xml_encoding encoding)
{
  reset();

  XmlCleaner<FILE> file(open_file_wide(path_, L"rb"), close_file);

  return load_file_impl(static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                          XmlDocumentBase *>(m_root),
                        file.data, options, encoding, &m_buffer);
}

inline XmlParseResult
XmlDocument::loadm_buffer(void const *contents, size_t size, unsigned int options, xml_encoding encoding)
{
  reset();

  return load_buffer_impl(static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                            XmlDocumentBase *>(m_root),
                          m_root,
                          const_cast< // NOLINT(cppcoreguidelines-pro-type-const-cast)
                            void *>(contents),
                          size, options, encoding, false, false, &m_buffer);
}

inline XmlParseResult
XmlDocument::loadm_buffer_inplace(void *contents, size_t size, unsigned int options, xml_encoding encoding)
{
  reset();

  return load_buffer_impl(static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                            XmlDocumentBase *>(m_root),
                          m_root, contents, size, options, encoding, true, false, &m_buffer);
}

inline XmlParseResult
XmlDocument::loadm_buffer_inplace_own(void *contents, size_t size, unsigned int options, xml_encoding encoding)
{
  reset();

  return load_buffer_impl(static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                            XmlDocumentBase *>(m_root),
                          m_root, contents, size, options, encoding, true, true, &m_buffer);
}

namespace
{
  inline bool
  has_declaration(XmlNodeBase *node)
  {
    for(XmlNodeBase *child = node->first_child; child != nullptr; child = child->next_sibling)
    {
      auto type = LUMEX_XML_NODETYPE(child);
      if(type == node_declaration) return true;
      if(type == node_element) return false;
    }

    return false;
  }
}

inline void
XmlDocument::save(IXmlWriter &writer, char_t const *indent, unsigned int flags, xml_encoding encoding) const
{
  XmlBufferedWriter buffered_writer(writer, encoding);

  if(((flags & Constants::kformat_write_bom) != 0) && buffered_writer.encoding != encoding_latin1)
  {
    // BOM always represents the codepoint U+FEFF, so just write it in native encoding
#ifdef LUMEX_XML_WCHAR_MODE
    unsigned int bom = 0xfeff;
    buffered_writer.write(static_cast<wchar_t>(bom));
#else
    buffered_writer.write('\xef', '\xbb', '\xbf');
#endif
  }

  if(((flags & Constants::kformat_no_declaration) == 0) && !has_declaration(m_root))
  {
    buffered_writer.write_string(LUMEX_XML_TEXT("<?xml version=\"1.0\""));
    if(buffered_writer.encoding == encoding_latin1)
      buffered_writer.write_string(LUMEX_XML_TEXT(" encoding=\"ISO-8859-1\""));
    buffered_writer.write('?', '>');
    if((flags & Constants::kformat_raw) == 0) buffered_writer.write('\n');
  }

  node_output(buffered_writer, m_root, indent, flags, 0);

  buffered_writer.flush();
}

inline void
XmlDocument::save(std::basic_ostream<char> &stream, char_t const *indent, unsigned int flags,
                  xml_encoding encoding) const
{
  XmlWriterStream writer(stream);

  save(writer, indent, flags, encoding);
}

inline void
XmlDocument::save(std::basic_ostream<wchar_t> &stream, char_t const *indent, unsigned int flags) const
{
  XmlWriterStream writer(stream);

  save(writer, indent, flags, encoding_wchar);
}

inline bool
XmlDocument::save_file(char const *path_, char_t const *indent, // NOLINT(bugprone-easily-swappable-parameters)
                       unsigned int flags, xml_encoding encoding) const
{
  XmlCleaner<FILE> file(open_file(path_, ((flags & Constants::kformat_save_file_text) != 0) ? "w" : "wb"), close_file);
  return save_file_impl(*this, file.data, indent, flags, encoding)
         && fclose(file.release()) == 0; // NOLINT(cppcoreguidelines-owning-memory)
}

inline bool
XmlDocument::save_file(wchar_t const *path_, char_t const *indent, unsigned int flags, xml_encoding encoding) const
{
  XmlCleaner<FILE> file(open_file_wide(path_, ((flags & Constants::kformat_save_file_text) != 0) ? L"w" : L"wb"),
                        close_file);
  return save_file_impl(*this, file.data, indent, flags, encoding)
         && fclose(file.release()) == 0; // NOLINT(cppcoreguidelines-owning-memory)
}

inline XmlNode
XmlDocument::document_element() const
{
  LUMEX_ASSERT(m_root);

  for(XmlNodeBase *i = m_root->first_child; i != nullptr; i = i->next_sibling)
    if(LUMEX_XML_NODETYPE(i) == node_element) return XmlNode(i);

  return {};
}
