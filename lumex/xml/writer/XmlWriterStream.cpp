/*
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

#define LUMEX_IMPLEMENTATION

#include "lumex/core/utility/assert/LumexAssert.hpp"

#include "XmlWriterStream.hpp"

using namespace lumex::xml::writer;

LUMEX_PUBLIC_API
XmlWriterStream::XmlWriterStream (std::basic_ostream<char> &stream)
    : narrow_stream (&stream), wide_stream (nullptr)
{
}

LUMEX_PUBLIC_API
XmlWriterStream::XmlWriterStream (std::basic_ostream<wchar_t> &stream)
    : narrow_stream (nullptr), wide_stream (&stream)
{
}

LUMEX_PUBLIC_API
void
XmlWriterStream::write (void const *data, std::size_t size)
{
  if (narrow_stream != nullptr)
    {
      LUMEX_ASSERT (!wide_stream);
      narrow_stream->write (
          reinterpret_cast<char const *> (
              data), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
          static_cast<std::streamsize> (size));
    }
  else
    {
      LUMEX_ASSERT (wide_stream);
      LUMEX_ASSERT (size % sizeof (wchar_t) == 0);

      wide_stream->write (
          reinterpret_cast<wchar_t const *> (
              data), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
          static_cast<std::streamsize> (size / sizeof (wchar_t)));
    }
}
