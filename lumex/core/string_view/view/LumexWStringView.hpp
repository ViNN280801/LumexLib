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

/**
 * @file LumexWStringView.hpp
 * @brief C++11 lightweight, non-owning view over contiguous wide character
 * sequences.
 * @details This header provides a manual implementation of a `wstring_view`
 * equivalent for C++11, offering a safe and efficient way to refer to wide
 * string data without owning or copying it. It is designed to be similar in
 * API and behavior to `std::wstring_view` introduced in C++17. The class
 * ensures cross-platform compatibility (Windows, Linux, macOS) and relies
 * exclusively on the C++11 standard library, ensuring exception-safety and
 * thread-safety for concurrent read-only access. It is a zero-overhead
 * abstraction that avoids dynamic memory allocation.
 *
 * @note This `LumexWStringView` class does not manage the lifetime of the wide
 * character data it views. It is the user's responsibility to ensure that the
 * underlying wide character sequence outlives the `LumexWStringView` instance.
 * Using a `LumexWStringView` that refers to destroyed or out-of-scope data
 *       will lead to undefined behavior.
 */
#ifndef LUMEX_CORE_STRING_VIEW_VIEW_WSTRING_VIEW_HPP
#define LUMEX_CORE_STRING_VIEW_VIEW_WSTRING_VIEW_HPP

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#include "lumex/LumexExport.hpp" // For LUMEX_API macro

#include <cstddef> // For std::size_t, std::ptrdiff_t
#include <cstring> // For std::wcslen, std::wmemcmp, std::wmemchr (for wide chars)
#include <iterator> // For std::reverse_iterator
#include <ostream>  // For std::wostream
#include <string>   // For std::wstring (for conversion functions)

#include "lumex/core/utility/LumexUtility" // For LUMEX_ATTRIBUTE_NODISCARD
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexConstantMacros.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
/**
 * @brief Core library components.
 * @details This namespace encapsulates fundamental, low-level utilities
 *          and data structures used across the Lumex Core library.
 */
namespace string_view
{
namespace view
{
/**
 * @brief A lightweight, non-owning view over a contiguous sequence of
 * `wchar_t` characters.
 * @details `LumexWStringView` provides a safe and efficient way to pass wide
 * string data around without incurring the cost of copying or dynamic
 * allocation. It holds a pointer to the beginning of a wide character sequence
 * and its length. It is an immutable view, meaning its contents cannot be
 * modified through the `LumexWStringView` itself, only the view's bounds can
 * be adjusted. This class is intended as a C++11 compatible alternative to
 * `std::wstring_view`.
 *
 * @tparam wchar_t The wide character type (fixed to `wchar_t` for this class).
 * @warning This class does not own the wide character data. The user must
 * ensure that the underlying wide character array outlives the
 * `LumexWStringView` instance. Dangling `LumexWStringView`s lead to undefined
 * behavior.
 */
class LUMEX_API LumexWStringView
{
public:
  // -- Public type aliases --
  /**
   * @brief Alias for the wide character type (`wchar_t`).
   */
  using value_type = wchar_t;
  /**
   * @brief Alias for a non-const pointer to the wide character type.
   */
  using pointer = wchar_t const *;
  /**
   * @brief Alias for a const pointer to the wide character type.
   */
  using const_pointer = wchar_t const *;
  /**
   * @brief Alias for a non-const reference to the wide character type.
   */
  using reference = wchar_t const &;
  /**
   * @brief Alias for a const reference to the wide character type.
   */
  using const_reference = wchar_t const &;
  /**
   * @brief Alias for a non-const iterator.
   * @details `iterator` and `const_iterator` are the same for
   * `LumexWStringView` as the view itself is constant (non-mutable).
   */
  using iterator = const_pointer; // iterator == const_iterator
  /**
   * @brief Alias for a const iterator.
   */
  using const_iterator = const_pointer;
  /**
   * @brief Alias for a reverse iterator.
   */
  using reverse_iterator = std::reverse_iterator<const_pointer>;
  /**
   * @brief Alias for a const reverse iterator.
   */
  using const_reverse_iterator = std::reverse_iterator<const_pointer>;
  /**
   * @brief Alias for the size type (`std::size_t`).
   */
  using size_type = std::size_t;
  /**
   * @brief Alias for the difference type (`std::ptrdiff_t`).
   */
  using difference_type = std::ptrdiff_t;

  /**
   * @brief A static constant representing "not a position".
   * @details Equivalent to `std::string::npos` and `std::string_view::npos`.
   *          Used as a return value for search functions when a substring or
   * character is not found.
   */
  static size_type const npos = static_cast<size_type> (-1);

  // -- Construction / assignment --
  /**
   * @brief Default constructor. Creates an empty `LumexWStringView`.
   * @details Initializes the view with a `nullptr` data pointer and a size of
   * 0.
   * @post `empty()` is `true`, `size()` is `0`, `data()` is `nullptr`.
   */
  LUMEX_CONSTEXPR_CTOR
  LumexWStringView () LUMEX_NOEXCEPT : m_data (nullptr), m_size (0) {}

  /**
   * @brief Constructs a `LumexWStringView` from a null-terminated C-style wide
   * string.
   * @details The view will encompass the characters from `str` up to, but not
   * including, the null terminator. If `str` is `nullptr`, the
   * `LumexWStringView` will be empty.
   * @param str A pointer to a null-terminated C-style wide string (`wchar_t
   * const *`).
   * @note Implicit, like `std::wstring_view`, so a literal or a string can be
   * passed where a view is expected. The view does not own the characters:
   * do not bind it to a temporary that dies first. `nullptr` gives an
   * empty view.
   * @note Complexity: O(N) where N is the length of the string, due to
   * `std::wcslen`.
   */
  LumexWStringView (wchar_t const *str)
      LUMEX_NOEXCEPT; // NOLINT(google-explicit-constructor)

  /**
   * @brief Constructs a `LumexWStringView` from a pointer to wide character
   * data and a specified length.
   * @details The view will encompass `len` characters starting from `str`.
   * @param str A pointer to the beginning of the wide character sequence.
   * @param len The number of wide characters in the sequence.
   * @note This constructor is `constexpr` and does not check if `str` is
   * `nullptr` when `len` is 0, by design, to allow views over `nullptr` for
   * empty strings.
   * @note Complexity: O(1).
   */
  LUMEX_CONSTEXPR_CTOR
  LumexWStringView (wchar_t const *str, size_type len) LUMEX_NOEXCEPT
      : m_data (str),
        m_size (len)
  {
  }

