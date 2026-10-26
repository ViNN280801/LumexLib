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
 * @file LumexProtocolDescriptor.hpp
 * @brief The run-time descriptor of a field: `field_descriptor_t`
 * (name, `field_info_t`, layout name, children, role) and
 * `make_descriptor<F> (name)`.
 * @details The descriptors are what the index lists for a row and what the
 * exports print: a name, the kind, the width, the scale and the unit of every
 * field, and for a field made of others (a layout, an array, a tail, a
 * union, a split word) the descriptors of its parts. They are built from the
 * field types by templates and live in static arrays, one per field type, so
 * a row costs nothing until its descriptors are asked for.
 */
#ifndef LUMEX_CORE_PROTOCOL_INDEX_DESCRIPTOR_HPP
#define LUMEX_CORE_PROTOCOL_INDEX_DESCRIPTOR_HPP

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "lumex/core/protocol/dsl/LumexProtocolRow.hpp"
#include "lumex/core/protocol/field/LumexProtocolBitFields.hpp"
#include "lumex/core/protocol/field/LumexProtocolCompositeFields.hpp"
#include "lumex/core/protocol/field/LumexProtocolFieldTraits.hpp"
#include "lumex/core/protocol/field/LumexProtocolIntegerFields.hpp"
#include "lumex/core/protocol/registry/LumexProtocolRegistryFields.hpp"
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
namespace index
{
/// @brief What a descriptor is to the descriptor that holds it.
enum class descriptor_role : std::uint8_t
{
  /// A field of a row, a layout or a split word.
  none,
  /// The element of an array.
  element,
  /// A case of a union (`value` is the discriminant).
  union_case,
  /// The form of a `by_length_` field for a short message.
  short_form,
  /// The form of a `by_length_` field for a long message.
  long_form,
  /// A field of an `optional_tail_`.
  tail,
  /// A part of a split word.
  part,
  /// The key field of a nested message.
  key
};

/**
 * @brief The descriptor of one field.
 * @details `children` points to `child_count` descriptors: the fields of a
 * layout, the element of an array, the fields of a tail, the cases of a
 * union, the two forms of a length-chosen field, the parts of a split word,
 * the key of a nested message.
 */
struct field_descriptor_t
{
  /// The name in its group, `""` when the field has none.
  char const *name;
  /// What the field is: kind, width, order, scale, unit, count.
  ::lumex::core::protocol::field::field_info_t info;
  /// The name of the layout when the field is one, otherwise `""`.
  char const *layout_name;
  /// The name of the registry for the key and value of a registry entry,
  /// otherwise `""`.
  char const *registry_name;
  /// The descriptors of the parts, `nullptr` without parts.
  field_descriptor_t const *children;
  /// The number of parts.
  std::size_t child_count;
  /// What the field is to its parent.
  descriptor_role role;
  /// The discriminant of a `union_case`, otherwise 0.
  std::int64_t value;
};

/// @brief A run of descriptors.
struct descriptor_run_t
{
  /// The first descriptor.
  field_descriptor_t const *items;
  /// How many.
  std::size_t count;
};

namespace detail
{
namespace field = ::lumex::core::protocol::field;
namespace dsl = ::lumex::core::protocol::dsl;

template <typename F, typename = void> struct describer;

inline field_descriptor_t
make_node (char const *name, descriptor_role role, std::int64_t value,
           field::field_info_t info, char const *layout_name,
           char const *registry_name, descriptor_run_t run)
{
  field_descriptor_t const node
      = { name,      info,      layout_name, registry_name,
          run.items, run.count, role,        value };
  return node;
}

inline char const *
name_at (dsl::names_t const &names, std::size_t position) LUMEX_NOEXCEPT
{
  return position < names.size () ? names.data ()[position] : "";
}

// The names of an owner (a row, a layout); void: none.
template <typename Owner> struct owner_names
{
  static dsl::names_t
  get () LUMEX_NOEXCEPT
  {
    return Owner::field_names ();
  }
};

template <> struct owner_names<void>
{
  static dsl::names_t
  get () LUMEX_NOEXCEPT
  {
    return dsl::names_t ();
  }
};

template <typename F>
field_descriptor_t make_descriptor_impl (char const *name,
                                         descriptor_role role,
                                         std::int64_t value);

// The descriptors of the fields F... named by Owner (void: no names), each
// with `Role`: one static array per owner and field list.
template <typename Owner, descriptor_role Role, typename Indices,
          typename... F>
struct run_of;

template <typename Owner, descriptor_role Role, std::size_t... I,
          typename... F>
struct run_of<Owner, Role,
              ::lumex::core::utility::sequence::index_sequence<I...>, F...>
{
  static descriptor_run_t
  get ()
  {
    // One element more than fields keeps an empty run a valid array.
    static field_descriptor_t const items[sizeof...(F) + 1]
        = { make_descriptor_impl<F> (name_at (owner_names<Owner>::get (), I),
                                     Role, 0)...,
            field_descriptor_t () };
    descriptor_run_t const run = { items, sizeof...(F) };
    return run;
  }
};

// The descriptors of the fields of an owner that has `fields_type` and
// `field_names ()` (a row, a layout).
template <descriptor_role Role, typename Owner, typename List>
struct owned_run;

template <descriptor_role Role, typename Owner, template <typename...> class L,
          typename... F>
struct owned_run<Role, Owner, L<F...>>
{
  static descriptor_run_t
  get ()
  {
    return run_of<
        Owner, Role,
        ::lumex::core::utility::sequence::make_index_sequence<sizeof...(F)>,
        F...>::get ();
  }
};

template <typename F, typename = void> struct layout_name_of
{
  static char const *
  get () LUMEX_NOEXCEPT
  {
    return "";
  }
};

template <typename F>
struct layout_name_of<F,
                      typename std::enable_if<dsl::is_layout<F>::value>::type>
{
  static char const *
  get () LUMEX_NOEXCEPT
  {
    return F::layout_name ();
  }
};

// A field without parts and without a registry.
struct leaf_describer
{
  static descriptor_run_t
  children () LUMEX_NOEXCEPT
  {
    descriptor_run_t const none = { nullptr, 0 };
    return none;
  }

