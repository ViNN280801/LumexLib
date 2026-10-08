// LumexCastBehavior.cxx11.tests.cpp
//
// What downcast and downcast_noexcept do at run time beyond the basic cases
// of LumexCast.cxx11.tests.cpp: virtual and multiple inheritance (the pointer
// is adjusted), the dynamic type against the static type, the text of
// bad_down_cast for both forms, abstract and non-copyable classes, volatile
// objects, and that the noexcept variant neither throws nor builds an
// exception. Which calls exist at all is in
// LumexCastConstraints.cxx11.tests.cpp.
#include <cstddef>
#include <exception>
#include <string>
#include <typeinfo>

#include <gtest/gtest.h>

#include "lumex/core/utility/attr/LumexAttributes.hpp"
#include "lumex/core/utility/cast/LumexCast.hpp"

using lumex::core::utility::cast::bad_down_cast;
using lumex::core::utility::cast::downcast;
using lumex::core::utility::cast::downcast_noexcept;

namespace
{
struct animal
{
  virtual ~animal () {}
  virtual int
  legs () const
  {
    return 0;
  }
};

struct dog : animal
{
  int
  legs () const override
  {
    return 4;
  }
};

struct bird : animal
{
  int
  legs () const override
  {
    return 2;
  }
};

struct puppy : dog
{
  int
  legs () const override
  {
    return 4;
  }
};

// Two unrelated polymorphic bases of one class: the base subobjects sit at
// different addresses.
struct swimmer
{
  virtual ~swimmer () {}
  long stroke = 7;
};

struct runner
{
  virtual ~runner () {}
  long stride = 9;
};

struct triathlete : swimmer, runner
{
  long bike = 11;
};

// A virtual base cannot be reached with static_cast, only with dynamic_cast.
struct root
{
  virtual ~root () {}
  int tag = 5;
};

struct branch_a : virtual root
{
  int a = 1;
};

struct branch_b : virtual root
{
  int b = 2;
};

struct joined : branch_a, branch_b
{
  int c = 3;
};

struct shape
{
  virtual double area () const = 0;
  virtual ~shape () {}
};

struct square : shape
{
  double
  area () const override
  {
    return 4.0;
  }
};

struct circle : shape
{
  double
  area () const override
  {
    return 3.0;
  }
};

// A non-copyable, non-movable polymorphic base: only references and pointers
// are used.
struct pinned
{
  pinned () {}
  pinned (pinned const &) = delete;
  pinned &operator= (pinned const &) = delete;
  virtual ~pinned () {}
};

struct pinned_derived : pinned
{
  int payload = 42;
};

bool
contains (std::string const &text, std::string const &part)
{
  return text.find (part) != std::string::npos;
}
} // namespace

// --- dynamic type against static type -----------------------------------

TEST (LumexCastBehaviorTest,
      GivenBaseObject_WhenDowncastToDerived_ThenThrowsAndNoexceptIsNull)
{
  animal plain;
  animal *as_base = &plain;
  EXPECT_THROW (LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (downcast<dog *> (as_base)),
                bad_down_cast);
  EXPECT_EQ (downcast_noexcept<dog *> (as_base), nullptr);
  animal &ref = plain;
  EXPECT_THROW (LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (downcast<dog &> (ref)),
                bad_down_cast);
}

TEST (LumexCastBehaviorTest, GivenSameClass_WhenDowncast_ThenIdentity)
{
  animal plain;
  animal *as_base = &plain;
  EXPECT_EQ (downcast<animal *> (as_base), &plain);
  EXPECT_EQ (downcast_noexcept<animal *> (as_base), &plain);
  animal &ref = plain;
  EXPECT_EQ (&downcast<animal &> (ref), &plain);
}

