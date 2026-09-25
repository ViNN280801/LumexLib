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

/// @warning Requires C++20 (concepts).

#ifndef LUMEX_CORE_UTILITY_CAST_HPP
#define LUMEX_CORE_UTILITY_CAST_HPP

#include <stdexcept>
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
template <typename T>
concept DynCastForm
    = traits::meta::PointerToClass<T> || traits::meta::LvalueRefToClass<T>;

template <typename Base, typename Derived>
concept ValidDownCast
    = DynCastForm<Base> && DynCastForm<Derived>
      && (std::is_pointer_v<Base>
          == std::is_pointer_v<Derived>) // matching ptr "shape"
      &&traits::meta::CompleteType<traits::meta::indirection_of_t<Base>>
      && traits::meta::CompleteType<traits::meta::indirection_of_t<Derived>> &&
      // p5: v must be pointer-to / glvalue-of a polymorphic type
      std::is_polymorphic_v<
          std::remove_cv_t<traits::meta::indirection_of_t<Base>>>
      &&
      // narrowed to an actual downcast - not an arbitrary cross-cast
      std::derived_from<
          std::remove_cv_t<traits::meta::indirection_of_t<Derived>>,
          std::remove_cv_t<traits::meta::indirection_of_t<Base>>>
      // p2
      && traits::meta::PreserveCV<traits::meta::indirection_of_t<Base>,
                                  traits::meta::indirection_of_t<Derived>>;
} // namespace Detail

/**
 * @brief Exception thrown by downcast() when the runtime type does not match
 * the requested target type.
 */
class BadDownCast : public std::bad_cast
{
public:
  BadDownCast (std::type_info const &from, std::type_info const &to)
      : m_message (std::string ("downcast failed: dynamic type '")
                   + from.name () + "' is not a '" + to.name () + "'")
  {
  }

  BadDownCast (std::type_info const &from, std::type_info const &to,
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
 * throws BadDownCast on failure.
 * @tparam Derived Target pointer type to downcast to.
 * @tparam Base Source pointer type, deduced from the argument.
 * @param base Pointer to downcast.
 * @return The downcast pointer.
 * @throws BadDownCast if `base` does not actually point to a `Derived`.
 * @note A null pointer input returns a null pointer (not treated as a cast
 * failure).
 * @note Split from the reference-form overload below rather than a single `if
 * constexpr` on one by-value `Base base` parameter: template argument
 * deduction for a by-value parameter can never deduce `Base` as a reference
 * type, so a single signature could never actually satisfy
 *       `Detail::LvalueRefToClass<Base>` for `downcast<Derived&>(...)` calls -
 * `Base` must be deduced from a plain lvalue-reference parameter instead (see
 * the overload below).
 */
template <typename Derived, typename Base>
  requires std::is_pointer_v<Derived> && Detail::ValidDownCast<Base, Derived>
LUMEX_ATTRIBUTE_NODISCARD ("return value must be used")
Derived downcast (Base base)
{
  if (base == nullptr)
    return nullptr; // p6: null in, null out - not a cast failure.

  Derived result = dynamic_cast<Derived> (base);
  if (result == nullptr)
    throw BadDownCast (typeid (*base),
                       typeid (std::remove_pointer_t<Derived>));
  return result;
}

/**
 * @brief Safe downcast helper (reference form): performs a `dynamic_cast` and
 * throws BadDownCast on failure.
 * @tparam Derived Target lvalue reference type to downcast to.
 * @tparam Base Source type (deduced, unreferenced) - wrapped back into `Base
 * &` before being checked against `Detail::ValidDownCast`, see the
 * pointer-form overload's note above.
 * @param base Reference to downcast.
 * @return The downcast reference.
 * @throws BadDownCast if `base` does not actually refer to a `Derived`.
 */
template <typename Derived, typename Base>
  requires std::is_lvalue_reference_v<Derived>
           && Detail::ValidDownCast<Base &, Derived>
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
      throw BadDownCast (typeid (base),
                         typeid (std::remove_reference_t<Derived>),
                         bc_exc.what ());
    }
}

/**
 * @brief Non-throwing variant of downcast(): returns nullptr instead of
 * throwing BadDownCast.
 * @tparam Derived Target pointer type to downcast to (reference forms still
 * throw from dynamic_cast on catastrophic failure paths outside our control,
 * so this overload is primarily useful for pointer downcasts).
 * @tparam Base Source pointer type, deduced from the argument.
 */
template <typename Derived, typename Base>
  requires Detail::ValidDownCast<Base, Derived>
LUMEX_ATTRIBUTE_NODISCARD ("return value must be used")
Derived downcast_noexcept (Base base) LUMEX_NOEXCEPT
{
  try
    {
      return downcast<Derived> (base);
    }
  catch (BadDownCast const &)
    {
      return nullptr;
    }
}
} // namespace cast
} // namespace utility
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_UTILITY_CAST_HPP
