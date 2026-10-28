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
 * @file LumexProtocolFormat.hpp
 * @brief The text of decoded values: `describe_options_t`, `append_fields`,
 * `append_dump`, and the value formatters behind `describe`.
 * @details A decoded record is written as `name=value` pairs separated by
 * spaces, with the engineering value of a scaled field and its unit
 * (`speed=12.5 rpm`), the enumerator name of an enumeration, quoted text,
 * `true` or `false` for a flag, `[a, b]` for an array, `{x=1 y=2}` for a
 * layout or the parts of a split word, `case 7: {...}` for a union and
 * `absent` for a tail the message did not carry. The formatters walk the
 * field types, so every kind of field of the module has its own text; a
 * kind with no formatter would not compile.
 */
#ifndef LUMEX_CORE_PROTOCOL_INDEX_FORMAT_HPP
#define LUMEX_CORE_PROTOCOL_INDEX_FORMAT_HPP

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <type_traits>

#include "lumex/core/protocol/dsl/LumexProtocolRow.hpp"
#include "lumex/core/protocol/field/LumexProtocolBitFields.hpp"
#include "lumex/core/protocol/field/LumexProtocolCompositeFields.hpp"
#include "lumex/core/protocol/field/LumexProtocolFieldTraits.hpp"
#include "lumex/core/protocol/field/LumexProtocolRecord.hpp"
#include "lumex/core/protocol/field/LumexProtocolValues.hpp"
#include "lumex/core/protocol/registry/LumexProtocolRegistryFields.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#if __has_warning("-Wlifetime-safety-invalidation")
// Clang 23 reports a reference parameter to a container as "invalidated" by
// its own push_back, insert or reserve (a false positive: only references into
// the container are invalidated, not the container).
#pragma clang diagnostic ignored "-Wlifetime-safety-invalidation"
#endif
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#if __has_warning("-Wunsafe-buffer-usage-in-libc-call")
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#endif
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(push)
// snprintf and strtod are the standard functions; the secure variants are
// not portable.
#pragma warning(disable : 4996)
#endif

namespace lumex
{
namespace core
{
namespace protocol
{
namespace index
{
/// @brief What `describe` writes beyond the default.
struct describe_options_t
{
  /// Write the raw value of a scaled field next to its engineering value
  /// (`speed=12.5 rpm (raw 125)`).
  bool show_raw;
  /// The most bytes of a dump (an unknown or damaged frame); more are cut
  /// with `...`.
  std::size_t dump_limit;
  /// The most characters of a text value (`token_`, `text_<N>`); a longer one
  /// is cut and followed by `... (N chars)` with its full length.
  std::size_t text_limit;

