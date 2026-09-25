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
 * @file LumexFormatRanges.hpp
 * @brief Ranges, `std::pair` and `std::tuple` for LumexFormat, as the C++23
 * range formatting of `std::format` (P2286 / P2585) defines them.
 *
 * @details
 * - A range whose `Range const &` can be walked (`std::begin` / `std::end`)
 *   prints as `[1, 2, 3]`; a set (it has `key_type`) as `{1, 2}`; a map (it
 *   has `key_type` and `mapped_type`) as `{"a": 1, "b": 2}`.
 * - `std::pair` / `std::tuple` print as `(1, "a")`.
 * - Strings and characters inside print with the debug presentation
 *   (`"a\tb"`, `'x'`) unless an element specification is given.
 * - Range specification: `[[fill]align][width][n][type][:element]`, type
 *   one of `m`, `s`, `?s`. `n` drops the brackets, `m` prints pairs as
 *   `k: v` inside `{}`, `s` and `?s` print a range of characters as a
 *   string / debug string, and the text after `:` is the specification of
 *   every element (for example `{::x}`).
 * - Tuple specification: `[[fill]align][width][n or m]`, where `m` prints
 *   a pair as `k: v`.
 *
 * Built-in arguments decay, so a C array argument formats as a pointer, not
 * as a range; pass `std::array` or a container instead. A range is walked as
 * `Range const &`, so views that cannot be iterated when const are not
 * supported.
 */
#ifndef LUMEX_CORE_FMT_FORMAT_RANGES_HPP
#define LUMEX_CORE_FMT_FORMAT_RANGES_HPP

