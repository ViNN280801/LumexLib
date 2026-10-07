// LumexInvokeMember.cxx11.tests.cpp
// The call result of a pointer to a member in invoke_result / is_invocable
// (LumexTypeTraits.hpp). The pointer to a data member overload of the INVOKE
// helper must not match a pointer to a member function: MinGW GCC 8 accepted
// `decltype (object.*pointer)` for it and reported the function type as the
// call result, so `expected::transform (&widget::size)` built the wrong
// type there. Every utility suite compiles this file.
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

namespace invoke = lumex::core::utility::traits::invoke;

namespace
{
struct widget_t
{
  int
  size () const
  {
    return 5;
  }
  int
  bump ()
  {
    return 1;
  }
  int field;
};
} // namespace

TEST (LumexInvokeMemberTest,
      GivenConstMemberFunctionWithoutArguments_WhenInvokeResult_ThenItsResult)
{
  static_assert (
      std::is_same<
          invoke::invoke_result_t<int (widget_t::*) () const, widget_t &>,
          int>::value,
      "called on an object");
  static_assert (
      std::is_same<invoke::invoke_result_t<int (widget_t::*) () const,
                                           widget_t const &>,
                   int>::value,
      "called on a const object");
  static_assert (
      std::is_same<
          invoke::invoke_result_t<int (widget_t::*) () const, widget_t *>,
          int>::value,
      "called through a pointer");
  static_assert (
      std::is_same<
          invoke::invoke_result_t<int (widget_t::*) () const, widget_t &&>,
          int>::value,
      "called on a temporary");
  SUCCEED ();
}

TEST (
    LumexInvokeMemberTest,
    GivenNonConstMemberFunction_WhenInvokeResult_ThenItsResultOnANonConstObject)
{
  static_assert (
      std::is_same<invoke::invoke_result_t<int (widget_t::*) (), widget_t &>,
                   int>::value,
      "called on an object");
  static_assert (
      std::is_same<invoke::invoke_result_t<int (widget_t::*) (), widget_t *>,
                   int>::value,
      "called through a pointer");
  static_assert (
      !invoke::is_invocable<int (widget_t::*) (), widget_t const &>::value,
      "a const object cannot call it");
  SUCCEED ();
}

TEST (LumexInvokeMemberTest,
      GivenMemberData_WhenInvokeResult_ThenAReferenceToIt)
{
  static_assert (
      std::is_same<invoke::invoke_result_t<int widget_t::*, widget_t &>,
                   int &>::value,
      "an lvalue object gives an lvalue");
  static_assert (
      std::is_same<invoke::invoke_result_t<int widget_t::*, widget_t const &>,
                   int const &>::value,
      "a const object gives a const lvalue");
  static_assert (
      std::is_same<invoke::invoke_result_t<int widget_t::*, widget_t &&>,
                   int &&>::value,
      "a temporary gives an rvalue");
  static_assert (
      std::is_same<invoke::invoke_result_t<int widget_t::*, widget_t *>,
                   int &>::value,
      "a pointer gives an lvalue");
  SUCCEED ();
}

TEST (LumexInvokeMemberTest,
      GivenMemberPointers_WhenIsInvocable_ThenTheArgumentsDecide)
{
  static_assert (
      invoke::is_invocable<int (widget_t::*) () const, widget_t &>::value,
      "member function with its object");
  static_assert (!invoke::is_invocable<int (widget_t::*) () const>::value,
                 "member function without an object");
  static_assert (!invoke::is_invocable<int (widget_t::*) () const, widget_t &,
                                       int>::value,
                 "member function with an extra argument");
  static_assert (invoke::is_invocable<int widget_t::*, widget_t &>::value,
                 "data member with its object");
  static_assert (!invoke::is_invocable<int widget_t::*, int>::value,
                 "data member of another class");
  SUCCEED ();
}