  /// @brief The defaults: no raw values, dumps of up to 256 bytes, texts of
  /// up to 256 characters.
  describe_options_t () LUMEX_NOEXCEPT : show_raw (false),
                                         dump_limit (256),
                                         text_limit (256)
  {
  }
};

namespace detail
{
namespace field = ::lumex::core::protocol::field;
namespace dsl = ::lumex::core::protocol::dsl;
namespace registry = ::lumex::core::protocol::registry;

inline void
append_unsigned (std::string &out, std::uint64_t value)
{
  out += std::to_string (static_cast<unsigned long long> (value));
}

inline void
append_signed (std::string &out, std::int64_t value)
{
  out += std::to_string (static_cast<long long> (value));
}

// The decimal point is '.' whatever the locale.
inline void
fix_point (std::string &text)
{
  for (std::size_t i = 0; i < text.size (); ++i)
    if (text[i] == ',')
      text[i] = '.';
}

inline void
trim_zeros (std::string &text)
{
  if (text.find ('.') == std::string::npos)
    return;
  while (!text.empty () && text[text.size () - 1u] == '0')
    text.erase (text.size () - 1u);
  if (!text.empty () && text[text.size () - 1u] == '.')
    text.erase (text.size () - 1u);
  if (text == "-0")
    text = "0";
}

// A number with the digits its scale needs: 1/100 gives two decimals.
inline void
append_scaled (std::string &out, double value, std::intmax_t num,
               std::intmax_t den)
{
  int decimals = 0;
  // Scales with a huge numerator are written without decimals (the search
  // below multiplies by powers of ten).
  if (num != 0 && den > 0 && num < 1000000000)
    {
      std::intmax_t factor = 1;
      while (decimals < 9 && (num * factor) % den != 0)
        {
          factor *= 10;
          ++decimals;
        }
      if ((num * factor) % den != 0)
        decimals = 6;
    }
  char buffer[64];
  std::snprintf (buffer, sizeof (buffer), "%.*f", decimals, value);
  std::string text = buffer;
  fix_point (text);
  trim_zeros (text);
  out += text;
}

// The shortest %g text that reads back as the same number.
inline void
append_floating (std::string &out, double value, bool single)
{
  if (value != value)
    {
      out += "nan";
      return;
    }
  if (std::isinf (value))
    {
      out += value < 0 ? "-inf" : "inf";
      return;
    }
  char buffer[64];
  std::snprintf (buffer, sizeof (buffer), "%.*g", single ? 9 : 15, value);
  if (!single && std::strtod (buffer, nullptr) != value)
    std::snprintf (buffer, sizeof (buffer), "%.17g", value);
  std::string text = buffer;
  fix_point (text);
  out += text;
}

inline void
append_hex_byte (std::string &out, unsigned byte)
{
  static char const digits[] = "0123456789ABCDEF";
  out += digits[(byte >> 4) & 0xFu];
  out += digits[byte & 0xFu];
}

inline void
append_escaped (std::string &out, char c)
{
  unsigned const byte = static_cast<unsigned char> (c);
  if (c == '"' || c == '\\' || c == '\'')
    {
      out += '\\';
      out += c;
    }
  else if (byte >= 0x20 && byte < 0x7F)
    out += c;
  else
    {
      out += "\\x";
      append_hex_byte (out, byte);
    }
}

inline void
append_quoted (std::string &out, char const *text, std::size_t size,
               std::size_t limit)
{
  std::size_t const shown = size < limit ? size : limit;
  out += '"';
  for (std::size_t i = 0; i < shown; ++i)
    append_escaped (out, text[i]);
  out += '"';
  if (shown < size)
    {
      out += "... (";
      append_unsigned (out, size);
      out += " chars)";
    }
}

inline void
append_unit (std::string &out, char const *unit)
{
  if (unit != nullptr && unit[0] != '\0')
    {
      out += ' ';
      out += unit;
    }
}

// How one decoded value is written (the leaves of the field tree).
template <typename V, typename = void> struct leaf_value
{
  static void
  apply (std::string &out, V const &, describe_options_t const &)
  {
    out += "?";
  }
};

template <typename V>
struct leaf_value<
    V, typename std::enable_if<
           std::is_integral<V>::value && !std::is_same<V, bool>::value
           && !std::is_same<V, char>::value && std::is_signed<V>::value>::type>
{
  static void
  apply (std::string &out, V const &value, describe_options_t const &)
  {
    append_signed (out, static_cast<std::int64_t> (value));
  }
};

template <typename V>
struct leaf_value<
    V,
    typename std::enable_if<
        std::is_integral<V>::value && !std::is_same<V, bool>::value
        && !std::is_same<V, char>::value && !std::is_signed<V>::value>::type>
{
  static void
  apply (std::string &out, V const &value, describe_options_t const &)
  {
    append_unsigned (out, static_cast<std::uint64_t> (value));
  }
};

template <> struct leaf_value<bool, void>
{
  static void
  apply (std::string &out, bool value, describe_options_t const &)
  {
    out += value ? "true" : "false";
  }
};

template <> struct leaf_value<char, void>
{
  static void
  apply (std::string &out, char value, describe_options_t const &)
  {
    out += '\'';
    append_escaped (out, value);
    out += '\'';
  }
};

template <typename V>
struct leaf_value<
    V, typename std::enable_if<std::is_floating_point<V>::value>::type>
{
  static void
  apply (std::string &out, V value, describe_options_t const &)
  {
    append_floating (out, static_cast<double> (value),
                     std::is_same<V, float>::value);
  }
};

template <typename V>
struct leaf_value<V, typename std::enable_if<std::is_enum<V>::value>::type>
{
  static void
  apply (std::string &out, V value, describe_options_t const &)
  {
    char const *const name = field::enum_name (value);
    if (name != nullptr)
      out += name;
    else
      leaf_value<typename std::underlying_type<V>::type>::apply (
          out, static_cast<typename std::underlying_type<V>::type> (value),
          describe_options_t ());
  }
};

template <typename Raw, typename Ratio>
struct leaf_value<field::fixed_point<Raw, Ratio>, void>
{
  static void
  apply (std::string &out, field::fixed_point<Raw, Ratio> const &value,
         describe_options_t const &)
  {
    append_scaled (out, value.value (), Ratio::num, Ratio::den);
  }
};

template <> struct leaf_value<field::text_view, void>
{
  static void
  apply (std::string &out, field::text_view const &value,
         describe_options_t const &options)
  {
    append_quoted (out, value.data (), value.size (), options.text_limit);
  }
};

template <std::size_t N> struct leaf_value<field::fixed_string<N>, void>
{
  static void
  apply (std::string &out, field::fixed_string<N> const &value,
         describe_options_t const &options)
  {
    append_quoted (out, value.chars, value.length, options.text_limit);
  }
};

template <> struct leaf_value<registry::parameter_value_t, void>
{
  static void
  apply (std::string &out, registry::parameter_value_t const &value,
         describe_options_t const &options)
  {
    if (value.entry != nullptr)
      {
        // An enumeration entry: the enumerator, as for an enum_ field.
        char const *const name
            = value.has_raw && value.entry->value_name != nullptr
                  ? value.entry->value_name (value.raw)
                  : nullptr;
        if (name != nullptr)
          out += name;
        else
          {
            append_scaled (out, value.engineering, value.entry->type.scale_num,
                           value.entry->type.scale_den);
            append_unit (out, value.entry->type.unit);
          }
        if (options.show_raw)
          {
            out += " (raw ";
            append_signed (out, value.raw);
            out += ')';
          }
      }
    else if (value.has_raw)
      {
        out += "raw ";
        append_signed (out, value.raw);
      }
    else
      append_floating (out, value.engineering, false);
  }
};

// The raw value next to the engineering value, when asked for.
template <typename V> struct raw_text
{
  static void
  apply (std::string &, V const &, describe_options_t const &)
  {
  }
};

template <typename Raw, typename Ratio>
struct raw_text<field::fixed_point<Raw, Ratio>>
{
  static void
  apply (std::string &out, field::fixed_point<Raw, Ratio> const &value,
         describe_options_t const &options)
  {
    if (options.show_raw)
      {
        out += " (raw ";
        leaf_value<Raw>::apply (out, value.raw, options);
        out += ')';
      }
  }
};

template <typename F, typename = void> struct value_formatter;

// The fields F... of a record, written as name=value pairs.
struct record_writer
{
  std::string &out;
  dsl::names_t names;
  describe_options_t const &options;
  bool first;