  /**
   * @brief Constructs a `LumexWStringView` from a
   * `std::basic_string<wchar_t>`.
   * @details Creates a view over the internal wide character data of the
   * provided `std::basic_string`.
   * @tparam Allocator The allocator type of the `std::basic_string`.
   * @param str The `std::basic_string` to view.
   * @note The `LumexWStringView` does not own the string data; `str` must
   * outlive the view.
   * @note Complexity: O(1).
   */
  template <class Allocator>
  LumexWStringView ( // NOLINT(google-explicit-constructor)
      std::basic_string<wchar_t, std::char_traits<wchar_t>, Allocator> const
          &str) LUMEX_NOEXCEPT : m_data (str.data ()),
                                 m_size (str.size ())
  {
  }

  /**
   * @brief Constructs a `LumexWStringView` from a `std::wstring`.
   * @details Enables implicit instantiation of the default allocator for
   * `std::wstring`. Creates a view over the internal wide character data of
   * the provided `std::wstring`.
   * @param str The `std::wstring` to view.
   * @note The `LumexWStringView` does not own the string data; `str` must
   * outlive the view.
   * @note Complexity: O(1).
   */
  LumexWStringView (std::wstring const &str)
      LUMEX_NOEXCEPT; // NOLINT(google-explicit-constructor)

  /**
   * @brief Copy constructor. Creates a new `LumexWStringView` that views the
   * same data.
   * @details Performs a shallow copy. The new `LumexWStringView` will point to
   * the same wide character data as `other`, and have the same size.
   * @param other The `LumexWStringView` to copy.
   * @note Complexity: O(1).
   */
  LUMEX_CONSTEXPR_CTOR
  LumexWStringView (LumexWStringView const &) LUMEX_NOEXCEPT = default;
  /**
   * @brief Copy assignment operator. Assigns the view of another
   * `LumexWStringView`.
   * @details Performs a shallow copy. This `LumexWStringView` will point to
   * the same wide character data as `other`, and have the same size.
   * @param other The `LumexWStringView` to assign from.
   * @return A reference to `*this`.
   * @note Complexity: O(1).
   */
  LumexWStringView &operator= (LumexWStringView const &) LUMEX_NOEXCEPT
      = default;
  /**
   * @brief Move constructor. Creates a new `LumexWStringView` by moving from
   * another.
   * @details Performs a shallow copy. Since `LumexWStringView` is a non-owning
   * type, move operations are effectively equivalent to copy operations.
   * @param other The `LumexWStringView` to move from.
   * @note Complexity: O(1).
   */
  LUMEX_CONSTEXPR_CTOR
  LumexWStringView (LumexWStringView &&) LUMEX_NOEXCEPT = default;
  /**
   * @brief Move assignment operator. Assigns the view of another
   * `LumexWStringView` by moving.
   * @details Performs a shallow copy. Since `LumexWStringView` is a non-owning
   * type, move operations are effectively equivalent to copy operations.
   * @param other The `LumexWStringView` to assign from.
   * @return A reference to `*this`.
   * @note Complexity: O(1).
   */
  LumexWStringView &operator= (LumexWStringView &&) LUMEX_NOEXCEPT = default;
  /**
   * @brief Destructor.
   * @details Does nothing as `LumexWStringView` does not own the wide
   * character data.
   * @note Complexity: O(1).
   */
  ~LumexWStringView () = default;

  // -- Iterator support --
  /**
   * @brief Returns a const iterator to the first wide character of the view.
   * @warning It is not recommended to ignore the return value of `begin()`.
   * @return A `const_iterator` pointing to the first wide character.
   * @note Complexity: O(1).
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "It is not recommended to ignore return value of begin()")
  LUMEX_CONSTEXPR const_iterator
  begin () const LUMEX_NOEXCEPT
  {
    return m_data;
  }

  /**
   * @brief Returns a const iterator to the first wide character of the view.
   * @details Alias for `begin()`.
   * @warning It is not recommended to ignore the return value of `cbegin()`.
   * @return A `const_iterator` pointing to the first wide character.
   * @note Complexity: O(1).
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "It is not recommended to ignore return value of cbegin()")
  LUMEX_CONSTEXPR const_iterator
  cbegin () const LUMEX_NOEXCEPT
  {
    return m_data;
  }

  /**
   * @brief Returns a const iterator to the wide character following the last
   * wide character of the view.
   * @details This character acts as a placeholder; attempting to dereference
   * it results in undefined behavior.
   * @warning It is not recommended to ignore the return value of `end()`.
   * @return A `const_iterator` pointing one past the last wide character.
   * @note Complexity: O(1).
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "It is not recommended to ignore return value of end()")
  LUMEX_CONSTEXPR const_iterator
  end () const LUMEX_NOEXCEPT
  {
    return m_data
           + m_size; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  }

  /**
   * @brief Returns a const iterator to the wide character following the last
   * wide character of the view.
   * @details Alias for `end()`.
   * @warning It is not recommended to ignore the return value of `cend()`.
   * @return A `const_iterator` pointing one past the last wide character.
   * @note Complexity: O(1).
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "It is not recommended to ignore return value of cend()")
  LUMEX_CONSTEXPR const_iterator
  cend () const LUMEX_NOEXCEPT
  {
    return m_data
           + m_size; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  }

  /**
   * @brief Returns a reverse iterator to the last wide character of the view.
   * @details The returned iterator points to the wide character that would be
   * last in a reverse iteration.
   * @warning It is not recommended to ignore the return value of `rbegin()`.
   * @return A `const_reverse_iterator` pointing to the last wide character.
   * @note Complexity: O(1).
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "It is not recommended to ignore return value of rbegin()")
  const_reverse_iterator rbegin () const LUMEX_NOEXCEPT;

  /**
   * @brief Returns a const reverse iterator to the last wide character of the
   * view.
   * @details Alias for `rbegin()`.
   * @warning It is not recommended to ignore the return value of `crbegin()`.
   * @return A `const_reverse_iterator` pointing to the last wide character.
   * @note Complexity: O(1).
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "It is not recommended to ignore return value of crbegin()")
  const_reverse_iterator crbegin () const LUMEX_NOEXCEPT;

  /**
   * @brief Returns a reverse iterator to the wide character preceding the
   * first wide character of the view.
   * @details This character acts as a placeholder; attempting to dereference
   * it results in undefined behavior.
   * @warning It is not recommended to ignore the return value of `rend()`.
   * @return A `const_reverse_iterator` pointing one before the first wide
   * character.
   * @note Complexity: O(1).
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "It is not recommended to ignore return value of rend()")
  const_reverse_iterator rend () const LUMEX_NOEXCEPT;

  /**
   * @brief Returns a const reverse iterator to the wide character preceding
   * the first wide character of the view.
   * @details Alias for `crend()`.
   * @warning It is not recommended to ignore the return value of `crend()`.
   * @return A `const_reverse_iterator` pointing one before the first wide
   * character.
   * @note Complexity: O(1).
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "It is not recommended to ignore return value of crend()")
  const_reverse_iterator crend () const LUMEX_NOEXCEPT;

  // -- Capacity --
  /**
   * @brief Returns the number of wide characters in the view.
   * @warning It is not recommended to ignore the return value of `size()`.
   * @return The number of wide characters in the view.
   * @note Complexity: O(1).
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "It is not recommended to ignore return value of size()")
  LUMEX_CONSTEXPR size_type
  size () const LUMEX_NOEXCEPT
  {
    return m_size;
  }

  /**
   * @brief Returns the number of wide characters in the view.
   * @details Alias for `size()`.
   * @warning It is not recommended to ignore the return value of `length()`.
   * @return The number of wide characters in the view.
   * @note Complexity: O(1).
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "It is not recommended to ignore return value of length()")
  LUMEX_CONSTEXPR size_type
  length () const LUMEX_NOEXCEPT
  {
    return m_size;
  }

  /**
   * @brief Checks if the view is empty (i.e., has a size of 0).
   * @warning It is not recommended to ignore the return value of `empty()`.
   * @return `true` if the view is empty, `false` otherwise.
   * @note Complexity: O(1).
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "It is not recommended to ignore return value of empty()")
  LUMEX_CONSTEXPR bool
  empty () const LUMEX_NOEXCEPT
  {
    return m_size == 0;
  }

  /**
   * @brief Returns the maximum possible number of wide characters in a
   * `LumexWStringView`.
   * @details This is typically the largest possible value for `size_type`
   * divided by 2, representing a practical limit on the view's length.
   * @return The maximum size.
   * @note Complexity: O(1).
   */
  static LUMEX_CONSTEXPR size_type
  max_size () LUMEX_NOEXCEPT
  {
    return static_cast<size_type> (-1) / 2;
  }