TEST (LumexCastBehaviorTest,
      GivenDeeperObject_WhenDowncastToEveryClassOnTheChain_ThenSucceeds)
{
  puppy pup;
  animal *as_animal = &pup;
  EXPECT_EQ (downcast<animal *> (as_animal), &pup);
  EXPECT_EQ (downcast<dog *> (as_animal), &pup);
  EXPECT_EQ (downcast<puppy *> (as_animal), &pup);
  EXPECT_THROW (
      LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (downcast<bird *> (as_animal)),
      bad_down_cast);

  dog *as_dog = &pup;
  EXPECT_EQ (downcast<puppy *> (as_dog), &pup);
}

TEST (LumexCastBehaviorTest,
      GivenDogUnderDogPointer_WhenDowncastToPuppy_ThenThrows)
{
  dog adult;
  dog *as_dog = &adult;
  EXPECT_THROW (LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (downcast<puppy *> (as_dog)),
                bad_down_cast);
  EXPECT_EQ (downcast_noexcept<puppy *> (as_dog), nullptr);
}

TEST (LumexCastBehaviorTest,
      GivenSiblingObject_WhenDowncast_ThenSiblingsDiffer)
{
  bird sparrow;
  dog hound;
  animal *first = &sparrow;
  animal *second = &hound;
  EXPECT_EQ (downcast<bird *> (first), &sparrow);
  EXPECT_EQ (downcast<dog *> (second), &hound);
  EXPECT_EQ (downcast_noexcept<dog *> (first), nullptr);
  EXPECT_EQ (downcast_noexcept<bird *> (second), nullptr);
}

TEST (LumexCastBehaviorTest,
      GivenNullPointers_WhenDowncast_ThenEveryFormGivesNull)
{
  animal *none = nullptr;
  animal const *none_const = nullptr;
  EXPECT_EQ (downcast<dog *> (none), nullptr);
  EXPECT_EQ (downcast<dog const *> (none), nullptr);
  EXPECT_EQ (downcast<dog const *> (none_const), nullptr);
  EXPECT_EQ (downcast_noexcept<dog *> (none), nullptr);
  EXPECT_EQ (downcast_noexcept<dog const *> (none_const), nullptr);
  EXPECT_NO_THROW (LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (downcast<dog *> (none)));
}

// --- multiple and virtual inheritance -----------------------------------

TEST (LumexCastBehaviorTest,
      GivenSecondBase_WhenDowncast_ThenPointerIsAdjustedToTheWholeObject)
{
  triathlete athlete;
  runner *as_runner = &athlete;
  swimmer *as_swimmer = &athlete;
  // The base subobjects are not at the address of the object.
  ASSERT_NE (static_cast<void *> (as_runner), static_cast<void *> (&athlete));

  triathlete *from_runner = downcast<triathlete *> (as_runner);
  triathlete *from_swimmer = downcast<triathlete *> (as_swimmer);
  EXPECT_EQ (from_runner, &athlete);
  EXPECT_EQ (from_swimmer, &athlete);
  EXPECT_EQ (from_runner->bike, 11L);
  EXPECT_EQ (from_runner->stride, 9L);
  EXPECT_EQ (from_swimmer->stroke, 7L);

  triathlete &from_ref = downcast<triathlete &> (*as_runner);
  EXPECT_EQ (&from_ref, &athlete);
}

TEST (LumexCastBehaviorTest, GivenVirtualBase_WhenDowncast_ThenFindsTheObject)
{
  joined whole;
  root *as_root = &whole;
  EXPECT_EQ (downcast<joined *> (as_root), &whole);
  EXPECT_EQ (downcast<branch_a *> (as_root), static_cast<branch_a *> (&whole));
  EXPECT_EQ (downcast<branch_b *> (as_root), static_cast<branch_b *> (&whole));
  EXPECT_EQ (&downcast<joined &> (*as_root), &whole);
  EXPECT_EQ (downcast<joined *> (as_root)->c, 3);

  branch_a *as_a = &whole;
  EXPECT_EQ (downcast<joined *> (as_a), &whole);
  EXPECT_EQ (downcast_noexcept<joined *> (as_a), &whole);
}

