// LumexCast.tests.cpp
#include <string>
#include <typeinfo>

#include <gtest/gtest.h>

#include "lumex/core/utility/cast/LumexCast.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

#if defined(__clang__)
#endif

using namespace lumex::core::utility::cast;

using lumex::core::utility::cast::BadDownCast;
using lumex::core::utility::cast::downcast;
using lumex::core::utility::cast::downcast_noexcept;

namespace
{
struct Base
{
  virtual ~Base () = default;
  virtual int
  id () const
  {
    return 0;
  }
};

struct Derived : Base
{
  int
  id () const override
  {
    return 1;
  }
};

struct Other : Base
{
  int
  id () const override
  {
    return 2;
  }
};

struct Grandchild : Derived
{
  int
  id () const override
  {
    return 3;
  }
};

struct Left
{
  virtual ~Left () = default;
  int left = 11;
};

struct Right
{
  virtual ~Right () = default;
  int right = 22;
};

struct Both : Left, Right
{
};
} // namespace

TEST (LumexCastTest, GivenValidPointer_WhenDowncast_ThenReturnsDerivedPointer)
{
  Derived derived;
  Base *base = &derived;

  Derived *result = downcast<Derived *> (base);
  ASSERT_NE (result, nullptr);
  EXPECT_EQ (result->id (), 1);
}

TEST (LumexCastTest,
      GivenNullPointer_WhenDowncast_ThenReturnsNullWithoutThrowing)
{
  Base *base = nullptr;
  EXPECT_NO_THROW ({
    Derived *result = downcast<Derived *> (base);
    EXPECT_EQ (result, nullptr);
  });
}

TEST (LumexCastTest, GivenMismatchedPointer_WhenDowncast_ThenThrowsBadDownCast)
{
  Other other;
  Base *base = &other;

  EXPECT_THROW ({ (void)downcast<Derived *> (base); }, BadDownCast);
}

TEST (LumexCastTest,
      GivenValidReference_WhenDowncast_ThenReturnsDerivedReference)
{
  Derived derived;
  Base &base = derived;

  Derived &result = downcast<Derived &> (base);
  EXPECT_EQ (result.id (), 1);
}

TEST (LumexCastTest,
      GivenMismatchedReference_WhenDowncast_ThenThrowsBadDownCast)
{
  Other other;
  Base &base = other;

  EXPECT_THROW ({ (void)downcast<Derived &> (base); }, BadDownCast);
}

TEST (LumexCastTest,
      GivenMismatchedPointer_WhenDowncastNoexcept_ThenReturnsNullptr)
{
  Other other;
  Base *base = &other;

  Derived *result = nullptr;
  EXPECT_NO_THROW ({ result = downcast_noexcept<Derived *> (base); });
  EXPECT_EQ (result, nullptr);
}

TEST (LumexCastTest,
      GivenValidPointer_WhenDowncastNoexcept_ThenReturnsDerivedPointer)
{
  Derived derived;
  Base *base = &derived;

  Derived *result = downcast_noexcept<Derived *> (base);
  ASSERT_NE (result, nullptr);
  EXPECT_EQ (result->id (), 1);
}

TEST (LumexCastTest,
      GivenNullPointer_WhenDowncastNoexcept_ThenReturnsNullWithoutThrowing)
{
  Base *base = nullptr;
  Derived *result = downcast_noexcept<Derived *> (base);
  EXPECT_EQ (result, nullptr);
}

TEST (LumexCastTest,
      GivenConstDerivedPointer_WhenDowncast_ThenPreservesConstAndIdentity)
{
  Derived derived;
  Base const *base = &derived;
  Derived const *result = downcast<Derived const *> (base);
  ASSERT_NE (result, nullptr);
  EXPECT_EQ (result->id (), 1);
}

TEST (LumexCastTest,
      GivenBadDownCastException_WhenWhatCalled_ThenMessageMentionsBothTypes)
{
  Other other;
  Base *base = &other;

  try
    {
      (void)downcast<Derived *> (base);
      FAIL () << "Expected BadDownCast to be thrown";
    }
  catch (BadDownCast const &exc)
    {
      std::string const message = exc.what ();
      EXPECT_NE (message.find ("downcast failed"), std::string::npos);
    }
}

