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
 * @file LumexProtocolSample.hpp
 * @brief `sample_value<F> ()`: the smallest valid value of a field, a layout
 * or the field group of a row, one that encodes.
 * @details A value-initialized value does not always encode: a reflected
 * enumeration may have no enumerator 0, a union holds its first case only
 * when its discriminant says so, a computed array must have the number of
 * elements its expression gives, a registry value must name an entry the
 * command may access and stay inside its limits. The sample is, by kind:
 *  - a number, a text, a character, a token: value-initialized (0, empty);
 *  - an enumeration (`enum_`, a `bits_` or `mask_` part, `split_` parts):
 *    the enumerator nearest to 0 that the field accepts (0, 1, -1, 2, -2,
 *    ... up to 65535; a reflected enumeration accepts only its enumerators,
 *    a part only what fits its bits), value-initialized when none is found;
 *  - `array_`: every element sampled; `counted_`: no element;
 *    `computed_count_`: as many sampled elements as its expression gives for
 *    the sampled fields before it (none when that is not a valid count);
 *  - `optional_tail_`: absent; `by_length_`: the short form, sampled;
 *  - `union_by_`, `union_by_context_`: the first case, sampled, with its
 *    discriminant; with `field_<I>` field `I` of the group is set to it;
 *  - `registry_key_`: the first entry; with a `registry_value_` of the same
 *    group, the first entry from there on that allows the access of the
 *    value, and the raw value 0, or the lower limit, or the upper limit,
 *    whichever the entry and the carrier accept;
 *  - `nested_`: the sampled key and no payload; a group (a layout, the
 *    fields of a row): every field sampled, then the fields that depend on
 *    earlier ones adjusted.
 * A union chosen by a session value decodes only with a session that knows
 * its first case (the encoder checks the session when it is given one).
 * The short form of `by_length_` is shorter than `Min` in a table whose
 * short form can be decoded at all.
 */
#ifndef LUMEX_CORE_PROTOCOL_FIELD_SAMPLE_HPP
#define LUMEX_CORE_PROTOCOL_FIELD_SAMPLE_HPP

#include <cstddef>
#include <cstdint>
#include <limits>
#include <system_error>
#include <type_traits>

#include "lumex/core/protocol/field/LumexProtocolBitFields.hpp"
#include "lumex/core/protocol/field/LumexProtocolCompositeFields.hpp"
#include "lumex/core/protocol/field/LumexProtocolFieldTraits.hpp"
#include "lumex/core/protocol/field/LumexProtocolNumeric.hpp"
#include "lumex/core/protocol/field/LumexProtocolRecord.hpp"
#include "lumex/core/protocol/field/LumexProtocolValues.hpp"
#include "lumex/core/protocol/registry/LumexProtocolRegistry.hpp"
#include "lumex/core/protocol/registry/LumexProtocolRegistryFields.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/sequence/LumexIndexSequence.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#endif

