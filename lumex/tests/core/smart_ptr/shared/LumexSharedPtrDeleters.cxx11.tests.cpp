// Tests of the deleters and of get_deleter ([util.smartptr.shared.dest],
// [util.smartptr.getdeleter]): when a deleter runs and with what, that it is
// destroyed with the control block (not with the object), that aliases and
// converted pointers keep it, which kinds of deleter are stored (function
// pointer, function object, lambda, std::function, reference wrapper,
// move-only), the empty-base optimization of the control block, and the
// exact set of calls the destructor ledger records.

#include <atomic>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/tests/core/smart_ptr/LumexSmartPtrTestSupport.hpp"

namespace
{
using namespace smart_ptr_test;

// A deleter that reports its own construction, copy, move and destruction.
class LifeDeleter
{
public:
  struct Counts
  {
    std::atomic<int> constructed{ 0 };
    std::atomic<int> copied{ 0 };
    std::atomic<int> moved{ 0 };
    std::atomic<int> destroyed{ 0 };
    std::atomic<int> called{ 0 };
  };

  explicit LifeDeleter (Counts *counts) : counts_ (counts)
  {
    counts_->constructed.fetch_add (1);
  }
  LifeDeleter (LifeDeleter const &other) : counts_ (other.counts_)
  {
    counts_->copied.fetch_add (1);
    counts_->constructed.fetch_add (1);
  }
  LifeDeleter (LifeDeleter &&other) noexcept : counts_ (other.counts_)
  {
    counts_->moved.fetch_add (1);
    counts_->constructed.fetch_add (1);
  }
  ~LifeDeleter () { counts_->destroyed.fetch_add (1); }
  LifeDeleter &operator= (LifeDeleter const &) = delete;