  static char const *
  registry () LUMEX_NOEXCEPT
  {
    return "";
  }
};

// Leaf: no parts.
template <typename F, typename> struct describer : leaf_describer
{
};

template <typename List> struct unnamed_list_run;

template <template <typename...> class L, typename... G>
struct unnamed_list_run<L<G...>>
{
  static descriptor_run_t
  get ()
  {
    return run_of<
        void, descriptor_role::none,
        ::lumex::core::utility::sequence::make_index_sequence<sizeof...(G)>,
        G...>::get ();
  }
};

// A group of fields: a layout, or the field list of a row.
template <typename F>
struct describer<
    F, typename std::enable_if<field::detail::has_fields<F>::value>::type>
    : leaf_describer
{
  static descriptor_run_t
  children ()
  {
    return group_run (typename dsl::is_layout<F>::type ());
  }

private:
  static descriptor_run_t
  group_run (std::true_type)
  {
    return owned_run<descriptor_role::none, F,
                     typename F::fields_type>::get ();
  }

  static descriptor_run_t
  group_run (std::false_type)
  {
    return unnamed_list_run<typename F::fields_type>::get ();
  }
};

template <typename T, std::size_t N>
struct describer<field::array_<T, N>, void> : leaf_describer
{
  static descriptor_run_t
  children ()
  {
    return run_of<void, descriptor_role::element,
                  ::lumex::core::utility::sequence::index_sequence<0>,
                  T>::get ();
  }
};

template <typename T, typename C, std::size_t Max>
struct describer<field::counted_<T, C, Max>, void> : leaf_describer
{
  static descriptor_run_t
  children ()
  {
    return run_of<void, descriptor_role::element,
                  ::lumex::core::utility::sequence::index_sequence<0>,
                  T>::get ();
  }
};

template <typename T, typename E, std::size_t Max>
struct describer<field::computed_count_<T, E, Max>, void> : leaf_describer
{
  static descriptor_run_t
  children ()
  {
    return run_of<void, descriptor_role::element,
                  ::lumex::core::utility::sequence::index_sequence<0>,
                  T>::get ();
  }
};

template <typename... F>
struct describer<field::optional_tail_<F...>, void> : leaf_describer
{
  static descriptor_run_t
  children ()
  {
    return run_of<
        void, descriptor_role::tail,
        ::lumex::core::utility::sequence::make_index_sequence<sizeof...(F)>,
        F...>::get ();
  }
};

// The two forms of a by_length_ field (the roles differ, so a run of its own).
template <typename A, typename B> struct form_run
{
  static descriptor_run_t
  get ()
  {
    static field_descriptor_t const items[3]
        = { make_descriptor_impl<A> ("", descriptor_role::short_form, 0),
            make_descriptor_impl<B> ("", descriptor_role::long_form, 0),
            field_descriptor_t () };
    descriptor_run_t const run = { items, 2 };
    return run;
  }
};

template <std::size_t Min, typename A, typename B>
struct describer<field::by_length_<Min, A, B>, void> : leaf_describer
{
  static descriptor_run_t
  children ()
  {
    return form_run<A, B>::get ();
  }
};

// The cases of a union.
template <typename... Cases> struct case_run
{
  static descriptor_run_t
  get ()
  {
    static field_descriptor_t const items[sizeof...(Cases) + 1]
        = { make_descriptor_impl<typename Cases::layout_type> (
                "", descriptor_role::union_case, Cases::value ())...,
            field_descriptor_t () };
    descriptor_run_t const run = { items, sizeof...(Cases) };
    return run;
  }
};

template <typename D, typename... Cases>
struct describer<field::union_by_<D, Cases...>, void> : leaf_describer
{
  static descriptor_run_t
  children ()
  {
    return case_run<Cases...>::get ();
  }
};

template <typename K, typename... Cases>
struct describer<field::union_by_context_<K, Cases...>, void> : leaf_describer
{
  static descriptor_run_t
  children ()
  {
    return case_run<Cases...>::get ();
  }
};

// The parts of a split word: the fields of one layout of parts (named), or
// the parts written in the split.
template <bool OneGroup, typename... Parts> struct split_run
{
  static descriptor_run_t
  get ()
  {
    return run_of<void, descriptor_role::part,
                  ::lumex::core::utility::sequence::make_index_sequence<
                      sizeof...(Parts)>,
                  Parts...>::get ();
  }
};

template <typename Layout> struct split_run<true, Layout>
{
  static descriptor_run_t
  get ()
  {
    return owned_run<
        descriptor_role::part, typename field::resolve<Layout>::type,
        typename field::resolve<Layout>::type::fields_type>::get ();
  }
};

template <typename Word, typename... Parts>
struct describer<field::split_<Word, Parts...>, void> : leaf_describer
{
  static descriptor_run_t
  children ()
  {
    return split_run<field::detail::single_group<Parts...>::value,
                     Parts...>::get ();
  }
};

template <typename P, typename K>
struct describer<field::nested_<P, K>, void> : leaf_describer
{
  static descriptor_run_t
  children ()
  {
    static field_descriptor_t const items[2]
        = { make_descriptor_impl<K> ("key", descriptor_role::key, 0),
            field_descriptor_t () };
    descriptor_run_t const run = { items, 1 };
    return run;
  }
};

template <typename R, typename F1, typename F2>
struct describer<registry::registry_key_<R, F1, F2>, void> : leaf_describer
{
  static char const *
  registry () LUMEX_NOEXCEPT
  {
    return R::registry_name ();
  }
};

template <typename R, std::size_t K, typename Raw, registry::access_t N>
struct describer<registry::registry_value_<R, K, Raw, N>, void>
    : leaf_describer
{
  static char const *
  registry () LUMEX_NOEXCEPT
  {
    return R::registry_name ();
  }
};

template <typename F>
field_descriptor_t
make_descriptor_impl (char const *name, descriptor_role role,
                      std::int64_t value)
{
  typedef typename field::resolve<F>::type resolved;
  return make_node (name, role, value, field::impl_of<resolved>::type::info (),
                    layout_name_of<resolved>::get (),
                    describer<resolved>::registry (),
                    describer<resolved>::children ());
}

} // namespace detail

/// @brief The name of a kind of field (`"integer"`, `"hex_text"`, ...).
inline char const *
field_kind_name (::lumex::core::protocol::field::field_kind kind)
    LUMEX_NOEXCEPT
{
  static char const *const names[]
      = { "integer",      "floating",       "sign_magnitude",
          "hex_text",     "decimal_text",   "character",
          "text",         "token",          "bits",
          "mask",         "enumeration",    "array",
          "counted",      "computed_count", "optional_tail",
          "by_length",    "union_by",       "union_by_context",
          "split",        "nested",         "group",
          "registry_key", "registry_value" };
  unsigned const at = static_cast<unsigned> (kind);
  return at < sizeof (names) / sizeof (names[0]) ? names[at] : "unknown";
}

/// @brief The name of a descriptor role (`"element"`, `"union_case"`, ...).
inline char const *
role_name (descriptor_role role) LUMEX_NOEXCEPT
{
  static char const *const names[]
      = { "field",     "element", "union_case", "short_form",
          "long_form", "tail",    "part",       "key" };
  unsigned const at = static_cast<unsigned> (role);
  return at < sizeof (names) / sizeof (names[0]) ? names[at] : "unknown";
}

/**
 * @brief The descriptor of field `F` named `name`.
 * @param role What the field is to its parent.
 */
template <typename F>
field_descriptor_t
make_descriptor (char const *name,
                 descriptor_role role = descriptor_role::none)
{
  return detail::make_descriptor_impl<F> (name, role, 0);
}

/// @brief The descriptors of the request fields of row `Row`, named by the
/// row (a static array, one per row type).
template <typename Row>
descriptor_run_t
request_descriptors ()
{
  return detail::owned_run<descriptor_role::none, Row,
                           typename Row::fields_type>::get ();
}
} // namespace index
} // namespace protocol
} // namespace core
} // namespace lumex

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // !LUMEX_CORE_PROTOCOL_INDEX_DESCRIPTOR_HPP