  // -- Element access --
  /**
   * @brief Accesses the wide character at a specified position.
   * @details Returns a const reference to the wide character at index `idx`.
   *          No bounds checking is performed.
   * @param idx The zero-based index of the wide character to access.
   * @return A `const_reference` to the wide character at `idx`.
   * @warning Accessing an element beyond `[0, size() - 1]` results in
   * undefined behavior.
   * @note Complexity: O(1).
   */
  LUMEX_CONSTEXPR const_reference
  operator[] (size_type idx) const LUMEX_NOEXCEPT
  {
    return m_data
        [idx]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  }

  /**
   * @brief Accesses the wide character at a specified position with bounds
   * checking.
   * @details Returns a const reference to the wide character at index `idx`.
   *          Throws `std::out_of_range` if `idx` is out of bounds.
   * @warning It is not recommended to ignore the return value of
   * `at(size_type)`.
   * @param idx The zero-based index of the wide character to access.
   * @return A `const_reference` to the wide character at `idx`.
   * @throws std::out_of_range If `idx >= size()`.
   * @note Complexity: O(1).
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "It is not recommended to ignore return value of at(size_type)")
  const_reference at (size_type idx) const;

  /**
   * @brief Returns a const reference to the first wide character of the view.
   * @details Behavior is undefined if the view is empty.
   * @warning It is not recommended to ignore the return value of `front()`.
   * @return A `const_reference` to the first wide character.
   * @note Complexity: O(1).
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "It is not recommended to ignore return value of front()")
  LUMEX_CONSTEXPR const_reference
  front () const LUMEX_NOEXCEPT
  {
    return m_data
        [0]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  }

  /**
   * @brief Returns a const reference to the last wide character of the view.
   * @details Behavior is undefined if the view is empty.
   * @warning It is not recommended to ignore the return value of `back()`.
   * @return A `const_reference` to the last wide character.
   * @note Complexity: O(1).
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "It is not recommended to ignore return value of back()")
  LUMEX_CONSTEXPR const_reference
  back () const LUMEX_NOEXCEPT
  {
    return m_data
        [m_size
         - 1]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  }

  /**
   * @brief Returns a pointer to the beginning of the wide character data.
   * @details This pointer is typically non-null for non-empty views and
   * `nullptr` for empty views.
   * @warning It is not recommended to ignore the return value of `data()`.
   * @return A `const_pointer` to the first wide character.
   * @note Complexity: O(1).
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "It is not recommended to ignore return value of data()")
  LUMEX_CONSTEXPR const_pointer
  data () const LUMEX_NOEXCEPT
  {
    return m_data;
  }

  // -- Modifiers (affecting the view, not the underlying data) --
  /**
   * @brief Clears the view, making it empty.
   * @details Sets the data pointer to `nullptr` and the size to `0`.
   * @post `empty()` is `true`, `size()` is `0`, `data()` is `nullptr`.
   * @note Complexity: O(1).
   */
  void clear () LUMEX_NOEXCEPT;
  /**
   * @brief Removes `n` wide characters from the beginning of the view.
   * @details Adjusts the `m_data` pointer and `m_size` to effectively
   *          "chop off" the first `n` wide characters. If `n` is greater than
   * `size()`, the view becomes empty.
   * @param n The number of wide characters to remove from the prefix.
   * @note Complexity: O(1).
   */
  void remove_prefix (size_type n) LUMEX_NOEXCEPT;
  /**
   * @brief Removes `n` wide characters from the end of the view.
   * @details Adjusts the `m_size` to effectively "chop off" the last `n` wide
   * characters. If `n` is greater than `size()`, the view becomes empty.
   * @param n The number of wide characters to remove from the suffix.
   * @note Complexity: O(1).
   */
  void remove_suffix (size_type n) LUMEX_NOEXCEPT;
  /**
   * @brief Swaps the contents (data pointer and size) of this view with
   * another.
   * @details This is an efficient, non-throwing swap operation that reassigns
   *          the views without touching the underlying wide character data.
   * @param other The `LumexWStringView` to swap with.
   * @note Complexity: O(1).
   */
  void swap (LumexWStringView &other) LUMEX_NOEXCEPT;

