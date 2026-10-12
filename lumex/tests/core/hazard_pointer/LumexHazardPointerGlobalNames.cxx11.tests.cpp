// The module declares its names only in lumex::core::hazard_pointer: a
// program with global classes of the same names keeps compiling. These
// declarations come before the module header, so the translation unit stops
// compiling if the header declares any of the names at global scope.

#include <memory>
#include <type_traits>

#include <gtest/gtest.h>

struct hazard_pointer
{
  int global_marker;
};

template <class T, class D = std::default_delete<T>>
struct hazard_pointer_obj_base
{
  int global_marker;
};

inline int
make_hazard_pointer ()
{
  return 42;
}

inline int
make_hazard_pointer_batch ()
{
  return 43;
}

inline int
clear_hazard_pointer_batch ()
{
  return 44;
}

inline int
clean_up ()
{
  return 45;
}

namespace engine
{
struct slot_t
{
  int global_marker;
};
} // namespace engine

#include "lumex/core/hazard_pointer/LumexHazardPointer"

TEST (LumexHazardPointerGlobalNamesTest,
      GivenGlobalNames_WhenTheModuleIsIncluded_ThenTheyAreStillTheGlobalOnes)
{
  hazard_pointer global_class = { 1 };
  EXPECT_EQ (global_class.global_marker, 1);
  EXPECT_EQ (::make_hazard_pointer (), 42);
  EXPECT_EQ (::make_hazard_pointer_batch (), 43);
  EXPECT_EQ (::clear_hazard_pointer_batch (), 44);
  EXPECT_EQ (::clean_up (), 45);
  ::hazard_pointer_obj_base<int> global_base = { 2 };
  EXPECT_EQ (global_base.global_marker, 2);
  EXPECT_EQ (::engine::slot_t ().global_marker, 0);
}

TEST (LumexHazardPointerGlobalNamesTest,
      GivenTheModule_WhenNamingItsClasses_ThenTheyAreInTheModuleNamespace)
{
  namespace hp = lumex::core::hazard_pointer;
  EXPECT_FALSE ((std::is_same<::hazard_pointer, hp::hazard_pointer>::value));
  EXPECT_FALSE ((std::is_same<::hazard_pointer_obj_base<int>,
                              hp::hazard_pointer_obj_base<int>>::value));
}