#include <cstddef>
#include <iterator>
#include <locale>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "lumex/core/fmt/LumexFormat.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace fmt
{
namespace Detail
{
// ----------------------------------------------------------------------
// Traits
// ----------------------------------------------------------------------

/**
 * @brief Element type a range of `Range` is formatted as: the decayed
 * reference type, or `Range::value_type` when only that one has a
 * formatter (proxy references such as `std::vector<bool>`).
 */
template <typename Range, typename Char, typename Enable = void>
struct range_element
{
  using type =
      typename std::decay<typename lumex::core::utility::traits::range::
                              range_reference<Range>::type>::type;
};

template <typename Range, typename Char>
struct range_element<
    Range, Char,
    typename std::enable_if<
        !has_formatter<Char, typename std::decay<
                                 typename lumex::core::utility::traits::range::
                                     range_reference<Range>::type>::type> ()
        && has_formatter<Char, typename Range::value_type> ()
        && std::is_convertible<typename lumex::core::utility::traits::range::
                                   range_reference<Range>::type,
                               typename Range::value_type>::value>::type>
{
  using type = typename Range::value_type;
};

template <typename Range, typename Char, typename Enable = void>
struct is_formattable_range : std::false_type
{
};

template <typename Range, typename Char>
struct is_formattable_range<
    Range, Char,
    typename std::enable_if<
        lumex::core::utility::traits::range::is_iterable<Range>::value>::type>
    : std::integral_constant<
          bool, builtin_kind<Char, typename std::decay<Range>::type> ()
                        == ArgKind::none
                    && !lumex::core::utility::traits::string::is_any_string<
                        Range>::value
                    && !lumex::core::utility::traits::enums::is_reflected_enum<
                        Range>::value
                    && !std::is_same<typename range_element<Range, Char>::type,
                                     Range>::value
                    && has_formatter<
                        Char, typename range_element<Range, Char>::type> ()>
{
};

/** @brief All `Types` (without cv / reference) have a formatter for `Char`. */
template <typename Char, typename... Types> struct all_formattable;

template <typename Char> struct all_formattable<Char> : std::true_type
{
};

template <typename Char, typename First, typename... Rest>
struct all_formattable<Char, First, Rest...>
    : std::integral_constant<
          bool,
          has_formatter<
              Char, lumex::core::utility::traits::meta::CleanType<First>> ()
              && all_formattable<Char, Rest...>::value>
{
};

// ----------------------------------------------------------------------
// Parsing and writing helpers
// ----------------------------------------------------------------------

/**
 * @brief `[[fill]align][width]` of a range / tuple specification: `:` is
 * never a fill here (it starts the element specification), so `{::>3}`
 * right-aligns every element.
 */
template <typename Char>
Char const *
parse_range_fill_align_width (BasicFormatParseContext<Char> &ctx,
                              format_specs_t<Char> &specs)
{
  return parse_fill_align_width (ctx, specs, false);
}

/** @brief Strings and characters print quoted inside ranges and tuples. */
template <typename Char, typename T>
LUMEX_CONSTEXPR bool
uses_debug_by_default () LUMEX_NOEXCEPT
{
  return builtin_kind<Char, T> () == ArgKind::character
         || builtin_kind<Char, T> () == ArgKind::string
         || builtin_kind<Char, T> () == ArgKind::c_string;
}

/**
 * @brief Gives an element formatter its default specification: `?` for
 * strings / characters, empty otherwise.
 */
template <typename Char, typename T, typename ElementFormatter>
void
parse_default_element (ElementFormatter &formatter)
{
  static Char const debug_spec[]
      = { static_cast<Char> ('?'), static_cast<Char> ('}'), Char () };
  static Char const empty_spec[] = { static_cast<Char> ('}'), Char () };
  BasicFormatParseContext<Char> ctx (BasicStringRef<Char> (
      uses_debug_by_default<Char, T> () ? debug_spec : empty_spec));
  formatter.parse (ctx);
}

/** @brief Map elements print `key: value` without brackets. */
template <typename ElementFormatter>
auto
use_map_element_style (ElementFormatter &formatter, int)
    -> decltype (formatter.set_brackets (BasicStringRef<char> (""),
                                         BasicStringRef<char> ("")),
                 void ())
{
  formatter.set_brackets (BasicStringRef<char> (""),
                          BasicStringRef<char> (""));
  formatter.set_separator (BasicStringRef<char> (": "));
}

template <typename ElementFormatter>
auto
use_map_element_style (ElementFormatter &formatter, long)
    -> decltype (formatter.set_brackets (BasicStringRef<wchar_t> (L""),
                                         BasicStringRef<wchar_t> (L"")),
                 void ())
{
  formatter.set_brackets (BasicStringRef<wchar_t> (L""),
                          BasicStringRef<wchar_t> (L""));
  formatter.set_separator (BasicStringRef<wchar_t> (L": "));
}

/** @brief A user pair formatter without `set_brackets` keeps its style. */
template <typename ElementFormatter>
void
use_map_element_style (ElementFormatter &, ...)
{
}

/**
 * @brief Writes with `write_body (context)` directly, or into a temporary
 * text padded to the resolved width when a width is given.
 */
template <typename Char, typename WriteBody>
void
write_with_width (format_specs_t<Char> const &parsed,
                  BasicFormatContext<Char> &ctx, WriteBody const &write_body)
{
  format_specs_t<Char> specs = parsed;
  specs.width = resolve_dynamic (specs.width_ref, specs.width, ctx);
  if (specs.width == 0)
    {
      write_body (ctx);
      return;
    }
  std::basic_string<Char> text;
  StringBuffer<Char> buffer (text);
  std::locale const locale = ctx.locale ();
  BasicFormatContext<Char> inner (buffer, ctx.args (), &locale);
  write_body (inner);
  write_padded (ctx.out ().buffer (), specs, Align::left, text);
}

// ----------------------------------------------------------------------
// Tuple formatter
// ----------------------------------------------------------------------

template <std::size_t Index, std::size_t Count> struct tuple_each
{
  template <typename Char, typename Formatters>
  static void
  parse (Formatters &formatters)
  {
    typedef
        typename std::tuple_element<Index, Formatters>::type formatter_type;
    parse_default_element<Char, typename formatter_type::value_type> (
        std::get<Index> (formatters));
    tuple_each<Index + 1, Count>::template parse<Char> (formatters);
  }

  template <typename Char, typename Formatters, typename Tuple>
  static void
  format (Formatters const &formatters, Tuple const &value,
          std::basic_string<Char> const &separator,
          BasicFormatContext<Char> &ctx)
  {
    if (Index > 0)
      ctx.out ().buffer ().append (separator);
    std::get<Index> (formatters).format (std::get<Index> (value), ctx);
    tuple_each<Index + 1, Count>::format (formatters, value, separator, ctx);
  }
};

template <std::size_t Count> struct tuple_each<Count, Count>
{
  template <typename Char, typename Formatters>
  static void
  parse (Formatters &)
  {
  }

  template <typename Char, typename Formatters, typename Tuple>
  static void
  format (Formatters const &, Tuple const &, std::basic_string<Char> const &,
          BasicFormatContext<Char> &)
  {
  }
};

/** @brief `Formatter<T, Char>` that also names the formatted type. */
template <typename T, typename Char>
class ElementFormatter : public Formatter<T, Char>
{
public:
  using value_type = T;
};

/**
 * @class TupleFormatter
 * @brief Formats a `std::pair` / `std::tuple` of `Types` as `(a, b)`; base
 * of the public `Formatter` specializations.
 */
template <typename Char, typename... Types> class TupleFormatter
{
public:
  TupleFormatter ()
      : _specs (), _separator (widen<Char> (", ")), _open (widen<Char> ("(")),
        _close (widen<Char> (")"))
  {
  }

  /** @brief Text between elements (`", "` by default). */
  void
  set_separator (BasicStringRef<Char> separator)
  {
    _separator.assign (separator.begin (), separator.end ());
  }

  /** @brief Text around the elements (`"("` and `")"` by default). */
  void
  set_brackets (BasicStringRef<Char> open, BasicStringRef<Char> close)
  {
    _open.assign (open.begin (), open.end ());
    _close.assign (close.begin (), close.end ());
  }

  Char const *
  parse (BasicFormatParseContext<Char> &ctx)
  {
    _specs = format_specs_t<Char> (); // brackets set by the user stay
    Char const *it = parse_range_fill_align_width (ctx, _specs);
    Char const *const end = ctx.end ();
    if (it != end && *it == static_cast<Char> ('n'))
      {
        _open.clear ();
        _close.clear ();
        ++it;
      }
    else if (it != end && *it == static_cast<Char> ('m'))
      {
        if (sizeof...(Types) != 2)
          throw FormatError ("invalid format specifier");
        _open.clear ();
        _close.clear ();
        _separator = widen<Char> (": ");
        ++it;
      }
    if (it != end && *it != static_cast<Char> ('}'))
      throw FormatError ("invalid format specifier");
    tuple_each<0, sizeof...(Types)>::template parse<Char> (_formatters);
    return it;
  }

  template <typename Tuple>
  BasicAppender<Char>
  format (Tuple const &value, BasicFormatContext<Char> &ctx) const
  {
    TupleFormatter const &self = *this;
    write_with_width (_specs, ctx,
                      [&self, &value] (BasicFormatContext<Char> &out)
                        {
                          out.out ().buffer ().append (self._open);
                          tuple_each<0, sizeof...(Types)>::format (
                              self._formatters, value, self._separator, out);
                          out.out ().buffer ().append (self._close);
                        });
    return ctx.out ();
  }

private:
  format_specs_t<Char> _specs;
  std::basic_string<Char> _separator;
  std::basic_string<Char> _open;
  std::basic_string<Char> _close;
  std::tuple<ElementFormatter<
      lumex::core::utility::traits::meta::CleanType<Types>, Char>...>
      _formatters;
};
} // namespace Detail

// ----------------------------------------------------------------------
// Public formatters
// ----------------------------------------------------------------------

template <typename First, typename Second, typename Char>
class Formatter<std::pair<First, Second>, Char,
                typename std::enable_if<
                    Detail::all_formattable<Char, First, Second>::value>::type>
    : public Detail::TupleFormatter<Char, First, Second>
{
};

template <typename... Types, typename Char>
class Formatter<std::tuple<Types...>, Char,
                typename std::enable_if<
                    Detail::all_formattable<Char, Types...>::value>::type>
    : public Detail::TupleFormatter<Char, Types...>
{
};

/**
 * @brief Ranges: `[a, b]`, sets `{a, b}`, maps `{k: v}`; see the file
 * description for the specification.
 */
template <typename Range, typename Char>
class Formatter<Range, Char,
                typename std::enable_if<
                    Detail::is_formattable_range<Range, Char>::value>::type>
{
public:
  using element_type = typename Detail::range_element<Range, Char>::type;

  Formatter ()
      : _specs (), _presentation (default_presentation ()),
        _separator (Detail::widen<Char> (", ")), _open (), _close (),
        _underlying ()
  {
    set_default_brackets ();
  }

  /** @brief Text between elements (`", "` by default). */
  void
  set_separator (Detail::BasicStringRef<Char> separator)
  {
    _separator.assign (separator.begin (), separator.end ());
  }

  /** @brief Text around the elements (`[` `]`, or `{` `}` for sets / maps). */
  void
  set_brackets (Detail::BasicStringRef<Char> open,
                Detail::BasicStringRef<Char> close)
  {
    _open.assign (open.begin (), open.end ());
    _close.assign (close.begin (), close.end ());
  }

  /** @brief The formatter of one element, to customize it further. */
  Formatter<element_type, Char> &
  underlying () LUMEX_NOEXCEPT
  {
    return _underlying;
  }

  Char const *
  parse (BasicFormatParseContext<Char> &ctx)
  {
    _specs = Detail::format_specs_t<Char> (); // user brackets stay
    Char const *it = Detail::parse_range_fill_align_width (ctx, _specs);
    Char const *const end = ctx.end ();
    if (it != end && *it == static_cast<Char> ('n'))
      {
        _open.clear ();
        _close.clear ();
        ++it;
      }
    if (it != end && *it == static_cast<Char> ('m'))
      {
        if (!lumex::core::utility::traits::tuple::is_pair_like<
                element_type>::value)
          throw FormatError ("invalid format specifier");
        if (!_open.empty ())
          {
            _open = Detail::widen<Char> ("{");
            _close = Detail::widen<Char> ("}");
          }
        _presentation = presentation::map;
        ++it;
      }
    else if (it != end && *it == static_cast<Char> ('s'))
      {
        if (!std::is_same<element_type, Char>::value)
          throw FormatError ("invalid format specifier");
        _presentation = presentation::text;
        ++it;
      }
    else if (it != end && *it == static_cast<Char> ('?') && end - it > 1
             && it[1] == static_cast<Char> ('s'))
      {
        if (!std::is_same<element_type, Char>::value)
          throw FormatError ("invalid format specifier");
        _presentation = presentation::debug_text;
        it += 2;
      }
    if (it != end && *it == static_cast<Char> (':'))
      {
        if (_presentation == presentation::text
            || _presentation == presentation::debug_text)
          throw FormatError ("invalid format specifier");
        ctx.advance_to (it + 1);
        it = _underlying.parse (ctx);
      }
    else
      Detail::parse_default_element<Char, element_type> (_underlying);
    if (it != end && *it != static_cast<Char> ('}'))
      throw FormatError ("invalid format specifier");
    if (_presentation == presentation::map)
      Detail::use_map_element_style (_underlying, 0);
    return it;
  }

  BasicAppender<Char>
  format (Range const &range, BasicFormatContext<Char> &ctx) const
  {
    Formatter const &self = *this;
    Detail::write_with_width (_specs, ctx,
                              [&self, &range] (BasicFormatContext<Char> &out)
                                { self.write_body (range, out); });
    return ctx.out ();
  }

private:
  enum class presentation
  {
    sequence,
    set,
    map,
    text,
    debug_text
  };

  static LUMEX_CONSTEXPR presentation
  default_presentation () LUMEX_NOEXCEPT
  {
    return lumex::core::utility::traits::range::has_key_type<Range>::value
                   && lumex::core::utility::traits::range::has_mapped_type<
                       Range>::value
                   && lumex::core::utility::traits::tuple::is_pair_like<
                       element_type>::value
               ? presentation::map
           : lumex::core::utility::traits::range::has_key_type<Range>::value
               ? presentation::set
               : presentation::sequence;
  }

  void
  set_default_brackets ()
  {
    bool const braces = _presentation == presentation::map
                        || _presentation == presentation::set;
    _open = Detail::widen<Char> (braces ? "{" : "[");
    _close = Detail::widen<Char> (braces ? "}" : "]");
  }

  /** @brief `s` / `?s`: the characters as one (debug) string. */
  void
  write_text (Range const &range, Detail::Buffer<Char> &buffer,
              std::true_type) const
  {
    std::basic_string<Char> text;
    for (auto it = std::begin (range); it != std::end (range); ++it)
      text.push_back (*it);
    if (_presentation == presentation::debug_text)
      buffer.append (Detail::escape (text.data (), text.data () + text.size (),
                                     static_cast<Char> ('"')));
    else
      buffer.append (text);
  }

  /** @brief Unreachable: `parse` accepts `s` only for character ranges. */
  void
  write_text (Range const &, Detail::Buffer<Char> &, std::false_type) const
  {
  }

  void
  write_body (Range const &range, BasicFormatContext<Char> &ctx) const
  {
    Detail::Buffer<Char> &buffer = ctx.out ().buffer ();
    if (_presentation == presentation::text
        || _presentation == presentation::debug_text)
      {
        write_text (range, buffer, std::is_same<element_type, Char> ());
        return;
      }
    buffer.append (_open);
    bool first = true;
    for (auto it = std::begin (range); it != std::end (range); ++it)
      {
        if (!first)
          buffer.append (_separator);
        first = false;
        _underlying.format (*it, ctx);
      }
    buffer.append (_close);
  }

  Detail::format_specs_t<Char> _specs;
  presentation _presentation;
  std::basic_string<Char> _separator;
  std::basic_string<Char> _open;
  std::basic_string<Char> _close;
  Formatter<element_type, Char> _underlying;
};
} // namespace fmt
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_FMT_FORMAT_RANGES_HPP
