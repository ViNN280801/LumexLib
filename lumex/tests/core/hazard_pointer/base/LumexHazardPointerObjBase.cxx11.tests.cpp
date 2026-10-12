// Tests of hazard_pointer_obj_base<T, D> ([saferecl.hp.base]) and of the
// trait is_hazard_protectable ([saferecl.hp.general]): which classes are
// hazard-protectable, what the base costs, who runs the deleter and when, and
// that copies and assignments leave the retire state of an object alone.

#include <atomic>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/tests/core/hazard_pointer/LumexHazardPointerTestSupport.hpp"

namespace
{
using lumex_hp_test::counted_node;
using lumex_hp_test::counters_t;
namespace hp = lumex::core::hazard_pointer;
using hp::is_hazard_protectable;

// --- classes for the trait
// ---------------------------------------------------

struct good : hp::hazard_pointer_obj_base<good>
{
};

struct stateless_deleter
{
  void
  operator() (void *p) const
  {
    ++count ();
    ::operator delete (p);
  }

  static int &
  count ()
  {
    static int value = 0;
    return value;
  }
};

struct custom_deleter_node;

struct custom_deleter_node
    : hp::hazard_pointer_obj_base<custom_deleter_node,
                                  void (*) (custom_deleter_node *)>
{
};

struct no_base
{
};

struct derived_of_good : good
{
};

struct wrong_object : hp::hazard_pointer_obj_base<good>
{
};

struct first_deleter
{
  void
  operator() (struct two_bases *) const
  {
  }
};

struct second_deleter
{
  void
  operator() (struct two_bases *) const
  {
  }
};

struct two_bases : hp::hazard_pointer_obj_base<two_bases, first_deleter>,
                   hp::hazard_pointer_obj_base<two_bases, second_deleter>
{
};

struct private_base : private hp::hazard_pointer_obj_base<private_base>
{
};

struct virtual_base : virtual hp::hazard_pointer_obj_base<virtual_base>
{
};

struct protected_base : protected hp::hazard_pointer_obj_base<protected_base>
{
};

// --- deleters
// ----------------------------------------------------------------

struct counting_deleter
{
  template <class T>
  void
  operator() (T *p) const
  {
    ++calls ();
    delete p;
  }

  static int &
  calls ()
  {
    static int value = 0;
    return value;
  }
};

struct counting_node
    : hp::hazard_pointer_obj_base<counting_node, counting_deleter>
{
};

struct stateful_deleter
{
  int *calls;
  void **last_pointer;

  stateful_deleter () : calls (nullptr), last_pointer (nullptr) {}
  stateful_deleter (int *c, void **p) : calls (c), last_pointer (p) {}

  void operator() (struct stateful_node *p) const;
};

struct stateful_node
    : hp::hazard_pointer_obj_base<stateful_node, stateful_deleter>
{
};

void
stateful_deleter::operator() (stateful_node *p) const
{
  if (calls != nullptr)
    {
      ++*calls;
    }
  if (last_pointer != nullptr)
    {
      *last_pointer = p;
    }
  delete p;
}

struct final_deleter final
{
  void operator() (struct final_node *p) const;
};

struct final_node : hp::hazard_pointer_obj_base<final_node, final_deleter>
{
};

int g_final_calls = 0;

void
final_deleter::operator() (final_node *p) const
{
  ++g_final_calls;
  delete p;
}

struct padding_t
{
  long pad[3];
};

struct offset_node : padding_t, hp::hazard_pointer_obj_base<offset_node>
{
  ~offset_node () { seen_destroyed () = true; }

  static bool &
  seen_destroyed ()
  {
    static bool value = false;
    return value;
  }
};

struct leading_node : hp::hazard_pointer_obj_base<leading_node>, padding_t
{
};

void
delete_function_node (custom_deleter_node *p)
{
  delete p;
}
} // namespace

// --- the trait
// ---------------------------------------------------------------

