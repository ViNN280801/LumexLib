/**
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy
 * of this software and associated
 * documentation files (the "Software"), to deal
 * in the Software without
 * restriction, including without limitation the rights
 * to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the
 * Software, and to permit persons to whom the Software is
 * furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice
 * and this permission notice shall be included in
 * all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT
 * WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO
 * THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef LUMEX_XML_WRITER_XML_WRITER_STREAM_HPP
#define LUMEX_XML_WRITER_XML_WRITER_STREAM_HPP

#include "lumex/LumexExport.hpp"

#include <iostream>

#include "IXmlWriter.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace xml
{
namespace writer
{
/**
 * @brief An `IXmlWriter` implementation that writes XML data to a C++ standard
 * output stream.
 * @details `XmlWriterStream` provides a concrete implementation of the
 * `IXmlWriter` interface, allowing XML serialization directly to
 * `std::ostream` (for `char_t` streams) or `std::wostream` (for `wchar_t`
 * streams). It handles the differences between narrow and wide character
 * streams.
 * @note This class does not own the stream; the caller is responsible for its
 * lifetime.
 * @see IXmlWriter
 */
class LUMEX_API XmlWriterStream : public IXmlWriter
{
public:
  /**
   * @brief Constructs an `XmlWriterStream` for a narrow character stream
   * (`std::ostream`).
   * @param[in,out] stream A reference to the `std::basic_ostream<char>` to
   * write to.
   */
  XmlWriterStream (std::basic_ostream<char> &stream);
  /**
   * @brief Constructs an `XmlWriterStream` for a wide character stream
   * (`std::wostream`).
   * @param[in,out] stream A reference to the `std::basic_ostream<wchar_t>` to
   * write to.
   */
  XmlWriterStream (std::basic_ostream<wchar_t> &stream);

  /**
   * @brief Writes a block of raw data to the associated stream.
   * @param[in] data A pointer to the data buffer to write. Must not be
   * `nullptr`.
   * @param[in] size The number of bytes to write from the `data` buffer.
   * @details This function determines whether to write to the narrow or wide
   * stream based on how the object was constructed, casting `data` to the
   * appropriate character type if needed.
   * @note Implements the `IXmlWriter::write` pure virtual function.
   * @throws `std::ios_base::failure` if stream operations fail and exceptions
   * are enabled on the stream.
   */
  void write (void const *data, std::size_t size) override;

private:
  /// @brief Pointer to the narrow character stream (if used). `nullptr` if a
  /// wide stream is active.
  std::basic_ostream<char> *narrow_stream;
  /// @brief Pointer to the wide character stream (if used). `nullptr` if a
  /// narrow stream is active.
  std::basic_ostream<wchar_t> *wide_stream;
};
} // namespace writer
} // namespace xml
} // namespace lumex

#endif // !LUMEX_XML_WRITER_XML_WRITER_STREAM_HPP