  // -- Copy out --
  /**
   * @brief Copies wide characters from the view into a wide character array.
   * @details Copies up to `count` wide characters from this view, starting at
   * `pos`, into the `dest` buffer. The actual number of wide characters copied
   * is the smaller of `count` and `size() - pos`.
   * @param dest The destination wide character array to copy into.
   * @param count The maximum number of wide characters to copy.
   * @param pos The starting position in this `LumexWStringView` from which to
   * copy. Defaults to 0.
   * @return The number of wide characters actually copied.
   * @throws std::out_of_range If `pos > size()`.
   * @note Complexity: O(N) where N is the number of wide characters copied.
   */
  size_type copy (wchar_t *dest, size_type count, size_type pos = 0) const;

  // -- Substring --
  /**
   * @brief Returns a new `LumexWStringView` representing a substring of this
   * view.
   * @details The new view will start at `pos` and extend for `n` wide
   * characters. If `pos` is out of bounds, an exception is thrown. If `n`
   * extends beyond the end of the current view, it is clamped to the remaining
   * length.
   * @warning It is not recommended to ignore the return value of
   * `substr(size_type, size_type)`.
   * @param pos The starting position of the substring. Defaults to 0.
   * @param n The length of the substring. Defaults to `npos` (until the end of
   * the view).
   * @return A new `LumexWStringView` object.
   * @throws std::out_of_range If `pos > size()`.
   * @note Complexity: O(1).
   */
  LUMEX_ATTRIBUTE_NODISCARD ("It is not recommended to ignore return value of "
                             "substr(size_type, size_type)")
  LumexWStringView substr (size_type pos = 0, size_type n = npos) const;

  // -- Comparison --
  /**
   * @brief Compares this `LumexWStringView` with another `LumexWStringView`.
   * @details Performs a lexicographical comparison.
   * @warning It is not recommended to ignore the return value of
   * `compare(LumexWStringView)`.
   * @param other The `LumexWStringView` to compare with.
   * @return An integer representing the comparison result:
   *         - Less than 0 if `*this` is lexicographically less than `other`.
   *         - 0 if `*this` is lexicographically equal to `other`.
   *         - Greater than 0 if `*this` is lexicographically greater than
   * `other`.
   * @note Complexity: O(N) where N is the minimum length of the two views.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("It is not recommended to ignore return value of "
                             "compare(LumexWStringView)")
  int compare (LumexWStringView other) const LUMEX_NOEXCEPT;

  /**
   * @brief Compares a substring of this `LumexWStringView` with another
   * `LumexWStringView`.
   * @details Extracts a substring from `*this` starting at `pos` with length
   * `len`, and then compares that substring lexicographically with `other`.
   * @warning It is not recommended to ignore the return value of
   * `compare(size_type, size_type, LumexWStringView)`.
   * @param pos The starting position of the substring in `*this`.
   * @param len The length of the substring in `*this`.
   * @param other The `LumexWStringView` to compare with.
   * @return An integer representing the comparison result (see
   * `compare(LumexWStringView)`).
   * @throws std::out_of_range If `pos > size()`.
   * @note Complexity: O(N) where N is the minimum length of the compared
   * substring and `other`.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("It is not recommended to ignore return value of "
                             "compare(size_type, size_type, LumexWStringView)")
  int compare (size_type pos, size_type len, LumexWStringView other) const;

  /**
   * @brief Compares this `LumexWStringView` with a null-terminated C-style
   * wide string.
   * @details Converts `cstr` to a `LumexWStringView` internally and then
   * performs a comparison.
   * @param cstr The null-terminated C-style wide string to compare with.
   * @return An integer representing the comparison result (see
   * `compare(LumexWStringView)`).
   * @note Complexity: O(N) where N is the minimum length of `*this` and
   * `cstr`.
   */
  int compare (wchar_t const *cstr) const;

  // -- Starts / ends / contains helpers (non-standard extensions but useful)
  // --
  /**
   * @brief Checks if the `LumexWStringView` starts with a specific wide
   * character.
   * @warning It is not recommended to ignore the return value of
   * `starts_with(wchar_t)`.
   * @param chr The wide character to check for at the beginning of the view.
   * @return `true` if the view is not empty and its first wide character is
   * `chr`, `false` otherwise.
   * @note Complexity: O(1).
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "It is not recommended to ignore return value of starts_with(wchar_t)")
  bool starts_with (wchar_t chr) const LUMEX_NOEXCEPT;

  /**
   * @brief Checks if the `LumexWStringView` starts with a specific
   * `LumexWStringView`.
   * @warning It is not recommended to ignore the return value of
   * `starts_with(LumexWStringView)`.
   * @param str The `LumexWStringView` to check for at the beginning of the
   * view.
   * @return `true` if the view has `str` as a prefix, `false` otherwise.
   * @note Complexity: O(N) where N is `str.size()`.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("It is not recommended to ignore return value of "
                             "starts_with(LumexWStringView)")
  bool starts_with (LumexWStringView str) const LUMEX_NOEXCEPT;

  /**
   * @brief Checks if the `LumexWStringView` ends with a specific wide
   * character.
   * @warning It is not recommended to ignore the return value of
   * `ends_with(wchar_t)`.
   * @param chr The wide character to check for at the end of the view.
   * @return `true` if the view is not empty and its last wide character is
   * `chr`, `false` otherwise.
   * @note Complexity: O(1).
   */
  LUMEX_ATTRIBUTE_NODISCARD (
      "It is not recommended to ignore return value of ends_with(wchar_t)")
  bool ends_with (wchar_t chr) const LUMEX_NOEXCEPT;