TEST (LumexHazardPointerObjBaseTest,
      GivenClasses_WhenAskingIsHazardProtectable_ThenOnlyTheRightOnesAre)
{
  static_assert (is_hazard_protectable<good>::value, "one public base");
  static_assert (is_hazard_protectable<good const>::value, "cv is ignored");
  static_assert (is_hazard_protectable<good volatile>::value, "cv is ignored");
  static_assert (is_hazard_protectable<counting_node>::value, "own deleter");
  static_assert (is_hazard_protectable<stateful_node>::value, "state");
  static_assert (is_hazard_protectable<final_node>::value, "final deleter");
  static_assert (is_hazard_protectable<custom_deleter_node>::value,
                 "function pointer deleter");
  static_assert (is_hazard_protectable<offset_node>::value, "offset base");
  static_assert (is_hazard_protectable<leading_node>::value, "leading base");
  static_assert (!is_hazard_protectable<no_base>::value, "no base");
  static_assert (!is_hazard_protectable<derived_of_good>::value,
                 "the base names another object type");
  static_assert (!is_hazard_protectable<wrong_object>::value,
                 "the base names another object type");
  static_assert (!is_hazard_protectable<two_bases>::value,
                 "two bases of the same object type");
  static_assert (!is_hazard_protectable<private_base>::value, "private base");
  static_assert (!is_hazard_protectable<protected_base>::value,
                 "protected base");
  static_assert (!is_hazard_protectable<virtual_base>::value, "virtual base");
  static_assert (!is_hazard_protectable<int>::value, "not a class");
  static_assert (!is_hazard_protectable<void>::value, "void");
  static_assert (!is_hazard_protectable<good *>::value, "a pointer");
  static_assert (
      !is_hazard_protectable<hp::hazard_pointer_obj_base<good>>::value,
      "the base alone is not the object");
  SUCCEED ();
}

TEST (
    LumexHazardPointerObjBaseTest,
    GivenTheBase_WhenCheckingMembers_ThenRetireIsNoexceptAndSpecialMembersAreProtected)
{
  static_assert (noexcept (std::declval<good &> ().retire ()), "noexcept");
  static_assert (
      noexcept (std::declval<good &> ().retire (std::default_delete<good> ())),
      "noexcept with a deleter");
  static_assert (std::is_copy_constructible<good>::value,
                 "a derived class is copyable");
  static_assert (std::is_move_constructible<good>::value, "and movable");
  static_assert (std::is_copy_assignable<good>::value, "assignable");
  static_assert (std::is_nothrow_default_constructible<good>::value,
                 "default constructible");
  static_assert (
      !std::is_default_constructible<hp::hazard_pointer_obj_base<good>>::value,
      "the base constructor is protected");
  static_assert (
      !std::is_copy_constructible<hp::hazard_pointer_obj_base<good>>::value,
      "the base copy constructor is protected");
  static_assert (
      !std::is_destructible<hp::hazard_pointer_obj_base<good>>::value,
      "the base destructor is protected");
  SUCCEED ();
}

TEST (LumexHazardPointerObjBaseTest,
      GivenDeleterKinds_WhenMeasuringTheBase_ThenAnEmptyDeleterCostsNothing)
{
  std::size_t const word = sizeof (void *);
  EXPECT_EQ (sizeof (good), 2 * word) << "node only";
  EXPECT_EQ (sizeof (counting_node), 2 * word) << "empty deleter";
  EXPECT_EQ (sizeof (stateful_node), 2 * word + sizeof (stateful_deleter))
      << "two pointers of state";
  EXPECT_EQ (sizeof (custom_deleter_node), 3 * word)
      << "a function pointer is state";
  EXPECT_GE (sizeof (final_node), 2 * word + 1)
      << "a final deleter cannot be a base";
}

// --- who deletes, and when
// ---------------------------------------------------

TEST (LumexHazardPointerObjBaseTest,
      GivenAnUnprotectedObject_WhenRetiredAndCleanedUp_ThenDeletedOnce)
{
  counters_t counters;
  (new counted_node (counters, 1))->retire ();
  hp::clean_up ();
  EXPECT_EQ (counters.constructed.load (), 1);
  EXPECT_EQ (counters.deleted.load (), 1);
}

TEST (LumexHazardPointerObjBaseTest,
      GivenAStatelessDeleter_WhenRetired_ThenItRunsOnce)
{
  counting_deleter::calls () = 0;
  (new counting_node)->retire ();
  hp::clean_up ();
  EXPECT_EQ (counting_deleter::calls (), 1);
}

TEST (LumexHazardPointerObjBaseTest,
      GivenADeleterArgument_WhenRetired_ThenItReplacesTheStoredDeleter)
{
  int calls = 0;
  void *seen = nullptr;
  stateful_node *node = new stateful_node;
  void *address = node;
  node->retire (stateful_deleter (&calls, &seen));
  hp::clean_up ();
  EXPECT_EQ (calls, 1);
  EXPECT_EQ (seen, address) << "the deleter gets the whole object";
}

TEST (LumexHazardPointerObjBaseTest, GivenAFinalDeleter_WhenRetired_ThenItRuns)
{
  g_final_calls = 0;
  (new final_node)->retire ();
  hp::clean_up ();
  EXPECT_EQ (g_final_calls, 1);
}

TEST (LumexHazardPointerObjBaseTest,
      GivenAFunctionPointerDeleter_WhenRetired_ThenItRuns)
{
  (new custom_deleter_node)->retire (&delete_function_node);
  hp::clean_up ();
  SUCCEED () << "no leak report is the check";
}

