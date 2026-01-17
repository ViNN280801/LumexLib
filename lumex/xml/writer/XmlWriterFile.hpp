#ifndef LUMEXXML_WRITER_FILE_HPP
#define LUMEXXML_WRITER_FILE_HPP

#include "lumex/LumexExport.hpp"

#include "IXmlWriter.hpp"

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Writer
    {
      /**
       * @brief An `IXmlWriter` implementation that writes XML data to a file.
       * @details This class provides a concrete implementation of the `IXmlWriter` interface,
       *          enabling XML serialization directly to a file stream. It wraps a `void*`
       *          file handle, assuming the caller manages the underlying file lifecycle.
       * @note This class does not own the `file` pointer; it's the caller's responsibility
       *       to open and close the file. It is not copyable or movable.
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
        XmlWriterFile(void *file) : file(file) {}
        /**
         * @brief Virtual destructor for `XmlWriterFile`.
         * @details Ensures proper cleanup of derived classes.
         */
        ~XmlWriterFile() override = default;
        /**
         * @brief Deleted copy constructor.
         * @details `XmlWriterFile` is not copyable to prevent issues with file handle ownership.
         */
        XmlWriterFile(XmlWriterFile const &) = delete;
        /**
         * @brief Deleted move constructor.
         * @details `XmlWriterFile` is not movable.
         */
        XmlWriterFile(XmlWriterFile &&) = delete;
        /**
         * @brief Deleted copy assignment operator.
         * @details `XmlWriterFile` is not copy-assignable.
         */
        XmlWriterFile &operator=(XmlWriterFile const &) = delete;
        /**
         * @brief Deleted move assignment operator.
         * @details `XmlWriterFile` is not move-assignable.
         */
        XmlWriterFile &operator=(XmlWriterFile &&) = delete;

        /**
         * @brief Writes a block of raw data to the associated file.
         * @param[in] data A pointer to the data buffer to write. Must not be `nullptr`.
         * @param[in] size The number of bytes to write from the `data` buffer.
         * @details This function uses platform-specific file writing mechanisms (`fwrite` on POSIX/Windows)
         *          to write the data.
         * @note Implements the `IXmlWriter::write` pure virtual function.
         */
        void write(void const *data, size_t size) override;

      private:
        /// @brief Internal pointer to the file handle.
        void *file;
      };
    } // namespace Writer
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEXXML_WRITER_FILE_HPP