TEST (LumexCastBehaviorTest,
      GivenVirtualBaseOfOtherBranch_WhenDowncast_ThenThrows)
{
  branch_a only_a;
  root *as_root = &only_a;
  EXPECT_THROW (
      LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (downcast<branch_b *> (as_root)),
      bad_down_cast);
  EXPECT_THROW (
      LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (downcast<joined &> (*as_root)),
      bad_down_cast);
  EXPECT_EQ (downcast_noexcept<branch_b *> (as_root), nullptr);
  EXPECT_EQ (downcast<branch_a *> (as_root), &only_a);
}

// --- abstract, non-copyable and volatile --------------------------------

TEST (LumexCastBehaviorTest,
      GivenAbstractBase_WhenDowncast_ThenWorksByReference)
{
  square tile;
  shape &as_shape = tile;
  EXPECT_EQ (&downcast<square &> (as_shape), &tile);
  EXPECT_DOUBLE_EQ (downcast<square &> (as_shape).area (), 4.0);
  EXPECT_THROW (
      LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (downcast<circle &> (as_shape)),
      bad_down_cast);

  shape *as_ptr = &tile;
  EXPECT_EQ (downcast<square *> (as_ptr), &tile);
  EXPECT_EQ (downcast_noexcept<circle *> (as_ptr), nullptr);
}

TEST (LumexCastBehaviorTest,
      GivenNonCopyableBase_WhenDowncast_ThenNothingIsCopied)
{
  pinned_derived object;
  pinned &as_base = object;
  pinned_derived &back = downcast<pinned_derived &> (as_base);
  EXPECT_EQ (&back, &object);
  EXPECT_EQ (back.payload, 42);
  pinned *as_ptr = &object;
  EXPECT_EQ (downcast<pinned_derived *> (as_ptr), &object);
}

TEST (LumexCastBehaviorTest,
      GivenVolatileObject_WhenDowncast_ThenKeepsVolatile)
{
  dog hound;
  animal volatile *as_base = &hound;
  dog volatile *back = downcast<dog volatile *> (as_base);
  EXPECT_EQ (static_cast<void *> (const_cast<dog *> (back)),
             static_cast<void *> (&hound));
  animal volatile &ref = hound;
  dog volatile &back_ref = downcast<dog volatile &> (ref);
  EXPECT_EQ (static_cast<void *> (const_cast<dog *> (&back_ref)),
             static_cast<void *> (&hound));
  animal volatile *wrong = &hound;
  EXPECT_TRUE (downcast_noexcept<puppy volatile *> (wrong) == nullptr);
}

TEST (LumexCastBehaviorTest, GivenConstObject_WhenDowncast_ThenKeepsConst)
{
  dog const hound;
  animal const *as_base = &hound;
  dog const *back = downcast<dog const *> (as_base);
  EXPECT_EQ (back, &hound);
  EXPECT_EQ (back->legs (), 4);
  animal const &ref = hound;
  EXPECT_EQ (&downcast<dog const &> (ref), &hound);
  EXPECT_EQ (downcast_noexcept<puppy const *> (as_base), nullptr);
}

TEST (LumexCastBehaviorTest,
      GivenNonConstSource_WhenDowncastToConst_ThenAddsConst)
{
  dog hound;
  animal *as_base = &hound;
  dog const *back = downcast<dog const *> (as_base);
  EXPECT_EQ (back, &hound);
  animal &ref = hound;
  dog const &back_ref = downcast<dog const &> (ref);
  EXPECT_EQ (&back_ref, &hound);
}

TEST (LumexCastBehaviorTest, GivenRvaluePointer_WhenDowncast_ThenWorks)
{
  dog hound;
  EXPECT_EQ (downcast<dog *> (static_cast<animal *> (&hound)), &hound);
  EXPECT_EQ (downcast_noexcept<dog *> (static_cast<animal *> (&hound)),
             &hound);
  EXPECT_EQ (downcast<dog *> (static_cast<animal *> (nullptr)), nullptr);
}

// --- the exception ------------------------------------------------------