namespace lumex
{
namespace core
{
namespace protocol
{
namespace field
{
namespace detail
{
/// The farthest distance from 0 the sample of an enumeration looks at.
LUMEX_CONSTEXPR std::int64_t sample_enum_reach = 65535;

// Whether `value` is a value of U.
template <typename U>
bool
fits_underlying (std::int64_t value) LUMEX_NOEXCEPT
{
  if (std::is_signed<U>::value)
    return value
               >= static_cast<std::int64_t> ((std::numeric_limits<U>::min) ())
           && value <= static_cast<std::int64_t> (
                  (std::numeric_limits<U>::max) ());
  return value >= 0
         && static_cast<std::uint64_t> (value) <= static_cast<std::uint64_t> (
                (std::numeric_limits<U>::max) ());
}

// The enumerator of E nearest to 0 that field implementation Impl accepts.
template <typename Impl, typename E>
E
first_accepted () LUMEX_NOEXCEPT
{
  using underlying = typename std::underlying_type<E>::type;
  for (std::int64_t distance = 0; distance <= sample_enum_reach; ++distance)
    {
      std::int64_t const candidates[2] = { distance, -distance };
      for (std::size_t side = 0; side < (distance == 0 ? 1u : 2u); ++side)
        {
          if (!fits_underlying<underlying> (candidates[side]))
            continue;
          E const candidate
              = static_cast<E> (static_cast<underlying> (candidates[side]));
          typename Impl::value_type probe = typename Impl::value_type ();
          if (Impl::assign (probe, candidate))
            return candidate;
        }
    }
  return E ();
}

template <typename Impl, typename = void> struct sampler;

template <typename Seq, typename... F> struct record_sampler;

// Samples every field of a record, then lets the fields that depend on
// earlier ones adjust the record (twice: an adjustment may change a field an
// earlier one read).
template <std::size_t... I, typename... F>
struct record_sampler<::lumex::core::utility::sequence::index_sequence<I...>,
                      F...>
{
  template <typename Record>
  static void
  fill (Record &record)
  {
    int const filled[]
        = { 0, (sampler<typename impl_of<F>::type>::fill (get<I> (record)),
                0)... };
    LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (filled);
    for (int pass = 0; pass < 2; ++pass)
      {
        int const fixed[]
            = { 0,
                (sampler<typename impl_of<F>::type>::template fix<I> (record),
                 0)... };
        LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (fixed);
      }
  }
};

template <typename... F>
void
sample_record (record<F...> &value)
{
  record_sampler<
      ::lumex::core::utility::sequence::make_index_sequence<sizeof...(F)>,
      F...>::fill (value);
}

// A field that needs no adjustment.
struct no_fix
{
  template <std::size_t I, typename Record>
  static void
  fix (Record &) LUMEX_NOEXCEPT
  {
  }
};

// Any other field: value-initialized; an enumeration: the first accepted
// enumerator.
template <typename Impl, typename> struct sampler : no_fix
{
  using value_type = typename Impl::value_type;

  static void
  fill (value_type &value)
  {
    fill_value (value, std::is_enum<value_type> ());
  }

private:
  static void
  fill_value (value_type &value, std::false_type)
  {
    value = value_type ();
  }

  static void
  fill_value (value_type &value, std::true_type)
  {
    static value_type const first = first_accepted<Impl, value_type> ();
    value = first;
  }
};

template <template <typename...> class List, typename... F>
struct sampler<group_impl<List<F...>>> : no_fix
{
  static void
  fill (record<F...> &value)
  {
    sample_record (value);
  }
};

template <typename T, std::size_t N> struct sampler<array_<T, N>> : no_fix
{
  static void
  fill (typename array_<T, N>::value_type &value)
  {
    for (std::size_t i = 0; i < N; ++i)
      sampler<typename array_<T, N>::element_impl>::fill (value[i]);
  }
};

template <typename T, typename Expr, std::size_t Max>
struct sampler<computed_count_<T, Expr, Max>>
{
  using impl = computed_count_<T, Expr, Max>;
  using element_impl = typename impl::elements::element_impl;

  static void
  fill (typename impl::value_type &value)
  {
    value = typename impl::value_type ();
  }

  // As many elements as the expression gives for the fields before.
  template <std::size_t I, typename Record>
  static void
  fix (Record &record)
  {
    std::size_t const count = Expr::count (record);
    typename impl::value_type &value = get<I> (record);
    if (count == variable_size || count > Max)
      return;
    for (std::size_t i = value.count; i < count; ++i)
      sampler<element_impl>::fill (value.items[i]);
    value.count = count;
  }
};

template <std::size_t Min, typename A, typename B>
struct sampler<by_length_<Min, A, B>> : no_fix
{
  static void
  fill (typename by_length_<Min, A, B>::value_type &value)
  {
    sampler<typename by_length_<Min, A, B>::short_impl>::fill (
        value.template emplace<0> ());
  }
};

template <typename Head, typename... Tail> struct first_case
{
  using type = Head;
};

// The first case of a union and its discriminant.
template <typename Value, typename... Cases>
void
fill_first_case (Value &value)
{
  using head = typename first_case<Cases...>::type;
  sampler<typename impl_of<typename head::layout_type>::type>::fill (
      value.cases.template emplace<0> ());
  value.discriminant = head::value ();
}

// Where the discriminant of a union comes from: nothing to adjust, unless
// it is field J of the group, which gets the discriminant of the first case.
template <typename D> struct union_fix
{
  template <std::int64_t Value, typename Record>
  static void
  apply (Record &) LUMEX_NOEXCEPT
  {
  }
};

template <std::size_t J> struct union_fix<field_<J>>
{
  template <std::int64_t Value, typename Record>
  static void
  apply (Record &record)
  {
    from_discriminant (get<J> (record), Value);
  }
};

template <typename Discriminant, typename... Cases>
struct sampler<union_by_<Discriminant, Cases...>>
{
  static void
  fill (typename union_by_<Discriminant, Cases...>::value_type &value)
  {
    fill_first_case<typename union_by_<Discriminant, Cases...>::value_type,
                    Cases...> (value);
  }

