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

/**
 * @file LumexStacktrace.hpp
 * @brief Defines the lumex_basic_stacktrace class and related utilities for
 * capturing and symbolizing call stacks.
 * @details This header provides a cross-platform mechanism for generating and
 * manipulating stack traces, which are crucial for debugging, error reporting,
 * and understanding program execution flow. It includes platform-specific
 *          implementations for Windows (using DbgHelp) and POSIX systems
 * (using backtrace and dladdr; a frame without an exported symbol is named by
 * its module and the address inside it, for offline symbolization). The core
 * `lumex_basic_stacktrace`
 * template class provides a container for `lumex_stacktrace_entry` objects,
 * representing individual frames in the call stack. It is designed to be
 * allocator-aware and exception-safe.
 */
#ifndef LUMEX_CORE_EXCEPTIONS_STACKTRACE_STACKTRACE_HPP
#define LUMEX_CORE_EXCEPTIONS_STACKTRACE_STACKTRACE_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#if __has_warning("-Wnrvo")
#pragma clang diagnostic ignored "-Wnrvo"
#endif
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#if __has_warning("-Wvariadic-macro-arguments-omitted")
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#endif
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#include <algorithm> // std::min, std::max, std::equal, std::lexicographical_compare
#include <cstdio>     // std::snprintf
#include <cstdlib>    // std::free
#include <cstring>    // std::strlen, std::strrchr
#include <functional> // std::hash
#include <memory>     // std::unique_ptr
#include <mutex>      // std::mutex
#include <sstream>    // std::istringstream
#include <string>     // std::string, std::to_string
#include <vector>     // std::vector

// ****************** Platform-specific includes ****************** //
// Gated on compiler macros so they can precede the project headers (the
// code below still branches on LUMEX_OS_*).
#if defined(_WIN32)
#include <windows.h> // This include should be 1st

#include <dbghelp.h> // This include should be 2nd, because it uses types from windows.h
#else
#include <cxxabi.h>   // abi::__cxa_demangle
#include <dlfcn.h>    // dladdr, Dl_info
#include <execinfo.h> // backtrace
#endif

#include "lumex/core/exceptions/stacktrace/LumexStacktraceEntry.hpp"
#include "lumex/core/utility/LumexUtility"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(_WIN32)
#pragma comment(lib, "dbghelp.lib")
#endif