  /**
   * @brief Checks if the `LumexWStringView` ends with a specific
   * `LumexWStringView`.
   * @warning It is not recommended to ignore the return value of
   * `ends_with(LumexWStringView)`.
   * @param str The `LumexWStringView` to check for at the end of the view.
   * @return `true` if the view has `str` as a suffix, `false` otherwise.
   * @note Complexity: O(N) where N is `str.size()`.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("It is not recommended to ignore return value of "
                             "ends_with(LumexWStringView)")
  bool ends_with (LumexWStringView str) const LUMEX_NOEXCEPT;

  // -- Find (simple implementations) --
  /**
   * @brief Finds the first occurrence of a wide character within the view.
   * @details Searches for `chr` starting from `pos`.
   * @warning It is not recommended to ignore the return value of
   * `find(wchar_t, size_type)`.
   * @param chr The wide character to search for.
   * @param pos The starting position for the search. Defaults to 0.
   * @return The zero-based index of the first occurrence of `chr`, or `npos`
   * if not found.
   * @note Complexity: O(N) where N is `size() - pos`.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("It is not recommended to ignore return value of "
                             "find(wchar_t, size_type)")
  size_type find (wchar_t chr, size_type pos = 0) const LUMEX_NOEXCEPT;

  /**
   * @brief Finds the first occurrence of a `LumexWStringView` within this
   * view.
   * @details Searches for `str` starting from `pos`.
   * @warning It is not recommended to ignore the return value of
   * `find(LumexWStringView, size_type)`.
   * @param str The `LumexWStringView` to search for.
   * @param pos The starting position for the search. Defaults to 0.
   * @return The zero-based index of the first occurrence of `str`, or `npos`
   * if not found.
   * @note Complexity: O(N*M) in worst case, where N is `size() - pos` and M is
   * `str.size()`.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("It is not recommended to ignore return value of "
                             "find(LumexWStringView, size_type)")
  size_type find (LumexWStringView str,
                  size_type pos = 0) const LUMEX_NOEXCEPT;

  /**
   * @brief Finds the first occurrence of a C-style wide string within this
   * view with a specified count.
   * @details Searches for the first `count` wide characters of `cstr` starting
   * from `pos`.
   * @param cstr The C-style wide string to search for.
   * @param pos The starting position for the search.
   * @param count The number of wide characters from `cstr` to use for the
   * search.
   * @return The zero-based index of the first occurrence, or `npos` if not
   * found.
   * @note Complexity: O(N*M) in worst case, where N is `size() - pos` and M is
   * `count`.
   */
  size_type find (wchar_t const *cstr, size_type pos,
                  size_type count) const LUMEX_NOEXCEPT;

  /**
   * @brief Finds the first occurrence of a null-terminated C-style wide string
   * within this view.
   * @details Searches for `cstr` (up to its null terminator) starting from
   * `pos`.
   * @param cstr The null-terminated C-style wide string to search for.
   * @param pos The starting position for the search. Defaults to 0.
   * @return The zero-based index of the first occurrence, or `npos` if not
   * found.
   * @note Complexity: O(N*M) in worst case, where N is `size() - pos` and M is
   * `wcslen(cstr)`.
   */
  size_type find (wchar_t const *cstr, size_type pos = 0) const LUMEX_NOEXCEPT;

  // -- Reverse find --
  /**
   * @brief Finds the last occurrence of a `LumexWStringView` within this view.
   * @details Searches backward for `str` starting from `pos`.
   * @warning It is not recommended to ignore the return value of
   * `rfind(LumexWStringView, size_type)`.
   * @param str The `LumexWStringView` to search for.
   * @param pos The starting position for the reverse search. Defaults to
   * `npos` (end of view).
   * @return The zero-based index of the last occurrence of `str`, or `npos` if
   * not found.
   * @note Complexity: O(N*M) in worst case, where N is `pos` and M is
   * `str.size()`.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("It is not recommended to ignore return value of "
                             "rfind(LumexWStringView, size_type)")
  size_type rfind (LumexWStringView str,
                   size_type pos = npos) const LUMEX_NOEXCEPT;

  /**
   * @brief Finds the last occurrence of a wide character within the view.
   * @details Searches backward for `chr` starting from `pos`.
   * @warning It is not recommended to ignore the return value of
   * `rfind(wchar_t, size_type)`.
   * @param chr The wide character to search for.
   * @param pos The starting position for the reverse search. Defaults to
   * `npos` (end of view).
   * @return The zero-based index of the last occurrence of `chr`, or `npos` if
   * not found.
   * @note Complexity: O(N) where N is `pos`.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("It is not recommended to ignore return value of "
                             "rfind(wchar_t, size_type)")
  size_type rfind (wchar_t chr, size_type pos = npos) const LUMEX_NOEXCEPT;

  /**
   * @brief Finds the last occurrence of a C-style wide string within this view
   * with a specified count.
   * @details Searches backward for the last `count` wide characters of `cstr`
   * starting from `pos`.
   * @param cstr The C-style wide string to search for.
   * @param pos The starting position for the reverse search.
   * @param count The number of wide characters from `cstr` to use for the
   * search.
   * @return The zero-based index of the last occurrence, or `npos` if not
   * found.
   * @note Complexity: O(N*M) in worst case, where N is `pos` and M is `count`.
   */
  size_type rfind (wchar_t const *cstr, size_type pos,
                   size_type count) const LUMEX_NOEXCEPT;

  /**
   * @brief Finds the last occurrence of a null-terminated C-style wide string
   * within this view.
   * @details Searches backward for `cstr` (up to its null terminator) starting
   * from `pos`.
   * @param cstr The null-terminated C-style wide string to search for.
   * @param pos The starting position for the reverse search. Defaults to
   * `npos`.
   * @return The zero-based index of the last occurrence, or `npos` if not
   * found.
   * @note Complexity: O(N*M) in worst case, where N is `pos` and M is
   * `wcslen(cstr)`.
   */
  size_type rfind (wchar_t const *cstr,
                   size_type pos = npos) const LUMEX_NOEXCEPT;

