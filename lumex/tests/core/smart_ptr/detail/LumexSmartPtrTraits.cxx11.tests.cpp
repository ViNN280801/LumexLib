// Tests of the building blocks in detail/: the type identity without RTTI,
// the empty-base storage, the default allocator (alignment on every
// standard, overflow), the "compatible with" relations of the standard
// including the array forms, the deleter-callability trait, the default
// deleters, and the enable_shared_from_this probes (found, none, ambiguous,
// inaccessible, the standard base).

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <set>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/core/smart_ptr/LumexSmartPtrTestSupport.hpp"

namespace
{
using namespace smart_ptr_test;
namespace detail = sp::detail;

struct EmptyThing
{
};
struct FinalEmpty final
{
};
struct NonEmpty
{
  int value;
};

// Inside a class that has the empty type as a private base the name
// EmptyThing is the injected name of that base and is inaccessible: build
// the values outside.
EmptyThing
make_empty ()
{
  return EmptyThing ();
}
FinalEmpty
make_final ()
{
  return FinalEmpty ();
}

TEST (LumexSmartPtrTraitsTest,
      GivenTypeId_WhenAskedForTypes_ThenItIsUniqueAndStable)
{
  EXPECT_EQ (detail::type_id<int> (), detail::type_id<int> ());
  EXPECT_NE (detail::type_id<int> (), detail::type_id<long> ());
  EXPECT_NE (detail::type_id<int> (), detail::type_id<int const> ())
      << "the id is per exact type; get_deleter strips cv itself";
  EXPECT_NE (detail::type_id<EmptyThing> (), detail::type_id<NonEmpty> ());
  EXPECT_NE (detail::type_id<void> (), nullptr);
  std::set<void const *> ids;
  ids.insert (detail::type_id<int> ());
  ids.insert (detail::type_id<char> ());
  ids.insert (detail::type_id<double> ());
  ids.insert (detail::type_id<EmptyThing> ());
  ids.insert (detail::type_id<FinalEmpty> ());
  ids.insert (detail::type_id<NonEmpty> ());
  EXPECT_EQ (ids.size (), 6u);
}

typedef detail::ebo_member<EmptyThing, 0> empty_member_0;
typedef detail::ebo_member<EmptyThing, 1> empty_member_1;
typedef detail::ebo_member<FinalEmpty, 0> final_member;

struct WithEmpty : empty_member_0
{
  WithEmpty () : empty_member_0 (make_empty ()), value (1) {}
  int value;
};

struct WithFinal : final_member
{
  WithFinal () : final_member (make_final ()), value (1) {}
  int value;
};

struct WithBoth : empty_member_0, empty_member_1
{
  WithBoth ()
      : empty_member_0 (make_empty ()), empty_member_1 (make_empty ()),
        value (1)
  {
  }
  int value;
};

TEST (LumexSmartPtrTraitsTest,
      GivenEboMember_WhenStoringEmptyTypes_ThenTheyTakeNoSpace)
{
  static_assert (sizeof (WithEmpty) == sizeof (int),
                 "an empty class takes no room");
  static_assert (sizeof (WithFinal) > sizeof (int),
                 "a final class cannot be a base");
  static_assert (detail::is_final_class<FinalEmpty>::value, "");
  static_assert (!detail::is_final_class<EmptyThing>::value, "");
  EXPECT_LE (sizeof (WithBoth), 2 * sizeof (int))
      << "two tags keep two empty members apart";
  WithEmpty a;
  a.get () = make_empty ();
  detail::ebo_member<NonEmpty, 0> stored (NonEmpty{ 5 });
  EXPECT_EQ (stored.get ().value, 5);
  stored.get ().value = 6;
  EXPECT_EQ (stored.get ().value, 6);
}

TEST (LumexSmartPtrTraitsTest,
      GivenBlockAllocator_WhenAllocatingAlignedTypes_ThenTheMemoryIsAligned)
{
  {
    detail::block_allocator<char> small;
    char *p = small.allocate (3);
    ASSERT_NE (p, nullptr);
    small.deallocate (p, 3);
  }
  {
    detail::block_allocator<OverAligned> over;
    std::vector<OverAligned *> blocks;
    for (int i = 0; i < 16; ++i)
      {
        OverAligned *p = over.allocate (static_cast<std::size_t> (i) + 1);
        ASSERT_NE (p, nullptr);
        EXPECT_EQ (reinterpret_cast<std::uintptr_t> (p) % 64, 0u);
        blocks.push_back (p);
      }
    for (std::size_t i = 0; i < blocks.size (); ++i)
      over.deallocate (blocks[i], i + 1);
  }
  struct alignas (256) Huge
  {
    char bytes[8];
  };
  detail::block_allocator<Huge> huge;
  Huge *h = huge.allocate (2);
  EXPECT_EQ (reinterpret_cast<std::uintptr_t> (h) % 256, 0u);
  huge.deallocate (h, 2);
}

TEST (LumexSmartPtrTraitsTest,
      GivenBlockAllocator_WhenTheSizeOverflows_ThenBadAllocIsThrown)
{
  detail::block_allocator<std::uint64_t> allocator;
  EXPECT_THROW (
      allocator.allocate ((std::numeric_limits<std::size_t>::max) () / 4),
      std::bad_alloc);
}

TEST (LumexSmartPtrTraitsTest,
      GivenBlockAllocator_WhenUsedThroughAllocatorTraits_ThenItIsAnAllocator)
{
  typedef detail::block_allocator<int> alloc;
  typedef std::allocator_traits<alloc> traits;
  static_assert (std::is_same<traits::rebind_alloc<double>,
                              detail::block_allocator<double>>::value,
                 "");
  static_assert (std::is_same<traits::value_type, int>::value, "");
  static_assert (std::is_empty<alloc>::value, "");
  EXPECT_TRUE (alloc () == detail::block_allocator<long> ());
  EXPECT_FALSE (alloc () != detail::block_allocator<long> ());
  alloc a;
  int *p = traits::allocate (a, 4);
  traits::construct (a, p, 7);
  EXPECT_EQ (*p, 7);
  traits::destroy (a, p);
  traits::deallocate (a, p, 4);
  // The void form is only a rebind source.
  typedef traits::rebind_alloc<NonEmpty> rebound;
  static_assert (
      std::is_same<rebound, detail::block_allocator<NonEmpty>>::value, "");
  static_assert (
      std::is_same<std::allocator_traits<
                       detail::block_allocator<void>>::rebind_alloc<int>,
                   detail::block_allocator<int>>::value,
      "");
}

TEST (
    LumexSmartPtrTraitsTest,
    GivenIsNewCompatible_WhenPointersAreHandedToATarget_ThenTheStandardRulesApply)
{
  static_assert (detail::is_new_compatible<int, int>::value, "");
  static_assert (detail::is_new_compatible<int, int const>::value, "");
  static_assert (!detail::is_new_compatible<int const, int>::value, "");
  static_assert (detail::is_new_compatible<Dog, Animal>::value, "");
  static_assert (!detail::is_new_compatible<Animal, Dog>::value, "");
  static_assert (detail::is_new_compatible<int, void>::value, "");
  static_assert (!detail::is_new_compatible<int, double>::value, "");
  // Arrays: the pointer is to the element, the target is the array type.
  static_assert (detail::is_new_compatible<int, int[]>::value, "");
  static_assert (detail::is_new_compatible<int, int const[]>::value, "");
  static_assert (!detail::is_new_compatible<int const, int[]>::value, "");
  static_assert (!detail::is_new_compatible<Dog, Animal[]>::value,
                 "Dog (*)[] does not convert to Animal (*)[]");
  static_assert (detail::is_new_compatible<int, int[4]>::value, "");
  static_assert (!detail::is_new_compatible<Dog, Animal[4]>::value, "");
  static_assert (!detail::is_new_compatible<int, double[]>::value, "");
  SUCCEED ();
}

TEST (
    LumexSmartPtrTraitsTest,
    GivenIsPtrCompatible_WhenConvertingSharedPointers_ThenTheStandardRulesApply)
{
  static_assert (detail::is_ptr_compatible<int, int>::value, "");
  static_assert (detail::is_ptr_compatible<int, int const>::value, "");
  static_assert (!detail::is_ptr_compatible<int const, int>::value, "");
  static_assert (detail::is_ptr_compatible<Dog, Animal>::value, "");
  static_assert (detail::is_ptr_compatible<int, void>::value, "");
  static_assert (detail::is_ptr_compatible<int[], int[]>::value, "");
  static_assert (detail::is_ptr_compatible<int[], int const[]>::value, "");
  static_assert (detail::is_ptr_compatible<int[4], int[4]>::value, "");
  static_assert (detail::is_ptr_compatible<int[4], int const[4]>::value, "");
  static_assert (
      detail::is_ptr_compatible<int[4], int[]>::value,
      "a known bound converts to an unknown bound on every standard");
  static_assert (detail::is_ptr_compatible<int[4], int const[]>::value, "");
  static_assert (!detail::is_ptr_compatible<int[], int[4]>::value, "");
  static_assert (!detail::is_ptr_compatible<int[4], int[5]>::value, "");
  static_assert (!detail::is_ptr_compatible<int const[4], int[]>::value, "");
  static_assert (!detail::is_ptr_compatible<Dog[], Animal[]>::value, "");
  SUCCEED ();
}

TEST (LumexSmartPtrTraitsTest, GivenIsDeleterFor_WhenCallable_ThenTrue)
{
  static_assert (detail::is_deleter_for<CountingDeleter<int>, int *>::value,
                 "");
  static_assert (detail::is_deleter_for<void (*) (int *), int *>::value, "");
  static_assert (!detail::is_deleter_for<void (*) (double *), int *>::value,
                 "");
  static_assert (detail::is_deleter_for<EmptyDeleter, int *>::value, "");
  static_assert (!detail::is_deleter_for<int, int *>::value, "");
  struct AcceptsNull
  {
    void
    operator() (std::nullptr_t) const
    {
    }
  };
  static_assert (detail::is_deleter_for<AcceptsNull, std::nullptr_t>::value,
                 "");
  SUCCEED ();
}

TEST (LumexSmartPtrTraitsTest,
      GivenDefaultDeleters_WhenCalled_ThenTheyDeleteTheRightWay)
{
  std::atomic<int> live (0);
  detail::default_deleter<Dog> single;
  single (new Dog (&live));
  EXPECT_EQ (live.load (), 0);
  detail::default_array_deleter<Animal> many;
  Animal *array
      = new Animal[3]{ Animal (&live), Animal (&live), Animal (&live) };
  EXPECT_EQ (live.load (), 3);
  many (array);
  EXPECT_EQ (live.load (), 0);
  static_assert (std::is_same<detail::default_deleter_for<Dog, Dog>::type,
                              detail::default_deleter<Dog>>::value,
                 "");
  static_assert (
      std::is_same<detail::default_deleter_for<Animal[], Animal>::type,
                   detail::default_array_deleter<Animal>>::value,
      "");
  static_assert (
      std::is_same<detail::default_deleter_for<Animal[3], Animal>::type,
                   detail::default_array_deleter<Animal>>::value,
      "");
  static_assert (std::is_empty<detail::default_deleter<int>>::value, "");
  static_assert (noexcept (single (static_cast<Dog *> (nullptr))), "");
}

// --- enable_shared_from_this probes
// ----------------------------------------------

class NoBase
{
};
class OwnBase : public sp::enable_shared_from_this<OwnBase>
{
};
class OwnDerived : public OwnBase
{
};
class StdBase : public std::enable_shared_from_this<StdBase>
{
};
class BothBases : public sp::enable_shared_from_this<BothBases>,
                  public std::enable_shared_from_this<BothBases>
{
};
class PrivateOwn : private sp::enable_shared_from_this<PrivateOwn>
{
};
class PrivateStd : private std::enable_shared_from_this<PrivateStd>
{
};
class AmbigA
{
};
class AmbigB
{
};
class Ambiguous : public sp::enable_shared_from_this<AmbigA>,
                  public sp::enable_shared_from_this<AmbigB>
{
};
class DiamondLeft : public sp::enable_shared_from_this<DiamondLeft>
{
};
class Grandchild : public DiamondLeft
{
};

// Derives from integral_constant: the tests bind `value` to a reference
// (EXPECT_*), which needs a definition without optimization.
template <class Y>
struct found
    : std::integral_constant<
          bool, !std::is_same<typename detail::esft_probe_result<Y>::type,
                              detail::esft_none_t>::value>
{
};

TEST (LumexSmartPtrTraitsTest,
      GivenTheProbe_WhenAskedForVariousClasses_ThenItFindsOnlyUsableBases)
{
  EXPECT_FALSE (found<NoBase>::value);
  EXPECT_FALSE (found<int>::value);
  EXPECT_FALSE (found<void>::value);
  EXPECT_TRUE (found<OwnBase>::value);
  EXPECT_TRUE (found<OwnDerived>::value);
  EXPECT_TRUE (found<OwnBase const>::value);
  EXPECT_TRUE (found<BothBases>::value);
  EXPECT_FALSE (found<PrivateOwn>::value)
      << "an inaccessible base is not a base";
  EXPECT_FALSE (found<Ambiguous>::value)
      << "two different bases are ambiguous";
  EXPECT_TRUE (found<Grandchild>::value);
  static_assert (std::is_same<detail::esft_probe_result<OwnDerived>::type,
                              detail::esft_found_t<OwnBase>>::value,
                 "the base is enable_shared_from_this<OwnBase>, as declared");
}

TEST (
    LumexSmartPtrTraitsTest,
    GivenIsStdEsftOnly_WhenAskedForVariousClasses_ThenOnlyTheStandardOnlyOnesAreFlagged)
{
  static_assert (!detail::is_std_esft_only<NoBase>::value, "");
  static_assert (!detail::is_std_esft_only<int>::value, "");
  static_assert (!detail::is_std_esft_only<OwnBase>::value, "");
  static_assert (detail::is_std_esft_only<StdBase>::value, "");
  static_assert (!detail::is_std_esft_only<BothBases>::value,
                 "the module's base is present, so the pointer can be set");
  static_assert (
      !detail::is_std_esft_only<PrivateStd>::value,
      "a private standard base is not an enable_shared_from_this base");
  static_assert (detail::is_std_esft_only<StdBase const>::value, "");
  SUCCEED ();
}

TEST (LumexSmartPtrTraitsTest,
      GivenTheDispatch_WhenApplied_ThenOnlyFoundBasesAreSet)
{
  sp::shared_ptr<OwnBase> owner (new OwnBase ());
  // Applying the hook for a plain class is a no-op.
  NoBase plain;
  detail::esft_dispatch<NoBase>::apply (owner, &plain);
  // For a found base, the first application sets, the second leaves.
  OwnBase object;
  sp::shared_ptr<OwnBase> first (&object, [] (OwnBase *) {});
  EXPECT_FALSE (object.weak_from_this ().expired ());
  sp::shared_ptr<OwnBase> second (&object, [] (OwnBase *) {});
  EXPECT_EQ (object.weak_from_this ().lock (), first)
      << "an owned base is not reassigned";
  // A null pointer is ignored.
  detail::esft_dispatch<OwnBase>::apply (owner,
                                         static_cast<OwnBase *> (nullptr));
}
} // namespace
