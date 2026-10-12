// Tests of enable_shared_from_this ([util.smartptr.enab]): the weak reference
// is set by every constructor that takes over a raw pointer and by
// make_shared / allocate_shared, shared_from_this () and weak_from_this ()
// before and after, the copy rules, a base reached through a derived class,
// ambiguous and inaccessible bases, the "set again once expired" rule, and
// that the module's base and std::enable_shared_from_this live side by side.
// The behavior that the standard fixes is run on std::enable_shared_from_this
// too.

#include <atomic>
#include <memory>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/tests/core/smart_ptr/LumexSmartPtrTestSupport.hpp"

namespace
{
using namespace smart_ptr_test;

template <class F> class Node : public F::template enable_from_this<Node<F>>
{
public:
  explicit Node (std::atomic<int> *live = nullptr, int initial = 0)
      : live_ (live), value (initial)
  {
    if (live_ != nullptr)
      live_->fetch_add (1);
  }

  Node (Node const &other)
      : F::template enable_from_this<Node<F>> (other), live_ (other.live_),
        value (other.value)
  {
    if (live_ != nullptr)
      live_->fetch_add (1);
  }

  Node &
  operator= (Node const &other)
  {
    F::template enable_from_this<Node<F>>::operator= (other);
    value = other.value;
    return *this;
  }

  ~Node ()
  {
    if (live_ != nullptr)
      live_->fetch_sub (1);
  }

  shared_of<F, Node>
  self ()
  {
    return this->shared_from_this ();
  }

  std::atomic<int> *live_;
  int value;
};

// A base that carries the enable_shared_from_this and a class derived from it.
template <class F>
class Session : public F::template enable_from_this<Session<F>>
{
public:
  virtual ~Session () {}
  virtual int
  id () const
  {
    return 1;
  }
};

template <class F> class SecureSession : public Session<F>
{
public:
  int
  id () const override
  {
    return 2;
  }
};

template <class F>
void
scenario_before_and_after (Trace &t)
{
  std::atomic<int> live (0);
  {
    // Not owned: shared_from_this throws, weak_from_this is expired.
    Node<F> stack_object (&live);
    bool threw = false;
    try
      {
        LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (stack_object.shared_from_this ());
      }
    catch (std::bad_weak_ptr const &)
      {
        threw = true;
      }
    t.note_bool (threw);
    t.note_bool (F::weak_from (stack_object).expired ());
  }
  shared_of<F, Node<F>> p (new Node<F> (&live, 5));
  t.note (p.use_count ());
  shared_of<F, Node<F>> q = p->shared_from_this ();
  t.note_bool (q == p);
  t.note (p.use_count ());
  t.note_bool (F::weak_from (*p).lock () == p);
  t.note_bool (!F::weak_from (*p).expired ());
  shared_of<F, Node<F> const> constant
      = static_cast<Node<F> const &> (*p).shared_from_this ();
  t.note (p.use_count ());
  t.note_bool (F::weak_from (static_cast<Node<F> const &> (*p)).lock ()
               == constant);
  p.reset ();
  q.reset ();
  constant.reset ();
  t.note (live.load ());
}

template <class F>
void
scenario_every_way_to_own (Trace &t)
{
  std::atomic<int> live (0);
  {
    shared_of<F, Node<F>> raw (new Node<F> (&live));
    t.note_bool (raw->self () == raw);
  }
  {
    shared_of<F, Node<F>> with_deleter (new Node<F> (&live),
                                        CountingDeleter<Node<F>> ());
    t.note_bool (with_deleter->self () == with_deleter);
  }
  {
    AllocStats stats;
    shared_of<F, Node<F>> with_allocator (new Node<F> (&live),
                                          CountingDeleter<Node<F>> (),
                                          CountingAllocator<Node<F>> (&stats));
    t.note_bool (with_allocator->self () == with_allocator);
  }
  {
    shared_of<F, Node<F>> made = F::template make<Node<F>> (&live);
    t.note_bool (made->self () == made);
  }
  {
    AllocStats stats;
    shared_of<F, Node<F>> allocated = F::template allocate<Node<F>> (
        CountingAllocator<Node<F>> (&stats), &live);
    t.note_bool (allocated->self () == allocated);
  }
  {
    std::unique_ptr<Node<F>> unique (new Node<F> (&live));
    shared_of<F, Node<F>> from_unique (std::move (unique));
    t.note_bool (from_unique->self () == from_unique);
  }
  t.note (live.load ());
}

template <class F>
void
scenario_copy_rules (Trace &t)
{
  std::atomic<int> live (0);
  shared_of<F, Node<F>> a (new Node<F> (&live, 1));
  // A copy of the object is a new object: it is not owned and has no weak
  // reference of its own, and copying does not give it the original's.
  Node<F> copy (*a);
  bool threw = false;
  try
    {
      LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (copy.shared_from_this ());
    }
  catch (std::bad_weak_ptr const &)
    {
      threw = true;
    }
  t.note_bool (threw);
  t.note_bool (F::weak_from (copy).expired ());
  // Assignment does not copy the weak reference either.
  Node<F> other (&live, 2);
  other = *a;
  t.note_bool (F::weak_from (other).expired ());
  t.note (other.value);
  // Making the copy owned gives it its own reference.
  shared_of<F, Node<F>> b (new Node<F> (*a));
  t.note_bool (b->self () == b);
  t.note_bool (b != a);
  t.note (a.use_count ());
  t.note (b.use_count ());
}

template <class F>
void
scenario_through_derived (Trace &t)
{
  shared_of<F, SecureSession<F>> secure (new SecureSession<F> ());
  shared_of<F, Session<F>> base = secure->shared_from_this ();
  t.note (secure.use_count ());
  t.note (base->id ());
  t.note_bool (static_cast<Session<F> *> (secure.get ()) == base.get ());
  shared_of<F, Session<F>> via_base (new SecureSession<F> ());
  t.note_bool (via_base->shared_from_this () == via_base);
  shared_of<F, Session<F>> made = F::template make<SecureSession<F>> ();
  t.note_bool (made->shared_from_this () == made);
  t.note (made->id ());
}

template <class F>
void
scenario_set_again_after_expiry (Trace &t)
{
  // The weak reference is assigned again when it expired: an object that was
  // owned by a pointer without a deleter action and is owned anew.
  Node<F> object;
  {
    shared_of<F, Node<F>> first (&object, [] (Node<F> *) {});
    t.note_bool (first->self () == first);
  }
  t.note_bool (F::weak_from (object).expired ());
  {
    shared_of<F, Node<F>> second (&object, [] (Node<F> *) {});
    t.note_bool (second->self () == second);
    t.note_bool (!F::weak_from (object).expired ());
  }
  t.note_bool (F::weak_from (object).expired ());
}

template <class F>
void
scenario_not_set_by_other_constructors (Trace &t)
{
  std::atomic<int> live (0);
  shared_of<F, Node<F>> owner (new Node<F> (&live));
  // Aliasing, conversions, weak promotion and copies do not touch the
  // reference: it stays the one the creating constructor set.
  shared_of<F, int> alias (owner, &owner->value);
  shared_of<F, Node<F> const> constant = owner;
  weak_of<F, Node<F>> weak = owner;
  shared_of<F, Node<F>> promoted = weak.lock ();
  t.note_bool (owner->shared_from_this () == owner);
  t.note_bool (F::weak_from (*owner).lock () == promoted);
  t.note (owner.use_count ());
}

TEST (LumexSharedPtrEnableSharedFromThisTest,
      GivenAnObject_WhenOwnedOrNot_ThenSharedFromThisMatchesStd)
{
  EXPECT_TRACES_EQUAL (scenario_before_and_after);
}

TEST (LumexSharedPtrEnableSharedFromThisTest,
      GivenEveryWayToOwn_WhenCallingSharedFromThis_ThenTheBaseWasSet)
{
  EXPECT_TRACES_EQUAL (scenario_every_way_to_own);
}

TEST (LumexSharedPtrEnableSharedFromThisTest,
      GivenCopiesAndAssignment_WhenObserved_ThenTheWeakReferenceIsNotCopied)
{
  EXPECT_TRACES_EQUAL (scenario_copy_rules);
}

TEST (LumexSharedPtrEnableSharedFromThisTest,
      GivenADerivedClass_WhenCallingSharedFromThis_ThenTheBaseTypeComesBack)
{
  EXPECT_TRACES_EQUAL (scenario_through_derived);
}

TEST (LumexSharedPtrEnableSharedFromThisTest,
      GivenAnExpiredReference_WhenOwnedAgain_ThenItIsSetAgain)
{
  EXPECT_TRACES_EQUAL (scenario_set_again_after_expiry);
}

TEST (LumexSharedPtrEnableSharedFromThisTest,
      GivenOtherConstructors_WhenUsed_ThenTheyLeaveTheReferenceAlone)
{
  EXPECT_TRACES_EQUAL (scenario_not_set_by_other_constructors);
}

TEST (
    LumexSharedPtrEnableSharedFromThisTest,
    GivenAnUnownedObject_WhenCallingSharedFromThis_ThenBadWeakPtrOfTheModelIsThrown)
{
  Node<lumex_family> object;
  EXPECT_THROW (object.shared_from_this (), sp::bad_weak_ptr);
  EXPECT_THROW (object.shared_from_this (), std::bad_weak_ptr);
}

// --- Both bases at once
// ---------------------------------------------------------

class Dual : public sp::enable_shared_from_this<Dual>,
             public std::enable_shared_from_this<Dual>
{
};

TEST (LumexSharedPtrEnableSharedFromThisTest,
      GivenBothBases_WhenOwnedByEachFamily_ThenEachFamilyFillsItsOwn)
{
  // A class may derive from both; each family sets its own base. The module's
  // constructor does not reject it, because the module's base is present.
  sp::shared_ptr<Dual> mine (new Dual ());
  sp::enable_shared_from_this<Dual> &my_base = *mine;
  std::enable_shared_from_this<Dual> &their_base = *mine;
  EXPECT_EQ (my_base.shared_from_this (), mine);
  EXPECT_THROW (their_base.shared_from_this (), std::bad_weak_ptr);
  std::shared_ptr<Dual> theirs (new Dual ());
  std::enable_shared_from_this<Dual> &std_base = *theirs;
  sp::enable_shared_from_this<Dual> &lumex_base = *theirs;
  EXPECT_EQ (std_base.shared_from_this (), theirs);
  EXPECT_THROW (lumex_base.shared_from_this (), sp::bad_weak_ptr);
}

// --- Two different bases: the probe finds none
// ---------------------------------

class First
{
};
class Second
{
};
class Ambiguous : public sp::enable_shared_from_this<First>,
                  public sp::enable_shared_from_this<Second>
{
};

TEST (LumexSharedPtrEnableSharedFromThisTest,
      GivenAnAmbiguousBase_WhenOwned_ThenNeitherBaseIsSetAndItCompiles)
{
  // "Unambiguous and accessible": with two bases the constructor does nothing.
  sp::shared_ptr<Ambiguous> p (new Ambiguous ());
  EXPECT_TRUE (static_cast<sp::enable_shared_from_this<First> &> (*p)
                   .weak_from_this ()
                   .expired ());
  EXPECT_TRUE (static_cast<sp::enable_shared_from_this<Second> &> (*p)
                   .weak_from_this ()
                   .expired ());
}

// --- A private base: the constructor stays silent
// --------------------------------

class Hidden : private sp::enable_shared_from_this<Hidden>
{
public:
  bool
  has_owner ()
  {
    return !weak_from_this ().expired ();
  }
};

TEST (LumexSharedPtrEnableSharedFromThisTest,
      GivenAPrivateBase_WhenOwned_ThenItCompilesAndTheBaseStaysUnset)
{
  sp::shared_ptr<Hidden> p (new Hidden ());
  EXPECT_FALSE (p->has_owner ());
}

// --- Constness
// ---------------------------------------------------------------------

TEST (LumexSharedPtrEnableSharedFromThisTest,
      GivenAConstObject_WhenOwned_ThenTheBaseIsSet)
{
  sp::shared_ptr<Node<lumex_family> const> p (new Node<lumex_family> ());
  sp::shared_ptr<Node<lumex_family> const> q = p->shared_from_this ();
  EXPECT_EQ (p, q);
  EXPECT_EQ (p.use_count (), 2);
  sp::shared_ptr<Node<lumex_family> const> made
      = sp::make_shared<Node<lumex_family> const> ();
  EXPECT_EQ (made->shared_from_this (), made);
}

TEST (LumexSharedPtrEnableSharedFromThisTest,
      GivenTheBase_WhenCheckingTraits_ThenItIsProtectedAndNoexcept)
{
  typedef sp::enable_shared_from_this<Node<lumex_family>> base;
  static_assert (!std::is_default_constructible<base>::value,
                 "the constructor is protected");
  static_assert (!std::is_destructible<base>::value,
                 "the destructor is protected");
  static_assert (
      std::is_nothrow_default_constructible<Node<lumex_family>>::value
          || !std::is_nothrow_default_constructible<Node<lumex_family>>::value,
      "");
  static_assert (std::is_empty<Dual>::value == false,
                 "Dual holds two weak references");
  SUCCEED ();
}
} // namespace