  template <typename F, typename V>
  void
  operator() (std::size_t position, field::field_type_tag<F>, V const &value)
  {
    if (!first)
      out += ' ';
    first = false;
    char const *const name
        = position < names.size () ? names.data ()[position] : "";
    if (name[0] != '\0')
      {
        out += name;
        out += '=';
      }
    value_formatter<typename field::resolve<F>::type>::apply (out, value,
                                                              options);
  }
};

template <typename... F>
void
append_record (std::string &out, field::record<F...> const &record,
               dsl::names_t const &names, describe_options_t const &options)
{
  record_writer writer = { out, names, options, true };
  field::visit_fields (record, writer);
}

// A leaf: the value, its raw value when asked for, then the unit.
template <typename F, typename> struct value_formatter
{
  typedef typename field::impl_of<F>::type impl;

  static void
  apply (std::string &out, typename impl::value_type const &value,
         describe_options_t const &options)
  {
    leaf_value<typename impl::value_type>::apply (out, value, options);
    append_unit (out, impl::info ().unit);
    raw_text<typename impl::value_type>::apply (out, value, options);
  }
};

// A group of fields: a layout (named) or a field list.
template <typename F, typename Enable = void> struct group_names
{
  static dsl::names_t
  get () LUMEX_NOEXCEPT
  {
    return dsl::names_t ();
  }
};

template <typename F>
struct group_names<F, typename std::enable_if<dsl::is_layout<F>::value>::type>
{
  static dsl::names_t
  get () LUMEX_NOEXCEPT
  {
    return F::field_names ();
  }
};

template <typename F>
struct value_formatter<
    F, typename std::enable_if<field::detail::has_fields<F>::value>::type>
{
  typedef typename field::impl_of<F>::type impl;

  static void
  apply (std::string &out, typename impl::value_type const &value,
         describe_options_t const &options)
  {
    out += '{';
    append_record (out, value, group_names<F>::get (), options);
    out += '}';
  }
};

template <typename T, typename Array>
void
append_elements (std::string &out, Array const &items, std::size_t count,
                 describe_options_t const &options)
{
  out += '[';
  for (std::size_t i = 0; i < count; ++i)
    {
      if (i != 0)
        out += ", ";
      value_formatter<typename field::resolve<T>::type>::apply (out, items[i],
                                                                options);
    }
  out += ']';
}

template <typename T, std::size_t N>
struct value_formatter<field::array_<T, N>, void>
{
  typedef typename field::impl_of<field::array_<T, N>>::type impl;