TEST (
    LumexHazardPointerObjBaseTest,
    GivenABaseAtANonzeroOffset_WhenProtectedAndRetired_ThenDeletedAfterTheProtectionEnds)
{
  offset_node::seen_destroyed () = false;
  offset_node *node = new offset_node;
  std::atomic<offset_node *> source (node);
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  offset_node *got = holder.protect (source);
  ASSERT_EQ (got, node);
  source.store (nullptr);
  node->retire ();
  hp::clean_up ();
  EXPECT_FALSE (offset_node::seen_destroyed ())
      << "protected through the address of the object, not of the base";
  holder.reset_protection ();
  hp::clean_up ();
  EXPECT_TRUE (offset_node::seen_destroyed ());
}

TEST (
    LumexHazardPointerObjBaseTest,
    GivenALeadingBase_WhenProtectedAndRetired_ThenDeletedAfterTheProtectionEnds)
{
  leading_node *node = new leading_node;
  std::atomic<leading_node *> source (node);
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  ASSERT_EQ (holder.protect (source), node);
  source.store (nullptr);
  std::size_t const before = hp::engine::statistics ().reclaimed;
  node->retire ();
  hp::clean_up ();
  EXPECT_EQ (hp::engine::statistics ().reclaimed, before);
  holder.reset_protection ();
  hp::clean_up ();
  EXPECT_EQ (hp::engine::statistics ().reclaimed, before + 1);
}

// --- copies and assignments
// --------------------------------------------------

TEST (LumexHazardPointerObjBaseTest,
      GivenARetiredObject_WhenCopied_ThenTheCopyIsNotRetired)
{
  counters_t counters;
  counted_node *original = new counted_node (counters, 5);
  std::atomic<counted_node *> source (original);
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  ASSERT_EQ (holder.protect (source), original);
  original->retire ();
  hp::clean_up ();
  ASSERT_EQ (counters.deleted.load (), 0) << "protected";

  counted_node *copy = new counted_node (*original);
  copy->retire ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 1) << "the copy was retired once";
  EXPECT_EQ (original->value, 5) << "the protected original is untouched";

  holder.reset_protection ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 2);
}

TEST (LumexHazardPointerObjBaseTest,
      GivenARetiredObject_WhenAnotherIsAssignedToIt_ThenItStaysRetiredOnce)
{
  counters_t counters;
  counted_node *pending = new counted_node (counters, 1);
  counted_node *plain = new counted_node (counters, 2);
  std::atomic<counted_node *> source (pending);
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  ASSERT_EQ (holder.protect (source), pending);
  pending->retire ();
  hp::clean_up ();
  *pending = *plain;
  EXPECT_EQ (pending->value, 2);
  holder.reset_protection ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 1) << "only the retired object";
  plain->retire ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 2);
}

TEST (LumexHazardPointerObjBaseTest,
      GivenAnObject_WhenAssignedFromARetiredOne_ThenItCanStillBeRetired)
{
  counters_t counters;
  counted_node *pending = new counted_node (counters, 1);
  counted_node *plain = new counted_node (counters, 2);
  std::atomic<counted_node *> source (pending);
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  ASSERT_EQ (holder.protect (source), pending);
  pending->retire ();
  hp::clean_up ();
  *plain = *pending;
  plain->retire ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 1) << "plain, not pending";
  holder.reset_protection ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 2);
}

// --- reentrancy
// ----------------------------------------------------------------

namespace
{
struct chain_node;

struct chain_deleter
{
  void operator() (chain_node *p) const;
};

struct chain_node : hp::hazard_pointer_obj_base<chain_node, chain_deleter>
{
  chain_node *next;
  int *deleted;
  chain_node (chain_node *the_next, int *the_deleted)
      : next (the_next), deleted (the_deleted)
  {
  }
};

void
chain_deleter::operator() (chain_node *p) const
{
  // A deleter may use hazard pointers and retire more objects, and may ask
  // for another pass.
  chain_node *next = p->next;
  ++*p->deleted;
  delete p;
  if (next != nullptr)
    {
      hp::hazard_pointer holder = hp::make_hazard_pointer ();
      std::atomic<chain_node *> source (next);
      chain_node *protected_next = holder.protect (source);
      hp::clean_up ();
      protected_next->retire ();
    }
}
} // namespace

TEST (
    LumexHazardPointerObjBaseTest,
    GivenADeleterThatRetiresAndCleansUp_WhenAPassRuns_ThenTheWholeChainGoesAndNothingHangs)
{
  int deleted = 0;
  chain_node *head = nullptr;
  for (int i = 0; i < 20; ++i)
    {
      head = new chain_node (head, &deleted);
    }
  head->retire ();
  hp::clean_up ();
  EXPECT_EQ (deleted, 20);
}