  // -- Find first of (finds any wide character from a set) --
  /**
   * @brief Finds the first occurrence of any wide character from a given set
   * of wide characters.
   * @details Searches for the first wide character in `*this` that matches any
   * wide character in `str`, starting from `pos`.
   * @warning It is not recommended to ignore the return value of
   * `find_first_of(LumexWStringView, size_type)`.
   * @param str A `LumexWStringView` containing the set of wide characters to
   * search for.
   * @param pos The starting position for the search. Defaults to 0.
   * @return The zero-based index of the first match, or `npos` if no wide
   * character is found.
   * @note Complexity: O(N*M) in worst case, where N is `size() - pos` and M is
   * `str.size()`.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("It is not recommended to ignore return value of "
                             "find_first_of(LumexWStringView, size_type)")
  size_type find_first_of (LumexWStringView str,
                           size_type pos = 0) const LUMEX_NOEXCEPT;

  /**
   * @brief Finds the first occurrence of a specific wide character.
   * @details Alias for `find(wchar_t, size_type)`.
   * @warning It is not recommended to ignore the return value of
   * `find_first_of(wchar_t, size_type)`.
   * @param chr The wide character to search for.
   * @param pos The starting position for the search. Defaults to 0.
   * @return The zero-based index of the first match, or `npos` if not found.
   * @note Complexity: O(N) where N is `size() - pos`.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("It is not recommended to ignore return value of "
                             "find_first_of(wchar_t, size_type)")
  size_type find_first_of (wchar_t chr,
                           size_type pos = 0) const LUMEX_NOEXCEPT;

  /**
   * @brief Finds the first occurrence of any wide character from a C-style
   * wide string set with a specified count.
   * @details Searches for the first wide character in `*this` that matches any
   * of the first `count` wide characters of `cstr`, starting from `pos`.
   * @param cstr A C-style wide string containing the set of wide characters to
   * search for.
   * @param pos The starting position for the search.
   * @param count The number of wide characters from `cstr` to use as the set.
   * @return The zero-based index of the first match, or `npos` if no wide
   * character is found.
   * @note Complexity: O(N*M) in worst case, where N is `size() - pos` and M is
   * `count`.
   */
  size_type find_first_of (wchar_t const *cstr, size_type pos,
                           size_type count) const LUMEX_NOEXCEPT;

  /**
   * @brief Finds the first occurrence of any wide character from a
   * null-terminated C-style wide string set.
   * @details Searches for the first wide character in `*this` that matches any
   * wide character in `cstr` (up to its null terminator), starting from `pos`.
   * @param cstr A null-terminated C-style wide string containing the set of
   * wide characters to search for.
   * @param pos The starting position for the search. Defaults to 0.
   * @return The zero-based index of the first match, or `npos` if no wide
   * character is found.
   * @note Complexity: O(N*M) in worst case, where N is `size() - pos` and M is
   * `wcslen(cstr)`.
   */
  size_type find_first_of (wchar_t const *cstr,
                           size_type pos = 0) const LUMEX_NOEXCEPT;

  // -- Find last of (finds any wide character from a set, searching backward)
  // --
  /**
   * @brief Finds the last occurrence of any wide character from a given set of
   * wide characters.
   * @details Searches backward for the last wide character in `*this` that
   * matches any wide character in `str`, starting from `pos`.
   * @warning It is not recommended to ignore the return value of
   * `find_last_of(LumexWStringView, size_type)`.
   * @param str A `LumexWStringView` containing the set of wide characters to
   * search for.
   * @param pos The starting position for the reverse search. Defaults to
   * `npos`.
   * @return The zero-based index of the last match, or `npos` if no wide
   * character is found.
   * @note Complexity: O(N*M) in worst case, where N is `pos` and M is
   * `str.size()`.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("It is not recommended to ignore return value of "
                             "find_last_of(LumexWStringView, size_type)")
  size_type find_last_of (LumexWStringView str,
                          size_type pos = npos) const LUMEX_NOEXCEPT;

  /**
   * @brief Finds the last occurrence of a specific wide character.
   * @details Alias for `rfind(wchar_t, size_type)`.
   * @warning It is not recommended to ignore the return value of
   * `find_last_of(wchar_t, size_type)`.
   * @param chr The wide character to search for.
   * @param pos The starting position for the reverse search. Defaults to
   * `npos`.
   * @return The zero-based index of the last match, or `npos` if not found.
   * @note Complexity: O(N) where N is `pos`.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("It is not recommended to ignore return value of "
                             "find_last_of(wchar_t, size_type)")
  size_type find_last_of (wchar_t chr,
                          size_type pos = npos) const LUMEX_NOEXCEPT;

  /**
   * @brief Finds the last occurrence of any wide character from a C-style wide
   * string set with a specified count.
   * @details Searches backward for the last wide character in `*this` that
   * matches any of the first `count` wide characters of `cstr`, starting from
   * `pos`.
   * @param cstr A C-style wide string containing the set of wide characters to
   * search for.
   * @param pos The starting position for the reverse search.
   * @param count The number of wide characters from `cstr` to use as the set.
   * @return The zero-based index of the last match, or `npos` if no wide
   * character is found.
   * @note Complexity: O(N*M) in worst case, where N is `pos` and M is `count`.
   */
  size_type find_last_of (wchar_t const *cstr, size_type pos,
                          size_type count) const LUMEX_NOEXCEPT;

  /**
   * @brief Finds the last occurrence of any wide character from a
   * null-terminated C-style wide string set.
   * @details Searches backward for the last wide character in `*this` that
   * matches any wide character in `cstr` (up to its null terminator), starting
   * from `pos`.
   * @param cstr A null-terminated C-style wide string containing the set of
   * wide characters to search for.
   * @param pos The starting position for the reverse search. Defaults to
   * `npos`.
   * @return The zero-based index of the last match, or `npos` if no wide
   * character is found.
   * @note Complexity: O(N*M) in worst case, where N is `pos` and M is
   * `wcslen(cstr)`.
   */
  size_type find_last_of (wchar_t const *cstr,
                          size_type pos = npos) const LUMEX_NOEXCEPT;

