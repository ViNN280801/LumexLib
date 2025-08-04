#define LUMEX_IMPLEMENTATION

#include "lumex/xml/utility/XmlUtils.hpp"

#include "XmlBufferedWriter.hpp"

using namespace Lumex::Xml::Writer;
using namespace Lumex::Xml::Utility;

LUMEX_PUBLIC_API
XmlBufferedWriter::XmlBufferedWriter(IXmlWriter &writer_, // NOLINT(cppcoreguidelines-pro-type-member-init)
                                     xml_encoding user_encoding)
    : buffer(), scratch(), writer(writer_), encoding(Utility::get_write_encoding(user_encoding))
{
  LUMEX_STATIC_ASSERT(bufcapacity >= kBufCapacity8);
}

LUMEX_PUBLIC_API
size_t
XmlBufferedWriter::flush()
{
  flush(buffer.data(), bufsize);
  bufsize = 0;
  return 0;
}

LUMEX_PUBLIC_API
void
XmlBufferedWriter::flush(char_t const *data, size_t size)
{
  if(size == 0) return;

  // fast path, just write data
  if(encoding == get_write_native_encoding())
    writer.write(data, size * sizeof(char_t));
  else
  {
    // convert chunk
    size_t result
      = Utility::convert_buffer_output(scratch.data_char.data(), // NOLINT(cppcoreguidelines-pro-type-union-access)
                                       scratch.data_u8.data(),   // NOLINT(cppcoreguidelines-pro-type-union-access)
                                       scratch.data_u16.data(),  // NOLINT(cppcoreguidelines-pro-type-union-access)
                                       scratch.data_u32.data(),  // NOLINT(cppcoreguidelines-pro-type-union-access)
                                       data, size, encoding);
    LUMEX_ASSERT(result <= sizeof(scratch));

    // write data
    writer.write(scratch.data_u8.data(), result); // NOLINT(cppcoreguidelines-pro-type-union-access)
  }
}

LUMEX_PUBLIC_API
void
XmlBufferedWriter::write_direct(char_t const *data, size_t length)
{
  // flush the remaining buffer contents
  flush();

  // handle large chunks
  if(length > bufcapacity)
  {
    if(encoding == get_write_native_encoding())
    {
      // fast path, can just write data chunk
      writer.write(data, length * sizeof(char_t));
      return;
    }

    // need to convert in suitable chunks
    while(length > bufcapacity)
    {
      // get chunk size by selecting such number of characters that are guaranteed to fit into scratch buffer
      // and form a complete codepoint sequence (i.e. discard start of last codepoint if necessary)
      size_t chunk_size = get_valid_length(data, bufcapacity);
      LUMEX_ASSERT(chunk_size);

      // convert chunk and write
      flush(data, chunk_size);

      // iterate
      data += chunk_size;
      length -= chunk_size;
    }

    // small tail is copied below
    bufsize = 0;
  }

  memcpy(buffer.data() + bufsize, data, length * sizeof(char_t));
  bufsize += length;
}

LUMEX_PUBLIC_API
void
XmlBufferedWriter::write_buffer(char_t const *data, size_t length)
{
  size_t offset = bufsize;

  if(offset + length <= bufcapacity)
  {
    memcpy(buffer.data() + offset, data, length * sizeof(char_t));
    bufsize = offset + length;
  }
  else { write_direct(data, length); }
}

LUMEX_PUBLIC_API
void
XmlBufferedWriter::write_string(char_t const *data)
{
  // write the part of the string that fits in the buffer
  size_t offset = bufsize;

  while((*data != 0) && offset < bufcapacity) buffer.at(offset++) = *data++;

  // write the rest
  if(offset < bufcapacity) { bufsize = offset; }
  else
  {
    // backtrack a bit if we have split the codepoint
    size_t length = offset - bufsize;
    size_t extra  = length - get_valid_length(data - length, length);

    bufsize       = offset - extra;

    write_direct(data - extra, Utility::strlength(data) + extra);
  }
}

LUMEX_PUBLIC_API
void
XmlBufferedWriter::write(char_t d0_)
{
  size_t offset = bufsize;
  if(offset > bufcapacity - 1) 
  {
    flush();
    offset = 0;
  }

  buffer.at(offset + 0) = d0_;
  bufsize               = offset + 1;
}

LUMEX_PUBLIC_API
void
XmlBufferedWriter::write(char_t d0_, char_t d1_) // NOLINT(bugprone-easily-swappable-parameters)
{
  size_t offset = bufsize;
  if(offset > bufcapacity - 2) 
  {
    flush();
    offset = 0;
  }

  buffer.at(offset + 0) = d0_;
  buffer.at(offset + 1) = d1_;
  bufsize               = offset + 2;
}

LUMEX_PUBLIC_API
void
XmlBufferedWriter::write(char_t d0_, char_t d1_, char_t d2_) // NOLINT(bugprone-easily-swappable-parameters)
{
  size_t offset = bufsize;
  if(offset > bufcapacity - 3) 
  {
    flush();
    offset = 0;
  }

  buffer.at(offset + 0) = d0_;
  buffer.at(offset + 1) = d1_;
  buffer.at(offset + 2) = d2_;
  bufsize               = offset + 3;
}

LUMEX_PUBLIC_API
void
XmlBufferedWriter::write(char_t d0_, char_t d1_, // NOLINT(bugprone-easily-swappable-parameters)
                         char_t d2_, char_t d3_)
{
  size_t offset = bufsize;
  if(offset > bufcapacity - 4) 
  {
    flush();
    offset = 0;
  }

  buffer.at(offset + 0) = d0_;
  buffer.at(offset + 1) = d1_;
  buffer.at(offset + 2) = d2_;
  buffer.at(offset + 3) = d3_;
  bufsize               = offset + 4;
}

LUMEX_PUBLIC_API
void
XmlBufferedWriter::write(char_t d0_, char_t d1_, // NOLINT(bugprone-easily-swappable-parameters)
                         char_t d2_, char_t d3_, char_t d4_)
{
  size_t offset = bufsize;
  if(offset > bufcapacity - kShift5) 
  {
    flush();
    offset = 0;
  }

  buffer.at(offset + 0) = d0_;
  buffer.at(offset + 1) = d1_;
  buffer.at(offset + 2) = d2_;
  buffer.at(offset + 3) = d3_;
  buffer.at(offset + 4) = d4_;
  bufsize               = offset + kShift5;
}

LUMEX_PUBLIC_API
void
XmlBufferedWriter::write(char_t d0_, char_t d1_, char_t d2_, // NOLINT(bugprone-easily-swappable-parameters)
                         char_t d3_, char_t d4_, char_t d5_)
{
  size_t offset = bufsize;
  if(offset > bufcapacity - kShift6) 
  {
    flush();
    offset = 0;
  }

  buffer.at(offset + 0)       = d0_;
  buffer.at(offset + 1)       = d1_;
  buffer.at(offset + 2)       = d2_;
  buffer.at(offset + 3)       = d3_;
  buffer.at(offset + 4)       = d4_;
  buffer.at(offset + kShift5) = d5_;
  bufsize                     = offset + kShift6;
}
