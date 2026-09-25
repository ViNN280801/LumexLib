/**
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/**
 * @file LumexFormat.hpp
 * @brief `std::format`-style text formatting from C++11 on.
 *
 * @details `format ("{:>8.3f} ms", value)` builds a `std::string` from a
 * format string with replacement fields and the standard format
 * specification mini-language:
 * `[[fill]align][sign][#][0][width][.precision][L][type]`.
 *
 * - Arguments: automatic `{}`, positional `{0}`, named `{name}` (pass
 *   `arg ("name", value)`), dynamic width / precision `{:{}.{}}`.
 * - Built-in types: every integer type (plus `__int128` where available),
 *   `bool`, the character type, `float` / `double` / `long double`, C strings,
 *   `std::basic_string`, `std::basic_string_view`, `LumexStringView`,
 *   `void const *` / `std::nullptr_t`, and enums declared with
 *   `LUMEX_DEFINE_REFLECTED_ENUM` (name by default, number with an integer
 *   presentation type).
 * - User types: specialize `Formatter<T, Char>`; types with `operator<<`
 *   opt in through `OstreamFormatter` or `streamed (value)`.
 * - `char` and `wchar_t` format strings (`format (L"{}", 1)` returns
 *   `std::wstring`).
 * - `L` uses the grouping, decimal point and bool names of `std::locale`
 *   (the global one, or the one passed to the `format (loc, ...)` overloads).
 *   Without `L` the output never depends on the C or C++ locale.
 * - `{}` for a floating-point value prints the shortest text that reads back
 *   to the same value, as `std::format` does.
 * - C++20: the format string is checked at compile time against the
 *   argument types (`consteval`); a string known only at run time goes
 *   through `runtime (str)` or `vformat`. Below C++20 the same checks run at
 *   run time. Every failure throws `FormatError`; `try_format` never throws.
 *
 * Ranges / tuples live in `LumexFormatRanges.hpp`, chrono types in
 * `LumexFormatChrono.hpp`; the umbrella `lumex/core/fmt/LumexFormat`
 * includes all three.
 *
 * Width and precision are measured in estimated display columns of extended
 * grapheme clusters, as `std::format` (C++23) defines it; the `?` (debug)
 * presentation escapes control characters and invalid code units, not the
 * full Unicode "printable" set.
 */
#ifndef LUMEX_CORE_FMT_FORMAT_HPP
#define LUMEX_CORE_FMT_FORMAT_HPP

#include <climits>
#include <clocale>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <iterator>
#include <limits>
#include <locale>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#if __cplusplus >= 201703L
#include <string_view>
#if defined(__has_include)
#if __has_include(<charconv>)
#include <charconv>
#endif
#endif
#endif

#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexExceptionMacros.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
/** @brief `constexpr` where loops are allowed in it (C++14 on). */
#if __cplusplus >= 201402L
#define LUMEX_FORMAT_CONSTEXPR constexpr
#else
#define LUMEX_FORMAT_CONSTEXPR
#endif

/** @brief 1 when the format string is checked at compile time. */
#if __cplusplus >= 202002L && defined(__cpp_consteval)
#define LUMEX_FORMAT_HAS_CONSTEVAL 1
#else
#define LUMEX_FORMAT_HAS_CONSTEVAL 0
#endif

/** @brief 1 when `std::to_chars` handles floating point. */
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
#define LUMEX_FORMAT_HAS_FLOAT_TO_CHARS 1
#else
#define LUMEX_FORMAT_HAS_FLOAT_TO_CHARS 0
#endif

