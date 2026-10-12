/*
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
 * @file LumexCast.hpp
 * @brief `downcast()`, a checked `dynamic_cast` from a polymorphic base to a
 * derived class that throws `bad_down_cast` on a type mismatch, and the
 * non-throwing `downcast_noexcept()`.
 * @details Works from C++11. The constraints accept only a real downcast:
 * pointer to pointer or lvalue reference to lvalue reference, complete class
 * types, a polymorphic source, a target derived from the source through a
 * public and unambiguous base (or the same class) and no loss of
 * cv-qualifiers. Any other use finds no overload (SFINAE, the same
 * `std::enable_if` form in every standard), so it can be detected and never
 * fails inside the body. A null pointer converts to a null pointer without an
 * error. `bad_down_cast` derives from `std::bad_cast` and names both types in
 * its message.
 */
#ifndef LUMEX_CORE_UTILITY_CAST_HPP
#define LUMEX_CORE_UTILITY_CAST_HPP

#include <string>
#include <type_traits>
#include <typeinfo>

#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

namespace lumex
{
namespace core
{
namespace utility
{
namespace cast
{
namespace Detail
{
/// @see https://timsong-cpp.github.io/cppwp/n4868/expr.dynamic.cast
// --- dynamic_cast operand forms (pointer / lvalue reference to class) --- //

/// @brief `T` is a pointer or an lvalue reference to a class type.
template <typename T>
struct is_dyn_cast_form
    : std::integral_constant<
          bool, traits::meta::is_pointer_to_class<T>::value
                    || traits::meta::is_lvalue_ref_to_class<T>::value>
{
};

/// @brief Both operands are dynamic_cast forms of the same shape (two
/// pointers or two references).
template <typename Base, typename Derived>
struct have_matching_shape
    : std::integral_constant<bool,
                             is_dyn_cast_form<Base>::value
                                 && is_dyn_cast_form<Derived>::value
                                 && std::is_pointer<Base>::value
                                        == std::is_pointer<Derived>::value>
{
};

/// @brief The shapes match and both classes are complete. The classes are
/// looked at only after the shapes matched, so no trait below sees a type it
/// cannot be asked about (`std::is_polymorphic` needs a complete class).
template <typename Base, typename Derived,
          bool = have_matching_shape<Base, Derived>::value>
struct have_complete_classes : std::false_type
{
};

template <typename Base, typename Derived>
struct have_complete_classes<Base, Derived, true>
    : std::integral_constant<
          bool, traits::meta::is_complete_type<
                    traits::meta::indirection_of_t<Base>>::value
                    && traits::meta::is_complete_type<
                        traits::meta::indirection_of_t<Derived>>::value>
{
};

template <typename Base, typename Derived,
          bool = have_complete_classes<Base, Derived>::value>
struct is_valid_down_cast_impl : std::false_type
{
};

template <typename Base, typename Derived>
struct is_valid_down_cast_impl<Base, Derived, true>
    : std::integral_constant<
          bool,
          // p5: v must be pointer-to / glvalue-of a polymorphic type
          std::is_polymorphic<typename std::remove_cv<
              traits::meta::indirection_of_t<Base>>::type>::value
              // narrowed to an actual downcast - not an arbitrary cross-cast
              && traits::meta::is_derived_from<
                  typename std::remove_cv<
                      traits::meta::indirection_of_t<Derived>>::type,
                  typename std::remove_cv<
                      traits::meta::indirection_of_t<Base>>::type>::value
              // p2
              && traits::meta::preserves_cv<
                  traits::meta::indirection_of_t<Base>,
                  traits::meta::indirection_of_t<Derived>>::value>
{
};

/**
 * @brief `Base` can be cast to `Derived` by `downcast`: both are pointers to
 * classes or both are lvalue references to classes, both classes are complete,
 * the source class is polymorphic, the target class is derived from it (or is
 * the same class) through a public and unambiguous base and the cast drops no
 * `const` or `volatile`.
 * @details `Base` is the type of the operand as `downcast` sees it: a pointer
 * type, or `Base &` for the reference form.
 */
template <typename Base, typename Derived>
struct is_valid_down_cast : is_valid_down_cast_impl<Base, Derived>
{
};
} // namespace Detail

/**
 * @brief Exception thrown by downcast() when the runtime type does not match
 * the requested target type.
 */
class bad_down_cast : public std::bad_cast
{
public:
  bad_down_cast (std::type_info const &from, std::type_info const &to)
      : m_message (std::string ("downcast failed: dynamic type '")
                   + from.name () + "' is not a '" + to.name () + "'")
  {
  }