  template <std::size_t I, typename Record>
  static void
  fix (Record &record)
  {
    union_fix<Discriminant>::template apply<
        first_case<Cases...>::type::value ()> (record);
  }
};

template <typename Key, typename... Cases>
struct sampler<union_by_context_<Key, Cases...>> : no_fix
{
  static void
  fill (typename union_by_context_<Key, Cases...>::value_type &value)
  {
    fill_first_case<typename union_by_context_<Key, Cases...>::value_type,
                    Cases...> (value);
  }
};

template <typename Word, typename... P>
struct sampler<split_impl<Word, part_list<P...>>> : no_fix
{
  static void
  fill (typename split_impl<Word, part_list<P...>>::value_type &value)
  {
    value.raw = typename split_impl<Word, part_list<P...>>::raw_type ();
    sample_record (value.parts);
  }
};

// Whether the raw value `raw` of `entry` is a value the carrier Raw takes and
// the entry allows.
template <typename RawImpl>
bool
raw_accepted (::lumex::core::protocol::registry::registry_entry_t const &entry,
              std::int64_t raw)
{
  typename RawImpl::value_type carrier = typename RawImpl::value_type ();
  return ::lumex::core::protocol::registry::raw_in_range (entry, raw)
         && !::lumex::core::protocol::registry::check_limits (
             entry,
             ::lumex::core::protocol::registry::to_engineering (entry, raw))
         && RawImpl::assign (carrier, raw);
}

template <typename R, std::size_t KeyIndex, typename RawField,
          ::lumex::core::protocol::registry::access_t Needs>
struct sampler<::lumex::core::protocol::registry::registry_value_<
    R, KeyIndex, RawField, Needs>>
{
  using impl
      = ::lumex::core::protocol::registry::registry_value_<R, KeyIndex,
                                                           RawField, Needs>;

  static void
  fill (::lumex::core::protocol::registry::parameter_value_t &value)
  {
    value = ::lumex::core::protocol::registry::parameter_value_t::of_raw (0);
  }

  // The first entry from the one the key names that allows the access and a
  // raw value: the key and the value are set to it.
  template <std::size_t I, typename Record>
  static void
  fix (Record &record)
  {
    namespace registry = ::lumex::core::protocol::registry;
    registry::entries_t const entries = R::entries ();
    using key_type =
        typename std::decay<decltype (get<KeyIndex> (record))>::type;
    std::size_t const start
        = static_cast<std::size_t> (get<KeyIndex> (record));
    for (std::size_t n = 0; n < entries.size (); ++n)
      {
        std::size_t const at = (start + n) % entries.size ();
        registry::registry_entry_t const &entry = entries.data ()[at];
        if (registry::check_access (entry, Needs))
          continue;
        std::int64_t raw = 0;
        if (!raw_accepted<typename impl::raw_impl> (entry, raw)
            && !(entry.has_limits && registry::to_raw (entry, entry.lower, raw)
                 && raw_accepted<typename impl::raw_impl> (entry, raw))
            && !(entry.has_limits && registry::to_raw (entry, entry.upper, raw)
                 && raw_accepted<typename impl::raw_impl> (entry, raw)))
          continue;
        get<KeyIndex> (record) = static_cast<key_type> (at);
        registry::parameter_value_t &value = get<I> (record);
        value = registry::parameter_value_t::of_raw (raw);
        value.engineering = registry::to_engineering (entry, raw);
        value.entry = &entry;
        return;
      }
  }
};
} // namespace detail

/**
 * @brief The smallest valid value of field `F` (a field, a layout, the
 * fields of a row: `Row::fields_type`, `Row::reply_type`); see the file
 * description.
 */
template <typename F>
typename value_of<F>::type
sample_value ()
{
  using impl = typename impl_of<F>::type;
  typename impl::value_type value = typename impl::value_type ();
  detail::sampler<impl>::fill (value);
  return value;
}
} // namespace field
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_FIELD_SAMPLE_HPP
