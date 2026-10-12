// Tests of make_shared and allocate_shared ([util.smartptr.shared.create]):
// the arguments are forwarded, block and object are ONE allocation (counted
// through allocate_shared with a counting allocator), the allocator is
// rebound to the block, construct and destroy of the allocator are used, an
// exception from the constructor or from the allocator leaves nothing behind,
// over-aligned types are aligned on every standard, cv-qualified types work,
// and a class with enable_shared_from_this is wired up. The observable
// behavior is also run on std::make_shared.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/core/smart_ptr/LumexSmartPtrTestSupport.hpp"

namespace
{
using namespace smart_ptr_test;

// A type that records how its constructor saw its arguments.
class Recorder
{
public:
  Recorder (int first, std::string text)
      : a (first), b (std::move (text)), kind ("copy-ish")
  {
  }
  Recorder (int first, std::string &&text, int)
      : a (first), b (std::move (text)), kind ("rvalue")
  {
  }
  Recorder (int first, std::string &text, int, int)
      : a (first), b (text), kind ("lvalue ref")
  {
  }
  int a;
  std::string b;
  char const *kind;
};

class MoveOnly
{
public:
  explicit MoveOnly (std::unique_ptr<int> owned) : p (std::move (owned)) {}
  std::unique_ptr<int> p;
};

std::atomic<std::size_t> &
measured_size ()
{
  static std::atomic<std::size_t> size (0);
  return size;
}

// A stateless allocator that counts its blocks in statics, to see the size of
// the block that a make_shared of the module itself allocates.
template <class T> class MeasuringAllocator
{
public:
  typedef T value_type;
  MeasuringAllocator () {}
  template <class U> MeasuringAllocator (MeasuringAllocator<U> const &) {}
  template <class U>
  friend bool
  operator== (MeasuringAllocator const &, MeasuringAllocator<U> const &)
  {
    return true;
  }
  template <class U>
  friend bool
  operator!= (MeasuringAllocator const &, MeasuringAllocator<U> const &)
  {
    return false;
  }
  T *
  allocate (std::size_t n)
  {
    measured_size ().store (n * sizeof (T));
    return std::allocator<T> ().allocate (n);
  }
  void
  deallocate (T *p, std::size_t n)
  {
    std::allocator<T> ().deallocate (p, n);
  }
  // One counter for every rebind: the block is allocated through the
  // allocator rebound to the block type.
  static std::atomic<std::size_t> &
  last_size ()
  {
    return measured_size ();
  }
};
template <class F>
void
scenario_make (Trace &t)
{
  lumex_test::ObjectLedger ledger (4);
  {
    shared_of<F, Probe> p = F::template make<Probe> (std::ref (ledger), 12);
    t.note (p->value);
    t.note (p.use_count ());
    shared_of<F, Probe> q = p;
    t.note (p.use_count ());
    weak_of<F, Probe> w = p;
    p.reset ();
    q.reset ();
    t.note_bool (w.expired ());
    t.note (static_cast<long> (ledger.alive ()));
  }
  t.note_bool (ledger.balanced ());
  shared_of<F, std::vector<int>> v = F::template make<std::vector<int>> (3, 4);
  t.note (static_cast<long> (v->size ()));
  t.note ((*v)[2]);
  shared_of<F, int const> constant = F::template make<int const> (7);
  t.note (*constant);
  shared_of<F, std::string> s = F::template make<std::string> ();
  t.note_bool (s->empty ());
}

template <class F>
void
scenario_forwarding (Trace &t)
{
  std::string lvalue = "x";
  shared_of<F, Recorder> a = F::template make<Recorder> (1, lvalue);
  t.note_bool (a->kind == std::string ("copy-ish"));
  t.note_bool (lvalue == "x");
  shared_of<F, Recorder> b
      = F::template make<Recorder> (2, std::string ("y"), 0);
  t.note_bool (b->kind == std::string ("rvalue"));
  shared_of<F, Recorder> c = F::template make<Recorder> (3, lvalue, 0, 0);
  t.note_bool (c->kind == std::string ("lvalue ref"));
  t.note_bool (c->b == "x");
  shared_of<F, MoveOnly> m
      = F::template make<MoveOnly> (std::unique_ptr<int> (new int (5)));
  t.note (*m->p);
}

template <class F>
void
scenario_constructor_throws (Trace &t)
{
  std::atomic<int> live (0);
  bool threw = false;
  try
    {
      shared_of<F, Fragile> p = F::template make<Fragile> (&live, true);
    }
  catch (std::runtime_error const &)
    {
      threw = true;
    }
  t.note_bool (threw);
  t.note (live.load ());
  shared_of<F, Fragile> ok = F::template make<Fragile> (&live, false);
  t.note (live.load ());
}

template <class F>
void
scenario_allocate (Trace &t)
{
  AllocStats stats;
  lumex_test::ObjectLedger ledger (4);
  {
    shared_of<F, Probe> p = F::template allocate<Probe> (
        CountingAllocator<Probe> (&stats), std::ref (ledger), 4);
    t.note (stats.allocations.load ());
    t.note (stats.constructs.load ());
    t.note (p->value);
    weak_of<F, Probe> w = p;
    p.reset ();
    // The object is destroyed through the allocator, the block stays.
    t.note (stats.destroys.load ());
    t.note (stats.deallocations.load ());
    t.note (static_cast<long> (ledger.alive ()));
  }
  t.note (stats.deallocations.load ());
  // An allocator of another value type is rebound.
  AllocStats other_stats;
  {
    shared_of<F, Probe> p = F::template allocate<Probe> (
        CountingAllocator<char> (&other_stats), std::ref (ledger), 1);
    t.note (other_stats.allocations.load ());
    t.note (other_stats.constructs.load ());
  }
  t.note (other_stats.deallocations.load ());
  t.note_bool (ledger.balanced ());
}

template <class F>
void
scenario_allocation_failure (Trace &t)
{
  AllocStats stats;
  stats.fail_at.store (1);
  lumex_test::ObjectLedger ledger (4);
  bool threw = false;
  try
    {
      shared_of<F, Probe> p = F::template allocate<Probe> (
          CountingAllocator<Probe> (&stats), std::ref (ledger));
    }
  catch (std::bad_alloc const &)
    {
      threw = true;
    }
  t.note_bool (threw);
  t.note (static_cast<long> (ledger.constructed ()));
  t.note (stats.live_blocks ());
}

template <class F>
void
scenario_constructor_throws_with_allocator (Trace &t)
{
  AllocStats stats;
  std::atomic<int> live (0);
  bool threw = false;
  try
    {
      shared_of<F, Fragile> p = F::template allocate<Fragile> (
          CountingAllocator<Fragile> (&stats), &live, true);
    }
  catch (std::runtime_error const &)
    {
      threw = true;
    }
  t.note_bool (threw);
  t.note (stats.allocations.load ());
  t.note (stats.deallocations.load ());
  t.note (live.load ());
}

TEST (LumexMakeSharedTest, GivenMakeShared_WhenObserved_ThenItMatchesStd)
{
  EXPECT_TRACES_EQUAL (scenario_make);
}

TEST (LumexMakeSharedTest,
      GivenArguments_WhenForwarded_ThenTheValueCategoryIsKept)
{
  EXPECT_TRACES_EQUAL (scenario_forwarding);
}

TEST (LumexMakeSharedTest,
      GivenAThrowingConstructor_WhenMaking_ThenNothingLeaks)
{
  EXPECT_TRACES_EQUAL (scenario_constructor_throws);
}

TEST (
    LumexMakeSharedTest,
    GivenAllocateShared_WhenObserved_ThenOneBlockIsAllocatedAndConstructedThroughIt)
{
  EXPECT_TRACES_EQUAL (scenario_allocate);
  AllocStats stats;
  lumex_test::ObjectLedger ledger (2);
  sp::shared_ptr<Probe> p
      = sp::allocate_shared<Probe> (CountingAllocator<Probe> (&stats), ledger);
  EXPECT_EQ (stats.allocations.load (), 1)
      << "block and object are one allocation";
  EXPECT_EQ (stats.constructs.load (), 1);
  p.reset ();
  EXPECT_EQ (stats.destroys.load (), 1);
  EXPECT_EQ (stats.deallocations.load (), 1);
}

TEST (LumexMakeSharedTest,
      GivenAFailingAllocator_WhenAllocating_ThenNoObjectIsConstructed)
{
  EXPECT_TRACES_EQUAL (scenario_allocation_failure);
}

TEST (
    LumexMakeSharedTest,
    GivenAThrowingConstructorAndAnAllocator_WhenAllocating_ThenTheBlockIsReturned)
{
  EXPECT_TRACES_EQUAL (scenario_constructor_throws_with_allocator);
}

TEST (LumexMakeSharedTest,
      GivenAnAllocatorOfAnotherType_WhenAllocating_ThenItIsReboundToTheBlock)
{
  AllocStats stats;
  lumex_test::ObjectLedger ledger (2);
  {
    sp::shared_ptr<Probe> p = sp::allocate_shared<Probe> (
        CountingAllocator<char> (&stats), ledger);
    // The allocation is the size of the block, not one char.
    EXPECT_GE (static_cast<std::size_t> (stats.bytes.load ()),
               32 + sizeof (Probe));
    EXPECT_EQ (stats.allocations.load (), 1);
  }
  EXPECT_EQ (stats.deallocations.load (), 1);
}

TEST (LumexMakeSharedTest,
      GivenMakeShared_WhenMeasured_ThenTheBlockIsThirtyTwoBytesPlusTheObject)
{
  sp::shared_ptr<int> p
      = sp::allocate_shared<int> (MeasuringAllocator<int> (), 5);
  // 32 bytes of control block, then the object; the allocator is empty and
  // takes no room.
  if (sizeof (void *) == 8)
    {
      EXPECT_EQ (MeasuringAllocator<int>::last_size ().load (), 40u);
    }
  sp::shared_ptr<std::string> s
      = sp::allocate_shared<std::string> (MeasuringAllocator<std::string> ());
  if (sizeof (void *) == 8)
    {
      EXPECT_EQ (MeasuringAllocator<std::string>::last_size ().load (),
                 32 + sizeof (std::string));
    }
}

TEST (LumexMakeSharedTest, GivenAnOverAlignedType_WhenMaking_ThenItIsAligned)
{
  for (int i = 0; i < 8; ++i)
    {
      sp::shared_ptr<OverAligned> p = sp::make_shared<OverAligned> (i);
      EXPECT_EQ (reinterpret_cast<std::uintptr_t> (p.get ()) % 64, 0u);
      EXPECT_EQ (p->value, i);
    }
  AllocStats stats;
  sp::shared_ptr<OverAligned> q = sp::allocate_shared<OverAligned> (
      CountingAllocator<OverAligned> (&stats), 3);
  EXPECT_EQ (reinterpret_cast<std::uintptr_t> (q.get ()) % 64, 0u);
  // The block is allocated with the alignment of the object.
  EXPECT_EQ (stats.last_alignment_seen.load (), 64u);
}

TEST (LumexMakeSharedTest,
      GivenCvQualifiedTypes_WhenMaking_ThenTheyAreConstructed)
{
  lumex_test::ObjectLedger ledger (2);
  {
    sp::shared_ptr<Probe const> p = sp::make_shared<Probe const> (ledger, 3);
    EXPECT_EQ (p->value, 3);
    sp::shared_ptr<volatile int> v = sp::make_shared<volatile int> (4);
    EXPECT_EQ (*v, 4);
    sp::shared_ptr<int const volatile> cv
        = sp::make_shared<int const volatile> (5);
    EXPECT_EQ (*cv, 5);
  }
  EXPECT_TRUE (ledger.balanced ());
}

TEST (LumexMakeSharedTest, GivenAnAllocator_WhenCopied_ThenTheBlockHoldsACopy)
{
  AllocStats stats;
  CountingAllocator<int> alloc (&stats);
  sp::shared_ptr<int> p = sp::allocate_shared<int> (alloc, 1);
  alloc = CountingAllocator<int> (nullptr); // the original is gone
  EXPECT_EQ (*p, 1);
  p.reset ();
  EXPECT_EQ (stats.deallocations.load (), 1) << "the block's own copy frees";
}

TEST (LumexMakeSharedTest,
      GivenTheObjectAddress_WhenMade_ThenItIsInsideTheBlock)
{
  // One allocation: the object is inside the memory the allocator returned.
  struct Spy
  {
    int value = 3;
  };
  AllocStats stats;
  sp::shared_ptr<Spy> p
      = sp::allocate_shared<Spy> (CountingAllocator<Spy> (&stats));
  EXPECT_EQ (p->value, 3);
  EXPECT_EQ (stats.allocations.load (), 1);
  typedef sp::detail::ctl_inplace<Spy, CountingAllocator<Spy>> block_type;
  EXPECT_EQ (stats.bytes.load (), static_cast<long> (sizeof (block_type)))
      << "the one allocation is the block";
  if (sizeof (void *) == 8)
    EXPECT_EQ (sizeof (block_type), 48u)
        << "header 32, the allocator 8, the object 4, padded to 8";
}

TEST (LumexMakeSharedTest, GivenEnableSharedFromThis_WhenMade_ThenItIsWired)
{
  struct Node : sp::enable_shared_from_this<Node>
  {
  };
  sp::shared_ptr<Node> p = sp::make_shared<Node> ();
  EXPECT_EQ (p->shared_from_this (), p);
  AllocStats stats;
  sp::shared_ptr<Node> q
      = sp::allocate_shared<Node> (CountingAllocator<Node> (&stats));
  EXPECT_EQ (q->shared_from_this (), q);
}

TEST (LumexMakeSharedTest,
      GivenTheSignatures_WhenChecked_ThenTheyMatchTheStandard)
{
  static_assert (std::is_same<decltype (sp::make_shared<int> (1)),
                              sp::shared_ptr<int>>::value,
                 "");
  static_assert (
      std::is_same<decltype (sp::allocate_shared<int> (
                       std::declval<std::allocator<int> const &> (), 1)),
                   sp::shared_ptr<int>>::value,
      "");
  SUCCEED ();
}
} // namespace
