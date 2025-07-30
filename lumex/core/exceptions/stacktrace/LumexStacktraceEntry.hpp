/**
 * @file LumexStacktraceEntry.hpp
 * @brief Defines the LumexStacktraceEntry class, representing a single frame in a call stack.
 * @details This header provides the declaration for the `LumexStacktraceEntry` class,
 *          which encapsulates information about one function call within a stack trace.
 *          This includes the raw memory address, and (lazily resolved) human-readable
 *          function name, source file path, and line number. It aims to provide an
 *          interface similar to the C++23 `std::stacktrace_entry`.
 */
#ifndef LUMEX_STACKTRACE_ENTRY_HPP
#define LUMEX_STACKTRACE_ENTRY_HPP

#include "lumex/LumexExport.hpp"

#include <cstddef> // std::size_t
#include <cstdint> // std::uint32_t
#include <string>  // std::string

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Core
  {
    namespace Stacktrace
    {
      /**
       * @brief Represents a single entry (frame) in a stacktrace.
       * @details This class provides information about a single function call in the
       *          call stack, including the function name, source file, line number,
       *          and raw address. It follows the C++23 `std::stacktrace_entry` interface
       *          by providing accessors to symbolized information.
       *
       * @note The class is designed to be lightweight and copyable. All string
       *       operations for `description()`, `source_file()`, and `source_line()`
       *       are performed on-demand (lazily) and the results are cached to
       *       minimize overhead during stacktrace capture and repeated access.
       *       Symbol resolution can be an expensive operation.
       * @warning Symbol resolution might fail or provide incomplete information
       *          if debug symbols are not available or if the address is invalid.
       *
       * @example
       * LumexStacktraceEntry entry(reinterpret_cast<void*>(0x12345678));
       * std::cout << "Function: " << entry.description() << std::endl;
       * std::cout << "Source: " << entry.source_file() << ":" << entry.source_line() << std::endl;
       */
      class LUMEX_API LumexStacktraceEntry
      {
      public:
        /// @brief Type alias for the native handle (raw address) of a stack frame.
        using native_handle_type = void *;

      private:
        /// @brief The raw memory address of the stack frame.
        native_handle_type m_address;

// Suppress C4251 warnings for STL containers in DLL interface
// C4251: 'class' : class 'type' needs to have dll-interface to be used by clients of class 'class'
// This warning is common when `std::string` (or other STL containers) is a member of a class
// exported from a DLL, because `std::string` itself is not exported. It's usually safe to ignore
// if the client code is also compiled with the same C++ standard library version.
#ifdef _WIN32
  #pragma warning(push)
  #pragma warning(disable : 4251)
#endif
        /// @brief Cached human-readable description of the stack frame (function name, offset).
        mutable std::string m_cached_description;
        /// @brief Cached source file path where the frame's function is defined.
        mutable std::string m_cached_source_file;
        /// @brief Cached line number within the source file where the call originated.
        mutable std::uint32_t m_cached_source_line;
        /// @brief Flag indicating whether cached symbol information is valid.
        mutable bool m_cache_valid;
#ifdef _WIN32
  #pragma warning(pop)
#endif

        /**
         * @brief Ensures that the cached symbol information (`m_cached_description`, `m_cached_source_file`,
         * `m_cached_source_line`) is valid.
         * @details This private helper method performs lazy initialization of the cached
         *          symbol information. If `m_cache_valid` is false and `m_address` is not `nullptr`,
         *          it calls the platform-specific `detail::resolve_symbol_info` to populate
         *          the cache. This approach defers the potentially expensive symbol resolution
         *          until the information is actually requested.
         * @note This method is `const` because it only modifies `mutable` members (`m_cached_*` and `m_cache_valid`),
         *       which is considered a logical `const` operation (caching results does not
         *       change the observable state of the object).
         */
        void ensure_cache_valid() const;

      public:
        /**
         * @brief Default constructor for `LumexStacktraceEntry`.
         * @details Creates an empty/invalid stacktrace entry by initializing the address to `nullptr`
         *          and marking the cache as invalid.
         * @note This constructor is `noexcept`.
         */
        LumexStacktraceEntry() noexcept : m_address(nullptr), m_cached_source_line(0), m_cache_valid(false) {}

        /**
         * @brief Constructs a `LumexStacktraceEntry` from a native address.
         * @details Initializes the stacktrace entry with the provided raw memory address.
         *          The cached symbol information is initially marked as invalid and will be
         *          resolved on the first call to `description()`, `source_file()`, or `source_line()`.
         * @param[in] addr The raw `void*` address of the stack frame.
         * @note This constructor is `noexcept`.
         */
        explicit LumexStacktraceEntry(native_handle_type addr) noexcept
            : m_address(addr), m_cached_source_line(0), m_cache_valid(false)
        {}

        /**
         * @brief Copy constructor for `LumexStacktraceEntry`.
         * @details Performs a member-wise copy, including the address and the cached symbol information.
         * @param[in] other The `LumexStacktraceEntry` object to copy from.
         * @note This constructor is `noexcept` because all member types are `noexcept` copyable.
         */
        LumexStacktraceEntry(LumexStacktraceEntry const &other) noexcept
            : m_address(other.m_address),
              m_cached_description(other.m_cached_description),
              m_cached_source_file(other.m_cached_source_file),
              m_cached_source_line(other.m_cached_source_line),
              m_cache_valid(other.m_cache_valid)
        {}

        /**
         * @brief Copy assignment operator for `LumexStacktraceEntry`.
         * @details Assigns the contents of another `LumexStacktraceEntry` object.
         * @param[in] other The `LumexStacktraceEntry` object to assign from.
         * @return A reference to `*this` after assignment.
         * @note This operator is `noexcept` and provides the strong exception guarantee.
         */
        LumexStacktraceEntry &
        operator=(LumexStacktraceEntry const &other) noexcept
        {
          if(this != &other)
          {
            m_address            = other.m_address;
            m_cached_description = other.m_cached_description;
            m_cached_source_file = other.m_cached_source_file;
            m_cached_source_line = other.m_cached_source_line;
            m_cache_valid        = other.m_cache_valid;
          }
          return *this;
        }

        /**
         * @brief Default destructor for `LumexStacktraceEntry`.
         * @details Cleans up any resources held by the member variables (primarily `std::string`).
         */
        ~LumexStacktraceEntry() = default;

        /**
         * @brief Returns the native handle (raw memory address) of this stack frame.
         * @return The address as a `void*` pointer.
         * @note This method is `constexpr` and `noexcept` as it simply returns a member variable.
         */
        constexpr native_handle_type
        native_handle() const noexcept
        {
          return m_address;
        }

        /**
         * @brief Checks if this stacktrace entry is valid (i.e., has a non-null address).
         * @return True if the entry contains a valid (non-`nullptr`) address, false otherwise.
         * @note This operator allows `LumexStacktraceEntry` objects to be used directly
         *       in boolean contexts (e.g., `if (entry)`). It is `constexpr` and `noexcept`.
         */
        constexpr explicit
        operator bool() const noexcept
        {
          return m_address != nullptr;
        }

        /**
         * @brief Gets a human-readable description of this stack frame.
         * @details This method triggers lazy symbol resolution if the cached information
         *          is not yet valid. The description typically includes the function name
         *          and may also contain the source file and line number if available and
         *          resolved by the underlying platform-specific symbolization mechanism.
         * @return A `std::string` containing the description of the frame. Returns an empty
         *         string if the address is `nullptr` or symbol resolution fails.
         * @note This operation may be expensive on the first call for a given entry as it queries
         *       debug symbols from the operating system or external tools. Subsequent calls
         *       will return the cached value quickly.
         */
        std::string description() const;

        /**
         * @brief Gets the source file path where the function represented by this frame is defined.
         * @details This method triggers lazy symbol resolution if needed.
         * @return The `std::string` path to the source file, or an empty string if unavailable.
         * @note This operation may be expensive on the first call for a given entry as it queries
         *       debug symbols. Subsequent calls will return the cached value quickly.
         */
        std::string source_file() const;

        /**
         * @brief Gets the source line number where this function call originated.
         * @details This method triggers lazy symbol resolution if needed.
         * @return The `std::uint32_t` line number, or `0` if unavailable.
         * @note This operation may be expensive on the first call for a given entry as it queries
         *       debug symbols. Subsequent calls will return the cached value quickly.
         */
        std::uint32_t source_line() const;

        /**
         * @brief Equality comparison operator for `LumexStacktraceEntry`.
         * @details Two `LumexStacktraceEntry` objects are considered equal if their
         *          native handles (addresses) are identical.
         * @param lhs The left-hand side `LumexStacktraceEntry` object.
         * @param rhs The right-hand side `LumexStacktraceEntry` object.
         * @return True if the addresses are equal, false otherwise.
         * @note This operator is `noexcept` and a `friend` function.
         */
        friend bool
        operator==(LumexStacktraceEntry const &lhs, LumexStacktraceEntry const &rhs) noexcept
        {
          return lhs.m_address == rhs.m_address;
        }

        /**
         * @brief Inequality comparison operator for `LumexStacktraceEntry`.
         * @details This is the logical negation of `operator==`.
         * @param lhs The left-hand side `LumexStacktraceEntry` object.
         * @param rhs The right-hand side `LumexStacktraceEntry` object.
         * @return True if the addresses are not equal, false otherwise.
         * @note This operator is `noexcept` and a `friend` function.
         */
        friend bool
        operator!=(LumexStacktraceEntry const &lhs, LumexStacktraceEntry const &rhs) noexcept
        {
          return !(lhs == rhs);
        }

        /**
         * @brief Less-than comparison operator for `LumexStacktraceEntry`.
         * @details Compares two `LumexStacktraceEntry` objects based on their
         *          native handles (addresses). This provides a strict weak ordering,
         *          allowing entries to be used in ordered containers (e.g., `std::set`).
         * @param lhs The left-hand side `LumexStacktraceEntry` object.
         * @param rhs The right-hand side `LumexStacktraceEntry` object.
         * @return True if `lhs`'s address is less than `rhs`'s address, false otherwise.
         * @note This operator is `noexcept` and a `friend` function.
         */
        friend bool
        operator<(LumexStacktraceEntry const &lhs, LumexStacktraceEntry const &rhs) noexcept
        {
          return lhs.m_address < rhs.m_address;
        }

        /**
         * @brief Less-than-or-equal-to comparison operator for `LumexStacktraceEntry`.
         * @details This is the logical negation of `operator>`.
         * @param lhs The left-hand side `LumexStacktraceEntry` object.
         * @param rhs The right-hand side `LumexStacktraceEntry` object.
         * @return True if `lhs`'s address is less than or equal to `rhs`'s address, false otherwise.
         * @note This operator is `noexcept` and a `friend` function.
         */
        friend bool
        operator<=(LumexStacktraceEntry const &lhs, LumexStacktraceEntry const &rhs) noexcept
        {
          return !(rhs < lhs);
        }

        /**
         * @brief Greater-than comparison operator for `LumexStacktraceEntry`.
         * @details This is equivalent to `rhs < lhs`.
         * @param lhs The left-hand side `LumexStacktraceEntry` object.
         * @param rhs The right-hand side `LumexStacktraceEntry` object.
         * @return True if `lhs`'s address is greater than `rhs`'s address, false otherwise.
         * @note This operator is `noexcept` and a `friend` function.
         */
        friend bool
        operator>(LumexStacktraceEntry const &lhs, LumexStacktraceEntry const &rhs) noexcept
        {
          return rhs < lhs;
        }

        /**
         * @brief Greater-than-or-equal-to comparison operator for `LumexStacktraceEntry`.
         * @details This is the logical negation of `operator<`.
         * @param lhs The left-hand side `LumexStacktraceEntry` object.
         * @param rhs The right-hand side `LumexStacktraceEntry` object.
         * @return True if `lhs`'s address is greater than or equal to `rhs`'s address, false otherwise.
         * @note This operator is `noexcept` and a `friend` function.
         */
        friend bool
        operator>=(LumexStacktraceEntry const &lhs, LumexStacktraceEntry const &rhs) noexcept
        {
          return !(lhs < rhs);
        }
      };

      /**
       * @brief Converts a `LumexStacktraceEntry` object to its string representation.
       * @details This inline function provides a convenient way to get the string
       *          description of a stacktrace entry by simply calling its `description()` method.
       * @param entry The `LumexStacktraceEntry` object to convert.
       * @return A `std::string` containing the description of the entry.
       */
      inline std::string
      to_string(LumexStacktraceEntry const &entry)
      {
        return entry.description();
      }

      /**
       * @brief Overloads the `operator<<` for `std::basic_ostream` to print a `LumexStacktraceEntry`.
       * @details This allows `LumexStacktraceEntry` objects to be easily streamed to
       *          any `std::basic_ostream` (e.g., `std::cout`, `std::cerr`) using
       *          its string `description()`.
       * @tparam CharT The character type of the output stream.
       * @tparam Traits The character traits of the output stream.
       * @param oss The output stream to write to.
       * @param entry The `LumexStacktraceEntry` object to print.
       * @return A reference to the output stream after the entry has been written.
       */
      template <typename CharT, typename Traits>
      std::basic_ostream<CharT, Traits> &
      operator<<(std::basic_ostream<CharT, Traits> &oss, LumexStacktraceEntry const &entry)
      {
        return oss << entry.description();
      }

    } // namespace Stacktrace
  } // namespace Core
} // namespace Lumex

// Hash support in std namespace (for C++11 compatibility)
namespace std
{
  /**
   * @brief Partial specialization of `std::hash` for `Lumex::Core::Stacktrace::LumexStacktraceEntry`.
   * @details This specialization provides a hash function for `LumexStacktraceEntry` objects,
   *          enabling their use in hash-based containers like `std::unordered_set`
   *          and `std::unordered_map`. The hash value is computed based on the
   *          native handle (address) of the stack entry.
   */
  template <> struct hash<Lumex::Core::Stacktrace::LumexStacktraceEntry> {
    /**
     * @brief Computes the hash value for a `LumexStacktraceEntry` object.
     * @param entry The `LumexStacktraceEntry` object to hash.
     * @return A `size_t` representing the hash value of the entry's native handle.
     * @note This operator is `noexcept`.
     */
    size_t
    operator()(Lumex::Core::Stacktrace::LumexStacktraceEntry const &entry) const noexcept
    {
      return std::hash<void *>()(entry.native_handle());
    }
  };
} // namespace std

#endif // LUMEX_STACKTRACE_ENTRY_HPP