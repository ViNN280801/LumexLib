// LumexCallbackSlot.tests.cpp
#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/callback/LumexCallbackSlot.hpp"

using lumex::core::utility::callback::LumexCallbackSlot;

namespace
{
struct text_tag_t;
struct other_text_tag_t;
struct sum_tag_t;

using TextSlot = LumexCallbackSlot<text_tag_t, void (char const *)>;
using OtherTextSlot = LumexCallbackSlot<other_text_tag_t, void (char const *)>;
using SumSlot = LumexCallbackSlot<sum_tag_t, int (int, int)>;

std::string g_primary_text;
std::string g_secondary_text;
std::string g_fallback_text;
int g_primary_calls = 0;

void
primary_sink (char const *text)
{
  ++g_primary_calls;
  g_primary_text = text;
}

void
secondary_sink (char const *text)
{
  g_secondary_text = text;
}

void
throwing_sink (char const *)
{
  throw std::runtime_error ("sink failed");
}

void
fallback_sink (char const *text)
{
  g_fallback_text = text;
}

int
add (int lhs, int rhs)
{
  return lhs + rhs;
}

int
throwing_add (int, int)
{
  throw std::logic_error ("add failed");
}

int
multiply (int lhs, int rhs)
{
  return lhs * rhs;
}

class LumexCallbackSlotTest : public ::testing::Test
{
protected:
  void
  SetUp () override
  {
    TextSlot::reset ();
    OtherTextSlot::reset ();
    SumSlot::reset ();
    g_primary_text.clear ();
    g_secondary_text.clear ();
    g_fallback_text.clear ();
    g_primary_calls = 0;
  }

  void
  TearDown () override
  {
    TextSlot::reset ();
    OtherTextSlot::reset ();
    SumSlot::reset ();
  }
};
} // namespace

TEST_F (LumexCallbackSlotTest, GivenFreshSlot_WhenGet_ThenIsNullAndNotSet)
{
  EXPECT_EQ (TextSlot::get (), nullptr);
  EXPECT_FALSE (TextSlot::is_set ());
}

TEST_F (LumexCallbackSlotTest, GivenInstalledFunction_WhenGet_ThenReturnsIt)
{
  TextSlot::set (&primary_sink);
  EXPECT_EQ (TextSlot::get (), &primary_sink);
  EXPECT_TRUE (TextSlot::is_set ());
}

TEST_F (LumexCallbackSlotTest, GivenInstalledFunction_WhenSetNull_ThenCleared)
{
  TextSlot::set (&primary_sink);
  TextSlot::set (nullptr);
  EXPECT_EQ (TextSlot::get (), nullptr);
  EXPECT_FALSE (TextSlot::is_set ());
}

TEST_F (LumexCallbackSlotTest, GivenInstalledFunction_WhenReset_ThenCleared)
{
  TextSlot::set (&primary_sink);
  TextSlot::reset ();
  EXPECT_FALSE (TextSlot::is_set ());
}

TEST_F (LumexCallbackSlotTest,
        GivenInstalledFunction_WhenSetAnother_ThenLastOneWins)
{
  TextSlot::set (&primary_sink);
  TextSlot::set (&secondary_sink);
  EXPECT_EQ (TextSlot::get (), &secondary_sink);
}

TEST_F (LumexCallbackSlotTest, GivenEmptySlot_WhenExchange_ThenReturnsNull)
{
  EXPECT_EQ (TextSlot::exchange (&primary_sink), nullptr);
  EXPECT_EQ (TextSlot::get (), &primary_sink);
}

TEST_F (LumexCallbackSlotTest,
        GivenInstalledFunction_WhenExchange_ThenReturnsPrevious)
{
  TextSlot::set (&primary_sink);
  EXPECT_EQ (TextSlot::exchange (&secondary_sink), &primary_sink);
  EXPECT_EQ (TextSlot::get (), &secondary_sink);
}

TEST_F (LumexCallbackSlotTest,
        GivenInstalledFunction_WhenInvokeOr_ThenCallsItAndNotFallback)
{
  TextSlot::set (&primary_sink);
  TextSlot::invoke_or (&fallback_sink, "hello");
  EXPECT_EQ (g_primary_text, "hello");
  EXPECT_EQ (g_primary_calls, 1);
  EXPECT_TRUE (g_fallback_text.empty ());
}

TEST_F (LumexCallbackSlotTest, GivenEmptySlot_WhenInvokeOr_ThenCallsFallback)
{
  TextSlot::invoke_or (&fallback_sink, "unfound");
  EXPECT_EQ (g_fallback_text, "unfound");
  EXPECT_EQ (g_primary_calls, 0);
}

TEST_F (LumexCallbackSlotTest,
        GivenThrowingFunction_WhenInvokeOr_ThenFallbackGetsSameArgs)
{
  TextSlot::set (&throwing_sink);
  EXPECT_NO_THROW (TextSlot::invoke_or (&fallback_sink, "rescued"));
  EXPECT_EQ (g_fallback_text, "rescued");
}

TEST_F (LumexCallbackSlotTest,
        GivenEmptySlot_WhenInvokeOrWithThrowingFallback_ThenPropagates)
{
  EXPECT_THROW (TextSlot::invoke_or (&throwing_sink, "x"), std::runtime_error);
}

