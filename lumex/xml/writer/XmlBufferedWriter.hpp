#ifndef LUMEX_XML_WRITER_XML_BUFFERED_WRITER_HPP
#define LUMEX_XML_WRITER_XML_BUFFERED_WRITER_HPP

#include "lumex/LumexExport.hpp"

#include <array>
#include <cstddef>

#include "lumex/xml/types/XmlTypes.hpp"

#include "IXmlWriter.hpp"

using namespace Lumex::Xml::Types;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Writer
    {

      /**
       * @brief A buffered writer for XML output, optimizing write operations by accumulating data in a buffer.
       * @details `XmlBufferedWriter` acts as an intermediary between the XML serialization logic and a raw
       *          `IXmlWriter` interface. It collects characters in an internal buffer and flushes them to the
       *          underlying writer when the buffer is full or explicitly requested. This significantly
       *          reduces the number of system calls for small write operations, improving performance.
       *          It also handles character encoding conversions if necessary.
       * @note This class is not copyable and manages its own internal buffers.
       * @see IXmlWriter
       */
      class LUMEX_API XmlBufferedWriter // NOLINT(cppcoreguidelines-special-member-functions)
      {
      public:
        /// @brief Constant for buffer capacity related to UTF-8 character expansion.
        static constexpr short const kBufCapacity8 = 8;
        /// @brief Constant for bit shift operation, often used in character encoding.
        static constexpr short const kShift5 = 5;
        /// @brief Constant for bit shift operation, often used in character encoding.
        static constexpr short const kShift6 = 6;

        /// @brief Enumeration defining buffer capacities in bytes and characters.
        enum : std::uint16_t
        {
          bufcapacitybytes = 10240,                             ///< @brief Total capacity of the buffer in bytes.
          bufcapacity = bufcapacitybytes / (sizeof(char_t) + 4) ///< @brief Capacity of the buffer in `char_t` units.
        };

        /// @brief The main buffer for accumulating characters before writing.
        std::array<char_t, bufcapacity> buffer; // NOLINT(misc-non-private-member-variables-in-classes)

        /// @brief Union for scratch space, used for temporary conversions, optimized for different character sizes.
        union {
          std::array<std::uint8_t, static_cast<size_t>(4U * bufcapacity)>
            data_u8; ///< @brief Scratch buffer for 8-bit characters (UTF-8).
          std::array<std::uint16_t, static_cast<size_t>(2U * bufcapacity)>
            data_u16;                                      ///< @brief Scratch buffer for 16-bit characters (UTF-16).
          std::array<std::uint32_t, bufcapacity> data_u32; ///< @brief Scratch buffer for 32-bit characters (UTF-32).
          std::array<char_t, bufcapacity> data_char;       ///< @brief Scratch buffer for generic `char_t`.
        } scratch;                                         // NOLINT(misc-non-private-member-variables-in-classes)

        /// @brief Reference to the underlying `IXmlWriter` to which buffered data is flushed.
        IXmlWriter &writer; // NOLINT(misc-non-private-member-variables-in-classes,
                            // cppcoreguidelines-avoid-const-or-ref-data-members)
        /// @brief Current size of the data in the buffer.
        size_t bufsize{}; // NOLINT(misc-non-private-member-variables-in-classes)
        /// @brief The character encoding to use for output.
        xml_encoding encoding; // NOLINT(misc-non-private-member-variables-in-classes)

        /**
         * @brief Deleted copy constructor.
         * @details `XmlBufferedWriter` is not copyable to prevent issues with buffer ownership and state.
         */
        XmlBufferedWriter(XmlBufferedWriter const &) = delete;
        /**
         * @brief Deleted copy assignment operator.
         * @details `XmlBufferedWriter` is not copy-assignable.
         */
        XmlBufferedWriter &operator=(XmlBufferedWriter const &) = delete;

        /**
         * @brief Constructs an `XmlBufferedWriter`.
         * @param[in,out] writer_ The underlying `IXmlWriter` to which data will be flushed.
         * @param[in] user_encoding The character encoding to use for the output.
         */
        XmlBufferedWriter(IXmlWriter &writer_, xml_encoding user_encoding);

        /**
         * @brief Flushes the current buffer content to the underlying writer.
         * @return The number of bytes successfully written.
         * @details Empties the internal buffer by writing its contents to the `writer` and resets `bufsize`.
         */
        size_t flush();

        /**
         * @brief Writes a block of data directly to the underlying writer after flushing the buffer.
         * @param[in] data A pointer to the data to write.
         * @param[in] size The number of bytes to write.
         * @details This function first flushes any data currently in the internal buffer, then writes the provided
         * `data` directly.
         */
        void flush(char_t const *data, size_t size);

        /**
         * @brief Writes a block of characters directly to the buffer, bypassing encoding.
         * @param[in] data A pointer to the character data to write.
         * @param[in] length The number of characters to write.
         * @details This function attempts to append `length` characters from `data` to the internal buffer.
         *          If the buffer is too small, it flushes and retries.
         */
        void write_direct(char_t const *data, size_t length);

        /**
         * @brief Writes a block of characters to the buffer, handling encoding conversions if necessary.
         * @param[in] data A pointer to the character data to write.
         * @param[in] length The number of characters to write.
         * @details This function performs character encoding conversion from the internal `char_t` representation
         *          to the `encoding` specified during construction, then writes to the buffer.
         */
        void write_buffer(char_t const *data, size_t length);

        /**
         * @brief Writes a null-terminated string to the buffer.
         * @param[in] data A pointer to the null-terminated string to write.
         * @details This function calculates the string length and calls `write_buffer`.
         */
        void write_string(char_t const *data);

        /**
         * @brief Writes a single character to the buffer.
         * @param[in] d0_ The character to write.
         * @details This is an optimized function for writing a single character.
         */
        void write(char_t d0_);

        /**
         * @brief Writes two characters to the buffer.
         * @param[in] d0_ The first character.
         * @param[in] d1_ The second character.
         */
        void write(char_t d0_, char_t d1_); // NOLINT(bugprone-easily-swappable-parameters)

        /**
         * @brief Writes three characters to the buffer.
         * @param[in] d0_ The first character.
         * @param[in] d1_ The second character.
         * @param[in] d2_ The third character.
         */
        void write(char_t d0_, char_t d1_, char_t d2_); // NOLINT(bugprone-easily-swappable-parameters)

        /**
         * @brief Writes four characters to the buffer.
         * @param[in] d0_ The first character.
         * @param[in] d1_ The second character.
         * @param[in] d2_ The third character.
         * @param[in] d3_ The fourth character.
         */
        void write(char_t d0_, char_t d1_, char_t d2_, char_t d3_); // NOLINT(bugprone-easily-swappable-parameters)

        /**
         * @brief Writes five characters to the buffer.
         * @param[in] d0_ The first character.
         * @param[in] d1_ The second character.
         * @param[in] d2_ The third character.
         * @param[in] d3_ The fourth character.
         * @param[in] d4_ The fifth character.
         */
        void write(char_t d0_, char_t d1_, // NOLINT(bugprone-easily-swappable-parameters)
                   char_t d2_, char_t d3_, char_t d4_);

        /**
         * @brief Writes six characters to the buffer.
         * @param[in] d0_ The first character.
         * @param[in] d1_ The second character.
         * @param[in] d2_ The third character.
         * @param[in] d3_ The fourth character.
         * @param[in] d4_ The fifth character.
         * @param[in] d5_ The sixth character.
         */
        void write(char_t d0_, char_t d1_, char_t d2_, // NOLINT(bugprone-easily-swappable-parameters)
                   char_t d3_, char_t d4_, char_t d5_);
      };
    } // namespace Writer
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_WRITER_XML_BUFFERED_WRITER_HPP