// *********************************************************************** //

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace exceptions
{
namespace stacktrace
{
/**
 * @brief Contains classes and utilities related to stack trace management and
 * exception handling.
 * @details This namespace groups all components for capturing, symbolizing,
 * and representing call stacks within the LumexCore library, providing tools
 * essential for diagnostics and error reporting.
 */
template <typename Allocator> class lumex_basic_stacktrace;

/**
 * @brief Internal detail namespace for platform-specific stacktrace
 * functionalities.
 * @details This namespace encapsulates helper functions and constants that are
 *          used internally by `lumex_basic_stacktrace` for platform-dependent
 *          operations like capturing raw frames and resolving symbol
 * information.
 */
namespace detail
{
LUMEX_CONSTEXPR short const kHashRightShift
    = 2; ///< Constant for right bit shift operation in hash calculation.
LUMEX_CONSTEXPR short const kHashLeftShift
    = 6; ///< Constant for left bit shift operation in hash calculation.
LUMEX_CONSTEXPR short const kDefaultAddrStrSize
    = 32; ///< Default buffer size for address string representations.
LUMEX_CONSTEXPR short const kDefaultMaxFrames
    = 128; ///< Default maximum number of frames to capture in a stacktrace.
LUMEX_CONSTEXPR std::size_t kHashGoldenRatio
    = 0x9e3779b9U; ///< Golden ratio constant used in hash calculation to
                   ///< provide good distribution.

#if defined(LUMEX_OS_WINDOWS)
/// @brief Global mutex for synchronizing access to DbgHelp API functions.
/// @details DbgHelp library is not thread-safe, so all calls to its functions
///          must be protected by this mutex. It is extern because its
///          definition is in `LumexStacktrace.cpp`.
extern std::mutex
    g_dbghelp_mutex; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

/**
 * @brief Helper class for RAII-style initialization and deinitialization of
 * DbgHelp.
 * @details This class ensures that `SymInitialize` is called once when the
 * first `dbg_help_initializer` object is created, and symbol options are set.
 * It uses a static flag `s_initialized` and `g_dbghelp_mutex` to ensure
 *          thread-safe, one-time initialization.
 */
class dbg_help_initializer
{
private:
  /// @brief Static flag indicating if DbgHelp has been initialized.
  static bool s_initialized;

public:
  /**
   * @brief Constructs a `dbg_help_initializer` object.
   * @details Attempts to initialize DbgHelp if it hasn't been initialized yet.
   *          This operation is thread-safe, protected by `g_dbghelp_mutex`.
   * @note This constructor is responsible for calling `SymInitialize` and
   * `SymSetOptions`.
   */
  dbg_help_initializer ();

  /**
   * @brief Default destructor for `dbg_help_initializer`.
   * @details No explicit deinitialization of DbgHelp is performed here as
   * `SymCleanup` is usually called at process exit or when the last DbgHelp
   * user is done.
   */
  ~dbg_help_initializer () = default;

  /**
   * @brief Copy constructor for `dbg_help_initializer`.
   * @details Performs a shallow copy of the `dbg_help_initializer` object.
   */
  dbg_help_initializer (dbg_help_initializer const &) = default;

  /**
   * @brief Move constructor for `dbg_help_initializer`.
   * @details Performs a move of the `dbg_help_initializer` object.
   */
  dbg_help_initializer (dbg_help_initializer &&) LUMEX_NOEXCEPT = default;

  /**
   * @brief Copy assignment operator for `dbg_help_initializer`.
   * @details Performs a copy assignment of the `dbg_help_initializer` object.
   */
  dbg_help_initializer &operator= (dbg_help_initializer const &) = default;

  /**
   * @brief Move assignment operator for `dbg_help_initializer`.
   * @details Performs a move assignment of the `dbg_help_initializer` object.
   */
  dbg_help_initializer &operator= (dbg_help_initializer &&) LUMEX_NOEXCEPT
      = default;

  /**
   * @brief Checks if DbgHelp has been successfully initialized.
   * @return True if DbgHelp is initialized, false otherwise.
   * @note This method is `noexcept` as it only reads a static boolean flag.
   */
  bool is_initialized () const LUMEX_NOEXCEPT;
};

/**
 * @brief Platform-specific stacktrace capture implementation for Windows.
 * @details This function captures the current call stack using
 * `CaptureStackBackTrace` Win32 API function. It then converts the raw
 * addresses into a vector of `lumex_stacktrace_entry` objects.
 * @tparam Allocator The allocator type for the resulting stacktrace.
 * @param skip Number of frames to skip from the top of the call stack (e.g.,
 * `capture_stacktrace` itself).
 * @param max_depth Maximum number of frames to capture.
 * @param alloc The allocator to use for the internal container.
 * @return A `lumex_basic_stacktrace` containing the captured frames. Returns
 * an empty stacktrace if DbgHelp is not initialized or no frames are captured.
 * @note This function is `noexcept` and designed to be exception-safe.
 */
template <typename Allocator>
stacktrace::lumex_basic_stacktrace<Allocator>
capture_stacktrace (std::size_t skip, std::size_t max_depth,
                    Allocator const &alloc) LUMEX_NOEXCEPT;

/**
 * @brief Resolves symbol information (function name, source file, line number)
 * for a given address on Windows.
 * @details This function uses DbgHelp API functions (`SymFromAddr`,
 * `SymGetLineFromAddr64`, `SymGetModuleInfo64`, `UnDecorateSymbolName`) to
 * retrieve detailed information about a stack address. It attempts to demangle
 * C++ function names and provides fallbacks if full symbol info is not
 * available.
 * @param address The `void*` address of the stack frame to resolve.
 * @param function_name Output parameter for the resolved function name.
 * @param source_file Output parameter for the source file path.
 * @param line_number Output parameter for the line number in the source file.
 * @return True if any symbol information was successfully resolved, false
 * otherwise.
 * @note This function is thread-safe due to the use of `g_dbghelp_mutex`.
 * @note Does not throw.
 */
bool resolve_symbol_info (void *address, std::string &function_name,
                          std::string &source_file,
                          std::uint32_t &line_number) LUMEX_NOEXCEPT;
#else
/**
 * @brief Demangles a C++ mangled symbol name on POSIX systems.
 * @details This function uses `abi::__cxa_demangle` from the GNU C++ ABI
 * library to convert a compiler-mangled C++ symbol name into a human-readable
 * form.
 * @param mangled The null-terminated C-string of the mangled symbol name.
 * @return A `std::string` containing the demangled symbol name. If demangling
 * fails, the original mangled name is returned. Returns an empty string if
 * `mangled` is `nullptr`.
 */
std::string demangle_symbol (char const *mangled);

/**
 * @brief Platform-specific stacktrace capture implementation for POSIX
 * systems.
 * @details This function captures the current call stack using the `backtrace`
 *          function. It then converts the raw addresses into a vector of
 *          `lumex_stacktrace_entry` objects. It includes logic to handle cases
 *          where optimizations might reduce frame count (e.g., Release
 * builds).
 * @tparam Allocator The allocator type for the resulting stacktrace.
 * @param skip Number of frames to skip from the top of the call stack.
 * @param max_depth Maximum number of frames to capture.
 * @param alloc The allocator to use for the internal container.
 * @return A `lumex_basic_stacktrace` containing the captured frames. Returns
 * an empty stacktrace if no frames are captured.
 * @note This function is `noexcept` and designed to be exception-safe.
 */
template <typename Allocator>
stacktrace::lumex_basic_stacktrace<Allocator>
capture_stacktrace (std::size_t skip, std::size_t max_depth,
                    Allocator const &alloc) LUMEX_NOEXCEPT;

/**
 * @brief Resolves the description of a given address on POSIX systems.
 * @details An exported symbol is named through `dladdr` and demangled, and
 * the module with the address inside it follows in parentheses
 * ("name (libfoo.so.1+0x1A2B)"); a frame without one is described by its
 * module and offset alone ("app+0x401234"), which addr2line or gdb resolve
 * offline with that module or its separate debug file. No process is started
 * and no debug information is read, so source file and line are not
 * resolved here.
 * @param address The `void*` address of the stack frame to resolve.
 * @param function_name Output parameter for the description.
 * @param source_file Output parameter for the source file path (always
 * cleared on POSIX).
 * @param line_number Output parameter for the line number (always 0 on
 * POSIX).
 * @return True if a symbol or a module holds the address, false otherwise.
 * @note Does not throw.
 */
bool resolve_symbol_info (void *address, std::string &function_name,
                          std::string &source_file,
                          std::uint32_t &line_number) LUMEX_NOEXCEPT;
#endif
} // namespace detail

// Suppress C4251 warnings for STL containers in DLL interface for the entire
// template class C4251: 'class' : class 'type' needs to have dll-interface to
// be used by clients of class 'class' This warning is common when
// `std::vector` (or other STL containers) is a member of a class exported from
// a DLL, because `std::vector` itself is not exported. It's usually safe to
// ignore if the client code is also compiled with the same C++ standard
// library version.
#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

/**
 * @brief A basic, allocator-aware class for representing a call stack
 * (stacktrace).
 * @details This template class provides a collection of
 * `lumex_stacktrace_entry` objects, each representing a single frame in a call
 * stack. It supports custom allocators and provides standard container-like
 * accessors (iterators, size, element access). The actual stack capture is
 * delegated to platform-specific helper functions.
 * @tparam Allocator The allocator type to use for managing the underlying
 * container of `lumex_stacktrace_entry` objects. Defaults to
 * `std::allocator<lumex_stacktrace_entry>`.
 * @note This class is non-copyable if the allocator is not copyable, but
 * standard allocators are typically copyable. Move semantics are fully
 * supported.
 */
template <typename Allocator = std::allocator<lumex_stacktrace_entry>>
class lumex_basic_stacktrace
{
  /**
   * @brief Friend declaration for the `detail::capture_stacktrace` function.
   * @details This allows the `capture_stacktrace` function to access the
   * private constructor `lumex_basic_stacktrace(container_type &&entries)` to
   * efficiently construct a `lumex_basic_stacktrace` object from a rvalue
   * reference to its internal container.
   * @tparam A The allocator type used by the friendly function.
   */
  template <typename A>
  friend lumex_basic_stacktrace<A>
  detail::capture_stacktrace (std::size_t, std::size_t,
                              A const &) LUMEX_NOEXCEPT;

public:
  /// @brief The type of elements stored in the stacktrace, which is
  /// `lumex_stacktrace_entry`.
  using value_type = lumex_stacktrace_entry;
  /// @brief The allocator type used by this stacktrace container.
  using allocator_type = Allocator;
  /// @brief The unsigned integer type used for sizes and counts.
  using size_type = typename std::allocator_traits<Allocator>::size_type;
  /// @brief The signed integer type used for differences between iterators.
  using difference_type =
      typename std::allocator_traits<Allocator>::difference_type;
  /// @brief A reference to an element in the stacktrace.
  using reference = value_type &;
  /// @brief A constant reference to an element in the stacktrace.
  using const_reference = value_type const &;
  /// @brief A pointer to an element in the stacktrace.
  using pointer = typename std::allocator_traits<Allocator>::pointer;
  /// @brief A constant pointer to an element in the stacktrace.
  using const_pointer =
      typename std::allocator_traits<Allocator>::const_pointer;

  /// @brief The underlying container type used to store stacktrace entries,
  /// typically `std::vector`.
  using container_type = std::vector<value_type, allocator_type>;

  /// @brief Iterator type for traversing the stacktrace entries (constant).
  using iterator = typename container_type::const_iterator;
  /// @brief Constant iterator type for traversing the stacktrace entries.
  using const_iterator = typename container_type::const_iterator;
  /// @brief Reverse iterator type for traversing the stacktrace entries in
  /// reverse (constant).
  using reverse_iterator = typename container_type::const_reverse_iterator;
  /// @brief Constant reverse iterator type for traversing the stacktrace
  /// entries in reverse.
  using const_reverse_iterator =
      typename container_type::const_reverse_iterator;

private:
  /// @brief The internal container holding the stacktrace entries.
  container_type m_entries;

  /**
   * @brief Private constructor for `lumex_basic_stacktrace`.
   * @details This constructor is used by `detail::capture_stacktrace` to
   * efficiently initialize the stacktrace object by moving a pre-populated
   * container of entries.
   * @param[in] entries An rvalue reference to a `container_type` holding the
   * stacktrace entries.
   */
  explicit lumex_basic_stacktrace (container_type &&entries)
      : m_entries (std::move (entries))
  {
  }

public:
  /**
   * @brief Default constructor for `lumex_basic_stacktrace`.
   * @details Constructs an empty stacktrace. The `noexcept` specification
   *          depends on the `container_type`'s default constructor.
   */
  lumex_basic_stacktrace () LUMEX_NOEXCEPT_IF (noexcept (container_type ()))
      = default;

  /**
   * @brief Constructs a `lumex_basic_stacktrace` with a specific allocator.
   * @details Constructs an empty stacktrace using the provided allocator.
   * @param[in] alloc The allocator to use for the internal container.
   * @note This constructor is `noexcept` because `std::vector`'s
   * allocator-aware constructor is `noexcept`.
   */
  explicit lumex_basic_stacktrace (allocator_type const &alloc) LUMEX_NOEXCEPT
      : m_entries (alloc)
  {
  }

  /**
   * @brief Copy constructor for `lumex_basic_stacktrace`.
   * @details Performs a deep copy of the `m_entries` container.
   */
  lumex_basic_stacktrace (lumex_basic_stacktrace const &) = default;
  /**
   * @brief Move constructor for `lumex_basic_stacktrace`.
   * @details Efficiently moves the resources from another
   * `lumex_basic_stacktrace` object.
   * @note This constructor is `noexcept` because `std::vector`'s move
   * constructor is `noexcept`.
   */
  lumex_basic_stacktrace (lumex_basic_stacktrace &&) LUMEX_NOEXCEPT = default;
  /**
   * @brief Copy assignment operator for `lumex_basic_stacktrace`.
   * @details Assigns the contents of another `lumex_basic_stacktrace` via deep
   * copy.
   */
  lumex_basic_stacktrace &operator= (lumex_basic_stacktrace const &) = default;
  /**
   * @brief Move assignment operator for `lumex_basic_stacktrace`.
   * @details Efficiently moves the resources from another
   * `lumex_basic_stacktrace` object.
   * @note This operator is `noexcept` because `std::vector`'s move assignment
   * operator is `noexcept`.
   */
  lumex_basic_stacktrace &operator= (lumex_basic_stacktrace &&) LUMEX_NOEXCEPT
      = default;
  /**
   * @brief Default destructor for `lumex_basic_stacktrace`.
   * @details Destroys the internal `m_entries` container, releasing all
   * allocated resources.
   */
  ~lumex_basic_stacktrace () = default;

  /**
   * @brief Captures the current call stack.
   * @details This static factory method creates a `lumex_basic_stacktrace`
   * object representing the current call stack. It delegates the actual
   * capture to the platform-specific `detail::capture_stacktrace` function,
   *          automatically skipping the `current` function itself and
   * potentially one more frame from `LumexBasicstacktrace::current`'s caller.
   * @param skip The number of frames to skip from the top of the call stack,
   *             in addition to the `current` function itself. Defaults to 1.
   * @param max_depth The maximum number of frames to capture. Defaults to all
   * available frames.
   * @param alloc The allocator to use for the new stacktrace object. Defaults
   * to `std::allocator<lumex_stacktrace_entry>()`.
   * @return A `lumex_basic_stacktrace` object containing the captured frames.
   * @note This method is `noexcept` as `detail::capture_stacktrace` is
   * `noexcept`.
   */
  static lumex_basic_stacktrace
  current (size_type skip = 1,
           size_type max_depth = static_cast<size_type> (-1),
           allocator_type const &alloc = allocator_type ()) LUMEX_NOEXCEPT
  {
    return detail::capture_stacktrace<Allocator> (skip + 1, max_depth, alloc);
  }

  /**
   * @brief Returns a copy of the allocator used by the internal container.
   * @return A copy of the `allocator_type` object.
   * @note This method is `noexcept`.
   */
  allocator_type
  get_allocator () const LUMEX_NOEXCEPT
  {
    return m_entries.get_allocator ();
  }

  /**
   * @brief Returns a constant iterator to the beginning of the stacktrace.
   * @return A `const_iterator` pointing to the first `lumex_stacktrace_entry`.
   * @note This method is `noexcept`.
   */
  const_iterator
  begin () const LUMEX_NOEXCEPT
  {
    return m_entries.begin ();
  }
  /**
   * @brief Returns a constant iterator to the end of the stacktrace.
   * @return A `const_iterator` pointing one past the last
   * `lumex_stacktrace_entry`.
   * @note This method is `noexcept`.
   */
  const_iterator
  end () const LUMEX_NOEXCEPT
  {
    return m_entries.end ();
  }
  /**
   * @brief Returns a constant iterator to the beginning of the stacktrace
   * (C++11 alias).
   * @return A `const_iterator` pointing to the first `lumex_stacktrace_entry`.
   * @note This method is `noexcept`.
   */
  const_iterator
  cbegin () const LUMEX_NOEXCEPT
  {
    return m_entries.cbegin ();
  }
  /**
   * @brief Returns a constant iterator to the end of the stacktrace (C++11
   * alias).
   * @return A `const_iterator` pointing one past the last
   * `lumex_stacktrace_entry`.
   * @note This method is `noexcept`.
   */
  const_iterator
  cend () const LUMEX_NOEXCEPT
  {
    return m_entries.cend ();
  }
  /**
   * @brief Returns a constant reverse iterator to the reverse beginning of the
   * stacktrace.
   * @return A `const_reverse_iterator` pointing to the last
   * `lumex_stacktrace_entry`.
   * @note This method is `noexcept`.
   */
  const_reverse_iterator
  rbegin () const LUMEX_NOEXCEPT
  {
    return m_entries.rbegin ();
  }
  /**
   * @brief Returns a constant reverse iterator to the reverse end of the
   * stacktrace.
   * @return A `const_reverse_iterator` pointing one before the first
   * `lumex_stacktrace_entry`.
   * @note This method is `noexcept`.
   */
  const_reverse_iterator
  rend () const LUMEX_NOEXCEPT
  {
    return m_entries.rend ();
  }
  /**
   * @brief Returns a constant reverse iterator to the reverse beginning of the
   * stacktrace (C++11 alias).
   * @return A `const_reverse_iterator` pointing to the last
   * `lumex_stacktrace_entry`.
   * @note This method is `noexcept`.
   */
  const_reverse_iterator
  crbegin () const LUMEX_NOEXCEPT
  {
    return m_entries.crbegin ();
  }
  /**
   * @brief Returns a constant reverse iterator to the reverse end of the
   * stacktrace (C++11 alias).
   * @return A `const_reverse_iterator` pointing one before the first
   * `lumex_stacktrace_entry`.
   * @note This method is `noexcept`.
   */
  const_reverse_iterator
  crend () const LUMEX_NOEXCEPT
  {
    return m_entries.crend ();
  }

  /**
   * @brief Checks if the stacktrace contains no entries.
   * @return True if the stacktrace is empty, false otherwise.
   * @note This method is `noexcept`.
   */
  bool
  empty () const LUMEX_NOEXCEPT
  {
    return m_entries.empty ();
  }
  /**
   * @brief Returns the number of entries in the stacktrace.
   * @return The number of `lumex_stacktrace_entry` objects in the stacktrace.
   * @note This method is `noexcept`.
   */
  size_type
  size () const LUMEX_NOEXCEPT
  {
    return m_entries.size ();
  }
  /**
   * @brief Returns the maximum possible number of entries that can be stored
   * in the stacktrace.
   * @return The maximum size of the underlying container.
   * @note This method is `noexcept`.
   */
  size_type
  max_size () const LUMEX_NOEXCEPT
  {
    return m_entries.max_size ();
  }

  /**
   * @brief Provides constant access to the element at the specified position.
   * @param pos The zero-based index of the element to access.
   * @return A constant reference to the `lumex_stacktrace_entry` at `pos`.
   * @warning No bounds checking is performed; accessing elements out of range
   *          results in undefined behavior.
   */
  const_reference
  operator[] (size_type pos) const
  {
    return m_entries[pos];
  }
  /**
   * @brief Provides constant access to the element at the specified position
   * with bounds checking.
   * @param pos The zero-based index of the element to access.
   * @return A constant reference to the `lumex_stacktrace_entry` at `pos`.
   * @throws std::out_of_range If `pos` is greater than or equal to `size()`.
   */
  const_reference
  at (size_type pos) const
  {
    return m_entries.at (pos);
  }

  /**
   * @brief Exchanges the contents of the stacktrace with another stacktrace.
   * @param other The other `lumex_basic_stacktrace` object to swap contents
   * with.
   * @note This operation is `noexcept` if the `container_type`'s `swap` method
   * is `noexcept`.
   */
  void
  swap (lumex_basic_stacktrace &other)
      LUMEX_NOEXCEPT_IF (noexcept (m_entries.swap (other.m_entries)))
  {
    m_entries.swap (other.m_entries);
  }
};

#ifdef _WIN32
#pragma warning(pop)
#endif

/**
 * @brief Alias for `lumex_basic_stacktrace` using the default
 * `std::allocator`.
 * @details This provides a convenient type name for the most common use case
 *          of the stacktrace class.
 */
using lumex_stacktrace
    = lumex_basic_stacktrace<std::allocator<lumex_stacktrace_entry>>;

/**
 * @brief Compares two `lumex_basic_stacktrace` objects for equality.
 * @details Two stacktraces are considered equal if they have the same number
 * of entries and all corresponding entries are equal (based on
 * `lumex_stacktrace_entry::operator==`).
 * @tparam Allocator1 The allocator type of the left-hand side stacktrace.
 * @tparam Allocator2 The allocator type of the right-hand side stacktrace.
 * @param lhs The left-hand side `lumex_basic_stacktrace` object.
 * @param rhs The right-hand side `lumex_basic_stacktrace` object.
 * @return True if the stacktraces are equal, false otherwise.
 * @note This operator is `noexcept`.
 */
template <typename Allocator1, typename Allocator2>
bool
operator== (lumex_basic_stacktrace<Allocator1> const &lhs,
            lumex_basic_stacktrace<Allocator2> const &rhs) LUMEX_NOEXCEPT
{
  if (lhs.size () != rhs.size ())
    return false;
  return std::equal (lhs.begin (), lhs.end (), rhs.begin ());
}

/**
 * @brief Compares two `lumex_basic_stacktrace` objects for inequality.
 * @details This is the logical negation of `operator==`.
 * @tparam Allocator1 The allocator type of the left-hand side stacktrace.
 * @tparam Allocator2 The allocator type of the right-hand side stacktrace.
 * @param lhs The left-hand side `lumex_basic_stacktrace` object.
 * @param rhs The right-hand side `lumex_basic_stacktrace` object.
 * @return True if the stacktraces are not equal, false otherwise.
 * @note This operator is `noexcept`.
 */
template <typename Allocator1, typename Allocator2>
bool
operator!= (lumex_basic_stacktrace<Allocator1> const &lhs,
            lumex_basic_stacktrace<Allocator2> const &rhs) LUMEX_NOEXCEPT
{
  return !(lhs == rhs);
}

/**
 * @brief Lexicographically compares two `lumex_basic_stacktrace` objects.
 * @details Compares stacktraces element by element using
 * `lumex_stacktrace_entry::operator<`.
 * @tparam Allocator1 The allocator type of the left-hand side stacktrace.
 * @tparam Allocator2 The allocator type of the right-hand side stacktrace.
 * @param lhs The left-hand side `lumex_basic_stacktrace` object.
 * @param rhs The right-hand side `lumex_basic_stacktrace` object.
 * @return True if `lhs` is lexicographically less than `rhs`, false otherwise.
 * @note This operator is `noexcept`.
 */
template <typename Allocator1, typename Allocator2>
bool
operator< (lumex_basic_stacktrace<Allocator1> const &lhs,
           lumex_basic_stacktrace<Allocator2> const &rhs) LUMEX_NOEXCEPT
{
  return std::lexicographical_compare (lhs.begin (), lhs.end (), rhs.begin (),
                                       rhs.end ());
}

/**
 * @brief Compares two `lumex_basic_stacktrace` objects for less than or equal
 * to.
 * @details This is the logical negation of `operator>`.
 * @tparam Allocator1 The allocator type of the left-hand side stacktrace.
 * @tparam Allocator2 The allocator type of the right-hand side stacktrace.
 * @param lhs The left-hand side `lumex_basic_stacktrace` object.
 * @param rhs The right-hand side `lumex_basic_stacktrace` object.
 * @return True if `lhs` is lexicographically less than or equal to `rhs`,
 * false otherwise.
 * @note This operator is `noexcept`.
 */
template <typename Allocator1, typename Allocator2>
bool
operator<= (lumex_basic_stacktrace<Allocator1> const &lhs,
            lumex_basic_stacktrace<Allocator2> const &rhs) LUMEX_NOEXCEPT
{
  return !(rhs < lhs);
}

/**
 * @brief Compares two `lumex_basic_stacktrace` objects for greater than.
 * @details This is equivalent to `rhs < lhs`.
 * @tparam Allocator1 The allocator type of the left-hand side stacktrace.
 * @tparam Allocator2 The allocator type of the right-hand side stacktrace.
 * @param lhs The left-hand side `lumex_basic_stacktrace` object.
 * @param rhs The right-hand side `lumex_basic_stacktrace` object.
 * @return True if `lhs` is lexicographically greater than `rhs`, false
 * otherwise.
 * @note This operator is `noexcept`.
 */
template <typename Allocator1, typename Allocator2>
bool
operator> (lumex_basic_stacktrace<Allocator1> const &lhs,
           lumex_basic_stacktrace<Allocator2> const &rhs) LUMEX_NOEXCEPT
{
  return rhs < lhs;
}

/**
 * @brief Compares two `lumex_basic_stacktrace` objects for greater than or
 * equal to.
 * @details This is the logical negation of `operator<`.
 * @tparam Allocator1 The allocator type of the left-hand side stacktrace.
 * @tparam Allocator2 The allocator type of the right-hand side stacktrace.
 * @param lhs The left-hand side `lumex_basic_stacktrace` object.
 * @param rhs The right-hand side `lumex_basic_stacktrace` object.
 * @return True if `lhs` is lexicographically greater than or equal to `rhs`,
 * false otherwise.
 * @note This operator is `noexcept`.
 */
template <typename Allocator1, typename Allocator2>
bool
operator>= (lumex_basic_stacktrace<Allocator1> const &lhs,
            lumex_basic_stacktrace<Allocator2> const &rhs) LUMEX_NOEXCEPT
{
  return !(lhs < rhs);
}

/**
 * @brief Global `swap` function for `lumex_basic_stacktrace`.
 * @details This non-member `swap` function provides an efficient way to
 * exchange the contents of two `lumex_basic_stacktrace` objects, utilizing the
 *          member `swap` function of the underlying container.
 * @tparam Allocator The allocator type of the stacktrace objects.
 * @param lhs The first `lumex_basic_stacktrace` object.
 * @param rhs The second `lumex_basic_stacktrace` object.
 * @note This function is `noexcept` if the underlying container's `swap` is
 * `noexcept`.
 */
template <typename Allocator>
void
swap (lumex_basic_stacktrace<Allocator> &lhs,
      lumex_basic_stacktrace<Allocator> &rhs)
    LUMEX_NOEXCEPT_IF (noexcept (lhs.swap (rhs)))
{
  lhs.swap (rhs);
}

/**
 * @brief Converts a `lumex_basic_stacktrace` object to its string
 * representation.
 * @details This function iterates through each entry in the stacktrace and
 *          formats it into a multi-line string, with each line representing
 *          a stack frame, prefixed with its frame number.
 * @tparam Allocator The allocator type of the stacktrace.
 * @param stacktrace The `lumex_basic_stacktrace` object to convert.
 * @return A `std::string` containing the formatted stacktrace.
 */
template <typename Allocator>
std::string
to_string (lumex_basic_stacktrace<Allocator> const &stacktrace)
{
  std::string result;
  result.reserve (stacktrace.size ()
                  * detail::kDefaultMaxFrames); // Pre-allocate for efficiency

  for (std::size_t i = 0; i < stacktrace.size (); ++i)
    {
      result += std::to_string (i);
      result += "# ";
      result += stacktrace[i].description ();
      result += '\n';
    }

  return result;
}

/**
 * @brief Overloads the `operator<<` for `std::basic_ostream` to print a
 * `lumex_basic_stacktrace`.
 * @details This allows `lumex_basic_stacktrace` objects to be easily printed
 * to any `std::basic_ostream` (e.g., `std::cout`, `std::cerr`) using the
 * `to_string` conversion.
 * @tparam CharT The character type of the output stream.
 * @tparam Traits The character traits of the output stream.
 * @tparam Allocator The allocator type of the stacktrace.
 * @param ostream The output stream to write to.
 * @param stacktrace The `lumex_basic_stacktrace` object to print.
 * @return A reference to the output stream.
 */
template <typename CharT, typename Traits, typename Allocator>
std::basic_ostream<CharT, Traits> &
operator<< (std::basic_ostream<CharT, Traits> &ostream,
            lumex_basic_stacktrace<Allocator> const &stacktrace)
{
  return ostream << to_string (stacktrace);
}

/**
 * @brief Partial specialization of `std::hash` for `lumex_basic_stacktrace`.
 * @details This struct provides a hash function for `lumex_basic_stacktrace`
 * objects, enabling their use in hash-based containers like
 * `std::unordered_set` and `std::unordered_map`. The hash is computed based on
 * the native handles (addresses) of the stack entries, combined using a golden
 * ratio hash combination technique.
 * @tparam Allocator The allocator type of the stacktrace.
 */
template <typename Allocator> struct hash;

template <typename Allocator> struct hash<lumex_basic_stacktrace<Allocator>>
{
  /**
   * @brief Computes the hash value for a `lumex_basic_stacktrace` object.
   * @param stacktrace The `lumex_basic_stacktrace` object to hash.
   * @return A `std::size_t` representing the hash value of the stacktrace.
   * @note This operator is `noexcept`.
   */
  std::size_t
  operator() (lumex_basic_stacktrace<Allocator> const &stacktrace) const
      LUMEX_NOEXCEPT
  {
    std::size_t seed = 0;
    for (auto const &entry : stacktrace)
      {
        seed ^= std::hash<void *> () (entry.native_handle ())
                + detail::kHashGoldenRatio + (seed << detail::kHashLeftShift)
                + (seed >> detail::kHashRightShift);
      }
    return seed;
  }
};
} // namespace stacktrace
} // namespace exceptions
} // namespace core
} // namespace lumex

// Global type aliases for convenience
/**
 * @brief Global alias for
 * `lumex::core::exceptions::stacktrace::lumex_stacktrace`.
 * @details This provides a simplified name for the default stacktrace type,
 *          making it easier to use without full namespace qualification.
 */
using lumex_stacktrace = lumex::core::exceptions::stacktrace::lumex_stacktrace;
/**
 * @brief Global alias for
 * `lumex::core::exceptions::stacktrace::lumex_stacktrace_entry`.
 * @details This provides a simplified name for the stacktrace entry type,
 *          making it easier to use without full namespace qualification.
 */
using lumex_stacktrace_entry
    = lumex::core::exceptions::stacktrace::lumex_stacktrace_entry;

// Bring key functions into global namespace for convenience
/**
 * @brief Brings `lumex::core::exceptions::stacktrace::to_string` into the
 * global namespace.
 * @details This allows `to_string` to be called without full namespace
 * qualification when used with `lumex_stacktrace` objects, improving
 * readability.
 */
using lumex::core::exceptions::stacktrace::to_string;

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_EXCEPTIONS_STACKTRACE_STACKTRACE_HPP