TEST_F (LumexCallbackSlotTest,
        GivenLambdaFallback_WhenInvokeOrOnEmptySlot_ThenLambdaRuns)
{
  std::string captured;
  TextSlot::invoke_or ([&captured] (char const *text) { captured = text; },
                       "lambda");
  EXPECT_EQ (captured, "lambda");
}

TEST_F (LumexCallbackSlotTest,
        GivenResultSignature_WhenInvokeOr_ThenReturnsInstalledResult)
{
  SumSlot::set (&add);
  EXPECT_EQ (SumSlot::invoke_or (&multiply, 3, 4), 7);
}

TEST_F (LumexCallbackSlotTest,
        GivenResultSignatureAndEmptySlot_WhenInvokeOr_ThenReturnsFallback)
{
  EXPECT_EQ (SumSlot::invoke_or (&multiply, 3, 4), 12);
}

TEST_F (LumexCallbackSlotTest,
        GivenResultSignatureAndThrowingFunction_WhenInvokeOr_ThenFallback)
{
  SumSlot::set (&throwing_add);
  EXPECT_EQ (SumSlot::invoke_or (&multiply, 5, 6), 30);
}

TEST_F (LumexCallbackSlotTest,
        GivenSameSignatureDifferentTags_WhenSetOne_ThenOtherStaysEmpty)
{
  TextSlot::set (&primary_sink);
  EXPECT_FALSE (OtherTextSlot::is_set ());
  OtherTextSlot::set (&secondary_sink);
  EXPECT_EQ (TextSlot::get (), &primary_sink);
  EXPECT_EQ (OtherTextSlot::get (), &secondary_sink);
}

TEST_F (LumexCallbackSlotTest,
        GivenScoped_WhenInScopeAndAfter_ThenInstallsAndRestoresPrevious)
{
  TextSlot::set (&primary_sink);
  {
    TextSlot::Scoped const scoped (&secondary_sink);
    EXPECT_EQ (TextSlot::get (), &secondary_sink);
  }
  EXPECT_EQ (TextSlot::get (), &primary_sink);
}

TEST_F (LumexCallbackSlotTest,
        GivenEmptySlot_WhenScopedEnds_ThenSlotIsEmptyAgain)
{
  {
    TextSlot::Scoped const scoped (&primary_sink);
    EXPECT_TRUE (TextSlot::is_set ());
  }
  EXPECT_FALSE (TextSlot::is_set ());
}

TEST_F (LumexCallbackSlotTest, GivenNestedScoped_WhenUnwound_ThenLifoRestore)
{
  {
    TextSlot::Scoped const outer (&primary_sink);
    {
      TextSlot::Scoped const inner (&secondary_sink);
      EXPECT_EQ (TextSlot::get (), &secondary_sink);
    }
    EXPECT_EQ (TextSlot::get (), &primary_sink);
  }
  EXPECT_EQ (TextSlot::get (), nullptr);
}

TEST_F (LumexCallbackSlotTest, GivenScopedNull_WhenInScope_ThenSlotIsEmpty)
{
  TextSlot::set (&primary_sink);
  {
    TextSlot::Scoped const scoped (nullptr);
    EXPECT_FALSE (TextSlot::is_set ());
  }
  EXPECT_EQ (TextSlot::get (), &primary_sink);
}

TEST_F (LumexCallbackSlotTest, GivenSlotType_WhenInspected_ThenContractHolds)
{
  static_assert (
      std::is_same<TextSlot::function_type, void (*) (char const *)>::value,
      "function_type is the pointer to the signature");
  static_assert (
      std::is_same<SumSlot::function_type, int (*) (int, int)>::value,
      "function_type keeps the result type");
  static_assert (!std::is_default_constructible<TextSlot>::value,
                 "the slot is a static-only type");
  static_assert (!std::is_copy_constructible<TextSlot::Scoped>::value,
                 "Scoped is not copyable");
  static_assert (noexcept (TextSlot::set (nullptr)), "set is noexcept");
  static_assert (noexcept (TextSlot::get ()), "get is noexcept");
  static_assert (noexcept (TextSlot::exchange (nullptr)),
                 "exchange is noexcept");
  SUCCEED ();
}

TEST_F (LumexCallbackSlotTest,
        Stress_GivenConcurrentSetAndGet_WhenRacing_ThenOnlyKnownValuesSeen)
{
  std::atomic<bool> stop (false);
  std::atomic<int> unknown (0);
  std::vector<std::thread> threads;
  for (int writer = 0; writer < 2; ++writer)
    {
      threads.emplace_back ([writer, &stop] () {
        while (!stop.load ())
          TextSlot::set (writer == 0 ? &primary_sink : &secondary_sink);
      });
    }
  threads.emplace_back ([&stop, &unknown] () {
    for (int i = 0; i < 100000; ++i)
      {
        TextSlot::function_type const fn = TextSlot::get ();
        if (fn != nullptr && fn != &primary_sink && fn != &secondary_sink)
          ++unknown;
      }
    stop.store (true);
  });
  for (std::size_t i = 0; i < threads.size (); ++i)
    threads[i].join ();
  EXPECT_EQ (unknown.load (), 0);
}
