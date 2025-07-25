#ifndef LUMEX_STRING_VIEW_HPP
#define LUMEX_STRING_VIEW_HPP

//  C++11 lightweight, non-owning view over character sequences.
//  Similar to std::string_view (C++17) but implemented manually.
//  The implementation is cross-platform (Windows / Linux / macOS) and relies
//  solely on the C++11 standard library.
//  It respects exception-safety, thread-safety (concurrent read-only access),
//  and does not perform dynamic allocation.

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
    namespace StringView
    {
      class LUMEX_API LumexStringView
      {
      public:
        // -- Public type aliases --
        using value_type             = char;
        using pointer                = char const *;
        using const_pointer          = char const *;
        using reference              = char const &;
        using const_reference        = char const &;
        using iterator               = const_pointer; // iterator == const_iterator
        using const_iterator         = const_pointer;
        using reverse_iterator       = std::reverse_iterator<const_pointer>;
        using const_reverse_iterator = std::reverse_iterator<const_pointer>;
        using size_type              = std::size_t;
        using difference_type        = std::ptrdiff_t;

        static size_type const npos  = static_cast<size_type>(-1);

        // -- Construction / assignment --
        constexpr LumexStringView() noexcept : m_data(nullptr), m_size(0) {}

        // From C-string (constexpr friendly, does not throw)
        explicit LumexStringView(char const *str) noexcept;

        // From pointer + size (does **not** check nullptr when len==0 on purpose)
        constexpr LumexStringView(char const *str, size_type len) noexcept : m_data(str), m_size(len) {}

        // Rule of five conformance
        constexpr LumexStringView(LumexStringView const &) noexcept  = default;
        LumexStringView &operator=(LumexStringView const &) noexcept = default;
        constexpr LumexStringView(LumexStringView &&) noexcept       = default;
        LumexStringView &operator=(LumexStringView &&) noexcept      = default;
        ~LumexStringView()                                           = default;

        // -- Iterator support --
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of begin()")
        constexpr const_iterator
        begin() const noexcept
        {
          return m_data;
        }

        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of cbegin()")
        constexpr const_iterator
        cbegin() const noexcept
        {
          return m_data;
        }

        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of end()")
        constexpr const_iterator
        end() const noexcept
        {
          return m_data + m_size; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }

        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of cend()")
        constexpr const_iterator
        cend() const noexcept
        {
          return m_data + m_size; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }

        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of rbegin()")
        const_reverse_iterator rbegin() const noexcept;

        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of crbegin()")
        const_reverse_iterator crbegin() const noexcept;

        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of rend()")
        const_reverse_iterator rend() const noexcept;

        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of crend()")
        const_reverse_iterator crend() const noexcept;

        // -- Capacity --
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of size()")
        constexpr size_type
        size() const noexcept
        {
          return m_size;
        }

        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of length()")
        constexpr size_type
        length() const noexcept
        {
          return m_size;
        }

        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of empty()")
        constexpr bool
        empty() const noexcept
        {
          return m_size == 0;
        }

        static constexpr size_type
        max_size() noexcept
        {
          return static_cast<size_type>(-1) / 2;
        }

        // -- Element access --
        constexpr const_reference
        operator[](size_type idx) const noexcept
        {
          return m_data[idx]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }

        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of at(size_type)")
        const_reference at(size_type idx) const;

        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of front()")
        constexpr const_reference
        front() const noexcept
        {
          return m_data[0]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }

        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of back()")
        constexpr const_reference
        back() const noexcept
        {
          return m_data[m_size - 1]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }

        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of data()")
        constexpr const_pointer
        data() const noexcept
        {
          return m_data;
        }

        // -- Modifiers --
        void clear() noexcept;
        void remove_prefix(size_type n) noexcept;
        void remove_suffix(size_type n) noexcept;
        void swap(LumexStringView &other) noexcept;

        // -- Copy out --
        size_type copy(char *dest, size_type count, size_type pos = 0) const;

        // -- Substring --
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of substr(size_type, size_type)")
        LumexStringView substr(size_type pos = 0, size_type n = npos) const;

        // -- Comparison --
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of compare(LumexStringView)")
        int compare(LumexStringView other) const noexcept;

        LUMEX_ATTRIBUTE_NODISCARD(
          "It is not recommended to ignore return value of compare(size_type, size_type, LumexStringView)")
        int compare(size_type pos, size_type len, LumexStringView other) const;

        int compare(char const *cstr) const;

        // -- Starts / ends / contains helpers --
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of starts_with(char)")
        bool starts_with(char chr) const noexcept;

        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of starts_with(LumexStringView)")
        bool starts_with(LumexStringView str) const noexcept;

        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of ends_with(char)")
        bool ends_with(char chr) const noexcept;

        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of ends_with(chaLumexStringViewr)")
        bool ends_with(LumexStringView str) const noexcept;

        // -- Find (simple implementations) --
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of find(char, size_type)")
        size_type find(char chr, size_type pos = 0) const noexcept;

        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of find(LumexStringView, size_type)")
        size_type find(LumexStringView str, size_type pos = 0) const noexcept;

        size_type find(char const *cstr, size_type pos, size_type count) const noexcept;

        size_type find(char const *cstr, size_type pos = 0) const noexcept;

        // -- Reverse find --
        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of rfind(LumexStringView, size_type)")
        size_type rfind(LumexStringView str, size_type pos = npos) const noexcept;

        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of rfind(char, size_type)")
        size_type rfind(char chr, size_type pos = npos) const noexcept;

        size_type rfind(char const *cstr, size_type pos, size_type count) const noexcept;

        size_type rfind(char const *cstr, size_type pos = npos) const noexcept;

        // -- Find first of --
        LUMEX_ATTRIBUTE_NODISCARD(
          "It is not recommended to ignore return value of find_first_of(LumexStringView, size_type)")
        size_type find_first_of(LumexStringView str, size_type pos = 0) const noexcept;

        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of find_first_of(char, size_type)")
        size_type find_first_of(char chr, size_type pos = 0) const noexcept;

        size_type find_first_of(char const *cstr, size_type pos, size_type count) const noexcept;

        size_type find_first_of(char const *cstr, size_type pos = 0) const noexcept;

        // -- Find last of --
        LUMEX_ATTRIBUTE_NODISCARD(
          "It is not recommended to ignore return value of find_last_of(LumexStringView, size_type)")
        size_type find_last_of(LumexStringView str, size_type pos = npos) const noexcept;

        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of find_last_of(char, size_type)")
        size_type find_last_of(char chr, size_type pos = npos) const noexcept;

        size_type find_last_of(char const *cstr, size_type pos, size_type count) const noexcept;

        size_type find_last_of(char const *cstr, size_type pos = npos) const noexcept;

        // -- Find first not of --
        LUMEX_ATTRIBUTE_NODISCARD(
          "It is not recommended to ignore return value of find_first_not_of(LumexStringView, size_type)")
        size_type find_first_not_of(LumexStringView str, size_type pos = 0) const noexcept;

        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of find_first_not_of(char, size_type)")
        size_type find_first_not_of(char chr, size_type pos = 0) const noexcept;

        size_type find_first_not_of(char const *cstr, size_type pos, size_type count) const noexcept;

        size_type find_first_not_of(char const *cstr, size_type pos = 0) const noexcept;

        // -- Find last not of --
        LUMEX_ATTRIBUTE_NODISCARD(
          "It is not recommended to ignore return value of find_last_not_of(LumexStringView, size_type)")
        size_type find_last_not_of(LumexStringView str, size_type pos = npos) const noexcept;

        LUMEX_ATTRIBUTE_NODISCARD("It is not recommended to ignore return value of find_last_not_of(char, size_type)")
        size_type find_last_not_of(char chr, size_type pos = npos) const noexcept;

        size_type find_last_not_of(char const *cstr, size_type pos, size_type count) const noexcept;

        size_type find_last_not_of(char const *cstr, size_type pos = npos) const noexcept;

        // -- Conversion back to std::string --
        template <class Allocator = std::allocator<char>>
        explicit
        operator std::basic_string<char, std::char_traits<char>, Allocator>() const
        {
          return std::basic_string<char, std::char_traits<char>, Allocator>(m_data, m_size);
        }

        template <class Allocator = std::allocator<char>>
        std::basic_string<char, std::char_traits<char>, Allocator>
        to_string(Allocator const &alloc = Allocator()) const
        {
          return std::basic_string<char, std::char_traits<char>, Allocator>(m_data, m_size, alloc);
        }

        template <class Allocator>
        explicit LumexStringView(std::basic_string<char, std::char_traits<char>, Allocator> const &str) noexcept
            : m_data(str.data()), m_size(str.size())
        {}

        // enable construction from std::string by implicit instantiation of default allocator
        explicit LumexStringView(std::string const &str) noexcept;

      private:
        char const *m_data;
        size_type m_size;
      };

      // -- Non-member relational operators --
      LUMEX_API inline bool
      operator==(LumexStringView lhs, LumexStringView rhs) noexcept
      {
        return lhs.size() == rhs.size() && lhs.compare(rhs) == 0;
      }

      LUMEX_API inline bool
      operator!=(LumexStringView lhs, LumexStringView rhs) noexcept
      {
        return !(lhs == rhs);
      }

      LUMEX_API inline bool
      operator<(LumexStringView lhs, LumexStringView rhs) noexcept
      {
        return lhs.compare(rhs) < 0;
      }

      LUMEX_API inline bool
      operator>(LumexStringView lhs, LumexStringView rhs) noexcept
      {
        return lhs.compare(rhs) > 0;
      }

      LUMEX_API inline bool
      operator<=(LumexStringView lhs, LumexStringView rhs) noexcept
      {
        return lhs.compare(rhs) <= 0;
      }

      LUMEX_API inline bool
      operator>=(LumexStringView lhs, LumexStringView rhs) noexcept
      {
        return lhs.compare(rhs) >= 0;
      }

      // -- Stream inserter --
      LUMEX_API std::ostream &operator<<(std::ostream &ostr, LumexStringView sview);
    } // namespace StringView
  } // namespace Core
} // namespace Lumex

using LumexStringView = Lumex::Core::StringView::LumexStringView;

#endif // LUMEX_STRING_VIEW_HPP