  void
  operator() (int *p) const
  {
    counts_->called.fetch_add (1);
    delete p;
  }

private:
  Counts *counts_;
};

void
function_deleter (int *p)
{
  delete p;
}

template <class F>
void
scenario_get_deleter (Trace &t)
{
  std::atomic<int> calls (0);
  shared_of<F, int> with (new int (1), CountingDeleter<int> (&calls));
  CountingDeleter<int> *found
      = F::template get_deleter<CountingDeleter<int>> (with);
  t.note_bool (found != nullptr);
  if (found != nullptr)
    t.note_bool (found->counter () == &calls);
  // The wrong type, an empty pointer, a null-owner alias.
  t.note_bool (F::template get_deleter<CountingDeleter<double>> (with)
               == nullptr);
  t.note_bool (F::template get_deleter<int> (with) == nullptr);
  t.note_bool (
      F::template get_deleter<CountingDeleter<int>> (shared_of<F, int> ())
      == nullptr);
  // A copy, an alias and a converted pointer share the deleter.
  shared_of<F, int> copy = with;
  t.note_bool (F::template get_deleter<CountingDeleter<int>> (copy) == found);
  shared_of<F, int> alias (with, new int (0));
  t.note_bool (F::template get_deleter<CountingDeleter<int>> (alias) == found);
  delete alias.get ();
  shared_of<F, void> erased = with;
  t.note_bool (F::template get_deleter<CountingDeleter<int>> (erased)
               == found);
  // A pointer created without a deleter answers null.
  shared_of<F, int> plain (new int (2));
  t.note_bool (F::template get_deleter<std::default_delete<int>> (plain)
               == nullptr);
  // A function-pointer deleter is stored as that type.
  shared_of<F, int> by_function (new int (3), &function_deleter);
  typedef void (*function_type) (int *);
  function_type *stored = F::template get_deleter<function_type> (by_function);
  t.note_bool (stored != nullptr);
  if (stored != nullptr)
    t.note_bool (*stored == &function_deleter);
  // A std::function deleter.
  std::function<void (int *)> as_function = &function_deleter;
  shared_of<F, int> by_std_function (new int (4), as_function);
  t.note_bool (
      F::template get_deleter<std::function<void (int *)>> (by_std_function)
      != nullptr);
  // The deleter state is the one that runs.
  found->operator() (new int (0));
  t.note (calls.load ());
}

template <class F>
void
scenario_deleter_life (Trace &t)
{
  LifeDeleter::Counts counts;
  {
    shared_of<F, int> p (new int (1), LifeDeleter (&counts));
    // How often the library moves the deleter around is its own business;
    // what matters is that every constructed copy is destroyed in the end.
    shared_of<F, int> q = p;
    t.note (counts.copied.load ());
  }
  t.note (counts.called.load ());
  t.note (counts.constructed.load () - counts.destroyed.load ());
}

template <class F>
void
scenario_deleter_outlives_object (Trace &t)
{
  // With a weak_ptr alive the block stays: the object is deleted by the
  // deleter when the last owner goes, the deleter object is destroyed with
  // the block, when the last weak pointer goes.
  LifeDeleter::Counts counts;
  weak_of<F, int> weak;
  {
    shared_of<F, int> p (new int (1), LifeDeleter (&counts));
    weak = p;
  }
  // Only the call is compared: when the deleter object itself is destroyed
  // is up to the library (libstdc++ keeps it with the block, libc++ ends it
  // with the object); the module's own rule is tested separately.
  t.note (counts.called.load ());
  weak.reset ();
}

template <class F>
void
scenario_reference_wrapper (Trace &t)
{
  LifeDeleter::Counts counts;
  LifeDeleter external (&counts);
  {
    shared_of<F, int> p (new int (1), std::ref (external));
    t.note (counts.copied.load ());
  }
  t.note (counts.called.load ());
  t.note (counts.destroyed.load ());
}

template <class F>
void
scenario_deleter_order (Trace &t)
{
  // The deleter runs before the object's memory is reused and exactly once,
  // whatever the order in which several owners go.
  std::atomic<int> calls (0);
  for (int order = 0; order < 6; ++order)
    {
      shared_of<F, int> a (new int (1), CountingDeleter<int> (&calls));
      shared_of<F, int> b = a;
      shared_of<F, int> c = b;
      shared_of<F, int> *list[3] = { &a, &b, &c };
      int const permutations[6][3] = { { 0, 1, 2 }, { 0, 2, 1 }, { 1, 0, 2 },
                                       { 1, 2, 0 }, { 2, 0, 1 }, { 2, 1, 0 } };
      for (int step = 0; step < 3; ++step)
        {
          list[permutations[order][step]]->reset ();
          t.note (calls.load ());
        }
    }
}

template <class F>
void
scenario_deleter_exceptions_are_not_needed (Trace &t)
{
  // A deleter that is a lambda capturing by value and a mutable one: the
  // stored copy is the one that is called.
  int base = 10;
  int seen = 0;
  {
    shared_of<F, int> p (new int (5),
                         [base, &seen] (int *x)
                           {
                             seen = base + *x;
                             delete x;
                           });
    base = 99; // the lambda captured 10
  }
  t.note (seen);
  int counter = 0;
  {
    shared_of<F, int> q (new int (1),
                         [counter] (int *x) mutable
                           {
                             ++counter;
                             delete x;
                           });
  }
  t.note (counter);
}

TEST (LumexSharedPtrDeletersTest, GivenGetDeleter_WhenQueried_ThenItMatchesStd)
{
  EXPECT_TRACES_EQUAL (scenario_get_deleter);
}

TEST (LumexSharedPtrDeletersTest,
      GivenADeleter_WhenStored_ThenItIsMovedInAndCalledOnce)
{
  EXPECT_TRACES_EQUAL (scenario_deleter_life);
  LifeDeleter::Counts counts;
  {
    sp::shared_ptr<int> p (new int (1), LifeDeleter (&counts));
    EXPECT_EQ (counts.copied.load (), 0) << "an rvalue deleter must be moved";
    EXPECT_EQ (counts.moved.load (), 1);
  }
  EXPECT_EQ (counts.called.load (), 1);
  EXPECT_EQ (counts.constructed.load (), counts.destroyed.load ());
}

TEST (LumexSharedPtrDeletersTest,
      GivenAWeakPtr_WhenTheOwnersGo_ThenTheDeleterObjectGoesWithTheBlock)
{
  EXPECT_TRACES_EQUAL (scenario_deleter_outlives_object);
  LifeDeleter::Counts counts;
  sp::weak_ptr<int> weak;
  {
    sp::shared_ptr<int> p (new int (1), LifeDeleter (&counts));
    weak = p;
  }
  EXPECT_EQ (counts.called.load (), 1);
  EXPECT_EQ (counts.constructed.load () - counts.destroyed.load (), 1)
      << "the deleter object must live as long as the control block";
  weak.reset ();
  EXPECT_EQ (counts.constructed.load (), counts.destroyed.load ());
}

TEST (LumexSharedPtrDeletersTest,
      GivenAReferenceWrapper_WhenStored_ThenTheExternalDeleterRuns)
{
  EXPECT_TRACES_EQUAL (scenario_reference_wrapper);
}

TEST (LumexSharedPtrDeletersTest,
      GivenSeveralOwners_WhenTheyGoInAnyOrder_ThenTheDeleterRunsOnceAtTheLast)
{
  EXPECT_TRACES_EQUAL (scenario_deleter_order);
}

TEST (LumexSharedPtrDeletersTest,
      GivenCapturingLambdas_WhenStored_ThenTheStoredCopyIsCalled)
{
  EXPECT_TRACES_EQUAL (scenario_deleter_exceptions_are_not_needed);
}

TEST (LumexSharedPtrDeletersTest,
      GivenTheDefaultDelete_WhenQueried_ThenNoDeleterIsReported)
{
  sp::shared_ptr<int> plain (new int (1));
  EXPECT_EQ (sp::get_deleter<std::default_delete<int>> (plain), nullptr);
  sp::shared_ptr<int> made = sp::make_shared<int> (1);
  EXPECT_EQ (sp::get_deleter<std::default_delete<int>> (made), nullptr);
  EXPECT_EQ (sp::get_deleter<sp::detail::default_deleter<int>> (plain),
             nullptr)
      << "the implicit delete is not an observable deleter";
}

TEST (LumexSharedPtrDeletersTest,
      GivenTheNullptrForm_WhenQueried_ThenTheDeleterIsFound)
{
  std::atomic<int> calls (0);
  sp::shared_ptr<int> p (nullptr, CountingDeleter<int> (&calls));
  EXPECT_NE (sp::get_deleter<CountingDeleter<int>> (p), nullptr);
  EXPECT_EQ (p.use_count (), 1);
  EXPECT_EQ (p.get (), nullptr);
}

TEST (LumexSharedPtrDeletersTest,
      GivenAMoveOnlyDeleter_WhenQueried_ThenItIsFound)
{
  std::atomic<int> calls (0);
  sp::shared_ptr<int> p (new int (1), MoveOnlyDeleter (&calls));
  EXPECT_NE (sp::get_deleter<MoveOnlyDeleter> (p), nullptr);
  p.reset ();
  EXPECT_EQ (calls.load (), 1);
}

TEST (LumexSharedPtrDeletersTest,
      GivenArrays_WhenOwnedByThePointerForm_ThenDeleteBracketsRuns)
{
  std::atomic<int> live (0);
  {
    // shared_ptr<Animal[]> is the C++17 form; the block of a pointer-to-
    // element constructor with an array type uses delete[].
    sp::shared_ptr<Animal[]> array (
        new Animal[3]{ Animal (&live), Animal (&live), Animal (&live) });
    EXPECT_EQ (live.load (), 3);
  }
  EXPECT_EQ (live.load (), 0);
}

TEST (LumexSharedPtrDeletersTest,
      GivenTheBlockSize_WhenTheDeleterIsEmpty_ThenItAddsNoBytes)
{
  // The control block with an empty deleter and the stateless allocator is as
  // small as the one without a deleter: the pair is compressed. Measured
  // through the allocator that sees the size of the block it allocates.
  AllocStats empty_stats;
  AllocStats counting_stats;
  std::atomic<int> calls (0);
  {
    sp::shared_ptr<int> a (new int (1), EmptyDeleter (),
                           CountingAllocator<int> (&empty_stats));
    sp::shared_ptr<int> b (new int (1), CountingDeleter<int> (&calls),
                           CountingAllocator<int> (&counting_stats));
  }
  long const with_empty = empty_stats.bytes.load ();
  long const with_state = counting_stats.bytes.load ();
  // 32 bytes of control block + the pointer; the CountingAllocator is one
  // pointer, the stateful deleter another.
  EXPECT_EQ (with_empty, 32 + 8 + 8);
  EXPECT_EQ (with_state, 32 + 8 + 8 + 8);
}
} // namespace