  static void
  apply (std::string &out, typename impl::value_type const &value,
         describe_options_t const &options)
  {
    append_elements<T> (out, value, N, options);
  }
};

template <typename T, typename C, std::size_t Max>
struct value_formatter<field::counted_<T, C, Max>, void>
{
  typedef typename field::impl_of<field::counted_<T, C, Max>>::type impl;

  static void
  apply (std::string &out, typename impl::value_type const &value,
         describe_options_t const &options)
  {
    append_elements<T> (out, value.items, value.count, options);
  }
};

template <typename T, typename E, std::size_t Max>
struct value_formatter<field::computed_count_<T, E, Max>, void>
{
  typedef
      typename field::impl_of<field::computed_count_<T, E, Max>>::type impl;

  static void
  apply (std::string &out, typename impl::value_type const &value,
         describe_options_t const &options)
  {
    append_elements<T> (out, value.items, value.count, options);
  }
};

template <typename... F>
struct value_formatter<field::optional_tail_<F...>, void>
{
  typedef typename field::impl_of<field::optional_tail_<F...>>::type impl;

  static void
  apply (std::string &out, typename impl::value_type const &value,
         describe_options_t const &options)
  {
    if (!value.present)
      {
        out += "absent";
        return;
      }
    out += '{';
    append_record (out, value.fields, dsl::names_t (), options);
    out += '}';
  }
};

template <std::size_t Min, typename A, typename B>
struct value_formatter<field::by_length_<Min, A, B>, void>
{
  typedef typename field::impl_of<field::by_length_<Min, A, B>>::type impl;

  static void
  apply (std::string &out, typename impl::value_type const &value,
         describe_options_t const &options)
  {
    if (value.index () == 0)
      value_formatter<typename field::resolve<A>::type>::apply (
          out, value.template get<0> (), options);
    else
      value_formatter<typename field::resolve<B>::type>::apply (
          out, value.template get<1> (), options);
  }
};

// The case of a union that the alternatives hold.
template <std::size_t K, typename... Cases> struct case_formatter;

template <std::size_t K> struct case_formatter<K>
{
  template <typename Alternatives>
  static void
  apply (std::string &out, Alternatives const &, std::size_t,
         describe_options_t const &)
  {
    out += "?";
  }
};

template <std::size_t K, typename Head, typename... Tail>
struct case_formatter<K, Head, Tail...>
{
  template <typename Alternatives>
  static void
  apply (std::string &out, Alternatives const &alternatives,
         std::size_t chosen, describe_options_t const &options)
  {
    if (chosen == K)
      value_formatter<typename field::resolve<typename Head::layout_type>::
                          type>::apply (out, alternatives.template get<K> (),
                                        options);
    else
      case_formatter<K + 1, Tail...>::apply (out, alternatives, chosen,
                                             options);
  }
};

template <typename Union, typename... Cases>
void
append_union (std::string &out, Union const &value,
              describe_options_t const &options)
{
  out += "case ";
  append_signed (out, value.discriminant);
  out += ": ";
  case_formatter<0, Cases...>::apply (out, value.cases, value.cases.index (),
                                      options);
}

template <typename D, typename... Cases>
struct value_formatter<field::union_by_<D, Cases...>, void>
{
  typedef typename field::impl_of<field::union_by_<D, Cases...>>::type impl;