  // -- Find first not of (finds first wide character *not* from a set) --
  /**
   * @brief Finds the first occurrence of any wide character *not* from a given
   * set of wide characters.
   * @details Searches for the first wide character in `*this` that does *not*
   * match any wide character in `str`, starting from `pos`.
   * @warning It is not recommended to ignore the return value of
   * `find_first_not_of(LumexWStringView, size_type)`.
   * @param str A `LumexWStringView` containing the set of wide characters to
   * exclude.
   * @param pos The starting position for the search. Defaults to 0.
   * @return The zero-based index of the first non-matching wide character, or
   * `npos` if all characters match the set.
   * @note Complexity: O(N*M) in worst case, where N is `size() - pos` and M is
   * `str.size()`.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("It is not recommended to ignore return value of "
                             "find_first_not_of(LumexWStringView, size_type)")
  size_type find_first_not_of (LumexWStringView str,
                               size_type pos = 0) const LUMEX_NOEXCEPT;

  /**
   * @brief Finds the first occurrence of a wide character *not* equal to a
   * specific wide character.
   * @details Searches for the first wide character in `*this` that is not
   * `chr`, starting from `pos`.
   * @warning It is not recommended to ignore the return value of
   * `find_first_not_of(wchar_t, size_type)`.
   * @param chr The wide character to exclude.
   * @param pos The starting position for the search. Defaults to 0.
   * @return The zero-based index of the first non-matching wide character, or
   * `npos` if all characters are `chr`.
   * @note Complexity: O(N) where N is `size() - pos`.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("It is not recommended to ignore return value of "
                             "find_first_not_of(wchar_t, size_type)")
  size_type find_first_not_of (wchar_t chr,
                               size_type pos = 0) const LUMEX_NOEXCEPT;

  /**
   * @brief Finds the first occurrence of any wide character *not* from a
   * C-style wide string set with a specified count.
   * @details Searches for the first wide character in `*this` that does *not*
   * match any of the first `count` wide characters of `cstr`, starting from
   * `pos`.
   * @param cstr A C-style wide string containing the set of wide characters to
   * exclude.
   * @param pos The starting position for the search.
   * @param count The number of wide characters from `cstr` to use as the set.
   * @return The zero-based index of the first non-matching wide character, or
   * `npos` if all characters match the set.
   * @note Complexity: O(N*M) in worst case, where N is `size() - pos` and M is
   * `count`.
   */
  size_type find_first_not_of (wchar_t const *cstr, size_type pos,
                               size_type count) const LUMEX_NOEXCEPT;

  /**
   * @brief Finds the first occurrence of any wide character *not* from a
   * null-terminated C-style wide string set.
   * @details Searches for the first wide character in `*this` that does *not*
   * match any wide character in `cstr` (up to its null terminator), starting
   * from `pos`.
   * @param cstr A null-terminated C-style wide string containing the set of
   * wide characters to exclude.
   * @param pos The starting position for the search. Defaults to 0.
   * @return The zero-based index of the first non-matching wide character, or
   * `npos` if all characters match the set.
   * @note Complexity: O(N*M) in worst case, where N is `size() - pos` and M is
   * `wcslen(cstr)`.
   */
  size_type find_first_not_of (wchar_t const *cstr,
                               size_type pos = 0) const LUMEX_NOEXCEPT;

  // -- Find last not of (finds last wide character *not* from a set, searching
  // backward) --
  /**
   * @brief Finds the last occurrence of any wide character *not* from a given
   * set of wide characters.
   * @details Searches backward for the last wide character in `*this` that
   * does *not* match any wide character in `str`, starting from `pos`.
   * @warning It is not recommended to ignore the return value of
   * `find_last_not_of(LumexWStringView, size_type)`.
   * @param str A `LumexWStringView` containing the set of wide characters to
   * exclude.
   * @param pos The starting position for the reverse search. Defaults to
   * `npos`.
   * @return The zero-based index of the last non-matching wide character, or
   * `npos` if all characters match the set.
   * @note Complexity: O(N*M) in worst case, where N is `pos` and M is
   * `str.size()`.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("It is not recommended to ignore return value of "
                             "find_last_not_of(LumexWStringView, size_type)")
  size_type find_last_not_of (LumexWStringView str,
                              size_type pos = npos) const LUMEX_NOEXCEPT;

  /**
   * @brief Finds the last occurrence of a wide character *not* equal to a
   * specific wide character.
   * @details Searches backward for the last wide character in `*this` that is
   * not `chr`, starting from `pos`.
   * @warning It is not recommended to ignore the return value of
   * `find_last_not_of(wchar_t, size_type)`.
   * @param chr The wide character to exclude.
   * @param pos The starting position for the reverse search. Defaults to
   * `npos`.
   * @return The zero-based index of the last non-matching wide character, or
   * `npos` if all characters are `chr`.
   * @note Complexity: O(N) where N is `pos`.
   */
  LUMEX_ATTRIBUTE_NODISCARD ("It is not recommended to ignore return value of "
                             "find_last_not_of(wchar_t, size_type)")
  size_type find_last_not_of (wchar_t chr,
                              size_type pos = npos) const LUMEX_NOEXCEPT;

  /**
   * @brief Finds the last occurrence of any wide character *not* from a
   * C-style wide string set with a specified count.
   * @details Searches backward for the last wide character in `*this` that
   * does *not* match any of the first `count` wide characters of `cstr`,
   * starting from `pos`.
   * @param cstr A C-style wide string containing the set of wide characters to
   * exclude.
   * @param pos The starting position for the reverse search.
   * @param count The number of wide characters from `cstr` to use as the set.
   * @return The zero-based index of the last non-matching wide character, or
   * `npos` if all characters match the set.
   * @note Complexity: O(N*M) in worst case, where N is `pos` and M is `count`.
   */
  size_type find_last_not_of (wchar_t const *cstr, size_type pos,
                              size_type count) const LUMEX_NOEXCEPT;

  /**
   * @brief Finds the last occurrence of any wide character *not* from a
   * null-terminated C-style wide string set.
   * @details Searches backward for the last wide character in `*this` that
   * does *not* match any wide character in `cstr` (up to its null terminator),
   * starting from `pos`.
   * @param cstr A null-terminated C-style wide string containing the set of
   * wide characters to exclude.
   * @param pos The starting position for the reverse search. Defaults to
   * `npos`.
   * @return The zero-based index of the last non-matching wide character, or
   * `npos` if all characters match the set.
   * @note Complexity: O(N*M) in worst case, where N is `pos` and M is
   * `wcslen(cstr)`.
   */
  size_type find_last_not_of (wchar_t const *cstr,
                              size_type pos = npos) const LUMEX_NOEXCEPT;

  // -- Conversion back to std::wstring --
  /**
   * @brief Explicit conversion operator to `std::basic_string<wchar_t>`.
   * @details Constructs a new `std::basic_string` (or `std::wstring`)
   * containing a copy of the wide characters viewed by this
   * `LumexWStringView`.
   * @tparam Allocator The allocator type for the `std::basic_string`. Defaults
   * to `std::allocator<wchar_t>`.
   * @return A new `std::basic_string` object.
   * @note Complexity: O(N) where N is `size()`.
   */
  template <class Allocator = std::allocator<wchar_t>>
  explicit
  operator std::basic_string<wchar_t, std::char_traits<wchar_t>, Allocator> ()
      const
  {
    return std::basic_string<wchar_t, std::char_traits<wchar_t>, Allocator> (
        m_data, m_size);
  }

