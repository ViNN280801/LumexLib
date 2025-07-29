/**
 * @file LumexStacktrace.hpp
 * @brief Defines the LumexBasicStacktrace class and related utilities for capturing and symbolizing call stacks.
 * @details This header provides a cross-platform mechanism for generating and manipulating
 *          stack traces, which are crucial for debugging, error reporting, and
 *          understanding program execution flow. It includes platform-specific
 *          implementations for Windows (using DbgHelp) and POSIX systems (using backtrace, dladdr, and addr2line).
 *          The core `LumexBasicStacktrace` template class provides a container for
 *          `LumexStacktraceEntry` objects, representing individual frames in the call stack.
 *          It is designed to be allocator-aware and exception-safe.
 */
#ifndef LUMEX_STACKTRACE_HPP
#define LUMEX_STACKTRACE_HPP

#include "lumex/core/exceptions/stacktrace/LumexStacktraceEntry.hpp"
#include "lumex/core/utility/LumexUtility"

#include <algorithm>  // std::min, std::max, std::equal, std::lexicographical_compare
#include <cstring>    // std::strlen, std::strrchr
#include <functional> // std::hash
#include <memory>     // std::unique_ptr
#include <string>     // std::string, std::to_string
#include <vector>     // std::vector

// ****************** Platform-specific includes ****************** //
#if LUMEX_OS_WINDOWS
  #include <windows.h> // This include should be 1st

  #include <DbgHelp.h> // This include should be 2nd, because it uses types from windows.h

  #include <mutex> // std::mutex

  #pragma comment(lib, "dbghelp.lib")
#else
  #include <cstdio>     // std::snprintf, popen, pclose, fgets
  #include <cstdlib>    // std::getenv, std::free, std::strtoul
  #include <cxxabi.h>   // abi::__cxa_demangle
  #include <dlfcn.h>    // dladdr, Dl_info
  #include <execinfo.h> // backtrace
  #include <sstream>    // std::istringstream

  #if defined(__linux__)
    #include <sys/wait.h> // waitpid (though not directly used, generally for popen related)
    #include <unistd.h>   // fork, exec (addr2line related)
  #endif
#endif