TEST (LumexCastBehaviorTest,
      GivenFailedPointerCast_WhenWhatCalled_ThenNamesDynamicAndTargetType)
{
  bird sparrow;
  animal *as_base = &sparrow;
  try
    {
      LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (downcast<dog *> (as_base));
      FAIL () << "bad_down_cast expected";
    }
  catch (bad_down_cast const &error)
    {
      std::string const text = error.what ();
      // The dynamic type (bird), not the static one (animal), then the target.
      EXPECT_EQ (text, std::string ("downcast failed: dynamic type '")
                           + typeid (bird).name () + "' is not a '"
                           + typeid (dog).name () + "'");
      EXPECT_FALSE (contains (text, "Details"));
    }
}

TEST (LumexCastBehaviorTest,
      GivenFailedReferenceCast_WhenWhatCalled_ThenNamesTypesAndTheStdReason)
{
  bird sparrow;
  animal &as_base = sparrow;
  try
    {
      LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (downcast<dog &> (as_base));
      FAIL () << "bad_down_cast expected";
    }
  catch (bad_down_cast const &error)
    {
      std::string const text = error.what ();
      // Reference form names the type of the reference (typeid of a
      // polymorphic lvalue is its dynamic type) and keeps what dynamic_cast
      // reported as the details.
      EXPECT_EQ (text, std::string ("downcast failed: dynamic type '")
                           + typeid (bird).name () + "' is not a '"
                           + typeid (dog).name ()
                           + "'. Details: " + std::bad_cast ().what ());
    }
}

TEST (LumexCastBehaviorTest, GivenBadDownCast_WhenCopied_ThenKeepsTheText)
{
  bird sparrow;
  animal *as_base = &sparrow;
  try
    {
      LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (downcast<dog *> (as_base));
      FAIL () << "bad_down_cast expected";
    }
  catch (bad_down_cast const &error)
    {
      bad_down_cast const copy (error);
      EXPECT_STREQ (copy.what (), error.what ());
      std::exception const &base_view = copy;
      EXPECT_STREQ (base_view.what (), error.what ());
    }
}

TEST (LumexCastBehaviorTest,
      GivenFailedCast_WhenCaughtAsStdException_ThenCaught)
{
  bird sparrow;
  animal &as_base = sparrow;
  bool caught = false;
  try
    {
      LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (downcast<dog &> (as_base));
    }
  catch (std::exception const &)
    {
      caught = true;
    }
  EXPECT_TRUE (caught);
}

TEST (LumexCastBehaviorTest,
      GivenTwoTypes_WhenConstructedDirectly_ThenTextHasBoth)
{
  bad_down_cast const plain_text (typeid (dog), typeid (bird));
  EXPECT_EQ (std::string (plain_text.what ()),
             std::string ("downcast failed: dynamic type '")
                 + typeid (dog).name () + "' is not a '"
                 + typeid (bird).name () + "'");
  bad_down_cast const detailed (typeid (dog), typeid (bird), "extra");
  EXPECT_EQ (std::string (detailed.what ()),
             std::string ("downcast failed: dynamic type '")
                 + typeid (dog).name () + "' is not a '"
                 + typeid (bird).name () + "'. Details: extra");
}

// --- the noexcept variant -----------------------------------------------

TEST (LumexCastBehaviorTest,
      GivenManyFailures_WhenDowncastNoexcept_ThenNeverThrows)
{
  bird sparrow;
  animal *as_base = &sparrow;
  for (int round = 0; round < 100; ++round)
    {
      dog stand_in;
      dog *result = &stand_in;
      EXPECT_NO_THROW (result = downcast_noexcept<dog *> (as_base));
      EXPECT_EQ (result, nullptr);
    }
}

TEST (LumexCastBehaviorTest,
      GivenNoexceptSuccess_WhenCompared_ThenMatchesThrowingForm)
{
  triathlete athlete;
  runner *as_runner = &athlete;
  EXPECT_EQ (downcast_noexcept<triathlete *> (as_runner),
             downcast<triathlete *> (as_runner));
  joined whole;
  root *as_root = &whole;
  EXPECT_EQ (downcast_noexcept<joined *> (as_root),
             downcast<joined *> (as_root));
}