  static void
  apply (std::string &out, typename impl::value_type const &value,
         describe_options_t const &options)
  {
    append_union<typename impl::value_type, Cases...> (out, value, options);
  }
};

template <typename K, typename... Cases>
struct value_formatter<field::union_by_context_<K, Cases...>, void>
{
  typedef typename field::impl_of<field::union_by_context_<K, Cases...>>::type
      impl;

  static void
  apply (std::string &out, typename impl::value_type const &value,
         describe_options_t const &options)
  {
    append_union<typename impl::value_type, Cases...> (out, value, options);
  }
};

// The names of the parts of a split word.
template <typename... Parts> struct split_part_names
{
  static dsl::names_t
  get () LUMEX_NOEXCEPT
  {
    return dsl::names_t ();
  }
};

template <typename Part> struct split_part_names<Part>
{
  static dsl::names_t
  get () LUMEX_NOEXCEPT
  {
    return group_names<typename field::resolve<Part>::type>::get ();
  }
};

template <typename Word, typename... Parts>
struct value_formatter<field::split_<Word, Parts...>, void>
{
  typedef typename field::impl_of<field::split_<Word, Parts...>>::type impl;

  static void
  apply (std::string &out, typename impl::value_type const &value,
         describe_options_t const &options)
  {
    out += '{';
    append_record (out, value.parts, split_part_names<Parts...>::get (),
                   options);
    out += '}';
  }
};

template <typename P, typename K>
struct value_formatter<field::nested_<P, K>, void>
{
  typedef typename field::impl_of<field::nested_<P, K>>::type impl;

  static void
  apply (std::string &out, typename impl::value_type const &value,
         describe_options_t const &options)
  {
    out += "{key=";
    value_formatter<typename field::resolve<K>::type>::apply (out, value.key,
                                                              options);
    out += " len=";
    append_unsigned (out, value.payload.size ());
    out += '}';
  }
};

template <typename R, typename F1, typename F2>
struct value_formatter<registry::registry_key_<R, F1, F2>, void>
{
  typedef typename R::id value_type;

  static void
  apply (std::string &out, value_type const &value, describe_options_t const &)
  {
    registry::entries_t const entries = R::entries ();
    if (static_cast<std::size_t> (value) < entries.size ())
      out += entries.data ()[value].identifier;
    else
      out += "?";
  }
};
} // namespace detail

/// @brief Appends the hex dump of `data` (`15 0A FF`), cut after `limit`
/// bytes with `...`.
inline void
append_dump (std::string &out, std::uint8_t const *data, std::size_t size,
             std::size_t limit)
{
  std::size_t const shown = size < limit ? size : limit;
  for (std::size_t i = 0; i < shown; ++i)
    {
      if (i != 0)
        out += ' ';
      detail::append_hex_byte (out, data[i]);
    }
  if (shown < size)
    out += shown == 0 ? "..." : " ...";
}

/// @brief Appends `0x` and `digits` hexadecimal digits of `value`.
inline void
append_hex (std::string &out, std::uint64_t value, std::size_t digits)
{
  static char const characters[] = "0123456789ABCDEF";
  out += "0x";
  for (std::size_t i = digits; i > 0; --i)
    out += characters[(value >> (4u * (i - 1u))) & 0xFu];
}
} // namespace index
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(pop)
#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_INDEX_FORMAT_HPP
