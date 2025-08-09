#ifndef LUMEXXML_WRITER_STREAM_HPP
#define LUMEXXML_WRITER_STREAM_HPP

#include "lumex/LumexExport.hpp"

#include <iostream>

#include "IXmlWriter.hpp"

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Writer
    {
      /**
       * @brief An `IXmlWriter` implementation that writes XML data to a C++ standard output stream.
       * @details `XmlWriterStream` provides a concrete implementation of the `IXmlWriter` interface,
       *          allowing XML serialization directly to `std::ostream` (for `char_t` streams)
       *          or `std::wostream` (for `wchar_t` streams). It handles the differences between
       *          narrow and wide character streams.
       * @note This class does not own the stream; the caller is responsible for its lifetime.
       * @see IXmlWriter
       */
      class LUMEX_API XmlWriterStream : public IXmlWriter
      {
      public:
        /**
         * @brief Constructs an `XmlWriterStream` for a narrow character stream (`std::ostream`).
         * @param[in,out] stream A reference to the `std::basic_ostream<char>` to write to.
         */
        XmlWriterStream(std::basic_ostream<char> &stream);
        /**
         * @brief Constructs an `XmlWriterStream` for a wide character stream (`std::wostream`).
         * @param[in,out] stream A reference to the `std::basic_ostream<wchar_t>` to write to.
         */
        XmlWriterStream(std::basic_ostream<wchar_t> &stream);

        /**
         * @brief Writes a block of raw data to the associated stream.
         * @param[in] data A pointer to the data buffer to write. Must not be `nullptr`.
         * @param[in] size The number of bytes to write from the `data` buffer.
         * @details This function determines whether to write to the narrow or wide stream based on
         *          how the object was constructed, casting `data` to the appropriate character type if needed.
         * @note Implements the `IXmlWriter::write` pure virtual function.
         * @throws `std::ios_base::failure` if stream operations fail and exceptions are enabled on the stream.
         */
        void write(void const *data, size_t size) override;

      private:
        /// @brief Pointer to the narrow character stream (if used). `nullptr` if a wide stream is active.
        std::basic_ostream<char> *narrow_stream;
        /// @brief Pointer to the wide character stream (if used). `nullptr` if a narrow stream is active.
        std::basic_ostream<wchar_t> *wide_stream;
      };
    } // namespace Writer
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEXXML_WRITER_STREAM_HPP
