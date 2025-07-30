/**
 * @file LumexStringView.hpp
 * @brief C++11 lightweight, non-owning view over character sequences.
 * @details This header provides a manual implementation of a `string_view` equivalent
 *          for C++11, offering a safe and efficient way to refer to string data
 *          without owning or copying it. It is designed to be similar in API and
 *          behavior to `std::string_view` introduced in C++17.
 *          The implementation is cross-platform (Windows / Linux / macOS) and relies
 *          solely on the C++11 standard library.
 *          It respects exception-safety, thread-safety (concurrent read-only access),
 *          and does not perform dynamic allocation.
 *
 * @note This `LumexStringView` class does not manage the lifetime of the character
 *       data it views. It is the user's responsibility to ensure that the underlying
 *       character sequence outlives the `LumexStringView` instance. Using a
 *       `LumexStringView` that refers to destroyed or out-of-scope data
 *       will lead to undefined behavior.
 */
#ifndef LUMEX_STRING_VIEW_HPP
#define LUMEX_STRING_VIEW_HPP

#include <cstddef>  // std::size_t, std::ptrdiff_t
#include <cstring>  // std::strlen, std::memcmp
#include <iterator> // std::reverse_iterator
#include <ostream>  // std::ostream
#include <string>   // std::string

#include "lumex/LumexExport.hpp"
#include "lumex/core/utility/LumexUtility"

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Core
  {
    /**
     * @brief Core library components.
     * @details This namespace encapsulates fundamental, low-level utilities
     *          and data structures used across the Lumex Core library.
     */
    namespace StringView
    {
      /**
       * @brief A lightweight, non-owning view over a contiguous sequence of `char` characters.
       * @details `LumexStringView` provides a safe and efficient way to pass string data
       *          around without incurring the cost of copying or dynamic allocation.
       *          It holds a pointer to the beginning of a character sequence and its length.
       *          It is an immutable view, meaning its contents cannot be modified through
       *          the `LumexStringView` itself, only the view's bounds can be adjusted.
       *          This class is intended as a C++11 compatible alternative to `std::string_view`.
       *
       * @tparam char The character type (fixed to `char` for this class).
       * @warning This class does not own the character data. The user must ensure
       *          that the underlying character array outlives the `LumexStringView` instance.
       *          Dangling `LumexStringView`s lead to undefined behavior.
       */
      class LUMEX_API LumexStringView
      {
      public:
        // -- Public type aliases --
        /**
         * @brief Alias for the character type (`char`).
         */
        using value_type = char;
        /**
         * @brief Alias for a non-const pointer to the character type.
         */
        using pointer = char const *;
        /**
         * @brief Alias for a const pointer to the character type.
         */
        using const_pointer = char const *;
        /**
         * @brief Alias for a non-const reference to the character type.
         */
        using reference = char const &;
        /**
         * @brief Alias for a const reference to the character type.
         */
        using const_reference = char const &;
        /**
         * @brief Alias for a non-const iterator.
         * @details `iterator` and `const_iterator` are the same for `LumexStringView`
         *          as the view itself is constant (non-mutable).
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
         *          Used as a return value for search functions when a substring or character is not found.
         */
        static size_type const npos = static_cast<size_type>(-1);

        // -- Construction / assignment --
        /**
         * @brief Default constructor. Creates an empty `LumexStringView`.
         * @details Initializes the view with a `nullptr` data pointer and a size of 0.
         * @post `empty()` is `true`, `size()` is `0`, `data()` is `nullptr`.
         */
        constexpr LumexStringView() noexcept : m_data(nullptr), m_size(0) {}

        /**
         * @brief Constructs a `LumexStringView` from a null-terminated C-style string.
         * @details The view will encompass the characters from `str` up to, but not including, the null terminator.
         *          If `str` is `nullptr`, the `LumexStringView` will be empty.
         * @param str A pointer to a null-terminated C-style string (`char const *`).
         * @note This constructor is `explicit` to prevent unintended implicit conversions.
         * @complexity O(N) where N is the length of the string, due to `std::strlen`.
         */
        explicit LumexStringView(char const *str) noexcept;

        /**
         * @brief Constructs a `LumexStringView` from a pointer to character data and a specified length.
         * @details The view will encompass `len` characters starting from `str`.
         * @param str A pointer to the beginning of the character sequence.
         * @param len The number of characters in the sequence.
         * @note This constructor is `constexpr` and does not check if `str` is `nullptr`
         *       when `len` is 0, by design, to allow views over `nullptr` for empty strings.
         * @complexity O(1).
         */
        constexpr LumexStringView(char const *str, size_type len) noexcept : m_data(str), m_size(len) {}

        /**
         * @brief Copy constructor. Creates a new `LumexStringView` that views the same data.
         * @details Performs a shallow copy. The new `LumexStringView` will point to the same
         *          character data as `other`, and have the same size.
         * @param other The `LumexStringView` to copy.
         * @complexity O(1).
         */
        constexpr LumexStringView(LumexStringView const &) noexcept = default;
        /**
         * @brief Copy assignment operator. Assigns the view of another `LumexStringView`.
         * @details Performs a shallow copy. This `LumexStringView` will point to the same
         *          character data as `other`, and have the same size.
         * @param other The `LumexStringView` to assign from.
         * @return A reference to `*this`.
         * @complexity O(1).
         */
        LumexStringView &operator=(LumexStringView const &) noexcept = default;
        /**
         * @brief Move constructor. Creates a new `LumexStringView` by moving from another.
         * @details Performs a shallow copy. Since `LumexStringView` is a non-owning type,
         *          move operations are effectively equivalent to copy operations.
         * @param other The `LumexStringView` to move from.
         * @complexity O(1).
         */
        constexpr LumexStringView(LumexStringView &&) noexcept = default;
        /**
         * @brief Move assignment operator. Assigns the view of another `LumexStringView` by moving.
         * @details Performs a shallow copy. Since `LumexStringView` is a non-owning type,
         *          move operations are effectively equivalent to copy operations.
         * @param other The `LumexStringView` to assign from.
         * @return A reference to `*this`.
         * @complexity O(1).
         */
        LumexStringView &operator=(LumexStringView &&) noexcept = default;
        /**
         * @brief Destructor.
         * @details Does nothing as `LumexStringView` does not own the character data.
         * @complexity O(1).
         */
        ~LumexStringView() = default;

        // -- Iterator support --
        /**
         * @brief Returns a const iterator to the first character of the view.
         * @warning It is not recommended to ignore the return value of `begin()`.
         * @return A `const_iterator` pointing to the first character.
         * @complexity O(1).
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of begin()")
        constexpr const_iterator
        begin() const noexcept
        {
          return m_data;
        }

        /**
         * @brief Returns a const iterator to the first character of the view.
         * @details Alias for `begin()`.
         * @warning It is not recommended to ignore the return value of `cbegin()`.
         * @return A `const_iterator` pointing to the first character.
         * @complexity O(1).
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of cbegin()")
        constexpr const_iterator
        cbegin() const noexcept
        {
          return m_data;
        }

        /**
         * @brief Returns a const iterator to the character following the last character of the view.
         * @details This character acts as a placeholder; attempting to dereference it results in undefined behavior.
         * @warning It is not recommended to ignore the return value of `end()`.
         * @return A `const_iterator` pointing one past the last character.
         * @complexity O(1).
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of end()")
        constexpr const_iterator
        end() const noexcept
        {
          return m_data + m_size; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }

        /**
         * @brief Returns a const iterator to the character following the last character of the view.
         * @details Alias for `end()`.
         * @warning It is not recommended to ignore the return value of `cend()`.
         * @return A `const_iterator` pointing one past the last character.
         * @complexity O(1).
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of cend()")
        constexpr const_iterator
        cend() const noexcept
        {
          return m_data + m_size; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }

        /**
         * @brief Returns a reverse iterator to the last character of the view.
         * @details The returned iterator points to the character that would be last in a reverse iteration.
         * @warning It is not recommended to ignore the return value of `rbegin()`.
         * @return A `const_reverse_iterator` pointing to the last character.
         * @complexity O(1).
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of rbegin()")
        const_reverse_iterator rbegin() const noexcept;

        /**
         * @brief Returns a const reverse iterator to the last character of the view.
         * @details Alias for `rbegin()`.
         * @warning It is not recommended to ignore the return value of `crbegin()`.
         * @return A `const_reverse_iterator` pointing to the last character.
         * @complexity O(1).
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of crbegin()")
        const_reverse_iterator crbegin() const noexcept;

        /**
         * @brief Returns a reverse iterator to the character preceding the first character of the view.
         * @details This character acts as a placeholder; attempting to dereference it results in undefined behavior.
         * @warning It is not recommended to ignore the return value of `rend()`.
         * @return A `const_reverse_iterator` pointing one before the first character.
         * @complexity O(1).
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of rend()")
        const_reverse_iterator rend() const noexcept;

        /**
         * @brief Returns a const reverse iterator to the character preceding the first character of the view.
         * @details Alias for `rend()`.
         * @warning It is not recommended to ignore the return value of `crend()`.
         * @return A `const_reverse_iterator` pointing one before the first character.
         * @complexity O(1).
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of crend()")
        const_reverse_iterator crend() const noexcept;

        // -- Capacity --
        /**
         * @brief Returns the number of characters in the view.
         * @warning It is not recommended to ignore the return value of `size()`.
         * @return The number of characters in the view.
         * @complexity O(1).
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of size()")
        constexpr size_type
        size() const noexcept
        {
          return m_size;
        }

        /**
         * @brief Returns the number of characters in the view.
         * @details Alias for `size()`.
         * @warning It is not recommended to ignore the return value of `length()`.
         * @return The number of characters in the view.
         * @complexity O(1).
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of length()")
        constexpr size_type
        length() const noexcept
        {
          return m_size;
        }

        /**
         * @brief Checks if the view is empty (i.e., has a size of 0).
         * @warning It is not recommended to ignore the return value of `empty()`.
         * @return `true` if the view is empty, `false` otherwise.
         * @complexity O(1).
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of empty()")
        constexpr bool
        empty() const noexcept
        {
          return m_size == 0;
        }

        /**
         * @brief Returns the maximum possible number of characters in a `LumexStringView`.
         * @details This is typically the largest possible value for `size_type` divided by 2,
         *          representing a practical limit on the view's length.
         * @return The maximum size.
         * @complexity O(1).
         */
        static constexpr size_type
        max_size() noexcept
        {
          return static_cast<size_type>(-1) / 2;
        }

        // -- Element access --
        /**
         * @brief Accesses the character at a specified position.
         * @details Returns a const reference to the character at index `idx`.
         *          No bounds checking is performed.
         * @param idx The zero-based index of the character to access.
         * @return A `const_reference` to the character at `idx`.
         * @warning Accessing an element beyond `[0, size() - 1]` results in undefined behavior.
         * @complexity O(1).
         */
        constexpr const_reference
        operator[](size_type idx) const noexcept
        {
          return m_data[idx]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }

        /**
         * @brief Accesses the character at a specified position with bounds checking.
         * @details Returns a const reference to the character at index `idx`.
         *          Throws `std::out_of_range` if `idx` is out of bounds.
         * @warning It is not recommended to ignore the return value of `at(size_type)`.
         * @param idx The zero-based index of the character to access.
         * @return A `const_reference` to the character at `idx`.
         * @throws std::out_of_range If `idx >= size()`.
         * @complexity O(1).
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of at(size_type)")
        const_reference at(size_type idx) const;

        /**
         * @brief Returns a const reference to the first character of the view.
         * @details Behavior is undefined if the view is empty.
         * @warning It is not recommended to ignore the return value of `front()`.
         * @return A `const_reference` to the first character.
         * @complexity O(1).
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of front()")
        constexpr const_reference
        front() const noexcept
        {
          return m_data[0]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }

        /**
         * @brief Returns a const reference to the last character of the view.
         * @details Behavior is undefined if the view is empty.
         * @warning It is not recommended to ignore the return value of `back()`.
         * @return A `const_reference` to the last character.
         * @complexity O(1).
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of back()")
        constexpr const_reference
        back() const noexcept
        {
          return m_data[m_size - 1]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }

        /**
         * @brief Returns a pointer to the beginning of the character data.
         * @details This pointer is typically non-null for non-empty views and `nullptr` for empty views.
         * @warning It is not recommended to ignore the return value of `data()`.
         * @return A `const_pointer` to the first character.
         * @complexity O(1).
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of data()")
        constexpr const_pointer
        data() const noexcept
        {
          return m_data;
        }

        // -- Modifiers (affecting the view, not the underlying data) --
        /**
         * @brief Clears the view, making it empty.
         * @details Sets the data pointer to `nullptr` and the size to `0`.
         * @post `empty()` is `true`, `size()` is `0`, `data()` is `nullptr`.
         * @complexity O(1).
         */
        void clear() noexcept;
        /**
         * @brief Removes `n` characters from the beginning of the view.
         * @details Adjusts the `m_data` pointer and `m_size` to effectively
         *          "chop off" the first `n` characters. If `n` is greater than `size()`,
         *          the view becomes empty.
         * @param n The number of characters to remove from the prefix.
         * @complexity O(1).
         */
        void remove_prefix(size_type n) noexcept;
        /**
         * @brief Removes `n` characters from the end of the view.
         * @details Adjusts the `m_size` to effectively "chop off" the last `n` characters.
         *          If `n` is greater than `size()`, the view becomes empty.
         * @param n The number of characters to remove from the suffix.
         * @complexity O(1).
         */
        void remove_suffix(size_type n) noexcept;
        /**
         * @brief Swaps the contents (data pointer and size) of this view with another.
         * @details This is an efficient, non-throwing swap operation that reassigns
         *          the views without touching the underlying character data.
         * @param other The `LumexStringView` to swap with.
         * @complexity O(1).
         */
        void swap(LumexStringView &other) noexcept;

        // -- Copy out --
        /**
         * @brief Copies characters from the view into a character array.
         * @details Copies up to `count` characters from this view, starting at `pos`,
         *          into the `dest` buffer. The actual number of characters copied is
         *          the smaller of `count` and `size() - pos`.
         * @param dest The destination character array to copy into.
         * @param count The maximum number of characters to copy.
         * @param pos The starting position in this `LumexStringView` from which to copy. Defaults to 0.
         * @return The number of characters actually copied.
         * @throws std::out_of_range If `pos > size()`.
         * @complexity O(N) where N is the number of characters copied.
         */
        size_type copy(char *dest, size_type count, size_type pos = 0) const;

        // -- Substring --
        /**
         * @brief Returns a new `LumexStringView` representing a substring of this view.
         * @details The new view will start at `pos` and extend for `n` characters.
         *          If `pos` is out of bounds, an exception is thrown. If `n` extends
         *          beyond the end of the current view, it is clamped to the remaining length.
         * @warning It is not recommended to ignore the return value of `substr(size_type, size_type)`.
         * @param pos The starting position of the substring. Defaults to 0.
         * @param n The length of the substring. Defaults to `npos` (until the end of the view).
         * @return A new `LumexStringView` object.
         * @throws std::out_of_range If `pos > size()`.
         * @complexity O(1).
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of substr(size_type, size_type)")
        LumexStringView substr(size_type pos = 0, size_type n = npos) const;

        // -- Comparison --
        /**
         * @brief Compares this `LumexStringView` with another `LumexStringView`.
         * @details Performs a lexicographical comparison.
         * @warning It is not recommended to ignore the return value of `compare(LumexStringView)`.
         * @param other The `LumexStringView` to compare with.
         * @return An integer representing the comparison result:
         *         - Less than 0 if `*this` is lexicographically less than `other`.
         *         - 0 if `*this` is lexicographically equal to `other`.
         *         - Greater than 0 if `*this` is lexicographically greater than `other`.
         * @complexity O(N) where N is the minimum length of the two views.
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of compare(LumexStringView)")
        int compare(LumexStringView other) const noexcept;

        /**
         * @brief Compares a substring of this `LumexStringView` with another `LumexStringView`.
         * @details Extracts a substring from `*this` starting at `pos` with length `len`,
         *          and then compares that substring lexicographically with `other`.
         * @warning It is not recommended to ignore the return value of `compare(size_type, size_type,
         * LumexStringView)`.
         * @param pos The starting position of the substring in `*this`.
         * @param len The length of the substring in `*this`.
         * @param other The `LumexStringView` to compare with.
         * @return An integer representing the comparison result (see `compare(LumexStringView)`).
         * @throws std::out_of_range If `pos > size()`.
         * @complexity O(N) where N is the minimum length of the compared substring and `other`.
         */
        LUMEX_ATTRIBUTE_NODISCARD(
          "It is not recommended to ignore return value of compare(size_type, size_type, LumexStringView)")
        int compare(size_type pos, size_type len, LumexStringView other) const;

        /**
         * @brief Compares this `LumexStringView` with a null-terminated C-style string.
         * @details Converts `cstr` to a `LumexStringView` internally and then performs a comparison.
         * @param cstr The null-terminated C-style string to compare with.
         * @return An integer representing the comparison result (see `compare(LumexStringView)`).
         * @complexity O(N) where N is the minimum length of `*this` and `cstr`.
         */
        int compare(char const *cstr) const;

        // -- Starts / ends / contains helpers --
        /**
         * @brief Checks if the `LumexStringView` starts with a specific character.
         * @warning It is not recommended to ignore the return value of `starts_with(char)`.
         * @param chr The character to check for at the beginning of the view.
         * @return `true` if the view is not empty and its first character is `chr`, `false` otherwise.
         * @complexity O(1).
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of starts_with(char)")
        bool starts_with(char chr) const noexcept;

        /**
         * @brief Checks if the `LumexStringView` starts with a specific `LumexStringView`.
         * @warning It is not recommended to ignore the return value of `starts_with(LumexStringView)`.
         * @param str The `LumexStringView` to check for at the beginning of the view.
         * @return `true` if the view has `str` as a prefix, `false` otherwise.
         * @complexity O(N) where N is `str.size()`.
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of starts_with(LumexStringView)")
        bool starts_with(LumexStringView str) const noexcept;

        /**
         * @brief Checks if the `LumexStringView` ends with a specific character.
         * @warning It is not recommended to ignore the return value of `ends_with(char)`.
         * @param chr The character to check for at the end of the view.
         * @return `true` if the view is not empty and its last character is `chr`, `false` otherwise.
         * @complexity O(1).
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of ends_with(char)")
        bool ends_with(char chr) const noexcept;

        /**
         * @brief Checks if the `LumexStringView` ends with a specific `LumexStringView`.
         * @warning It is not recommended to ignore the return value of `ends_with(LumexStringView)`.
         * @param str The `LumexStringView` to check for at the end of the view.
         * @return `true` if the view has `str` as a suffix, `false` otherwise.
         * @complexity O(N) where N is `str.size()`.
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of ends_with(chaLumexStringViewr)")
        bool ends_with(LumexStringView str) const noexcept;

        // -- Find (simple implementations) --
        /**
         * @brief Finds the first occurrence of a character within the view.
         * @details Searches for `chr` starting from `pos`.
         * @warning It is not recommended to ignore the return value of `find(char, size_type)`.
         * @param chr The character to search for.
         * @param pos The starting position for the search. Defaults to 0.
         * @return The zero-based index of the first occurrence of `chr`, or `npos` if not found.
         * @complexity O(N) where N is `size() - pos`.
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of find(char, size_type)")
        size_type find(char chr, size_type pos = 0) const noexcept;

        /**
         * @brief Finds the first occurrence of a `LumexStringView` within this view.
         * @details Searches for `str` starting from `pos`.
         * @warning It is not recommended to ignore the return value of `find(LumexStringView, size_type)`.
         * @param str The `LumexStringView` to search for.
         * @param pos The starting position for the search. Defaults to 0.
         * @return The zero-based index of the first occurrence of `str`, or `npos` if not found.
         * @complexity O(N*M) in worst case, where N is `size() - pos` and M is `str.size()`.
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of find(LumexStringView, size_type)")
        size_type find(LumexStringView str, size_type pos = 0) const noexcept;

        /**
         * @brief Finds the first occurrence of a C-style string within this view with a specified count.
         * @details Searches for the first `count` characters of `cstr` starting from `pos`.
         * @param cstr The C-style string to search for.
         * @param pos The starting position for the search.
         * @param count The number of characters from `cstr` to use for the search.
         * @return The zero-based index of the first occurrence, or `npos` if not found.
         * @complexity O(N*M) in worst case, where N is `size() - pos` and M is `count`.
         */
        size_type find(char const *cstr, size_type pos, size_type count) const noexcept;

        /**
         * @brief Finds the first occurrence of a null-terminated C-style string within this view.
         * @details Searches for `cstr` (up to its null terminator) starting from `pos`.
         * @param cstr The null-terminated C-style string to search for.
         * @param pos The starting position for the search. Defaults to 0.
         * @return The zero-based index of the first occurrence, or `npos` if not found.
         * @complexity O(N*M) in worst case, where N is `size() - pos` and M is `strlen(cstr)`.
         */
        size_type find(char const *cstr, size_type pos = 0) const noexcept;

        // -- Reverse find --
        /**
         * @brief Finds the last occurrence of a `LumexStringView` within this view.
         * @details Searches backward for `str` starting from `pos`.
         * @warning It is not recommended to ignore the return value of `rfind(LumexStringView, size_type)`.
         * @param str The `LumexStringView` to search for.
         * @param pos The starting position for the reverse search. Defaults to `npos` (end of view).
         * @return The zero-based index of the last occurrence of `str`, or `npos` if not found.
         * @complexity O(N*M) in worst case, where N is `pos` and M is `str.size()`.
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of rfind(LumexStringView, size_type)")
        size_type rfind(LumexStringView str, size_type pos = npos) const noexcept;

        /**
         * @brief Finds the last occurrence of a character within the view.
         * @details Searches backward for `chr` starting from `pos`.
         * @warning It is not recommended to ignore the return value of `rfind(char, size_type)`.
         * @param chr The character to search for.
         * @param pos The starting position for the reverse search. Defaults to `npos` (end of view).
         * @return The zero-based index of the last occurrence of `chr`, or `npos` if not found.
         * @complexity O(N) where N is `pos`.
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of rfind(char, size_type)")
        size_type rfind(char chr, size_type pos = npos) const noexcept;

        /**
         * @brief Finds the last occurrence of a C-style string within this view with a specified count.
         * @details Searches backward for the last `count` characters of `cstr` starting from `pos`.
         * @param cstr The C-style string to search for.
         * @param pos The starting position for the reverse search.
         * @param count The number of characters from `cstr` to use for the search.
         * @return The zero-based index of the last occurrence, or `npos` if not found.
         * @complexity O(N*M) in worst case, where N is `pos` and M is `count`.
         */
        size_type rfind(char const *cstr, size_type pos, size_type count) const noexcept;

        /**
         * @brief Finds the last occurrence of a null-terminated C-style string within this view.
         * @details Searches backward for `cstr` (up to its null terminator) starting from `pos`.
         * @param cstr The null-terminated C-style string to search for.
         * @param pos The starting position for the reverse search. Defaults to `npos` (end of view).
         * @return The zero-based index of the last occurrence, or `npos` if not found.
         * @complexity O(N*M) in worst case, where N is `pos` and M is `strlen(cstr)`.
         */
        size_type rfind(char const *cstr, size_type pos = npos) const noexcept;

        // -- Find first of (finds any character from a set) --
        /**
         * @brief Finds the first occurrence of any character from a given set of characters.
         * @details Searches for the first character in `*this` that matches any character in `str`, starting from
         * `pos`.
         * @warning It is not recommended to ignore the return value of `find_first_of(LumexStringView, size_type)`.
         * @param str A `LumexStringView` containing the set of characters to search for.
         * @param pos The starting position for the search. Defaults to 0.
         * @return The zero-based index of the first match, or `npos` if no character is found.
         * @complexity O(N*M) in worst case, where N is `size() - pos` and M is `str.size()`.
         */
        LUMEX_ATTRIBUTE_NODISCARD(
          "It is not recommended to ignore return value of find_first_of(LumexStringView, size_type)")
        size_type find_first_of(LumexStringView str, size_type pos = 0) const noexcept;

        /**
         * @brief Finds the first occurrence of a specific character.
         * @details Alias for `find(char, size_type)`.
         * @warning It is not recommended to ignore the return value of `find_first_of(char, size_type)`.
         * @param chr The character to search for.
         * @param pos The starting position for the search. Defaults to 0.
         * @return The zero-based index of the first match, or `npos` if not found.
         * @complexity O(N) where N is `size() - pos`.
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of find_first_of(char, size_type)")
        size_type find_first_of(char chr, size_type pos = 0) const noexcept;

        /**
         * @brief Finds the first occurrence of any character from a C-style string set with a specified count.
         * @details Searches for the first character in `*this` that matches any of the first `count` characters of
         * `cstr`, starting from `pos`.
         * @param cstr A C-style string containing the set of characters to search for.
         * @param pos The starting position for the search.
         * @param count The number of characters from `cstr` to use as the set.
         * @return The zero-based index of the first match, or `npos` if no character is found.
         * @complexity O(N*M) in worst case, where N is `size() - pos` and M is `count`.
         */
        size_type find_first_of(char const *cstr, size_type pos, size_type count) const noexcept;

        /**
         * @brief Finds the first occurrence of any character from a null-terminated C-style string set.
         * @details Searches for the first character in `*this` that matches any character in `cstr` (up to its null
         * terminator), starting from `pos`.
         * @param cstr A null-terminated C-style string containing the set of characters to search for.
         * @param pos The starting position for the search. Defaults to 0.
         * @return The zero-based index of the first match, or `npos` if no character is found.
         * @complexity O(N*M) in worst case, where N is `size() - pos` and M is `strlen(cstr)`.
         */
        size_type find_first_of(char const *cstr, size_type pos = 0) const noexcept;

        // -- Find last of (finds any character from a set, searching backward) --
        /**
         * @brief Finds the last occurrence of any character from a given set of characters.
         * @details Searches backward for the last character in `*this` that matches any character in `str`, starting
         * from `pos`.
         * @warning It is not recommended to ignore the return value of `find_last_of(LumexStringView, size_type)`.
         * @param str A `LumexStringView` containing the set of characters to search for.
         * @param pos The starting position for the reverse search. Defaults to `npos`.
         * @return The zero-based index of the last match, or `npos` if no character is found.
         * @complexity O(N*M) in worst case, where N is `pos` and M is `str.size()`.
         */
        LUMEX_ATTRIBUTE_NODISCARD(
          "It is not recommended to ignore return value of find_last_of(LumexStringView, size_type)")
        size_type find_last_of(LumexStringView str, size_type pos = npos) const noexcept;

        /**
         * @brief Finds the last occurrence of a specific character.
         * @details Alias for `rfind(char, size_type)`.
         * @warning It is not recommended to ignore the return value of `find_last_of(char, size_type)`.
         * @param chr The character to search for.
         * @param pos The starting position for the reverse search. Defaults to `npos`.
         * @return The zero-based index of the last match, or `npos` if not found.
         * @complexity O(N) where N is `pos`.
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of find_last_of(char, size_type)")
        size_type find_last_of(char chr, size_type pos = npos) const noexcept;

        /**
         * @brief Finds the last occurrence of any character from a C-style string set with a specified count.
         * @details Searches backward for the last character in `*this` that matches any of the first `count` characters
         * of `cstr`, starting from `pos`.
         * @param cstr A C-style string containing the set of characters to search for.
         * @param pos The starting position for the reverse search.
         * @param count The number of characters from `cstr` to use as the set.
         * @return The zero-based index of the last match, or `npos` if no character is found.
         * @complexity O(N*M) in worst case, where N is `pos` and M is `count`.
         */
        size_type find_last_of(char const *cstr, size_type pos, size_type count) const noexcept;

        /**
         * @brief Finds the last occurrence of any character from a null-terminated C-style string set.
         * @details Searches backward for the last character in `*this` that matches any character in `cstr` (up to its
         * null terminator), starting from `pos`.
         * @param cstr A null-terminated C-style string containing the set of characters to search for.
         * @param pos The starting position for the reverse search. Defaults to `npos`.
         * @return The zero-based index of the last match, or `npos` if no character is found.
         * @complexity O(N*M) in worst case, where N is `pos` and M is `strlen(cstr)`.
         */
        size_type find_last_of(char const *cstr, size_type pos = npos) const noexcept;

        // -- Find first not of (finds first character *not* from a set) --
        /**
         * @brief Finds the first occurrence of any character *not* from a given set of characters.
         * @details Searches for the first character in `*this` that does *not* match any character in `str`, starting
         * from `pos`.
         * @warning It is not recommended to ignore the return value of `find_first_not_of(LumexStringView, size_type)`.
         * @param str A `LumexStringView` containing the set of characters to exclude.
         * @param pos The starting position for the search. Defaults to 0.
         * @return The zero-based index of the first non-matching character, or `npos` if all characters match the set.
         * @complexity O(N*M) in worst case, where N is `size() - pos` and M is `str.size()`.
         */
        LUMEX_ATTRIBUTE_NODISCARD(
          "It is not recommended to ignore return value of find_first_not_of(LumexStringView, size_type)")
        size_type find_first_not_of(LumexStringView str, size_type pos = 0) const noexcept;

        /**
         * @brief Finds the first occurrence of a character *not* equal to a specific character.
         * @details Searches for the first character in `*this` that is not `chr`, starting from `pos`.
         * @warning It is not recommended to ignore the return value of `find_first_not_of(char, size_type)`.
         * @param chr The character to exclude.
         * @param pos The starting position for the search. Defaults to 0.
         * @return The zero-based index of the first non-matching character, or `npos` if all characters are `chr`.
         * @complexity O(N) where N is `size() - pos`.
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of find_first_not_of(char, size_type)")
        size_type find_first_not_of(char chr, size_type pos = 0) const noexcept;

        /**
         * @brief Finds the first occurrence of any character *not* from a C-style string set with a specified count.
         * @details Searches for the first character in `*this` that does *not* match any of the first `count`
         * characters of `cstr`, starting from `pos`.
         * @param cstr A C-style string containing the set of characters to exclude.
         * @param pos The starting position for the search.
         * @param count The number of characters from `cstr` to use as the set.
         * @return The zero-based index of the first non-matching character, or `npos` if all characters match the set.
         * @complexity O(N*M) in worst case, where N is `size() - pos` and M is `count`.
         */
        size_type find_first_not_of(char const *cstr, size_type pos, size_type count) const noexcept;

        /**
         * @brief Finds the first occurrence of any character *not* from a null-terminated C-style string set.
         * @details Searches for the first character in `*this` that does *not* match any character in `cstr` (up to its
         * null terminator), starting from `pos`.
         * @param cstr A null-terminated C-style string containing the set of characters to exclude.
         * @param pos The starting position for the search. Defaults to 0.
         * @return The zero-based index of the first non-matching character, or `npos` if all characters match the set.
         * @complexity O(N*M) in worst case, where N is `size() - pos` and M is `strlen(cstr)`.
         */
        size_type find_first_not_of(char const *cstr, size_type pos = 0) const noexcept;

        // -- Find last not of (finds last character *not* from a set, searching backward) --
        /**
         * @brief Finds the last occurrence of any character *not* from a given set of characters.
         * @details Searches backward for the last character in `*this` that does *not* match any character in `str`,
         * starting from `pos`.
         * @warning It is not recommended to ignore the return value of `find_last_not_of(LumexStringView, size_type)`.
         * @param str A `LumexStringView` containing the set of characters to exclude.
         * @param pos The starting position for the reverse search. Defaults to `npos`.
         * @return The zero-based index of the last non-matching character, or `npos` if all characters match the set.
         * @complexity O(N*M) in worst case, where N is `pos` and M is `str.size()`.
         */
        LUMEX_ATTRIBUTE_NODISCARD(
          "It is not recommended to ignore return value of find_last_not_of(LumexStringView, size_type)")
        size_type find_last_not_of(LumexStringView str, size_type pos = npos) const noexcept;

        /**
         * @brief Finds the last occurrence of a character *not* equal to a specific character.
         * @details Searches backward for the last character in `*this` that is not `chr`, starting from `pos`.
         * @warning It is not recommended to ignore the return value of `find_last_not_of(char, size_type)`.
         * @param chr The character to exclude.
         * @param pos The starting position for the reverse search. Defaults to `npos`.
         * @return The zero-based index of the last non-matching character, or `npos` if all characters are `chr`.
         * @complexity O(N) where N is `pos`.
         */
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of find_last_not_of(char, size_type)")
        size_type find_last_not_of(char chr, size_type pos = npos) const noexcept;

        /**
         * @brief Finds the last occurrence of any character *not* from a C-style string set with a specified count.
         * @details Searches backward for the last character in `*this` that does *not* match any of the first `count`
         * characters of `cstr`, starting from `pos`.
         * @param cstr A C-style string containing the set of characters to exclude.
         * @param pos The starting position for the reverse search.
         * @param count The number of characters from `cstr` to use as the set.
         * @return The zero-based index of the last non-matching character, or `npos` if all characters match the set.
         * @complexity O(N*M) in worst case, where N is `pos` and M is `count`.
         */
        size_type find_last_not_of(char const *cstr, size_type pos, size_type count) const noexcept;

        /**
         * @brief Finds the last occurrence of any character *not* from a null-terminated C-style string set.
         * @details Searches backward for the last character in `*this` that does *not* match any character in `cstr`
         * (up to its null terminator), starting from `pos`.
         * @param cstr A null-terminated C-style string containing the set of characters to exclude.
         * @param pos The starting position for the reverse search. Defaults to `npos`.
         * @return The zero-based index of the last non-matching character, or `npos` if all characters match the set.
         * @complexity O(N*M) in worst case, where N is `pos` and M is `strlen(cstr)`.
         */
        size_type find_last_not_of(char const *cstr, size_type pos = npos) const noexcept;

        // -- Conversion back to std::string --
        /**
         * @brief Explicit conversion operator to `std::basic_string<char>`.
         * @details Constructs a new `std::basic_string` (or `std::string`) containing a copy
         *          of the characters viewed by this `LumexStringView`.
         * @tparam Allocator The allocator type for the `std::basic_string`. Defaults to `std::allocator<char>`.
         * @return A new `std::basic_string` object.
         * @complexity O(N) where N is `size()`.
         */
        template <class Allocator = std::allocator<char>>
        explicit
        operator std::basic_string<char, std::char_traits<char>, Allocator>() const
        {
          return std::basic_string<char, std::char_traits<char>, Allocator>(m_data, m_size);
        }

        /**
         * @brief Converts the `LumexStringView` to a `std::basic_string<char>`.
         * @details Constructs a new `std::basic_string` (or `std::string`) containing a copy
         *          of the characters viewed by this `LumexStringView`.
         * @tparam Allocator The allocator type for the `std::basic_string`. Defaults to `std::allocator<char>`.
         * @param alloc An allocator object to use for the new string. Defaults to a default-constructed allocator.
         * @return A new `std::basic_string` object.
         * @complexity O(N) where N is `size()`.
         */
        template <class Allocator = std::allocator<char>>
        std::basic_string<char, std::char_traits<char>, Allocator>
        to_string(Allocator const &alloc = Allocator()) const
        {
          return std::basic_string<char, std::char_traits<char>, Allocator>(m_data, m_size, alloc);
        }

        /**
         * @brief Constructs a `LumexStringView` from a `std::basic_string<char>`.
         * @details Creates a view over the internal character data of the provided `std::basic_string`.
         * @tparam Allocator The allocator type of the `std::basic_string`.
         * @param str The `std::basic_string` to view.
         * @note The `LumexStringView` does not own the string data; `str` must outlive the view.
         * @complexity O(1).
         */
        template <class Allocator>
        explicit LumexStringView(std::basic_string<char, std::char_traits<char>, Allocator> const &str) noexcept
            : m_data(str.data()), m_size(str.size())
        {}

        /**
         * @brief Constructs a `LumexStringView` from a `std::string`.
         * @details Enables implicit instantiation of the default allocator for `std::string`.
         *          Creates a view over the internal character data of the provided `std::string`.
         * @param str The `std::string` to view.
         * @note The `LumexStringView` does not own the string data; `str` must outlive the view.
         * @complexity O(1).
         */
        explicit LumexStringView(std::string const &str) noexcept;

      private:
        /**
         * @brief Pointer to the beginning of the character sequence.
         * @details This member holds the address of the first character in the viewed sequence.
         */
        char const *m_data;
        /**
         * @brief The number of characters in the viewed sequence.
         * @details This member stores the length of the string view.
         */
        size_type m_size;
      };

      // -- Non-member relational operators --
      /**
       * @brief Equality comparison operator for two `LumexStringView` objects.
       * @details Compares two `LumexStringView` objects for lexicographical equality.
       * @param lhs The left-hand side `LumexStringView`.
       * @param rhs The right-hand side `LumexStringView`.
       * @return `true` if both views are of the same size and their contents are identical, `false` otherwise.
       * @complexity O(N) where N is the minimum length of the two views.
       */
      LUMEX_API inline bool
      operator==(LumexStringView lhs, LumexStringView rhs) noexcept
      {
        return lhs.size() == rhs.size() && lhs.compare(rhs) == 0;
      }

      /**
       * @brief Inequality comparison operator for two `LumexStringView` objects.
       * @details Checks if two `LumexStringView` objects are not lexicographically equal.
       * @param lhs The left-hand side `LumexStringView`.
       * @param rhs The right-hand side `LumexStringView`.
       * @return `true` if the views are not equal, `false` otherwise.
       * @complexity O(N) where N is the minimum length of the two views.
       */
      LUMEX_API inline bool
      operator!=(LumexStringView lhs, LumexStringView rhs) noexcept
      {
        return !(lhs == rhs);
      }

      /**
       * @brief Less-than comparison operator for two `LumexStringView` objects.
       * @details Compares two `LumexStringView` objects lexicographically.
       * @param lhs The left-hand side `LumexStringView`.
       * @param rhs The right-hand side `LumexStringView`.
       * @return `true` if `lhs` is lexicographically less than `rhs`, `false` otherwise.
       * @complexity O(N) where N is the minimum length of the two views.
       */
      LUMEX_API inline bool
      operator<(LumexStringView lhs, LumexStringView rhs) noexcept
      {
        return lhs.compare(rhs) < 0;
      }

      /**
       * @brief Greater-than comparison operator for two `LumexStringView` objects.
       * @details Compares two `LumexStringView` objects lexicographically.
       * @param lhs The left-hand side `LumexStringView`.
       * @param rhs The right-hand side `LumexStringView`.
       * @return `true` if `lhs` is lexicographically greater than `rhs`, `false` otherwise.
       * @complexity O(N) where N is the minimum length of the two views.
       */
      LUMEX_API inline bool
      operator>(LumexStringView lhs, LumexStringView rhs) noexcept
      {
        return lhs.compare(rhs) > 0;
      }

      /**
       * @brief Less-than-or-equal-to comparison operator for two `LumexStringView` objects.
       * @details Compares two `LumexStringView` objects lexicographically.
       * @param lhs The left-hand side `LumexStringView`.
       * @param rhs The right-hand side `LumexStringView`.
       * @return `true` if `lhs` is lexicographically less than or equal to `rhs`, `false` otherwise.
       * @complexity O(N) where N is the minimum length of the two views.
       */
      LUMEX_API inline bool
      operator<=(LumexStringView lhs, LumexStringView rhs) noexcept
      {
        return lhs.compare(rhs) <= 0;
      }

      /**
       * @brief Greater-than-or-equal-to comparison operator for two `LumexStringView` objects.
       * @details Compares two `LumexStringView` objects lexicographically.
       * @param lhs The left-hand side `LumexStringView`.
       * @param rhs The right-hand side `LumexStringView`.
       * @return `true` if `lhs` is lexicographically greater than or equal to `rhs`, `false` otherwise.
       * @complexity O(N) where N is the minimum length of the two views.
       */
      LUMEX_API inline bool
      operator>=(LumexStringView lhs, LumexStringView rhs) noexcept
      {
        return lhs.compare(rhs) >= 0;
      }

      // -- Stream inserter --
      /**
       * @brief Overload for inserting a `LumexStringView` into an `std::ostream`.
       * @details This operator allows `LumexStringView` objects to be printed directly
       *          to standard output streams. It writes the viewed character data to the stream.
       * @param ostr The output stream.
       * @param sview The `LumexStringView` to insert.
       * @return A reference to the output stream.
       * @complexity O(N) where N is `sview.size()`.
       */
      LUMEX_API std::ostream &operator<<(std::ostream &ostr, LumexStringView sview);
    } // namespace StringView
  } // namespace Core
} // namespace Lumex

/**
 * @brief Global type alias for `Lumex::Core::StringView::LumexStringView`.
 * @details This `using` declaration brings `LumexStringView` into the global namespace
 *          (or enclosing namespace where it's included), allowing for more convenient
 *          usage without full namespace qualification, similar to `std::string_view`.
 */
using LumexStringView = Lumex::Core::StringView::LumexStringView;

#endif // LUMEX_STRING_VIEW_HPP
