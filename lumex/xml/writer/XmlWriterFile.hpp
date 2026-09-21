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

#ifndef LUMEX_XML_WRITER_XML_WRITER_FILE_HPP
#define LUMEX_XML_WRITER_XML_WRITER_FILE_HPP

#include "lumex/LumexExport.hpp"

#include "IXmlWriter.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace xml
{
namespace writer
{
/**
 * @brief An `IXmlWriter` implementation that writes XML data to a file.
 * @details This class provides a concrete implementation of the `IXmlWriter`
 * interface, enabling XML serialization directly to a file stream. It wraps a
 * `void*` file handle, assuming the caller manages the underlying file
 * lifecycle.
 * @note This class does not own the `file` pointer; it's the caller's
 * responsibility to open and close the file. It is not copyable or movable.
 * @see IXmlWriter
 */
class LUMEX_API XmlWriterFile : public IXmlWriter
{
public:
  /**
   * @brief Constructs an `XmlWriterFile` with a given file handle.
   * @param[in] file A `void*` pointer to the file handle (e.g., from `fopen`).
   *                   This class does not take ownership of this pointer.
   */
  XmlWriterFile (void *file) : file (file) {}
  /**
   * @brief Virtual destructor for `XmlWriterFile`.
   * @details Ensures proper cleanup of derived classes.
   */
  ~XmlWriterFile () override = default;
  /**
   * @brief Deleted copy constructor.
   * @details `XmlWriterFile` is not copyable to prevent issues with file
   * handle ownership.
   */
  XmlWriterFile (XmlWriterFile const &) = delete;
  /**
   * @brief Deleted move constructor.
   * @details `XmlWriterFile` is not movable.
   */
  XmlWriterFile (XmlWriterFile &&) = delete;
  /**
   * @brief Deleted copy assignment operator.
   * @details `XmlWriterFile` is not copy-assignable.
   */
  XmlWriterFile &operator= (XmlWriterFile const &) = delete;
  /**
   * @brief Deleted move assignment operator.
   * @details `XmlWriterFile` is not move-assignable.
   */
  XmlWriterFile &operator= (XmlWriterFile &&) = delete;

  /**
   * @brief Writes a block of raw data to the associated file.
   * @param[in] data A pointer to the data buffer to write. Must not be
   * `nullptr`.
   * @param[in] size The number of bytes to write from the `data` buffer.
   * @details This function uses platform-specific file writing mechanisms
   * (`fwrite` on POSIX/Windows) to write the data.
   * @note Implements the `IXmlWriter::write` pure virtual function.
   */
  void write (void const *data, std::size_t size) override;

private:
  /// @brief Internal pointer to the file handle.
  void *file;
};
} // namespace writer
} // namespace xml
} // namespace lumex

#endif // !LUMEX_XML_WRITER_XML_WRITER_FILE_HPP
