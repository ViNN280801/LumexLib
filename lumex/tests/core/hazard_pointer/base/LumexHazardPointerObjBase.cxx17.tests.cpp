// hazard_pointer_obj_base with deleter types that C++17 changed: a function
// pointer whose type carries noexcept, and a deleter passed as a function.

#include <atomic>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/tests/core/hazard_pointer/LumexHazardPointerTestSupport.hpp"

namespace
{
namespace hp = lumex::core::hazard_pointer;

struct noexcept_node;

using noexcept_deleter_t = void (*) (noexcept_node *) noexcept;

struct noexcept_node
    : hp::hazard_pointer_obj_base<noexcept_node, noexcept_deleter_t>
{
  explicit noexcept_node (int *the_counter) : counter (the_counter) {}
  ~noexcept_node () { ++*counter; }
  int *counter;
};

void
delete_noexcept_node (noexcept_node *p) noexcept
{
  delete p;
}
} // namespace

TEST (LumexHazardPointerObjBaseCxx17Test,
      GivenANoexceptFunctionPointerDeleter_WhenRetired_ThenItRuns)
{
  static_assert (hp::is_hazard_protectable<noexcept_node>::value, "");
  static_assert (noexcept (std::declval<noexcept_node &> ().retire (
                     &delete_noexcept_node)),
                 "retire stays noexcept");
  int deleted = 0;
  (new noexcept_node (&deleted))->retire (&delete_noexcept_node);
  hp::clean_up ();
  EXPECT_EQ (deleted, 1);
}

TEST (
    LumexHazardPointerObjBaseCxx17Test,
    GivenTheBaseOfAnInlineVariableClass_WhenCheckingFinal_ThenTheTraitUsesTheStandard)
{
  // From C++14 the library has std::is_final; the module's own test of
  // finality must agree with it.
  struct final_one final
  {
    void
    operator() (void *) const
    {
    }
  };
  struct open_one
  {
    void
    operator() (void *) const
    {
    }
  };
  static_assert (std::is_final<final_one>::value, "");
  static_assert (!std::is_final<open_one>::value, "");
  static_assert (hp::detail::is_final_class<final_one>::value, "agrees");
  static_assert (!hp::detail::is_final_class<open_one>::value, "agrees");
  SUCCEED ();
}
