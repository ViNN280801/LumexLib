// The module declares nothing at global scope: names of the same spelling that
// a program has in the global namespace (shared_ptr, weak_ptr, make_shared,
// ...) stay the program's own after the umbrella is included, and the
// module's names are reachable only through its namespace. The classes are
// the module's own: they are not aliases of the standard ones.

#include <memory>
#include <type_traits>

#include <gtest/gtest.h>

// Global names that would clash with a module that leaked its own.
struct shared_ptr
{
  int global_marker;
};

struct weak_ptr
{
  int global_marker;
};

struct bad_weak_ptr
{
  int global_marker;
};

template <class T> struct enable_shared_from_this
{
  int global_marker;
};

template <class T> struct owner_less
{
  int global_marker;
};

inline int
make_shared ()
{
  return 61;
}

inline int
allocate_shared ()
{
  return 62;
}

inline int
static_pointer_cast ()
{
  return 63;
}

inline int
from_std ()
{
  return 64;
}

inline int
to_std ()
{
  return 65;
}

#include "lumex/core/smart_ptr/LumexSmartPtr"

TEST (LumexSmartPtrGlobalNamesTest,
      GivenGlobalNames_WhenTheModuleIsIncluded_ThenTheyAreStillTheGlobalOnes)
{
  shared_ptr global_class = { 1 };
  weak_ptr global_weak = { 2 };
  bad_weak_ptr global_bad = { 3 };
  ::enable_shared_from_this<int> global_base = { 4 };
  ::owner_less<int> global_less = { 5 };
  EXPECT_EQ (global_class.global_marker, 1);
  EXPECT_EQ (global_weak.global_marker, 2);
  EXPECT_EQ (global_bad.global_marker, 3);
  EXPECT_EQ (global_base.global_marker, 4);
  EXPECT_EQ (global_less.global_marker, 5);
  EXPECT_EQ (::make_shared (), 61);
  EXPECT_EQ (::allocate_shared (), 62);
  EXPECT_EQ (::static_pointer_cast (), 63);
  EXPECT_EQ (::from_std (), 64);
  EXPECT_EQ (::to_std (), 65);
}

TEST (
    LumexSmartPtrGlobalNamesTest,
    GivenTheModule_WhenNamingItsClasses_ThenTheyAreInTheModuleNamespaceAndNotTheStandardOnes)
{
  namespace sp = lumex::core::smart_ptr;
  EXPECT_FALSE ((std::is_same<::shared_ptr, sp::shared_ptr<int>>::value));
  EXPECT_FALSE (
      (std::is_same<sp::shared_ptr<int>, std::shared_ptr<int>>::value));
  EXPECT_FALSE ((std::is_same<sp::weak_ptr<int>, std::weak_ptr<int>>::value));
  EXPECT_FALSE ((std::is_same<sp::bad_weak_ptr, std::bad_weak_ptr>::value));
  EXPECT_FALSE ((std::is_same<sp::owner_less<sp::shared_ptr<int>>,
                              std::owner_less<std::shared_ptr<int>>>::value));
  EXPECT_FALSE ((std::is_same<sp::enable_shared_from_this<int>,
                              std::enable_shared_from_this<int>>::value));
  EXPECT_TRUE ((std::is_base_of<std::bad_weak_ptr, sp::bad_weak_ptr>::value))
      << "the exception is a standard bad_weak_ptr, so standard handlers "
         "catch it";
}