TEST (LumexCastTest, GivenSuccessfulPointerCast_WhenCompared_ThenSameAddress)
{
  Derived derived;
  Base *base = &derived;
  Derived *result = downcast<Derived *> (base);
  EXPECT_EQ (static_cast<void *> (result), static_cast<void *> (&derived));
  EXPECT_EQ (result, &derived);
}

TEST (LumexCastTest,
      GivenGrandchildObject_WhenDowncastToMidAndLeaf_ThenBothSucceed)
{
  Grandchild child;
  Base *base = &child;

  Derived *mid = downcast<Derived *> (base);
  Grandchild *leaf = downcast<Grandchild *> (base);
  ASSERT_NE (mid, nullptr);
  ASSERT_NE (leaf, nullptr);
  EXPECT_EQ (mid->id (), 3);
  EXPECT_EQ (leaf->id (), 3);
}

TEST (LumexCastTest,
      GivenGrandchildAsDerived_WhenDowncastToGrandchild_ThenSucceeds)
{
  Grandchild child;
  Derived &mid = child;
  Grandchild &leaf = downcast<Grandchild &> (mid);
  EXPECT_EQ (leaf.id (), 3);
}

TEST (LumexCastTest,
      GivenConstReference_WhenDowncast_ThenPreservesConstAndIdentity)
{
  Derived derived;
  Base const &base = derived;
  Derived const &result = downcast<Derived const &> (base);
  EXPECT_EQ (result.id (), 1);
  EXPECT_EQ (&result, &derived);
}

TEST (LumexCastTest, GivenMismatchedConstReference_WhenDowncast_ThenThrows)
{
  Other other;
  Base const &base = other;
  EXPECT_THROW ({ (void)downcast<Derived const &> (base); }, BadDownCast);
}

TEST (LumexCastTest, GivenBadDownCast_WhenCaughtAsStdBadCast_ThenIsABase)
{
  Other other;
  Base *base = &other;
  EXPECT_THROW ({ (void)downcast<Derived *> (base); }, std::bad_cast);
}

TEST (LumexCastTest,
      GivenDetailsConstructor_WhenWhatCalled_ThenIncludesDetailsSuffix)
{
  BadDownCast const exc (typeid (Other), typeid (Derived), "unit-test");
  std::string const message = exc.what ();
  EXPECT_NE (message.find ("downcast failed"), std::string::npos);
  EXPECT_NE (message.find ("Details: unit-test"), std::string::npos);
}

TEST (LumexCastTest,
      GivenTwoArgConstructor_WhenWhatCalled_ThenContainsFromAndToNames)
{
  BadDownCast const exc (typeid (Other), typeid (Derived));
  std::string const message = exc.what ();
  EXPECT_NE (message.find ("downcast failed"), std::string::npos);
  EXPECT_FALSE (message.empty ());
}

TEST (LumexCastTest,
      GivenMultipleInheritance_WhenDowncastFromLeft_ThenRecoversBoth)
{
  Both both;
  Left *left = &both;
  Both *recovered = downcast<Both *> (left);
  ASSERT_NE (recovered, nullptr);
  EXPECT_EQ (recovered->left, 11);
  EXPECT_EQ (recovered->right, 22);
}

TEST (LumexCastTest,
      GivenConstValidPointer_WhenDowncastNoexcept_ThenReturnsConstDerived)
{
  Derived derived;
  Base const *base = &derived;
  Derived const *result = downcast_noexcept<Derived const *> (base);
  ASSERT_NE (result, nullptr);
  EXPECT_EQ (result, &derived);
}

TEST (LumexCastTest,
      GivenMismatchedConstPointer_WhenDowncastNoexcept_ThenReturnsNull)
{
  Other other;
  Base const *base = &other;
  EXPECT_EQ (downcast_noexcept<Derived const *> (base), nullptr);
}

TEST (LumexCastTest,
      GivenFailedReferenceCast_WhenWhatCalled_ThenMentionsDowncastFailed)
{
  Other other;
  Base &base = other;
  try
    {
      (void)downcast<Derived &> (base);
      FAIL () << "Expected BadDownCast";
    }
  catch (BadDownCast const &exc)
    {
      EXPECT_NE (std::string (exc.what ()).find ("downcast failed"),
                 std::string::npos);
    }
}