  /**
   * @brief Converts the `LumexWStringView` to a `std::basic_string<wchar_t>`.
   * @details Constructs a new `std::basic_string` (or `std::wstring`)
   * containing a copy of the wide characters viewed by this
   * `LumexWStringView`.
   * @tparam Allocator The allocator type for the `std::basic_string`. Defaults
   * to `std::allocator<wchar_t>`.
   * @param alloc An allocator object to use for the new wide string. Defaults
   * to a default-constructed allocator.
   * @return A new `std::basic_string` object.
   * @note Complexity: O(N) where N is `size()`.
   */
  template <class Allocator = std::allocator<wchar_t>>
  std::basic_string<wchar_t, std::char_traits<wchar_t>, Allocator>
  to_string (Allocator const &alloc = Allocator ()) const
  {
    return std::basic_string<wchar_t, std::char_traits<wchar_t>, Allocator> (
        m_data, m_size, alloc);
  }

private:
  /**
   * @brief Pointer to the beginning of the wide character sequence.
   * @details This member holds the address of the first wide character in the
   * viewed sequence.
   */
  wchar_t const *m_data;
  /**
   * @brief The number of wide characters in the viewed sequence.
   * @details This member stores the length of the wide string view.
   */
  size_type m_size;
};

// -- Non-member relational operators --
/**
 * @brief Equality comparison operator for two `LumexWStringView` objects.
 * @details Compares two `LumexWStringView` objects for lexicographical
 * equality.
 * @param lhs The left-hand side `LumexWStringView`.
 * @param rhs The right-hand side `LumexWStringView`.
 * @return `true` if both views are of the same size and their contents are
 * identical, `false` otherwise.
 * @note Complexity: O(N) where N is the minimum length of the two views.
 */
LUMEX_API inline bool
operator== (LumexWStringView lhs, LumexWStringView rhs) LUMEX_NOEXCEPT
{
  return lhs.size () == rhs.size () && lhs.compare (rhs) == 0;
}

/**
 * @brief Inequality comparison operator for two `LumexWStringView` objects.
 * @details Checks if two `LumexWStringView` objects are not lexicographically
 * equal.
 * @param lhs The left-hand side `LumexWStringView`.
 * @param rhs The right-hand side `LumexWStringView`.
 * @return `true` if the views are not equal, `false` otherwise.
 * @note Complexity: O(N) where N is the minimum length of the two views.
 */
LUMEX_API inline bool
operator!= (LumexWStringView lhs, LumexWStringView rhs) LUMEX_NOEXCEPT
{
  return !(lhs == rhs);
}

/**
 * @brief Less-than comparison operator for two `LumexWStringView` objects.
 * @details Compares two `LumexWStringView` objects lexicographically.
 * @param lhs The left-hand side `LumexWStringView`.
 * @param rhs The right-hand side `LumexWStringView`.
 * @return `true` if `lhs` is lexicographically less than `rhs`, `false`
 * otherwise.
 * @note Complexity: O(N) where N is the minimum length of the two views.
 */
LUMEX_API inline bool
operator< (LumexWStringView lhs, LumexWStringView rhs) LUMEX_NOEXCEPT
{
  return lhs.compare (rhs) < 0;
}

/**
 * @brief Greater-than comparison operator for two `LumexWStringView` objects.
 * @details Compares two `LumexWStringView` objects lexicographically.
 * @param lhs The left-hand side `LumexWStringView`.
 * @param rhs The right-hand side `LumexWStringView`.
 * @return `true` if `lhs` is lexicographically greater than `rhs`, `false`
 * otherwise.
 * @note Complexity: O(N) where N is the minimum length of the two views.
 */
LUMEX_API inline bool
operator> (LumexWStringView lhs, LumexWStringView rhs) LUMEX_NOEXCEPT
{
  return lhs.compare (rhs) > 0;
}

/**
 * @brief Less-than-or-equal-to comparison operator for two `LumexWStringView`
 * objects.
 * @details Compares two `LumexWStringView` objects lexicographically.
 * @param lhs The left-hand side `LumexWStringView`.
 * @param rhs The right-hand side `LumexWStringView`.
 * @return `true` if `lhs` is lexicographically less than or equal to `rhs`,
 * `false` otherwise.
 * @note Complexity: O(N) where N is the minimum length of the two views.
 */
LUMEX_API inline bool
operator<= (LumexWStringView lhs, LumexWStringView rhs) LUMEX_NOEXCEPT
{
  return lhs.compare (rhs) <= 0;
}

/**
 * @brief Greater-than-or-equal-to comparison operator for two
 * `LumexWStringView` objects.
 * @details Compares two `LumexWStringView` objects lexicographically.
 * @param lhs The left-hand side `LumexWStringView`.
 * @param rhs The right-hand side `LumexWStringView`.
 * @return `true` if `lhs` is lexicographically greater than or equal to `rhs`,
 * `false` otherwise.
 * @note Complexity: O(N) where N is the minimum length of the two views.
 */
LUMEX_API inline bool
operator>= (LumexWStringView lhs, LumexWStringView rhs) LUMEX_NOEXCEPT
{
  return lhs.compare (rhs) >= 0;
}

// -- Stream inserter --
/**
 * @brief Overload for inserting a `LumexWStringView` into an `std::wostream`.
 * @details This operator allows `LumexWStringView` objects to be printed
 * directly to wide standard output streams. It writes the viewed wide
 * character data to the stream.
 * @param wostr The wide output stream.
 * @param wsview The `LumexWStringView` to insert.
 * @return A reference to the wide output stream.
 * @note Complexity: O(N) where N is `wsview.size()`.
 */
LUMEX_API std::wostream &operator<< (std::wostream &wostr,
                                     LumexWStringView wsview);
} // namespace view
} // namespace string_view
} // namespace core
} // namespace lumex

/**
 * @brief Global type alias for `lumex::core::string_view::LumexWStringView`.
 * @details This `using` declaration brings `LumexWStringView` into the global
 * namespace (or enclosing namespace where it's included), allowing for more
 * convenient usage without full namespace qualification, similar to
 * `std::wstring_view`.
 */
using LumexWStringView = lumex::core::string_view::view::LumexWStringView;

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // LUMEX_CORE_STRING_VIEW_VIEW_WSTRING_VIEW_HPP