// *********************************************************************** //

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Core
  {
    /**
     * @brief Contains classes and utilities related to stack trace management and exception handling.
     * @details This namespace groups all components for capturing, symbolizing, and
     *          representing call stacks within the LumexCore library, providing
     *          tools essential for diagnostics and error reporting.
     */
    namespace Stacktrace
    {
      template <typename Allocator> class LumexBasicStacktrace;

      /**
       * @brief Internal detail namespace for platform-specific stacktrace functionalities.
       * @details This namespace encapsulates helper functions and constants that are
       *          used internally by `LumexBasicStacktrace` for platform-dependent
       *          operations like capturing raw frames and resolving symbol information.
       */
      namespace detail
      {
        constexpr short const kHashRightShift     = 2;  ///< Constant for right bit shift operation in hash calculation.
        constexpr short const kHashLeftShift      = 6;  ///< Constant for left bit shift operation in hash calculation.
        constexpr short const kDefaultAddrStrSize = 32; ///< Default buffer size for address string representations.
        constexpr short const kDefaultMaxFrames = 128; ///< Default maximum number of frames to capture in a stacktrace.
        constexpr short const kDefaultBufferSize
          = 256; ///< Default buffer size for reading output from external commands (e.g., addr2line).
        constexpr short const kDefaultCmdSize
          = 512; ///< Default buffer size for constructing external commands (e.g., addr2line command string).
        constexpr std::size_t kHashGoldenRatio
          = 0x9e3779b9U; ///< Golden ratio constant used in hash calculation to provide good distribution.

#if LUMEX_OS_WINDOWS
        /// @brief Global mutex for synchronizing access to DbgHelp API functions.
        /// @details DbgHelp library is not thread-safe, so all calls to its functions
        ///          must be protected by this mutex. It is extern because its definition
        ///          is in `LumexStacktrace.cpp`.
        extern std::mutex g_dbghelp_mutex; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

        /**
         * @brief Helper class for RAII-style initialization and deinitialization of DbgHelp.
         * @details This class ensures that `SymInitialize` is called once when the first
         *          `DbgHelpInitializer` object is created, and symbol options are set.
         *          It uses a static flag `s_initialized` and `g_dbghelp_mutex` to ensure
         *          thread-safe, one-time initialization.
         */
        class DbgHelpInitializer
        {
        private:
          /// @brief Static flag indicating if DbgHelp has been initialized.
          static bool s_initialized;

        public:
          /**
           * @brief Constructs a `DbgHelpInitializer` object.
           * @details Attempts to initialize DbgHelp if it hasn't been initialized yet.
           *          This operation is thread-safe, protected by `g_dbghelp_mutex`.
           * @note This constructor is responsible for calling `SymInitialize` and `SymSetOptions`.
           */
          DbgHelpInitializer();

          /**
           * @brief Default destructor for `DbgHelpInitializer`.
           * @details No explicit deinitialization of DbgHelp is performed here as `SymCleanup`
           *          is usually called at process exit or when the last DbgHelp user is done.
           */
          ~DbgHelpInitializer() = default;

          /**
           * @brief Copy constructor for `DbgHelpInitializer`.
           * @details Performs a shallow copy of the `DbgHelpInitializer` object.
           */
          DbgHelpInitializer(DbgHelpInitializer const &) = default;

          /**
           * @brief Move constructor for `DbgHelpInitializer`.
           * @details Performs a move of the `DbgHelpInitializer` object.
           */
          DbgHelpInitializer(DbgHelpInitializer &&) noexcept = default;

          /**
           * @brief Copy assignment operator for `DbgHelpInitializer`.
           * @details Performs a copy assignment of the `DbgHelpInitializer` object.
           */
          DbgHelpInitializer &operator=(DbgHelpInitializer const &) = default;

          /**
           * @brief Move assignment operator for `DbgHelpInitializer`.
           * @details Performs a move assignment of the `DbgHelpInitializer` object.
           */
          DbgHelpInitializer &operator=(DbgHelpInitializer &&) noexcept = default;

          /**
           * @brief Checks if DbgHelp has been successfully initialized.
           * @return True if DbgHelp is initialized, false otherwise.
           * @note This method is `noexcept` as it only reads a static boolean flag.
           */
          bool is_initialized() const noexcept;
        };

        /**
         * @brief Platform-specific stacktrace capture implementation for Windows.
         * @details This function captures the current call stack using `CaptureStackBackTrace`
         *          Win32 API function. It then converts the raw addresses into a vector
         *          of `LumexStacktraceEntry` objects.
         * @tparam Allocator The allocator type for the resulting stacktrace.
         * @param skip Number of frames to skip from the top of the call stack (e.g., `capture_stacktrace` itself).
         * @param max_depth Maximum number of frames to capture.
         * @param alloc The allocator to use for the internal container.
         * @return A `LumexBasicStacktrace` containing the captured frames. Returns an empty
         *         stacktrace if DbgHelp is not initialized or no frames are captured.
         * @note This function is `noexcept` and designed to be exception-safe.
         */
        template <typename Allocator>
        Stacktrace::LumexBasicStacktrace<Allocator>
        capture_stacktrace(size_t skip, size_t max_depth, Allocator const &alloc) noexcept;

        /**
         * @brief Resolves symbol information (function name, source file, line number) for a given address on Windows.
         * @details This function uses DbgHelp API functions (`SymFromAddr`, `SymGetLineFromAddr64`,
         * `SymGetModuleInfo64`, `UnDecorateSymbolName`) to retrieve detailed information about a stack address. It
         * attempts to demangle C++ function names and provides fallbacks if full symbol info is not available.
         * @param address The `void*` address of the stack frame to resolve.
         * @param function_name Output parameter for the resolved function name.
         * @param source_file Output parameter for the source file path.
         * @param line_number Output parameter for the line number in the source file.
         * @return True if any symbol information was successfully resolved, false otherwise.
         * @note This function is thread-safe due to the use of `g_dbghelp_mutex`.
         * @throws Nothing.
         */
        bool resolve_symbol_info(void *address, std::string &function_name, std::string &source_file,
                                 std::uint32_t &line_number) noexcept;
#else
        /**
         * @brief Demangles a C++ mangled symbol name on POSIX systems.
         * @details This function uses `abi::__cxa_demangle` from the GNU C++ ABI library
         *          to convert a compiler-mangled C++ symbol name into a human-readable form.
         * @param mangled The null-terminated C-string of the mangled symbol name.
         * @return A `std::string` containing the demangled symbol name. If demangling fails,
         *         the original mangled name is returned. Returns an empty string if `mangled` is `nullptr`.
         */
        std::string demangle_symbol(char const *mangled);

        /**
         * @brief Retrieves source file and line number information for a given address using `addr2line` on POSIX
         * systems.
         * @details This function attempts to get more precise source location information
         *          by executing the `addr2line` utility as a subprocess. It parses the
         *          output to extract the file path and line number.
         * @param address The `void*` address of the stack frame to resolve.
         * @param file Output parameter for the source file path.
         * @param line Output parameter for the line number.
         * @return True if source information was successfully retrieved, false otherwise.
         * @note This function typically works best on Linux systems with `binutils` installed.
         * @warning This method involves spawning a child process (`popen`), which can be
         *          relatively slow and resource-intensive.
         */
        bool get_source_info_addr2line(void *address, std::string &file, std::uint32_t &line);

        /**
         * @brief Platform-specific stacktrace capture implementation for POSIX systems.
         * @details This function captures the current call stack using the `backtrace`
         *          function. It then converts the raw addresses into a vector of
         *          `LumexStacktraceEntry` objects. It includes logic to handle cases
         *          where optimizations might reduce frame count (e.g., Release builds).
         * @tparam Allocator The allocator type for the resulting stacktrace.
         * @param skip Number of frames to skip from the top of the call stack.
         * @param max_depth Maximum number of frames to capture.
         * @param alloc The allocator to use for the internal container.
         * @return A `LumexBasicStacktrace` containing the captured frames. Returns an empty
         *         stacktrace if no frames are captured.
         * @note This function is `noexcept` and designed to be exception-safe.
         */
        template <typename Allocator>
        Stacktrace::LumexBasicStacktrace<Allocator>
        capture_stacktrace(size_t skip, size_t max_depth, Allocator const &alloc) noexcept;

        /**
         * @brief Resolves symbol information (function name, source file, line number) for a given address on POSIX
         * systems.
         * @details This function uses `dladdr` to get basic symbol information (module, symbol name)
         *          and then attempts to use `get_source_info_addr2line` for more precise
         *          source file and line number information. It demangles C++ symbols.
         * @param address The `void*` address of the stack frame to resolve.
         * @param function_name Output parameter for the resolved function name.
         * @param source_file Output parameter for the source file path.
         * @param line_number Output parameter for the line number in the source file.
         * @return True if any symbol information was successfully resolved, false otherwise.
         * @throws Nothing.
         */
        bool resolve_symbol_info(void *address, std::string &function_name, std::string &source_file,
                                 std::uint32_t &line_number) noexcept;
#endif
      } // namespace detail

// Suppress C4251 warnings for STL containers in DLL interface for the entire template class
// C4251: 'class' : class 'type' needs to have dll-interface to be used by clients of class 'class'
// This warning is common when `std::vector` (or other STL containers) is a member of a class
// exported from a DLL, because `std::vector` itself is not exported. It's usually safe to ignore
// if the client code is also compiled with the same C++ standard library version.
#ifdef _WIN32
  #pragma warning(push)
  #pragma warning(disable : 4251)
#endif

      /**
       * @brief A basic, allocator-aware class for representing a call stack (stacktrace).
       * @details This template class provides a collection of `LumexStacktraceEntry` objects,
       *          each representing a single frame in a call stack. It supports custom
       *          allocators and provides standard container-like accessors (iterators, size, element access).
       *          The actual stack capture is delegated to platform-specific helper functions.
       * @tparam Allocator The allocator type to use for managing the underlying container
       *                   of `LumexStacktraceEntry` objects. Defaults to `std::allocator<LumexStacktraceEntry>`.
       * @note This class is non-copyable if the allocator is not copyable, but standard
       *       allocators are typically copyable. Move semantics are fully supported.
       */
      template <typename Allocator = std::allocator<LumexStacktraceEntry>> class LumexBasicStacktrace
      {
        /**
         * @brief Friend declaration for the `detail::capture_stacktrace` function.
         * @details This allows the `capture_stacktrace` function to access the private
         *          constructor `LumexBasicStacktrace(container_type &&entries)`
         *          to efficiently construct a `LumexBasicStacktrace` object from a
         *          rvalue reference to its internal container.
         * @tparam A The allocator type used by the friendly function.
         */
        template <typename A>
        friend LumexBasicStacktrace<A> detail::capture_stacktrace(size_t, size_t, A const &) noexcept;

      public:
        /// @brief The type of elements stored in the stacktrace, which is `LumexStacktraceEntry`.
        using value_type = LumexStacktraceEntry;
        /// @brief The allocator type used by this stacktrace container.
        using allocator_type = Allocator;
        /// @brief The unsigned integer type used for sizes and counts.
        using size_type = typename std::allocator_traits<Allocator>::size_type;
        /// @brief The signed integer type used for differences between iterators.
        using difference_type = typename std::allocator_traits<Allocator>::difference_type;
        /// @brief A reference to an element in the stacktrace.
        using reference = value_type &;
        /// @brief A constant reference to an element in the stacktrace.
        using const_reference = value_type const &;
        /// @brief A pointer to an element in the stacktrace.
        using pointer = typename std::allocator_traits<Allocator>::pointer;
        /// @brief A constant pointer to an element in the stacktrace.
        using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;

        /// @brief The underlying container type used to store stacktrace entries, typically `std::vector`.
        using container_type = std::vector<value_type, allocator_type>;

        /// @brief Iterator type for traversing the stacktrace entries (constant).
        using iterator = typename container_type::const_iterator;
        /// @brief Constant iterator type for traversing the stacktrace entries.
        using const_iterator = typename container_type::const_iterator;
        /// @brief Reverse iterator type for traversing the stacktrace entries in reverse (constant).
        using reverse_iterator = typename container_type::const_reverse_iterator;
        /// @brief Constant reverse iterator type for traversing the stacktrace entries in reverse.
        using const_reverse_iterator = typename container_type::const_reverse_iterator;

      private:
        /// @brief The internal container holding the stacktrace entries.
        container_type m_entries;

        /**
         * @brief Private constructor for `LumexBasicStacktrace`.
         * @details This constructor is used by `detail::capture_stacktrace` to efficiently
         *          initialize the stacktrace object by moving a pre-populated container of entries.
         * @param[in] entries An rvalue reference to a `container_type` holding the stacktrace entries.
         */
        explicit LumexBasicStacktrace(container_type &&entries) : m_entries(std::move(entries)) {}

      public:
        /**
         * @brief Default constructor for `LumexBasicStacktrace`.
         * @details Constructs an empty stacktrace. The `noexcept` specification
         *          depends on the `container_type`'s default constructor.
         */
        LumexBasicStacktrace() noexcept(noexcept(container_type())) = default;

        /**
         * @brief Constructs a `LumexBasicStacktrace` with a specific allocator.
         * @details Constructs an empty stacktrace using the provided allocator.
         * @param[in] alloc The allocator to use for the internal container.
         * @note This constructor is `noexcept` because `std::vector`'s allocator-aware
         *       constructor is `noexcept`.
         */
        explicit LumexBasicStacktrace(allocator_type const &alloc) noexcept : m_entries(alloc) {}

        /**
         * @brief Copy constructor for `LumexBasicStacktrace`.
         * @details Performs a deep copy of the `m_entries` container.
         */
        LumexBasicStacktrace(LumexBasicStacktrace const &) = default;
        /**
         * @brief Move constructor for `LumexBasicStacktrace`.
         * @details Efficiently moves the resources from another `LumexBasicStacktrace` object.
         * @note This constructor is `noexcept` because `std::vector`'s move constructor is `noexcept`.
         */
        LumexBasicStacktrace(LumexBasicStacktrace &&) noexcept = default;
        /**
         * @brief Copy assignment operator for `LumexBasicStacktrace`.
         * @details Assigns the contents of another `LumexBasicStacktrace` via deep copy.
         */
        LumexBasicStacktrace &operator=(LumexBasicStacktrace const &) = default;
        /**
         * @brief Move assignment operator for `LumexBasicStacktrace`.
         * @details Efficiently moves the resources from another `LumexBasicStacktrace` object.
         * @note This operator is `noexcept` because `std::vector`'s move assignment operator is `noexcept`.
         */
        LumexBasicStacktrace &operator=(LumexBasicStacktrace &&) noexcept = default;
        /**
         * @brief Default destructor for `LumexBasicStacktrace`.
         * @details Destroys the internal `m_entries` container, releasing all allocated resources.
         */
        ~LumexBasicStacktrace() = default;

        /**
         * @brief Captures the current call stack.
         * @details This static factory method creates a `LumexBasicStacktrace` object
         *          representing the current call stack. It delegates the actual capture
         *          to the platform-specific `detail::capture_stacktrace` function,
         *          automatically skipping the `current` function itself and potentially
         *          one more frame from `LumexBasicStacktrace::current`'s caller.
         * @param skip The number of frames to skip from the top of the call stack,
         *             in addition to the `current` function itself. Defaults to 1.
         * @param max_depth The maximum number of frames to capture. Defaults to all available frames.
         * @param alloc The allocator to use for the new stacktrace object. Defaults to
         * `std::allocator<LumexStacktraceEntry>()`.
         * @return A `LumexBasicStacktrace` object containing the captured frames.
         * @note This method is `noexcept` as `detail::capture_stacktrace` is `noexcept`.
         */
        static LumexBasicStacktrace
        current(size_type skip = 1, size_type max_depth = static_cast<size_type>(-1),
                allocator_type const &alloc = allocator_type()) noexcept
        {
          return detail::capture_stacktrace<Allocator>(skip + 1, max_depth, alloc);
        }

        /**
         * @brief Returns a copy of the allocator used by the internal container.
         * @return A copy of the `allocator_type` object.
         * @note This method is `noexcept`.
         */
        allocator_type
        get_allocator() const noexcept
        {
          return m_entries.get_allocator();
        }

        /**
         * @brief Returns a constant iterator to the beginning of the stacktrace.
         * @return A `const_iterator` pointing to the first `LumexStacktraceEntry`.
         * @note This method is `noexcept`.
         */
        const_iterator
        begin() const noexcept
        {
          return m_entries.begin();
        }
        /**
         * @brief Returns a constant iterator to the end of the stacktrace.
         * @return A `const_iterator` pointing one past the last `LumexStacktraceEntry`.
         * @note This method is `noexcept`.
         */
        const_iterator
        end() const noexcept
        {
          return m_entries.end();
        }
        /**
         * @brief Returns a constant iterator to the beginning of the stacktrace (C++11 alias).
         * @return A `const_iterator` pointing to the first `LumexStacktraceEntry`.
         * @note This method is `noexcept`.
         */
        const_iterator
        cbegin() const noexcept
        {
          return m_entries.cbegin();
        }
        /**
         * @brief Returns a constant iterator to the end of the stacktrace (C++11 alias).
         * @return A `const_iterator` pointing one past the last `LumexStacktraceEntry`.
         * @note This method is `noexcept`.
         */
        const_iterator
        cend() const noexcept
        {
          return m_entries.cend();
        }
        /**
         * @brief Returns a constant reverse iterator to the reverse beginning of the stacktrace.
         * @return A `const_reverse_iterator` pointing to the last `LumexStacktraceEntry`.
         * @note This method is `noexcept`.
         */
        const_reverse_iterator
        rbegin() const noexcept
        {
          return m_entries.rbegin();
        }
        /**
         * @brief Returns a constant reverse iterator to the reverse end of the stacktrace.
         * @return A `const_reverse_iterator` pointing one before the first `LumexStacktraceEntry`.
         * @note This method is `noexcept`.
         */
        const_reverse_iterator
        rend() const noexcept
        {
          return m_entries.rend();
        }
        /**
         * @brief Returns a constant reverse iterator to the reverse beginning of the stacktrace (C++11 alias).
         * @return A `const_reverse_iterator` pointing to the last `LumexStacktraceEntry`.
         * @note This method is `noexcept`.
         */
        const_reverse_iterator
        crbegin() const noexcept
        {
          return m_entries.crbegin();
        }
        /**
         * @brief Returns a constant reverse iterator to the reverse end of the stacktrace (C++11 alias).
         * @return A `const_reverse_iterator` pointing one before the first `LumexStacktraceEntry`.
         * @note This method is `noexcept`.
         */
        const_reverse_iterator
        crend() const noexcept
        {
          return m_entries.crend();
        }

        /**
         * @brief Checks if the stacktrace contains no entries.
         * @return True if the stacktrace is empty, false otherwise.
         * @note This method is `noexcept`.
         */
        bool
        empty() const noexcept
        {
          return m_entries.empty();
        }
        /**
         * @brief Returns the number of entries in the stacktrace.
         * @return The number of `LumexStacktraceEntry` objects in the stacktrace.
         * @note This method is `noexcept`.
         */
        size_type
        size() const noexcept
        {
          return m_entries.size();
        }
        /**
         * @brief Returns the maximum possible number of entries that can be stored in the stacktrace.
         * @return The maximum size of the underlying container.
         * @note This method is `noexcept`.
         */
        size_type
        max_size() const noexcept
        {
          return m_entries.max_size();
        }

        /**
         * @brief Provides constant access to the element at the specified position.
         * @param pos The zero-based index of the element to access.
         * @return A constant reference to the `LumexStacktraceEntry` at `pos`.
         * @warning No bounds checking is performed; accessing elements out of range
         *          results in undefined behavior.
         */
        const_reference
        operator[](size_type pos) const
        {
          return m_entries[pos];
        }
        /**
         * @brief Provides constant access to the element at the specified position with bounds checking.
         * @param pos The zero-based index of the element to access.
         * @return A constant reference to the `LumexStacktraceEntry` at `pos`.
         * @throws std::out_of_range If `pos` is greater than or equal to `size()`.
         */
        const_reference
        at(size_type pos) const
        {
          return m_entries.at(pos);
        }

        /**
         * @brief Exchanges the contents of the stacktrace with another stacktrace.
         * @param other The other `LumexBasicStacktrace` object to swap contents with.
         * @note This operation is `noexcept` if the `container_type`'s `swap` method is `noexcept`.
         */
        void
        swap(LumexBasicStacktrace &other) noexcept(noexcept(m_entries.swap(other.m_entries)))
        {
          m_entries.swap(other.m_entries);
        }
      };

#ifdef _WIN32
  #pragma warning(pop)
#endif

      /**
       * @brief Alias for `LumexBasicStacktrace` using the default `std::allocator`.
       * @details This provides a convenient type name for the most common use case
       *          of the stacktrace class.
       */
      using LumexStacktrace = LumexBasicStacktrace<std::allocator<LumexStacktraceEntry>>;

      /**
       * @brief Compares two `LumexBasicStacktrace` objects for equality.
       * @details Two stacktraces are considered equal if they have the same number of entries
       *          and all corresponding entries are equal (based on `LumexStacktraceEntry::operator==`).
       * @tparam Allocator1 The allocator type of the left-hand side stacktrace.
       * @tparam Allocator2 The allocator type of the right-hand side stacktrace.
       * @param lhs The left-hand side `LumexBasicStacktrace` object.
       * @param rhs The right-hand side `LumexBasicStacktrace` object.
       * @return True if the stacktraces are equal, false otherwise.
       * @note This operator is `noexcept`.
       */
      template <typename Allocator1, typename Allocator2>
      bool
      operator==(LumexBasicStacktrace<Allocator1> const &lhs, LumexBasicStacktrace<Allocator2> const &rhs) noexcept
      {
        if(lhs.size() != rhs.size()) return false;
        return std::equal(lhs.begin(), lhs.end(), rhs.begin());
      }

      /**
       * @brief Compares two `LumexBasicStacktrace` objects for inequality.
       * @details This is the logical negation of `operator==`.
       * @tparam Allocator1 The allocator type of the left-hand side stacktrace.
       * @tparam Allocator2 The allocator type of the right-hand side stacktrace.
       * @param lhs The left-hand side `LumexBasicStacktrace` object.
       * @param rhs The right-hand side `LumexBasicStacktrace` object.
       * @return True if the stacktraces are not equal, false otherwise.
       * @note This operator is `noexcept`.
       */
      template <typename Allocator1, typename Allocator2>
      bool
      operator!=(LumexBasicStacktrace<Allocator1> const &lhs, LumexBasicStacktrace<Allocator2> const &rhs) noexcept
      {
        return !(lhs == rhs);
      }

      /**
       * @brief Lexicographically compares two `LumexBasicStacktrace` objects.
       * @details Compares stacktraces element by element using `LumexStacktraceEntry::operator<`.
       * @tparam Allocator1 The allocator type of the left-hand side stacktrace.
       * @tparam Allocator2 The allocator type of the right-hand side stacktrace.
       * @param lhs The left-hand side `LumexBasicStacktrace` object.
       * @param rhs The right-hand side `LumexBasicStacktrace` object.
       * @return True if `lhs` is lexicographically less than `rhs`, false otherwise.
       * @note This operator is `noexcept`.
       */
      template <typename Allocator1, typename Allocator2>
      bool
      operator<(LumexBasicStacktrace<Allocator1> const &lhs, LumexBasicStacktrace<Allocator2> const &rhs) noexcept
      {
        return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
      }

      /**
       * @brief Compares two `LumexBasicStacktrace` objects for less than or equal to.
       * @details This is the logical negation of `operator>`.
       * @tparam Allocator1 The allocator type of the left-hand side stacktrace.
       * @tparam Allocator2 The allocator type of the right-hand side stacktrace.
       * @param lhs The left-hand side `LumexBasicStacktrace` object.
       * @param rhs The right-hand side `LumexBasicStacktrace` object.
       * @return True if `lhs` is lexicographically less than or equal to `rhs`, false otherwise.
       * @note This operator is `noexcept`.
       */
      template <typename Allocator1, typename Allocator2>
      bool
      operator<=(LumexBasicStacktrace<Allocator1> const &lhs, LumexBasicStacktrace<Allocator2> const &rhs) noexcept
      {
        return !(rhs < lhs);
      }

      /**
       * @brief Compares two `LumexBasicStacktrace` objects for greater than.
       * @details This is equivalent to `rhs < lhs`.
       * @tparam Allocator1 The allocator type of the left-hand side stacktrace.
       * @tparam Allocator2 The allocator type of the right-hand side stacktrace.
       * @param lhs The left-hand side `LumexBasicStacktrace` object.
       * @param rhs The right-hand side `LumexBasicStacktrace` object.
       * @return True if `lhs` is lexicographically greater than `rhs`, false otherwise.
       * @note This operator is `noexcept`.
       */
      template <typename Allocator1, typename Allocator2>
      bool
      operator>(LumexBasicStacktrace<Allocator1> const &lhs, LumexBasicStacktrace<Allocator2> const &rhs) noexcept
      {
        return rhs < lhs;
      }

      /**
       * @brief Compares two `LumexBasicStacktrace` objects for greater than or equal to.
       * @details This is the logical negation of `operator<`.
       * @tparam Allocator1 The allocator type of the left-hand side stacktrace.
       * @tparam Allocator2 The allocator type of the right-hand side stacktrace.
       * @param lhs The left-hand side `LumexBasicStacktrace` object.
       * @param rhs The right-hand side `LumexBasicStacktrace` object.
       * @return True if `lhs` is lexicographically greater than or equal to `rhs`, false otherwise.
       * @note This operator is `noexcept`.
       */
      template <typename Allocator1, typename Allocator2>
      bool
      operator>=(LumexBasicStacktrace<Allocator1> const &lhs, LumexBasicStacktrace<Allocator2> const &rhs) noexcept
      {
        return !(lhs < rhs);
      }

      /**
       * @brief Global `swap` function for `LumexBasicStacktrace`.
       * @details This non-member `swap` function provides an efficient way to exchange
       *          the contents of two `LumexBasicStacktrace` objects, utilizing the
       *          member `swap` function of the underlying container.
       * @tparam Allocator The allocator type of the stacktrace objects.
       * @param lhs The first `LumexBasicStacktrace` object.
       * @param rhs The second `LumexBasicStacktrace` object.
       * @note This function is `noexcept` if the underlying container's `swap` is `noexcept`.
       */
      template <typename Allocator>
      void
      swap(LumexBasicStacktrace<Allocator> &lhs, LumexBasicStacktrace<Allocator> &rhs) noexcept(noexcept(lhs.swap(rhs)))
      {
        lhs.swap(rhs);
      }

      /**
       * @brief Converts a `LumexBasicStacktrace` object to its string representation.
       * @details This function iterates through each entry in the stacktrace and
       *          formats it into a multi-line string, with each line representing
       *          a stack frame, prefixed with its frame number.
       * @tparam Allocator The allocator type of the stacktrace.
       * @param stacktrace The `LumexBasicStacktrace` object to convert.
       * @return A `std::string` containing the formatted stacktrace.
       */
      template <typename Allocator>
      std::string
      to_string(LumexBasicStacktrace<Allocator> const &stacktrace)
      {
        std::string result;
        result.reserve(stacktrace.size() * detail::kDefaultMaxFrames); // Pre-allocate for efficiency

        for(size_t i = 0; i < stacktrace.size(); ++i)
        {
          result += std::to_string(i);
          result += "# ";
          result += stacktrace[i].description();
          result += '\n';
        }

        return result;
      }

      /**
       * @brief Overloads the `operator<<` for `std::basic_ostream` to print a `LumexBasicStacktrace`.
       * @details This allows `LumexBasicStacktrace` objects to be easily printed to
       *          any `std::basic_ostream` (e.g., `std::cout`, `std::cerr`) using
       *          the `to_string` conversion.
       * @tparam CharT The character type of the output stream.
       * @tparam Traits The character traits of the output stream.
       * @tparam Allocator The allocator type of the stacktrace.
       * @param ostream The output stream to write to.
       * @param stacktrace The `LumexBasicStacktrace` object to print.
       * @return A reference to the output stream.
       */
      template <typename CharT, typename Traits, typename Allocator>
      std::basic_ostream<CharT, Traits> &
      operator<<(std::basic_ostream<CharT, Traits> &ostream, LumexBasicStacktrace<Allocator> const &stacktrace)
      {
        return ostream << to_string(stacktrace);
      }

      /**
       * @brief Partial specialization of `std::hash` for `LumexBasicStacktrace`.
       * @details This struct provides a hash function for `LumexBasicStacktrace` objects,
       *          enabling their use in hash-based containers like `std::unordered_set`
       *          and `std::unordered_map`. The hash is computed based on the native
       *          handles (addresses) of the stack entries, combined using a golden ratio
       *          hash combination technique.
       * @tparam Allocator The allocator type of the stacktrace.
       */
      template <typename Allocator> struct hash;

      template <typename Allocator> struct hash<LumexBasicStacktrace<Allocator>> {
        /**
         * @brief Computes the hash value for a `LumexBasicStacktrace` object.
         * @param stacktrace The `LumexBasicStacktrace` object to hash.
         * @return A `size_t` representing the hash value of the stacktrace.
         * @note This operator is `noexcept`.
         */
        size_t
        operator()(LumexBasicStacktrace<Allocator> const &stacktrace) const noexcept
        {
          size_t seed = 0;
          for(auto const &entry : stacktrace)
          {
            seed ^= std::hash<void *>()(entry.native_handle()) + detail::kHashGoldenRatio
                    + (seed << detail::kHashLeftShift) + (seed >> detail::kHashRightShift);
          }
          return seed;
        }
      };
    } // namespace Stacktrace
  } // namespace Core
} // namespace Lumex

// Global type aliases for convenience
/**
 * @brief Global alias for `Lumex::Core::Stacktrace::LumexStacktrace`.
 * @details This provides a simplified name for the default stacktrace type,
 *          making it easier to use without full namespace qualification.
 */
using LumexStacktrace = Lumex::Core::Stacktrace::LumexStacktrace;
/**
 * @brief Global alias for `Lumex::Core::Stacktrace::LumexStacktraceEntry`.
 * @details This provides a simplified name for the stacktrace entry type,
 *          making it easier to use without full namespace qualification.
 */
using LumexStacktraceEntry = Lumex::Core::Stacktrace::LumexStacktraceEntry;

// Bring key functions into global namespace for convenience
/**
 * @brief Brings `Lumex::Core::Stacktrace::to_string` into the global namespace.
 * @details This allows `to_string` to be called without full namespace qualification
 *          when used with `LumexStacktrace` objects, improving readability.
 */
using Lumex::Core::Stacktrace::to_string;

#endif // !LUMEX_STACKTRACE_HPP