/** @brief 1 when the compiler provides `__int128`. */
#if defined(__SIZEOF_INT128__)
#define LUMEX_FORMAT_HAS_INT128 1
#else
#define LUMEX_FORMAT_HAS_INT128 0
#endif
// NOLINTEND(cppcoreguidelines-macro-usage)

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace fmt
{
/**
 * @class FormatError
 * @brief Thrown for an invalid format string, a format specification that
 * does not fit its argument, or a missing argument.
 * @details `position()` is the offset of the replacement field (or of the
 * stray brace) in the format string when it is known, otherwise
 * `no_position()`.
 */
LUMEX_DEFINE_EXCEPTION_WITH_BODY (
    FormatError, std::runtime_error,
    FormatError (std::string const &message,
                 std::size_t position) : std::runtime_error (message),
    _position (position) {}

    // Value of position () when the offset is unknown.
    static std::size_t no_position ()
        LUMEX_NOEXCEPT { return static_cast<std::size_t> (-1); }

    // Offset of the failing field in the format string.
    std::size_t position () const LUMEX_NOEXCEPT { return _position; }

    private : std::size_t _position
    = static_cast<std::size_t> (-1);)

template <typename Char> class BasicFormatContext;
template <typename Char> class BasicFormatParseContext;

/**
 * @class Formatter
 * @brief Customization point: specialize `Formatter<T, Char>` with
 * `Char const *parse (BasicFormatParseContext<Char> &)` and
 * `BasicAppender<Char> format (T const &, BasicFormatContext<Char> &) const`.
 * @details The primary template is disabled (deleted constructor), so
 * `std::is_default_constructible<Formatter<T, Char>>` tells whether `T` is
 * formattable. Built-in types have specializations a user formatter can
 * inherit from to reuse the standard specification.
 */
template <typename T, typename Char = char, typename Enable = void>
class Formatter
{
public:
  Formatter () = delete;
  Formatter (Formatter const &) = delete;
  Formatter &operator= (Formatter const &) = delete;
};

namespace Detail
{
// ----------------------------------------------------------------------
// String reference and buffers
// ----------------------------------------------------------------------

template <typename Char>
LUMEX_FORMAT_CONSTEXPR std::size_t
c_string_length (Char const *text) LUMEX_NOEXCEPT
{
  std::size_t length = 0;
  if (text != nullptr)
    while (text[length] != Char ())
      ++length;
  return length;
}

/**
 * @class BasicStringRef
 * @brief Non-owning `(pointer, size)` view used for format strings; converts
 * from C strings, `std::basic_string` and `std::basic_string_view`.
 */
template <typename Char> class BasicStringRef
{
public:
  LUMEX_CONSTEXPR_CTOR
  BasicStringRef () LUMEX_NOEXCEPT : _data (nullptr), _size (0) {}

  LUMEX_CONSTEXPR_CTOR
  BasicStringRef (Char const *data, std::size_t size) LUMEX_NOEXCEPT
      : _data (data),
        _size (size)
  {
  }

  LUMEX_FORMAT_CONSTEXPR
  BasicStringRef (Char const *text) LUMEX_NOEXCEPT
      : _data (text),
        _size (c_string_length (text))
  {
  }

  template <typename Traits, typename Allocator>
  BasicStringRef (std::basic_string<Char, Traits, Allocator> const &text)
      LUMEX_NOEXCEPT : _data (text.data ()),
                       _size (text.size ())
  {
  }

#if __cplusplus >= 201703L
  LUMEX_CONSTEXPR_CTOR
  BasicStringRef (std::basic_string_view<Char> text) LUMEX_NOEXCEPT
      : _data (text.data ()),
        _size (text.size ())
  {
  }
#endif

  LUMEX_CONSTEXPR Char const *
  data () const LUMEX_NOEXCEPT
  {
    return _data;
  }

  LUMEX_CONSTEXPR std::size_t
  size () const LUMEX_NOEXCEPT
  {
    return _size;
  }

  LUMEX_CONSTEXPR Char const *
  begin () const LUMEX_NOEXCEPT
  {
    return _data;
  }

  LUMEX_CONSTEXPR Char const *
  end () const LUMEX_NOEXCEPT
  {
    return _data + _size;
  }

private:
  Char const *_data;
  std::size_t _size;
};

/**
 * @class Buffer
 * @brief Output sink the formatting engine writes to; the concrete buffers
 * write into a string, an output iterator, a counter or a bounded iterator.
 */
template <typename Char> class Buffer
{
public:
  virtual ~Buffer () = default;

  virtual void push_back (Char value) = 0;

  virtual void
  append (Char const *first, Char const *stop)
  {
    for (; first != stop; ++first)
      push_back (*first);
  }

  void
  append (std::basic_string<Char> const &text)
  {
    append (text.data (), text.data () + text.size ());
  }
};

template <typename Char> class StringBuffer final : public Buffer<Char>
{
public:
  explicit StringBuffer (std::basic_string<Char> &target) : _target (target) {}

  void
  push_back (Char value) override
  {
    _target.push_back (value);
  }

  void
  append (Char const *first, Char const *stop) override
  {
    _target.append (first, stop);
  }

private:
  std::basic_string<Char> &_target;
};

template <typename Char, typename OutputIt>
class IteratorBuffer final : public Buffer<Char>
{
public:
  explicit IteratorBuffer (OutputIt out) : _out (out) {}

  void
  push_back (Char value) override
  {
    *_out = value;
    ++_out;
  }

  OutputIt
  out () const
  {
    return _out;
  }

private:
  OutputIt _out;
};

template <typename Char> class CountingBuffer final : public Buffer<Char>
{
public:
  void
  push_back (Char) override
  {
    ++_count;
  }

  void
  append (Char const *first, Char const *stop) override
  {
    _count += static_cast<std::size_t> (stop - first);
  }

  std::size_t
  count () const LUMEX_NOEXCEPT
  {
    return _count;
  }

private:
  std::size_t _count = 0;
};

template <typename Char, typename OutputIt>
class TruncatingBuffer final : public Buffer<Char>
{
public:
  TruncatingBuffer (OutputIt out, std::size_t limit)
      : _out (out), _limit (limit)
  {
  }

  void
  push_back (Char value) override
  {
    if (_count < _limit)
      {
        *_out = value;
        ++_out;
      }
    ++_count;
  }

  OutputIt
  out () const
  {
    return _out;
  }

  std::size_t
  count () const LUMEX_NOEXCEPT
  {
    return _count;
  }

private:
  OutputIt _out;
  std::size_t _limit;
  std::size_t _count = 0;
};
} // namespace Detail

/**
 * @class BasicAppender
 * @brief Output iterator of `BasicFormatContext`: every assignment appends
 * one character to the formatting buffer.
 */
template <typename Char> class BasicAppender
{
public:
  using iterator_category = std::output_iterator_tag;
  using value_type = void;
  using difference_type = std::ptrdiff_t;
  using pointer = void;
  using reference = void;

  explicit BasicAppender (Detail::Buffer<Char> &buffer) LUMEX_NOEXCEPT
      : _buffer (&buffer)
  {
  }

  BasicAppender &
  operator= (Char value)
  {
    _buffer->push_back (value);
    return *this;
  }

  BasicAppender &
  operator* () LUMEX_NOEXCEPT
  {
    return *this;
  }

  BasicAppender &
  operator++ () LUMEX_NOEXCEPT
  {
    return *this;
  }

  BasicAppender
  operator++ (int) LUMEX_NOEXCEPT
  {
    return *this;
  }

  /** @brief The buffer this appender writes to. */
  Detail::Buffer<Char> &
  buffer () const LUMEX_NOEXCEPT
  {
    return *_buffer;
  }

private:
  Detail::Buffer<Char> *_buffer;
};

namespace Detail
{
// ----------------------------------------------------------------------
// Argument kinds, specifications
// ----------------------------------------------------------------------

/** @brief How a type-erased argument stores its value. */
enum class ArgKind : unsigned char
{
  none,
  signed_int,
  unsigned_int,
  signed_int128,
  unsigned_int128,
  boolean,
  character,
  float_value,
  double_value,
  long_double_value,
  c_string,
  string,
  pointer,
  custom
};

/** @brief Which presentation types a specification may use. */
enum class SpecKind : unsigned char
{
  integer,
  character,
  boolean,
  floating,
  string,
  pointer,
  enumeration
};

enum class Align : unsigned char
{
  none,
  left,
  right,
  center
};

enum class Sign : unsigned char
{
  none,
  minus,
  plus,
  space
};

enum class ArgRefKind : unsigned char
{
  none,
  index,
  name
};

/** @brief Reference to an argument: by position or by name. */
template <typename Char> struct arg_ref_t
{
  LUMEX_CONSTEXPR_CTOR
  arg_ref_t () LUMEX_NOEXCEPT : kind (ArgRefKind::none),
                                index (0),
                                name (nullptr),
                                name_size (0)
  {
  }

  ArgRefKind kind;
  int index;
  Char const *name;
  std::size_t name_size;
};

/** @brief A parsed standard format specification. */
template <typename Char> struct format_specs_t
{
  LUMEX_FORMAT_CONSTEXPR
  format_specs_t () LUMEX_NOEXCEPT
      : fill{ static_cast<Char> (' '), Char (), Char (), Char () },
        fill_size (1),
        align (Align::none),
        sign (Sign::none),
        alternate (false),
        zero_pad (false),
        localized (false),
        type (Char ()),
        width (0),
        precision (-1),
        width_ref (),
        precision_ref ()
  {
  }

  Char fill[4];
  unsigned char fill_size;
  Align align;
  Sign sign;
  bool alternate;
  bool zero_pad;
  bool localized;
  Char type;
  int width;
  int precision;
  arg_ref_t<Char> width_ref;
  arg_ref_t<Char> precision_ref;
};

LUMEX_CONSTEXPR bool
is_builtin_kind (ArgKind kind) LUMEX_NOEXCEPT
{
  return kind != ArgKind::none && kind != ArgKind::custom;
}

LUMEX_CONSTEXPR bool
is_integral_kind (ArgKind kind) LUMEX_NOEXCEPT
{
  return kind == ArgKind::signed_int || kind == ArgKind::unsigned_int
         || kind == ArgKind::signed_int128 || kind == ArgKind::unsigned_int128;
}

LUMEX_CONSTEXPR SpecKind
spec_kind_of (ArgKind kind) LUMEX_NOEXCEPT
{
  return kind == ArgKind::boolean     ? SpecKind::boolean
         : kind == ArgKind::character ? SpecKind::character
         : (kind == ArgKind::float_value || kind == ArgKind::double_value
            || kind == ArgKind::long_double_value)
             ? SpecKind::floating
         : (kind == ArgKind::c_string || kind == ArgKind::string)
             ? SpecKind::string
         : kind == ArgKind::pointer ? SpecKind::pointer
                                    : SpecKind::integer;
}

// ----------------------------------------------------------------------
// Type classification
// ----------------------------------------------------------------------

/** @brief A named argument, created by `arg ("name", value)`. */
template <typename Char, typename T> struct named_arg_t
{
  Char const *name;
  T const &value;
};

template <typename T> struct is_named_arg : std::false_type
{
};

template <typename Char, typename T>
struct is_named_arg<named_arg_t<Char, T>> : std::true_type
{
};

template <typename T> struct unwrap_named
{
  using type = T;
};

template <typename Char, typename T> struct unwrap_named<named_arg_t<Char, T>>
{
  using type = T;
};

template <typename Char, typename T>
LUMEX_CONSTEXPR bool
is_character_type () LUMEX_NOEXCEPT
{
  return std::is_same<T, Char>::value
         || (std::is_same<Char, wchar_t>::value
             && std::is_same<T, char>::value);
}

template <typename T>
LUMEX_CONSTEXPR bool
is_other_character_type () LUMEX_NOEXCEPT
{
  return std::is_same<T, char>::value || std::is_same<T, wchar_t>::value
         || std::is_same<T, char16_t>::value
         || std::is_same<T, char32_t>::value;
}

/**
 * @brief Storage kind of a built-in type `T` (decayed) for character type
 * `Char`, or `ArgKind::none` when `T` is not built in.
 */
template <typename Char, typename T>
LUMEX_CONSTEXPR ArgKind
builtin_kind () LUMEX_NOEXCEPT
{
  return std::is_same<T, bool>::value    ? ArgKind::boolean
         : is_character_type<Char, T> () ? ArgKind::character
         : is_other_character_type<T> () ? ArgKind::none
#if LUMEX_FORMAT_HAS_INT128
         : std::is_same<T, __int128>::value          ? ArgKind::signed_int128
         : std::is_same<T, unsigned __int128>::value ? ArgKind::unsigned_int128
#endif
         : (std::is_integral<T>::value && std::is_signed<T>::value)
             ? ArgKind::signed_int
         : std::is_integral<T>::value          ? ArgKind::unsigned_int
         : std::is_same<T, float>::value       ? ArgKind::float_value
         : std::is_same<T, double>::value      ? ArgKind::double_value
         : std::is_same<T, long double>::value ? ArgKind::long_double_value
         : (std::is_same<T, Char const *>::value
            || std::is_same<T, Char *>::value)
             ? ArgKind::c_string
         : lumex::core::utility::traits::string::is_string_like<T, Char>::value
             ? ArgKind::string
         : (std::is_same<T, void const *>::value
            || std::is_same<T, void *>::value
            || std::is_same<T, std::nullptr_t>::value)
             ? ArgKind::pointer
             : ArgKind::none;
}

/** @brief `Formatter<T, Char>` is enabled. */
template <typename Char, typename T>
LUMEX_CONSTEXPR bool
has_formatter () LUMEX_NOEXCEPT
{
  return std::is_default_constructible<Formatter<T, Char>>::value;
}

/**
 * @brief Storage kind of an argument of type `T` (after decay and after
 * unwrapping a named argument): built in, custom (has a `Formatter`) or none.
 */
template <typename Char, typename T>
LUMEX_CONSTEXPR ArgKind
arg_kind_of () LUMEX_NOEXCEPT
{
  return builtin_kind<Char, typename unwrap_named<T>::type> () != ArgKind::none
             ? builtin_kind<Char, typename unwrap_named<T>::type> ()
         : has_formatter<Char, typename unwrap_named<T>::type> ()
             ? ArgKind::custom
             : ArgKind::none;
}

template <typename... Args> struct count_named;

template <> struct count_named<> : std::integral_constant<std::size_t, 0>
{
};

template <typename First, typename... Rest>
struct count_named<First, Rest...>
    : std::integral_constant<
          std::size_t,
          (is_named_arg<typename std::decay<First>::type>::value ? 1 : 0)
              + count_named<Rest...>::value>
{
};

// ----------------------------------------------------------------------
// Parsing helpers (constexpr from C++14 on, for the compile-time check)
// ----------------------------------------------------------------------

template <typename Char>
LUMEX_CONSTEXPR bool
is_digit (Char value) LUMEX_NOEXCEPT
{
  return value >= static_cast<Char> ('0') && value <= static_cast<Char> ('9');
}

template <typename Char>
LUMEX_CONSTEXPR bool
is_name_start (Char value) LUMEX_NOEXCEPT
{
  return (value >= static_cast<Char> ('a') && value <= static_cast<Char> ('z'))
         || (value >= static_cast<Char> ('A')
             && value <= static_cast<Char> ('Z'))
         || value == static_cast<Char> ('_');
}

template <typename Char>
LUMEX_CONSTEXPR bool
is_align (Char value) LUMEX_NOEXCEPT
{
  return value == static_cast<Char> ('<') || value == static_cast<Char> ('>')
         || value == static_cast<Char> ('^');
}

/** @brief Number of code units of the code point starting at `it`. */
template <typename Char>
LUMEX_FORMAT_CONSTEXPR std::size_t
code_point_length (Char const *it, Char const *end) LUMEX_NOEXCEPT
{
  std::size_t length = 1;
  if (sizeof (Char) == 1)
    {
      unsigned char const lead = static_cast<unsigned char> (*it);
      if (lead >= 0xF0 && lead < 0xF8)
        length = 4;
      else if (lead >= 0xE0)
        length = lead < 0xF0 ? 3 : 1;
      else if (lead >= 0xC0)
        length = 2;
    }
  else if (sizeof (Char) == 2)
    {
      unsigned const unit = static_cast<unsigned> (*it) & 0xFFFFu;
      if (unit >= 0xD800u && unit < 0xDC00u)
        length = 2;
    }
  if (static_cast<std::size_t> (end - it) < length)
    length = 1;
  return length;
}

/** @brief Parses a non-negative decimal number; throws when above INT_MAX. */
template <typename Char>
LUMEX_FORMAT_CONSTEXPR int
parse_nonnegative_int (Char const *&it, Char const *end)
{
  unsigned long long value = 0;
  while (it != end && is_digit (*it))
    {
      value
          = value * 10 + static_cast<unsigned> (*it - static_cast<Char> ('0'));
      if (value > static_cast<unsigned long long> (INT_MAX))
        throw FormatError ("number is too big");
      ++it;
    }
  return static_cast<int> (value);
}

/**
 * @brief Parses an arg-id (`0`, `12`, `name`) at `it`; leaves `it` on the
 * character after it.
 */
template <typename Char>
LUMEX_FORMAT_CONSTEXPR arg_ref_t<Char>
parse_arg_id (Char const *&it, Char const *end)
{
  arg_ref_t<Char> ref;
  if (it != end && is_digit (*it))
    {
      if (*it == static_cast<Char> ('0') && it + 1 != end && is_digit (it[1]))
        throw FormatError ("invalid format string");
      // An index above INT_MAX cannot name an argument: saturate, so the
      // lookup reports "argument not found" rather than a number error.
      unsigned long long value = 0;
      while (it != end && is_digit (*it))
        {
          if (value <= static_cast<unsigned long long> (INT_MAX))
            value = value * 10
                    + static_cast<unsigned> (*it - static_cast<Char> ('0'));
          ++it;
        }
      ref.kind = ArgRefKind::index;
      ref.index = value > static_cast<unsigned long long> (INT_MAX)
                      ? INT_MAX
                      : static_cast<int> (value);
      return ref;
    }
  if (it != end && is_name_start (*it))
    {
      Char const *const start = it;
      while (it != end && (is_name_start (*it) || is_digit (*it)))
        ++it;
      ref.kind = ArgRefKind::name;
      ref.name = start;
      ref.name_size = static_cast<std::size_t> (it - start);
      return ref;
    }
  throw FormatError ("invalid format string");
}
} // namespace Detail

/**
 * @class BasicFormatParseContext
 * @brief The format specification being parsed and the automatic /
 * manual argument numbering state (as `std::basic_format_parse_context`).
 */
template <typename Char> class BasicFormatParseContext
{
public:
  using char_type = Char;
  using iterator = Char const *;
  using const_iterator = Char const *;

  LUMEX_FORMAT_CONSTEXPR explicit BasicFormatParseContext (
      Detail::BasicStringRef<Char> text, int arg_count = 0,
      Detail::ArgKind const *kinds = nullptr,
      bool has_named = false) LUMEX_NOEXCEPT : _begin (text.begin ()),
                                               _end (text.end ()),
                                               _next_arg_id (0),
                                               _arg_count (arg_count),
                                               _kinds (kinds),
                                               _has_named (has_named)
  {
  }

  LUMEX_CONSTEXPR iterator
  begin () const LUMEX_NOEXCEPT
  {
    return _begin;
  }

  LUMEX_CONSTEXPR iterator
  end () const LUMEX_NOEXCEPT
  {
    return _end;
  }

  LUMEX_FORMAT_CONSTEXPR void
  advance_to (iterator it) LUMEX_NOEXCEPT
  {
    _begin = it;
  }

  /** @brief Next automatic argument index; throws after manual indexing. */
  LUMEX_FORMAT_CONSTEXPR int
  next_arg_id ()
  {
    if (_next_arg_id < 0)
      throw FormatError (
          "cannot switch from manual to automatic argument indexing");
    int const id = _next_arg_id++;
    if (_kinds != nullptr && id >= _arg_count)
      throw FormatError ("argument not found");
    return id;
  }

  /** @brief Records manual indexing; throws after automatic indexing. */
  LUMEX_FORMAT_CONSTEXPR void
  check_arg_id (int id)
  {
    if (_next_arg_id > 0)
      throw FormatError (
          "cannot switch from automatic to manual argument indexing");
    _next_arg_id = -1;
    if (_kinds != nullptr && id >= _arg_count)
      throw FormatError ("argument not found");
  }

  /**
   * @brief Records a reference by name: it counts as manual indexing when
   * no automatic index was used yet (so `{a} {}` is an error, `{} {a}` is
   * not). At compile time, fails when no argument is named.
   */
  LUMEX_FORMAT_CONSTEXPR void
  check_named_arg ()
  {
    if (_next_arg_id == 0)
      _next_arg_id = -1;
    if (_kinds != nullptr && !_has_named)
      throw FormatError ("argument not found");
  }

  /** @brief Compile-time check only: argument `id` is an integer. */
  LUMEX_FORMAT_CONSTEXPR void
  check_dynamic_spec (int id) const
  {
    if (_kinds != nullptr && id < _arg_count
        && !Detail::is_integral_kind (_kinds[id]))
      throw FormatError ("width/precision is not integer");
  }

private:
  iterator _begin;
  iterator _end;
  int _next_arg_id;
  int _arg_count;
  Detail::ArgKind const *_kinds;
  bool _has_named;
};

namespace Detail
{
/** @brief Parses `{}` / `{3}` / `{name}` used as dynamic width / precision. */
template <typename Char>
LUMEX_FORMAT_CONSTEXPR arg_ref_t<Char>
parse_dynamic_ref (Char const *&it, Char const *end,
                   BasicFormatParseContext<Char> &ctx)
{
  ++it; // '{'
  arg_ref_t<Char> ref;
  if (it != end && *it == static_cast<Char> ('}'))
    {
      ref.kind = ArgRefKind::index;
      ref.index = ctx.next_arg_id ();
      ctx.check_dynamic_spec (ref.index);
    }
  else
    {
      ref = parse_arg_id (it, end);
      if (ref.kind == ArgRefKind::index)
        {
          ctx.check_arg_id (ref.index);
          ctx.check_dynamic_spec (ref.index);
        }
      else
        ctx.check_named_arg ();
    }
  if (it == end || *it != static_cast<Char> ('}'))
    throw FormatError ("invalid format string");
  ++it;
  return ref;
}

template <typename Char>
LUMEX_FORMAT_CONSTEXPR bool
is_integer_presentation (Char type) LUMEX_NOEXCEPT
{
  return type == static_cast<Char> ('d') || type == static_cast<Char> ('b')
         || type == static_cast<Char> ('B') || type == static_cast<Char> ('o')
         || type == static_cast<Char> ('x') || type == static_cast<Char> ('X');
}

template <typename Char>
LUMEX_FORMAT_CONSTEXPR bool
is_float_presentation (Char type) LUMEX_NOEXCEPT
{
  return type == static_cast<Char> ('a') || type == static_cast<Char> ('A')
         || type == static_cast<Char> ('e') || type == static_cast<Char> ('E')
         || type == static_cast<Char> ('f') || type == static_cast<Char> ('F')
         || type == static_cast<Char> ('g') || type == static_cast<Char> ('G');
}

/**
 * @brief Checks the parsed specification against what `kind` accepts.
 * @details Sign, `#` and `0` need a numeric presentation; precision needs a
 * floating-point or string presentation; `L` needs a numeric or bool one.
 */
template <typename Char>
LUMEX_FORMAT_CONSTEXPR void
validate_specs (format_specs_t<Char> const &specs, SpecKind kind)
{
  Char const type = specs.type;
  bool numeric = false;
  bool allows_precision = false;
  bool allows_localized = false;
  bool allows_zero = false;
  char const *const invalid = kind == SpecKind::character
                                  ? "invalid format specifier for char"
                                  : "invalid format specifier";
  switch (kind)
    {
    case SpecKind::integer:
      if (type != Char () && type != static_cast<Char> ('c')
          && !is_integer_presentation (type))
        throw FormatError ("invalid format specifier");
      numeric = type != static_cast<Char> ('c');
      allows_localized = numeric;
      allows_zero = numeric;
      break;
    case SpecKind::character:
      if (type != Char () && type != static_cast<Char> ('c')
          && type != static_cast<Char> ('?')
          && !is_integer_presentation (type))
        throw FormatError ("invalid format specifier");
      numeric = is_integer_presentation (type);
      allows_localized = numeric;
      allows_zero = numeric;
      break;
    case SpecKind::boolean:
      if (type != Char () && type != static_cast<Char> ('s')
          && !is_integer_presentation (type))
        throw FormatError ("invalid format specifier");
      numeric = is_integer_presentation (type);
      allows_localized = true;
      allows_zero = numeric;
      break;
    case SpecKind::floating:
      if (type != Char () && !is_float_presentation (type))
        throw FormatError ("invalid format specifier");
      numeric = true;
      allows_precision = true;
      allows_localized = true;
      allows_zero = true;
      break;
    case SpecKind::string:
      if (type != Char () && type != static_cast<Char> ('s')
          && type != static_cast<Char> ('?'))
        throw FormatError ("invalid format specifier");
      allows_precision = true;
      break;
    case SpecKind::pointer:
      if (type != Char () && type != static_cast<Char> ('p')
          && type != static_cast<Char> ('P'))
        throw FormatError ("invalid format specifier");
      // P2510 (a defect report against C++20): zeros pad after "0x".
      allows_zero = true;
      break;
    case SpecKind::enumeration:
      if (type != Char () && type != static_cast<Char> ('s')
          && !is_integer_presentation (type))
        throw FormatError ("invalid format specifier");
      numeric = is_integer_presentation (type);
      allows_precision = !numeric;
      allows_localized = numeric;
      allows_zero = numeric;
      break;
    }
  if ((specs.sign != Sign::none || specs.alternate) && !numeric)
    throw FormatError (invalid);
  if (specs.zero_pad && !allows_zero)
    throw FormatError (kind == SpecKind::character
                           ? invalid
                           : "format specifier requires numeric argument");
  if ((specs.precision >= 0 || specs.precision_ref.kind != ArgRefKind::none)
      && !allows_precision)
    throw FormatError ("invalid format specifier");
  if (specs.localized && !allows_localized)
    throw FormatError ("invalid format specifier");
}

/**
 * @brief Parses `[[fill]align][sign][#][0][width][.precision][L][type]`
 * starting at `ctx.begin()`; returns the position of the closing `}` (or
 * the end).
 */
template <typename Char>
LUMEX_FORMAT_CONSTEXPR Char const *
parse_std_specs (BasicFormatParseContext<Char> &ctx,
                 format_specs_t<Char> &specs, SpecKind kind)
{
  Char const *it = ctx.begin ();
  Char const *const end = ctx.end ();
  if (it == end || *it == static_cast<Char> ('}'))
    {
      validate_specs (specs, kind);
      return it;
    }

  // [[fill]align]
  std::size_t const fill_length = code_point_length (it, end);
  if (static_cast<std::size_t> (end - it) > fill_length
      && is_align (it[fill_length]))
    {
      if (*it == static_cast<Char> ('{'))
        throw FormatError ("invalid fill character '{'");
      if (*it == static_cast<Char> ('}'))
        throw FormatError ("invalid fill character '}'");
      for (std::size_t i = 0; i < fill_length; ++i)
        specs.fill[i] = it[i];
      specs.fill_size = static_cast<unsigned char> (fill_length);
      it += fill_length;
    }
  if (it != end && is_align (*it))
    {
      specs.align = *it == static_cast<Char> ('<')   ? Align::left
                    : *it == static_cast<Char> ('>') ? Align::right
                                                     : Align::center;
      ++it;
    }

  // [sign]
  if (it != end
      && (*it == static_cast<Char> ('+') || *it == static_cast<Char> ('-')
          || *it == static_cast<Char> (' ')))
    {
      specs.sign = *it == static_cast<Char> ('+')   ? Sign::plus
                   : *it == static_cast<Char> ('-') ? Sign::minus
                                                    : Sign::space;
      ++it;
    }

  // [#]
  if (it != end && *it == static_cast<Char> ('#'))
    {
      specs.alternate = true;
      ++it;
    }

  // [0]
  if (it != end && *it == static_cast<Char> ('0'))
    {
      specs.zero_pad = true;
      ++it;
      // The width that may follow starts with a nonzero digit (`{:00}`
      // is an error, as in std::format).
      if (it != end && *it == static_cast<Char> ('0'))
        throw FormatError ("invalid format specifier");
    }

  // [width]
  if (it != end && is_digit (*it))
    specs.width = parse_nonnegative_int (it, end);
  else if (it != end && *it == static_cast<Char> ('{'))
    specs.width_ref = parse_dynamic_ref (it, end, ctx);

  // [.precision]
  if (it != end && *it == static_cast<Char> ('.'))
    {
      ++it;
      if (it != end && is_digit (*it))
        specs.precision = parse_nonnegative_int (it, end);
      else if (it != end && *it == static_cast<Char> ('{'))
        specs.precision_ref = parse_dynamic_ref (it, end, ctx);
      else if (it == end)
        throw FormatError ("invalid precision");
      else
        throw FormatError ("invalid format string");
    }

  // [L]
  if (it != end && *it == static_cast<Char> ('L'))
    {
      specs.localized = true;
      ++it;
    }

  // [type]
  if (it != end && *it != static_cast<Char> ('}'))
    {
      specs.type = *it;
      ++it;
    }

  if (it != end && *it != static_cast<Char> ('}'))
    throw FormatError ("invalid format specifier");
  validate_specs (specs, kind);
  return it;
}

/** @brief Skips a specification this engine cannot check at compile time. */
template <typename Char>
LUMEX_FORMAT_CONSTEXPR Char const *
skip_specs (Char const *it, Char const *end)
{
  int depth = 0;
  for (; it != end; ++it)
    {
      if (*it == static_cast<Char> ('{'))
        ++depth;
      else if (*it == static_cast<Char> ('}'))
        {
          if (depth == 0)
            return it;
          --depth;
        }
    }
  return it;
}

/**
 * @brief Walks a format string and reports literal text, replacement fields
 * and their specifications to `handler`. Shared by the compile-time checker
 * and the run-time formatter so both follow the same grammar.
 */
template <typename Char, typename Handler>
LUMEX_FORMAT_CONSTEXPR void
parse_format_string (Char const *begin, Char const *end, Handler &handler)
{
  Char const *text = begin;
  Char const *it = begin;
  while (it != end)
    {
      Char const value = *it;
      if (value == static_cast<Char> ('{'))
        {
          handler.on_text (text, it);
          handler.on_field_start (static_cast<std::size_t> (it - begin));
          ++it;
          if (it == end)
            throw FormatError ("invalid format string");
          if (*it == static_cast<Char> ('{'))
            {
              handler.on_text (it, it + 1);
              ++it;
              text = it;
              continue;
            }
          if (*it == static_cast<Char> ('}') || *it == static_cast<Char> (':'))
            handler.on_auto_arg ();
          else
            {
              arg_ref_t<Char> const ref = parse_arg_id (it, end);
              if (ref.kind == ArgRefKind::index)
                handler.on_index_arg (ref.index);
              else
                handler.on_name_arg (ref.name, ref.name_size);
            }
          if (it == end)
            throw FormatError ("invalid format string");
          if (*it == static_cast<Char> (':'))
            ++it;
          else if (*it != static_cast<Char> ('}'))
            throw FormatError ("invalid format string");
          it = handler.on_format_specs (it, end);
          if (it == end)
            throw FormatError ("missing '}' in format string");
          if (*it != static_cast<Char> ('}'))
            throw FormatError ("unknown format specifier");
          ++it;
          text = it;
        }
      else if (value == static_cast<Char> ('}'))
        {
          handler.on_text (text, it);
          handler.on_field_start (static_cast<std::size_t> (it - begin));
          ++it;
          if (it == end || *it != static_cast<Char> ('}'))
            throw FormatError ("unmatched '}' in format string");
          handler.on_text (it, it + 1);
          ++it;
          text = it;
        }
      else
        ++it;
    }
  handler.on_text (text, end);
}

// ----------------------------------------------------------------------
// Type-erased arguments
// ----------------------------------------------------------------------

template <typename Char> struct string_arg_t
{
  Char const *data;
  std::size_t size;
};

template <typename Char> struct custom_arg_t
{
  void const *value;
  void (*format) (void const *, BasicFormatParseContext<Char> &,
                  BasicFormatContext<Char> &);
};

/** @brief One argument: a kind tag plus the value (or a pointer to it). */
template <typename Char> struct format_arg_t
{
  format_arg_t () LUMEX_NOEXCEPT : kind (ArgKind::none), unsigned_value (0) {}

  ArgKind kind;
  union
  {
    long long signed_value;
    unsigned long long unsigned_value;
#if LUMEX_FORMAT_HAS_INT128
    __int128 int128_value;
    unsigned __int128 uint128_value;
#endif
    bool bool_value;
    Char char_value;
    float float_value;
    double double_value;
    long double long_double_value;
    Char const *c_string;
    string_arg_t<Char> string;
    void const *pointer;
    custom_arg_t<Char> custom;
  };
};

template <typename Char> struct named_arg_entry_t
{
  Char const *name;
  int index;
};

/** @brief Storage for `N` arguments, `M` of them named. */
template <typename Char, std::size_t N, std::size_t M>
struct format_arg_store_t
{
  format_arg_t<Char> args[N == 0 ? 1 : N];
  named_arg_entry_t<Char> named[M == 0 ? 1 : M];
  std::size_t named_count = 0;
};
} // namespace Detail

/**
 * @class BasicFormatArgs
 * @brief Non-owning view of the arguments of one formatting call (as
 * `std::basic_format_args`); built from `make_format_args`.
 */
template <typename Char> class BasicFormatArgs
{
public:
  BasicFormatArgs () LUMEX_NOEXCEPT : _args (nullptr),
                                      _count (0),
                                      _named (nullptr),
                                      _named_count (0)
  {
  }

  template <std::size_t N, std::size_t M>
  BasicFormatArgs (Detail::format_arg_store_t<Char, N, M> const &store)
      LUMEX_NOEXCEPT : _args (store.args),
                       _count (static_cast<int> (N)),
                       _named (store.named),
                       _named_count (static_cast<int> (store.named_count))
  {
  }

  /** @brief Argument `id`, or an argument of kind `none` when absent. */
  Detail::format_arg_t<Char>
  get (int id) const LUMEX_NOEXCEPT
  {
    if (id < 0 || id >= _count)
      return Detail::format_arg_t<Char> ();
    return _args[id];
  }

  /** @brief Index of the named argument `name`, or -1. */
  int
  find (Char const *name, std::size_t size) const LUMEX_NOEXCEPT
  {
    for (int i = 0; i < _named_count; ++i)
      {
        Char const *const candidate = _named[i].name;
        std::size_t const length = Detail::c_string_length (candidate);
        if (length == size
            && std::char_traits<Char>::compare (candidate, name, size) == 0)
          return _named[i].index;
      }
    return -1;
  }

  int
  size () const LUMEX_NOEXCEPT
  {
    return _count;
  }

private:
  Detail::format_arg_t<Char> const *_args;
  int _count;
  Detail::named_arg_entry_t<Char> const *_named;
  int _named_count;
};

using FormatArgs = BasicFormatArgs<char>;
using WFormatArgs = BasicFormatArgs<wchar_t>;

/**
 * @class BasicFormatContext
 * @brief The output and the arguments of one formatting call (as
 * `std::basic_format_context`).
 */
template <typename Char> class BasicFormatContext
{
public:
  using char_type = Char;
  using iterator = BasicAppender<Char>;

  BasicFormatContext (Detail::Buffer<Char> &buffer,
                      BasicFormatArgs<Char> const &args,
                      std::locale const *locale) LUMEX_NOEXCEPT
      : _buffer (&buffer),
        _args (args),
        _locale (locale)
  {
  }

  iterator
  out () const LUMEX_NOEXCEPT
  {
    return iterator (*_buffer);
  }

  void
  advance_to (iterator) LUMEX_NOEXCEPT
  {
  }

  Detail::format_arg_t<Char>
  arg (int id) const LUMEX_NOEXCEPT
  {
    return _args.get (id);
  }

  int
  arg_id (Char const *name, std::size_t size) const LUMEX_NOEXCEPT
  {
    return _args.find (name, size);
  }

  BasicFormatArgs<Char> const &
  args () const LUMEX_NOEXCEPT
  {
    return _args;
  }

  /** @brief The locale passed to `format (loc, ...)`, else the global one. */
  std::locale
  locale () const
  {
    return _locale != nullptr ? *_locale : std::locale ();
  }

private:
  Detail::Buffer<Char> *_buffer;
  BasicFormatArgs<Char> _args;
  std::locale const *_locale;
};

using FormatContext = BasicFormatContext<char>;
using WFormatContext = BasicFormatContext<wchar_t>;
using FormatParseContext = BasicFormatParseContext<char>;
using WFormatParseContext = BasicFormatParseContext<wchar_t>;

namespace Detail
{
// ----------------------------------------------------------------------
// Writers
// ----------------------------------------------------------------------

template <typename Char>
std::basic_string<Char>
widen (std::string const &text)
{
  std::basic_string<Char> result;
  result.reserve (text.size ());
  for (std::size_t i = 0; i < text.size (); ++i)
    result.push_back (
        static_cast<Char> (static_cast<unsigned char> (text[i])));
  return result;
}

inline bool
is_wide_code_point (std::uint32_t cp) LUMEX_NOEXCEPT
{
  return (cp >= 0x1100 && cp <= 0x115F) || (cp >= 0x2329 && cp <= 0x232A)
         || (cp >= 0x2E80 && cp <= 0x303E) || (cp >= 0x3040 && cp <= 0xA4CF)
         || (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0xF900 && cp <= 0xFAFF)
         || (cp >= 0xFE10 && cp <= 0xFE19) || (cp >= 0xFE30 && cp <= 0xFE6F)
         || (cp >= 0xFF00 && cp <= 0xFF60) || (cp >= 0xFFE0 && cp <= 0xFFE6)
         || (cp >= 0x1F300 && cp <= 0x1F64F)
         || (cp >= 0x1F900 && cp <= 0x1F9FF)
         || (cp >= 0x20000 && cp <= 0x2FFFD)
         || (cp >= 0x30000 && cp <= 0x3FFFD);
}

/**
 * @brief Decodes one code point at `it` (UTF-8 for `char`, UTF-16 or UTF-32
 * for `wchar_t`); an invalid sequence yields its first unit and
 * `valid == false`.
 */
template <typename Char>
std::uint32_t
decode (Char const *&it, Char const *end, bool &valid) LUMEX_NOEXCEPT
{
  valid = true;
  if (sizeof (Char) == 1)
    {
      unsigned char const lead = static_cast<unsigned char> (*it);
      std::size_t length = 0;
      std::uint32_t cp = 0;
      if (lead < 0x80)
        {
          ++it;
          return lead;
        }
      if (lead >= 0xC2 && lead < 0xE0)
        {
          length = 2;
          cp = lead & 0x1Fu;
        }
      else if (lead >= 0xE0 && lead < 0xF0)
        {
          length = 3;
          cp = lead & 0x0Fu;
        }
      else if (lead >= 0xF0 && lead < 0xF5)
        {
          length = 4;
          cp = lead & 0x07u;
        }
      if (length == 0 || static_cast<std::size_t> (end - it) < length)
        {
          valid = false;
          ++it;
          return lead;
        }
      for (std::size_t i = 1; i < length; ++i)
        {
          unsigned char const unit = static_cast<unsigned char> (it[i]);
          if ((unit & 0xC0u) != 0x80u)
            {
              valid = false;
              ++it;
              return lead;
            }
          cp = (cp << 6) | (unit & 0x3Fu);
        }
      it += length;
      return cp;
    }
  if (sizeof (Char) == 2)
    {
      std::uint32_t const unit = static_cast<std::uint32_t> (*it) & 0xFFFFu;
      if (unit >= 0xD800u && unit < 0xDC00u && end - it >= 2)
        {
          std::uint32_t const low
              = static_cast<std::uint32_t> (it[1]) & 0xFFFFu;
          if (low >= 0xDC00u && low < 0xE000u)
            {
              it += 2;
              return 0x10000u + ((unit - 0xD800u) << 10) + (low - 0xDC00u);
            }
        }
      if (unit >= 0xD800u && unit < 0xE000u)
        valid = false;
      ++it;
      return unit;
    }
  std::uint32_t const unit = static_cast<std::uint32_t> (*it);
  ++it;
  return unit;
}

/**
 * @brief The code point joins the preceding one in the same extended
 * grapheme cluster: Grapheme_Cluster_Break Extend or SpacingMark (combining
 * marks, variation selectors, emoji modifiers, tags). Ranges generated from
 * the Unicode 15.0 character database (categories Mn, Me, Mc plus the
 * Other_Grapheme_Extend additions).
 */
inline bool
is_grapheme_extend (std::uint32_t cp) LUMEX_NOEXCEPT
{
  static std::uint32_t const ranges[][2]
      = { { 0x300, 0x36F },     { 0x483, 0x489 },     { 0x591, 0x5BD },
          { 0x5BF, 0x5BF },     { 0x5C1, 0x5C2 },     { 0x5C4, 0x5C5 },
          { 0x5C7, 0x5C7 },     { 0x610, 0x61A },     { 0x64B, 0x65F },
          { 0x670, 0x670 },     { 0x6D6, 0x6DC },     { 0x6DF, 0x6E4 },
          { 0x6E7, 0x6E8 },     { 0x6EA, 0x6ED },     { 0x711, 0x711 },
          { 0x730, 0x74A },     { 0x7A6, 0x7B0 },     { 0x7EB, 0x7F3 },
          { 0x7FD, 0x7FD },     { 0x816, 0x819 },     { 0x81B, 0x823 },
          { 0x825, 0x827 },     { 0x829, 0x82D },     { 0x859, 0x85B },
          { 0x898, 0x89F },     { 0x8CA, 0x8E1 },     { 0x8E3, 0x903 },
          { 0x93A, 0x93C },     { 0x93E, 0x94F },     { 0x951, 0x957 },
          { 0x962, 0x963 },     { 0x981, 0x983 },     { 0x9BC, 0x9BC },
          { 0x9BE, 0x9C4 },     { 0x9C7, 0x9C8 },     { 0x9CB, 0x9CD },
          { 0x9D7, 0x9D7 },     { 0x9E2, 0x9E3 },     { 0x9FE, 0x9FE },
          { 0xA01, 0xA03 },     { 0xA3C, 0xA3C },     { 0xA3E, 0xA42 },
          { 0xA47, 0xA48 },     { 0xA4B, 0xA4D },     { 0xA51, 0xA51 },
          { 0xA70, 0xA71 },     { 0xA75, 0xA75 },     { 0xA81, 0xA83 },
          { 0xABC, 0xABC },     { 0xABE, 0xAC5 },     { 0xAC7, 0xAC9 },
          { 0xACB, 0xACD },     { 0xAE2, 0xAE3 },     { 0xAFA, 0xAFF },
          { 0xB01, 0xB03 },     { 0xB3C, 0xB3C },     { 0xB3E, 0xB44 },
          { 0xB47, 0xB48 },     { 0xB4B, 0xB4D },     { 0xB55, 0xB57 },
          { 0xB62, 0xB63 },     { 0xB82, 0xB82 },     { 0xBBE, 0xBC2 },
          { 0xBC6, 0xBC8 },     { 0xBCA, 0xBCD },     { 0xBD7, 0xBD7 },
          { 0xC00, 0xC04 },     { 0xC3C, 0xC3C },     { 0xC3E, 0xC44 },
          { 0xC46, 0xC48 },     { 0xC4A, 0xC4D },     { 0xC55, 0xC56 },
          { 0xC62, 0xC63 },     { 0xC81, 0xC83 },     { 0xCBC, 0xCBC },
          { 0xCBE, 0xCC4 },     { 0xCC6, 0xCC8 },     { 0xCCA, 0xCCD },
          { 0xCD5, 0xCD6 },     { 0xCE2, 0xCE3 },     { 0xCF3, 0xCF3 },
          { 0xD00, 0xD03 },     { 0xD3B, 0xD3C },     { 0xD3E, 0xD44 },
          { 0xD46, 0xD48 },     { 0xD4A, 0xD4D },     { 0xD57, 0xD57 },
          { 0xD62, 0xD63 },     { 0xD81, 0xD83 },     { 0xDCA, 0xDCA },
          { 0xDCF, 0xDD4 },     { 0xDD6, 0xDD6 },     { 0xDD8, 0xDDF },
          { 0xDF2, 0xDF3 },     { 0xE31, 0xE31 },     { 0xE34, 0xE3A },
          { 0xE47, 0xE4E },     { 0xEB1, 0xEB1 },     { 0xEB4, 0xEBC },
          { 0xEC8, 0xECE },     { 0xF18, 0xF19 },     { 0xF35, 0xF35 },
          { 0xF37, 0xF37 },     { 0xF39, 0xF39 },     { 0xF3E, 0xF3F },
          { 0xF71, 0xF84 },     { 0xF86, 0xF87 },     { 0xF8D, 0xF97 },
          { 0xF99, 0xFBC },     { 0xFC6, 0xFC6 },     { 0x102B, 0x103E },
          { 0x1056, 0x1059 },   { 0x105E, 0x1060 },   { 0x1062, 0x1064 },
          { 0x1067, 0x106D },   { 0x1071, 0x1074 },   { 0x1082, 0x108D },
          { 0x108F, 0x108F },   { 0x109A, 0x109D },   { 0x135D, 0x135F },
          { 0x1712, 0x1715 },   { 0x1732, 0x1734 },   { 0x1752, 0x1753 },
          { 0x1772, 0x1773 },   { 0x17B4, 0x17D3 },   { 0x17DD, 0x17DD },
          { 0x180B, 0x180D },   { 0x180F, 0x180F },   { 0x1885, 0x1886 },
          { 0x18A9, 0x18A9 },   { 0x1920, 0x192B },   { 0x1930, 0x193B },
          { 0x1A17, 0x1A1B },   { 0x1A55, 0x1A5E },   { 0x1A60, 0x1A7C },
          { 0x1A7F, 0x1A7F },   { 0x1AB0, 0x1ACE },   { 0x1B00, 0x1B04 },
          { 0x1B34, 0x1B44 },   { 0x1B6B, 0x1B73 },   { 0x1B80, 0x1B82 },
          { 0x1BA1, 0x1BAD },   { 0x1BE6, 0x1BF3 },   { 0x1C24, 0x1C37 },
          { 0x1CD0, 0x1CD2 },   { 0x1CD4, 0x1CE8 },   { 0x1CED, 0x1CED },
          { 0x1CF4, 0x1CF4 },   { 0x1CF7, 0x1CF9 },   { 0x1DC0, 0x1DFF },
          { 0x200C, 0x200C },   { 0x20D0, 0x20F0 },   { 0x2CEF, 0x2CF1 },
          { 0x2D7F, 0x2D7F },   { 0x2DE0, 0x2DFF },   { 0x302A, 0x302F },
          { 0x3099, 0x309A },   { 0xA66F, 0xA672 },   { 0xA674, 0xA67D },
          { 0xA69E, 0xA69F },   { 0xA6F0, 0xA6F1 },   { 0xA802, 0xA802 },
          { 0xA806, 0xA806 },   { 0xA80B, 0xA80B },   { 0xA823, 0xA827 },
          { 0xA82C, 0xA82C },   { 0xA880, 0xA881 },   { 0xA8B4, 0xA8C5 },
          { 0xA8E0, 0xA8F1 },   { 0xA8FF, 0xA8FF },   { 0xA926, 0xA92D },
          { 0xA947, 0xA953 },   { 0xA980, 0xA983 },   { 0xA9B3, 0xA9C0 },
          { 0xA9E5, 0xA9E5 },   { 0xAA29, 0xAA36 },   { 0xAA43, 0xAA43 },
          { 0xAA4C, 0xAA4D },   { 0xAA7B, 0xAA7D },   { 0xAAB0, 0xAAB0 },
          { 0xAAB2, 0xAAB4 },   { 0xAAB7, 0xAAB8 },   { 0xAABE, 0xAABF },
          { 0xAAC1, 0xAAC1 },   { 0xAAEB, 0xAAEF },   { 0xAAF5, 0xAAF6 },
          { 0xABE3, 0xABEA },   { 0xABEC, 0xABED },   { 0xFB1E, 0xFB1E },
          { 0xFE00, 0xFE0F },   { 0xFE20, 0xFE2F },   { 0xFF9E, 0xFF9F },
          { 0x101FD, 0x101FD }, { 0x102E0, 0x102E0 }, { 0x10376, 0x1037A },
          { 0x10A01, 0x10A03 }, { 0x10A05, 0x10A06 }, { 0x10A0C, 0x10A0F },
          { 0x10A38, 0x10A3A }, { 0x10A3F, 0x10A3F }, { 0x10AE5, 0x10AE6 },
          { 0x10D24, 0x10D27 }, { 0x10EAB, 0x10EAC }, { 0x10EFD, 0x10EFF },
          { 0x10F46, 0x10F50 }, { 0x10F82, 0x10F85 }, { 0x11000, 0x11002 },
          { 0x11038, 0x11046 }, { 0x11070, 0x11070 }, { 0x11073, 0x11074 },
          { 0x1107F, 0x11082 }, { 0x110B0, 0x110BA }, { 0x110C2, 0x110C2 },
          { 0x11100, 0x11102 }, { 0x11127, 0x11134 }, { 0x11145, 0x11146 },
          { 0x11173, 0x11173 }, { 0x11180, 0x11182 }, { 0x111B3, 0x111C0 },
          { 0x111C9, 0x111CC }, { 0x111CE, 0x111CF }, { 0x1122C, 0x11237 },
          { 0x1123E, 0x1123E }, { 0x11241, 0x11241 }, { 0x112DF, 0x112EA },
          { 0x11300, 0x11303 }, { 0x1133B, 0x1133C }, { 0x1133E, 0x11344 },
          { 0x11347, 0x11348 }, { 0x1134B, 0x1134D }, { 0x11357, 0x11357 },
          { 0x11362, 0x11363 }, { 0x11366, 0x1136C }, { 0x11370, 0x11374 },
          { 0x11435, 0x11446 }, { 0x1145E, 0x1145E }, { 0x114B0, 0x114C3 },
          { 0x115AF, 0x115B5 }, { 0x115B8, 0x115C0 }, { 0x115DC, 0x115DD },
          { 0x11630, 0x11640 }, { 0x116AB, 0x116B7 }, { 0x1171D, 0x1172B },
          { 0x1182C, 0x1183A }, { 0x11930, 0x11935 }, { 0x11937, 0x11938 },
          { 0x1193B, 0x1193E }, { 0x11940, 0x11940 }, { 0x11942, 0x11943 },
          { 0x119D1, 0x119D7 }, { 0x119DA, 0x119E0 }, { 0x119E4, 0x119E4 },
          { 0x11A01, 0x11A0A }, { 0x11A33, 0x11A39 }, { 0x11A3B, 0x11A3E },
          { 0x11A47, 0x11A47 }, { 0x11A51, 0x11A5B }, { 0x11A8A, 0x11A99 },
          { 0x11C2F, 0x11C36 }, { 0x11C38, 0x11C3F }, { 0x11C92, 0x11CA7 },
          { 0x11CA9, 0x11CB6 }, { 0x11D31, 0x11D36 }, { 0x11D3A, 0x11D3A },
          { 0x11D3C, 0x11D3D }, { 0x11D3F, 0x11D45 }, { 0x11D47, 0x11D47 },
          { 0x11D8A, 0x11D8E }, { 0x11D90, 0x11D91 }, { 0x11D93, 0x11D97 },
          { 0x11EF3, 0x11EF6 }, { 0x11F00, 0x11F01 }, { 0x11F03, 0x11F03 },
          { 0x11F34, 0x11F3A }, { 0x11F3E, 0x11F42 }, { 0x13440, 0x13440 },
          { 0x13447, 0x13455 }, { 0x16AF0, 0x16AF4 }, { 0x16B30, 0x16B36 },
          { 0x16F4F, 0x16F4F }, { 0x16F51, 0x16F87 }, { 0x16F8F, 0x16F92 },
          { 0x16FE4, 0x16FE4 }, { 0x16FF0, 0x16FF1 }, { 0x1BC9D, 0x1BC9E },
          { 0x1CF00, 0x1CF2D }, { 0x1CF30, 0x1CF46 }, { 0x1D165, 0x1D169 },
          { 0x1D16D, 0x1D172 }, { 0x1D17B, 0x1D182 }, { 0x1D185, 0x1D18B },
          { 0x1D1AA, 0x1D1AD }, { 0x1D242, 0x1D244 }, { 0x1DA00, 0x1DA36 },
          { 0x1DA3B, 0x1DA6C }, { 0x1DA75, 0x1DA75 }, { 0x1DA84, 0x1DA84 },
          { 0x1DA9B, 0x1DA9F }, { 0x1DAA1, 0x1DAAF }, { 0x1E000, 0x1E006 },
          { 0x1E008, 0x1E018 }, { 0x1E01B, 0x1E021 }, { 0x1E023, 0x1E024 },
          { 0x1E026, 0x1E02A }, { 0x1E08F, 0x1E08F }, { 0x1E130, 0x1E136 },
          { 0x1E2AE, 0x1E2AE }, { 0x1E2EC, 0x1E2EF }, { 0x1E4EC, 0x1E4EF },
          { 0x1E8D0, 0x1E8D6 }, { 0x1E944, 0x1E94A }, { 0x1F3FB, 0x1F3FF },
          { 0xE0020, 0xE007F }, { 0xE0100, 0xE01EF } };
  if (cp < 0x300u)
    return false;
  std::size_t low = 0;
  std::size_t high = sizeof (ranges) / sizeof (ranges[0]);
  while (low < high)
    {
      std::size_t const middle = low + (high - low) / 2;
      if (cp < ranges[middle][0])
        high = middle;
      else if (cp > ranges[middle][1])
        low = middle + 1;
      else
        return true;
    }
  return false;
}

/** @brief Regional indicator symbols (two of them form one flag). */
inline bool
is_regional_indicator (std::uint32_t cp) LUMEX_NOEXCEPT
{
  return cp >= 0x1F1E6u && cp <= 0x1F1FFu;
}

/**
 * @brief Extended_Pictographic (approximated by the emoji blocks): a code
 * point that a zero width joiner glues to the previous emoji.
 */
inline bool
is_extended_pictographic (std::uint32_t cp) LUMEX_NOEXCEPT
{
  return cp == 0xA9u || cp == 0xAEu || cp == 0x203Cu || cp == 0x2049u
         || cp == 0x2122u || cp == 0x2139u || (cp >= 0x2194u && cp <= 0x21AAu)
         || (cp >= 0x231Au && cp <= 0x23FFu)
         || (cp >= 0x25AAu && cp <= 0x25FEu)
         || (cp >= 0x2600u && cp <= 0x27BFu)
         || (cp >= 0x2934u && cp <= 0x2935u)
         || (cp >= 0x2B05u && cp <= 0x2B55u) || cp == 0x3030u || cp == 0x303Du
         || cp == 0x3297u || cp == 0x3299u
         || (cp >= 0x1F000u && cp <= 0x1FAFFu)
         || (cp >= 0x1FC00u && cp <= 0x1FFFDu);
}

/**
 * @brief Moves `it` past one extended grapheme cluster and returns its
 * estimated width: the width of its first code point (2 for the wide ranges
 * of the standard, else 1), as C++23 `std::format` measures text. An invalid
 * code unit is a cluster of its own with width 1.
 */
template <typename Char>
std::size_t
next_grapheme (Char const *&it, Char const *end) LUMEX_NOEXCEPT
{
  if (*it == static_cast<Char> ('\r') && it + 1 != end
      && it[1] == static_cast<Char> ('\n'))
    {
      it += 2; // CR LF is one cluster
      return 1;
    }
  if (static_cast<std::uint32_t> (*it) < 0x80u
      && (it + 1 == end || static_cast<std::uint32_t> (it[1]) < 0x80u))
    {
      ++it; // ASCII followed by ASCII (or the end): the common fast path
      return 1;
    }
  bool valid = true;
  std::uint32_t const first = decode (it, end, valid);
  if (!valid)
    return 1;
  std::size_t const width = is_wide_code_point (first) ? 2 : 1;
  std::uint32_t previous = first;
  bool regional_pair_open = is_regional_indicator (first);
  while (it != end)
    {
      Char const *next = it;
      bool next_valid = true;
      std::uint32_t const cp = decode (next, end, next_valid);
      if (!next_valid)
        break;
      bool joins = is_grapheme_extend (cp) || cp == 0x200Du;
      if (!joins && previous == 0x200Du && is_extended_pictographic (cp))
        joins = true;
      if (!joins && regional_pair_open && is_regional_indicator (cp))
        {
          joins = true;
          regional_pair_open = false;
        }
      if (!joins)
        break;
      previous = cp;
      it = next;
    }
  return width;
}

/** @brief Estimated display width, as `std::format` (C++23) defines it. */
template <typename Char>
std::size_t
estimate_width (Char const *it, Char const *end) LUMEX_NOEXCEPT
{
  std::size_t width = 0;
  while (it != end)
    width += next_grapheme (it, end);
  return width;
}

/**
 * @brief End of the longest prefix of whole grapheme clusters whose
 * estimated width is <= `limit`.
 */
template <typename Char>
Char const *
truncate_to_width (Char const *it, Char const *end,
                   std::size_t limit) LUMEX_NOEXCEPT
{
  std::size_t width = 0;
  while (it != end)
    {
      Char const *next = it;
      std::size_t const cluster_width = next_grapheme (next, end);
      if (width + cluster_width > limit)
        break;
      width += cluster_width;
      it = next;
    }
  return it;
}

template <typename Char>
void
write_fill (Buffer<Char> &buffer, format_specs_t<Char> const &specs,
            std::size_t count)
{
  for (std::size_t i = 0; i < count; ++i)
    buffer.append (specs.fill, specs.fill + specs.fill_size);
}

/** @brief Writes `[first, stop)` padded to `specs.width`. */
template <typename Char>
void
write_padded (Buffer<Char> &buffer, format_specs_t<Char> const &specs,
              Align default_align, Char const *first, Char const *stop)
{
  if (specs.width <= 0)
    {
      buffer.append (first, stop); // no width: nothing to measure
      return;
    }
  std::size_t const width = estimate_width (first, stop);
  std::size_t const target = static_cast<std::size_t> (specs.width);
  std::size_t const padding = target > width ? target - width : 0;
  Align const align = specs.align == Align::none ? default_align : specs.align;
  std::size_t const left = align == Align::right    ? padding
                           : align == Align::center ? padding / 2
                                                    : 0;
  write_fill (buffer, specs, left);
  buffer.append (first, stop);
  write_fill (buffer, specs, padding - left);
}

template <typename Char>
void
write_padded (Buffer<Char> &buffer, format_specs_t<Char> const &specs,
              Align default_align, std::basic_string<Char> const &text)
{
  write_padded (buffer, specs, default_align, text.data (),
                text.data () + text.size ());
}

/**
 * @brief Writes a number: `prefix` (sign and base prefix) then `digits`,
 * zero-padded after the prefix when `0` is set and no alignment is given,
 * otherwise padded like any text (right-aligned by default).
 */
template <typename Char>
void
write_number (Buffer<Char> &buffer, format_specs_t<Char> const &specs,
              std::basic_string<Char> const &prefix,
              std::basic_string<Char> const &digits, bool allow_zero_pad)
{
  if (specs.zero_pad && specs.align == Align::none && allow_zero_pad)
    {
      std::size_t const width
          = estimate_width (prefix.data (), prefix.data () + prefix.size ())
            + estimate_width (digits.data (), digits.data () + digits.size ());
      std::size_t const target = static_cast<std::size_t> (specs.width);
      buffer.append (prefix);
      for (std::size_t i = width; i < target; ++i)
        buffer.push_back (static_cast<Char> ('0'));
      buffer.append (digits);
      return;
    }
  write_padded (buffer, specs, Align::right, prefix + digits);
}

/** @brief Inserts `separator` into the integral digits per `grouping`. */
template <typename Char>
std::basic_string<Char>
group_digits (std::basic_string<Char> const &digits,
              std::string const &grouping, Char separator)
{
  if (grouping.empty () || digits.empty ())
    return digits;
  std::basic_string<Char> reversed;
  std::size_t group_index = 0;
  int group_size = static_cast<unsigned char> (grouping[0]);
  int in_group = 0;
  for (std::size_t i = digits.size (); i-- > 0;)
    {
      if (group_size > 0 && group_size < CHAR_MAX && in_group == group_size)
        {
          reversed.push_back (separator);
          in_group = 0;
          if (group_index + 1 < grouping.size ())
            {
              ++group_index;
              group_size = static_cast<unsigned char> (grouping[group_index]);
            }
        }
      reversed.push_back (digits[i]);
      ++in_group;
    }
  return std::basic_string<Char> (reversed.rbegin (), reversed.rend ());
}

template <typename UInt>
std::string
to_base (UInt value, unsigned base, bool upper)
{
  char const *const digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
  char buffer[130];
  char *end = buffer + sizeof (buffer);
  char *it = end;
  do
    {
      *--it = digits[static_cast<unsigned> (value % base)];
      value = static_cast<UInt> (value / base);
    }
  while (value != 0);
  return std::string (it, end);
}

template <typename Char>
std::basic_string<Char>
sign_text (bool negative, Sign sign)
{
  if (negative)
    return std::basic_string<Char> (1, static_cast<Char> ('-'));
  if (sign == Sign::plus)
    return std::basic_string<Char> (1, static_cast<Char> ('+'));
  if (sign == Sign::space)
    return std::basic_string<Char> (1, static_cast<Char> (' '));
  return std::basic_string<Char> ();
}

template <typename Char>
void write_character (Buffer<Char> &buffer, format_specs_t<Char> const &specs,
                      Char value);

/** @brief Writes an integer given as sign + magnitude. */
template <typename Char, typename UInt>
void
write_integer (Buffer<Char> &buffer, format_specs_t<Char> const &specs,
               bool negative, UInt magnitude,
               BasicFormatContext<Char> const &ctx)
{
  Char const type = specs.type;
  if (type == static_cast<Char> ('c'))
    {
      // The value must fit the character type itself (as std::format):
      // -128..127 for a signed `char`, 0..65535 for a 16-bit `wchar_t`.
      unsigned long long const max_positive = static_cast<unsigned long long> (
          (std::numeric_limits<Char>::max) ());
      unsigned long long const max_negative
          = std::is_signed<Char>::value
                ? 0ull
                      - static_cast<unsigned long long> (
                          static_cast<long long> (
                              (std::numeric_limits<Char>::min) ()))
                : 0ull;
      if (magnitude
          > static_cast<UInt> (negative ? max_negative : max_positive))
        throw FormatError ("character out of range");
      unsigned long long const bits
          = static_cast<unsigned long long> (magnitude);
      Char const value
          = negative ? static_cast<Char> (0ll - static_cast<long long> (bits))
                     : static_cast<Char> (bits);
      write_character (buffer, specs, value);
      return;
    }
  unsigned base = 10;
  std::string prefix;
  if (type == static_cast<Char> ('b') || type == static_cast<Char> ('B'))
    {
      base = 2;
      if (specs.alternate)
        prefix = type == static_cast<Char> ('b') ? "0b" : "0B";
    }
  else if (type == static_cast<Char> ('o'))
    {
      base = 8;
      if (specs.alternate && magnitude != 0)
        prefix = "0";
    }
  else if (type == static_cast<Char> ('x') || type == static_cast<Char> ('X'))
    {
      base = 16;
      if (specs.alternate)
        prefix = type == static_cast<Char> ('x') ? "0x" : "0X";
    }
  if (!specs.localized)
    {
      // Hot path without heap allocations: sign, prefix and digits are
      // built right-to-left in a stack array (128-bit binary fits).
      char const *const table = type == static_cast<Char> ('X')
                                    ? "0123456789ABCDEF"
                                    : "0123456789abcdef";
      Char text[140];
      Char *const end = text + sizeof (text) / sizeof (text[0]);
      Char *it = end;
      UInt value = magnitude;
      do
        {
          *--it = static_cast<Char> (
              table[static_cast<unsigned> (value % base)]);
          value = static_cast<UInt> (value / base);
        }
      while (value != 0);
      Char *const digits_begin = it;
      for (std::size_t i = prefix.size (); i-- > 0;)
        *--it = static_cast<Char> (prefix[i]);
      if (negative)
        *--it = static_cast<Char> ('-');
      else if (specs.sign == Sign::plus)
        *--it = static_cast<Char> ('+');
      else if (specs.sign == Sign::space)
        *--it = static_cast<Char> (' ');
      if (specs.zero_pad && specs.align == Align::none)
        {
          // ASCII only, so the width is the number of characters.
          std::size_t const length = static_cast<std::size_t> (end - it);
          std::size_t const target = static_cast<std::size_t> (specs.width);
          buffer.append (it, digits_begin);
          for (std::size_t i = length; i < target; ++i)
            buffer.push_back (static_cast<Char> ('0'));
          buffer.append (digits_begin, end);
          return;
        }
      write_padded (buffer, specs, Align::right, it, end);
      return;
    }
  std::basic_string<Char> digits = widen<Char> (
      to_base (magnitude, base, type == static_cast<Char> ('X')));
  // `L` groups the digits of every integer presentation (b, o, x too), as
  // std::format does; the base prefix stays in front of the groups.
  {
    std::numpunct<Char> const &facet
        = std::use_facet<std::numpunct<Char>> (ctx.locale ());
    digits = group_digits (digits, facet.grouping (), facet.thousands_sep ());
  }
  write_number (buffer, specs,
                sign_text<Char> (negative, specs.sign) + widen<Char> (prefix),
                digits, true);
}

/** @brief The `?` (debug) form of a string or a character. */
template <typename Char>
std::basic_string<Char>
escape (Char const *it, Char const *end, Char quote)
{
  std::basic_string<Char> result (1, quote);
  while (it != end)
    {
      Char const *const start = it;
      bool valid = true;
      std::uint32_t const cp = decode (it, end, valid);
      if (!valid)
        {
          for (Char const *unit = start; unit != it; ++unit)
            {
              std::uint32_t const raw
                  = sizeof (Char) == 1
                        ? static_cast<unsigned char> (*unit)
                        : static_cast<std::uint32_t> (*unit)
                              & (sizeof (Char) == 2 ? 0xFFFFu : 0xFFFFFFFFu);
              result += widen<Char> ("\\x{" + to_base (raw, 16, false) + "}");
            }
          continue;
        }
      if (cp == '\t')
        result += widen<Char> ("\\t");
      else if (cp == '\n')
        result += widen<Char> ("\\n");
      else if (cp == '\r')
        result += widen<Char> ("\\r");
      else if (cp == '\\')
        result += widen<Char> ("\\\\");
      else if (cp == static_cast<std::uint32_t> (quote))
        {
          result.push_back (static_cast<Char> ('\\'));
          result.push_back (quote);
        }
      else if (cp < 0x20 || cp == 0x7F || (cp >= 0x80 && cp < 0xA0))
        result += widen<Char> ("\\u{" + to_base (cp, 16, false) + "}");
      else
        result.append (start, it);
    }
  result.push_back (quote);
  return result;
}

template <typename Char>
void
write_character (Buffer<Char> &buffer, format_specs_t<Char> const &specs,
                 Char value)
{
  if (specs.type == static_cast<Char> ('?'))
    {
      write_padded (buffer, specs, Align::left,
                    escape (&value, &value + 1, static_cast<Char> ('\'')));
      return;
    }
  write_padded (buffer, specs, Align::left, &value, &value + 1);
}

template <typename Char>
void
write_string (Buffer<Char> &buffer, format_specs_t<Char> const &specs,
              Char const *first, Char const *stop)
{
  if (specs.type == static_cast<Char> ('?'))
    {
      std::basic_string<Char> escaped
          = escape (first, stop, static_cast<Char> ('"'));
      Char const *end = escaped.data () + escaped.size ();
      if (specs.precision >= 0)
        end = truncate_to_width (escaped.data (), end,
                                 static_cast<std::size_t> (specs.precision));
      write_padded (buffer, specs, Align::left, escaped.data (), end);
      return;
    }
  if (specs.precision >= 0)
    stop = truncate_to_width (first, stop,
                              static_cast<std::size_t> (specs.precision));
  write_padded (buffer, specs, Align::left, first, stop);
}

template <typename Char>
void
write_bool (Buffer<Char> &buffer, format_specs_t<Char> const &specs,
            bool value, BasicFormatContext<Char> const &ctx)
{
  if (is_integer_presentation (specs.type))
    {
      write_integer (buffer, specs, false, value ? 1ull : 0ull, ctx);
      return;
    }
  std::basic_string<Char> text;
  if (specs.localized)
    {
      std::numpunct<Char> const &facet
          = std::use_facet<std::numpunct<Char>> (ctx.locale ());
      text = value ? facet.truename () : facet.falsename ();
    }
  else
    text = widen<Char> (value ? "true" : "false");
  write_padded (buffer, specs, Align::left, text);
}

template <typename Char>
void
write_pointer (Buffer<Char> &buffer, format_specs_t<Char> const &specs,
               void const *value)
{
  bool const upper = specs.type == static_cast<Char> ('P');
  std::uintptr_t const address = reinterpret_cast<std::uintptr_t> (value);
  write_number (buffer, specs, widen<Char> (upper ? "0X" : "0x"),
                widen<Char> (to_base (address, 16, upper)), true);
}

// ----------------------------------------------------------------------
// Floating point
// ----------------------------------------------------------------------

/** @brief Floating-point style chosen from the presentation type. */
enum class FloatStyle : unsigned char
{
  shortest,
  general,
  scientific,
  fixed,
  hex
};

template <typename F> struct float_limits_t;

template <> struct float_limits_t<float>
{
  static int
  max_digits () LUMEX_NOEXCEPT
  {
    return 9;
  }

  static float
  read (char const *text) LUMEX_NOEXCEPT
  {
    return std::strtof (text, nullptr);
  }
};

template <> struct float_limits_t<double>
{
  static int
  max_digits () LUMEX_NOEXCEPT
  {
    return 17;
  }

  static double
  read (char const *text) LUMEX_NOEXCEPT
  {
    return std::strtod (text, nullptr);
  }
};

template <> struct float_limits_t<long double>
{
  static int
  max_digits () LUMEX_NOEXCEPT
  {
    return std::numeric_limits<long double>::max_digits10;
  }

  static long double
  read (char const *text) LUMEX_NOEXCEPT
  {
    return std::strtold (text, nullptr);
  }
};

/** @brief Replaces the C locale's decimal point with `.`. */
inline void
normalize_decimal_point (std::string &text)
{
  std::lconv const *const conventions = std::localeconv ();
  if (conventions == nullptr || conventions->decimal_point == nullptr)
    return;
  std::string const point (conventions->decimal_point);
  if (point.empty () || point == ".")
    return;
  std::size_t const found = text.find (point);
  if (found != std::string::npos)
    text.replace (found, point.size (), ".");
}

/** @brief `snprintf` with one `*` precision, as a `std::string`. */
template <typename F>
std::string
printf_float (char const *pattern, int precision, F value)
{
  typedef typename std::conditional<std::is_same<F, long double>::value,
                                    long double, double>::type promoted_type;
  promoted_type const promoted = static_cast<promoted_type> (value);
  int const size = std::snprintf (nullptr, 0, pattern, precision, promoted);
  if (size < 0)
    throw FormatError ("floating-point conversion failed");
  std::vector<char> text (static_cast<std::size_t> (size) + 1);
  std::snprintf (text.data (), text.size (), pattern, precision, promoted);
  return std::string (text.data (), static_cast<std::size_t> (size));
}

template <typename F>
char const *
printf_pattern (char type, bool alternate)
{
  bool const is_long = std::is_same<F, long double>::value;
  switch (type)
    {
    case 'e':
      return is_long ? (alternate ? "%#.*Le" : "%.*Le")
                     : (alternate ? "%#.*e" : "%.*e");
    case 'f':
      return is_long ? (alternate ? "%#.*Lf" : "%.*Lf")
                     : (alternate ? "%#.*f" : "%.*f");
    case 'g':
      return is_long ? (alternate ? "%#.*Lg" : "%.*Lg")
                     : (alternate ? "%#.*g" : "%.*g");
    default:
      return is_long ? (alternate ? "%#.*La" : "%.*La")
                     : (alternate ? "%#.*a" : "%.*a");
    }
}

/** @brief Removes `0x` / `0X` and redundant trailing zeros of `%a`. */
inline std::string
strip_hex_prefix (std::string text, bool trim_zeros)
{
  if (text.size () >= 2 && text[0] == '0'
      && (text[1] == 'x' || text[1] == 'X'))
    text.erase (0, 2);
  if (trim_zeros)
    {
      std::size_t const exponent = text.find ('p');
      std::size_t const point = text.find ('.');
      if (point != std::string::npos && exponent != std::string::npos)
        {
          std::size_t stop = exponent;
          while (stop > point + 1 && text[stop - 1] == '0')
            --stop;
          if (stop == point + 1)
            --stop;
          text.erase (stop, exponent - stop);
        }
    }
  return text;
}

/**
 * @brief Shortest decimal text that reads back to `value` (non-negative,
 * finite), in `std::to_chars` plain style: fixed or scientific, whichever is
 * shorter (fixed on a tie).
 */
template <typename F>
std::string
shortest_decimal (F value)
{
#if LUMEX_FORMAT_HAS_FLOAT_TO_CHARS
  char text[128];
  std::to_chars_result const result
      = std::to_chars (text, text + sizeof (text), value);
  return std::string (text, result.ptr);
#else
  if (value == 0)
    return "0";
  std::string scientific;
  int const max_digits = float_limits_t<F>::max_digits ();
  for (int digits = 1; digits <= max_digits; ++digits)
    {
      scientific
          = printf_float (printf_pattern<F> ('e', false), digits - 1, value);
      if (float_limits_t<F>::read (scientific.c_str ()) == value)
        break;
    }
  normalize_decimal_point (scientific);
  // scientific is "d[.ddd]e[+-]XX"
  std::size_t const e = scientific.find ('e');
  std::string mantissa = scientific.substr (0, e);
  int const exponent = std::atoi (scientific.c_str () + e + 1);
  std::string digits;
  for (std::size_t i = 0; i < mantissa.size (); ++i)
    if (mantissa[i] != '.')
      digits.push_back (mantissa[i]);
  while (digits.size () > 1 && digits[digits.size () - 1] == '0')
    digits.erase (digits.size () - 1);

  std::string sci (1, digits[0]);
  if (digits.size () > 1)
    sci += "." + digits.substr (1);
  sci += exponent < 0 ? "e-" : "e+";
  int const magnitude = exponent < 0 ? -exponent : exponent;
  if (magnitude < 10)
    sci += "0";
  sci += std::to_string (magnitude);

  std::string fixed;
  if (exponent >= 0)
    {
      std::size_t const integral = static_cast<std::size_t> (exponent) + 1;
      // Shortest digits that stop before the decimal point mean the value is
      // an integer; to_chars prints its exact digits (smallest difference on
      // a length tie), so 123456789.0f gives "123456792", not "123456790".
      if (digits.size () <= integral)
        fixed = printf_float (printf_pattern<F> ('f', false), 0, value);
      else
        fixed = digits.substr (0, integral) + "." + digits.substr (integral);
    }
  else
    fixed = "0." + std::string (static_cast<std::size_t> (-exponent - 1), '0')
            + digits;
  return fixed.size () <= sci.size () ? fixed : sci;
#endif
}

/**
 * @brief Text of a non-negative finite `value` in `style` / `precision`.
 * @param value Non-negative, finite value.
 * @param style Fixed, scientific, general, hex or shortest.
 * @param precision Digits after the point (significant digits for general);
 *        ignored for the shortest form.
 * @param alternate `#`: always print the decimal point.
 * @param keep_zeros `#` with an explicit `g` / `G`: trailing zeros stay (as
 *        std::format; without a type `#` only forces the decimal point).
 */
template <typename F>
std::string
float_body (F value, FloatStyle style, int precision, bool alternate,
            bool keep_zeros)
{
  std::string text;
  if (style == FloatStyle::shortest)
    text = shortest_decimal (value);
  else if (style == FloatStyle::general && keep_zeros)
    {
      text = printf_float (printf_pattern<F> ('g', true),
                           precision == 0 ? 1 : precision, value);
      normalize_decimal_point (text);
    }
  else
    {
#if LUMEX_FORMAT_HAS_FLOAT_TO_CHARS
      std::chars_format const format
          = style == FloatStyle::general      ? std::chars_format::general
            : style == FloatStyle::scientific ? std::chars_format::scientific
            : style == FloatStyle::fixed      ? std::chars_format::fixed
                                              : std::chars_format::hex;
      int const digits
          = style == FloatStyle::general && precision == 0 ? 1 : precision;
      // A stack buffer covers every common case; only a large precision
      // (or a huge fixed value) needs the heap.
      char stack[512];
      std::to_chars_result const first_try
          = precision < 0
                ? std::to_chars (stack, stack + sizeof (stack), value, format)
                : std::to_chars (stack, stack + sizeof (stack), value, format,
                                 digits);
      if (first_try.ec == std::errc ())
        text.assign (stack, first_try.ptr);
      else
        {
          std::vector<char> buffer (static_cast<std::size_t> (
              1024 + (precision > 0 ? precision : 0)));
          for (;;)
            {
              char *const first = buffer.data ();
              char *const stop = first + buffer.size ();
              std::to_chars_result const result
                  = precision < 0
                        ? std::to_chars (first, stop, value, format)
                        : std::to_chars (first, stop, value, format, digits);
              if (result.ec == std::errc ())
                {
                  text.assign (first, result.ptr);
                  break;
                }
              buffer.resize (buffer.size () * 2);
            }
        }
#else
      if (style == FloatStyle::hex)
        {
          if (precision < 0)
            {
              char const *const pattern
                  = std::is_same<F, long double>::value ? "%La" : "%a";
              typedef typename std::conditional<
                  std::is_same<F, long double>::value, long double,
                  double>::type promoted_type;
              char raw[128];
              std::snprintf (raw, sizeof (raw), pattern,
                             static_cast<promoted_type> (value));
              text = raw;
              normalize_decimal_point (text);
              text = strip_hex_prefix (text, true);
            }
          else
            {
              text = printf_float (printf_pattern<F> ('a', false), precision,
                                   value);
              normalize_decimal_point (text);
              text = strip_hex_prefix (text, false);
            }
        }
      else
        {
          char const type = style == FloatStyle::general      ? 'g'
                            : style == FloatStyle::scientific ? 'e'
                                                              : 'f';
          text = printf_float (
              printf_pattern<F> (type, false),
              style == FloatStyle::general && precision == 0 ? 1 : precision,
              value);
          normalize_decimal_point (text);
        }
#endif
    }
  if (alternate && text.find ('.') == std::string::npos)
    {
      std::size_t const exponent
          = text.find_first_of (style == FloatStyle::hex ? "p" : "e");
      if (exponent == std::string::npos)
        text.push_back ('.');
      else
        text.insert (exponent, 1, '.');
    }
  return text;
}

template <typename Char, typename F>
void
write_float (Buffer<Char> &buffer, format_specs_t<Char> const &specs, F value,
             BasicFormatContext<Char> const &ctx)
{
  Char const type = specs.type;
  bool const upper
      = type == static_cast<Char> ('E') || type == static_cast<Char> ('F')
        || type == static_cast<Char> ('G') || type == static_cast<Char> ('A');
  bool const negative = std::signbit (value);
  std::basic_string<Char> const sign = sign_text<Char> (negative, specs.sign);
  if (!std::isfinite (value))
    {
      std::string text = std::isnan (value) ? "nan" : "inf";
      if (upper)
        for (std::size_t i = 0; i < text.size (); ++i)
          text[i] = static_cast<char> (text[i] - 'a' + 'A');
      write_number (buffer, specs, sign, widen<Char> (text), false);
      return;
    }
  F const magnitude = negative ? -value : value;
  FloatStyle style = FloatStyle::shortest;
  int precision = specs.precision;
  if (type == Char ())
    style = precision < 0 ? FloatStyle::shortest : FloatStyle::general;
  else if (type == static_cast<Char> ('a') || type == static_cast<Char> ('A'))
    style = FloatStyle::hex;
  else if (type == static_cast<Char> ('e') || type == static_cast<Char> ('E'))
    {
      style = FloatStyle::scientific;
      if (precision < 0)
        precision = 6;
    }
  else if (type == static_cast<Char> ('f') || type == static_cast<Char> ('F'))
    {
      style = FloatStyle::fixed;
      if (precision < 0)
        precision = 6;
    }
  else
    {
      style = FloatStyle::general;
      if (precision < 0)
        precision = 6;
    }
  // Fixed / scientific output grows with the precision: refuse a size that
  // cannot be represented rather than trying to allocate it.
  if ((style == FloatStyle::fixed || style == FloatStyle::scientific)
      && precision > INT_MAX - 1024)
    throw FormatError ("number is too big");
  // Past the number of significant digits a value can have, a larger
  // general precision prints the same text; cap it to bound the buffer.
  int const general_cap = std::numeric_limits<F>::digits
                          - std::numeric_limits<F>::min_exponent + 16;
  if (style == FloatStyle::general && precision > general_cap)
    precision = general_cap;
  std::string text = float_body (magnitude, style, precision, specs.alternate,
                                 specs.alternate && type != Char ());
  if (upper)
    for (std::size_t i = 0; i < text.size (); ++i)
      if (text[i] >= 'a' && text[i] <= 'z')
        text[i] = static_cast<char> (text[i] - 'a' + 'A');

  std::basic_string<Char> body = widen<Char> (text);
  if (specs.localized && style != FloatStyle::hex)
    {
      std::numpunct<Char> const &facet
          = std::use_facet<std::numpunct<Char>> (ctx.locale ());
      std::size_t integral = 0;
      while (integral < body.size () && is_digit (body[integral]))
        ++integral;
      std::basic_string<Char> rest = body.substr (integral);
      if (!rest.empty () && rest[0] == static_cast<Char> ('.'))
        rest[0] = facet.decimal_point ();
      body = group_digits (body.substr (0, integral), facet.grouping (),
                           facet.thousands_sep ())
             + rest;
    }
  write_number (buffer, specs, sign, body, true);
}

// ----------------------------------------------------------------------
// Built-in formatter
// ----------------------------------------------------------------------

/**
 * @brief Parses `[[fill]align][width]` from `ctx.begin()`, the part every
 * specification shares (used by the range and chrono formatters, whose
 * specifications continue differently).
 * @param ctx Parse context positioned at the specification.
 * @param specs Receives the fill, alignment and (dynamic) width.
 * @param colon_is_fill Whether `:` may be the fill character.
 */
template <typename Char>
Char const *
parse_fill_align_width (BasicFormatParseContext<Char> &ctx,
                        format_specs_t<Char> &specs, bool colon_is_fill)
{
  Char const *it = ctx.begin ();
  Char const *const end = ctx.end ();
  if (it == end || *it == static_cast<Char> ('}'))
    return it;
  std::size_t const fill_length = code_point_length (it, end);
  if ((colon_is_fill || *it != static_cast<Char> (':'))
      && static_cast<std::size_t> (end - it) > fill_length
      && is_align (it[fill_length]))
    {
      if (*it == static_cast<Char> ('{'))
        throw FormatError ("invalid fill character '{'");
      for (std::size_t i = 0; i < fill_length; ++i)
        specs.fill[i] = it[i];
      specs.fill_size = static_cast<unsigned char> (fill_length);
      it += fill_length;
    }
  if (it != end && is_align (*it))
    {
      specs.align = *it == static_cast<Char> ('<')   ? Align::left
                    : *it == static_cast<Char> ('>') ? Align::right
                                                     : Align::center;
      ++it;
    }
  if (it != end && is_digit (*it))
    specs.width = parse_nonnegative_int (it, end);
  else if (it != end && *it == static_cast<Char> ('{'))
    specs.width_ref = parse_dynamic_ref (it, end, ctx);
  return it;
}

template <typename Char>
int
resolve_dynamic (arg_ref_t<Char> const &ref, int value,
                 BasicFormatContext<Char> const &ctx)
{
  if (ref.kind == ArgRefKind::none)
    return value;
  int const id = ref.kind == ArgRefKind::index
                     ? ref.index
                     : ctx.arg_id (ref.name, ref.name_size);
  format_arg_t<Char> const arg = ctx.arg (id);
  switch (arg.kind)
    {
    case ArgKind::signed_int:
      if (arg.signed_value < 0 || arg.signed_value > INT_MAX)
        throw FormatError ("width/precision is out of range");
      return static_cast<int> (arg.signed_value);
    case ArgKind::unsigned_int:
      if (arg.unsigned_value > static_cast<unsigned long long> (INT_MAX))
        throw FormatError ("width/precision is out of range");
      return static_cast<int> (arg.unsigned_value);
#if LUMEX_FORMAT_HAS_INT128
    case ArgKind::signed_int128:
      if (arg.int128_value < 0 || arg.int128_value > INT_MAX)
        throw FormatError ("width/precision is out of range");
      return static_cast<int> (arg.int128_value);
    case ArgKind::unsigned_int128:
      if (arg.uint128_value > static_cast<unsigned __int128> (INT_MAX))
        throw FormatError ("width/precision is out of range");
      return static_cast<int> (arg.uint128_value);
#endif
    case ArgKind::none:
      throw FormatError ("argument not found");
    default:
      throw FormatError ("width/precision is not integer");
    }
}

/**
 * @class BuiltinFormatter
 * @brief Parses a standard specification for one `SpecKind` and writes a
 * built-in argument with it. Base of the public built-in `Formatter`
 * specializations.
 */
template <typename Char> class BuiltinFormatter
{
public:
  LUMEX_FORMAT_CONSTEXPR explicit BuiltinFormatter (SpecKind kind)
      LUMEX_NOEXCEPT : _kind (kind),
                       _specs ()
  {
  }

  LUMEX_FORMAT_CONSTEXPR Char const *
  parse (BasicFormatParseContext<Char> &ctx)
  {
    _specs = format_specs_t<Char> (); // a second parse starts from scratch
    return parse_std_specs (ctx, _specs, _kind);
  }

  /** @brief The specification with dynamic width / precision resolved. */
  format_specs_t<Char>
  resolved_specs (BasicFormatContext<Char> const &ctx) const
  {
    format_specs_t<Char> specs = _specs;
    specs.width = resolve_dynamic (specs.width_ref, specs.width, ctx);
    specs.precision
        = resolve_dynamic (specs.precision_ref, specs.precision, ctx);
    return specs;
  }

  BasicAppender<Char>
  format_arg (format_arg_t<Char> const &arg,
              BasicFormatContext<Char> &ctx) const
  {
    format_specs_t<Char> const specs = resolved_specs (ctx);
    Buffer<Char> &buffer = ctx.out ().buffer ();
    switch (arg.kind)
      {
      case ArgKind::signed_int:
        write_integer (
            buffer, specs, arg.signed_value < 0,
            arg.signed_value < 0
                ? 0ull - static_cast<unsigned long long> (arg.signed_value)
                : static_cast<unsigned long long> (arg.signed_value),
            ctx);
        break;
      case ArgKind::unsigned_int:
        write_integer (buffer, specs, false, arg.unsigned_value, ctx);
        break;
#if LUMEX_FORMAT_HAS_INT128
      case ArgKind::signed_int128:
        write_integer (
            buffer, specs, arg.int128_value < 0,
            arg.int128_value < 0
                ? static_cast<unsigned __int128> (0)
                      - static_cast<unsigned __int128> (arg.int128_value)
                : static_cast<unsigned __int128> (arg.int128_value),
            ctx);
        break;
      case ArgKind::unsigned_int128:
        write_integer (buffer, specs, false, arg.uint128_value, ctx);
        break;
#endif
      case ArgKind::boolean:
        write_bool (buffer, specs, arg.bool_value, ctx);
        break;
      case ArgKind::character:
        if (is_integer_presentation (specs.type))
          {
            typedef typename std::make_unsigned<Char>::type unsigned_type;
            write_integer (buffer, specs, false,
                           static_cast<unsigned long long> (
                               static_cast<unsigned_type> (arg.char_value)),
                           ctx);
          }
        else
          write_character (buffer, specs, arg.char_value);
        break;
      case ArgKind::float_value:
        write_float (buffer, specs, arg.float_value, ctx);
        break;
      case ArgKind::double_value:
        write_float (buffer, specs, arg.double_value, ctx);
        break;
      case ArgKind::long_double_value:
        write_float (buffer, specs, arg.long_double_value, ctx);
        break;
      case ArgKind::c_string:
        if (arg.c_string == nullptr)
          throw FormatError ("string pointer is null");
        write_string (buffer, specs, arg.c_string,
                      arg.c_string + c_string_length (arg.c_string));
        break;
      case ArgKind::string:
        write_string (buffer, specs, arg.string.data,
                      arg.string.data + arg.string.size);
        break;
      case ArgKind::pointer:
        write_pointer (buffer, specs, arg.pointer);
        break;
      default:
        throw FormatError ("argument not found");
      }
    return ctx.out ();
  }

  format_specs_t<Char> const &
  specs () const LUMEX_NOEXCEPT
  {
    return _specs;
  }

private:
  SpecKind _kind;
  format_specs_t<Char> _specs;
};

// ----------------------------------------------------------------------
// Building arguments
// ----------------------------------------------------------------------

template <typename Char, typename T>
void
format_custom_arg (void const *value, BasicFormatParseContext<Char> &pctx,
                   BasicFormatContext<Char> &fctx)
{
  Formatter<T, Char> formatter;
  pctx.advance_to (formatter.parse (pctx));
  fctx.advance_to (formatter.format (*static_cast<T const *> (value), fctx));
}

template <typename Char, ArgKind Kind>
using kind_tag = std::integral_constant<ArgKind, Kind>;

template <typename Char, typename T>
void
fill_arg (format_arg_t<Char> &arg, T const &value,
          kind_tag<Char, ArgKind::signed_int>)
{
  arg.signed_value = static_cast<long long> (value);
}

template <typename Char, typename T>
void
fill_arg (format_arg_t<Char> &arg, T const &value,
          kind_tag<Char, ArgKind::unsigned_int>)
{
  arg.unsigned_value = static_cast<unsigned long long> (value);
}

#if LUMEX_FORMAT_HAS_INT128
template <typename Char, typename T>
void
fill_arg (format_arg_t<Char> &arg, T const &value,
          kind_tag<Char, ArgKind::signed_int128>)
{
  arg.int128_value = value;
}

template <typename Char, typename T>
void
fill_arg (format_arg_t<Char> &arg, T const &value,
          kind_tag<Char, ArgKind::unsigned_int128>)
{
  arg.uint128_value = value;
}
#endif

template <typename Char, typename T>
void
fill_arg (format_arg_t<Char> &arg, T const &value,
          kind_tag<Char, ArgKind::boolean>)
{
  arg.bool_value = value;
}

template <typename Char, typename T>
void
fill_arg (format_arg_t<Char> &arg, T const &value,
          kind_tag<Char, ArgKind::character>)
{
  arg.char_value = static_cast<Char> (
      static_cast<typename std::make_unsigned<T>::type> (value));
}

template <typename Char, typename T>
void
fill_arg (format_arg_t<Char> &arg, T const &value,
          kind_tag<Char, ArgKind::float_value>)
{
  arg.float_value = value;
}

template <typename Char, typename T>
void
fill_arg (format_arg_t<Char> &arg, T const &value,
          kind_tag<Char, ArgKind::double_value>)
{
  arg.double_value = value;
}

template <typename Char, typename T>
void
fill_arg (format_arg_t<Char> &arg, T const &value,
          kind_tag<Char, ArgKind::long_double_value>)
{
  arg.long_double_value = value;
}

template <typename Char, typename T>
void
fill_arg (format_arg_t<Char> &arg, T const &value,
          kind_tag<Char, ArgKind::c_string>)
{
  arg.c_string = value;
}

template <typename Char, typename T>
void
fill_arg (format_arg_t<Char> &arg, T const &value,
          kind_tag<Char, ArgKind::string>)
{
  arg.string.data = value.data ();
  arg.string.size = static_cast<std::size_t> (value.size ());
}

template <typename Char, typename T>
void
fill_arg (format_arg_t<Char> &arg, T const &value,
          kind_tag<Char, ArgKind::pointer>)
{
  arg.pointer = static_cast<void const *> (value);
}

template <typename Char>
void
fill_arg (format_arg_t<Char> &arg, std::nullptr_t const &,
          kind_tag<Char, ArgKind::pointer>)
{
  arg.pointer = nullptr;
}

template <typename Char, typename T>
void
fill_arg (format_arg_t<Char> &arg, T const &value,
          kind_tag<Char, ArgKind::custom>)
{
  arg.custom.value = static_cast<void const *> (&value);
  arg.custom.format = &format_custom_arg<Char, T>;
}

/** @brief Type-erases one (decayed) value into `arg`. */
template <typename Char, typename T>
void
make_arg (format_arg_t<Char> &arg, T const &value)
{
  typedef typename std::decay<T>::type decayed_type;
  LUMEX_CONSTEXPR ArgKind kind = arg_kind_of<Char, decayed_type> ();
  static_assert (!is_other_character_type<decayed_type> ()
                     || is_character_type<Char, decayed_type> (),
                 "mixing character types is disallowed");
  static_assert (!std::is_pointer<decayed_type>::value
                     || kind != ArgKind::none,
                 "formatting of non-void pointers is disallowed; cast to "
                 "void const *");
  static_assert (kind != ArgKind::none, "type is not formattable: specialize "
                                        "lumex::core::fmt::Formatter");
  arg.kind = kind;
  Detail::fill_arg (arg, value, kind_tag<Char, kind> ());
}

template <typename Char, std::size_t N, std::size_t M, typename T>
void
store_arg (format_arg_store_t<Char, N, M> &store, std::size_t index,
           T const &value)
{
  Detail::make_arg (store.args[index], value);
}

template <typename Char, std::size_t N, std::size_t M, typename T>
void
store_arg (format_arg_store_t<Char, N, M> &store, std::size_t index,
           named_arg_t<Char, T> const &value)
{
  Detail::make_arg (store.args[index], value.value);
  std::size_t const length = c_string_length (value.name);
  for (std::size_t i = 0; i < store.named_count; ++i)
    if (c_string_length (store.named[i].name) == length
        && std::char_traits<Char>::compare (store.named[i].name, value.name,
                                            length)
               == 0)
      throw FormatError ("duplicate named arg");
  store.named[store.named_count].name = value.name;
  store.named[store.named_count].index = static_cast<int> (index);
  ++store.named_count;
}

template <typename Char, typename Store>
void
store_args (Store &, std::size_t)
{
}

template <typename Char, typename Store, typename First, typename... Rest>
void
store_args (Store &store, std::size_t index, First const &first,
            Rest const &...rest)
{
  Detail::store_arg (store, index, first);
  Detail::store_args<Char> (store, index + 1, rest...);
}

// ----------------------------------------------------------------------
// Run-time formatting and the compile-time checker
// ----------------------------------------------------------------------

/** @brief `parse_format_string` handler that formats into a buffer. */
template <typename Char> class FormatHandler
{
public:
  FormatHandler (Buffer<Char> &buffer, BasicStringRef<Char> text,
                 BasicFormatArgs<Char> const &args, std::locale const *locale)
      : _buffer (buffer), _parse_ctx (text, args.size ()),
        _format_ctx (buffer, args, locale), _args (args),
        _field_start (FormatError::no_position ())
  {
  }

  void
  on_text (Char const *first, Char const *stop)
  {
    _buffer.append (first, stop);
  }

  void
  on_field_start (std::size_t offset) LUMEX_NOEXCEPT
  {
    _field_start = offset;
  }

  void
  on_auto_arg ()
  {
    _arg = _args.get (_parse_ctx.next_arg_id ());
  }

  void
  on_index_arg (int id)
  {
    _parse_ctx.check_arg_id (id);
    _arg = _args.get (id);
  }

  void
  on_name_arg (Char const *name, std::size_t size)
  {
    _parse_ctx.check_named_arg ();
    int const id = _args.find (name, size);
    if (id < 0)
      throw FormatError ("argument not found");
    _arg = _args.get (id);
  }

  Char const *
  on_format_specs (Char const *it, Char const *)
  {
    if (_arg.kind == ArgKind::none)
      throw FormatError ("argument not found");
    _parse_ctx.advance_to (it);
    if (_arg.kind == ArgKind::custom)
      _arg.custom.format (_arg.custom.value, _parse_ctx, _format_ctx);
    else
      {
        BuiltinFormatter<Char> formatter (spec_kind_of (_arg.kind));
        _parse_ctx.advance_to (formatter.parse (_parse_ctx));
        formatter.format_arg (_arg, _format_ctx);
      }
    return _parse_ctx.begin ();
  }

  std::size_t
  field_start () const LUMEX_NOEXCEPT
  {
    return _field_start;
  }

private:
  Buffer<Char> &_buffer;
  BasicFormatParseContext<Char> _parse_ctx;
  BasicFormatContext<Char> _format_ctx;
  BasicFormatArgs<Char> _args;
  format_arg_t<Char> _arg;
  std::size_t _field_start;
};

/** @brief Formats into `buffer`; adds the field offset to errors. */
template <typename Char>
void
vformat_to_buffer (Buffer<Char> &buffer, BasicStringRef<Char> text,
                   BasicFormatArgs<Char> const &args,
                   std::locale const *locale)
{
  FormatHandler<Char> handler (buffer, text, args, locale);
  try
    {
      parse_format_string (text.begin (), text.end (), handler);
    }
  catch (FormatError const &error)
    {
      if (error.position () != FormatError::no_position ())
        throw;
      throw FormatError (error.what (), handler.field_start ());
    }
}

#if LUMEX_FORMAT_HAS_CONSTEVAL
/** @brief `parse_format_string` handler that only validates (consteval). */
template <typename Char> class FormatStringChecker
{
public:
  constexpr FormatStringChecker (BasicStringRef<Char> text, int arg_count,
                                 ArgKind const *kinds, bool has_named)
      : _ctx (text, arg_count, kinds, has_named), _kinds (kinds), _current (-1)
  {
  }

  constexpr void
  on_text (Char const *, Char const *)
  {
  }

  constexpr void
  on_field_start (std::size_t)
  {
  }

  constexpr void
  on_auto_arg ()
  {
    _current = _ctx.next_arg_id ();
  }

  constexpr void
  on_index_arg (int id)
  {
    _ctx.check_arg_id (id);
    _current = id;
  }

  constexpr void
  on_name_arg (Char const *, std::size_t)
  {
    _ctx.check_named_arg ();
    _current = -2;
  }

  constexpr Char const *
  on_format_specs (Char const *it, Char const *end)
  {
    if (_current < 0 || _kinds[_current] == ArgKind::custom)
      return skip_specs (it, end);
    _ctx.advance_to (it);
    BuiltinFormatter<Char> formatter (spec_kind_of (_kinds[_current]));
    return formatter.parse (_ctx);
  }

private:
  BasicFormatParseContext<Char> _ctx;
  ArgKind const *_kinds;
  int _current;
};

template <typename Char, typename... Args>
consteval void
check_format_string (BasicStringRef<Char> text)
{
  constexpr ArgKind kinds[]
      = { arg_kind_of<Char, typename std::decay<Args>::type> ()...,
          ArgKind::none };
  constexpr bool has_named
      = count_named<typename std::decay<Args>::type...>::value > 0;
  FormatStringChecker<Char> checker (text, static_cast<int> (sizeof...(Args)),
                                     kinds, has_named);
  parse_format_string (text.begin (), text.end (), checker);
}
#endif
} // namespace Detail

// ----------------------------------------------------------------------
// Format strings
// ----------------------------------------------------------------------

/** @brief A format string that is only checked at run time. */
template <typename Char> struct runtime_format_string_t
{
  Detail::BasicStringRef<Char> text;
};

/** @brief Skips the compile-time check; the string is checked when used. */
inline runtime_format_string_t<char>
runtime (Detail::BasicStringRef<char> text) LUMEX_NOEXCEPT
{
  return runtime_format_string_t<char>{ text };
}

inline runtime_format_string_t<wchar_t>
runtime (Detail::BasicStringRef<wchar_t> text) LUMEX_NOEXCEPT
{
  return runtime_format_string_t<wchar_t>{ text };
}

/**
 * @class BasicFormatString
 * @brief Format string for arguments `Args...`: checked against them at
 * compile time from C++20 on (`consteval` constructor), at run time before.
 */
template <typename Char, typename... Args> class BasicFormatString
{
public:
#if LUMEX_FORMAT_HAS_CONSTEVAL
  template <typename Text,
            typename = typename std::enable_if<std::is_convertible<
                Text const &, Detail::BasicStringRef<Char>>::value>::type>
  consteval BasicFormatString (Text const &text) : _text (text)
  {
    Detail::check_format_string<Char, Args...> (_text);
  }
#else
  template <typename Text,
            typename = typename std::enable_if<std::is_convertible<
                Text const &, Detail::BasicStringRef<Char>>::value>::type>
  BasicFormatString (Text const &text) : _text (text)
  {
  }
#endif

  BasicFormatString (runtime_format_string_t<Char> text) LUMEX_NOEXCEPT
      : _text (text.text)
  {
  }

  LUMEX_CONSTEXPR Detail::BasicStringRef<Char>
  get () const LUMEX_NOEXCEPT
  {
    return _text;
  }

private:
  Detail::BasicStringRef<Char> _text;
};

template <typename... Args>
using FormatString = BasicFormatString<
    char,
    typename lumex::core::utility::traits::meta::type_identity<Args>::type...>;

template <typename... Args>
using WFormatString = BasicFormatString<
    wchar_t,
    typename lumex::core::utility::traits::meta::type_identity<Args>::type...>;

// ----------------------------------------------------------------------
// Arguments
// ----------------------------------------------------------------------

/** @brief Names `value` for a `{name}` replacement field. */
template <typename T>
Detail::named_arg_t<char, T>
arg (char const *name, T const &value) LUMEX_NOEXCEPT
{
  return Detail::named_arg_t<char, T>{ name, value };
}

template <typename T>
Detail::named_arg_t<wchar_t, T>
arg (wchar_t const *name, T const &value) LUMEX_NOEXCEPT
{
  return Detail::named_arg_t<wchar_t, T>{ name, value };
}

/**
 * @brief Type-erased arguments for `vformat`; the result refers to `args`,
 * so use it within the same full expression.
 */
template <typename... Args>
Detail::format_arg_store_t<char, sizeof...(Args),
                           Detail::count_named<Args...>::value>
make_format_args (Args const &...args)
{
  Detail::format_arg_store_t<char, sizeof...(Args),
                             Detail::count_named<Args...>::value>
      store;
  Detail::store_args<char> (store, 0, args...);
  return store;
}

template <typename... Args>
Detail::format_arg_store_t<wchar_t, sizeof...(Args),
                           Detail::count_named<Args...>::value>
make_wformat_args (Args const &...args)
{
  Detail::format_arg_store_t<wchar_t, sizeof...(Args),
                             Detail::count_named<Args...>::value>
      store;
  Detail::store_args<wchar_t> (store, 0, args...);
  return store;
}

// ----------------------------------------------------------------------
// Built-in formatters (for reuse by user formatters)
// ----------------------------------------------------------------------

/**
 * @brief Standard formatter of a built-in type; a user `Formatter` can
 * inherit it to accept the standard specification.
 */
template <typename T, typename Char>
class Formatter<T, Char,
                typename std::enable_if<
                    Detail::builtin_kind<Char, typename std::decay<T>::type> ()
                    != Detail::ArgKind::none>::type>
{
public:
  LUMEX_FORMAT_CONSTEXPR
  Formatter () LUMEX_NOEXCEPT
      : _impl (Detail::spec_kind_of (
            Detail::builtin_kind<Char, typename std::decay<T>::type> ()))
  {
  }

  LUMEX_FORMAT_CONSTEXPR Char const *
  parse (BasicFormatParseContext<Char> &ctx)
  {
    return _impl.parse (ctx);
  }

  BasicAppender<Char>
  format (T const &value, BasicFormatContext<Char> &ctx) const
  {
    Detail::format_arg_t<Char> arg;
    Detail::make_arg (arg, value);
    return _impl.format_arg (arg, ctx);
  }

private:
  Detail::BuiltinFormatter<Char> _impl;
};

/**
 * @brief Enums declared with `LUMEX_DEFINE_REFLECTED_ENUM`: the name by
 * default (`{}`, `{:s}`, string options), the underlying value with an
 * integer presentation type (`{:d}`, `{:#x}`, ...).
 */
template <typename Enum, typename Char>
class Formatter<
    Enum, Char,
    typename std::enable_if<lumex::core::utility::traits::enums::
                                is_reflected_enum<Enum>::value>::type>
{
public:
  LUMEX_FORMAT_CONSTEXPR
  Formatter () LUMEX_NOEXCEPT : _impl (Detail::SpecKind::enumeration) {}

  LUMEX_FORMAT_CONSTEXPR Char const *
  parse (BasicFormatParseContext<Char> &ctx)
  {
    return _impl.parse (ctx);
  }

  BasicAppender<Char>
  format (Enum value, BasicFormatContext<Char> &ctx) const
  {
    Detail::format_specs_t<Char> const specs = _impl.resolved_specs (ctx);
    Detail::Buffer<Char> &buffer = ctx.out ().buffer ();
    typedef typename std::underlying_type<Enum>::type underlying_type;
    if (Detail::is_integer_presentation (specs.type))
      {
        underlying_type const number = static_cast<underlying_type> (value);
        bool const negative = std::is_signed<underlying_type>::value
                              && number < underlying_type ();
        unsigned long long const magnitude
            = negative ? 0ull - static_cast<unsigned long long> (number)
                       : static_cast<unsigned long long> (number);
        Detail::write_integer (buffer, specs, negative, magnitude, ctx);
      }
    else
      {
        std::basic_string<Char> const name
            = Detail::widen<Char> (std::string (toString (value)));
        Detail::write_string (buffer, specs, name.data (),
                              name.data () + name.size ());
      }
    return ctx.out ();
  }

private:
  Detail::BuiltinFormatter<Char> _impl;
};

/**
 * @class OstreamFormatter
 * @brief Base for formatters of types that have `operator<<`: the value is
 * streamed, then written with the string specification (fill, align, width,
 * precision).
 */
template <typename Char> class OstreamFormatter
{
public:
  LUMEX_FORMAT_CONSTEXPR
  OstreamFormatter () LUMEX_NOEXCEPT : _impl (Detail::SpecKind::string) {}

  LUMEX_FORMAT_CONSTEXPR Char const *
  parse (BasicFormatParseContext<Char> &ctx)
  {
    return _impl.parse (ctx);
  }

  template <typename T>
  BasicAppender<Char>
  format (T const &value, BasicFormatContext<Char> &ctx) const
  {
    std::basic_ostringstream<Char> stream;
    stream << value;
    std::basic_string<Char> const text = stream.str ();
    Detail::write_string (ctx.out ().buffer (), _impl.resolved_specs (ctx),
                          text.data (), text.data () + text.size ());
    return ctx.out ();
  }

private:
  Detail::BuiltinFormatter<Char> _impl;
};

/** @brief Wraps a value so it is formatted through its `operator<<`. */
template <typename T> struct streamed_t
{
  T const &value;
};

/** @brief Formats `value` through `operator<<` (opt-in, fmt 10+ style). */
template <typename T>
streamed_t<T>
streamed (T const &value) LUMEX_NOEXCEPT
{
  return streamed_t<T>{ value };
}

template <typename T, typename Char>
class Formatter<streamed_t<T>, Char, void> : public OstreamFormatter<Char>
{
public:
  BasicAppender<Char>
  format (streamed_t<T> const &value, BasicFormatContext<Char> &ctx) const
  {
    return OstreamFormatter<Char>::format (value.value, ctx);
  }
};

// ----------------------------------------------------------------------
// Output API
// ----------------------------------------------------------------------

/** @brief Result of `format_to_n`: the iterator past the written text and
 * the size the full output would have had. */
template <typename OutputIt> struct format_to_n_result_t
{
  OutputIt out;
  std::ptrdiff_t size;
};

/** @brief Result of `try_format`: the text, or `success == false` and the
 * error message. Never throws. */
template <typename Char> struct try_format_result_t
{
  std::basic_string<Char> text;
  bool success;
  std::string error;
};

inline std::string
vformat (Detail::BasicStringRef<char> text, FormatArgs args)
{
  std::string result;
  Detail::StringBuffer<char> buffer (result);
  Detail::vformat_to_buffer (buffer, text, args, nullptr);
  return result;
}

inline std::wstring
vformat (Detail::BasicStringRef<wchar_t> text, WFormatArgs args)
{
  std::wstring result;
  Detail::StringBuffer<wchar_t> buffer (result);
  Detail::vformat_to_buffer (buffer, text, args, nullptr);
  return result;
}

inline std::string
vformat (std::locale const &locale, Detail::BasicStringRef<char> text,
         FormatArgs args)
{
  std::string result;
  Detail::StringBuffer<char> buffer (result);
  Detail::vformat_to_buffer (buffer, text, args, &locale);
  return result;
}

inline std::wstring
vformat (std::locale const &locale, Detail::BasicStringRef<wchar_t> text,
         WFormatArgs args)
{
  std::wstring result;
  Detail::StringBuffer<wchar_t> buffer (result);
  Detail::vformat_to_buffer (buffer, text, args, &locale);
  return result;
}

template <typename OutputIt>
OutputIt
vformat_to (OutputIt out, Detail::BasicStringRef<char> text, FormatArgs args)
{
  Detail::IteratorBuffer<char, OutputIt> buffer (out);
  Detail::vformat_to_buffer (buffer, text, args, nullptr);
  return buffer.out ();
}

template <typename OutputIt>
OutputIt
vformat_to (OutputIt out, Detail::BasicStringRef<wchar_t> text,
            WFormatArgs args)
{
  Detail::IteratorBuffer<wchar_t, OutputIt> buffer (out);
  Detail::vformat_to_buffer (buffer, text, args, nullptr);
  return buffer.out ();
}

/** @brief Formats `args` into a `std::string`. */
template <typename... Args>
std::string
format (FormatString<Args...> text, Args &&...args)
{
  return ::lumex::core::fmt::vformat (
      text.get (), ::lumex::core::fmt::make_format_args (args...));
}

template <typename... Args>
std::wstring
format (WFormatString<Args...> text, Args &&...args)
{
  return ::lumex::core::fmt::vformat (
      text.get (), ::lumex::core::fmt::make_wformat_args (args...));
}

/** @brief As `format`; `L` fields use `locale`. */
template <typename... Args>
std::string
format (std::locale const &locale, FormatString<Args...> text, Args &&...args)
{
  return ::lumex::core::fmt::vformat (
      locale, text.get (), ::lumex::core::fmt::make_format_args (args...));
}

template <typename... Args>
std::wstring
format (std::locale const &locale, WFormatString<Args...> text, Args &&...args)
{
  return ::lumex::core::fmt::vformat (
      locale, text.get (), ::lumex::core::fmt::make_wformat_args (args...));
}

/** @brief Writes the formatted text through an output iterator. */
template <typename OutputIt, typename... Args>
OutputIt
format_to (OutputIt out, FormatString<Args...> text, Args &&...args)
{
  return ::lumex::core::fmt::vformat_to (
      out, text.get (), ::lumex::core::fmt::make_format_args (args...));
}

template <typename OutputIt, typename... Args>
OutputIt
format_to (OutputIt out, WFormatString<Args...> text, Args &&...args)
{
  return ::lumex::core::fmt::vformat_to (
      out, text.get (), ::lumex::core::fmt::make_wformat_args (args...));
}

template <typename OutputIt, typename... Args>
OutputIt
format_to (OutputIt out, std::locale const &locale, FormatString<Args...> text,
           Args &&...args)
{
  Detail::IteratorBuffer<char, OutputIt> buffer (out);
  Detail::vformat_to_buffer (
      buffer, text.get (),
      FormatArgs (::lumex::core::fmt::make_format_args (args...)), &locale);
  return buffer.out ();
}

/** @brief Writes at most `limit` characters; `size` is the full length. */
template <typename OutputIt, typename... Args>
format_to_n_result_t<OutputIt>
format_to_n (OutputIt out, std::ptrdiff_t limit, FormatString<Args...> text,
             Args &&...args)
{
  Detail::TruncatingBuffer<char, OutputIt> buffer (
      out, limit > 0 ? static_cast<std::size_t> (limit) : 0);
  Detail::vformat_to_buffer (
      buffer, text.get (),
      FormatArgs (::lumex::core::fmt::make_format_args (args...)), nullptr);
  format_to_n_result_t<OutputIt> result
      = { buffer.out (), static_cast<std::ptrdiff_t> (buffer.count ()) };
  return result;
}

template <typename OutputIt, typename... Args>
format_to_n_result_t<OutputIt>
format_to_n (OutputIt out, std::ptrdiff_t limit, WFormatString<Args...> text,
             Args &&...args)
{
  Detail::TruncatingBuffer<wchar_t, OutputIt> buffer (
      out, limit > 0 ? static_cast<std::size_t> (limit) : 0);
  Detail::vformat_to_buffer (
      buffer, text.get (),
      WFormatArgs (::lumex::core::fmt::make_wformat_args (args...)), nullptr);
  format_to_n_result_t<OutputIt> result
      = { buffer.out (), static_cast<std::ptrdiff_t> (buffer.count ()) };
  return result;
}

/** @brief Number of characters `format` would produce. */
template <typename... Args>
std::size_t
formatted_size (FormatString<Args...> text, Args &&...args)
{
  Detail::CountingBuffer<char> buffer;
  Detail::vformat_to_buffer (
      buffer, text.get (),
      FormatArgs (::lumex::core::fmt::make_format_args (args...)), nullptr);
  return buffer.count ();
}

template <typename... Args>
std::size_t
formatted_size (WFormatString<Args...> text, Args &&...args)
{
  Detail::CountingBuffer<wchar_t> buffer;
  Detail::vformat_to_buffer (
      buffer, text.get (),
      WFormatArgs (::lumex::core::fmt::make_wformat_args (args...)), nullptr);
  return buffer.count ();
}

/**
 * @brief Like `format`, but never throws: a failure (bad specification at
 * run time, missing argument, allocation failure) sets `success = false`
 * and `error`.
 */
template <typename... Args>
try_format_result_t<char>
try_format (FormatString<Args...> text, Args &&...args) LUMEX_NOEXCEPT
{
  try_format_result_t<char> result;
  result.success = false;
  try
    {
      result.text = ::lumex::core::fmt::vformat (
          text.get (), ::lumex::core::fmt::make_format_args (args...));
      result.success = true;
    }
  catch (std::exception const &error)
    {
      try
        {
          result.error = error.what ();
        }
      catch (...)
        {
        }
    }
  catch (...)
    {
      try
        {
          result.error = "unknown exception";
        }
      catch (...)
        {
        }
    }
  return result;
}

template <typename... Args>
try_format_result_t<wchar_t>
try_format (WFormatString<Args...> text, Args &&...args) LUMEX_NOEXCEPT
{
  try_format_result_t<wchar_t> result;
  result.success = false;
  try
    {
      result.text = ::lumex::core::fmt::vformat (
          text.get (), ::lumex::core::fmt::make_wformat_args (args...));
      result.success = true;
    }
  catch (std::exception const &error)
    {
      try
        {
          result.error = error.what ();
        }
      catch (...)
        {
        }
    }
  catch (...)
    {
      try
        {
          result.error = "unknown exception";
        }
      catch (...)
        {
        }
    }
  return result;
}

/** @brief Writes the formatted text to `stream`. */
template <typename... Args>
void
print (std::ostream &stream, FormatString<Args...> text, Args &&...args)
{
  stream << ::lumex::core::fmt::vformat (
      text.get (), ::lumex::core::fmt::make_format_args (args...));
}

template <typename... Args>
void
print (std::wostream &stream, WFormatString<Args...> text, Args &&...args)
{
  stream << ::lumex::core::fmt::vformat (
      text.get (), ::lumex::core::fmt::make_wformat_args (args...));
}

/** @brief Writes the formatted text to `std::cout`. */
template <typename... Args>
void
print (FormatString<Args...> text, Args &&...args)
{
  std::cout << ::lumex::core::fmt::vformat (
      text.get (), ::lumex::core::fmt::make_format_args (args...));
}

/** @brief `print` followed by a new line. */
template <typename... Args>
void
println (std::ostream &stream, FormatString<Args...> text, Args &&...args)
{
  stream << ::lumex::core::fmt::vformat (
      text.get (), ::lumex::core::fmt::make_format_args (args...))
         << '\n';
}

template <typename... Args>
void
println (std::wostream &stream, WFormatString<Args...> text, Args &&...args)
{
  stream << ::lumex::core::fmt::vformat (
      text.get (), ::lumex::core::fmt::make_wformat_args (args...))
         << L'\n';
}

template <typename... Args>
void
println (FormatString<Args...> text, Args &&...args)
{
  std::cout << ::lumex::core::fmt::vformat (
      text.get (), ::lumex::core::fmt::make_format_args (args...))
            << '\n';
}
} // namespace fmt
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_FMT_FORMAT_HPP