  bad_down_cast (std::type_info const &from, std::type_info const &to,
                 char const *details)
      : m_message (std::string ("downcast failed: dynamic type '")
                   + from.name () + "' is not a '" + to.name ()
                   + "'. Details: " + details)
  {
  }

  LUMEX_ATTRIBUTE_NODISCARD ("return value must be used")
  char const *
  what () const LUMEX_NOEXCEPT override
  {
    return m_message.c_str ();
  }

private:
  std::string m_message;
};

/**
 * @brief Safe downcast helper (pointer form): performs a `dynamic_cast` and
 * throws bad_down_cast on failure.
 * @tparam Derived Target pointer type to downcast to.
 * @tparam Base Source pointer type, deduced from the argument.
 * @param base Pointer to downcast.
 * @return The downcast pointer.
 * @throws bad_down_cast if `base` does not actually point to a `Derived`.
 * @note A null pointer input returns a null pointer (not treated as a cast
 * failure).
 * @note Split from the reference-form overload below rather than one
 * function on one by-value `Base base` parameter: template argument
 * deduction for a by-value parameter can never deduce `Base` as a reference
 * type, so a single signature could never actually satisfy
 * `Detail::is_valid_down_cast<Base, Derived>` for `downcast<Derived &>(...)`
 * calls - `Base` must be deduced from a plain lvalue-reference parameter
 * instead (see the overload below).
 */
template <typename Derived, typename Base,
          typename std::enable_if<
              std::is_pointer<Derived>::value
                  && Detail::is_valid_down_cast<Base, Derived>::value,
              int>::type
          = 0>
LUMEX_ATTRIBUTE_NODISCARD ("return value must be used")
Derived downcast (Base base)
{
  if (base == nullptr)
    return nullptr; // p6: null in, null out - not a cast failure.

  Derived result = dynamic_cast<Derived> (base);
  if (result == nullptr)
    throw bad_down_cast (typeid (*base),
                         typeid (typename std::remove_pointer<Derived>::type));
  return result;
}

/**
 * @brief Safe downcast helper (reference form): performs a `dynamic_cast` and
 * throws bad_down_cast on failure.
 * @tparam Derived Target lvalue reference type to downcast to.
 * @tparam Base Source type (deduced, unreferenced) - wrapped back into `Base
 * &` before being checked against `Detail::is_valid_down_cast`, see the
 * pointer-form overload's note above.
 * @param base Reference to downcast.
 * @return The downcast reference.
 * @throws bad_down_cast if `base` does not actually refer to a `Derived`.
 */
template <typename Derived, typename Base,
          typename std::enable_if<
              std::is_lvalue_reference<Derived>::value
                  && Detail::is_valid_down_cast<Base &, Derived>::value,
              int>::type
          = 0>
LUMEX_ATTRIBUTE_NODISCARD ("return value must be used")
Derived downcast (Base &base)
{
  // According to p9, std::bad_cast may be thrown.
  try
    {
      return dynamic_cast<Derived> (base);
    }
  catch (std::bad_cast const &bc_exc)
    {
      throw bad_down_cast (
          typeid (base),
          typeid (typename std::remove_reference<Derived>::type),
          bc_exc.what ());
    }
}

/**
 * @brief Non-throwing variant of downcast(): returns nullptr instead of
 * throwing bad_down_cast.
 * @tparam Derived Target pointer type to downcast to. A reference target
 * finds no overload: a reference cannot report a failure by `nullptr`.
 * @tparam Base Source pointer type, deduced from the argument.
 * @note A null pointer input returns a null pointer. The function does not
 * build an exception, so it cannot run out of memory.
 */
template <typename Derived, typename Base,
          typename std::enable_if<
              std::is_pointer<Derived>::value
                  && Detail::is_valid_down_cast<Base, Derived>::value,
              int>::type
          = 0>
LUMEX_ATTRIBUTE_NODISCARD ("return value must be used")
Derived downcast_noexcept (Base base) LUMEX_NOEXCEPT
{
  // dynamic_cast of a null pointer is a null pointer, and a failed cast of a
  // pointer is a null pointer: no exception is thrown here.
  return dynamic_cast<Derived> (base);
}
} // namespace cast
} // namespace utility
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_UTILITY_CAST_HPP
